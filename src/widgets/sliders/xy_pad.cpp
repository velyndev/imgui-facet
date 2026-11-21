// XYPad: a two-dimensional area with one draggable point. The area is a field frame with an
// optional grid; the point is the slider knob, with guide lines through it and the two values as
// readouts at the edges of the frame. One item and one Tab stop under the label's id.
//
// Input is the two-axis value drag (src/internal/value_drag.h): a press on the point grabs it, a
// press elsewhere moves it there first, Shift keeps the axis the drag goes along, Ctrl snaps to
// the coarse grid; the arrows step x and y, Page Up and Down move y by a tenth, Home and End send
// x to its ends, Escape restores the point from the start of the edit.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/text_field_number.h"
#include "internal/value_drag.h"
#include "widgets/sliders/pad_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

// A readout moves to the other side of the frame when the point comes within this many text
// lines of its usual edge, so the knob never covers it.
constexpr float kXYPadReadoutAvoid = 1.5f;

// One readout with its top-left at `at`, kept inside the frame.
void DrawXYPadReadout(ImDrawList* drawList, const ImRect& frame, const char* text, ImVec2 at, bool disabled)
{
    const ImVec2 size = ImGui::CalcTextSize(text);
    const float x = std::round(std::clamp(at.x, frame.Min.x, std::max(frame.Min.x, frame.Max.x - size.x)));
    const float y = std::round(std::clamp(at.y, frame.Min.y, std::max(frame.Min.y, frame.Max.y - size.y)));
    drawList->AddText(ImVec2(x, y), ToU32(internal::ControlSecondaryColor(disabled)), text);
}

} // namespace

