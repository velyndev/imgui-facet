// CurveEditor (facet/widgets/composites.h): a response curve through control points the caller
// keeps sorted by x in its own storage.
//
// The canvas is a field frame with the grid of the chart core's nice ticks; the curve is drawn
// through the points (monotone cubic, linear or steps) in the first series color. Each point is its
// own item and Tab stop, in the order of the points, driven by the two-axis value drag
// (src/internal/value_drag.h): a press on a point grabs it, Shift keeps the axis the drag goes along,
// Ctrl snaps to the coarse grid; the arrows step x and y (Shift finer, Ctrl coarser), Page Up and
// Down move y by a tenth, Home and End send x to its neighbours' limits, Escape restores the point
// from the start of the edit. A point never passes its neighbours: its x is clamped between theirs.
// With lockEnds the first and last point move only up and down.
//
// A double click on empty canvas adds a point there, between the points around it, when the
// storage has room (the vector overload grows its vector); a double click on a point, or Delete or
// Backspace while it has the focus, removes it, never below two points and never a locked end. The
// focused or dragged point shows its x and y at the top of the canvas.
//
// Nothing is allocated per frame: the curve is drawn in chunks of a fixed buffer and the points are
// drawn as they are submitted, on a draw list channel above the curve.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/composites/curve_parts.h"
#include "widgets/composites/editors_frame.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

using CurveGrowFn = bool (*)(void* user, ImVec2** data, int* capacity, int count);

// Pixels per sample along a smooth segment: smooth enough at any scale, few enough points.
constexpr float kCurveSamplePixels = 2.0f;

// What the last frame laid out, for tests and demo probes (CurveEditorPointPos).
struct CurveEditorState
{
    ImRect area;
    internal::ValueMapping x;
    internal::ValueMapping y;
};

ImGuiID CurvePointId(ImGuiID widgetId, int index)
{
    return ImHashData(&index, sizeof(index), widgetId);
}

// The slope of the monotone cubic at point i (Fritsch and Butland's weighted harmonic mean of the
// neighbouring secants): no overshoot between points, and local, so a segment needs only its four
// nearest points.
double CurveTangent(const ImVec2* p, int n, int i)
{
    const auto secant = [&](int k) {
        const double dx = static_cast<double>(p[k + 1].x) - p[k].x;
        return dx > 0.0 ? (static_cast<double>(p[k + 1].y) - p[k].y) / dx : 0.0;
    };
    if (n < 2)
        return 0.0;
    if (i == 0)
        return secant(0);
    if (i == n - 1)
        return secant(n - 2);
    const double d0 = secant(i - 1);
    const double d1 = secant(i);
    if (d0 * d1 <= 0.0)
        return 0.0;
    const double h0 = static_cast<double>(p[i].x) - p[i - 1].x;
    const double h1 = static_cast<double>(p[i + 1].x) - p[i].x;
    if (h0 <= 0.0 || h1 <= 0.0)
        return 0.0;
    return 3.0 * (h0 + h1) / ((2.0 * h1 + h0) / d0 + (h1 + 2.0 * h0) / d1);
}

// The curve's y at x on segment k (between points k and k + 1).
double CurveSegmentValue(const ImVec2* p, int n, int k, double x, CurveInterpolation interpolation)
{
    const ImVec2 a = p[k];
    const ImVec2 b = p[k + 1];
    const double h = static_cast<double>(b.x) - a.x;
    if (interpolation == CurveInterpolation::Step || h <= 0.0)
        return x < b.x || h <= 0.0 ? a.y : b.y;
    const double t = std::clamp((x - a.x) / h, 0.0, 1.0);
    if (interpolation == CurveInterpolation::Linear)
        return a.y + (static_cast<double>(b.y) - a.y) * t;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;
    return h00 * a.y + h10 * h * CurveTangent(p, n, k) + h01 * b.y + h11 * h * CurveTangent(p, n, k + 1);
}

// The curve's y at x: held flat before the first and after the last point.
double CurveValueAt(const ImVec2* p, int n, double x, CurveInterpolation interpolation)
{
    if (n <= 0)
        return 0.0;
    if (x <= p[0].x || n == 1)
        return p[0].y;
    for (int k = 0; k + 1 < n; ++k)
        if (x <= p[k + 1].x)
            return CurveSegmentValue(p, n, k, x, interpolation);
    return p[n - 1].y;
}

