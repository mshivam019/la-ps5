#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hle.h"
#include "input_bind.h"

#define SETTINGS_FILE settings_file()

// LARECOMP_SETTINGS=<path>: another settings file (test runs never touch the player's larecomp.ini)
static const char *settings_file(void) {
    const char *e = getenv("LARECOMP_SETTINGS");
    return e && *e ? e : "larecomp.ini";
}

Settings g_settings = {
    .aspect = ASPECT_UNLOCKED,
#ifdef __PROSPERO__
    .prompts = PROMPTS_PLAYSTATION,  // native DualSense prompts, still configurable
    .res_mode = 1,  // initial native target: the game's original 1080p
#endif
    .fullscreen = false,
    .volume = 1.0f,
    .reverb = true,
    .movie_audio = true,
    .show_fps = false,
    .inv_keys = true,
    .tracker = true,
    .chaos_letters = CHAOS_LETTERS_MAP,
    .diff_damage = 1.0f,
    .upscaler = UPSCALE_SHARP,  // (MetalFX on macOS, AMD FSR 1 on Windows)
};

void settings_load(void) {
    static bool loaded;
    if (loaded) return;
    loaded = true;
    FILE *f = fopen(SETTINGS_FILE, "r");
    if (!f) return;
    char line[256];
    bool tip_key = false, aspect_key = false;
    while (fgets(line, sizeof line, f)) {
        char key[64];
        char val[128];
        if (sscanf(line, " %63[^= ] = %127[^\n]", key, val) < 1) continue;
        if (!strcmp(key, "rando_seed")) {
            if (sscanf(line, " %63[^= ] = %127[^\n]", key, val) != 2) val[0] = 0;
            strlcpy(g_settings.rando_seed, val, sizeof g_settings.rando_seed);
            continue;
        }
        if (!strcmp(key, "aspect")) g_settings.aspect = !strcmp(val, "original") ? ASPECT_ORIGINAL : ASPECT_UNLOCKED;
        else if (!strcmp(key, "fullscreen")) g_settings.fullscreen = atoi(val) != 0;
        else if (!strcmp(key, "volume")) g_settings.volume = (float)atof(val);
        else if (!strcmp(key, "reverb")) g_settings.reverb = atoi(val) != 0;
        else if (!strcmp(key, "movie_audio")) g_settings.movie_audio = atoi(val) != 0;
        else if (!strcmp(key, "show_fps")) g_settings.show_fps = atoi(val) != 0;
        else if (!strcmp(key, "resolution")) {
            int n = atoi(val);
            g_settings.res_mode = !strcmp(val, "dynamic") ? RES_DYNAMIC : (!strcmp(val, "75%") || !strcmp(val, "67%")) ? RES_Q75 :
                                  !strcmp(val, "50%") ? RES_Q50 : !strcmp(val, "25%") ? RES_Q25 :
                                  (n >= 1 && n <= 4) ? n : n > 4 ? 4 : RES_MATCH;  // (5x-8x were removed)
        }
        else if (!strcmp(key, "upscaler")) g_settings.upscaler = !strcmp(val, "bilinear") ? UPSCALE_BILINEAR : UPSCALE_SHARP;  // (metalfx / fsr1: this OS's)
        else if (!strcmp(key, "antialias")) g_settings.antialias = !strcmp(val, "off") ? AA_OFF : AA_SMAA;  // ("fxaa": SMAA now)
        else if (!strcmp(key, "inv_keys")) g_settings.inv_keys = atoi(val) != 0;
        else if (!strcmp(key, "dpad_move")) g_settings.dpad_move = atoi(val) != 0;
        else if (!strcmp(key, "move_360")) g_settings.move_360 = atoi(val) != 0;
        else if (!strcmp(key, "blur_removal")) g_settings.blur_removal = atoi(val) != 0;
        else if (!strcmp(key, "hide_hero_icon")) g_settings.hide_hero_icon = atoi(val) != 0;
        else if (!strcmp(key, "prompts")) g_settings.prompts = !strcmp(val, "nintendo") ? PROMPTS_NINTENDO : !strcmp(val, "xbox") ?
                                                              PROMPTS_XBOX : !strcmp(val, "playstation") ? PROMPTS_PLAYSTATION : PROMPTS_AUTO;
        else if (!strcmp(key, "tracker")) g_settings.tracker = atoi(val) != 0;
        else if (!strcmp(key, "menu_tip_seen")) g_settings.menu_tip_seen = atoi(val) != 0, tip_key = true;
        else if (!strcmp(key, "aspect_asked")) g_settings.aspect_asked = atoi(val) != 0, aspect_key = true;
        else if (!strcmp(key, "chaos_letters"))
            g_settings.chaos_letters = !strcmp(val, "off") ? CHAOS_LETTERS_OFF : !strcmp(val, "explored") ? CHAOS_LETTERS_EXPLORED : CHAOS_LETTERS_MAP;
        else if (!strcmp(key, "cheat_hearts")) g_settings.cheat_hearts = atoi(val) != 0;
        else if (!strcmp(key, "cheat_ammo")) g_settings.cheat_ammo = atoi(val) != 0;
        else if (!strcmp(key, "cheat_rupees")) g_settings.cheat_rupees = atoi(val) != 0;
        else if (!strcmp(key, "cheat_hookshot")) g_settings.cheat_hookshot = atoi(val) != 0;
        else if (!strcmp(key, "no_buff_drops")) g_settings.no_buff_drops = atoi(val) != 0;
        else if (!strcmp(key, "diff_damage")) g_settings.diff_damage = (float)atof(val);
        else if (!strcmp(key, "diff_ohko")) g_settings.diff_ohko = atoi(val) != 0;
        else if (!strcmp(key, "diff_hero")) g_settings.diff_hero = atoi(val) != 0;
        else if (!strcmp(key, "tunic")) { for (int i = 0; i < N_TUNIC_COLOURS; i++) if (!strcmp(val, g_tunic_colours[i])) g_settings.tunic = i; }
        else if (!strcmp(key, "hearts")) { for (int i = 0; i < N_HEART_COLOURS; i++) if (!strcmp(val, g_heart_colours[i])) g_settings.hearts = i; }
        else bind_settings_load(key, val);  // bind_pad_* / bind_key_*: runtime/hle/bindings.c
    }
    fclose(f);
    if (!tip_key) g_settings.menu_tip_seen = true;  // (a settings file from before the note: not a first launch)
    if (!aspect_key) g_settings.aspect_asked = true;  // (likewise: the aspect ratio was chosen already)
    if (g_settings.volume < 0) g_settings.volume = 0;
    if (g_settings.volume > 2) g_settings.volume = 2;
    LOG("[settings] loaded %s\n", SETTINGS_FILE);
}

