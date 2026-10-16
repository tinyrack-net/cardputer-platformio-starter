/**
 * @file usb_hid.cpp
 * @brief USB HID transport (keyboard + mouse).
 */
#include "usb_hid.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb_hid.h"

using namespace HID;

static const char* TAG = "hid_usb";

static volatile bool s_mounted = false;
static bool s_initialized = false;
static uint8_t s_buttons = 0;


//--------------------------------------------------------------------+
// TinyUSB device callbacks
//--------------------------------------------------------------------+

extern "C" void tud_mount_cb(void)
{
    s_mounted = true;
    ESP_LOGI(TAG, "USB mounted");
}

extern "C" void tud_umount_cb(void)
{
    s_mounted = false;
    ESP_LOGI(TAG, "USB unmounted");
}

extern "C" void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
}

extern "C" void tud_resume_cb(void) {}


static void tusb_device_task(void* arg)
{
    (void)arg;
    while (1)
        tud_task();
}


static void usb_phy_init_otg(void)
{
    usb_phy_handle_t phy_hdl = nullptr;
    usb_phy_config_t phy_conf = {};
    phy_conf.controller = USB_PHY_CTRL_OTG;
    phy_conf.otg_mode = USB_OTG_MODE_DEVICE;
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_new_phy(&phy_conf, &phy_hdl);
}


void UsbHid::begin()
{
    // The PHY can only be routed to OTG once and tud_init must not be called
    // twice, so keep this idempotent across channel switches.
    if (s_initialized)
        return;
    s_initialized = true;

    usb_phy_init_otg();
    tud_init(BOARD_TUD_RHPORT);
    xTaskCreate(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "USB HID started");
}


bool UsbHid::mounted() const
{
    return s_mounted;
}


void UsbHid::kbReport(uint8_t modifier, const uint8_t keys[6])
{
    tinyusb_hid_keyboard_report(modifier, keys);
}


void UsbHid::mouseMove(int8_t dx, int8_t dy, int8_t wheel)
{
    tinyusb_hid_mouse_report(s_buttons, dx, dy, wheel, 0);
}


void UsbHid::mouseButton(uint8_t mask)
{
    s_buttons = mask;
    tinyusb_hid_mouse_report(mask, 0, 0, 0, 0);
}