void DrawCurve(ImDrawList* drawList, const ImVec2* p, int n, const internal::Scale& sx, const internal::Scale& sy,
               CurveInterpolation interpolation, ImU32 color, const ImRect& clip)
{
    if (n <= 0)
        return;
    internal::CurvePolyline line(drawList, color, clip);
    const auto at = [&](double x, double y) { return ImVec2(sx.ToPixel(x), sy.ToPixel(y)); };
    // Flat out to the left edge from the first point.
    line.Add(at(std::min(sx.domainMin, static_cast<double>(p[0].x)), p[0].y));
    for (int k = 0; k + 1 < n; ++k)
    {
        const ImVec2 a = p[k];
        const ImVec2 b = p[k + 1];
        line.Add(at(a.x, a.y));
        if (interpolation == CurveInterpolation::Step)
        {
            line.Add(at(b.x, a.y));
        }
        else if (interpolation == CurveInterpolation::Smooth)
        {
            const float pixels = std::fabs(sx.ToPixel(b.x) - sx.ToPixel(a.x));
            const int samples = std::clamp(static_cast<int>(pixels / kCurveSamplePixels), 1, 512);
            for (int s = 1; s < samples; ++s)
            {
                const double x = a.x + (static_cast<double>(b.x) - a.x) * s / samples;
                line.Add(at(x, CurveSegmentValue(p, n, k, x, interpolation)));
            }
        }
    }
    line.Add(at(p[n - 1].x, p[n - 1].y));
    line.Add(at(std::max(sx.domainMax, static_cast<double>(p[n - 1].x)), p[n - 1].y));
    line.Finish();
}

