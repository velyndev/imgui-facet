// BezierEditor (facet/widgets/composites.h): an easing curve from (0, 0) to (1, 1) given by its two
// control points.
//
// The canvas is the one of the curve editor (curve_parts.h): a field frame with the grid of the
// chart core's ticks, the curve in the first series color, a hairline from each end to its control
// point, and the two control points as items and Tab stops on the two-axis value drag, x kept in 0
// to 1. Without allowOvershoot y stays in 0 to 1 as well; with it the canvas shows y from
// -kBezierOvershoot to 1 + kBezierOvershoot, with hairlines at 0 and 1, and the points may go
// anywhere in it. Under the canvas: a Dropdown of the common easings (Strings::easeLinear, ease,
// easeIn, easeOut, easeInOut; Strings::presets while the curve is none of them) and the four numbers
// as two VectorInput fields, the first control point and the second. After every edit a dot runs the
// curve once (composites.easingPreviewTime times motion.scale), unless motion is reduced.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/lists.h"
#include "internal/chart_core.h"
#include "internal/control.h"
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

// How far beyond 0 and 1 the canvas shows y when overshoot is allowed: room for a back-out or
// elastic easing without squeezing the 0 to 1 part into a sliver.
constexpr double kBezierOvershoot = 0.5;
// Samples along the curve: smooth at any canvas size an easing editor gets.
constexpr int kBezierSamples = 64;
// Bisection steps when solving x(t) = x for the preview dot: far below a pixel.
constexpr int kBezierSolveSteps = 24;
constexpr int kBezierPresetCount = 5;

struct BezierEditorState
{
    double previewStart = -1.0;  // ImGui time of the last edit; < 0: no preview running
    ImRect area;                 // the canvas area and the y range of the last frame, for probes
    double yMin = 0.0;
    double yMax = 1.0;
};

const CubicBezier kBezierPresets[kBezierPresetCount] = {
    {{0.0f, 0.0f}, {1.0f, 1.0f}},       // linear
    {{0.25f, 0.1f}, {0.25f, 1.0f}},     // ease
    {{0.42f, 0.0f}, {1.0f, 1.0f}},      // ease in
    {{0.0f, 0.0f}, {0.58f, 1.0f}},      // ease out
    {{0.42f, 0.0f}, {0.58f, 1.0f}},     // ease in and out
};

ImGuiID BezierPointId(ImGuiID widgetId, int index)
{
    return ImHashData(&index, sizeof(index), ImHashStr("##point", 0, widgetId));
}

// One coordinate of the curve at parameter t, for control values a (P1) and b (P2).
double BezierAxis(double a, double b, double t)
{
    const double u = 1.0 - t;
    return 3.0 * u * u * t * a + 3.0 * u * t * t * b + t * t * t;
}

// The y of the curve where its x is `x` (x(t) grows with t while both x lie in 0 to 1).
double BezierValueAt(const CubicBezier& c, double x)
{
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < kBezierSolveSteps; ++i)
    {
        const double mid = (lo + hi) * 0.5;
        if (BezierAxis(c.p1.x, c.p2.x, mid) < x)
            lo = mid;
        else
            hi = mid;
    }
    return BezierAxis(c.p1.y, c.p2.y, (lo + hi) * 0.5);
}

int BezierPresetIndex(const CubicBezier& c)
{
    for (int i = 0; i < kBezierPresetCount; ++i)
    {
        const CubicBezier& p = kBezierPresets[i];
        if (p.p1.x == c.p1.x && p.p1.y == c.p1.y && p.p2.x == c.p2.x && p.p2.y == c.p2.y)
            return i;
    }
    return -1;
}

} // namespace

