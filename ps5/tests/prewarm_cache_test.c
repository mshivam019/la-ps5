#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../runtime/nvn/vulkan/prewarm_cache.h"

typedef struct { unsigned n; uint32_t types[8], values[8]; } Seen;
static bool visit(uint32_t type, const void *data, uint32_t size, void *opaque) {
    Seen *s = opaque;
    if (size != sizeof(uint32_t) || s->n == 8) return false;
    s->types[s->n] = type;
    memcpy(&s->values[s->n], data, size);
    s->n++;
    return true;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const char *path = argv[1];
    remove(path);
    uint32_t a = 0x12345678, b = 0xabcdef01;
    assert(vk_prewarm_append(path, 7, 1, &a, sizeof a));
    assert(vk_prewarm_append(path, 7, 2, &b, sizeof b));
    Seen s = {0}; unsigned ok = 0, bad = 0;
    assert(vk_prewarm_load(path, 7, visit, &s, &ok, &bad));
    assert(ok == 2 && bad == 0 && s.n == 2 && s.types[0] == 1 && s.values[0] == a && s.types[1] == 2 && s.values[1] == b);
    assert(!vk_prewarm_load(path, 8, visit, &s, &ok, &bad)); // schema invalidation

    FILE *f = fopen(path, "r+b");
    assert(f && fseek(f, -1, SEEK_END) == 0);
    int c = fgetc(f); assert(c != EOF && fseek(f, -1, SEEK_CUR) == 0);
    fputc(c ^ 0x55, f); fclose(f);
    memset(&s, 0, sizeof s);
    assert(vk_prewarm_load(path, 7, visit, &s, &ok, &bad));
    assert(ok == 1 && bad == 1 && s.n == 1); // corrupt tail never poisons prior records

    uint32_t d = 0x55aa55aa;
    assert(vk_prewarm_append(path, 7, 4, &d, sizeof d)); // append repairs the corrupt tail
    memset(&s, 0, sizeof s);
    assert(vk_prewarm_load(path, 7, visit, &s, &ok, &bad));
    assert(ok == 2 && bad == 0 && s.n == 2 && s.types[1] == 4 && s.values[1] == d);

    void *large = calloc(1, VK_PREWARM_MAX_RECORD + 1u);
    assert(large && !vk_prewarm_append(path, 7, 3, large, VK_PREWARM_MAX_RECORD + 1u));
    free(large); remove(path);
    puts("prewarm cache tests passed");
    return 0;
}
