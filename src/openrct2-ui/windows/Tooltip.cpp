/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Context.h>
#include <openrct2/Input.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Localisation.h>

// clang-format off
enum {
    WIDX_BACKGROUND
};

static Widget window_tooltip_widgets[] = {
    MakeWidget({0, 0}, {200, 32}, WindowWidgetType::ImgBtn, WindowColour::Primary),
    WIDGETS_END,
};

// clang-format on

class TooltipWindow final : public Window
{
private:
    utf8 _tooltipText[CommonTextBufferSize]{};
    int16_t _tooltipNumLines = 1;

public:
    TooltipWindow(const OpenRCT2String& message, ScreenCoordsXY screenCoords)
    {
        int32_t textWidth = FormatTextForTooltip(message);
        width = textWidth + 3;
        height = ((_tooltipNumLines + 1) * FontGetLineHeight(FontStyle::Small)) + 4;

        window_tooltip_widgets[WIDX_BACKGROUND].right = width;
        window_tooltip_widgets[WIDX_BACKGROUND].bottom = height;

        int32_t screenWidth = ContextGetWidth();
        int32_t screenHeight = ContextGetHeight();
        screenCoords.x = std::clamp(screenCoords.x - (width / 2), 0, screenWidth - width);

        // TODO The cursor size will be relative to the window DPI.
        //      The amount to offset the y should be adjusted.

        int32_t max_y = screenHeight - height;
        screenCoords.y += 26; // Normally, we'd display the tooltip 26 lower
        if (screenCoords.y > max_y)
            // If y is too large, the tooltip could be forced below the cursor if we'd just clamped y,
            // so we'll subtract a bit more
            screenCoords.y -= height + 40;
        screenCoords.y = std::clamp(screenCoords.y, 22, max_y);

        windowPos = screenCoords;
    }

    void OnOpen() override
    {
        widgets = window_tooltip_widgets;
        ResetTooltipNotShown();
    }

    void OnUpdate() override
    {
        ResetTooltipNotShown();
    }

    void OnDraw(DrawPixelInfo& dpi) override
    {
        int32_t left = windowPos.x;
        int32_t top = windowPos.y;
        int32_t right = windowPos.x + width - 1;
        int32_t bottom = windowPos.y + height - 1;

        // Background
        GfxFilterRect(&dpi, { { left + 1, top + 1 }, { right - 1, bottom - 1 } }, FilterPaletteID::Palette45);
        GfxFilterRect(&dpi, { { left + 1, top + 1 }, { right - 1, bottom - 1 } }, FilterPaletteID::PaletteGlassLightOrange);

        // Sides
        GfxFilterRect(&dpi, { { left + 0, top + 2 }, { left + 0, bottom - 2 } }, FilterPaletteID::PaletteDarken3);
        GfxFilterRect(&dpi, { { right + 0, top + 2 }, { right + 0, bottom - 2 } }, FilterPaletteID::PaletteDarken3);
        GfxFilterRect(&dpi, { { left + 2, bottom + 0 }, { right - 2, bottom + 0 } }, FilterPaletteID::PaletteDarken3);
        GfxFilterRect(&dpi, { { left + 2, top + 0 }, { right - 2, top + 0 } }, FilterPaletteID::PaletteDarken3);

        // Corners
        GfxFilterPixel(&dpi, { left + 1, top + 1 }, FilterPaletteID::PaletteDarken3);
        GfxFilterPixel(&dpi, { right - 1, top + 1 }, FilterPaletteID::PaletteDarken3);
        GfxFilterPixel(&dpi, { left + 1, bottom - 1 }, FilterPaletteID::PaletteDarken3);
        GfxFilterPixel(&dpi, { right - 1, bottom - 1 }, FilterPaletteID::PaletteDarken3);

        // Text
        left = windowPos.x + ((width + 1) / 2) - 1;
        top = windowPos.y + 1;
        DrawStringCentredRaw(&dpi, { left, top }, _tooltipNumLines, _tooltipText, FontStyle::Small);
    }

private:
    // Returns the width of the new tooltip text
    int32_t FormatTextForTooltip(const OpenRCT2String& message)
    {
        utf8 tempBuffer[CommonTextBufferSize];
        OpenRCT2::FormatStringLegacy(tempBuffer, sizeof(tempBuffer), message.str, message.args.Data());

        OpenRCT2String formattedMessage{ STR_STRING_TOOLTIP, Formatter() };
        formattedMessage.args.Add<const char*>(tempBuffer);
        OpenRCT2::FormatStringLegacy(_tooltipText, sizeof(_tooltipText), formattedMessage.str, formattedMessage.args.Data());

        auto textWidth = GfxGetStringWidthNewLined(_tooltipText, FontStyle::Small);
        textWidth = std::min(textWidth, 196);

        int32_t numLines;
        textWidth = GfxWrapString(_tooltipText, textWidth + 1, FontStyle::Small, &numLines);
        _tooltipNumLines = numLines;
        return textWidth;
    }
};

void WindowTooltipReset(const ScreenCoordsXY& screenCoords)
{
    gTooltipCursor = screenCoords;
    gTooltipTimeout = 0;
    gTooltipWidget.window_classification = WindowClass::Null;
    InputSetState(InputState::Normal);
    InputSetFlag(INPUT_FLAG_4, false);
}

void WindowTooltipShow(const OpenRCT2String& message, ScreenCoordsXY screenCoords)
{
    auto* w = WindowFindByClass(WindowClass::Error);
    if (w != nullptr)
        return;

    auto tooltipWindow = std::make_unique<TooltipWindow>(message, screenCoords);
    auto windowPos = tooltipWindow->windowPos;
    auto width = tooltipWindow->width;
    auto height = tooltipWindow->height;
    WindowCreate(std::move(tooltipWindow), WindowClass::Tooltip, windowPos, width, height, WF_TRANSPARENT | WF_STICK_TO_FRONT);
}

void WindowTooltipOpen(WindowBase* widgetWindow, WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords)
{
    if (widgetWindow == nullptr || widgetIndex == -1)
        return;

    auto widget = &widgetWindow->widgets[widgetIndex];
    WindowEventInvalidateCall(widgetWindow);

    OpenRCT2String result;
    if (widget->flags & WIDGET_FLAGS::TOOLTIP_IS_STRING)
    {
        auto tooltipString = widget->sztooltip;
        if (*tooltipString == 0)
            return;

        result.str = STR_STRING_TOOLTIP;
        result.args = Formatter();
        result.args.Add<const char*>(tooltipString);

        gTooltipWidget.window_classification = widgetWindow->classification;
        gTooltipWidget.window_number = widgetWindow->number;
        gTooltipWidget.widget_index = widgetIndex;
    }
    else
    {
        auto stringId = widget->tooltip;
        if (stringId == STR_NONE)
            return;

        gTooltipWidget.window_classification = widgetWindow->classification;
        gTooltipWidget.window_number = widgetWindow->number;
        gTooltipWidget.widget_index = widgetIndex;
        result = WindowEventTooltipCall(widgetWindow, widgetIndex, stringId);
        if (result.str == STR_NONE)
            return;
    }

    WindowTooltipShow(result, screenCoords);
}

void WindowTooltipClose()
{
    WindowCloseByClass(WindowClass::Tooltip);
    gTooltipTimeout = 0;
    gTooltipWidget.window_classification = WindowClass::Null;
}
