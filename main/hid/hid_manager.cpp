/**
 * @file hid_manager.cpp
 * @brief Transport abstraction and channel switching.
 */
#include "hid_manager.h"

#include "esp_log.h"

using namespace HID;

static const char* TAG = "hid_mgr";


void Manager::begin()
{
    _config.begin();

    Channel channel = static_cast<Channel>(_config.config().channel);
    _activate(channel, false);
}


void Manager::_activate(Channel channel, bool persist)
{
    // Release everything on the old transport before touching the new one so
    // the previous host does not keep a stuck key/button.
    if (_started)
    {
        kbReleaseAll();
        mouseButton(0);
    }

    const uint8_t index = static_cast<uint8_t>(channel);

    if (channel == Channel::Usb)
    {
        _usb.begin();
    }
    else
    {
        _ble.begin(index - 1, &_config);
    }

    _started = true;
    _status.channel = channel;
    _status.active_slot = (channel == Channel::Usb) ? -1 : static_cast<int8_t>(index - 1);

    if (persist)
        _config.saveChannel(index);

    _refreshStatus();

    ESP_LOGI(TAG, "active channel %u", index);
}


void Manager::switchChannel(Channel channel)
{
    _activate(channel, true);
}


void Manager::unpairSlot(Channel channel)
{
    if (channel == Channel::Usb)
        return;
    _ble.unpairSlot(static_cast<uint8_t>(channel) - 1);
    _refreshStatus();
}


void Manager::unpairCurrentSlot()
{
    if (_status.channel == Channel::Usb)
        return;
    _ble.unpair();
    _refreshStatus();
}


void Manager::setMouseMode(bool on)
{
    if (on == _mouse_mode)
        return;

    // Never leave the old mode with dangling keys/buttons.
    if (_started)
    {
        kbReleaseAll();
        mouseButton(0);
    }

    _mouse_mode = on;
    _status.mouse_mode = on;
    ESP_LOGI(TAG, "mouse mode %s", on ? "on" : "off");
}


void Manager::toggleMouseMode()
{
    setMouseMode(!_mouse_mode);
}


void Manager::kbReport(uint8_t modifier, const uint8_t keys[6])
{
    if (!_started)
        return;

    if (_status.channel == Channel::Usb)
        _usb.kbReport(modifier, keys);
    else
        _ble.kbReport(modifier, keys);
}


void Manager::kbReleaseAll()
{
    const uint8_t empty[6] = {0};
    kbReport(0, empty);
}


void Manager::mouseMove(int8_t dx, int8_t dy, int8_t wheel)
{
    if (!_started)
        return;

    if (_status.channel == Channel::Usb)
        _usb.mouseMove(dx, dy, wheel);
    else
        _ble.mouseMove(dx, dy, wheel);
}


void Manager::mouseButton(uint8_t mask)
{
    if (!_started)
        return;

    if (_status.channel == Channel::Usb)
        _usb.mouseButton(mask);
    else
        _ble.mouseButton(mask);
}


void Manager::setBattery(uint8_t level)
{
    _status.battery = level;
    _ble.setBattery(level);
}


void Manager::_refreshStatus()
{
    for (uint8_t i = 0; i < ConfigStore::kSlotNum; i++)
        _status.slot_bonded[i] = _ble.slotBonded(i);

    if (_status.channel == Channel::Usb)
    {
        _status.connected = _usb.mounted();
        _status.bonded = false;
        _status.advertising = false;
    }
    else
    {
        _status.connected = _ble.connected();
        _status.bonded = _ble.bonded();
        _status.advertising = _ble.advertising();
    }

    _status.mouse_mode = _mouse_mode;
}


void Manager::poll()
{
    _ble.poll();
    _refreshStatus();
}
