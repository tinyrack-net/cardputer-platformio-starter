/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "tinyusb_hid.h"
#include "usb_descriptors.h"
#include "device/usbd.h"

static const char *TAG = "tinyusb_hid.h";

void tinyusb_hid_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t vertical, int8_t horizontal)
{
    ESP_LOGD(TAG, "buttons=%u, x=%d, y=%d, vertical=%d, horizontal=%d", buttons, x, y, vertical, horizontal);

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        // skip if hid is not ready yet
        if (!tud_hid_ready()) {
            ESP_LOGW(TAG, "tinyusb not ready %s %d", __func__, __LINE__);
            return;
        }

        tud_hid_mouse_report(REPORT_ID_MOUSE, buttons, x, y, vertical, horizontal);
    }
}

void tinyusb_hid_mouse_move_report(int8_t x, int8_t y, int8_t vertical, int8_t horizontal)
{
    tinyusb_hid_mouse_report(0x00, x, y, vertical, horizontal);
}

void tinyusb_hid_mouse_button_report(uint8_t buttons_map)
{
    tinyusb_hid_mouse_report(buttons_map, 0, 0, 0, 0);
}

void tinyusb_hid_keyboard_report(uint8_t modifier, const uint8_t keycode[6])
{
    ESP_LOGD(TAG, "modifier = %u, keycode = %u %u %u %u %u %u", modifier, keycode[0], keycode[1], keycode[2], keycode[3], keycode[4], keycode[5]);

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        // skip if hid is not ready yet
        if (!tud_hid_ready()) {
            ESP_LOGW(TAG, "tinyusb not ready %s %d", __func__, __LINE__);
            return;
        }

        // Send the full keyboard state. The report is only emitted on state
        // change by the caller, so no synthetic "all released" report is sent
        // here (that would break holding keys and modifiers).
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, (uint8_t *)keycode);
    }
}

/************************************************** TinyUSB callbacks ***********************************************/
// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint16_t len)
{
    (void) itf;
    (void) report;
    (void) len;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    // TODO set LED based on CAPLOCK, NUMLOCK etc...
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}