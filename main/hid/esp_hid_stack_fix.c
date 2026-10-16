/*
 * Work around a BLE HID stack overflow in the pinned ESP-IDF 4.4.6.
 *
 * esp_hid creates its "ble_hidd_events" event-loop task with a hard-coded
 * 2048-byte stack (components/esp_hid/src/ble_hidd.c). That is far too small
 * and the task overflows as soon as a host connects or disconnects, which
 * panics and reboots the chip in a loop.
 *
 * Upstream fixed this by making the size configurable, with a 4096-byte
 * default (Kconfig ESPHID_TASK_SIZE_BLE); IDF 4.4.6 has neither the option nor
 * the larger literal, and the version is pinned so it cannot simply be
 * upgraded (see espressif/esp-idf#9720). Instead of vendoring the whole
 * component or patching the framework in place, the call is intercepted at
 * link time (see the --wrap flag in CMakeLists.txt) and only that one task is
 * given a bigger stack.
 */

#include <string.h>

#include "esp_event.h"

#define BLE_HIDD_TASK_NAME "ble_hidd_events"
#define BLE_HIDD_TASK_STACK 4096

esp_err_t __real_esp_event_loop_create(const esp_event_loop_args_t *args, esp_event_loop_handle_t *handle);

esp_err_t __wrap_esp_event_loop_create(const esp_event_loop_args_t *args, esp_event_loop_handle_t *handle)
{
    esp_event_loop_args_t patched;

    if (args != NULL && args->task_name != NULL &&
        strcmp(args->task_name, BLE_HIDD_TASK_NAME) == 0 &&
        args->task_stack_size < BLE_HIDD_TASK_STACK)
    {
        patched = *args;
        patched.task_stack_size = BLE_HIDD_TASK_STACK;
        args = &patched;
    }

    return __real_esp_event_loop_create(args, handle);
}
