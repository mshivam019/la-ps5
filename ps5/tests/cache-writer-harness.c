// Exercise the production checkpoint functions with real threads/files and a
// controlled Vulkan snapshot provider: no GPU is required for IO concurrency.
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
typedef int VkResult;
enum { VK_SUCCESS, VK_INCOMPLETE };
static struct { int dev, pcache; } g_vk;
static atomic_bool hold_snapshot, snapshot_entered;
static atomic_int incomplete, writes;
#define LOG(...) ((void)0)
static uint64_t SDL_GetTicksNS(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}
static VkResult vkGetPipelineCacheData(int dev, int cache, size_t *size, void *data) {
    (void)dev; (void)cache;
    if (!data) { *size = 64; return VK_SUCCESS; }
    atomic_store(&snapshot_entered, true);
    while (atomic_load(&hold_snapshot)) usleep(1000);
    if (atomic_load(&incomplete)) { atomic_fetch_sub(&incomplete, 1); return VK_INCOMPLETE; }
    memset(data, 0x5a, 64); *size = 64;
    atomic_fetch_add(&writes, 1);
    return VK_SUCCESS;
}
#include "production-cache-writer.inc"
static void wait_entered(void) {
    uint64_t until = SDL_GetTicksNS() + 2000000000ull;
    while (!atomic_load(&snapshot_entered) && SDL_GetTicksNS() < until) usleep(1000);
    assert(atomic_load(&snapshot_entered));
}
int main(void) {
    char dir[] = "/tmp/la-cache-test-XXXXXX";
    assert(mkdtemp(dir) && chdir(dir) == 0);
    mkdir("cache", 0700);
    FILE *f = fopen("cache/vk_pipelines.bin", "wb"); assert(f); fputs("previous", f); fclose(f);
    atomic_store(&incomplete, 3);
    pipeline_cache_write();
    char previous[9] = {0}; f = fopen("cache/vk_pipelines.bin", "rb"); assert(f);
    assert(fread(previous, 1, 8, f) == 8 && !strcmp(previous, "previous")); fclose(f);
    atomic_store(&hold_snapshot, true); atomic_store(&snapshot_entered, false);
    vk_pipeline_cache_save(); // must return even while the writer cannot finish
    wait_entered();
    for (int i = 0; i < 100; i++) vk_pipeline_cache_save();
    atomic_store(&hold_snapshot, false);
    pipeline_cache_flush_exit(); // drains/coalesces pending writes, then saves final snapshot
    assert(atomic_load(&writes) == 3);
    unsigned char data[65]; f = fopen("cache/vk_pipelines.bin", "rb"); assert(f);
    assert(fread(data, 1, sizeof data, f) == 64); fclose(f);
    for (int i = 0; i < 64; i++) assert(data[i] == 0x5a);
    vk_pipeline_cache_save(); // exit rejects further requests
    assert(!cache_save_pending);
    unlink("cache/vk_pipelines.bin"); rmdir("cache"); chdir("/"); rmdir(dir);
    puts("cache writer: nonblocking requests, coalescing, incomplete snapshot preservation, exit flush passed");
}
