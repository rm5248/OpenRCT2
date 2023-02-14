/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Widget.h"

#include "Window.h"

#include <algorithm>
#include <cmath>
#include <openrct2/Context.h>
#include <openrct2/Input.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Localisation.h>
#include <openrct2/sprites.h>
#include <openrct2/util/Util.h>

static void WidgetFrameDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetResizeDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetButtonDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetTabDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetFlatButtonDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetTextButton(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetTextCentred(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetText(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetTextInset(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetTextBoxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetGroupboxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetCaptionDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetCheckboxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetCloseboxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetScrollDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);
static void WidgetHScrollbarDraw(
    DrawPixelInfo* dpi, const ScrollBar& scroll, int32_t l, int32_t t, int32_t r, int32_t b, int32_t colour);
static void WidgetVScrollbarDraw(
    DrawPixelInfo* dpi, const ScrollBar& scroll, int32_t l, int32_t t, int32_t r, int32_t b, int32_t colour);
static void WidgetDrawImage(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex);

/**
 *
 *  rct2: 0x006EB2A8
 */
void WidgetDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    const auto* widget = GetWidgetByIndex(w, widgetIndex);
    if (widget == nullptr)
    {
        LOG_ERROR("Tried drawing an out-of-bounds widget index!");
        return;
    }

    switch (widget->type)
    {
        case WindowWidgetType::Frame:
            WidgetFrameDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Resize:
            WidgetResizeDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::ImgBtn:
            WidgetButtonDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::ColourBtn:
        case WindowWidgetType::TrnBtn:
        case WindowWidgetType::Tab:
            WidgetTabDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::FlatBtn:
            WidgetFlatButtonDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Button:
        case WindowWidgetType::TableHeader:
            WidgetTextButton(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::LabelCentred:
            WidgetTextCentred(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Label:
            WidgetText(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Spinner:
        case WindowWidgetType::DropdownMenu:
        case WindowWidgetType::Viewport:
            WidgetTextInset(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Groupbox:
            WidgetGroupboxDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Caption:
            WidgetCaptionDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::CloseBox:
            WidgetCloseboxDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Scroll:
            WidgetScrollDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::Checkbox:
            WidgetCheckboxDraw(dpi, w, widgetIndex);
            break;
        case WindowWidgetType::TextBox:
            WidgetTextBoxDraw(dpi, w, widgetIndex);
            break;
        default:
            break;
    }
}

/**
 *
 *  rct2: 0x006EB6CE
 */
static void WidgetFrameDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    auto leftTop = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };
    int32_t r = w.windowPos.x + widget.right;
    int32_t b = w.windowPos.y + widget.bottom;

    //
    uint8_t press = ((w.flags & WF_10) ? INSET_RECT_FLAG_FILL_MID_LIGHT : 0);

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    // Draw the frame
    GfxFillRectInset(dpi, { leftTop, { r, b } }, colour, press);

    // Check if the window can be resized
    if (!(w.flags & WF_RESIZABLE))
        return;
    if (w.min_width == w.max_width && w.min_height == w.max_height)
        return;

    // Draw the resize sprite at the bottom right corner
    leftTop = w.windowPos + ScreenCoordsXY{ widget.right - 18, widget.bottom - 18 };
    GfxDrawSprite(dpi, ImageId(SPR_RESIZE, colour & 0x7F), leftTop);
}

/**
 *
 *  rct2: 0x006EB765
 */
static void WidgetResizeDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    auto leftTop = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };
    int32_t r = w.windowPos.x + widget.right;
    int32_t b = w.windowPos.y + widget.bottom;

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    // Draw the panel
    GfxFillRectInset(dpi, { leftTop, { r, b } }, colour, 0);

    // Check if the window can be resized
    if (!(w.flags & WF_RESIZABLE))
        return;
    if (w.min_width == w.max_width && w.min_height == w.max_height)
        return;

    // Draw the resize sprite at the bottom right corner
    leftTop = w.windowPos + ScreenCoordsXY{ widget.right - 18, widget.bottom - 18 };
    GfxDrawSprite(dpi, ImageId(SPR_RESIZE, colour & 0x7F), leftTop);
}

/**
 *
 *  rct2: 0x006EB8E5
 */
static void WidgetButtonDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    ScreenRect rect{ w.windowPos + ScreenCoordsXY{ widget.left, widget.top },
                     w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom } };

    // Check if the button is pressed down
    uint8_t press = WidgetIsPressed(w, widgetIndex) || WidgetIsActiveTool(w, widgetIndex) ? INSET_RECT_FLAG_BORDER_INSET : 0;

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    if (static_cast<int32_t>(widget.image.ToUInt32()) == -2)
    {
        // Draw border with no fill
        GfxFillRectInset(dpi, rect, colour, press | INSET_RECT_FLAG_FILL_NONE);
        return;
    }

    // Draw the border with fill
    GfxFillRectInset(dpi, rect, colour, press);

    WidgetDrawImage(dpi, w, widgetIndex);
}

/**
 *
 *  rct2: 0x006EB806
 */
