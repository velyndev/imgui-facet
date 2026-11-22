// CanvasElement (facet/widgets/composites.h): a box inside BeginPreviewCanvas, placed in reference
// pixels from an anchor of the canvas.
//
// The box is one item and Tab stop: a press selects it and starts a move, the arrows move it by one
// reference pixel, Shift by ten; with `resizable`, Ctrl with the arrows moves its right or bottom edge
// (Shift: ten). Escape during a move or resize puts it back: the caller's state as it was, exactly.
// While selected, a resizable element shows eight handles (corners and edge middles, composites.canvasHandleSize, each grown to the minimum hit
// target) that resize it like the edges and corners of the BoxRangePad, and `anchorPicker` shows a
// 3 x 3 anchor control beside it (CanvasAnchorControlSpot: inside the canvas where it hides no element,
// else beside the canvas); choosing a cell keeps the element where it is and changes its anchor
// and offset. With `snap`, a moved box or edge comes to the canvas edges and center, the safe area and
// the edges and centers of the other elements of the canvas (as they were in the last frame) when it
// is within composites.canvasSnapDistance; Alt held moves freely. The guide it snapped to and the
// canvas center guides are drawn by EndPreviewCanvas.
//
// The box: fill[2] (fill[1] on hover), the boundary hairline, an accent outline (focusRing) while
// selected, the visible label part centered and cut to fit; disabled in fill[3] and labelTertiary.
// After the call the box is Dear ImGui's last item, so the caller can draw its own preview into
// ImGui::GetItemRectMin and Max.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/composites/editors_frame.h"

