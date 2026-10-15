/**
 * @file config_store.h
 * @brief NVS persistence for the HID OS (channel, mouse settings, pairing slots).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace HID
{
    struct Config
    {
        uint8_t channel = 0; // 0 = USB, 1 = BT1, 2 = BT2, 3 = BT3
        uint8_t mouse_speed = 1;
        int8_t mouse_accel = 1; // >0 = acceleration on
    };

    /* NVS namespace "hid_os" wrapper. Bluedroid keeps its own bond keys; we only
     * store the slot -> bonded address mapping here. */
    class ConfigStore
    {
    public:
        static constexpr size_t kAddrLen = 7; // addr_type(1) + bd_addr(6)
        static constexpr size_t kSlotNum = 3;

        /* Init NVS (erase + retry on version/page errors) and load the cache. */
        bool begin();

        const Config& config() const { return _config; }

        void saveChannel(uint8_t channel);
        void saveMouse(uint8_t speed, int8_t accel);
        void saveDeviceName(const char* name);

        /* Slot address: byte 0 is the BLE address type, bytes 1..6 the address. */
        bool getSlotAddr(uint8_t slot, uint8_t out[kAddrLen]) const;
        bool setSlotAddr(uint8_t slot, const uint8_t addr[kAddrLen]);
        void clearSlotAddr(uint8_t slot);
        bool isSlotBonded(uint8_t slot) const;

        const char* deviceName() const { return _device_name; }

    private:
        Config _config;
        uint8_t _slot_addr[kSlotNum][kAddrLen] = {{0}};
        bool _slot_valid[kSlotNum] = {false, false, false};
        char _device_name[32] = "M5-Keyboard";
    };
} // namespace HID
