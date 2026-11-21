// ImageCompare (facet/widgets/media.h): two images of the same size on top of each other, the first
// shown before the divider and the second after it. The divider is one handle of the multi-handle
// block (src/internal/multi_handle.h): drag it, press anywhere to move it there, or focus it and use
// the arrows (Shift fine, Ctrl coarse), Page Up and Down, Home and End.

#include "facet/widgets/media.h"

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
#include "internal/multi_handle.h"
#include "internal/popup_host.h"
#include "widgets/media/media_look.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

// The height for an unknown aspect ratio, as a share of the width.
constexpr float kCompareAspect = 9.0f / 16.0f;

} // namespace

bool ImageCompare(const char* label, float* split, ImageHandle before, ImageHandle after,
                  const ImageCompareOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();
    const Colors& c = t.colors;
    const bool vertical = options.orientation == Orientation::Vertical;
    const internal::MediaImage first = internal::MediaImageOf(before);
    const internal::MediaImage second = internal::MediaImageOf(after);

    // Measure: the available width, the height from the first image (else the second).
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const ImVec2 natural = first.status == ImageStatus::Ready ? first.size : second.size;
    const float aspect = natural.x > 0.0f && natural.y > 0.0f ? natural.y / natural.x : kCompareAspect;
    const float height = std::max(1.0f, std::round(options.height > 0.0f ? Px(options.height) : width * aspect));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    window->DC.CursorPos = origin;
    const ImRect rect(origin, ImVec2(origin.x + width, origin.y + height));
    const ImVec2 center(std::round(rect.GetCenter().x), std::round(rect.GetCenter().y));
    // The handle centre travels the whole width (height); vertical tracks count from the bottom, so
    // the handle value is 1 - split there.
    const ImRect track = vertical ? ImRect(center.x, rect.Min.y, center.x, rect.Max.y)
                                  : ImRect(rect.Min.x, center.y, rect.Max.x, center.y);
    const bool readOnly = options.readOnly || split == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const double raw = split != nullptr && std::isfinite(*split) ? std::clamp(static_cast<double>(*split), 0.0, 1.0) : 0.5;
    double values[1] = {vertical ? 1.0 - raw : raw};
    const double drawn = values[0];
    const internal::SliderMetrics m = internal::GetSliderMetrics(ControlSize::Regular);

    ImGui::BeginGroup();
    ImGui::ItemSize(rect.GetSize());
    const ImVec2 cursor = window->DC.CursorPos;
    window->DC.CursorPos = rect.Min;
    internal::ApplyNextItemData(next);
    internal::MultiHandleOptions handleOptions;
    handleOptions.constraints.min = 0.0;
    handleOptions.constraints.max = 1.0;
    handleOptions.drag.mapping = {.min = 0.0, .max = 1.0};
    handleOptions.drag.orientation = options.orientation;
    handleOptions.drag.readOnly = readOnly;
    handleOptions.handleSize = m.knob;
    handleOptions.spanDrag = false;
    internal::ItemState handles[1];
    const internal::MultiHandleResult r = internal::MultiHandleBehavior(id, rect, track, std::span<double>(values, 1),
                                                                        std::span<internal::ItemState>(handles, 1), handleOptions);
    window->DC.CursorPos = cursor;
    const bool disabled = r.track.disabled;
    const bool written = r.changed && !readOnly && !disabled && values[0] != drawn;
    if (written)
        *split = static_cast<float>(vertical ? 1.0 - values[0] : values[0]);

    // Draw: the first image before the divider, the second after it, then the divider and its knob.
    if (ImGui::IsRectVisible(rect.Min, rect.Max))
    {
        ImDrawList* drawList = window->DrawList;
        const float radius = std::min(Px(t.radii.card), 0.5f * std::min(width, height));
        const double shown = values[0];
        const float at = internal::SliderTrackPos(track, options.orientation, shown);
        internal::MediaDrawOptions draw;
        draw.fit = options.fit;
        draw.radius = radius;
        FillRect(drawList, rect.Min, rect.Max, ToU32(disabled ? c.fill[3] : c.fill[2]), radius);
        const internal::MediaText beforeAlt = internal::MediaVisibleText(options.beforeLabel);
        const internal::MediaText afterAlt = internal::MediaVisibleText(options.afterLabel);
        const ImRect firstPart = vertical ? ImRect(rect.Min, ImVec2(rect.Max.x, at)) : ImRect(rect.Min, ImVec2(at, rect.Max.y));
        const ImRect secondPart = vertical ? ImRect(ImVec2(rect.Min.x, at), rect.Max) : ImRect(ImVec2(at, rect.Min.y), rect.Max);
        if (firstPart.GetWidth() > 0.0f && firstPart.GetHeight() > 0.0f)
        {
            drawList->PushClipRect(firstPart.Min, firstPart.Max, true);
            internal::MediaDrawImageSlot(drawList, first, rect, draw, beforeAlt.text);
            drawList->PopClipRect();
        }
        if (secondPart.GetWidth() > 0.0f && secondPart.GetHeight() > 0.0f)
        {
            drawList->PushClipRect(secondPart.Min, secondPart.Max, true);
            internal::MediaDrawImageSlot(drawList, second, rect, draw, afterAlt.text);
            drawList->PopClipRect();
        }
        if (!beforeAlt.IsEmpty())
            internal::MediaDrawCaption(drawList, firstPart, internal::MediaCorner::TopLeft, beforeAlt.text, disabled);
        if (!afterAlt.IsEmpty())
            internal::MediaDrawCaption(drawList, secondPart, vertical ? internal::MediaCorner::BottomLeft : internal::MediaCorner::TopRight,
                                       afterAlt.text, disabled);

        // The divider: a line of the surface color between two boundary hairlines, so it shows
        // over light and dark pictures alike.
        const float line = std::max(1.0f, PxSnapped(t.media.playheadWidth));
        const float hair = HairlineThickness();
        const float lineStart = std::round(at - line * 0.5f);
        const ImU32 edge = ToU32(disabled ? c.labelTertiary : ControlBoundaryColor(c));
        drawList->PushClipRect(rect.Min, rect.Max, true);
        if (vertical)
        {
            FillRect(drawList, ImVec2(rect.Min.x, lineStart - hair), ImVec2(rect.Max.x, lineStart + line + hair), edge);
            FillRect(drawList, ImVec2(rect.Min.x, lineStart), ImVec2(rect.Max.x, lineStart + line), ToU32(c.bg));
        }
        else
        {
            FillRect(drawList, ImVec2(lineStart - hair, rect.Min.y), ImVec2(lineStart + line + hair, rect.Max.y), edge);
            FillRect(drawList, ImVec2(lineStart, rect.Min.y), ImVec2(lineStart + line, rect.Max.y), ToU32(c.bg));
        }
        drawList->PopClipRect();
        // The knob stays inside the frame at the ends, the line itself goes all the way.
        ImRect knob = internal::SliderKnobRect(track, options.orientation, m.knob, shown);
        const ImVec2 shift(std::max(0.0f, rect.Min.x - knob.Min.x) - std::max(0.0f, knob.Max.x - rect.Max.x),
                           std::max(0.0f, rect.Min.y - knob.Min.y) - std::max(0.0f, knob.Max.y - rect.Max.y));
        knob.Translate(shift);
        internal::DrawSliderKnob(drawList, knob, handles[0], internal::PickSliderLook(disabled, readOnly));
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    const internal::MediaText alt = internal::MediaVisibleText(label);
    if (!alt.IsEmpty() &&
        internal::BeginPopupTooltip(id, rect, (r.track.hovered || handles[0].hovered) && r.activeHandle < 0, handles[0].focusVisible))
    {
        Text(TextStyle::Footnote, "%s", alt.text);
        internal::EndPopupTooltip();
    }
    return written;
}

} // namespace facet
