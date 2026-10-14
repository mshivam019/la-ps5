// Mini-bosses on the overworld (enemy randomizer, "Include mini-bosses"; kinds in LAS enemies.py FIELD_MINIBOSSES).
// Armos Knight, Giant Buzz Blob and Dodongo Snakes fight on their own outside their arenas. Hinox sits in its idle
// state until its dungeon room starts it: entering the room's camera zone (zone manager -> f_7000029520 -> actor
// vfunc +0x100) hands a callback to the room's AreaEventBox, Link inside the box fires it, the actor's next state
// requests the HinoxStart event (BGM, camera, shutters) and that event's end moves it into its fight state. The Field
// has no zones or boxes, so here the runtime moves it into the fight state itself (no start event: no boss music or
// shutter steps) once Link comes near. Actors are seen through a per-frame type check (tools/patches.py
// MB_SEEN_SITE) and picked out by class vtable.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"

void guest_call(Cpu *c, uint64_t addr);
uint64_t doorjump_player(uint64_t frame);  // PlayerCollision component: position at +0x80

#define PLAYER_STATUS_PTR (MAIN_BASE + 0x1CC1368)  // PlayerStatus*: current level name at +0

// per class: vtable, its state machine's change(sm, state) (+0xd current state, +0xe previous; states 0x30 apart
// from +0x40: ctx, enter, exec, exit member functions), the state machine's offset in the actor, idle state, fight
// state, position offset (x y z floats: the spawn point)
typedef struct { uint64_t vt, chg; uint32_t sm, pos; uint8_t idle, fight; const char *name; } Kind;
static const Kind g_kinds[] = {
    {MAIN_BASE + 0x1b80e90, MAIN_BASE + 0x3bfc60, 0xff0, 0x260, 0, 1, "Hinox"},  // 0x53: 10 = request HinoxStart
};
#define NKINDS (int)(sizeof g_kinds / sizeof g_kinds[0])
#define WAKE_DIST 7.0f  // (a Field screen is ~15 x 12 units)

typedef struct { uint64_t obj, first, last; int kind, woken; } Seen;
static Seen g_seen[32];
RT_ON_RESET(miniboss_reset) { memset(g_seen, 0, sizeof g_seen); }
extern uint64_t renderer_present_count(void);

static void log_new_vtable(uint64_t o, uint64_t vt) {  // LARECOMP_MB_LOG (RE): every actor class seen, once
    static uint64_t vts[512];
    static int n;
    for (int i = 0; i < n; i++)
        if (vts[i] == vt) return;
    if (n < 512) vts[n++] = vt;
    LOG("[mb] new class vtable main+%#llx (actor %llx, +0x100 main+%#llx)\n", (unsigned long long)(vt - MAIN_BASE),
        (unsigned long long)o, (unsigned long long)(LD64(vt + 0x100) - MAIN_BASE));
}

// LARECOMP_MB_SCAN=<vtable offsets, hex, comma-separated> (RE): finds the state machines inside actors of those classes
// (ctx pointer at +0x40 = the actor, state 0's enter function at +0x48 / +0x50), lists their states and logs changes
static uint64_t g_scan_vt[16], g_scan_obj[8], g_scan_sm[8][8];
static uint8_t g_scan_last[8][8];
static int g_scan_nv = -1;

static bool code_ptr(uint64_t p) { return p >= MAIN_BASE && p < MAIN_BASE + 0x1600000; }

