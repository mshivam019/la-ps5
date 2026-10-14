#include "prewarm_cache.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

enum { FILE_MAGIC = 0x5752504c, RECORD_MAGIC = 0x5243504c }; // LPRW / LPCR
typedef struct { uint32_t magic, schema, header_size, reserved; } FileHeader;
typedef struct { uint32_t magic, type, size, checksum; } RecordHeader;

static pthread_mutex_t append_lock = PTHREAD_MUTEX_INITIALIZER;

static bool truncate_at(FILE *f, size_t size) {
    fflush(f);
#ifdef _WIN32
    return _chsize_s(_fileno(f), size) == 0;
#else
    return ftruncate(fileno(f), (off_t)size) == 0;
#endif
}

static uint32_t checksum(const void *data, size_t size, uint32_t type) {
    const uint8_t *p = data;
    uint32_t h = 2166136261u ^ type ^ (uint32_t)size;
    for (size_t i = 0; i < size; i++) { h ^= p[i]; h *= 16777619u; }
    return h ? h : 1;
}

bool vk_prewarm_load(const char *path, uint32_t schema, VkPrewarmVisit visit, void *opaque,
                     unsigned *accepted, unsigned *rejected) {
    if (accepted) *accepted = 0;
    if (rejected) *rejected = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    FileHeader fh;
    if (fread(&fh, 1, sizeof fh, f) != sizeof fh || fh.magic != FILE_MAGIC || fh.schema != schema ||
        fh.header_size != sizeof fh) { fclose(f); return false; }
    size_t used = sizeof fh;
    for (unsigned n = 0; n < VK_PREWARM_MAX_RECORDS && used + sizeof(RecordHeader) <= VK_PREWARM_MAX_FILE; n++) {
        RecordHeader rh;
        size_t got = fread(&rh, 1, sizeof rh, f);
        if (!got) break;
        if (got != sizeof rh || rh.magic != RECORD_MAGIC || !rh.size || rh.size > VK_PREWARM_MAX_RECORD ||
            used + sizeof rh + rh.size > VK_PREWARM_MAX_FILE) {
            if (rejected) (*rejected)++;
            break;
        }
        void *data = malloc(rh.size);
        if (!data || fread(data, 1, rh.size, f) != rh.size) {
            free(data);
            if (rejected) (*rejected)++;
            break;
        }
        used += sizeof rh + rh.size;
        bool ok = checksum(data, rh.size, rh.type) == rh.checksum && (!visit || visit(rh.type, data, rh.size, opaque));
        free(data);
        if (ok) { if (accepted) (*accepted)++; }
        else if (rejected) (*rejected)++;
    }
    fclose(f);
    return true;
}

bool vk_prewarm_append(const char *path, uint32_t schema, uint32_t type, const void *data, uint32_t size) {
    if (!data || !size || size > VK_PREWARM_MAX_RECORD) return false;
    pthread_mutex_lock(&append_lock);
    struct stat st;
    bool fresh = stat(path, &st) != 0;
    FILE *f = fopen(path, fresh ? "wb" : "r+b");
    if (!f) { pthread_mutex_unlock(&append_lock); return false; }
    FileHeader fh;
    if (fresh) {
        fh = (FileHeader){FILE_MAGIC, schema, sizeof fh, 0};
        if (fwrite(&fh, 1, sizeof fh, f) != sizeof fh) { fclose(f); pthread_mutex_unlock(&append_lock); return false; }
    } else {
        bool valid = fread(&fh, 1, sizeof fh, f) == sizeof fh && fh.magic == FILE_MAGIC && fh.schema == schema &&
                     fh.header_size == sizeof fh;
        if (!valid) {
            fclose(f);
            f = fopen(path, "wb");
            fh = (FileHeader){FILE_MAGIC, schema, sizeof fh, 0};
            if (!f || fwrite(&fh, 1, sizeof fh, f) != sizeof fh) {
                if (f) fclose(f);
                pthread_mutex_unlock(&append_lock);
                return false;
            }
        } else {
            // Recover a valid prefix after a power loss/torn final append.  New
            // records must never be hidden forever behind a corrupt tail.
            size_t good = sizeof fh;
            unsigned records = 0;
            for (; records < VK_PREWARM_MAX_RECORDS; records++) {
                RecordHeader old;
                size_t got = fread(&old, 1, sizeof old, f);
                if (!got) break;
                if (got != sizeof old || old.magic != RECORD_MAGIC || !old.size || old.size > VK_PREWARM_MAX_RECORD ||
                    good + sizeof old + old.size > VK_PREWARM_MAX_FILE) break;
                void *payload = malloc(old.size);
                bool valid_record = payload && fread(payload, 1, old.size, f) == old.size &&
                                    checksum(payload, old.size, old.type) == old.checksum;
                free(payload);
                if (!valid_record) break;
                good += sizeof old + old.size;
            }
            if (records == VK_PREWARM_MAX_RECORDS || good + sizeof(RecordHeader) + size > VK_PREWARM_MAX_FILE ||
                !truncate_at(f, good) || fseek(f, (long)good, SEEK_SET) != 0) {
                fclose(f); pthread_mutex_unlock(&append_lock); return false;
            }
        }
    }
    RecordHeader rh = {RECORD_MAGIC, type, size, checksum(data, size, type)};
    bool ok = fwrite(&rh, 1, sizeof rh, f) == sizeof rh && fwrite(data, 1, size, f) == size && fflush(f) == 0;
    fclose(f);
    pthread_mutex_unlock(&append_lock);
    return ok;
}