static void WidgetTabDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    auto& widget = w.widgets[widgetIndex];

    if (widget.type != WindowWidgetType::Tab && widget.image.GetIndex() == ImageIndexUndefined)
        return;

    if (widget.type == WindowWidgetType::Tab)
    {
        if (WidgetIsDisabled(w, widgetIndex))
            return;

        if (widget.image.GetIndex() == ImageIndexUndefined)
        {
            // Set standard tab sprite to use.
            widget.image = ImageId(SPR_TAB, FilterPaletteID::PaletteNull);
        }
    }

    // Draw widgets that aren't explicitly disabled.
    if (!WidgetIsDisabled(w, widgetIndex))
    {
        WidgetDrawImage(dpi, w, widgetIndex);
        return;
    }

    if (widget.type != WindowWidgetType::TrnBtn)
    {
        WidgetDrawImage(dpi, w, widgetIndex);
        return;
    }

    // Resolve the absolute ltrb
    auto leftTop = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };

    // Get the colour and disabled image
    auto colour = w.colours[widget.colour] & 0x7F;
    const auto newIndex = widget.image.GetIndex() + 2;
    auto image = widget.image.WithIndex(newIndex).WithPrimary(colour);

    // Draw disabled image
    GfxDrawSprite(dpi, image, leftTop);
}

/**
 *
 *  rct2: 0x006EB861
 */
static void WidgetFlatButtonDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    if (!WidgetIsDisabled(w, widgetIndex) && WidgetIsHighlighted(w, widgetIndex))
    {
        WidgetButtonDraw(dpi, w, widgetIndex);
        return;
    }

    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    ScreenRect rect{ w.windowPos + ScreenCoordsXY{ widget.left, widget.top },
                     w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom } };

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    // Check if the button is pressed down
    if (WidgetIsPressed(w, widgetIndex) || WidgetIsActiveTool(w, widgetIndex))
    {
        if (static_cast<int32_t>(widget.image.ToUInt32()) == -2)
        {
            // Draw border with no fill
            GfxFillRectInset(dpi, rect, colour, INSET_RECT_FLAG_BORDER_INSET | INSET_RECT_FLAG_FILL_NONE);
            return;
        }

        // Draw the border with fill
        GfxFillRectInset(dpi, rect, colour, INSET_RECT_FLAG_BORDER_INSET);
    }

    // Draw image
    WidgetDrawImage(dpi, w, widgetIndex);
}

/**
 *
 *  rct2: 0x006EBBEB
 */
static void WidgetTextButton(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    ScreenRect rect{ w.windowPos + ScreenCoordsXY{ widget.left, widget.top },
                     w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom } };

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    // Border
    uint8_t press = WidgetIsPressed(w, widgetIndex) || WidgetIsActiveTool(w, widgetIndex) ? INSET_RECT_FLAG_BORDER_INSET : 0;
    GfxFillRectInset(dpi, rect, colour, press);

    // Button caption
    if (widget.type != WindowWidgetType::TableHeader)
    {
        WidgetTextCentred(dpi, w, widgetIndex);
    }
    else
    {
        WidgetText(dpi, w, widgetIndex);
    }
}

/**
 *
 *  rct2: 0x006EBC41
 */
static void WidgetTextCentred(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    if (widget.text == STR_NONE)
        return;

    // Get the colour
    colour_t colour = w.colours[widget.colour];
    colour &= ~(COLOUR_FLAG_TRANSLUCENT);
    if (WidgetIsDisabled(w, widgetIndex))
        colour |= COLOUR_FLAG_INSET;

    // Resolve the absolute ltrb
    auto topLeft = w.windowPos + ScreenCoordsXY{ widget.left, 0 };
    int32_t r = w.windowPos.x + widget.right;

    if (widget.type == WindowWidgetType::Button || widget.type == WindowWidgetType::TableHeader)
        topLeft.y += widget.textTop();
    else
        topLeft.y += widget.top;

    auto stringId = widget.text;
    auto ft = Formatter::Common();
    if (widget.flags & WIDGET_FLAGS::TEXT_IS_STRING)
    {
        stringId = STR_STRING;
        ft.Add<utf8*>(widget.string);
    }

    ScreenCoordsXY coords = { (topLeft.x + r + 1) / 2 - 1, topLeft.y };
    if (widget.type == WindowWidgetType::LabelCentred)
    {
        DrawTextWrapped(dpi, coords, widget.width() - 2, stringId, ft, { colour, TextAlignment::CENTRE });
    }
    else
    {
        DrawTextEllipsised(dpi, coords, widget.width() - 2, stringId, ft, { colour, TextAlignment::CENTRE });
    }
}

/**
 *
 *  rct2: 0x006EBD52
 */
static void WidgetText(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    if (widget.text == STR_NONE || widget.text == STR_VIEWPORT)
        return;

    // Get the colour
    uint8_t colour = w.colours[widget.colour];
    if (WidgetIsDisabled(w, widgetIndex))
        colour |= COLOUR_FLAG_INSET;

    // Resolve the absolute ltrb
    int32_t l = w.windowPos.x + widget.left;
    int32_t r = w.windowPos.x + widget.right;
    int32_t t;

    if (widget.type == WindowWidgetType::Button || widget.type == WindowWidgetType::DropdownMenu
        || widget.type == WindowWidgetType::Spinner || widget.type == WindowWidgetType::TableHeader)
    {
        t = w.windowPos.y + widget.textTop();
    }
    else
        t = w.windowPos.y + widget.top;

    auto stringId = widget.text;
    auto ft = Formatter::Common();
    if (widget.flags & WIDGET_FLAGS::TEXT_IS_STRING)
    {
        stringId = STR_STRING;
        ft.Add<utf8*>(widget.string);
    }

    ScreenCoordsXY coords = { l + 1, t };
    if (widget.type == WindowWidgetType::LabelCentred)
    {
        DrawTextWrapped(dpi, coords, r - l, stringId, ft, { colour, TextAlignment::CENTRE });
    }
    else
    {
        DrawTextEllipsised(dpi, coords, r - l, stringId, ft, colour);
    }
}

