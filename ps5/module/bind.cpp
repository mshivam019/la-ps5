#include "bind.h"
#include "loader.h"
#include "abi.h"

#include <cstring>
#include <stdexcept>
#include <string>

const uint64_t *la_main_fn_addrs;
const GuestFn *la_main_fn_ptrs;
uint64_t la_main_fn_count;
const uint64_t *la_sdk_fn_addrs;
const GuestFn *la_sdk_fn_ptrs;
uint64_t la_sdk_fn_count;

namespace {
game_module::Image module;
std::string module_error = "Game module has not been loaded";
bool loaded;

void *required(const char *name, size_t size = 1) {
    void *result = module.symbol(name, size);
    if (!result) throw std::runtime_error(std::string("Missing game module symbol: ") + name);
    return result;
}

void validate_table(const char *label, const uint64_t *addresses, const GuestFn *functions,
                    uint64_t count, uint64_t low, uint64_t high) {
    if (!count || count > 1000000) throw std::runtime_error(std::string("Invalid ") + label + " function count");
    uint64_t previous = 0;
    for (uint64_t i = 0; i < count; ++i) {
        if (addresses[i] < low || addresses[i] >= high || (i && addresses[i] <= previous) || !functions[i])
            throw std::runtime_error(std::string("Invalid ") + label + " function table");
        previous = addresses[i];
    }
}
}

void *la_host_symbol(const char *);

extern "C" bool la_load_game_module(const char *path) {
    if (loaded) return true;
    if (!module.load(path, la_host_symbol, module_error)) return false;
    try {
        if (memcmp(required("la_game_abi", sizeof(LA_MODULE_ABI)), LA_MODULE_ABI, sizeof(LA_MODULE_ABI)))
            throw std::runtime_error("Game module is for a different app version; rerun setup from the matching release");
        auto main_count = *static_cast<const uint64_t *>(required("main_fn_count", sizeof(uint64_t)));
        auto sdk_count = *static_cast<const uint64_t *>(required("sdk_fn_count", sizeof(uint64_t)));
        if (!main_count || main_count > 1000000 || !sdk_count || sdk_count > 1000000)
            throw std::runtime_error("Invalid game module function counts");
        auto main_addresses = static_cast<const uint64_t *>(required("main_fn_addrs", main_count * sizeof(uint64_t)));
        auto main_functions = static_cast<const GuestFn *>(required("main_fn_ptrs", main_count * sizeof(GuestFn)));
        auto sdk_addresses = static_cast<const uint64_t *>(required("sdk_fn_addrs", sdk_count * sizeof(uint64_t)));
        auto sdk_functions = static_cast<const GuestFn *>(required("sdk_fn_ptrs", sdk_count * sizeof(GuestFn)));
        validate_table("main", main_addresses, main_functions, main_count, 0x7000000000ull, 0x7004000000ull);
        validate_table("SDK", sdk_addresses, sdk_functions, sdk_count, 0x7004000000ull, 0x7080000000ull);
        la_main_fn_addrs = main_addresses;
        la_main_fn_ptrs = main_functions;
        la_main_fn_count = main_count;
        la_sdk_fn_addrs = sdk_addresses;
        la_sdk_fn_ptrs = sdk_functions;
        la_sdk_fn_count = sdk_count;
        module_error.clear();
        loaded = true;
        return true;
    } catch (const std::exception &ex) {
        module_error = ex.what();
        return false;
    }
}

extern "C" bool la_game_module_loaded(void) { return loaded; }
extern "C" const char *la_game_module_error(void) { return module_error.c_str(); }
extern "C" void *la_game_module_symbol(const char *name) { return loaded ? module.symbol(name) : nullptr; }
