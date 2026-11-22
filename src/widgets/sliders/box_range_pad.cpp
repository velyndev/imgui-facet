// BoxRangePad: a two-dimensional range, the box from *low to *high inside [min, max] per axis,
// on a pad (pad_core.h). Eight handles, each its own item and Tab stop under the widget id: the
// left, top, right and bottom edges (parts 0 to 3) and the corners from the top left clockwise
// (parts 4 to 7), plus the body (part 8), which drags the whole box.
//
// Each axis is a pair of handles of the multi-handle model (src/internal/multi_handle.h): the
// handles keep their order and the minimum gap, and `collision` clamps or pushes, per axis. An
// edge moves one axis, a corner both, through the two-axis value drag (value_drag.h). A pad
// item under the parts takes every mouse press and hands it to the part it picks: a corner, an
// edge, the body, or else the nearest handle, which first moves under the pointer.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/sliders/pad_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

constexpr int kBoxRangeHandles = 8;
constexpr int kBoxRangeBodyPart = 8;
// Animation sub key of the hover amount of a part: the pad item takes the pointer for all of them.
constexpr int kBoxRangeHoverSub = 0x0C00;
// An edge handle is drawn on an edge at least this many knobs long; a smaller box shows its
// corners alone, so the knobs do not pile up (a focused or held edge is always drawn).
constexpr float kBoxRangeEdgeRoom = 3.0f;

// The axes a part moves: the index in the x pair and in the y pair (0 low, 1 high), -1 none.
struct BoxRangeAxes
{
    int x = -1;
    int y = -1;
};

// Parts 0 to 3 are the left, top, right and bottom edges, 4 to 7 the corners from the top left
// clockwise. The top is max.y, or min.y with yDown.
BoxRangeAxes BoxRangePartAxes(int part, bool yDown)
{
    const int top = yDown ? 0 : 1;
    const int bottom = 1 - top;
    switch (part)
    {
    case 0: return {0, -1};
    case 1: return {-1, top};
    case 2: return {1, -1};
    case 3: return {-1, bottom};
    case 4: return {0, top};
    case 5: return {1, top};
    case 6: return {1, bottom};
    default: return {0, bottom};
    }
}

// The values when an edit of a part began, for Escape.
struct BoxRangeSnapshot
{
    double x[2] = {0.0, 0.0};
    double y[2] = {0.0, 0.0};
    bool set = false;
};

// The distance from `p` to the segment a to b.
float BoxRangeSegmentDistance(ImVec2 p, ImVec2 a, ImVec2 b)
{
    const ImVec2 d(b.x - a.x, b.y - a.y);
    const float lengthSq = d.x * d.x + d.y * d.y;
    float t = lengthSq > 0.0f ? ((p.x - a.x) * d.x + (p.y - a.y) * d.y) / lengthSq : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float dx = p.x - (a.x + d.x * t);
    const float dy = p.y - (a.y + d.y * t);
    return std::sqrt(dx * dx + dy * dy);
}

internal::HandleConstraints BoxRangeConstraints(const internal::ValueMapping& mapping, float gap, HandleCollision collision)
{
    internal::HandleConstraints c;
    c.min = mapping.min;
    c.max = mapping.max;
    c.minGap = std::isnan(gap) ? 0.0 : std::max(0.0, static_cast<double>(gap));
    c.collision = collision == HandleCollision::Push ? internal::CollisionMode::Push : internal::CollisionMode::Clamp;
    return c;
}

} // namespace