/**
 *
 *  rct2: 0x006EBD1F
 */
static void WidgetTextInset(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    ScreenRect rect{ w.windowPos + ScreenCoordsXY{ widget.left, widget.top },
                     w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom } };

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    GfxFillRectInset(dpi, rect, colour, INSET_RECT_F_60);
    WidgetText(dpi, w, widgetIndex);
}

static std::pair<StringId, void*> WidgetGetStringidAndArgs(const Widget& widget)
{
    auto stringId = widget.text;
    void* formatArgs = gCommonFormatArgs;
    if (widget.flags & WIDGET_FLAGS::TEXT_IS_STRING)
    {
        if (widget.string == nullptr || widget.string[0] == '\0')
        {
            stringId = STR_NONE;
            formatArgs = nullptr;
        }
        else
        {
            stringId = STR_STRING;
            formatArgs = const_cast<void*>(reinterpret_cast<const void*>(&widget.string));
        }
    }
    return std::make_pair(stringId, formatArgs);
}

/**
 *
 *  rct2: 0x006EB535
 */
static void WidgetGroupboxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    auto l = w.windowPos.x + widget.left + 5;
    auto t = w.windowPos.y + widget.top;
    auto textRight = l;

    // Text
    auto [stringId, formatArgs] = WidgetGetStringidAndArgs(widget);
    if (stringId != STR_NONE)
    {
        uint8_t colour = w.colours[widget.colour] & 0x7F;
        if (WidgetIsDisabled(w, widgetIndex))
            colour |= COLOUR_FLAG_INSET;

        utf8 buffer[512] = { 0 };
        OpenRCT2::FormatStringLegacy(buffer, sizeof(buffer), stringId, formatArgs);
        auto ft = Formatter();
        ft.Add<utf8*>(buffer);
        DrawTextBasic(dpi, { l, t }, STR_STRING, ft, { colour });
        textRight = l + GfxGetStringWidth(buffer, FontStyle::Medium) + 1;
    }

    // Border
    // Resolve the absolute ltrb
    l = w.windowPos.x + widget.left;
    t = w.windowPos.y + widget.top + 4;
    const auto r = w.windowPos.x + widget.right;
    const auto b = w.windowPos.y + widget.bottom;

    // Get the colour
    uint8_t colour = w.colours[widget.colour] & 0x7F;

    // Border left of text
    GfxFillRect(dpi, { { l, t }, { l + 4, t } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + 1, t + 1 }, { l + 4, t + 1 } }, ColourMapA[colour].lighter);

    // Border right of text
    GfxFillRect(dpi, { { textRight, t }, { r - 1, t } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { textRight, t + 1 }, { r - 2, t + 1 } }, ColourMapA[colour].lighter);

    // Border right
    GfxFillRect(dpi, { { r - 1, t + 1 }, { r - 1, b - 1 } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { r, t }, { r, b } }, ColourMapA[colour].lighter);

    // Border bottom
    GfxFillRect(dpi, { { l, b - 1 }, { r - 2, b - 1 } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l, b }, { r - 1, b } }, ColourMapA[colour].lighter);

    // Border left
    GfxFillRect(dpi, { { l, t + 1 }, { l, b - 2 } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + 1, t + 2 }, { l + 1, b - 2 } }, ColourMapA[colour].lighter);
}

/**
 *
 *  rct2: 0x006EB2F9
 */
static void WidgetCaptionDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto* widget = &w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    auto topLeft = w.windowPos + ScreenCoordsXY{ widget->left, widget->top };
    auto bottomRight = w.windowPos + ScreenCoordsXY{ widget->right, widget->bottom };

    // Get the colour
    uint8_t colour = w.colours[widget->colour];

    uint8_t press = INSET_RECT_F_60;
    if (w.flags & WF_10)
        press |= INSET_RECT_FLAG_FILL_MID_LIGHT;

    GfxFillRectInset(dpi, { topLeft, bottomRight }, colour, press);

    // Black caption bars look slightly green, this fixes that
    if (colour == 0)
        GfxFillRect(
            dpi, { { topLeft + ScreenCoordsXY{ 1, 1 } }, { bottomRight - ScreenCoordsXY{ 1, 1 } } }, ColourMapA[colour].dark);
    else
        GfxFilterRect(
            dpi, { { topLeft + ScreenCoordsXY{ 1, 1 } }, { bottomRight - ScreenCoordsXY{ 1, 1 } } },
            FilterPaletteID::PaletteDarken3);

    // Draw text
    if (widget->text == STR_NONE)
        return;

    topLeft = w.windowPos + ScreenCoordsXY{ widget->left + 2, widget->top + 1 };
    int32_t width = widget->width() - 4;
    if ((widget + 1)->type == WindowWidgetType::CloseBox)
    {
        width -= 10;
        if ((widget + 2)->type == WindowWidgetType::CloseBox)
            width -= 10;
    }
    topLeft.x += width / 2;
    DrawTextEllipsised(
        dpi, topLeft, width, widget->text, Formatter::Common(),
        { COLOUR_WHITE | static_cast<uint8_t>(COLOUR_FLAG_OUTLINE), TextAlignment::CENTRE });
}

