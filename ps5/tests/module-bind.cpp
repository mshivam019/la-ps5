#include "bind.h"

#include <cstring>
#include <iostream>

static void placeholder() {}
void *la_host_symbol(const char *) { return reinterpret_cast<void *>(placeholder); }

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    bool result = la_load_game_module(argv[1]);
    if (!strcmp(argv[2], "ok")) {
        if (!result || !la_game_module_loaded() || !la_main_fn_count || !la_sdk_fn_count) {
            std::cerr << la_game_module_error() << '\n';
            return 1;
        }
    } else {
        if (result || la_game_module_loaded() || !strstr(la_game_module_error(), argv[2])) {
            std::cerr << la_game_module_error() << '\n';
            return 1;
        }
    }
    return 0;
}