bool BezierEditor(const char* label, CubicBezier* curve, const BezierEditorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& strings = GetStrings();
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const bool readOnly = options.readOnly || curve == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    CubicBezier value = curve != nullptr ? *curve : kBezierPresets[0];
    BezierEditorState& state = internal::GetState<BezierEditorState>(id);

    const double yMin = options.allowOvershoot ? -kBezierOvershoot : 0.0;
    const double yMax = options.allowOvershoot ? 1.0 + kBezierOvershoot : 1.0;
    internal::ValueMapping mx;
    internal::ValueMapping my;
    my.min = yMin;
    my.max = yMax;

    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float height = options.height > 0.0f ? Px(options.height) : Px(t.metrics.chartHeight);
    const internal::CurveCanvas canvas = internal::LayoutCurveCanvas(window->DC.CursorPos, width, height);
    const internal::Scale sx = internal::MakeScale(internal::ScaleKind::Linear, 0.0, 1.0, canvas.area.Min.x, canvas.area.Max.x);
    const internal::Scale sy = internal::MakeScale(internal::ScaleKind::Linear, yMin, yMax, canvas.area.Max.y, canvas.area.Min.y);
    state.area = canvas.area;
    state.yMin = yMin;
    state.yMax = yMax;
    const auto at = [&](double x, double y) { return ImVec2(sx.ToPixel(x), sy.ToPixel(std::clamp(y, yMin, yMax))); };

    bool written = false;
    ImGui::BeginGroup();
    ImGui::ItemSize(canvas.frame.GetSize());
    internal::ApplyNextItemData(next);

    // The two control points.
    internal::ItemState points[2];
    ImVec2* controls[2] = {&value.p1, &value.p2};
    const float hit = Px(t.metrics.minHitTarget);
    for (int i = 0; i < 2; ++i)
    {
        internal::ItemOptions itemOptions;
        itemOptions.press = internal::PressMode::Click;
        itemOptions.readOnly = readOnly;
        itemOptions.forced = &forced;
        itemOptions.part = i;
        points[i] = internal::PartBehavior(BezierPointId(id, i), internal::CurvePointRect(at(controls[i]->x, controls[i]->y), canvas.handle),
                                           itemOptions);
        double x = internal::SliderFloatToDouble(controls[i]->x);
        double y = internal::SliderFloatToDouble(controls[i]->y);
        internal::ValueDrag2DOptions drag;
        drag.mappingX = mx;
        drag.mappingY = my;
        drag.grabRadius = hit * 0.5f;
        drag.readOnly = readOnly;
        const internal::ValueDrag2DResult edit = internal::ValueDrag2DBehavior(points[i], &x, &y, canvas.area, drag);
        if (edit.changed && !readOnly)
        {
            const ImVec2 moved(static_cast<float>(x), static_cast<float>(y));
            if (moved.x != controls[i]->x || moved.y != controls[i]->y)
            {
                *controls[i] = moved;
                written = true;
            }
        }
    }
    internal::ItemOptions canvasOptions;
    canvasOptions.navigable = false;
    canvasOptions.hitTarget = false;
    canvasOptions.handCursor = false;
    canvasOptions.readOnly = readOnly;
    canvasOptions.forced = &forced;
    canvasOptions.part = -1;
    const internal::ItemState canvasItem = internal::PartBehavior(ImHashStr("##canvas", 0, id), canvas.frame, canvasOptions);
    const bool disabled = canvasItem.disabled;

    // The presets and the numbers under the canvas, with ids under the editor's.
    ImGui::PushOverrideID(id);
    if (options.presets)
    {
        const char* names[kBezierPresetCount] = {strings.easeLinear, strings.ease, strings.easeIn, strings.easeOut,
                                                 strings.easeInOut};
        int preset = BezierPresetIndex(value);
        ImGui::SetNextItemWidth(width);
        if (Dropdown("##presets", &preset, std::span<const char* const>(names),
                     {.placeholder = strings.presets, .readOnly = readOnly}) &&
            !readOnly && preset >= 0 && preset < kBezierPresetCount)
        {
            value = kBezierPresets[preset];
            written = true;
        }
    }
    if (options.numericFields)
    {
        // The first and the second control point, side by side while each keeps room for its two
        // fields, else one under the other.
        // Each pair is labelled in front with its Strings name (control.h), so the label column
        // counts in the room a pair needs.
        const float gap = Px(t.spacing.sm);
        const bool sideBySide = width >= Px(t.layout.labelColumn + t.layout.minRowControl) * 2.0f + gap;
        const float fieldWidth = sideBySide ? std::floor((width - gap) * 0.5f) : width;
        for (int i = 0; i < 2; ++i)
        {
            char fieldLabel[96];
            ImFormatString(fieldLabel, sizeof(fieldLabel), "%s###p%d",
                           i == 0 ? strings.controlPoint1 : strings.controlPoint2, i + 1);
            if (i == 1 && sideBySide)
                ImGui::SameLine(0.0f, gap);
            ImVec2 edited = *controls[i];
            ImGui::SetNextItemWidth(fieldWidth);
            VectorInputOptions fields;
            fields.format = "%.2f";
            fields.min = options.allowOvershoot ? yMin : 0.0;
            fields.max = options.allowOvershoot ? yMax : 1.0;
            fields.speed = 0.01;
            fields.readOnly = readOnly;
            if (VectorInput(fieldLabel, &edited, fields) && !readOnly)
            {
                edited.x = std::clamp(edited.x, 0.0f, 1.0f);
                edited.y = std::clamp(edited.y, static_cast<float>(yMin), static_cast<float>(yMax));
                *controls[i] = edited;
                written = true;
            }
        }
    }
    ImGui::PopID();

    // The preview runs once after each edit.
    const double now = ImGui::GetTime();
    if (written)
        state.previewStart = options.preview && !ReduceMotion() ? now : -1.0;
    const double duration = std::max(0.0, static_cast<double>(t.composites.easingPreviewTime * t.motion.scale));
    double progress = -1.0;
    if (options.preview && state.previewStart >= 0.0 && !ReduceMotion() && duration > 0.0)
    {
        progress = (now - state.previewStart) / duration;
        if (progress > 1.0)
        {
            progress = -1.0;
            state.previewStart = -1.0;
        }
    }

    // ---- Draw ----
    ImDrawList* drawList = window->DrawList;
    if (ImGui::IsRectVisible(canvas.frame.Min, canvas.frame.Max))
    {
        internal::DrawCurveCanvas(drawList, canvas, sx, sy, true, canvasItem, readOnly);
        drawList->PushClipRect(canvas.frame.Min, canvas.frame.Max, true);
        const ImU32 guide = ToU32(disabled ? c.labelQuaternary : c.labelTertiary);
        if (options.allowOvershoot)
        {
            // Where the easing starts and ends: the 0 and 1 lines.
            HairlineH(drawList, canvas.area.Min.x, canvas.area.Max.x, std::round(sy.ToPixel(0.0)), guide);
            HairlineH(drawList, canvas.area.Min.x, canvas.area.Max.x, std::round(sy.ToPixel(1.0)), guide);
        }
        // The arms from each end to its control point.
        const ImU32 arm = ToU32(internal::ControlSecondaryColor(disabled));
        const float hairline = HairlineThickness();
        drawList->AddLine(at(0.0, 0.0), at(value.p1.x, value.p1.y), arm, hairline);
        drawList->AddLine(at(1.0, 1.0), at(value.p2.x, value.p2.y), arm, hairline);
        internal::CurvePolyline line(drawList, internal::CurveLineColor(disabled), canvas.frame);
        for (int s = 0; s <= kBezierSamples; ++s)
        {
            const double u = static_cast<double>(s) / kBezierSamples;
            line.Add(ImVec2(sx.ToPixel(BezierAxis(value.p1.x, value.p2.x, u)), sy.ToPixel(BezierAxis(value.p1.y, value.p2.y, u))));
        }
        line.Finish();
        // The fixed ends.
        const float end = Px(t.charts.markerSize) * 0.5f;
        drawList->AddCircleFilled(at(0.0, 0.0), end, arm);
        drawList->AddCircleFilled(at(1.0, 1.0), end, arm);
        if (progress >= 0.0)
        {
            const double x = std::clamp(progress, 0.0, 1.0);
            drawList->AddCircleFilled(ImVec2(sx.ToPixel(x), sy.ToPixel(BezierValueAt(value, x))), end,
                                      internal::CurveLineColor(disabled));
        }
        drawList->PopClipRect();
        for (int i = 0; i < 2; ++i)
            internal::DrawCurvePoint(drawList, at(controls[i]->x, controls[i]->y), canvas.handle, points[i], readOnly);
    }

    // The label is not drawn beside the editor: it is the tooltip on hover and keyboard focus.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hovered = (canvasItem.hovered && canvasItem.forced == internal::ForcedItemState::None) ||
                         ((points[0].hovered || points[1].hovered) && points[0].forced == internal::ForcedItemState::None &&
                          points[1].forced == internal::ForcedItemState::None);
    internal::CutTextTooltip(id, canvas.frame, hovered, points[0].focusVisible || points[1].focusVisible, label, labelEnd,
                             labelEnd != label);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (written)
    {
        internal::MarkWidgetEdited(id);
        *curve = value;
    }
    return written;
}

namespace internal
{

ImVec2 BezierEditorPointPos(ImGuiID id, ImVec2 value)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return ImVec2(0.0f, 0.0f);
    const BezierEditorState& state = GetState<BezierEditorState>(id);
    ValueMapping my;
    my.min = state.yMin;
    my.max = state.yMax;
    return PadPoint(state.area, ValueMapping{}, my, false, value.x, value.y);
}

ImGuiID BezierEditorPointId(ImGuiID id, int index)
{
    return BezierPointId(id, index);
}

bool BezierEditorPreviewRunning(ImGuiID id)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    return GetState<BezierEditorState>(id).previewStart >= 0.0;
}

} // namespace internal

} // namespace facet
