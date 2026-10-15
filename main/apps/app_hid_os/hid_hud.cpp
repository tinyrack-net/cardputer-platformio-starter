/**
 * @file hid_hud.cpp
 * @brief HID OS status screen.
 */
#include "hid_hud.h"

#include "../utils/theme/theme_define.h"

using namespace HID;


void HidHud::begin(HAL::Hal* hal)
{
    _hal = hal;
}


const char* HidHud::_channelLabel(const Status& status) const
{
    switch (status.channel)
    {
    case Channel::Usb:
        return "USB";
    case Channel::Bt1:
        return "BT1";
    case Channel::Bt2:
        return "BT2";
    case Channel::Bt3:
        return "BT3";
    default:
        return "?";
    }
}


void HidHud::render(const Status& status)
{
    if (_hal == nullptr)
        return;

    _renderSystemBar(status);
    _renderMain(status);
    _renderKeyboardBar(status);
}


void HidHud::_renderSystemBar(const Status& status)
{
    auto* bar = _hal->canvas_system_bar();
    const int w = bar->width();
    const int h = bar->height();

    bar->fillScreen(THEME_COLOR_BG);
    bar->fillSmoothRoundRect(3, 3, w - 6, h - 6, (h - 6) / 2, THEME_COLOR_SYSTEM_BAR);
    bar->setFont(FONT_BASIC);
    bar->setTextSize(1);
    bar->setTextColor(TFT_BLACK, THEME_COLOR_SYSTEM_BAR);

    // Left: active channel.
    bar->drawString(_channelLabel(status), 12, h / 2 - FONT_HEIGHT / 2);

    // Center: connection state.
    const char* state = "";
    if (status.channel == Channel::Usb)
        state = status.connected ? "Mounted" : "Not plugged";
    else if (status.connected)
        state = "Connected";
    else
        state = status.bonded ? "Searching" : "Advertising";
    bar->drawCenterString(state, w / 2 + 18, h / 2 - FONT_HEIGHT / 2);

    // Right: mouse mode badge.
    if (status.mouse_mode)
    {
        bar->setTextColor(TFT_RED, THEME_COLOR_SYSTEM_BAR);
        bar->drawRightString("MOUSE", w - 10, h / 2 - FONT_HEIGHT / 2);
    }

    _hal->canvas_system_bar_update();
}


void HidHud::_renderMain(const Status& status)
{
    auto* canvas = _hal->canvas();
    const int w = canvas->width();

    canvas->fillScreen(THEME_COLOR_BG);
    canvas->setFont(FONT_BASIC);
    canvas->setTextSize(1);

    // Title.
    canvas->setTextColor(THEME_COLOR_SYSTEM_BAR, THEME_COLOR_BG);
    canvas->drawString("HID OS", 6, 0);

    const int rows_y = 20;
    const int row_h = 14;

    for (int i = 0; i < 4; i++)
    {
        const int y = rows_y + i * row_h;
        const bool is_usb = (i == 0);
        const int slot = i - 1;
        const bool active = is_usb ? (status.channel == Channel::Usb) : (status.active_slot == slot);
        const bool bonded = is_usb ? false : status.slot_bonded[slot];

        uint32_t color = active ? THEME_COLOR_SYSTEM_BAR : TFT_ORANGE;
        canvas->setTextColor(color, THEME_COLOR_BG);

        if (active)
            canvas->drawString(">", 4, y);

        const char* name = is_usb ? "USB" : (slot == 0 ? "BT1" : (slot == 1 ? "BT2" : "BT3"));
        canvas->drawString(name, 16, y);

        // Bond indicator.
        if (!is_usb)
        {
            if (bonded)
                canvas->fillCircle(66, y + FONT_HEIGHT / 2, 4, color);
            else
                canvas->drawCircle(66, y + FONT_HEIGHT / 2, 4, TFT_DARKGREY);
        }

        // Per-row detail.
        if (active)
        {
            const char* detail = status.connected ? "online" : (is_usb ? "idle" : (bonded ? "search" : "pairing"));
            canvas->drawString(detail, 82, y);
        }
    }

    // Help.
    canvas->setTextColor(TFT_LIGHTGREY, THEME_COLOR_BG);
    canvas->drawString("fn+1/2/3 BT  fn+0 USB", 6, 80);
    canvas->drawString("fn+M mouse   hold=repair", 6, 92);

    (void)w;
    _hal->canvas_update();
}


void HidHud::_renderKeyboardBar(const Status& status)
{
    auto* bar = _hal->canvas_keyboard_bar();
    const int w = bar->width();
    const int h = bar->height();

    bar->fillScreen(THEME_COLOR_BG);
    bar->fillSmoothRoundRect(3, 4, w - 6, h - 8, 8, THEME_COLOR_KB_BAR);
    bar->setFont(FONT_BASIC);
    bar->setTextSize(1);

    // Battery icon.
    const int bx = 8;
    const int by = 14;
    const int bw = w - 16;
    const int bh = 10;
    bar->drawRect(bx, by, bw, bh, TFT_BLACK);
    bar->fillRect(bx + bw, by + bh / 3, 2, bh / 3, TFT_BLACK);
    const int fill_w = (bw - 4) * (int)status.battery / 100;
    if (fill_w > 0)
        bar->fillRect(bx + 2, by + 2, fill_w, bh - 4, TFT_DARKGREEN);

    // Battery percentage.
    char text[8];
    snprintf(text, sizeof(text), "%u", (unsigned)status.battery);
    bar->setTextColor(TFT_BLACK, THEME_COLOR_KB_BAR);
    bar->drawCenterString(text, w / 2, by + bh + 6);

    // Mode badge.
    bar->setTextColor(status.mouse_mode ? TFT_RED : TFT_DARKGREY, THEME_COLOR_KB_BAR);
    bar->drawCenterString(status.mouse_mode ? "M" : "K", w / 2, by + bh + 6 + FONT_HEIGHT + 4);

    _hal->canvas_keyboard_bar_update();
}
