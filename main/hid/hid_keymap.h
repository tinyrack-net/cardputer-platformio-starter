/**
 * @file hid_keymap.h
 * @brief Physical Cardputer keys -> HID keyboard report / mouse input.
 */
#pragma once
#include <stdint.h>

#include "../hal/keyboard/keyboard.h"

namespace HID
{
    /* Mouse button bit mask (matches the HID boot mouse / TinyUSB helpers). */
    constexpr uint8_t kBtnLeft = 1u << 0;
    constexpr uint8_t kBtnRight = 1u << 1;
    constexpr uint8_t kBtnMiddle = 1u << 2;

    struct MouseInput
    {
        int8_t dx = 0;
        int8_t dy = 0;
        int8_t wheel = 0;
        uint8_t buttons = 0;
        bool any_key = false;
    };

    /* True if the physical key whose `value_first` is `name` is currently held. */
    bool keymapKeyPressed(KEYBOARD::Keyboard* keyboard, const char* name);

    bool keymapIsFn(KEYBOARD::Keyboard* keyboard);

    /* Build a HID keyboard modifier byte from the decoded key state. */
    uint8_t keymapModifier(const KEYBOARD::Keyboard::KeysState& state, bool caps_locked);

    /* Fill `keys` (6 slots, zero padded) from the decoded key state. */
    void keymapBuildKeys(const KEYBOARD::Keyboard::KeysState& state, uint8_t keys[6]);

    /* Read the mouse-mode key state (movement axes, wheel, buttons). */
    MouseInput keymapReadMouse(KEYBOARD::Keyboard* keyboard);
} // namespace HID