bool BoxRangePad(const char* label, ImVec2* low, ImVec2* high, ImVec2 min, ImVec2 max, const BoxRangePadOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
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
    const bool moveX = mx.max > mx.min;
    const bool moveY = my.max > my.min;
    const bool missing = low == nullptr || high == nullptr;
    const bool readOnly = o.readOnly || missing || !(moveX || moveY) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabledFlag = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::HandleConstraints cx = BoxRangeConstraints(mx, o.minGap.x, o.collision);
    const internal::HandleConstraints cy = BoxRangeConstraints(my, o.minGap.y, o.collision);

    // What the caller passed, as drawn: repaired, written only for the values an edit moves.
    double drawnX[2] = {low != nullptr ? internal::SliderValueIn(low->x) : mx.min,
                        high != nullptr ? internal::SliderValueIn(high->x) : mx.max};
    double drawnY[2] = {low != nullptr ? internal::SliderValueIn(low->y) : my.min,
                        high != nullptr ? internal::SliderValueIn(high->y) : my.max};
    for (int i = 0; i < 2; ++i)
    {
        if (std::isnan(drawnX[i]))
            drawnX[i] = i == 0 ? mx.min : mx.max;
        if (std::isnan(drawnY[i]))
            drawnY[i] = i == 0 ? my.min : my.max;
    }
    internal::ConstrainHandles(std::span<double>(drawnX, 2), cx);
    internal::ConstrainHandles(std::span<double>(drawnY, 2), cy);
    double vx[2] = {drawnX[0], drawnX[1]};
    double vy[2] = {drawnY[0], drawnY[1]};

    const float width = internal::ResolvePadWidth(o.width, next);
    const float height = o.height > 0.0f ? std::max(1.0f, std::round(Px(o.height))) : width;
    const internal::PadFrame pad = internal::LayoutPadFrame(window->DC.CursorPos, width, height, m);
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();

    ImGui::BeginGroup();
    ImGui::ItemSize(pad.frame.GetSize());
    ImGui::PushOverrideID(id);
    ImGuiID handleIds[kBoxRangeHandles];
    for (int i = 0; i < kBoxRangeHandles; ++i)
        handleIds[i] = ImGui::GetID(i);
    const ImGuiID bodyId = ImGui::GetID("##body");
    const ImGuiID padId = ImGui::GetID("##pad");
    ImGui::PopID();

    // Screen positions of the box and of each handle's centre, from the values at the frame's start.
    const auto boxRect = [&](const double* x, const double* y) {
        const ImVec2 a = internal::PadPoint(pad.area, mx, my, o.yDown, x[0], y[0]);
        const ImVec2 b = internal::PadPoint(pad.area, mx, my, o.yDown, x[1], y[1]);
        return ImRect(std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y));
    };
    const auto handleCenter = [](const ImRect& box, int part) {
        const ImVec2 mid = box.GetCenter();
        switch (part)
        {
        case 0: return ImVec2(box.Min.x, mid.y);
        case 1: return ImVec2(mid.x, box.Min.y);
        case 2: return ImVec2(box.Max.x, mid.y);
        case 3: return ImVec2(mid.x, box.Max.y);
        case 4: return box.Min;
        case 5: return ImVec2(box.Max.x, box.Min.y);
        case 6: return box.Max;
        default: return ImVec2(box.Min.x, box.Max.y);
        }
    };
    const ImRect box = boxRect(vx, vy);

    // The pad takes every press and hands it to the part it picks.
    internal::ItemOptions padOptions;
    padOptions.press = internal::PressMode::Click;
    padOptions.navigable = false;
    padOptions.part = -2;
    padOptions.forced = &forced;
    padOptions.readOnly = readOnly;
    const internal::ItemState padItem = internal::PartBehavior(padId, pad.frame, padOptions);
    const bool disabled = padItem.disabled || disabledFlag;
    const float grab = std::max(pad.knob, Px(t.metrics.minHitTarget)) * 0.5f;
    int pick = -1;
    bool pickBody = false;
    if (padItem.hovered || padItem.pressed)
    {
        const ImVec2 mouse = g.IO.MousePos;
        // A corner first, then an edge, then the body, then the nearest handle.
        float best = grab;
        for (int i = 4; i < kBoxRangeHandles; ++i)
        {
            const ImVec2 p = handleCenter(box, i);
            const float d = std::sqrt((mouse.x - p.x) * (mouse.x - p.x) + (mouse.y - p.y) * (mouse.y - p.y));
            if (d <= best)
            {
                best = d;
                pick = i;
            }
        }
        if (pick < 0)
        {
            best = grab;
            for (int i = 0; i < 4; ++i)
            {
                const float d = BoxRangeSegmentDistance(mouse, handleCenter(box, 4 + (i == 0 ? 3 : i - 1)),
                                                        handleCenter(box, 4 + i));
                if (d <= best)
                {
                    best = d;
                    pick = i;
                }
            }
        }
        if (pick < 0 && o.dragSpan && box.Contains(mouse))
            pickBody = true;
        if (pick < 0 && !pickBody)
        {
            best = FLT_MAX;
            for (int i = 0; i < kBoxRangeHandles; ++i)
            {
                const float d = i < 4 ? BoxRangeSegmentDistance(mouse, handleCenter(box, 4 + (i == 0 ? 3 : i - 1)),
                                                                handleCenter(box, 4 + i))
                                      : BoxRangeSegmentDistance(mouse, handleCenter(box, i), handleCenter(box, i));
                if (d < best)
                {
                    best = d;
                    pick = i;
                }
            }
        }
    }
    BoxRangeSnapshot& snapshot = internal::GetState<BoxRangeSnapshot>(id);
    const auto takeSnapshot = [&]() {
        snapshot.x[0] = drawnX[0];
        snapshot.x[1] = drawnX[1];
        snapshot.y[0] = drawnY[0];
        snapshot.y[1] = drawnY[1];
        snapshot.set = true;
    };
    const auto restoreSnapshot = [&]() {
        if (!snapshot.set)
            return;
        vx[0] = snapshot.x[0];
        vx[1] = snapshot.x[1];
        vy[0] = snapshot.y[0];
        vy[1] = snapshot.y[1];
    };
    if (padItem.pressed && !disabled && !readOnly && padItem.forced == internal::ForcedItemState::None)
    {
        takeSnapshot();
        if (pickBody)
            internal::HandOverMousePress(bodyId, window, false);
        else if (pick >= 0)
            internal::HandOverMousePress(handleIds[pick], window, true);
    }
    bool partActive = g.ActiveId == bodyId;
    for (const ImGuiID handleId : handleIds)
        partActive = partActive || g.ActiveId == handleId;
    const int hoveredPart = padItem.hovered && !partActive ? (pickBody ? kBoxRangeBodyPart : pick) : -1;

    // The handles.
    internal::ItemState handles[kBoxRangeHandles];
    int activeHandle = -1;
    int focusedHandle = -1;
    for (int i = 0; i < kBoxRangeHandles; ++i)
    {
        const BoxRangeAxes axes = BoxRangePartAxes(i, o.yDown);
        internal::ItemOptions handleOptions;
        handleOptions.press = internal::PressMode::Click;
        handleOptions.hitTarget = false;
        handleOptions.part = i;
        handleOptions.forced = &forced;
        handleOptions.readOnly = readOnly;
        const bool wasActive = g.ActiveId == handleIds[i];
        internal::ItemState& handle = handles[i];
        handle = internal::PartBehavior(handleIds[i], internal::PadKnobRect(handleCenter(box, i), pad.knob), handleOptions);
        internal::ValueDrag2DOptions drag;
        drag.mappingX = mx;
        drag.mappingY = my;
        drag.yDown = o.yDown;
        drag.grabRadius = grab;
        drag.moveX = axes.x >= 0 && moveX;
        drag.moveY = axes.y >= 0 && moveY;
        drag.readOnly = readOnly;
        double px = axes.x >= 0 ? vx[axes.x] : (vx[0] + vx[1]) * 0.5;
        double py = axes.y >= 0 ? vy[axes.y] : (vy[0] + vy[1]) * 0.5;
        if (axes.x >= 0)
            internal::HandleMoveLimits(std::span<const double>(vx, 2), axes.x, cx, &drag.clampLowX, &drag.clampHighX);
        if (axes.y >= 0)
            internal::HandleMoveLimits(std::span<const double>(vy, 2), axes.y, cy, &drag.clampLowY, &drag.clampHighY);
        const internal::ValueDrag2DResult edit = internal::ValueDrag2DBehavior(handle, &px, &py, pad.area, drag);
        // A key press started an edit of this handle: remember where the box was.
        if (!wasActive && g.ActiveId == handleIds[i])
            takeSnapshot();
        if (edit.canceled)
        {
            restoreSnapshot();
        }
        else if (edit.changed)
        {
            if (drag.moveX)
                internal::MoveHandle(std::span<double>(vx, 2), axes.x, px, cx);
            if (drag.moveY)
                internal::MoveHandle(std::span<double>(vy, 2), axes.y, py, cy);
        }
        if (g.ActiveId == handleIds[i])
            activeHandle = i;
        if (handle.focused)
            focusedHandle = i;
        handle.hovered = handle.hovered || i == hoveredPart;
        handle.hoverAmount = std::max(handle.hoverAmount, Tween(handleIds[i], kBoxRangeHoverSub, i == hoveredPart ? 1.0f : 0.0f,
                                                                t.motion.durationFast));
    }

    // The body drags the whole box, keeping its size.
    internal::ItemState body;
    if (o.dragSpan)
    {
        internal::ItemOptions bodyOptions;
        bodyOptions.press = internal::PressMode::Click;
        bodyOptions.navigable = false;
        bodyOptions.hitTarget = false;
        bodyOptions.part = kBoxRangeBodyPart;
        bodyOptions.forced = &forced;
        bodyOptions.readOnly = readOnly;
        body = internal::PartBehavior(bodyId, box, bodyOptions);
        const double spanX = vx[1] - vx[0];
        const double spanY = vy[1] - vy[0];
        internal::ValueDrag2DOptions drag;
        drag.mappingX = mx;
        drag.mappingY = my;
        drag.yDown = o.yDown;
        drag.grabRadius = -1.0f;
        drag.moveX = moveX;
        drag.moveY = moveY;
        drag.readOnly = readOnly;
        drag.clampLowX = mx.min;
        drag.clampHighX = std::max(mx.min, mx.max - spanX);
        drag.clampLowY = my.min;
        drag.clampHighY = std::max(my.min, my.max - spanY);
        double bx = vx[0];
        double by = vy[0];
        const internal::ValueDrag2DResult edit = internal::ValueDrag2DBehavior(body, &bx, &by, pad.area, drag);
        if (edit.canceled)
        {
            restoreSnapshot();
        }
        else if (edit.changed)
        {
            vx[0] = bx;
            vx[1] = bx + spanX;
            vy[0] = by;
            vy[1] = by + spanY;
        }
        // The body reports the pointer but never animates (multi_handle.h, the span).
        body.hovered = body.hovered || hoveredPart == kBoxRangeBodyPart;
        body.hoverAmount = 0.0f;
        body.pressAmount = 0.0f;
    }

    const bool writtenX[2] = {vx[0] != drawnX[0], vx[1] != drawnX[1]};
    const bool writtenY[2] = {vy[0] != drawnY[0], vy[1] != drawnY[1]};
    const bool changed = writtenX[0] || writtenX[1] || writtenY[0] || writtenY[1];
    if (changed && !missing && !readOnly)
    {
        if (writtenX[0])
            low->x = static_cast<float>(vx[0]);
        if (writtenX[1])
            high->x = static_cast<float>(vx[1]);
        if (writtenY[0])
            low->y = static_cast<float>(vy[0]);
        if (writtenY[1])
            high->y = static_cast<float>(vy[1]);
    }

    // ---- Draw ----
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    const bool mixedShown = o.mixed && !changed;
    ImDrawList* drawList = window->DrawList;
    bool focusVisible = false;
    for (const internal::ItemState& handle : handles)
        focusVisible = focusVisible || handle.focusVisible;
    if (ImGui::IsRectVisible(pad.frame.Min, pad.frame.Max))
    {
        internal::FieldFrameLook frameLook;
        frameLook.hoverAmount = padItem.hoverAmount;
        frameLook.disabled = disabled;
        frameLook.readOnly = readOnly && !disabled;
        frameLook.error = o.error != nullptr;
        internal::DrawFieldFrame(drawList, pad.frame, frameLook);
        const ImRect shown = boxRect(vx, vy);
        if (!mixedShown)
        {
            // The box: the selected look of the border rule, the accentSubtle wash and the accent
            // outline at the one hairline width. The body is not a control of its own, so it keeps
            // this look under the pointer and while dragged; only the handles show hover.
            ImVec4 fill = c.accentSubtle;
            internal::OutlineLook outline;
            outline.selected = true;
            if (look == internal::SliderLook::ReadOnly)
            {
                fill = c.fill[2];
                outline.selected = false;
            }
            else if (look == internal::SliderLook::Disabled)
            {
                fill = c.fill[3];
                outline.selected = false;
                outline.disabled = true;
            }
            FillRect(drawList, shown.Min, shown.Max, ToU32(fill));
            internal::DrawOutline(drawList, shown, 0.0f, internal::ControlOutlineColor(outline));
        }
        // Edges first, corners on top, the handle in use above all.
        const int top = activeHandle >= 0 ? activeHandle : focusedHandle;
        const float room = pad.knob * kBoxRangeEdgeRoom;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = pass == 0 ? 0 : 4; i < (pass == 0 ? 4 : kBoxRangeHandles); ++i)
            {
                if (i == top)
                    continue;
                const bool horizontalEdge = i == 1 || i == 3;
                const float length = horizontalEdge ? shown.GetWidth() : shown.GetHeight();
                if (i < 4 && length < room && handles[i].focusAmount <= 0.0f)
                    continue;
                const ImRect knob = internal::PadKnobRect(handleCenter(shown, i), pad.knob);
                if (mixedShown)
                    internal::DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, handles[i].focusAmount, 0,
                                            internal::RingShape::Round);
                else
                    internal::DrawSliderKnob(drawList, knob, handles[i], look);
            }
        }
        if (top >= 0)
        {
            const ImRect knob = internal::PadKnobRect(handleCenter(shown, top), pad.knob);
            if (mixedShown)
                internal::DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, handles[top].focusAmount, 0,
                                        internal::RingShape::Round);
            else
                internal::DrawSliderKnob(drawList, knob, handles[top], look);
        }
    }

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, pad.frame, padItem.hovered, focusVisible, label, labelEnd, labelEnd != label);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(pad.frame.Min.x, pad.frame.Max.y), pad.frame.GetWidth(), disabled, id,
                                     pad.frame);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return changed && !missing && !readOnly;
}

} // namespace facet