/**
 *
 *  rct2: 0x006EBB85
 */
static void WidgetCloseboxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    auto topLeft = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };
    auto bottomRight = w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom };

    // Check if the button is pressed down
    uint8_t press = 0;
    if (w.flags & WF_10)
        press |= INSET_RECT_FLAG_FILL_MID_LIGHT;
    if (WidgetIsPressed(w, widgetIndex) || WidgetIsActiveTool(w, widgetIndex))
        press |= INSET_RECT_FLAG_BORDER_INSET;

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    // Draw the button
    GfxFillRectInset(dpi, { topLeft, bottomRight }, colour, press);

    if (widget.text == STR_NONE)
        return;

    topLeft = w.windowPos + ScreenCoordsXY{ widget.midX() - 1, std::max<int32_t>(widget.top, widget.midY() - 5) };

    if (WidgetIsDisabled(w, widgetIndex))
        colour |= COLOUR_FLAG_INSET;

    DrawTextEllipsised(dpi, topLeft, widget.width() - 2, widget.text, Formatter::Common(), { colour, TextAlignment::CENTRE });
}

/**
 *
 *  rct2: 0x006EBAD9
 */
static void WidgetCheckboxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltb
    ScreenCoordsXY topLeft = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };
    ScreenCoordsXY bottomRight = w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom };
    ScreenCoordsXY midLeft = { topLeft.x, (topLeft.y + bottomRight.y) / 2 };

    // Get the colour
    colour_t colour = w.colours[widget.colour];

    // checkbox
    GfxFillRectInset(dpi, { midLeft - ScreenCoordsXY{ 0, 5 }, midLeft + ScreenCoordsXY{ 9, 4 } }, colour, INSET_RECT_F_60);

    if (WidgetIsDisabled(w, widgetIndex))
    {
        colour |= COLOUR_FLAG_INSET;
    }

    // fill it when checkbox is pressed
    if (WidgetIsPressed(w, widgetIndex))
    {
        GfxDrawString(
            dpi, { midLeft - ScreenCoordsXY{ 0, 5 } }, static_cast<const char*>(CheckBoxMarkString),
            { static_cast<colour_t>(NOT_TRANSLUCENT(colour)) });
    }

    // draw the text
    if (widget.text == STR_NONE)
        return;

    auto [stringId, formatArgs] = WidgetGetStringidAndArgs(widget);
    GfxDrawStringLeftCentred(dpi, stringId, formatArgs, colour, { midLeft + ScreenCoordsXY{ 14, 0 } });
}

/**
 *
 *  rct2: 0x006EBD96
 */
static void WidgetScrollDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    int32_t scrollIndex = WindowGetScrollDataIndex(w, widgetIndex);
    const auto& widget = w.widgets[widgetIndex];
    const auto& scroll = w.scrolls[scrollIndex];

    // Resolve the absolute ltrb
    ScreenCoordsXY topLeft = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };
    ScreenCoordsXY bottomRight = w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom };

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    // Draw the border
    GfxFillRectInset(dpi, { topLeft, bottomRight }, colour, INSET_RECT_F_60);

    // Inflate by -1
    topLeft.x++;
    topLeft.y++;
    bottomRight.x--;
    bottomRight.y--;

    // Horizontal scrollbar
    if (scroll.flags & HSCROLLBAR_VISIBLE)
        WidgetHScrollbarDraw(
            dpi, scroll, topLeft.x, bottomRight.y - SCROLLBAR_WIDTH,
            ((scroll.flags & VSCROLLBAR_VISIBLE) ? bottomRight.x - (SCROLLBAR_WIDTH + 1) : bottomRight.x), bottomRight.y,
            colour);

    // Vertical scrollbar
    if (scroll.flags & VSCROLLBAR_VISIBLE)
        WidgetVScrollbarDraw(
            dpi, scroll, bottomRight.x - SCROLLBAR_WIDTH, topLeft.y, bottomRight.x,
            ((scroll.flags & HSCROLLBAR_VISIBLE) ? bottomRight.y - (SCROLLBAR_WIDTH + 1) : bottomRight.y), colour);

    // Contents
    if (scroll.flags & HSCROLLBAR_VISIBLE)
        bottomRight.y -= (SCROLLBAR_WIDTH + 1);
    if (scroll.flags & VSCROLLBAR_VISIBLE)
        bottomRight.x -= (SCROLLBAR_WIDTH + 1);

    bottomRight.y++;
    bottomRight.x++;

    // Create a new inner scroll dpi
    DrawPixelInfo scroll_dpi = *dpi;

    // Clip the scroll dpi against the outer dpi
    int32_t cl = std::max<int32_t>(dpi->x, topLeft.x);
    int32_t ct = std::max<int32_t>(dpi->y, topLeft.y);
    int32_t cr = std::min<int32_t>(dpi->x + dpi->width, bottomRight.x);
    int32_t cb = std::min<int32_t>(dpi->y + dpi->height, bottomRight.y);

    // Set the respective dpi attributes
    scroll_dpi.x = cl - topLeft.x + scroll.h_left;
    scroll_dpi.y = ct - topLeft.y + scroll.v_top;
    scroll_dpi.width = cr - cl;
    scroll_dpi.height = cb - ct;
    scroll_dpi.bits += cl - dpi->x;
    scroll_dpi.bits += (ct - dpi->y) * (dpi->width + dpi->pitch);
    scroll_dpi.pitch = (dpi->width + dpi->pitch) - scroll_dpi.width;

    // Draw the scroll contents
    if (scroll_dpi.width > 0 && scroll_dpi.height > 0)
        WindowEventScrollPaintCall(&w, &scroll_dpi, scrollIndex);
}

