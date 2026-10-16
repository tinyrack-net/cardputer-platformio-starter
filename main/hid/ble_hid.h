/**
 * @file ble_hid.h
 * @brief BLE HID transport: keyboard + mouse report map, 3 pairing slots,
 *        Just Works bonding, per-slot whitelist advertising.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "config_store.h"

struct esp_hidd_dev_s;

namespace HID
{
    class BleHid
    {
    public:
        static BleHid& instance();

        /* Init the BLE stack and advertise for `slot` (0..2). */
        void begin(uint8_t slot, ConfigStore* config);

        /* Runtime slot change: disconnect the current host, allow `slot` only. */
        void switchSlot(uint8_t slot);

        /* Remove the current slot's bond + stored address, advertise generally. */
        void unpair();

        /* Remove an arbitrary slot's bond + stored address. */
        void unpairSlot(uint8_t slot);

        void setBattery(uint8_t level);

        void kbReport(uint8_t modifier, const uint8_t keys[6]);
        void mouseMove(int8_t dx, int8_t dy, int8_t wheel);
        void mouseButton(uint8_t mask);

        bool connected() const { return _connected; }
        bool advertising() const { return _advertising; }
        bool bonded() const { return _config != nullptr && _config->isSlotBonded(_slot); }
        bool slotBonded(uint8_t slot) const { return _config != nullptr && _config->isSlotBonded(slot); }
        int8_t slot() const { return static_cast<int8_t>(_slot); }

        /* Internal: driven by the GAP / HIDD callbacks. */
        void notifyStackStart();
        void notifyConnect();
        void notifyDisconnect();
        void notifyAuthComplete(const uint8_t* bda, uint8_t addr_type, bool success);
        void notifyWhitelistComplete();

        /* Housekeeping called from the main loop (whitelist timeout fallback). */
        void poll();

    private:
        BleHid() = default;

        void _initStack();
        void _validateSlots();
        void _applyWhitelist();
        void _startAdv();

        ConfigStore* _config = nullptr;
        uint8_t _slot = 0;
        bool _initialized = false;
        bool _connected = false;
        bool _advertising = false;

        esp_hidd_dev_s* _hid_dev = nullptr;

        uint8_t _peer_addr[6] = {0};

        uint8_t _wl_pending = 0;
        bool _need_whitelist = false;
        bool _need_adv = false;
        int64_t _wl_deadline_us = 0;
    };
} // namespace HID
