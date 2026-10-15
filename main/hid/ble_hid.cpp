/**
 * @file ble_hid.cpp
 * @brief BLE HID transport: keyboard + mouse report map, 3 pairing slots,
 *        Just Works bonding, per-slot whitelist advertising.
 */
#include "ble_hid.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#include "esp_hid_common.h"
#include "esp_hid_gap.h"
#include "esp_hidd.h"
#include "esp_hidd_gatts.h"
#include "esp_log.h"
#include "esp_timer.h"

using namespace HID;

static const char* TAG = "hid_ble";

/* Report IDs: 1 = keyboard (8 bytes), 2 = mouse (4 bytes). */
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE 2

static const uint8_t s_report_map[] = {
    // ---------------------------------------------------------------- keyboard
    0x05, 0x01,  // Usage Pg (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection: (Application)
    0x85, 0x01,  // Report Id (1)
    0x05, 0x07,  //   Usage Pg (Key Codes)
    0x19, 0xE0,  //   Usage Min (224)
    0x29, 0xE7,  //   Usage Max (231)
    0x15, 0x00,  //   Log Min (0)
    0x25, 0x01,  //   Log Max (1)
    //   Modifier byte
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x08,  //   Report Count (8)
    0x81, 0x02,  //   Input: (Data, Variable, Absolute)
    //   Reserved byte
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x08,  //   Report Size (8)
    0x81, 0x01,  //   Input: (Constant)
    //   LED report
    0x95, 0x05,  //   Report Count (5)
    0x75, 0x01,  //   Report Size (1)
    0x05, 0x08,  //   Usage Pg (LEDs)
    0x19, 0x01,  //   Usage Min (1)
    0x29, 0x05,  //   Usage Max (5)
    0x91, 0x02,  //   Output: (Data, Variable, Absolute)
    //   LED report padding
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x03,  //   Report Size (3)
    0x91, 0x01,  //   Output: (Constant)
    //   Key arrays (6 bytes)
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Log Min (0)
    0x25, 0xF1,  //   Log Max (241)
    0x05, 0x07,  //   Usage Pg (Key Codes)
    0x19, 0x00,  //   Usage Min (0)
    0x29, 0xF1,  //   Usage Max (241)
    0x81, 0x00,  //   Input: (Data, Array)
    0xC0,        // End Collection

    // ------------------------------------------------------------------- mouse
    0x05, 0x01,  // Usage Pg (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection: (Application)
    0x85, 0x02,  // Report Id (2)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection: (Physical)
    0x05, 0x09,  //     Usage Pg (Button)
    0x19, 0x01,  //     Usage Min (1)
    0x29, 0x03,  //     Usage Max (3)
    0x15, 0x00,  //     Log Min (0)
    0x25, 0x01,  //     Log Max (1)
    0x95, 0x03,  //     Report Count (3)
    0x75, 0x01,  //     Report Size (1)
    0x81, 0x02,  //     Input: (Data, Variable, Absolute)
    0x95, 0x01,  //     Report Count (1)
    0x75, 0x05,  //     Report Size (5)
    0x81, 0x01,  //     Input: (Constant)
    0x05, 0x01,  //     Usage Pg (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x09, 0x38,  //     Usage (Wheel)
    0x15, 0x81,  //     Log Min (-127)
    0x25, 0x7F,  //     Log Max (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x03,  //     Report Count (3)
    0x81, 0x06,  //     Input: (Data, Variable, Relative)
    0xC0,        //   End Collection
    0xC0         // End Collection
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    {.data = s_report_map, .len = sizeof(s_report_map)},
};

static esp_hid_device_config_t s_hid_config = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "M5-Keyboard",
    .manufacturer_name = "Espressif",
    .serial_number = "1234567890",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

static uint8_t s_mouse_buttons = 0;


//--------------------------------------------------------------------+
// C trampolines
//--------------------------------------------------------------------+

static void gap_auth_cb(esp_bd_addr_t bda, esp_ble_addr_type_t addr_type, bool success)
{
    BleHid::instance().notifyAuthComplete(bda, (uint8_t)addr_type, success);
}

static void gap_whitelist_cb(void)
{
    BleHid::instance().notifyWhitelistComplete();
}

