/**
 * @file cardputer.cpp
 * @author Forairaaaaa
 * @brief M5Cardputer HID OS entry point. HAL init -> single HID OS app.
 * @version 0.1
 * @date 2023-10-12
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdio.h>
#include <mooncake.h>
#include "hal/hal_cardputer.h"
#include "apps/apps.h"


using namespace HAL;
using namespace MOONCAKE;


HalCardputer hal;
Mooncake mooncake;


void _data_base_setup_callback(SIMPLEKV::SimpleKV& db)
{
    // DI
    db.Add<HAL::Hal*>("HAL", &hal);
    spdlog::info("hal injection: {}", db.Get("HAL")->value<HAL::Hal*>()->type());
}


extern "C" void app_main(void)
{
    // Init hal
    hal.init();

    // Init framework
    mooncake.setDatabaseSetupCallback(_data_base_setup_callback);
    mooncake.init();

    // The HID OS is the only app installed; no launcher, no menu.
    auto hid_os = new APPS::AppHidOs_Packer;
    mooncake.installApp(hid_os);

    // Run it forever
    mooncake.startApp(mooncake.createApp(hid_os));
    while (1)
        mooncake.update();
}