static void WidgetHScrollbarDraw(
    DrawPixelInfo* dpi, const ScrollBar& scroll, int32_t l, int32_t t, int32_t r, int32_t b, int32_t colour)
{
    colour &= 0x7F;
    // Trough
    GfxFillRect(dpi, { { l + SCROLLBAR_WIDTH, t }, { r - SCROLLBAR_WIDTH, b } }, ColourMapA[colour].lighter);
    GfxFillRect(dpi, { { l + SCROLLBAR_WIDTH, t }, { r - SCROLLBAR_WIDTH, b } }, 0x1000000 | ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + SCROLLBAR_WIDTH, t + 2 }, { r - SCROLLBAR_WIDTH, t + 2 } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + SCROLLBAR_WIDTH, t + 3 }, { r - SCROLLBAR_WIDTH, t + 3 } }, ColourMapA[colour].lighter);
    GfxFillRect(dpi, { { l + SCROLLBAR_WIDTH, t + 7 }, { r - SCROLLBAR_WIDTH, t + 7 } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + SCROLLBAR_WIDTH, t + 8 }, { r - SCROLLBAR_WIDTH, t + 8 } }, ColourMapA[colour].lighter);

    // Left button
    {
        uint8_t flags = (scroll.flags & HSCROLLBAR_LEFT_PRESSED) ? INSET_RECT_FLAG_BORDER_INSET : 0;

        GfxFillRectInset(dpi, { { l, t }, { l + (SCROLLBAR_WIDTH - 1), b } }, colour, flags);
        GfxDrawString(dpi, { l + 1, t }, static_cast<const char*>(BlackLeftArrowString), {});
    }

    // Thumb
    {
        int16_t left = std::max(l + SCROLLBAR_WIDTH, l + scroll.h_thumb_left - 1);
        int16_t right = std::min(r - SCROLLBAR_WIDTH, l + scroll.h_thumb_right - 1);
        uint8_t flags = (scroll.flags & HSCROLLBAR_THUMB_PRESSED) ? INSET_RECT_FLAG_BORDER_INSET : 0;

        GfxFillRectInset(dpi, { { left, t }, { right, b } }, colour, flags);
    }

    // Right button
    {
        uint8_t flags = (scroll.flags & HSCROLLBAR_RIGHT_PRESSED) ? INSET_RECT_FLAG_BORDER_INSET : 0;

        GfxFillRectInset(dpi, { { r - (SCROLLBAR_WIDTH - 1), t }, { r, b } }, colour, flags);
        GfxDrawString(dpi, { r - 6, t }, static_cast<const char*>(BlackRightArrowString), {});
    }
}

static void WidgetVScrollbarDraw(
    DrawPixelInfo* dpi, const ScrollBar& scroll, int32_t l, int32_t t, int32_t r, int32_t b, int32_t colour)
{
    colour &= 0x7F;
    // Trough
    GfxFillRect(dpi, { { l, t + SCROLLBAR_WIDTH }, { r, b - SCROLLBAR_WIDTH } }, ColourMapA[colour].lighter);
    GfxFillRect(dpi, { { l, t + SCROLLBAR_WIDTH }, { r, b - SCROLLBAR_WIDTH } }, 0x1000000 | ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + 2, t + SCROLLBAR_WIDTH }, { l + 2, b - SCROLLBAR_WIDTH } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + 3, t + SCROLLBAR_WIDTH }, { l + 3, b - SCROLLBAR_WIDTH } }, ColourMapA[colour].lighter);
    GfxFillRect(dpi, { { l + 7, t + SCROLLBAR_WIDTH }, { l + 7, b - SCROLLBAR_WIDTH } }, ColourMapA[colour].mid_dark);
    GfxFillRect(dpi, { { l + 8, t + SCROLLBAR_WIDTH }, { l + 8, b - SCROLLBAR_WIDTH } }, ColourMapA[colour].lighter);

    // Up button
    GfxFillRectInset(
        dpi, { { l, t }, { r, t + (SCROLLBAR_WIDTH - 1) } }, colour,
        ((scroll.flags & VSCROLLBAR_UP_PRESSED) ? INSET_RECT_FLAG_BORDER_INSET : 0));
    GfxDrawString(dpi, { l + 1, t - 1 }, static_cast<const char*>(BlackUpArrowString), {});

    // Thumb
    GfxFillRectInset(
        dpi,
        { { l, std::max(t + SCROLLBAR_WIDTH, t + scroll.v_thumb_top - 1) },
          { r, std::min(b - SCROLLBAR_WIDTH, t + scroll.v_thumb_bottom - 1) } },
        colour, ((scroll.flags & VSCROLLBAR_THUMB_PRESSED) ? INSET_RECT_FLAG_BORDER_INSET : 0));

    // Down button
    GfxFillRectInset(
        dpi, { { l, b - (SCROLLBAR_WIDTH - 1) }, { r, b } }, colour,
        ((scroll.flags & VSCROLLBAR_DOWN_PRESSED) ? INSET_RECT_FLAG_BORDER_INSET : 0));
    GfxDrawString(dpi, { l + 1, b - (SCROLLBAR_WIDTH - 1) }, static_cast<const char*>(BlackDownArrowString), {});
}

