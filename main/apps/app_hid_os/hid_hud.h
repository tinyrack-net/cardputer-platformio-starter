/**
 * @file hid_hud.h
 * @brief HID OS status screen (mode / channel / connection / slots / battery).
 */
#pragma once
#include "../../hal/hal.h"
#include "../../hid/hid_manager.h"

namespace HID
{
    class HidHud
    {
    public:
        void begin(HAL::Hal* hal);
        void render(const Status& status);

    private:
        void _renderSystemBar(const Status& status);
        void _renderMain(const Status& status);
        void _renderKeyboardBar(const Status& status);
        const char* _channelLabel(const Status& status) const;

        HAL::Hal* _hal = nullptr;
    };
} // namespace HID
