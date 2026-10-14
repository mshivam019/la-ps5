// Exercise the native bridge through real SDL virtual-device/event handling.
#include <SDL3/SDL.h>
#include <cassert>
#include <cstdarg>
#include "../native/platform.h"
extern "C" void ps5_input_initialize();
extern "C" void ps5_input_poll();
extern "C" void ps5_input_shutdown();
static pad sample{};
static bool connected = true;
extern "C" bool pad_open() { return true; }
extern "C" void pad_poll(pad *out) { *out = sample; }
extern "C" bool pad_player(int slot, pad *out) { *out = sample; return slot == 0 && connected; }
extern "C" void pad_player_vibrate(int, float, float) {}
extern "C" void pad_player_light_bar(int, uint8_t, uint8_t, uint8_t) {}
extern "C" void say(const char *, ...) {}
static void poll() {
    ps5_input_poll();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
}
int main() {
    assert(SDL_Init(SDL_INIT_GAMEPAD));
    ps5_input_initialize();
    poll();
    int count = 0;
    auto ids = SDL_GetGamepads(&count);
    assert(count == 1);
    auto gamepad = SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
    assert(gamepad);
    sample.held = PAD_CROSS | PAD_OPTIONS | PAD_TOUCH_PAD;
    sample.left_x = 0.5f; sample.l2 = 1.f;
    poll();
    assert(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH));
    assert(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START));
    assert(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD));
    assert(!SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK));
    assert(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) > 16000);
    assert(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 32000);
    assert(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) == 0);
    sample = {}; sample.held = PAD_L3 | PAD_R3;
    poll();
    assert(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK));
    assert(!SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH));
    connected = false; poll();
    assert(!SDL_GamepadConnected(gamepad));
    SDL_CloseGamepad(gamepad);
    connected = true; poll();
    ids = SDL_GetGamepads(&count); assert(count == 1); SDL_free(ids);
    ps5_input_shutdown();
    SDL_Quit();
    SDL_Log("PASS: native bridge mapping, triggers, disconnect and reconnect through SDL");
}
