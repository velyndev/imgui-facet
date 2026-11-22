// ChartRangeBrush (facet/widgets/charts.h): a small overview of the series (lines without axes)
// with a window from *low to *high over it, in the x units of the series. The window's edges are
// the two handles of the multi-handle block (src/internal/multi_handle.h), as on RangeSlider:
// each its own item, Tab stop and focus ring, moved by dragging or by the arrow keys (Shift fine,
// Ctrl coarse, Home and End); the window body drags both; a press outside the window moves the
// nearer edge; the edges never cross and keep minGap; Escape restores the values from the start
// of the edit. The wheel zooms the window around the pointer while an edge has keyboard focus or
// Alt is held, so a page full of brushes still scrolls. The label is the tooltip of the overview.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/charts/chart_parts.h"

namespace facet
{
namespace
{

using internal::SeriesView;

// One wheel notch toward the user zooms in to this share of the window (away: out by its inverse).
constexpr double kBrushZoomStep = 0.8;
// The grip of an edge: this share of the brush height, at most the slider knob size.
constexpr float kBrushGripShare = 0.6f;

// The grips of the last frame, for ChartBrushHandleRect.
struct BrushState
{
    ImRect grips[2];
    ImGuiID ids[2] = {0, 0};
};

// The window zoomed by `notches` wheel notches around `anchor`, kept inside [min, max] and at least
// `gap` wide.
void ZoomBrush(double values[2], double anchor, float notches, double min, double max, double gap)
{
    const double factor = std::pow(kBrushZoomStep, static_cast<double>(notches));
    double lo = anchor + (values[0] - anchor) * factor;
    double hi = anchor + (values[1] - anchor) * factor;
    const double range = max - min;
    double span = hi - lo;
    if (span < gap)
    {
        const double share = values[1] > values[0] ? (anchor - values[0]) / (values[1] - values[0]) : 0.5;
        lo = anchor - gap * std::clamp(share, 0.0, 1.0);
        span = gap;
    }
    span = std::min(span, range);
    lo = std::clamp(lo, min, max - span);
    values[0] = lo;
    values[1] = lo + span;
}

} // namespace

bool ChartRangeBrush(const char* label, double* low, double* high, double min, double max,
                     std::span<const ChartSeries> overview, const ChartRangeBrushOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    if (min > max)
        std::swap(min, max);
    const bool missing = low == nullptr || high == nullptr;
    const bool readOnly = o.readOnly || missing || !(max > min) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const ImGuiID id = window->GetID(label);

    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float height = std::max(1.0f, std::round(o.height > 0.0f ? Px(o.height) : ControlHeight(ControlSize::Inherit) * 2.0f));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    // The grips sit on the window's edges; the track keeps half a grip inside the box at both ends.
    const float gripWidth = std::max(2.0f, std::round(Px(t.spacing.sm)));
    const float gripHeight = std::max(2.0f, std::round(std::min(height * kBrushGripShare, Px(t.metrics.sliderKnobSize))));
    const float inset = std::min(std::ceil(gripWidth * 0.5f), std::floor(width * 0.25f));
    const float midY = std::round(bounds.GetCenter().y);
    const ImRect track(bounds.Min.x + inset, midY, std::max(bounds.Min.x + inset, bounds.Max.x - inset), midY);

    double values[2] = {missing ? min : *low, missing ? max : *high};
    internal::ItemState handles[2];
    internal::MultiHandleOptions mo;
    mo.constraints.min = min;
    mo.constraints.max = max;
    mo.constraints.minGap = std::isfinite(o.minGap) ? o.minGap : 0.0;
    mo.drag.mapping.min = min;
    mo.drag.mapping.max = max;
    mo.drag.wheel = false;
    mo.drag.readOnly = readOnly;
    mo.handleSize = std::max(gripWidth, Px(t.spacing.lg));
    mo.spanDrag = true;

    // The block lays out the brush (ItemSize of `bounds` at the cursor) and adds its items.
    ImGui::BeginGroup();
    ImGui::SetCursorScreenPos(bounds.Min);
    const internal::MultiHandleResult r = internal::MultiHandleBehavior(id, bounds, track, values, handles, mo);
    bool changed = r.changed && !readOnly;
    const bool disabled = r.track.disabled || groupDisabled;

    // The wheel zooms around the pointer while an edge is focused or Alt is held.
    const ImGuiIO& io = ImGui::GetIO();
    const bool focused = r.focusedHandle >= 0;
    if (o.wheelZoom && !readOnly && !disabled && r.track.hovered && (focused || io.KeyAlt) &&
        !internal::ScrollWheelBelongsToScroll() && track.GetWidth() > 0.0f)
    {
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, id);
        if (ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, id) && io.MouseWheel != 0.0f)
        {
            const double fraction = std::clamp(static_cast<double>((io.MousePos.x - track.Min.x) / track.GetWidth()), 0.0, 1.0);
            const double before[2] = {values[0], values[1]};
            ZoomBrush(values, min + fraction * (max - min), io.MouseWheel, min, max,
                      internal::EffectiveGap(mo.constraints, 2));
            if (values[0] != before[0] || values[1] != before[1])
            {
                changed = true;
                internal::MarkWidgetEdited(id);
            }
        }
    }
    if (changed)
    {
        *low = values[0];
        *high = values[1];
    }

    // ---- Draw ----
    ImDrawList* drawList = window->DrawList;
    drawList->PushClipRect(bounds.Min, bounds.Max, true);
    const float x0 = std::round(internal::FractionToTrack(track, Orientation::Horizontal, internal::ValueToFraction(mo.drag.mapping, values[0])));
    const float x1 = std::round(internal::FractionToTrack(track, Orientation::Horizontal, internal::ValueToFraction(mo.drag.mapping, values[1])));
    const float radius = Px(t.radii.element);
    // The plot: a wash outside the window, the accent wash inside it, the overview lines over both.
    FillRect(drawList, bounds.Min, bounds.Max, ToU32(disabled ? c.fill[3] : c.fill[2]), radius);
    if (x1 > x0)
        FillRect(drawList, ImVec2(x0, bounds.Min.y), ImVec2(x1, bounds.Max.y), ToU32(disabled ? c.fill[3] : c.accentSubtle));
    const ImRect plot = internal::SnapPlotRect(
        ImRect(track.Min.x, bounds.Min.y + inset, track.Max.x, bounds.Max.y - inset));
    if (plot.GetWidth() >= 1.0f && plot.GetHeight() >= 1.0f)
    {
        internal::Range ys;
        for (const ChartSeries& one : overview)
        {
            if (one.hidden)
                continue;
            const internal::Range r1 = internal::SeriesRangeY(internal::ChartSeriesView(one));
            if (r1.valid)
                ys = ys.valid ? internal::Range{std::min(ys.min, r1.min), std::max(ys.max, r1.max), true} : r1;
        }
        ys = internal::PadRange(ys, 0.05, internal::ScaleKind::Linear);
        const internal::Scale xScale = internal::MakeScale(internal::ScaleKind::Linear, min, max, plot.Min.x, plot.Max.x);
        const internal::Scale yScale = internal::MakeScale(internal::ScaleKind::Linear, ys.valid ? ys.min : 0.0,
                                                           ys.valid ? ys.max : 1.0, plot.Max.y, plot.Min.y);
        std::span<ImVec2> points = internal::ChartScratchPoints(internal::DecimatedCapacity(plot.GetWidth()));
        const float lineWidth = std::max(HairlineThickness(), std::round(Px(t.charts.lineWidth) * 0.5f));
        for (int s = 0; s < static_cast<int>(overview.size()); ++s)
        {
            const ChartSeries& one = overview[static_cast<std::size_t>(s)];
            if (one.hidden)
                continue;
            const SeriesView view = internal::ChartSeriesView(one);
            const bool sorted = internal::IsSortedByX(view);
            const ImU32 color = ToU32(internal::ChartDataColor(one.color, s, disabled));
            int from = 0, to = 0;
            internal::VisibleSampleRange(view, xScale, sorted, &from, &to);
            for (int begin = internal::NextUsableSample(view, from, to); begin < to;)
            {
                const int end = internal::NextGap(view, begin, to);
                const int n = internal::DecimatePolyline(view, xScale, yScale, points, sorted, begin, end, {});
                if (n > 1)
                    internal::DrawPolyline(drawList, std::span<const ImVec2>(points.data(), static_cast<std::size_t>(n)), color,
                                           lineWidth, bounds);
                begin = internal::NextUsableSample(view, end, to);
            }
        }
    }

    // The window's edges and their grips.
    const ImVec4 edge = disabled ? c.labelTertiary : c.focusRing;
    const float edgeWidth = std::max(HairlineThickness(), std::round(Px(t.charts.lineWidth)));
    const float xs[2] = {x0, x1};
    BrushState& state = internal::GetState<BrushState>(id);
    for (int i = 0; i < 2; ++i)
    {
        const float x = i == 0 ? xs[i] : xs[i] - edgeWidth;
        FillRect(drawList, ImVec2(x, bounds.Min.y), ImVec2(x + edgeWidth, bounds.Max.y), ToU32(edge));
    }
    for (int i = 0; i < 2; ++i)
    {
        const internal::ItemState& h = handles[i];
        const ImRect grip(ImVec2(std::round(xs[i] - gripWidth * 0.5f), std::round(midY - gripHeight * 0.5f)),
                          ImVec2(std::round(xs[i] - gripWidth * 0.5f) + gripWidth, std::round(midY - gripHeight * 0.5f) + gripHeight));
        state.grips[i] = grip;
        state.ids[i] = h.id;
        const float gripRadius = std::min(radius, gripWidth * 0.5f);
        const ImVec4 rest = c.ElevatedSurface(1);
        const ImVec4 fill = disabled ? c.fill[3] : internal::StateFill(rest, Mix(rest, c.fill[1], 0.5f), c.fill[0], h);
        FillRect(drawList, grip.Min, grip.Max, ToU32(fill), gripRadius);
        // The grip is the control: its fill and outline color follow hover and press, at the one
        // hairline width. The window body between the edges keeps its look under the pointer.
        internal::DrawOutline(drawList, grip, gripRadius,
                              internal::ControlOutlineColor({.hoverAmount = h.hoverAmount, .disabled = disabled}));
    }
    drawList->PopClipRect();
    for (int i = 0; i < 2; ++i)
        internal::DrawFocusRing(drawList, state.grips[i], std::min(radius, gripWidth * 0.5f), handles[i].focusAmount);

    // The label is the tooltip of the overview, on hover and on keyboard focus of an edge.
    const internal::ChartLabelText name = internal::ChartVisibleLabel(label);
    internal::CutTextTooltip(id, bounds, r.track.hovered, handles[0].focusVisible || handles[1].focusVisible, name.text,
                             name.end, !name.Empty());
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return changed;
}

namespace internal
{

ImRect ChartBrushHandleRect(ImGuiID brushId, int handle)
{
    const auto* state = static_cast<const BrushState*>(GlobalStateStore().Find(brushId, StateTypeOf<BrushState>()));
    return state != nullptr && handle >= 0 && handle < 2 ? state->grips[handle] : ImRect();
}

ImGuiID ChartBrushHandleId(ImGuiID brushId, int handle)
{
    const auto* state = static_cast<const BrushState*>(GlobalStateStore().Find(brushId, StateTypeOf<BrushState>()));
    return state != nullptr && handle >= 0 && handle < 2 ? state->ids[handle] : 0;
}

} // namespace internal

} // namespace facet