static void scan_seen(uint64_t o, uint64_t vt) {
    if (g_scan_nv < 0) {
        g_scan_nv = 0;
        for (const char *p = ENV("LARECOMP_MB_SCAN"); p && *p && g_scan_nv < 16;) {
            g_scan_vt[g_scan_nv++] = MAIN_BASE + strtoull(p, (char **)&p, 16);
            while (*p == ',') p++;
        }
    }
    int k = 0;
    while (k < g_scan_nv && g_scan_vt[k] != vt) k++;
    if (k == g_scan_nv) return;
    int fr = -1;
    for (int i = 0; i < 8; i++) {
        if (g_scan_obj[i] == o) return;
        if (fr < 0 && !g_scan_obj[i]) fr = i;
    }
    if (fr < 0) return;
    g_scan_obj[fr] = o;
    memset(g_scan_sm[fr], 0, sizeof g_scan_sm[fr]);
    int n = 0;
    for (uint64_t off = 0; off < 0x2400 && n < 8; off += 8)
        if (LD64(o + off + 0x40) == o && (code_ptr(LD64(o + off + 0x48)) || code_ptr(LD64(o + off + 0x50)))) {
            g_scan_sm[fr][n++] = off;
            g_scan_last[fr][n - 1] = *(uint8_t *)(uintptr_t)(o + off + 0xd);
            LOG("[mb-scan] actor %llx (main+%#llx): state machine at +%#llx, state %u\n", (unsigned long long)o,
                (unsigned long long)(vt - MAIN_BASE), (unsigned long long)off, g_scan_last[fr][n - 1]);
            for (int s = 0; s < 24; s++) {
                uint64_t e = o + off + 0x40 + s * 0x30, f1 = LD64(e + 8), f2 = LD64(e + 0x18), f3 = LD64(e + 0x28);
                if (code_ptr(f1) || code_ptr(f2) || code_ptr(f3))
                    LOG("[mb-scan]   state %d: enter %llx exec %llx exit %llx\n", s, (unsigned long long)f1,
                        (unsigned long long)f2, (unsigned long long)f3);
            }
        }
}

static void scan_tick(uint64_t frame) {
    for (int i = 0; i < 8; i++) {
        uint64_t o = g_scan_obj[i];
        if (!o) continue;
        int alive = 0;
        for (int k = 0; k < g_scan_nv; k++) alive |= LD64(o) == g_scan_vt[k];
        if (!alive) { g_scan_obj[i] = 0; continue; }
        for (int j = 0; j < 8 && g_scan_sm[i][j]; j++) {
            uint8_t st = *(uint8_t *)(uintptr_t)(o + g_scan_sm[i][j] + 0xd);
            if (st != g_scan_last[i][j])
                LOG("[mb-scan] f%llu actor %llx sm +%#llx: %u -> %u\n", (unsigned long long)frame, (unsigned long long)o,
                    (unsigned long long)g_scan_sm[i][j], g_scan_last[i][j], st);
            g_scan_last[i][j] = st;
        }
    }
}

void rt_mb_seen(uint64_t o) {
    uint64_t vt = LD64(o);
    if (ENV("LARECOMP_MB_LOG")) log_new_vtable(o, vt);
    if (ENV("LARECOMP_MB_SCAN")) scan_seen(o, vt);
    int k = 0;
    while (k < NKINDS && g_kinds[k].vt != vt) k++;
    if (k == NKINDS) return;
    uint64_t f = renderer_present_count();
    int free_i = -1;
    for (int i = 0; i < 32; i++) {
        if (g_seen[i].obj == o) { g_seen[i].last = f; return; }
        if (free_i < 0 && (!g_seen[i].obj || f - g_seen[i].last > 120)) free_i = i;
    }
    if (free_i >= 0) g_seen[free_i] = (Seen){o, f, f, k, 0};
}

void miniboss_tick(Cpu *c, uint64_t frame) {
    if (ENV("LARECOMP_MB_SCAN")) scan_tick(frame);
    uint64_t ps = LD64(PLAYER_STATUS_PTR);
    if (!ps || strcmp((const char *)(uintptr_t)ps, "Field")) return;  // (dungeon arenas start their own)
    uint64_t pc = doorjump_player(frame);
    if (!pc) return;
    const float *lp = (const float *)(uintptr_t)(pc + 0x80);
    for (int i = 0; i < 32; i++) {
        Seen *s = &g_seen[i];
        if (!s->obj || s->woken || frame - s->last > 2 || frame - s->first < 30 || LD64(s->obj) != g_kinds[s->kind].vt)
            continue;
        const Kind *k = &g_kinds[s->kind];
        const float *p = (const float *)(uintptr_t)(s->obj + k->pos);
        float dx = p[0] - lp[0], dz = p[2] - lp[2];
        if (*(uint8_t *)(uintptr_t)(s->obj + k->sm + 0xd) != k->idle || dx * dx + dz * dz > WAKE_DIST * WAKE_DIST) continue;
        s->woken = 1;
        uint64_t saved[31], sp = c->sp;
        memcpy(saved, c->x, sizeof saved);
        c->x[0] = s->obj + k->sm;
        c->x[1] = k->fight;
        guest_call(c, k->chg);
        memcpy(c->x, saved, sizeof saved);
        c->sp = sp;
        LOG("[miniboss] f%llu %s woken (Link %.1f away)\n", (unsigned long long)frame, k->name, sqrtf(dx * dx + dz * dz));
    }
}