void settings_save(void) {
    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f) { LOG("[settings] cannot write %s\n", SETTINGS_FILE); return; }
    fprintf(f, "# LAS Recomp settings (edited from the in-game menu: F1, Esc, or Minus+Plus on a controller)\n");
    fprintf(f, "aspect = %s\n", g_settings.aspect == ASPECT_ORIGINAL ? "original" : "unlocked");
    fprintf(f, "fullscreen = %d\n", g_settings.fullscreen);
    fprintf(f, "volume = %.2f\n", g_settings.volume);
    fprintf(f, "reverb = %d\n", g_settings.reverb);
    fprintf(f, "movie_audio = %d\n", g_settings.movie_audio);
    fprintf(f, "show_fps = %d\n", g_settings.show_fps);
    fprintf(f, "upscaler = %s\n", g_settings.upscaler == UPSCALE_METALFX ? "metalfx" : g_settings.upscaler == UPSCALE_FSR1 ? "fsr1" : "bilinear");
    fprintf(f, "antialias = %s\n", g_settings.antialias == AA_SMAA ? "smaa" : "off");
    if (g_settings.res_mode == RES_DYNAMIC) fprintf(f, "resolution = dynamic\n");
    else if (g_settings.res_mode == RES_Q75) fprintf(f, "resolution = 75%%\n");
    else if (g_settings.res_mode == RES_Q25) fprintf(f, "resolution = 25%%\n");
    else if (g_settings.res_mode == RES_Q50) fprintf(f, "resolution = 50%%\n");
    else if (g_settings.res_mode >= 1 && g_settings.res_mode <= 8) fprintf(f, "resolution = %dx\n", g_settings.res_mode);
    else fprintf(f, "resolution = match\n");
    fprintf(f, "inv_keys = %d\n", g_settings.inv_keys);
    fprintf(f, "dpad_move = %d\n", g_settings.dpad_move);
    fprintf(f, "move_360 = %d\nblur_removal = %d\nhide_hero_icon = %d\n", g_settings.move_360, g_settings.blur_removal,
            g_settings.hide_hero_icon);
    static const char *const prompts[] = {"auto", "nintendo", "xbox", "playstation"};
    fprintf(f, "prompts = %s\n", prompts[g_settings.prompts & 3]);
    fprintf(f, "tracker = %d\n", g_settings.tracker);
    fprintf(f, "aspect_asked = %d\n", g_settings.aspect_asked);
    fprintf(f, "menu_tip_seen = %d\n", g_settings.menu_tip_seen);
    static const char *const letters[] = {"off", "explored", "map"};
    fprintf(f, "chaos_letters = %s\n", letters[g_settings.chaos_letters]);
    fprintf(f, "cheat_hearts = %d\ncheat_ammo = %d\ncheat_rupees = %d\n", g_settings.cheat_hearts, g_settings.cheat_ammo,
            g_settings.cheat_rupees);
    fprintf(f, "cheat_hookshot = %d\n", g_settings.cheat_hookshot);
    fprintf(f, "no_buff_drops = %d\n", g_settings.no_buff_drops);
    fprintf(f, "diff_damage = %.2f\ndiff_ohko = %d\ndiff_hero = %d\n", g_settings.diff_damage, g_settings.diff_ohko,
            g_settings.diff_hero);
    fprintf(f, "tunic = %s\nhearts = %s\n", g_tunic_colours[g_settings.tunic], g_heart_colours[g_settings.hearts]);
    bind_settings_save(f);
    fprintf(f, "rando_seed = %s\n", g_settings.rando_seed);
    fclose(f);
}