/**
 *
 *  rct2: 0x006EB951
 */
static void WidgetDrawImage(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Get the image
    if (static_cast<int32_t>(widget.image.ToUInt32()) == SPR_NONE)
        return;
    auto image = widget.image;

    // Resolve the absolute ltrb
    auto screenCoords = w.windowPos + ScreenCoordsXY{ widget.left, widget.top };

    // Get the colour
    uint8_t colour = NOT_TRANSLUCENT(w.colours[widget.colour]);

    if (widget.type == WindowWidgetType::ColourBtn || widget.type == WindowWidgetType::TrnBtn
        || widget.type == WindowWidgetType::Tab)
        if (WidgetIsPressed(w, widgetIndex) || WidgetIsActiveTool(w, widgetIndex))
            image = image.WithIndexOffset(1);

    if (WidgetIsDisabled(w, widgetIndex))
    {
        // Draw greyed out (light border bottom right shadow)
        colour = w.colours[widget.colour];
        colour = ColourMapA[NOT_TRANSLUCENT(colour)].lighter;
        GfxDrawSpriteSolid(dpi, image, screenCoords + ScreenCoordsXY{ 1, 1 }, colour);

        // Draw greyed out (dark)
        colour = w.colours[widget.colour];
        colour = ColourMapA[NOT_TRANSLUCENT(colour)].mid_light;
        GfxDrawSpriteSolid(dpi, image, screenCoords, colour);
    }
    else
    {
        if (image.HasSecondary())
        {
            // ?
        }

        if (image.IsBlended())
            image = image.WithBlended(false);
        else
            image = image.WithPrimary(colour);

        GfxDrawSprite(dpi, image, screenCoords);
    }
}

bool WidgetIsDisabled(const WindowBase& w, WidgetIndex widgetIndex)
{
    if (w.classification == WindowClass::Custom)
        return w.widgets[widgetIndex].flags & WIDGET_FLAGS::IS_DISABLED;
    return (w.disabled_widgets & (1LL << widgetIndex)) != 0;
}

bool WidgetIsHoldable(const WindowBase& w, WidgetIndex widgetIndex)
{
    if (w.classification == WindowClass::Custom)
        return w.widgets[widgetIndex].flags & WIDGET_FLAGS::IS_HOLDABLE;
    return (w.hold_down_widgets & (1LL << widgetIndex)) != 0;
}

bool WidgetIsVisible(const WindowBase& w, WidgetIndex widgetIndex)
{
    return w.widgets[widgetIndex].IsVisible();
}

bool WidgetIsPressed(const WindowBase& w, WidgetIndex widgetIndex)
{
    if (w.classification == WindowClass::Custom)
    {
        if (w.widgets[widgetIndex].flags & WIDGET_FLAGS::IS_PRESSED)
        {
            return true;
        }
    }
    else
    {
        if (w.pressed_widgets & (1LL << widgetIndex))
        {
            return true;
        }
    }

    if (InputGetState() == InputState::WidgetPressed || InputGetState() == InputState::DropdownActive)
    {
        if (!(InputTestFlag(INPUT_FLAG_WIDGET_PRESSED)))
            return false;
        if (gPressedWidget.window_classification != w.classification)
            return false;
        if (gPressedWidget.window_number != w.number)
            return false;
        if (gPressedWidget.widget_index != widgetIndex)
            return false;
        return true;
    }
    return false;
}

bool WidgetIsHighlighted(const WindowBase& w, WidgetIndex widgetIndex)
{
    if (gHoverWidget.window_classification != w.classification)
        return false;
    if (gHoverWidget.window_number != w.number)
        return false;
    if (gHoverWidget.widget_index != widgetIndex)
        return false;
    return true;
}

bool WidgetIsActiveTool(const WindowBase& w, WidgetIndex widgetIndex)
{
    if (!(InputTestFlag(INPUT_FLAG_TOOL_ACTIVE)))
        return false;
    if (gCurrentToolWidget.window_classification != w.classification)
        return false;
    if (gCurrentToolWidget.window_number != w.number)
        return false;
    if (gCurrentToolWidget.widget_index != widgetIndex)
        return false;

    return true;
}

/**
 *
 *  rct2: 0x006E9F92
 *  eax: x / output_x
 *  ebx: y / output_y
 *  ecx: output_scroll_area
 *  edx: scroll_id
 *  esi: w
 *  edi: widget
 */
