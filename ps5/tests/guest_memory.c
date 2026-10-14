// Runs the real SDK range allocator on its Linux kernel model; no console claim.
#include "platform.h"
#include <ps5platform/shm.h>
#include "host_kernel.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static const uint64_t lo = 0x7000000000ull, hi = 0x7320000000ull;

static void expect_fault(uint64_t addr, bool write) {
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        volatile unsigned char *p = (void *)(uintptr_t)addr;
        if (write) *p = 7;
        else { volatile unsigned char value = *p; (void)value; }
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

static void *commit_worker(void *unused) {
    (void)unused;
    for (int i = 0; i < 16; i++) plat_commit(lo + 0x100000, 0x4000);
    return NULL;
}

int main(void) {
    struct rlimit core = {0, 0};
    assert(setrlimit(RLIMIT_CORE, &core) == 0);
    assert(!plat_reset_guest(lo, hi));
    assert(!plat_reserve_guest(lo + 1, hi));
    assert(!plat_reserve_guest(lo, lo));
    // Collision must preserve another owner's memory.
    assert(ps5_vrange_reserve_at((void *)(uintptr_t)lo, 0x10000) == 0);
    assert(ps5_vrange_commit((void *)(uintptr_t)lo, 0x10000, PS5_SHM_READ | PS5_SHM_WRITE) == 0);
    volatile unsigned char *p = (void *)(uintptr_t)lo;
    *p = 93;
    assert(!plat_reserve_guest(lo, hi) && *p == 93);
    assert(ps5_vrange_release((void *)(uintptr_t)lo, 0x10000) == 0);
    host_fail(HOST_CALL_RESERVE, 1);
    assert(!plat_reserve_guest(lo, hi));
    assert(plat_reserve_guest(lo, hi));
    assert(!plat_reserve_guest(lo, hi));
    struct ps5_shm_stats stats;
    ps5_shm_live(&stats);
    assert(stats.range_bytes == hi - lo && stats.committed_bytes == 0);
    // Loader writes may start on a guest 4 KiB boundary, within a host page.
    plat_commit(lo + 0x1000, 0x1000);
    assert(p[0x1000] == 0);
    p[0x1000] = 42;
    plat_commit(lo + 0x1000, 0x1000);
    assert(p[0x1000] == 42);
    expect_fault(lo + 0x4000, false);
    assert(plat_protect(lo, 0x4000, PLAT_PROT_READ) == 0);
    assert(p[0x1000] == 42);
    expect_fault(lo + 0x1000, true);
    assert(plat_protect(lo, 0x4000, PLAT_PROT_NONE) == 0);
    expect_fault(lo + 0x1000, false);
    assert(plat_protect(lo, 0x4000, PLAT_PROT_RW) == 0);
    assert(p[0x1000] == 42);
    assert(plat_protect(hi, 0x4000, PLAT_PROT_RW) == -1);
    assert(plat_protect(lo, UINT64_MAX, PLAT_PROT_RW) == -1);
    assert(plat_protect(lo + 1, 0x4000, PLAT_PROT_RW) == -1);
    assert(plat_protect(lo, 0x4000, 7) == -1);
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) assert(pthread_create(&threads[i], NULL, commit_worker, NULL) == 0);
    for (int i = 0; i < 4; i++) assert(pthread_join(threads[i], NULL) == 0);
    plat_commit(hi - 1, 1);
    *(volatile unsigned char *)(uintptr_t)(hi - 1) = 55;
    ps5_shm_live(&stats);
    assert(stats.committed_bytes == 3 * 0x10000);
    assert(!plat_reset_guest(lo + 0x10000, hi));
    assert(p[0x1000] == 42);
    assert(plat_reset_guest(lo, hi));
    ps5_shm_live(&stats);
    assert(stats.committed_bytes == 0 && stats.range_bytes == hi - lo);
    expect_fault(lo + 0x1000, false);
    plat_commit(lo, 0x4000);
    plat_commit(hi - 1, 1);
    assert(p[0x1000] == 0 && *(volatile unsigned char *)(uintptr_t)(hi - 1) == 0);
    // Allocation failure must stop the caller, never return partial success.
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        host_fail(HOST_CALL_ALLOCATE, 1);
        plat_commit(lo + 0x200000, 0x4000);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    assert(ps5_vrange_release((void *)(uintptr_t)lo, hi - lo) == 0);
    assert(host_direct_allocations() == 0);
    puts("PASS: guest reservation, sparse commit, collision, protection, concurrency, reset, allocation failure");
}
