/**
 * @file hid_keymap.cpp
 * @brief Physical Cardputer keys -> HID keyboard report / mouse input.
 */
#include "hid_keymap.h"

#include <cstring>

using namespace HID;


bool HID::keymapKeyPressed(KEYBOARD::Keyboard* keyboard, const char* name)
{
    if (keyboard == nullptr || name == nullptr)
        return false;

    for (const auto& point : keyboard->keyList())
    {
        if (strcmp(keyboard->getKeyValue(point).value_first, name) == 0)
            return true;
    }
    return false;
}


bool HID::keymapIsFn(KEYBOARD::Keyboard* keyboard)
{
    return keymapKeyPressed(keyboard, "fn");
}


uint8_t HID::keymapModifier(const KEYBOARD::Keyboard::KeysState& state, bool caps_locked)
{
    uint8_t modifier = 0;
    if (state.ctrl)
        modifier |= 0x01; // left ctrl
    if (state.shift || caps_locked)
        modifier |= 0x02; // left shift
    if (state.alt)
        modifier |= 0x04; // left alt
    if (state.opt)
        modifier |= 0x08; // left GUI
    return modifier;
}


void HID::keymapBuildKeys(const KEYBOARD::Keyboard::KeysState& state, uint8_t keys[6])
{
    memset(keys, 0, 6);
    int count = 0;
    for (int hid_key : state.hidKey)
    {
        if (count >= 6)
            break;
        if (hid_key == 0)
            continue;
        keys[count++] = static_cast<uint8_t>(hid_key);
    }
}


HID::MouseInput HID::keymapReadMouse(KEYBOARD::Keyboard* keyboard)
{
    MouseInput input;
    if (keyboard == nullptr)
        return input;

    if (keymapKeyPressed(keyboard, "w"))
    {
        input.dy -= 1;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "s"))
    {
        input.dy += 1;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "a"))
    {
        input.dx -= 1;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "d"))
    {
        input.dx += 1;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "r"))
    {
        input.wheel -= 1;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "f"))
    {
        input.wheel += 1;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "space"))
    {
        input.buttons |= kBtnLeft;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "enter"))
    {
        input.buttons |= kBtnRight;
        input.any_key = true;
    }
    if (keymapKeyPressed(keyboard, "m"))
    {
        input.buttons |= kBtnMiddle;
        input.any_key = true;
    }
    return input;
}