static void ble_hidd_event_callback(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    (void)handler_args;
    (void)base;

    esp_hidd_event_t event = (esp_hidd_event_t)id;
    BleHid& self = BleHid::instance();

    switch (event)
    {
    case ESP_HIDD_START_EVENT:
        self.notifyStackStart();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        self.notifyConnect();
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        self.notifyDisconnect();
        break;
    default:
        break;
    }
}


//--------------------------------------------------------------------+
// BleHid
//--------------------------------------------------------------------+

BleHid& BleHid::instance()
{
    static BleHid s_instance;
    return s_instance;
}


void BleHid::begin(uint8_t slot, ConfigStore* config)
{
    _config = config;
    _slot = slot < ConfigStore::kSlotNum ? slot : 0;

    if (_initialized)
    {
        switchSlot(_slot);
        return;
    }

    _initStack();
}


void BleHid::_initStack()
{
    if (esp_hid_gap_init(HIDD_BLE_MODE) != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_hid_gap_init failed");
        return;
    }

    // Drop slot entries whose Bluedroid bond no longer exists.
    _validateSlots();

    esp_hid_gap_set_auth_cb(gap_auth_cb);
    esp_hid_gap_set_whitelist_cb(gap_whitelist_cb);

    s_hid_config.device_name = _config->deviceName();

    if (esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, s_hid_config.device_name) != ESP_OK)
    {
        ESP_LOGE(TAG, "adv_init failed");
        return;
    }

    if (esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler) != ESP_OK)
    {
        ESP_LOGE(TAG, "gatts register failed");
        return;
    }

    if (esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &_hid_dev) != ESP_OK)
    {
        ESP_LOGE(TAG, "hidd dev init failed");
        return;
    }

    _initialized = true;
    ESP_LOGI(TAG, "BLE HID started on slot %u", _slot);
}


