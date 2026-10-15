/**
 * @file config_store.cpp
 * @brief NVS persistence for the HID OS.
 */
#include "config_store.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

using namespace HID;

static const char* TAG = "hid_config";

static const char* kNamespace = "hid_os";
static const char* kKeyChannel = "channel";
static const char* kKeySpeed = "mouse_speed";
static const char* kKeyAccel = "mouse_accel";
static const char* kKeyName = "device_name";
static const char* kSlotKeys[ConfigStore::kSlotNum] = {"slot0_addr", "slot1_addr", "slot2_addr"};


bool ConfigStore::begin()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "nvs needs erase (%s)", esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open(%s) failed", kNamespace);
        return false;
    }

    uint8_t channel = 0;
    if (nvs_get_u8(handle, kKeyChannel, &channel) != ESP_OK || channel > 3)
        channel = 0;
    _config.channel = channel;

    uint8_t speed = 1;
    if (nvs_get_u8(handle, kKeySpeed, &speed) != ESP_OK)
        speed = 1;
    _config.mouse_speed = speed;

    int8_t accel = 1;
    if (nvs_get_i8(handle, kKeyAccel, &accel) != ESP_OK)
        accel = 1;
    _config.mouse_accel = accel;

    size_t name_len = sizeof(_device_name);
    if (nvs_get_str(handle, kKeyName, _device_name, &name_len) != ESP_OK)
        strncpy(_device_name, "M5-Keyboard", sizeof(_device_name) - 1);

    for (size_t i = 0; i < kSlotNum; i++)
    {
        size_t len = kAddrLen;
        esp_err_t slot_err = nvs_get_blob(handle, kSlotKeys[i], _slot_addr[i], &len);
        _slot_valid[i] = (slot_err == ESP_OK && len == kAddrLen);
    }

    nvs_close(handle);

    ESP_LOGI(TAG, "loaded channel=%u mouse=(%u,%d) slots=(%d,%d,%d)",
             _config.channel,
             _config.mouse_speed,
             _config.mouse_accel,
             (int)_slot_valid[0],
             (int)_slot_valid[1],
             (int)_slot_valid[2]);
    return true;
}


void ConfigStore::saveChannel(uint8_t channel)
{
    if (channel > 3)
        channel = 0;
    _config.channel = channel;

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK)
        return;
    if (nvs_set_u8(handle, kKeyChannel, channel) == ESP_OK)
        nvs_commit(handle);
    nvs_close(handle);
}


void ConfigStore::saveMouse(uint8_t speed, int8_t accel)
{
    _config.mouse_speed = speed;
    _config.mouse_accel = accel;

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK)
        return;
    nvs_set_u8(handle, kKeySpeed, speed);
    nvs_set_i8(handle, kKeyAccel, accel);
    nvs_commit(handle);
    nvs_close(handle);
}


void ConfigStore::saveDeviceName(const char* name)
{
    if (name == nullptr || name[0] == '\0')
        return;
    strncpy(_device_name, name, sizeof(_device_name) - 1);
    _device_name[sizeof(_device_name) - 1] = '\0';

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK)
        return;
    if (nvs_set_str(handle, kKeyName, _device_name) == ESP_OK)
        nvs_commit(handle);
    nvs_close(handle);
}


bool ConfigStore::getSlotAddr(uint8_t slot, uint8_t out[kAddrLen]) const
{
    if (slot >= kSlotNum || !_slot_valid[slot])
        return false;
    memcpy(out, _slot_addr[slot], kAddrLen);
    return true;
}


bool ConfigStore::setSlotAddr(uint8_t slot, const uint8_t addr[kAddrLen])
{
    if (slot >= kSlotNum)
        return false;

    memcpy(_slot_addr[slot], addr, kAddrLen);
    _slot_valid[slot] = true;

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK)
        return false;
    esp_err_t err = nvs_set_blob(handle, kSlotKeys[slot], addr, kAddrLen);
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "save slot %u failed: %s", slot, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "saved slot %u", slot);
    return true;
}


void ConfigStore::clearSlotAddr(uint8_t slot)
{
    if (slot >= kSlotNum)
        return;

    _slot_valid[slot] = false;
    memset(_slot_addr[slot], 0, kAddrLen);

    nvs_handle_t handle;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK)
        return;
    nvs_erase_key(handle, kSlotKeys[slot]);
    nvs_commit(handle);
    nvs_close(handle);
}


bool ConfigStore::isSlotBonded(uint8_t slot) const
{
    return slot < kSlotNum && _slot_valid[slot];
}
