#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Small append-only record store used by the shader prewarmer.  The caller owns
// record schemas; this layer only supplies versioning, bounds and torn-write /
// corruption detection.
enum {
    VK_PREWARM_MAX_FILE = 16 * 1024 * 1024,
    VK_PREWARM_MAX_RECORD = 64 * 1024,
    VK_PREWARM_MAX_RECORDS = 16384,
};

typedef bool (*VkPrewarmVisit)(uint32_t type, const void *data, uint32_t size, void *opaque);

bool vk_prewarm_load(const char *path, uint32_t schema, VkPrewarmVisit visit, void *opaque,
                     unsigned *accepted, unsigned *rejected);
bool vk_prewarm_append(const char *path, uint32_t schema, uint32_t type, const void *data, uint32_t size);

