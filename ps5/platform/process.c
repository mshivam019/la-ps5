// PS5 process/time/diagnostics. Desktop extraction runs on the computer.
#include "platform.h"
#include <SDL3/SDL.h>
#include <ps5platform/context.h>
#include <ps5platform/klog.h>
#include <ps5platform/libc.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char *, const char *const *);
int sceKernelUsleep(unsigned);

void catchReturnFromMain(int status);
void __wrap_exit(int status) { catchReturnFromMain(status); }
void __wrap__Exit(int status) { catchReturnFromMain(status); }
void __wrap__exit(int status) { catchReturnFromMain(status); }
void __wrap_abort(void) { catchReturnFromMain(1); }
void plat_terminate(void) { catchReturnFromMain(0); }

void plat_init(void) {
    ps5_klog_capture_stderr("[Link's Awakening] ");
    fprintf(stderr, "[la] native startup\n");
    sceSystemServiceHideSplashScreen();
    mkdir("/app0/user", 0777);
    chmod("/app0/user", 0777);
    setenv("LARECOMP_DATA_ROOT", "/app0/user", 0);
    setenv("LARECOMP_PAD_TYPE", "playstation", 0);
    setenv("LARECOMP_FONT", "/app0/DejaVuSans.ttf", 0);
    setenv("LARECOMP_NO_UPDATE_CHECK", "1", 0);
    // User prefers temporary object pop-in to blocking first-use compilation.
    // Keep source/driver caches, but use the renderer's nonblocking worker path.
    unsetenv("LARECOMP_SYNC_SHADERS");
    unsetenv("LARECOMP_PREPARE_SHADERS");
    fprintf(stderr, "[la] shaders: asynchronous, unfinished draws appear when ready (disk cache enabled)\n");
    // Explicit diagnostic only: consume once, never ship as a default.
    FILE *depth_test = fopen("/app0/ps5-depth-test.txt", "r");
    if (depth_test) {
        fclose(depth_test);
        remove("/app0/ps5-depth-test.txt");
        setenv("LARECOMP_VK_EQUAL_TOL", "1", 1);
        fprintf(stderr, "[la] diagnostic: tolerant equal-depth comparison enabled\n");
    }
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIODRIVER", "ps5", true);
    // Optional one-shot test hook; absent from ordinary packages. Tests keep
    // their own settings and saves, and end through the normal shell exit.
    FILE *test = fopen("/app0/ps5-test.txt", "r");
    if (test) {
        char mode[16]; unsigned frames = 0;
        int fields = fscanf(test, "%15s %u", mode, &frames);
        fclose(test);
        if (unlink("/app0/ps5-test.txt")) abort();
        if (fields != 2 || !frames || frames > 3600 || (strcmp(mode, "launcher") && strcmp(mode, "game"))) abort();
        char count[16]; snprintf(count, sizeof count, "%u", frames);
        mkdir("/app0/user/build", 0777);
        setenv("LARECOMP_SETTINGS", "/app0/user/test.ini", 1);
        setenv("LARECOMP_SAVE_DIR", "/app0/user/test-save", 1);
        if (!strcmp(mode, "launcher")) setenv("LARECOMP_LAUNCHER_SHOT", count, 1);
        else {
            setenv("LARECOMP_NO_LAUNCHER", "1", 1);
            setenv("LARECOMP_WINDOW_SHOT", count, 1);
            setenv("LARECOMP_EXIT_AFTER_SHOT", "1", 1);
        }
        fprintf(stderr, "[la] one-shot %s test: %u frames\n", mode, frames);
    }
}

uint64_t plat_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
void plat_sleep_until_ns(uint64_t t) {
    struct timespec ts = {.tv_sec = t / 1000000000ull, .tv_nsec = t % 1000000000ull};
    while (ps5_clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR) {}
}
void plat_thread_realtime(uint64_t period_ns) { (void)period_ns; }
bool plat_exe_path(char *out, size_t size) {
    return size > strlen("/app0/eboot.bin") && (strcpy(out, "/app0/eboot.bin"), true);
}
void plat_exec(char *const argv[]) { (void)argv; errno = ENOTSUP; }
PlatProc plat_spawn(const char *const argv[], const char *log, bool append) {
    (void)argv; (void)log; (void)append;
    errno = ENOTSUP;
    fprintf(stderr, "[la] desktop helper requested: prepare game files/seeds on the computer\n");
    return 0;
}
int plat_proc_wait(PlatProc p, bool block) { (void)p; (void)block; return 255; }
bool plat_remove_tree(const char *path) {
    (void)path;
    errno = ENOTSUP; // Game-file management belongs to the computer setup.
    return false;
}
const char *plat_symbol(uint64_t pc, uint64_t *offset) {
    (void)pc; if (offset) *offset = 0; return NULL;
}
void plat_print_backtrace(void) {
    void *frames[32];
    int count = ps5_backtrace(frames, 32);
    for (int i = 0; i < count; i++) fprintf(stderr, "[la] frame %d: %p\n", i, frames[i]);
}
void plat_fatal_notice(void) { fflush(stderr); }

static void crash_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc = context;
    uint64_t addr = (uintptr_t)info->si_addr, pc = uc->uc_mcontext.mc_rip;
    if (rt_fault_watch(addr, pc)) return;
    // Unbacked accesses are reported, never silently mapped in a signal handler.
    char text[32];
    snprintf(text, sizeof text, "signal %d", sig);
    rt_fault_report(text, addr, pc, 0);
}
static pthread_once_t handler_once = PTHREAD_ONCE_INIT;
static void install_handlers(void) {
    struct sigaction action = {0};
    action.sa_sigaction = crash_handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE};
    for (unsigned i = 0; i < sizeof signals / sizeof *signals; i++)
        if (sigaction(signals[i], &action, NULL)) abort();
}
void plat_crash_handler_install(void) {
    static __thread bool installed;
    if (installed) return;
    // Alternate stack is owned by the thread for its lifetime.
    stack_t stack = {.ss_sp = malloc(0x10000), .ss_size = 0x10000};
    if (!stack.ss_sp || sigaltstack(&stack, NULL)) abort();
    pthread_once(&handler_once, install_handlers);
    installed = true;
}
