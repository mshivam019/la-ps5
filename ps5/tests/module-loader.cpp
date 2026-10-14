#include "loader.h"

#include <cassert>
#include <cstring>
#include <iostream>

static void host_throw() { throw 1234; }
static int twice(int n) { return n * 2; }
static int override_value() { return 40; }

void *resolve(const char *name) {
    if (!strcmp(name, "host_throw")) return reinterpret_cast<void *>(host_throw);
    if (!strcmp(name, "host_twice")) return reinterpret_cast<void *>(twice);
    if (!strcmp(name, "weak_value")) return reinterpret_cast<void *>(override_value);
    return nullptr;
}

int main(int argc, char **argv) {
    assert(argc >= 3);
    std::string error;
    game_module::Image good;
    if (!good.load(argv[1], resolve, error)) { std::cerr << error << '\n'; return 1; }
    auto function = reinterpret_cast<int (*)(int)>(good.symbol("run"));
    assert(function && function(3) == 53);
    auto unwind = reinterpret_cast<void (*)()>(good.symbol("unwind_test"));
    assert(unwind);
    bool caught = false;
    try { unwind(); } catch (int value) { caught = value == 1234; }
    assert(caught);
    assert(!good.symbol("missing"));
    assert(!good.symbol("value", 100000));
    assert(!good.load(argv[1], resolve, error));
    for (int i = 2; i < argc; ++i) {
        game_module::Image bad;
        assert(!bad.load(argv[i], resolve, error));
        assert(!error.empty());
    }
    game_module::Image missing_import;
    assert(!missing_import.load(argv[1], [](const char *) -> void * { return nullptr; }, error));
    assert(error.find("Missing runtime symbol") != std::string::npos);
    std::cout << "Module execution, relocations, weak overrides, unwinding, and invalid inputs passed\n";
}
