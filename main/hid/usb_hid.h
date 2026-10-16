/**
 * @file usb_hid.h
 * @brief USB HID transport (keyboard + mouse) built on the TinyUSB helpers.
 */
#pragma once
#include <stdint.h>

namespace HID
{
    class UsbHid
    {
    public:
        /* Route the USB PHY to OTG and start the TinyUSB device task (once). */
        void begin();

        bool mounted() const;

        void kbReport(uint8_t modifier, const uint8_t keys[6]);
        void mouseMove(int8_t dx, int8_t dy, int8_t wheel);
        void mouseButton(uint8_t mask);
    };
} // namespace HID