void WidgetScrollGetPart(
    WindowBase& w, const Widget* widget, const ScreenCoordsXY& screenCoords, ScreenCoordsXY& retScreenCoords,
    int32_t* output_scroll_area, int32_t* scroll_id)
{
    *scroll_id = 0;
    for (Widget* iterator = w.widgets; iterator != widget; iterator++)
    {
        if (iterator->type == WindowWidgetType::Scroll)
        {
            *scroll_id += 1;
        }
    }

    const auto& scroll = w.scrolls[*scroll_id];
    if ((scroll.flags & HSCROLLBAR_VISIBLE) && screenCoords.y >= (w.windowPos.y + widget->bottom - (SCROLLBAR_WIDTH + 1)))
    {
        // horizontal scrollbar
        int32_t rightOffset = 0;
        int32_t iteratorLeft = widget->left + w.windowPos.x + SCROLLBAR_WIDTH;
        int32_t iteratorRight = widget->right + w.windowPos.x - SCROLLBAR_WIDTH;
        if (!(scroll.flags & VSCROLLBAR_VISIBLE))
        {
            rightOffset = SCROLLBAR_WIDTH + 1;
        }

        if (screenCoords.x <= iteratorLeft)
        {
            *output_scroll_area = SCROLL_PART_HSCROLLBAR_LEFT;
        }
        else if (screenCoords.x >= iteratorRight + rightOffset)
        {
            *output_scroll_area = SCROLL_PART_NONE;
        }
        else if (screenCoords.x >= iteratorRight + rightOffset - SCROLLBAR_WIDTH)
        {
            *output_scroll_area = SCROLL_PART_HSCROLLBAR_RIGHT;
        }
        else if (screenCoords.x < (widget->left + w.windowPos.x + scroll.h_thumb_left))
        {
            *output_scroll_area = SCROLL_PART_HSCROLLBAR_LEFT_TROUGH;
        }
        else if (screenCoords.x > (widget->left + w.windowPos.x + scroll.h_thumb_right))
        {
            *output_scroll_area = SCROLL_PART_HSCROLLBAR_RIGHT_TROUGH;
        }
        else
        {
            *output_scroll_area = SCROLL_PART_HSCROLLBAR_THUMB;
        }
    }
    else if ((scroll.flags & VSCROLLBAR_VISIBLE) && (screenCoords.x >= w.windowPos.x + widget->right - (SCROLLBAR_WIDTH + 1)))
    {
        // vertical scrollbar
        int32_t bottomOffset = 0;
        int32_t iteratorTop = widget->top + w.windowPos.y + SCROLLBAR_WIDTH;
        int32_t iteratorBottom = widget->bottom + w.windowPos.y;
        if (scroll.flags & HSCROLLBAR_VISIBLE)
        {
            bottomOffset = (SCROLLBAR_WIDTH + 1);
        }

        if (screenCoords.y <= iteratorTop)
        {
            *output_scroll_area = SCROLL_PART_VSCROLLBAR_TOP;
        }
        else if (screenCoords.y >= (iteratorBottom - bottomOffset))
        {
            *output_scroll_area = SCROLL_PART_NONE;
        }
        else if (screenCoords.y >= (iteratorBottom - bottomOffset - SCROLLBAR_WIDTH))
        {
            *output_scroll_area = SCROLL_PART_VSCROLLBAR_BOTTOM;
        }
        else if (screenCoords.y < (widget->top + w.windowPos.y + scroll.v_thumb_top))
        {
            *output_scroll_area = SCROLL_PART_VSCROLLBAR_TOP_TROUGH;
        }
        else if (screenCoords.y > (widget->top + w.windowPos.y + scroll.v_thumb_bottom))
        {
            *output_scroll_area = SCROLL_PART_VSCROLLBAR_BOTTOM_TROUGH;
        }
        else
        {
            *output_scroll_area = SCROLL_PART_VSCROLLBAR_THUMB;
        }
    }
    else
    {
        // view
        *output_scroll_area = SCROLL_PART_VIEW;
        retScreenCoords.x = screenCoords.x - widget->left;
        retScreenCoords.y = screenCoords.y - widget->top;
        retScreenCoords -= w.windowPos;
        if (retScreenCoords.x <= 0 || retScreenCoords.y <= 0)
        {
            *output_scroll_area = SCROLL_PART_NONE;
        }
        else
        {
            retScreenCoords.x += scroll.h_left - 1;
            retScreenCoords.y += scroll.v_top - 1;
        }
    }
}

Widget* GetWidgetByIndex(const WindowBase& w, WidgetIndex widgetIndex)
{
    // Make sure we don't go out of bounds if we are given a bad widget index
    WidgetIndex index = 0;
    for (auto* widget = w.widgets; widget->type != WindowWidgetType::Last; widget++)
    {
        if (index == widgetIndex)
        {
            return widget;
        }
        index++;
    }

    LOG_ERROR("Widget index %i out of bounds for window class %u", widgetIndex, w.classification);

    return nullptr;
}

static void SafeSetWidgetFlag(WindowBase& w, WidgetIndex widgetIndex, WidgetFlags mask, bool value)
{
    Widget* widget = GetWidgetByIndex(w, widgetIndex);
    if (widget == nullptr)
    {
        return;
    }

    if (value)
        widget->flags |= mask;
    else
        widget->flags &= ~mask;
}