namespace facet
{
namespace
{

constexpr int kCanvasHandles = 8;
constexpr int kCanvasCells = 9;
// The keyboard steps in reference pixels: one, and ten with Shift.
constexpr float kCanvasStep = 1.0f;
constexpr float kCanvasStepCoarse = 10.0f;

struct CanvasElementDragState
{
    int part = -1;          // the part a mouse drag holds: 0 the box, 1 to 8 a handle; -1 none
    ImRect start;           // the element when the drag began, reference pixels
    facet::CanvasElementState startValue;  // the caller's state then, which Escape writes back unchanged
    ImVec2 pressMouse;
    ImRect rects[1 + kCanvasHandles + kCanvasCells];  // the parts of the last frame, screen pixels
    ImGuiID ids[1 + kCanvasHandles + kCanvasCells] = {};
};

ImVec2 CanvasAnchorFraction(CanvasAnchor anchor)
{
    const int i = static_cast<int>(anchor);
    return ImVec2(static_cast<float>(i % 3) * 0.5f, static_cast<float>(i / 3) * 0.5f);
}

// The element's rectangle in reference pixels.
ImRect CanvasElementRect(const facet::CanvasElementState& element, ImVec2 reference)
{
    const ImVec2 a = CanvasAnchorFraction(element.anchor);
    const ImVec2 extent(std::max(0.0f, element.extent.x), std::max(0.0f, element.extent.y));
    const ImVec2 min(a.x * reference.x + element.offset.x - a.x * extent.x, a.y * reference.y + element.offset.y - a.y * extent.y);
    return ImRect(min, ImVec2(min.x + extent.x, min.y + extent.y));
}

// The offset that puts the element's rectangle at `rect` with `anchor`.
ImVec2 CanvasOffsetFor(const ImRect& rect, CanvasAnchor anchor, ImVec2 reference)
{
    const ImVec2 a = CanvasAnchorFraction(anchor);
    return ImVec2(rect.Min.x - a.x * reference.x + a.x * rect.GetWidth(), rect.Min.y - a.y * reference.y + a.y * rect.GetHeight());
}

// The shift (reference pixels) that brings the nearest of `edges` onto the nearest target within
// `distance`; *guide receives that target, -1 for none.
float CanvasSnap(const float* edges, int edgeCount, const float* targets, int targetCount, float distance, float* guide)
{
    float best = distance;
    float shift = 0.0f;
    *guide = -1.0f;
    for (int e = 0; e < edgeCount; ++e)
        for (int t = 0; t < targetCount; ++t)
        {
            const float d = std::fabs(targets[t] - edges[e]);
            if (d <= best)
            {
                best = d;
                shift = targets[t] - edges[e];
                *guide = targets[t];
            }
        }
    return shift;
}

// The snap targets of one axis: the canvas edges and center, the safe area, the other elements.
int CanvasTargets(const internal::PreviewCanvasFrame& canvas, ImGuiID self, bool xAxis, float* out, int capacity)
{
    const float size = xAxis ? canvas.reference.x : canvas.reference.y;
    int n = 0;
    const auto add = [&](float v) {
        if (n < capacity)
            out[n++] = v;
    };
    add(0.0f);
    add(size * 0.5f);
    add(size);
    if (canvas.safeArea > 0.0f)
    {
        add(size * canvas.safeArea);
        add(size * (1.0f - canvas.safeArea));
    }
    for (int i = 0; i < canvas.otherCount; ++i)
    {
        if (canvas.otherIds[i] == self)
            continue;
        const ImRect& r = canvas.others[i];
        add(xAxis ? r.Min.x : r.Min.y);
        add(xAxis ? r.GetCenter().x : r.GetCenter().y);
        add(xAxis ? r.Max.x : r.Max.y);
    }
    return n;
}

const char* CanvasAnchorName(int cell)
{
    const Strings& s = GetStrings();
    const char* names[kCanvasCells] = {s.anchorTopLeft, s.anchorTop,        s.anchorTopRight, s.anchorLeft,      s.anchorCenter,
                                       s.anchorRight,   s.anchorBottomLeft, s.anchorBottom,   s.anchorBottomRight};
    return names[std::clamp(cell, 0, kCanvasCells - 1)];
}

// The top-left corner of the anchor control (`side` square) for the element `self` drawn at `box`, so
// that it hides neither that element nor another element of the canvas (as they were in the last
// frame): beside the element inside the canvas (right, else left), else under or above it; where no
// such spot is free, beside the canvas inside the caller's clip rect (right, else left); else clamped
// into the canvas.
ImVec2 CanvasAnchorControlSpot(const internal::PreviewCanvasFrame& canvas, ImGuiID self, const ImRect& box, float side, float gap)
{
    const ImRect& frame = canvas.frame;
    const ImRect& outer = canvas.outerClip;
    const auto spotFree = [&](float x, float y) {
        if (x < frame.Min.x || x + side > frame.Max.x || y < frame.Min.y || y + side > frame.Max.y)
            return false;
        const ImRect spot(ImVec2(x, y), ImVec2(x + side, y + side));
        for (int i = 0; i < canvas.otherCount; ++i)
        {
            if (canvas.otherIds[i] == self)
                continue;
            const ImRect& r = canvas.others[i];
            const ImRect other(ImVec2(canvas.screen.Min.x + r.Min.x * canvas.scale, canvas.screen.Min.y + r.Min.y * canvas.scale),
                               ImVec2(canvas.screen.Min.x + r.Max.x * canvas.scale, canvas.screen.Min.y + r.Max.y * canvas.scale));
            if (spot.Overlaps(other))
                return false;
        }
        return true;
    };
    const float inY = std::clamp(box.Min.y, frame.Min.y, std::max(frame.Min.y, frame.Max.y - side));
    const float inX = std::clamp(box.Min.x, frame.Min.x, std::max(frame.Min.x, frame.Max.x - side));
    const ImVec2 inside[4] = {{box.Max.x + gap, inY}, {box.Min.x - gap - side, inY}, {inX, box.Max.y + gap}, {inX, box.Min.y - gap - side}};
    for (const ImVec2& p : inside)
        if (spotFree(p.x, p.y))
            return p;
    const float outY = std::clamp(inY, outer.Min.y, std::max(outer.Min.y, outer.Max.y - side));
    const ImVec2 outside[2] = {{frame.Max.x + gap, outY}, {frame.Min.x - gap - side, outY}};
    for (const ImVec2& p : outside)
        if (p.x >= outer.Min.x && p.x + side <= outer.Max.x)
            return p;
    return ImVec2(inX, inY);
}

} // namespace

bool CanvasElement(const char* label, CanvasElementState* element, const CanvasElementOptions& options)
{
    internal::PreviewCanvasFrame* canvas = internal::CurrentPreviewCanvas();
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (canvas == nullptr)
    {
        internal::ReportUsageError("CanvasElement outside BeginPreviewCanvas and EndPreviewCanvas");
        return false;
    }
    if (!canvas->valid || canvas->window != window)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const ImGuiIO& io = g.IO;
    const Theme& t = T();
    const Colors& c = t.colors;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || element == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    CanvasElementDragState& state = internal::GetState<CanvasElementDragState>(id);
    facet::CanvasElementState value = element != nullptr ? *element : facet::CanvasElementState{};
    const ImVec2 R = canvas->reference;
    const float scale = canvas->scale;
    const auto toScreen = [&](const ImRect& r) {
        return ImRect(ImVec2(canvas->screen.Min.x + r.Min.x * scale, canvas->screen.Min.y + r.Min.y * scale),
                      ImVec2(canvas->screen.Min.x + r.Max.x * scale, canvas->screen.Min.y + r.Max.y * scale));
    };
    ImRect rect = CanvasElementRect(value, R);
    ImRect box = toScreen(rect);
    box.Max = ImVec2(std::max(box.Max.x, box.Min.x + 1.0f), std::max(box.Max.y, box.Min.y + 1.0f));

    // The box.
    internal::ItemOptions boxOptions;
    boxOptions.press = internal::PressMode::Click;
    boxOptions.hitTarget = false;
    boxOptions.allowOverlap = true;  // the handles and the anchor cells over it take their own clicks
    boxOptions.readOnly = readOnly;
    boxOptions.forced = &forced;
    boxOptions.part = 0;
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::PartBehavior(id, box, boxOptions);
    const ImGuiLastItemData boxData = g.LastItemData;
    const bool real = item.forced == internal::ForcedItemState::None && !item.disabled;
    if (real && (item.pressed || item.focused))
        *canvas->selected = id;
    const bool selected = *canvas->selected == id || Has(item.forced, internal::ForcedItemState::Focused) ||
                          Has(item.forced, internal::ForcedItemState::Dragging);
    bool written = false;
    bool restored = false;  // Escape ended a drag: the state at its start is written back as it was

    // A mouse drag of the box.
    if (real && !readOnly && item.active && io.MouseDown[ImGuiMouseButton_Left])
    {
        if (state.part != 0)
        {
            state.part = 0;
            state.start = rect;
            state.startValue = value;
            state.pressMouse = io.MousePos;
        }
    }

    // The handles, while selected.
    const bool handles = options.resizable && selected && !readOnly;
    const float handleSize = std::round(Px(t.composites.canvasHandleSize));
    ImRect handleRects[kCanvasHandles];
    internal::ItemState handleItems[kCanvasHandles];
    // Corners and edge middles, clockwise from the top-left; per handle the edges it moves.
    const ImVec2 at[kCanvasHandles] = {{0, 0}, {0.5f, 0}, {1, 0}, {1, 0.5f}, {1, 1}, {0.5f, 1}, {0, 1}, {0, 0.5f}};
    for (int k = 0; k < kCanvasHandles; ++k)
    {
        const ImVec2 p(box.Min.x + at[k].x * box.GetWidth(), box.Min.y + at[k].y * box.GetHeight());
        handleRects[k] = ImRect(ImVec2(std::round(p.x - handleSize * 0.5f), std::round(p.y - handleSize * 0.5f)),
                                ImVec2(std::round(p.x - handleSize * 0.5f) + handleSize, std::round(p.y - handleSize * 0.5f) + handleSize));
        state.rects[1 + k] = handles ? handleRects[k] : ImRect();
        state.ids[1 + k] = ImHashData(&k, sizeof(k), ImHashStr("##handle", 0, id));
        if (!handles)
            continue;
        internal::ItemOptions handleOptions;
        handleOptions.press = internal::PressMode::Click;
        handleOptions.navigable = false;
        handleOptions.handCursor = false;
        handleOptions.forced = &forced;
        handleOptions.part = 1 + k;
        handleItems[k] = internal::PartBehavior(state.ids[1 + k], handleRects[k], handleOptions);
        const internal::ItemState& h = handleItems[k];
        if (h.hovered || h.active)
        {
            const bool diagonalMain = k == 0 || k == 4;
            const bool diagonalOther = k == 2 || k == 6;
            ImGui::SetMouseCursor(diagonalMain ? ImGuiMouseCursor_ResizeNWSE
                                               : (diagonalOther ? ImGuiMouseCursor_ResizeNESW
                                                                : (at[k].x == 0.5f ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW)));
        }
        if (h.active && io.MouseDown[ImGuiMouseButton_Left] && h.forced == internal::ForcedItemState::None && state.part != 1 + k)
        {
            state.part = 1 + k;
            state.start = rect;
            state.startValue = value;
            state.pressMouse = io.MousePos;
        }
    }
    const ImGuiID activePartId = state.part == 0 ? id : (state.part > 0 ? state.ids[state.part] : 0);
    if (state.part >= 0 && (g.ActiveId != activePartId || !io.MouseDown[ImGuiMouseButton_Left]))
        state.part = -1;

    // Apply the drag: the pointer travel since the press, in reference pixels, then the snapping.
    if (state.part >= 0 && scale > 0.0f)
    {
        canvas->dragging = true;
        const ImVec2 delta((io.MousePos.x - state.pressMouse.x) / scale, (io.MousePos.y - state.pressMouse.y) / scale);
        ImRect moved = state.start;
        const bool snap = options.snap && !io.KeyAlt;
        const float distance = Px(t.composites.canvasSnapDistance) / scale;
        float targetsX[64];
        float targetsY[64];
        const int countX = CanvasTargets(*canvas, id, true, targetsX, 64);
        const int countY = CanvasTargets(*canvas, id, false, targetsY, 64);
        float guideX = -1.0f;
        float guideY = -1.0f;
        if (state.part == 0)
        {
            moved.Translate(delta);
            if (snap)
            {
                const float edgesX[3] = {moved.Min.x, moved.GetCenter().x, moved.Max.x};
                const float edgesY[3] = {moved.Min.y, moved.GetCenter().y, moved.Max.y};
                moved.Translate(ImVec2(CanvasSnap(edgesX, 3, targetsX, countX, distance, &guideX),
                                      CanvasSnap(edgesY, 3, targetsY, countY, distance, &guideY)));
            }
        }
        else
        {
            const ImVec2 side = at[state.part - 1];
            const auto moveEdge = [&](float* edge, float d, const float* targets, int count, float* guide) {
                *edge += d;
                if (snap)
                    *edge += CanvasSnap(edge, 1, targets, count, distance, guide);
            };
            if (side.x == 0.0f)
                moveEdge(&moved.Min.x, delta.x, targetsX, countX, &guideX);
            if (side.x == 1.0f)
                moveEdge(&moved.Max.x, delta.x, targetsX, countX, &guideX);
            if (side.y == 0.0f)
                moveEdge(&moved.Min.y, delta.y, targetsY, countY, &guideY);
            if (side.y == 1.0f)
                moveEdge(&moved.Max.y, delta.y, targetsY, countY, &guideY);
            // Never smaller than one reference pixel: the moved edge stops at the other.
            if (moved.GetWidth() < 1.0f)
                (side.x == 0.0f ? moved.Min.x : moved.Max.x) = side.x == 0.0f ? moved.Max.x - 1.0f : moved.Min.x + 1.0f;
            if (moved.GetHeight() < 1.0f)
                (side.y == 0.0f ? moved.Min.y : moved.Max.y) = side.y == 0.0f ? moved.Max.y - 1.0f : moved.Min.y + 1.0f;
        }
        canvas->snapX = guideX;
        canvas->snapY = guideY;
        // Escape puts the element back where the drag began.
        ImGui::SetKeyOwner(ImGuiKey_Escape, activePartId);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, activePartId))
        {
            moved = state.start;
            restored = true;
            state.part = -1;
            ImGui::ClearActiveID();
        }
        if (restored)
        {
            const facet::CanvasElementState& s = state.startValue;
            rect = moved;
            written = s.anchor != value.anchor || s.offset.x != value.offset.x || s.offset.y != value.offset.y ||
                      s.extent.x != value.extent.x || s.extent.y != value.extent.y;
        }
        else if (moved.Min.x != rect.Min.x || moved.Min.y != rect.Min.y || moved.Max.x != rect.Max.x || moved.Max.y != rect.Max.y)
        {
            rect = moved;
            written = true;
        }
    }

    // Keys on the focused box.
    if (real && !readOnly && item.focused && g.NavId == id && state.part < 0)
    {
        const ImGuiKey arrows[4] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow};
        const ImVec2 dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        const float step = io.KeyShift ? kCanvasStepCoarse : kCanvasStep;
        for (int k = 0; k < 4; ++k)
        {
            ImGui::SetKeyOwner(arrows[k], id);
            if (!ImGui::IsKeyPressed(arrows[k], ImGuiInputFlags_Repeat, id))
                continue;
            if (io.KeyCtrl && options.resizable)
            {
                rect.Max.x = std::max(rect.Min.x + 1.0f, rect.Max.x + dirs[k].x * step);
                rect.Max.y = std::max(rect.Min.y + 1.0f, rect.Max.y + dirs[k].y * step);
            }
            else
            {
                rect.Translate(ImVec2(dirs[k].x * step, dirs[k].y * step));
            }
            written = true;
            internal::SetKeyboardModality(true);
        }
    }

    // The anchor control beside the selected element.
    int anchorCell = static_cast<int>(value.anchor);
    if (options.anchorPicker && selected)
    {
        const float cell = std::round(Px(t.metrics.minHitTarget));
        const float gap = std::round(Px(t.spacing.xs));
        const float side = cell * 3.0f;
        const ImVec2 spot = CanvasAnchorControlSpot(*canvas, id, box, side, gap);
        const float x = spot.x;
        const float y = spot.y;
        // A spot outside the canvas lies outside its clip rect: the control is clipped to the caller's.
        ImGui::PushClipRect(canvas->outerClip.Min, canvas->outerClip.Max, false);
        ImDrawList* drawList = window->DrawList;
        // A small floating surface: bgElevated with the separator hairline of every floating layer.
        FillRect(drawList, ImVec2(x, y), ImVec2(x + side, y + side), ToU32(c.bgElevated), Px(t.radii.element));
        internal::DrawSurfaceOutline(drawList, ImRect(ImVec2(x, y), ImVec2(x + side, y + side)), Px(t.radii.element));
        for (int k = 0; k < kCanvasCells; ++k)
        {
            const ImRect cellRect(ImVec2(x + cell * static_cast<float>(k % 3), y + cell * static_cast<float>(k / 3)),
                                  ImVec2(x + cell * static_cast<float>(k % 3 + 1), y + cell * static_cast<float>(k / 3 + 1)));
            const ImGuiID cellId = ImHashData(&k, sizeof(k), ImHashStr("##anchor", 0, id));
            state.rects[1 + kCanvasHandles + k] = cellRect;
            state.ids[1 + kCanvasHandles + k] = cellId;
            internal::ItemOptions cellOptions;
            cellOptions.readOnly = readOnly;
            cellOptions.forced = &forced;
            cellOptions.part = 1 + kCanvasHandles + k;
            const internal::ItemState cellItem = internal::PartBehavior(cellId, cellRect, cellOptions);
            if (cellItem.pressed && !readOnly && cellItem.forced == internal::ForcedItemState::None && k != anchorCell)
            {
                anchorCell = k;
                written = true;
            }
            const float hover = cellItem.disabled ? 0.0f : std::max(cellItem.hoverAmount, cellItem.pressAmount);
            if (hover > 0.0f)
                FillRect(drawList, cellRect.Min, cellRect.Max, ToU32(WithAlpha(c.fill[1], c.fill[1].w * hover)), Px(t.radii.element));
            // The chosen anchor is a larger dot in the accent (focusRing, 3:1); the others small dots.
            const bool chosen = k == anchorCell;
            const float dot = Px(t.metrics.statusDotSize) * (chosen ? 0.5f : 0.25f);
            drawList->AddCircleFilled(cellRect.GetCenter(), dot,
                                      ToU32(cellItem.disabled ? c.labelTertiary : (chosen ? c.focusRing : c.labelSecondary)));
            // The cells touch each other and the control's edge: the ring sits inside the cell.
            internal::DrawItemFocusRingInside(cellItem, Px(t.radii.element));
            internal::CutTextTooltip(cellItem, CanvasAnchorName(k), nullptr, true);
        }
        ImGui::PopClipRect();
    }
    else
    {
        for (int k = 0; k < kCanvasCells; ++k)
            state.rects[1 + kCanvasHandles + k] = ImRect();
    }

    // Write back: the state the drag began with after Escape (exactly, not recomputed from `rect`),
    // else the anchor and offset that put the element at `rect`.
    if (written && !readOnly && element != nullptr && restored)
    {
        *element = state.startValue;
        box = toScreen(rect);
    }
    else if (written && !readOnly && element != nullptr)
    {
        const CanvasAnchor anchor = static_cast<CanvasAnchor>(anchorCell);
        element->anchor = anchor;
        element->extent = ImVec2(rect.GetWidth(), rect.GetHeight());
        element->offset = CanvasOffsetFor(rect, anchor, R);
        box = toScreen(rect);
    }
    else
    {
        written = false;
    }
    state.rects[0] = box;
    state.ids[0] = id;
    internal::PreviewCanvasRecordElement(id, rect);

    // ---- Draw the box under the handles ----
    ImDrawList* drawList = window->DrawList;
    if (ImGui::IsRectVisible(box.Min, box.Max))
    {
        const float radius = Px(t.radii.element);
        const bool disabled = item.disabled;
        // The tile look of the row rule: the neutral fill and the controlBorder hairline, a selected
        // element accentSubtle and the accent outline at the same width.
        const ImVec4 fill = disabled ? c.fill[3]
                                     : (selected ? c.accentSubtle
                                                 : Mix(c.fill[2], c.fill[1], std::max(item.hoverAmount, item.pressAmount)));
        FillRect(drawList, box.Min, box.Max, ToU32(fill), radius);
        internal::DrawOutline(drawList, box, radius,
                              internal::ControlOutlineColor(
                                  {.hoverAmount = item.hoverAmount, .selected = selected, .disabled = disabled}));
        const char* labelEnd = ImGui::FindRenderedTextEnd(label);
        internal::ControlTextScope footnote(TextStyle::Footnote);
        const float pad = Px(t.spacing.xs);
        const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, box.GetWidth() - pad * 2.0f));
        if (shown.text[0] != '\0' && box.GetHeight() >= ImGui::GetFontSize())
        {
            const ImVec2 pos(std::round(box.GetCenter().x - shown.width * 0.5f), std::round(box.GetCenter().y - ImGui::GetFontSize() * 0.5f));
            drawList->PushClipRect(box.Min, box.Max, true);
            drawList->AddText(pos, ToU32(internal::ControlLabelColor(disabled)), shown.text);
            drawList->PopClipRect();
        }
        internal::DrawFocusRing(drawList, box, radius, item.focusAmount);
        internal::CutTextTooltip(item, label, labelEnd, shown.truncated);
    }
    if (handles)
    {
        for (int k = 0; k < kCanvasHandles; ++k)
        {
            // Square handles on the box as it is now, in the accent outline of the selection; hover and
            // press fill the handle with the accent mark, so the stroke keeps its one width.
            const internal::ItemState& h = handleItems[k];
            const ImVec2 p(box.Min.x + at[k].x * box.GetWidth(), box.Min.y + at[k].y * box.GetHeight());
            const ImVec2 hMin(std::round(p.x - handleSize * 0.5f), std::round(p.y - handleSize * 0.5f));
            const ImRect hr(hMin, ImVec2(hMin.x + handleSize, hMin.y + handleSize));
            const float feedback = h.disabled ? 0.0f : std::max(h.hoverAmount, h.pressAmount);
            FillRect(drawList, hr.Min, hr.Max, ToU32(Mix(c.bg, c.focusRing, feedback)), 0.0f);
            internal::DrawOutline(drawList, hr, 0.0f, c.focusRing);
        }
    }

    // The box is the last item, so the item queries and the caller's own drawing use it.
    g.LastItemData = boxData;
    if (written)
        internal::MarkWidgetEdited(id);
    return written;
}

namespace internal
{

ImRect CanvasElementPartRect(ImGuiID id, int part, ImGuiID* itemId)
{
    if (itemId != nullptr)
        *itemId = 0;
    if (ImGui::GetCurrentContext() == nullptr || part < 0 || part > kCanvasHandles + kCanvasCells)
        return ImRect();
    const CanvasElementDragState& state = GetState<CanvasElementDragState>(id);
    if (itemId != nullptr)
        *itemId = state.ids[part];
    return state.rects[part];
}

} // namespace internal

} // namespace facet
