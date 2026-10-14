#!/usr/bin/env python3
"""Compare the standalone module rounding shim with the host libc contract."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source = work / 'test.c'
    source.write_text(r'''
#include <assert.h>
#include <fenv.h>
int la_fegetround(void);
int la_fesetround(int mode);
static unsigned mxcsr(void) {
    unsigned value;
    __asm__ volatile("stmxcsr %0" : "=m"(value));
    return value;
}
int main(void) {
    int original = fegetround();
    int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for (unsigned i = 0; i < 4; ++i) {
        assert(fesetround(modes[i]) == 0);
        unsigned expected = mxcsr();
        assert(la_fegetround() == modes[i]);
        assert(fesetround(modes[(i + 1) % 4]) == 0);
        assert(la_fesetround(modes[i]) == 0);
        assert(fegetround() == modes[i]);
        assert(mxcsr() == expected);
        assert(la_fesetround(1) != 0);
        assert(fegetround() == modes[i] && mxcsr() == expected);
    }
    assert(la_fesetround(original) == 0);
}
''')
    subprocess.run(['clang', '-O2', '-Dfegetround=la_fegetround', '-Dfesetround=la_fesetround',
                    '-c', ROOT / 'ps5/module/fenv-x86.c', '-o', work / 'shim.o'], check=True)
    subprocess.run(['clang', source, work / 'shim.o', '-lm', '-o', work / 'test'], check=True)
    subprocess.run([work / 'test'], check=True)
print('Module rounding matches libc for all four modes and preserves state on invalid input')