void BleHid::_validateSlots()
{
    int bond_num = 10;
    esp_ble_bond_dev_t bond_list[10] = {};
    if (esp_ble_get_bond_device_list(&bond_num, bond_list) != ESP_OK)
    {
        ESP_LOGW(TAG, "get_bond_device_list failed");
        return;
    }

    for (uint8_t slot = 0; slot < ConfigStore::kSlotNum; slot++)
    {
        uint8_t addr[ConfigStore::kAddrLen];
        if (!_config->getSlotAddr(slot, addr))
            continue;

        bool found = false;
        for (int i = 0; i < bond_num; i++)
        {
            if (memcmp(bond_list[i].bd_addr, &addr[1], 6) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ESP_LOGW(TAG, "slot %u bond missing, clearing", slot);
            _config->clearSlotAddr(slot);
        }
    }
}


void BleHid::notifyStackStart()
{
    ESP_LOGI(TAG, "HID stack started");
    _need_whitelist = true;
}


void BleHid::notifyConnect()
{
    _connected = true;
    _advertising = false;
    ESP_LOGI(TAG, "host connected (slot %u)", _slot);
}


void BleHid::notifyDisconnect()
{
    _connected = false;
    ESP_LOGI(TAG, "host disconnected (slot %u)", _slot);
    _need_whitelist = true;
}


void BleHid::notifyAuthComplete(const uint8_t* bda, uint8_t addr_type, bool success)
{
    if (!success)
        return;

    uint8_t addr[ConfigStore::kAddrLen];
    addr[0] = addr_type;
    memcpy(&addr[1], bda, 6);

    if (_config != nullptr)
        _config->setSlotAddr(_slot, addr);

    ESP_LOGI(TAG, "bonded peer saved to slot %u (addr_type %u)", _slot, addr_type);
}


void BleHid::notifyWhitelistComplete()
{
    if (_wl_pending > 0)
        _wl_pending--;
}


void BleHid::poll()
{
    const int64_t now = esp_timer_get_time();

    // Whitelist updates are issued and awaited on the main loop so no GAP API is
    // called from inside a Bluedroid callback.
    if (_wl_pending > 0)
    {
        if (now > _wl_deadline_us)
        {
            ESP_LOGW(TAG, "whitelist completion timeout, advertising anyway");
            _wl_pending = 0;
        }
        return;
    }

    if (_need_whitelist)
    {
        _need_whitelist = false;
        _applyWhitelist();
        return;
    }

    if (_need_adv)
    {
        _need_adv = false;
        _startAdv();
    }
}


void BleHid::_applyWhitelist()
{
    if (!_initialized)
        return;

    _wl_pending = 0;
    _need_adv = true;
    _wl_deadline_us = esp_timer_get_time() + 800000; // 800 ms

    // Clear previous entries (completion is counted as one pending op).
    if (esp_ble_gap_clear_whitelist() == ESP_OK)
        _wl_pending++;

    uint8_t addr[ConfigStore::kAddrLen];
    if (_config != nullptr && _config->getSlotAddr(_slot, addr))
    {
        esp_ble_wl_addr_type_t wl_type = BLE_WL_ADDR_TYPE_PUBLIC;
        if (addr[0] == BLE_ADDR_TYPE_RANDOM || addr[0] == BLE_ADDR_TYPE_RPA_RANDOM)
            wl_type = BLE_WL_ADDR_TYPE_RANDOM;

        if (esp_ble_gap_update_whitelist(true, &addr[1], wl_type) == ESP_OK)
            _wl_pending++;
    }

    ESP_LOGI(TAG, "whitelist update for slot %u (pending %u)", _slot, _wl_pending);
}


void BleHid::_startAdv()
{
    bool use_whitelist = _config != nullptr && _config->isSlotBonded(_slot);

    esp_err_t err = esp_hid_ble_gap_adv_start(use_whitelist);
    if (err == ESP_OK)
        _advertising = true;

    ESP_LOGI(TAG, "advertising on slot %u (whitelist=%d, err=%d)", _slot, (int)use_whitelist, err);
}


void BleHid::switchSlot(uint8_t slot)
{
    slot = slot < ConfigStore::kSlotNum ? slot : 0;
    if (slot == _slot)
    {
        if (!_connected && !_advertising)
            _need_whitelist = true;
        return;
    }

    ESP_LOGI(TAG, "switching BLE slot %u -> %u", _slot, slot);
    _slot = slot;

    if (_connected)
    {
        // Re-advertise for the new slot from the disconnect handler.
        esp_ble_gap_disconnect(_peer_addr);
    }
    else
    {
        _need_whitelist = true;
    }
}


void BleHid::unpair()
{
    unpairSlot(_slot);
}


void BleHid::unpairSlot(uint8_t slot)
{
    if (slot >= ConfigStore::kSlotNum)
        return;

    uint8_t addr[ConfigStore::kAddrLen];
    if (_config != nullptr && _config->getSlotAddr(slot, addr))
    {
        esp_ble_remove_bond_device(&addr[1]);
        _config->clearSlotAddr(slot);
        ESP_LOGI(TAG, "unpaired slot %u", slot);
    }

    if (slot != _slot)
        return;

    if (_connected)
        esp_ble_gap_disconnect(_peer_addr);
    else
        _need_whitelist = true;
}


void BleHid::setBattery(uint8_t level)
{
    if (_hid_dev != nullptr)
        esp_hidd_dev_battery_set(_hid_dev, level);
}


void BleHid::kbReport(uint8_t modifier, const uint8_t keys[6])
{
    if (!_connected || _hid_dev == nullptr)
        return;

    uint8_t buffer[8] = {0};
    buffer[0] = modifier;
    memcpy(&buffer[2], keys, 6);
    esp_hidd_dev_input_set(_hid_dev, 0, REPORT_ID_KEYBOARD, buffer, sizeof(buffer));
}


void BleHid::mouseMove(int8_t dx, int8_t dy, int8_t wheel)
{
    if (!_connected || _hid_dev == nullptr)
        return;

    uint8_t buffer[4] = {s_mouse_buttons, (uint8_t)dx, (uint8_t)dy, (uint8_t)wheel};
    esp_hidd_dev_input_set(_hid_dev, 0, REPORT_ID_MOUSE, buffer, sizeof(buffer));
}


void BleHid::mouseButton(uint8_t mask)
{
    s_mouse_buttons = mask;
    if (!_connected || _hid_dev == nullptr)
        return;

    uint8_t buffer[4] = {mask, 0, 0, 0};
    esp_hidd_dev_input_set(_hid_dev, 0, REPORT_ID_MOUSE, buffer, sizeof(buffer));
}
