/**
 * @file app_hid_os.h
 * @brief The single HID OS app: boot, key interpretation, mouse mode, HUD.
 */
#pragma once
#include <mooncake.h>

#include "../../hal/hal.h"
#include "../../hid/hid_manager.h"
#include "hid_hud.h"

namespace MOONCAKE
{
    namespace APPS
    {
        class AppHidOs : public APP_BASE
        {
        public:
            void onCreate() override;
            void onResume() override;
            void onRunning() override;
            void onDestroy() override;

        private:
            struct FnCombo_t
            {
                bool active = false;
                bool long_fired = false;
                int channel = -1;
                uint32_t start_ms = 0;
            };

            struct Data_t
            {
                HAL::Hal* hal = nullptr;
                HID::Manager manager;
                HID::HidHud hud;

                uint32_t battery_ms = 0;
                uint32_t hud_ms = 0;
                uint32_t mouse_ms = 0;
                uint32_t mouse_hold_ms = 0;
                uint8_t last_buttons = 0;
                bool force_render = true;
                bool fn_m_held = false;
            };

            Data_t _data;
            FnCombo_t _fn;
            HID::Status _last_status;

            void _handleFnCombos();
            void _handleKeyboard();
            void _handleMouse();
            void _updateBattery(bool force);
            void _updateHud(bool force);
        };

        class AppHidOs_Packer : public APP_PACKER_BASE
        {
            std::string getAppName() override { return "HID OS"; }
            void* newApp() override { return new AppHidOs; }
            void deleteApp(void* app) override { delete (AppHidOs*)app; }
        };
    } // namespace APPS
} // namespace MOONCAKE