bool XYPad(const char* label, ImVec2* value, ImVec2 min, ImVec2 max, const XYPadOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(ControlSize::Inherit);
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;

    internal::ValueMapping mx;
    mx.min = internal::SliderFloatToDouble(min.x);
    mx.max = internal::SliderFloatToDouble(max.x);
    mx.step = o.step.x;
    mx = internal::NormalizeMapping(mx);
    internal::ValueMapping my;
    my.min = internal::SliderFloatToDouble(min.y);
    my.max = internal::SliderFloatToDouble(max.y);
    my.step = o.step.y;
    my = internal::NormalizeMapping(my);
    // An axis without a range does not move; with neither the pad shows the point read-only.
    const bool moveX = mx.max > mx.min;
    const bool moveY = my.max > my.min;
    const bool readOnly = o.readOnly || value == nullptr || !(moveX || moveY) ||
                          (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    double x = value != nullptr ? internal::SliderValueIn(value->x) : mx.min;
    double y = value != nullptr ? internal::SliderValueIn(value->y) : my.min;
    const double startX = x;
    const double startY = y;

    // Size: the width rule, square unless a height is given.
    const float width = internal::ResolvePadWidth(o.width, next);
    const float height = o.height > 0.0f ? std::max(1.0f, std::round(Px(o.height))) : width;
    const internal::PadFrame pad = internal::LayoutPadFrame(window->DC.CursorPos, width, height, m);

    internal::ItemOptions itemOptions;
    itemOptions.press = internal::PressMode::Click;
    itemOptions.readOnly = readOnly;
    itemOptions.mixed = o.mixed;
    ImGui::BeginGroup();
    ImGui::ItemSize(pad.frame.GetSize());
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::PartBehavior(id, pad.frame, itemOptions);
    internal::ValueDrag2DOptions drag;
    drag.mappingX = mx;
    drag.mappingY = my;
    drag.yDown = o.yDown;
    drag.grabRadius = pad.knob * 0.5f;
    drag.moveX = moveX;
    drag.moveY = moveY;
    drag.readOnly = readOnly;
    const internal::ValueDrag2DResult edit = internal::ValueDrag2DBehavior(item, &x, &y, pad.area, drag);
    const bool writtenX = edit.changed && x != startX;
    const bool writtenY = edit.changed && y != startY;
    const bool written = (writtenX || writtenY) && value != nullptr && !readOnly;
    if (written)
    {
        if (writtenX)
            value->x = static_cast<float>(x);
        if (writtenY)
            value->y = static_cast<float>(y);
    }

    // ---- Draw ----
    const bool disabled = item.disabled;
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    const bool mixed = (o.mixed || item.mixed) && !written;
    ImDrawList* drawList = window->DrawList;
    if (item.visible)
    {
        internal::FieldFrameLook frameLook;
        frameLook.hoverAmount = item.hoverAmount;
        frameLook.disabled = disabled;
        frameLook.readOnly = readOnly && !disabled;
        frameLook.error = o.error != nullptr;
        internal::DrawFieldFrame(drawList, pad.frame, frameLook);
        drawList->PushClipRect(pad.frame.Min, pad.frame.Max, true);
        if (o.grid)
            internal::DrawPadGrid(drawList, pad.area, mx, my, o.yDown);
        const ImVec2 point = internal::PadPoint(pad.area, mx, my, o.yDown, x, y);
        if (!mixed && o.crosshair)
        {
            // Guide lines through the point in labelTertiary: decoration that shows where it is
            // against the edges, quieter than the knob.
            const ImU32 guide = ToU32(disabled ? c.labelQuaternary : c.labelTertiary);
            HairlineV(drawList, std::round(point.x), pad.frame.Min.y, pad.frame.Max.y, guide);
            HairlineH(drawList, pad.frame.Min.x, pad.frame.Max.x, std::round(point.y), guide);
            // The values at the edges: x beside the vertical guide, y above the horizontal one.
            internal::ControlTextScope footnote(TextStyle::Footnote);
            const float inset = PxSnapped(t.spacing.xs);
            const float line = std::ceil(ImGui::GetFontSize());
            char textX[64];
            char textY[64];
            internal::FormatNumber(textX, sizeof(textX), x, o.format, o.formatter, o.formatterUser, false, false);
            internal::FormatNumber(textY, sizeof(textY), y, o.format, o.formatter, o.formatterUser, false, false);
            const float widthX = ImGui::CalcTextSize(textX).x;
            const float widthY = ImGui::CalcTextSize(textY).x;
            // Only when both fit beside each other: a tiny pad shows the point alone.
            if (widthX + widthY + inset * 4.0f <= pad.frame.GetWidth() && line * 2.0f + inset * 4.0f <= pad.frame.GetHeight())
            {
                const bool xAtTop = point.y > pad.frame.Max.y - line * kXYPadReadoutAvoid - inset * 2.0f;
                const float rowX = xAtTop ? pad.frame.Min.y + inset : pad.frame.Max.y - inset - line;
                const bool xAtLeft = point.x + inset + widthX > pad.frame.Max.x - inset;
                DrawXYPadReadout(drawList, pad.frame, textX,
                                 ImVec2(xAtLeft ? point.x - inset - widthX : point.x + inset, rowX), disabled);
                const bool yAtRight = point.x < pad.frame.Min.x + widthY + inset * 2.0f + pad.knob;
                const float colY = yAtRight ? pad.frame.Max.x - inset - widthY : pad.frame.Min.x + inset;
                const bool yBelow = point.y - inset - line < pad.frame.Min.y + inset;
                DrawXYPadReadout(drawList, pad.frame, textY,
                                 ImVec2(colY, yBelow ? point.y + inset : point.y - inset - line), disabled);
            }
        }
        drawList->PopClipRect();
        if (mixed)
            internal::DrawFocusRing(drawList, pad.frame, Px(t.radii.control), item.focusAmount);
        else
            internal::DrawSliderKnob(drawList, internal::PadKnobRect(point, pad.knob), item, look);
    }

    // The label is not drawn beside a pad: it is the tooltip on hover and keyboard focus.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, pad.frame, item.hovered, item.focusVisible, label, labelEnd, labelEnd != label);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(pad.frame.Min.x, pad.frame.Max.y), pad.frame.GetWidth(), disabled, id,
                                     pad.frame);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
