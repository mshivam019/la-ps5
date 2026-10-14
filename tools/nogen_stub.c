// Stand-in for the generated code (generated/, made by `make gen` from the user's game files) so the runtime
// links before that step: the start screen works (it sets up the game files), booting the game does not.
#include "cpu.h"

static void no_code(Cpu *c) {
    (void)c;
    rt_udf(c, c->pc);
}

const uint64_t main_fn_addrs[] = {0}, sdk_fn_addrs[] = {0};
const GuestFn main_fn_ptrs[] = {no_code}, sdk_fn_ptrs[] = {no_code};
const uint64_t main_fn_count = 1, sdk_fn_count = 1;
