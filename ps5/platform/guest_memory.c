// Native guest backing uses the SDK's direct pool, not the small flexible pool.
// Integration candidate: only explicitly committed regions are accessible.
// Call sites commit before access; unexpected first-touch faults are reported.
#include "platform.h"
#include <ps5platform/shm.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// Reservation/reset run on the main thread before/after guest threads exist.
// Concurrent commits are serialized by libps5platform itself.
static uint64_t guest_lo, guest_hi;
#define DIRECT_UNIT 0x10000ull

static bool contains(uint64_t addr, uint64_t size) {
    return guest_hi && addr >= guest_lo && addr <= guest_hi && size <= guest_hi - addr;
}

bool plat_reserve_guest(uint64_t lo, uint64_t hi) {
    if (guest_hi || !lo || hi <= lo || lo % DIRECT_UNIT || hi % DIRECT_UNIT) return false;
    int rc = ps5_vrange_reserve_at((void *)(uintptr_t)lo, hi - lo);
    if (rc) return false;
    guest_lo = lo;
    guest_hi = hi;
    return true;
}

bool plat_reset_guest(uint64_t lo, uint64_t hi) {
    if (!guest_hi || lo != guest_lo || hi != guest_hi) return false;
    // Retain ownership; subsequent explicit commits produce fresh zero pages.
    return ps5_vrange_decommit((void *)(uintptr_t)lo, hi - lo) == 0;
}

void plat_commit(uint64_t addr, uint64_t size) {
    if (!size) return;
    int rc = PS5_SHM_BAD_REQUEST;
    if (contains(addr, size))
        rc = ps5_vrange_commit((void *)(uintptr_t)addr, size, PS5_SHM_READ | PS5_SHM_WRITE);
    if (rc) {
        // The caller cannot recover: continuing would use missing/partial backing.
        fprintf(stderr, "[ps5 memory] commit %llx+%llx failed: %08x\n",
                (unsigned long long)addr, (unsigned long long)size, (unsigned)rc);
        abort();
    }
}

int plat_protect(uint64_t addr, uint64_t size, int prot) {
    if (!contains(addr, size) || !size || addr % PLAT_PAGE || size % PLAT_PAGE ||
        (prot != PLAT_PROT_NONE && prot != PLAT_PROT_READ && prot != PLAT_PROT_RW)) {
        errno = EINVAL;
        return -1;
    }
    int flags = prot == PLAT_PROT_NONE ? 0 : prot == PLAT_PROT_READ ? PS5_SHM_READ :
        PS5_SHM_READ | PS5_SHM_WRITE;
    int rc = ps5_vrange_commit((void *)(uintptr_t)addr, size, flags);
    if (rc) { errno = ENOMEM; return -1; }
    return 0;
}
