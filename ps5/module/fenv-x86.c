/* Standalone x86 floating-point rounding support for Linux-generated modules.
 * Keep host libc dependencies out of the private PS5 game module. */
#include <fenv.h>

_Static_assert(FE_TONEAREST == 0 && FE_DOWNWARD == 0x400 &&
               FE_UPWARD == 0x800 && FE_TOWARDZERO == 0xc00,
               "This shim requires the Linux x86 fenv encoding");

__attribute__((visibility("hidden"))) int fegetround(void) {
    unsigned short control;
    __asm__ volatile("fnstcw %0" : "=m"(control));
    return control & 0xc00;
}

__attribute__((visibility("hidden"))) int fesetround(int mode) {
    if (mode & ~0xc00) return -1;
    unsigned short control;
    unsigned int mxcsr;
    __asm__ volatile("fnstcw %0" : "=m"(control));
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    control = (control & ~0xc00) | mode;
    mxcsr = (mxcsr & ~0x6000) | ((unsigned int)mode << 3);
    __asm__ volatile("fldcw %0" : : "m"(control));
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    return 0;
}
