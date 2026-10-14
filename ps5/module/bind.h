#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint64_t *la_main_fn_addrs;
extern const GuestFn *la_main_fn_ptrs;
extern uint64_t la_main_fn_count;
extern const uint64_t *la_sdk_fn_addrs;
extern const GuestFn *la_sdk_fn_ptrs;
extern uint64_t la_sdk_fn_count;

bool la_load_game_module(const char *path);
bool la_game_module_loaded(void);
const char *la_game_module_error(void);
void *la_game_module_symbol(const char *name);

#ifdef __cplusplus
}
#endif