bool CurveEditorImpl(const char* label, ImVec2* data, int capacity, int* count, CurveGrowFn grow, void* growUser,
                     ImVec2 min, ImVec2 max, const CurveEditorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const Colors& c = t.colors;
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();

    internal::ValueMapping mx;
    mx.min = internal::SliderFloatToDouble(min.x);
    mx.max = internal::SliderFloatToDouble(max.x);
    mx = internal::NormalizeMapping(mx);
    internal::ValueMapping my;
    my.min = internal::SliderFloatToDouble(min.y);
    my.max = internal::SliderFloatToDouble(max.y);
    my = internal::NormalizeMapping(my);
    // An empty range on an axis shows the points read-only (rule 14).
    const bool readOnly = options.readOnly || data == nullptr || count == nullptr || !(mx.max > mx.min) ||
                          !(my.max > my.min) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    int n = data != nullptr && count != nullptr ? std::clamp(*count, 0, capacity) : 0;

    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float height = options.height > 0.0f ? Px(options.height) : Px(t.metrics.chartHeight);
    const internal::CurveCanvas canvas = internal::LayoutCurveCanvas(window->DC.CursorPos, width, height);
    const internal::Scale sx =
        internal::MakeScale(internal::ScaleKind::Linear, mx.min, mx.max, canvas.area.Min.x, canvas.area.Max.x);
    const internal::Scale sy =
        internal::MakeScale(internal::ScaleKind::Linear, my.min, my.max, canvas.area.Max.y, canvas.area.Min.y);
    CurveEditorState& state = internal::GetState<CurveEditorState>(id);
    state.area = canvas.area;
    state.x = mx;
    state.y = my;

    ImDrawList* drawList = window->DrawList;
    const bool visible = ImGui::IsRectVisible(canvas.frame.Min, canvas.frame.Max);
    // The points go on a channel above the frame and the curve, which are drawn after them. A splitter of
    // its own, not the draw list's channels, so the editor also works where a table or another widget has
    // split the draw list; one editor draws at a time, so one splitter serves all and allocates once.
    static ImDrawListSplitter splitter;
    if (visible)
    {
        splitter.Split(drawList, 2);
        splitter.SetCurrentChannel(drawList, 1);
    }

    bool written = false;
    ImGui::BeginGroup();
    ImGui::ItemSize(canvas.frame.GetSize());
    internal::ApplyNextItemData(next);
    int removeIndex = -1;
    int shownIndex = -1;  // the point whose values the readout shows
    ImVec2 shownValue;
    bool anyPointHovered = false;
    bool focusVisible = false;
    const float hit = Px(t.metrics.minHitTarget);
    for (int i = 0; i < n; ++i)
    {
        const ImVec2 point = internal::PadPoint(canvas.area, mx, my, false, data[i].x, data[i].y);
        internal::ItemOptions itemOptions;
        itemOptions.press = internal::PressMode::Click;
        itemOptions.readOnly = readOnly;
        itemOptions.forced = &forced;
        itemOptions.part = i;
        const ImGuiID pointId = CurvePointId(id, i);
        const internal::ItemState item = internal::PartBehavior(pointId, internal::CurvePointRect(point, canvas.handle), itemOptions);
        const bool lockedEnd = options.lockEnds && (i == 0 || i == n - 1);
        double x = internal::SliderFloatToDouble(data[i].x);
        double y = internal::SliderFloatToDouble(data[i].y);
        internal::ValueDrag2DOptions drag;
        drag.mappingX = mx;
        drag.mappingY = my;
        drag.grabRadius = hit * 0.5f;
        drag.moveX = !lockedEnd;
        drag.readOnly = readOnly;
        // The neighbours are the limits: a point never passes them.
        if (i > 0)
            drag.clampLowX = std::min(x, internal::SliderFloatToDouble(data[i - 1].x));
        if (i + 1 < n)
            drag.clampHighX = std::max(x, internal::SliderFloatToDouble(data[i + 1].x));
        const internal::ValueDrag2DResult edit = internal::ValueDrag2DBehavior(item, &x, &y, canvas.area, drag);
        if (edit.changed && !readOnly)
        {
            const ImVec2 value(static_cast<float>(x), static_cast<float>(y));
            if (value.x != data[i].x || value.y != data[i].y)
            {
                data[i] = value;
                written = true;
            }
        }
        // Only a real pointer counts for the tooltip: a hover forced by a capture has no pointer to follow.
        anyPointHovered = anyPointHovered || (item.hovered && item.forced == internal::ForcedItemState::None);
        focusVisible = focusVisible || item.focusVisible;
        if (item.active || item.focusVisible || (shownIndex < 0 && item.forced != internal::ForcedItemState::None))
        {
            shownIndex = i;
            shownValue = data[i];
        }
        const bool real = item.forced == internal::ForcedItemState::None && !item.disabled && !readOnly;
        if (real && item.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !lockedEnd)
            removeIndex = i;
        if (real && item.focused && g.NavId == pointId && !lockedEnd)
        {
            ImGui::SetKeyOwner(ImGuiKey_Delete, pointId);
            ImGui::SetKeyOwner(ImGuiKey_Backspace, pointId);
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, ImGuiInputFlags_None, pointId) ||
                ImGui::IsKeyPressed(ImGuiKey_Backspace, ImGuiInputFlags_None, pointId))
                removeIndex = i;
        }
        if (visible)
            internal::DrawCurvePoint(drawList, internal::PadPoint(canvas.area, mx, my, false, data[i].x, data[i].y),
                                     canvas.handle, item, readOnly);
    }

    // The canvas behind the points: hover look, double click to add, the tooltip.
    internal::ItemOptions canvasOptions;
    canvasOptions.navigable = false;
    canvasOptions.hitTarget = false;
    canvasOptions.handCursor = false;
    canvasOptions.readOnly = readOnly;
    canvasOptions.forced = &forced;
    canvasOptions.part = -1;
    const internal::ItemState canvasItem = internal::PartBehavior(ImHashStr("##canvas", 0, id), canvas.frame, canvasOptions);
    const bool disabled = canvasItem.disabled;
    if (removeIndex >= 0 && n > 2)
    {
        for (int i = removeIndex; i + 1 < n; ++i)
            data[i] = data[i + 1];
        --n;
        written = true;
        // The press of the double click must not go on as a drag of the point that took the index.
        if (g.ActiveId == CurvePointId(id, removeIndex))
            ImGui::ClearActiveID();
        ImGui::SetFocusID(CurvePointId(id, std::min(removeIndex, n - 1)), window);
    }
    else if (canvasItem.hovered && !anyPointHovered && !readOnly && !disabled &&
             canvasItem.forced == internal::ForcedItemState::None && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        if (n >= capacity && grow != nullptr)
            grow(growUser, &data, &capacity, n);
        if (n < capacity && data != nullptr)
        {
            double fx = 0.0;
            double fy = 0.0;
            internal::PadFractions(canvas.area, false, g.IO.MousePos, &fx, &fy);
            const ImVec2 value(static_cast<float>(internal::FractionToValue(mx, fx)),
                               static_cast<float>(internal::FractionToValue(my, fy)));
            // Between the points around it; with locked ends, never outside them.
            int at = 0;
            while (at < n && data[at].x <= value.x)
                ++at;
            if (options.lockEnds && n >= 2)
                at = std::clamp(at, 1, n - 1);
            for (int i = n; i > at; --i)
                data[i] = data[i - 1];
            data[at] = value;
            ++n;
            written = true;
            ImGui::SetFocusID(CurvePointId(id, at), window);
        }
    }

    // ---- Draw: the frame, the grid, the curve and the marker under the points ----
    if (visible)
    {
        splitter.SetCurrentChannel(drawList, 0);
        internal::DrawCurveCanvas(drawList, canvas, sx, sy, options.grid, canvasItem, readOnly);
        drawList->PushClipRect(canvas.frame.Min, canvas.frame.Max, true);
        DrawCurve(drawList, data, n, sx, sy, options.interpolation, internal::CurveLineColor(disabled), canvas.frame);
        if (!std::isnan(options.marker) && n > 0)
        {
            // The live marker: a guide through the current input and a dot where it meets the curve.
            const double x = std::clamp(static_cast<double>(options.marker), mx.min, mx.max);
            const float px = std::round(sx.ToPixel(x));
            HairlineV(drawList, px, canvas.area.Min.y, canvas.area.Max.y,
                      ToU32(disabled ? c.labelQuaternary : c.labelTertiary));
            const double y = std::clamp(CurveValueAt(data, n, x, options.interpolation), my.min, my.max);
            const float radius = Px(t.charts.markerSize) * 0.5f;
            drawList->AddCircleFilled(ImVec2(px, sy.ToPixel(y)), radius, internal::CurveLineColor(disabled));
        }
        if (shownIndex >= 0)
        {
            internal::ControlTextScope footnote(TextStyle::Footnote);
            char text[64];
            // Three significant digits: enough to read a value off, short enough for a narrow canvas.
            ImFormatString(text, sizeof(text), "%.3g, %.3g", static_cast<double>(shownValue.x), static_cast<double>(shownValue.y));
            const float inset = PxSnapped(t.spacing.xs);
            drawList->AddText(ImVec2(canvas.frame.Min.x + inset * 2.0f, canvas.frame.Min.y + inset),
                              ToU32(internal::ControlSecondaryColor(disabled)), text);
        }
        drawList->PopClipRect();
        splitter.Merge(drawList);
    }

    // The label is not drawn beside the editor: it is the tooltip on hover and keyboard focus.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool canvasHovered = canvasItem.hovered && canvasItem.forced == internal::ForcedItemState::None;
    internal::CutTextTooltip(id, canvas.frame, canvasHovered || anyPointHovered, focusVisible, label, labelEnd,
                             labelEnd != label);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (written)
    {
        internal::MarkWidgetEdited(id);
        *count = n;
    }
    return written;
}

} // namespace