void WidgetSetEnabled(WindowBase& w, WidgetIndex widgetIndex, bool enabled)
{
    WidgetSetDisabled(w, widgetIndex, !enabled);
}

void WidgetSetDisabled(WindowBase& w, WidgetIndex widgetIndex, bool value)
{
    SafeSetWidgetFlag(w, widgetIndex, WIDGET_FLAGS::IS_DISABLED, value);
    if (value)
    {
        w.disabled_widgets |= (1uLL << widgetIndex);
    }
    else
    {
        w.disabled_widgets &= ~(1uLL << widgetIndex);
    }
}

void WidgetSetHoldable(WindowBase& w, WidgetIndex widgetIndex, bool value)
{
    SafeSetWidgetFlag(w, widgetIndex, WIDGET_FLAGS::IS_HOLDABLE, value);
    if (value)
    {
        w.hold_down_widgets |= (1uLL << widgetIndex);
    }
    else
    {
        w.hold_down_widgets &= ~(1uLL << widgetIndex);
    }
}

void WidgetSetVisible(WindowBase& w, WidgetIndex widgetIndex, bool value)
{
    SafeSetWidgetFlag(w, widgetIndex, WIDGET_FLAGS::IS_HIDDEN, !value);
}

void WidgetSetPressed(WindowBase& w, WidgetIndex widgetIndex, bool value)
{
    SafeSetWidgetFlag(w, widgetIndex, WIDGET_FLAGS::IS_PRESSED, value);
    if (value)
        w.pressed_widgets |= (1uLL << widgetIndex);
    else
        w.pressed_widgets &= ~(1uLL << widgetIndex);
}

void WidgetSetCheckboxValue(WindowBase& w, WidgetIndex widgetIndex, bool value)
{
    WidgetSetPressed(w, widgetIndex, value);
}

static void WidgetTextBoxDraw(DrawPixelInfo* dpi, WindowBase& w, WidgetIndex widgetIndex)
{
    int32_t no_lines = 0;
    char wrapped_string[TEXT_INPUT_SIZE];

    // Get the widget
    const auto& widget = w.widgets[widgetIndex];

    // Resolve the absolute ltrb
    ScreenCoordsXY topLeft{ w.windowPos + ScreenCoordsXY{ widget.left, widget.top } };
    ScreenCoordsXY bottomRight{ w.windowPos + ScreenCoordsXY{ widget.right, widget.bottom } };

    // Get the colour
    uint8_t colour = w.colours[widget.colour];

    bool active = w.classification == gCurrentTextBox.window.classification && w.number == gCurrentTextBox.window.number
        && widgetIndex == gCurrentTextBox.widget_index;

    // GfxFillRectInset(dpi, l, t, r, b, colour, 0x20 | (!active ? 0x40 : 0x00));
    GfxFillRectInset(dpi, { topLeft, bottomRight }, colour, INSET_RECT_F_60);

    // Figure out where the text should be positioned vertically.
    topLeft.y = w.windowPos.y + widget.textTop();

    if (!active || gTextInput == nullptr)
    {
        if (widget.text != 0)
        {
            SafeStrCpy(wrapped_string, widget.string, 512);
            GfxWrapString(wrapped_string, bottomRight.x - topLeft.x - 5, FontStyle::Medium, &no_lines);
            GfxDrawStringNoFormatting(dpi, { topLeft.x + 2, topLeft.y }, wrapped_string, { w.colours[1], FontStyle::Medium });
        }
        return;
    }

    SafeStrCpy(wrapped_string, gTextBoxInput, TEXT_INPUT_SIZE);

    // String length needs to add 12 either side of box
    // +13 for cursor when max length.
    GfxWrapString(wrapped_string, bottomRight.x - topLeft.x - 5 - 6, FontStyle::Medium, &no_lines);

    GfxDrawStringNoFormatting(dpi, { topLeft.x + 2, topLeft.y }, wrapped_string, { w.colours[1], FontStyle::Medium });

    size_t string_length = GetStringSize(wrapped_string) - 1;

    // Make a copy of the string for measuring the width.
    char temp_string[TEXT_INPUT_SIZE] = { 0 };
    std::memcpy(temp_string, wrapped_string, std::min(string_length, gTextInput->SelectionStart));
    int32_t cur_x = topLeft.x + GfxGetStringWidthNoFormatting(temp_string, FontStyle::Medium) + 3;

    int32_t width = 6;
    if (static_cast<uint32_t>(gTextInput->SelectionStart) < strlen(gTextBoxInput))
    {
        // Make a new 1 character wide string for measuring the width
        // of the character that the cursor is under.
        temp_string[1] = '\0';
        temp_string[0] = gTextBoxInput[gTextInput->SelectionStart];
        width = std::max(GfxGetStringWidthNoFormatting(temp_string, FontStyle::Medium) - 2, 4);
    }

    if (gTextBoxFrameNo <= 15)
    {
        colour = ColourMapA[w.colours[1]].mid_light;
        auto y = topLeft.y + (widget.height() - 1);
        GfxFillRect(dpi, { { cur_x, y }, { cur_x + width, y } }, colour + 5);
    }
}

ImageId GetColourButtonImage(colour_t colour)
{
    return ImageId(SPR_PALETTE_BTN, colour).WithBlended(true);
}
