/**
 * @file app_hid_os.cpp
 * @brief The single HID OS app: boot, key interpretation, mouse mode, HUD.
 */
#include "app_hid_os.h"

#include "spdlog/spdlog.h"

#include "../../hid/hid_keymap.h"
#include "../utils/common_define.h"

using namespace MOONCAKE::APPS;
using namespace HID;

static constexpr uint32_t kLongPressMs = 1000;
static constexpr uint32_t kMouseIntervalMs = 15;
static constexpr uint32_t kBatteryIntervalMs = 5000;
static constexpr uint32_t kHudIntervalMs = 250;


static bool statusEqual(const HID::Status& a, const HID::Status& b)
{
    if (a.channel != b.channel || a.connected != b.connected || a.bonded != b.bonded ||
        a.advertising != b.advertising || a.mouse_mode != b.mouse_mode || a.battery != b.battery ||
        a.active_slot != b.active_slot)
        return false;
    for (int i = 0; i < 3; i++)
    {
        if (a.slot_bonded[i] != b.slot_bonded[i])
            return false;
    }
    return true;
}


void AppHidOs::onCreate()
{
    spdlog::info("{} onCreate", getAppName());

    _data.hal = mcAppGetDatabase()->Get("HAL")->value<HAL::Hal*>();

    _data.manager.begin();
    _data.hud.begin(_data.hal);

    _last_status = _data.manager.status();
    _data.force_render = true;
}


void AppHidOs::onResume()
{
    _data.force_render = true;
}


void AppHidOs::onRunning()
{
    auto* keyboard = _data.hal->keyboard();

    keyboard->updateKeyList();
    keyboard->updateKeysState();

    _data.manager.poll();

    _handleFnCombos();

    if (_data.manager.mouseMode())
        _handleMouse();
    else
        _handleKeyboard();

    _updateBattery(false);
    _updateHud(false);
}


void AppHidOs::onDestroy()
{
    spdlog::info("{} onDestroy", getAppName());
}


void AppHidOs::_handleFnCombos()
{
    auto* keyboard = _data.hal->keyboard();
    const bool fn = keymapIsFn(keyboard);

    // fn+M toggles mouse mode.
    const bool fn_m = fn && keymapKeyPressed(keyboard, "m");
    if (fn_m && !_data.fn_m_held)
    {
        _data.manager.toggleMouseMode();
        _data.hal->playNextSound();
        _data.force_render = true;
    }
    _data.fn_m_held = fn_m;

    // fn + 0/1/2/3 selects the channel.
    int channel = -1;
    if (fn)
    {
        if (keymapKeyPressed(keyboard, "1"))
            channel = 1;
        else if (keymapKeyPressed(keyboard, "2"))
            channel = 2;
        else if (keymapKeyPressed(keyboard, "3"))
            channel = 3;
        else if (keymapKeyPressed(keyboard, "0"))
            channel = 0;
    }

    if (channel >= 0)
    {
        if (!_fn.active || _fn.channel != channel)
        {
            _fn.active = true;
            _fn.channel = channel;
            _fn.long_fired = false;
            _fn.start_ms = millis();
        }
        else if (!_fn.long_fired && channel != 0 && (millis() - _fn.start_ms) >= kLongPressMs)
        {
            // Long press on a BT slot: unpair it and end up on that slot so it
            // can be paired again.
            _fn.long_fired = true;
            const bool already_here = (static_cast<int>(_data.manager.status().channel) == channel);
            _data.manager.unpairSlot(static_cast<Channel>(channel));
            if (!already_here)
                _data.manager.switchChannel(static_cast<Channel>(channel));
            _data.hal->playKeyboardSound();
            _data.force_render = true;
        }
    }
    else if (_fn.active)
    {
        // Released without a long press: switch.
        if (!_fn.long_fired)
        {
            _data.manager.switchChannel(static_cast<Channel>(_fn.channel));
            _data.hal->playNextSound();
            _data.force_render = true;
        }
        _fn.active = false;
        _fn.channel = -1;
    }
}


void AppHidOs::_handleKeyboard()
{
    auto* keyboard = _data.hal->keyboard();

    // While fn is held all keys are OS combos; never leak them to the host.
    if (keymapIsFn(keyboard))
    {
        if (keyboard->isChanged())
            _data.manager.kbReleaseAll();
        return;
    }

    if (!keyboard->isChanged())
        return;

    const auto& state = keyboard->keysState();
    uint8_t modifier = keymapModifier(state, keyboard->capslocked());
    uint8_t keys[6];
    keymapBuildKeys(state, keys);
    _data.manager.kbReport(modifier, keys);
}


void AppHidOs::_handleMouse()
{
    auto* keyboard = _data.hal->keyboard();
    MouseInput input = keymapReadMouse(keyboard);
    const uint32_t now = millis();

    // fn combos are OS controls; do not treat them as mouse input.
    if (keymapIsFn(keyboard))
        input = MouseInput();

    // Buttons react immediately.
    if (input.buttons != _data.last_buttons)
    {
        _data.manager.mouseButton(input.buttons);
        _data.last_buttons = input.buttons;
        _data.force_render = true;
    }

    if ((now - _data.mouse_ms) < kMouseIntervalMs)
        return;
    _data.mouse_ms = now;

    int speed = _data.manager.config().config().mouse_speed;
    if (speed < 1)
        speed = 1;

    if (input.dx != 0 || input.dy != 0)
    {
        if (_data.mouse_hold_ms == 0)
            _data.mouse_hold_ms = now;

        if (_data.manager.config().config().mouse_accel > 0)
        {
            const uint32_t held = now - _data.mouse_hold_ms;
            if (held > 800)
                speed *= 3;
            else if (held > 300)
                speed *= 2;
        }

        _data.manager.mouseMove(static_cast<int8_t>(input.dx * speed),
                                static_cast<int8_t>(input.dy * speed),
                                input.wheel);
    }
    else
    {
        _data.mouse_hold_ms = 0;
        if (input.wheel != 0)
            _data.manager.mouseMove(0, 0, input.wheel);
    }
}


void AppHidOs::_updateBattery(bool force)
{
    const uint32_t now = millis();
    if (!force && _data.battery_ms != 0 && (now - _data.battery_ms) < kBatteryIntervalMs)
        return;

    _data.battery_ms = now;
    _data.manager.setBattery(_data.hal->getBatLevel());
    _data.force_render = true;
}


void AppHidOs::_updateHud(bool force)
{
    const HID::Status& status = _data.manager.status();
    const uint32_t now = millis();

    const bool changed = !statusEqual(status, _last_status);
    if (!force && !_data.force_render && !changed && (now - _data.hud_ms) < kHudIntervalMs)
        return;

    _data.hud_ms = now;
    _data.force_render = false;
    _last_status = status;
    _data.hud.render(status);
}