namespace internal
{

ImVec2 CurveEditorPointPos(ImGuiID id, ImVec2 value)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return ImVec2(0.0f, 0.0f);
    const CurveEditorState& state = GetState<CurveEditorState>(id);
    return PadPoint(state.area, state.x, state.y, false, value.x, value.y);
}

ImGuiID CurveEditorPointId(ImGuiID id, int index)
{
    return CurvePointId(id, index);
}

} // namespace internal

bool CurveEditor(const char* label, std::span<ImVec2> points, int* count, ImVec2 min, ImVec2 max,
                 const CurveEditorOptions& options)
{
    return CurveEditorImpl(label, points.data(), internal::EditorsCapacity(points.size()), count, nullptr, nullptr, min,
                           max, options);
}

bool CurveEditor(const char* label, std::vector<ImVec2>* points, ImVec2 min, ImVec2 max, const CurveEditorOptions& options)
{
    if (points == nullptr)
        return CurveEditorImpl(label, nullptr, 0, nullptr, nullptr, nullptr, min, max, options);
    int count = internal::EditorsCapacity(points->size());
    points->resize(points->capacity());  // within the capacity: no allocation
    const bool written = CurveEditorImpl(label, points->data(), internal::EditorsCapacity(points->size()), &count,
                                         &internal::EditorsGrowVector<ImVec2>, points, min, max, options);
    points->resize(static_cast<std::size_t>(std::max(0, count)));
    return written;
}

} // namespace facet
