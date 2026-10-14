// Native SDL controller bridge, shared with the Dusklight PS5 port.
#include <SDL3/SDL.h>
#include "../native/platform.h"
#include <algorithm>

namespace {
struct Player { int slot; SDL_JoystickID id = 0; SDL_Joystick* joystick = nullptr; };
Player players[PAD_PLAYERS]{{0},{1},{2},{3}};
bool rumble(void* data, Uint16 low, Uint16 high) {
    pad_player_vibrate(static_cast<Player*>(data)->slot, low / 65535.f, high / 65535.f);
    return true;
}
bool led(void* data, Uint8 r, Uint8 g, Uint8 b) {
    pad_player_light_bar(static_cast<Player*>(data)->slot, r, g, b);
    return true;
}
unsigned button_mask(int button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return PAD_CROSS;
    case SDL_GAMEPAD_BUTTON_EAST: return PAD_CIRCLE;
    case SDL_GAMEPAD_BUTTON_WEST: return PAD_SQUARE;
    case SDL_GAMEPAD_BUTTON_NORTH: return PAD_TRIANGLE;
    case SDL_GAMEPAD_BUTTON_START: return PAD_OPTIONS;
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return PAD_TOUCH_PAD;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return PAD_L3;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return PAD_R3;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return PAD_L1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return PAD_R1;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return PAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return PAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return PAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return PAD_RIGHT;
    default: return 0;
    }
}
Sint16 axis(float value) {
    return static_cast<Sint16>(std::clamp(value * 32767.f, -32768.f, 32767.f));
}
void detach(Player& p) {
    pad_player_vibrate(p.slot, 0, 0);
    if (p.joystick) SDL_CloseJoystick(p.joystick);
    if (p.id) SDL_DetachVirtualJoystick(p.id);
    p.id = 0; p.joystick = nullptr;
}
}
extern "C" void ps5_input_initialize() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    pad_open();
}
extern "C" void ps5_input_shutdown() { for (auto& p : players) detach(p); }
extern "C" void ps5_input_poll() {
    pad first{}; pad_poll(&first);
    for (auto& p : players) {
        pad state{};
        if (!pad_player(p.slot, &state)) { if (p.id) detach(p); continue; }
        if (!p.id) {
            SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
            desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
            desc.naxes = SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
            desc.axis_mask = (1u << desc.naxes) - 1;
            desc.button_mask = (1u << desc.nbuttons) - 1;
            desc.name = "PS5 Native Controller"; desc.userdata = &p;
            desc.Rumble = rumble; desc.SetLED = led;
            p.id = SDL_AttachVirtualJoystick(&desc);
            if (!p.id) { say("Controller attach failed: %s", SDL_GetError()); continue; }
            p.joystick = SDL_OpenJoystick(p.id);
            if (!p.joystick) { detach(p); continue; }
            SDL_SetJoystickPlayerIndex(p.joystick, p.slot);
            say("[input] native controller attached: player=%d id=%u", p.slot, unsigned(p.id));
        }
        const float axes[] = {state.left_x, state.left_y, state.right_x,
            state.right_y, state.l2 * 2 - 1, state.r2 * 2 - 1};
        for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
            SDL_SetJoystickVirtualAxis(p.joystick, i, axis(axes[i]));
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
            // Touchpad opens the host menu. Both stick clicks supply Switch Minus.
            const bool down = i == SDL_GAMEPAD_BUTTON_BACK
                ? (state.held & (PAD_L3 | PAD_R3)) == (PAD_L3 | PAD_R3)
                : (state.held & button_mask(i)) != 0;
            SDL_SetJoystickVirtualButton(p.joystick, i, down);
        }
    }
}
