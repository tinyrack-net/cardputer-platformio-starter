/**
 * @file hid_manager.h
 * @brief Transport abstraction: one active channel (USB / BT1 / BT2 / BT3),
 *        keyboard + mouse report routing, runtime channel switching.
 */
#pragma once
#include <stdint.h>

#include "ble_hid.h"
#include "config_store.h"
#include "usb_hid.h"

namespace HID
{
    enum class Channel : uint8_t
    {
        Usb = 0,
        Bt1 = 1,
        Bt2 = 2,
        Bt3 = 3,
    };

    struct Status
    {
        Channel channel = Channel::Usb;
        bool connected = false;   // USB mounted / BLE connected
        bool bonded = false;      // current BT slot has a bond
        bool advertising = false;
        bool mouse_mode = false;
        uint8_t battery = 100;
        bool slot_bonded[3] = {false, false, false};
        int8_t active_slot = -1;  // -1 = USB
    };

    class Manager
    {
    public:
        void begin();

        void switchChannel(Channel channel);
        void unpairSlot(Channel channel);
        void unpairCurrentSlot();

        void setMouseMode(bool on);
        void toggleMouseMode();
        bool mouseMode() const { return _mouse_mode; }

        void kbReport(uint8_t modifier, const uint8_t keys[6]);
        void kbReleaseAll();
        void mouseMove(int8_t dx, int8_t dy, int8_t wheel);
        void mouseButton(uint8_t mask);

        const Status& status() const { return _status; }
        void setBattery(uint8_t level);

        void poll();

        ConfigStore& config() { return _config; }

    private:
        void _activate(Channel channel, bool persist);
        void _refreshStatus();

        ConfigStore _config;
        UsbHid _usb;
        BleHid& _ble = BleHid::instance();

        Status _status;
        bool _mouse_mode = false;
        bool _started = false;
    };
} // namespace HID
