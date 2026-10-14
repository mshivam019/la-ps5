// Regression: projective IPA must use its explicit divisor, including destination aliasing and RZ.
#include "../../runtime/nvn/maxwell/shader.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    for (int glsl = 0; glsl < 2; ++glsl) {
        for (int variant = 0; variant < 4; ++variant) {
            uint64_t code[32] = {0};
            int src = variant == 0 ? 7 : variant == 1 ? 1 : 255;
            int mode = variant == 3 ? 0 : 1;
            code[15] = 15; // fragment output map at 0x78
            code[17] = 0xe00000000000ff00ull | ((uint64_t)mode << 54) |
                       ((uint64_t)0xc0 << 28) | ((uint64_t)src << 20) | (7ull << 16) | 1;
            code[18] = 0xe30000000007000full;
            code[19] = 0xe2400fffff87000full;
            size_t span = maxwell_shader_span((uint8_t *)code, sizeof code, STAGE_FRAGMENT);
            assert(span == 0x98); // excludes the self-branch and unrelated trailing bytes
            code[25] = UINT64_MAX;
            assert(maxwell_shader_span((uint8_t *)code, sizeof code, STAGE_FRAGMENT) == span);
            assert(maxwell_shader_span((uint8_t *)code, 16, STAGE_FRAGMENT) == 0);
            ShaderKey key = {.glsl = glsl, .vs_out_mask = 1u << 4};
            ShaderInfo *s = maxwell_translate((uint8_t *)code, sizeof code, STAGE_FRAGMENT, &key);
            assert(s && s->msl && !s->error[0]);
            char expected[192];
            const char *w = glsl ? "gl_FragCoord.w" : "in.pos.w";
            const char *v = glsl ? "v4" : "in.v4";
            const char *af = glsl ? "as_f" : "as_type<float>";
            if (mode == 1) {
                char r[16];
                if (src == 255) strcpy(r, "0u"); else snprintf(r, sizeof r, "r%d", src);
                snprintf(expected, sizeof expected, "(%s[0] * %s) * %s(%s)", v, w, af, r);
            } else snprintf(expected, sizeof expected, "%s[0] * %s", v, w);
            if (!strstr(s->msl, expected)) { fprintf(stderr, "Missing %s\n%s", expected, s->msl); return 1; }
            if (glsl) {
                char path[100]; snprintf(path, sizeof path, "build/test-ipa-%d.frag", variant);
                FILE *f = fopen(path, "w"); assert(f); fputs(s->msl, f); fclose(f);
            }
            maxwell_free(s);
        }
    }
    puts("IPA projective divisor, destination alias, RZ, and PASS regressions passed (MSL + GLSL)");
}
