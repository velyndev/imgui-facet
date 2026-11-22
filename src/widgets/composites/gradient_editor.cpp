// GradientEditor (facet/widgets/composites.h): a gradient bar over a row of color stop markers.
//
// The markers run on the multi-handle block (src/internal/multi_handle.h) with free handles, so a
// stop dragged past another passes it: after every edit the widget sorts the caller's stops by
// position and carries each stop's key with it, and the block keys every handle's item id, focus,
// drag and Escape snapshot by that key, so a stop keeps its drag, its focus and its open popover
// while it changes places. The markers row takes the presses of the block: a press on a marker grabs
// it, a press beside the markers moves the nearest one there. Each marker is a Tab stop; the arrows
// move it (Shift finer, Ctrl coarser), Home and End send it to the ends, Escape during an edit puts
// every stop back.
//
// Adding and removing: a click on the bar adds a stop there with the color the bar shows at that
// point and gives it the focus; Insert on a focused marker adds one half way to its right neighbour
// (to its left one for the last stop). Delete or Backspace on a focused marker removes it, and so
// does a marker dragged more than composites.stopRemoveDistance above or below the widget (it fades
// while it would go, and goes on release). The last stop is never removed. Adding needs room in the
// caller's storage (the vector overload grows its vector). At most kMaxHandles (16) stops are shown
// and edited; stops beyond that in the caller's storage are left as they are.
//
// A click on a marker that does not move it, or Enter or Space on a focused one, opens a popover with
// the stop's color picker (facet::ColorPicker), its position in percent (facet::NumberInput) and a
// remove button (facet::Button). Escape closes the popover and restores the color from before it
// opened; a click outside keeps the edit.
//
// The bar draws the gradient as the stored values read: each channel, alpha included, interpolated
// linearly between neighbouring stops in gamma-encoded sRGB (the space the color pickers draw in),
// the first and last color held out to the ends; with options.alpha over the checkerboard.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/pickers.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/composites/editors_frame.h"
#include "widgets/pickers/color_model.h"

namespace facet
{
namespace
{

constexpr int kGradientMaxStops = internal::kMaxHandles;
// Strips per rounded end of the bar: enough that the curve of a radii.element corner reads round
// under the hairline drawn over it.
constexpr int kGradientCapStrips = 6;
// The share of its color a marker keeps while a drag would remove it.
constexpr float kGradientRemoveOpacity = 0.35f;

using GradientGrowFn = bool (*)(void* user, GradientStop** data, int* capacity, int count);

struct GradientEditorState
{
    ImGuiID keys[kGradientMaxStops] = {};
    int count = -1;          // stops the keys belong to; -1 before the first frame
    ImGuiID nextKey = 1;
    ImGuiID popoverKey = 0;  // the stop whose popover is open
    ImVec4 before = {};      // its color when the popover opened, for Escape
    ImGuiID pressKey = 0;    // the stop a mouse press holds
    ImVec2 pressMouse = {};
    bool moved = false;      // the press moved the pointer beyond the drag threshold
    bool removeArmed = false;
    // Where the last frame drew the bar and the markers (GradientEditorBarRect, GradientEditorStopRect).
    ImRect bar;
    ImRect stops[kGradientMaxStops];
    int drawn = 0;
};

int GradientIndexOf(const GradientEditorState& state, int count, ImGuiID key)
{
    for (int i = 0; i < count; ++i)
        if (state.keys[i] == key)
            return i;
    return -1;
}

// The handle item id of the stop with `key`, as the multi-handle block derives it.
ImGuiID GradientHandleId(ImGuiID widgetId, ImGuiID key)
{
    return ImHashData(&key, sizeof(key), widgetId);
}

// The indices of the first `count` stops in the order of their positions.
void GradientOrder(const GradientStop* stops, int count, int* order)
{
    for (int i = 0; i < count; ++i)
        order[i] = i;
    for (int i = 1; i < count; ++i)
    {
        const int moving = order[i];
        int j = i;
        while (j > 0 && stops[order[j - 1]].position > stops[moving].position)
        {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = moving;
    }
}

ImVec4 GradientMix(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

// The color of the gradient at `position` (0 to 1), from the stops in `order`.
ImVec4 GradientColorAt(const GradientStop* stops, const int* order, int count, float position)
{
    if (count <= 0)
        return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    const GradientStop& first = stops[order[0]];
    if (position <= first.position)
        return first.color;
    for (int k = 1; k < count; ++k)
    {
        const GradientStop& a = stops[order[k - 1]];
        const GradientStop& b = stops[order[k]];
        if (position <= b.position)
        {
            const float span = b.position - a.position;
            return span > 0.0f ? GradientMix(a.color, b.color, (position - a.position) / span) : b.color;
        }
    }
    return stops[order[count - 1]].color;
}

// The bar: vertical strips between every stop and along the rounded ends, each a quad with the
// gradient's colors at its two edges, so the gradient is exact at every stop and the ends follow
// the rounded shape. `alphaShare` multiplies every alpha; `opaque` drops the stops' alpha.
void DrawGradientBar(ImDrawList* drawList, const ImRect& bar, float radius, const GradientStop* stops, const int* order,
                     int count, bool opaque, float alphaShare)
{
    const float width = bar.GetWidth();
    if (width <= 0.0f || bar.GetHeight() <= 0.0f || count <= 0)
        return;
    radius = std::min(radius, std::min(width, bar.GetHeight()) * 0.5f);
    constexpr int kMaxCuts = kGradientMaxStops + (kGradientCapStrips + 1) * 2 + 2;
    float cuts[kMaxCuts];
    int cutCount = 0;
    const auto add = [&](float x) {
        if (cutCount < kMaxCuts && x >= bar.Min.x && x <= bar.Max.x)
            cuts[cutCount++] = x;
    };
    add(bar.Min.x);
    add(bar.Max.x);
    for (int k = 0; k <= kGradientCapStrips; ++k)
    {
        const float angle = IM_PI * 0.5f * static_cast<float>(k) / static_cast<float>(kGradientCapStrips);
        const float d = radius * (1.0f - std::cos(angle));
        add(bar.Min.x + d);
        add(bar.Max.x - d);
    }
    for (int i = 0; i < count; ++i)
        add(bar.Min.x + std::clamp(stops[i].position, 0.0f, 1.0f) * width);
    std::sort(cuts, cuts + cutCount);

    const auto inset = [&](float x) {
        const float d = std::min(x - bar.Min.x, bar.Max.x - x);
        if (d >= radius || radius <= 0.0f)
            return 0.0f;
        const float u = radius - d;
        return radius - std::sqrt(std::max(0.0f, radius * radius - u * u));
    };
    const auto colorAt = [&](float x) {
        ImVec4 color = GradientColorAt(stops, order, count, (x - bar.Min.x) / width);
        color.w = (opaque ? 1.0f : color.w) * alphaShare;
        return ToU32(color);
    };
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    for (int k = 1; k < cutCount; ++k)
    {
        const float x0 = cuts[k - 1];
        const float x1 = cuts[k];
        if (x1 - x0 <= 0.0f)
            continue;
        const float i0 = inset(x0);
        const float i1 = inset(x1);
        const ImU32 c0 = colorAt(x0);
        const ImU32 c1 = colorAt(x1);
        drawList->PrimReserve(6, 4);
        const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
        drawList->PrimWriteVtx(ImVec2(x0, bar.Min.y + i0), uv, c0);
        drawList->PrimWriteVtx(ImVec2(x1, bar.Min.y + i1), uv, c1);
        drawList->PrimWriteVtx(ImVec2(x1, bar.Max.y - i1), uv, c1);
        drawList->PrimWriteVtx(ImVec2(x0, bar.Max.y - i0), uv, c0);
    }
}

// Sorts the first `count` stops by position (stable), carrying the keys with them.
void GradientSortStops(GradientStop* stops, ImGuiID* keys, int count)
{
    for (int i = 1; i < count; ++i)
    {
        const GradientStop stop = stops[i];
        const ImGuiID key = keys[i];
        int j = i;
        while (j > 0 && stops[j - 1].position > stop.position)
        {
            stops[j] = stops[j - 1];
            keys[j] = keys[j - 1];
            --j;
        }
        stops[j] = stop;
        keys[j] = key;
    }
}

// Removes the stop at `at`, keys included. Returns the new count.
int GradientRemoveStop(GradientStop* stops, ImGuiID* keys, int count, int at)
{
    for (int i = at; i + 1 < count; ++i)
    {
        stops[i] = stops[i + 1];
        keys[i] = keys[i + 1];
    }
    return count - 1;
}

internal::PopupAnchor GradientStopAnchor(const ImRect& track, float position, float size)
{
    internal::PopupAnchor anchor;
    anchor.rect = internal::HandleRect(track, Orientation::Horizontal, {.min = 0.0, .max = 1.0},
                                       std::clamp(position, 0.0f, 1.0f), size);
    anchor.side = internal::PopupSide::Below;
    anchor.align = internal::PopupAlign::Center;
    return anchor;
}

bool GradientEditorImpl(const char* label, GradientStop* data, int capacity, int* count, GradientGrowFn grow,
                        void* growUser, const GradientEditorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& strings = GetStrings();
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || data == nullptr || count == nullptr ||
                          (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const int stored = count != nullptr ? std::max(0, *count) : 0;
    int n = data != nullptr ? std::min({stored, capacity, kGradientMaxStops}) : 0;
    GradientEditorState& state = internal::GetState<GradientEditorState>(id);
    if (state.count != n)
    {
        // Stops changed from outside: fresh keys, and no popover or press carried over.
        for (int i = 0; i < n; ++i)
            state.keys[i] = state.nextKey++;
        state.count = n;
        state.popoverKey = 0;
        state.pressKey = 0;
    }

    // Layout: the bar, a gap, and the markers row (at least the minimum hit target tall).
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float height = options.height > 0.0f ? std::round(Px(options.height))
                                               : std::round(ControlHeight(ControlSize::Inherit) * 2.0f);
    const float stopSize = std::round(Px(t.composites.gradientStopSize));
    const float rowHeight = std::round(std::max(stopSize, Px(t.metrics.minHitTarget)));
    const float gap = std::round(Px(t.spacing.xs));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float barHeight = std::max(1.0f, height - rowHeight - gap);
    const float half = std::round(stopSize * 0.5f);
    const ImRect bar(origin.x + half, origin.y, std::max(origin.x + half + 1.0f, origin.x + width - half),
                     origin.y + barHeight);
    const ImRect row(origin.x, bar.Max.y + gap, std::max(origin.x + 1.0f, origin.x + width), bar.Max.y + gap + rowHeight);
    const float rowCenter = std::round((row.Min.y + row.Max.y) * 0.5f);
    const ImRect track(bar.Min.x, rowCenter, bar.Max.x, rowCenter);
    const ImRect frame(origin, row.Max);

    bool written = false;
    ImGui::BeginGroup();

    // The bar takes the clicks that add a stop. It is no Tab stop: Insert on a marker adds by keyboard.
    const internal::ForcedItemRequest noForced;
    internal::ItemOptions barOptions;
    barOptions.press = internal::PressMode::Click;
    barOptions.navigable = false;
    barOptions.hitTarget = false;
    barOptions.readOnly = readOnly;
    barOptions.forced = &noForced;
    internal::ApplyNextItemData(next);
    const internal::ItemState barItem = internal::PartBehavior(ImHashStr("##bar", 0, id), bar, barOptions);
    const bool disabled = barItem.disabled;

    // The markers.
    ImGui::SetCursorScreenPos(row.Min);
    double values[kGradientMaxStops];
    float clamped[kGradientMaxStops];
    for (int i = 0; i < n; ++i)
    {
        clamped[i] = std::isnan(data[i].position) ? 0.0f : std::clamp(data[i].position, 0.0f, 1.0f);
        values[i] = clamped[i];
    }
    internal::ItemState handles[kGradientMaxStops];
    internal::MultiHandleOptions handleOptions;
    handleOptions.constraints.collision = internal::CollisionMode::Free;
    handleOptions.drag.mapping = {.min = 0.0, .max = 1.0};
    handleOptions.drag.readOnly = readOnly;
    handleOptions.drag.wheel = false;
    handleOptions.handleSize = stopSize;
    handleOptions.spanDrag = false;
    handleOptions.removeKeys = !readOnly && n > 1;
    handleOptions.keys = std::span<const ImGuiID>(state.keys, static_cast<std::size_t>(n));
    const internal::MultiHandleResult edit = internal::MultiHandleBehavior(
        id, row, track, std::span<double>(values, static_cast<std::size_t>(n)),
        std::span<internal::ItemState>(handles, static_cast<std::size_t>(n)), handleOptions);
    if (edit.changed && !readOnly)
    {
        for (int i = 0; i < n; ++i)
        {
            if (static_cast<float>(values[i]) != clamped[i])
            {
                data[i].position = static_cast<float>(values[i]);
                written = true;
            }
        }
    }

    // A mouse press on a marker: a click that does not move it opens the popover; a drag far off the
    // widget removes the stop on release.
    const float threshold = g.IO.MouseDragThreshold;
    ImGuiID openKey = 0;
    ImGuiID removeKey = 0;
    const bool mouseHeld = edit.activeHandle >= 0 && g.IO.MouseDown[ImGuiMouseButton_Left] &&
                           handles[edit.activeHandle].forced == internal::ForcedItemState::None;
    if (mouseHeld)
    {
        const ImGuiID key = state.keys[edit.activeHandle];
        if (state.pressKey != key)
        {
            state.pressKey = key;
            state.pressMouse = g.IO.MousePos;
            state.moved = false;
        }
        const ImVec2 delta(g.IO.MousePos.x - state.pressMouse.x, g.IO.MousePos.y - state.pressMouse.y);
        if (delta.x * delta.x + delta.y * delta.y > threshold * threshold)
            state.moved = true;
        const float off = std::max(frame.Min.y - g.IO.MousePos.y, g.IO.MousePos.y - frame.Max.y);
        state.removeArmed = !readOnly && n > 1 && state.moved && off > Px(t.composites.stopRemoveDistance);
    }
    else if (state.pressKey != 0)
    {
        if (state.removeArmed && !edit.canceled)
            removeKey = state.pressKey;
        else if (!state.moved && !edit.canceled)
            openKey = state.pressKey;
        state.pressKey = 0;
        state.removeArmed = false;
    }
    for (int i = 0; i < n; ++i)
    {
        if (handles[i].pressed && handles[i].keyboardPressed)
            openKey = state.keys[i];
    }
    if (edit.removeRequested >= 0)
        removeKey = state.keys[edit.removeRequested];

    // The order follows the positions; keys, focus, drag and popover go with their stops.
    if (written)
        GradientSortStops(data, state.keys, n);

    // Adding: a click on the bar, or Insert on a focused marker.
    int order[kGradientMaxStops];
    GradientOrder(data, n, order);
    float addAt = -1.0f;
    if (barItem.pressed && !readOnly && !disabled && barItem.forced == internal::ForcedItemState::None)
        addAt = std::clamp((g.IO.MousePos.x - bar.Min.x) / std::max(1.0f, bar.GetWidth()), 0.0f, 1.0f);
    if (!readOnly && edit.focusedHandle >= 0 && edit.focusedHandle < n &&
        handles[edit.focusedHandle].forced == internal::ForcedItemState::None)
    {
        const ImGuiID owner = handles[edit.focusedHandle].id;
        ImGui::SetKeyOwner(ImGuiKey_Insert, owner);
        if (g.NavId == owner && ImGui::IsKeyPressed(ImGuiKey_Insert, ImGuiInputFlags_None, owner))
        {
            // The focused stop by its item id: a sort this frame may have moved it.
            int at = 0;
            for (int k = 0; k < n; ++k)
                if (GradientHandleId(id, state.keys[k]) == owner)
                    at = k;
            int place = 0;
            for (int k = 0; k < n; ++k)
                if (order[k] == at)
                    place = k;
            const float here = std::clamp(data[at].position, 0.0f, 1.0f);
            if (place + 1 < n)
                addAt = (here + std::clamp(data[order[place + 1]].position, 0.0f, 1.0f)) * 0.5f;
            else if (place > 0)
                addAt = (here + std::clamp(data[order[place - 1]].position, 0.0f, 1.0f)) * 0.5f;
            else
                addAt = here < 0.5f ? (here + 1.0f) * 0.5f : here * 0.5f;
        }
    }
    if (addAt >= 0.0f && n < kGradientMaxStops)
    {
        if (n >= capacity && grow != nullptr)
            grow(growUser, &data, &capacity, n);
        if (n < capacity && data != nullptr)
        {
            GradientStop stop;
            stop.position = addAt;
            stop.color = GradientColorAt(data, order, n, addAt);
            if (!options.alpha)
                stop.color.w = 1.0f;
            const ImGuiID key = state.nextKey++;
            data[n] = stop;
            state.keys[n] = key;
            ++n;
            GradientSortStops(data, state.keys, n);
            ImGui::SetFocusID(GradientHandleId(id, key), window);
            written = true;
        }
    }

    // Removing (never the last stop). The focus goes to the stop that took its place.
    if (removeKey != 0 && n > 1 && !readOnly)
    {
        const int at = GradientIndexOf(state, n, removeKey);
        if (at >= 0)
        {
            n = GradientRemoveStop(data, state.keys, n, at);
            if (state.popoverKey == removeKey)
                state.popoverKey = 0;
            written = true;
            ImGui::SetFocusID(GradientHandleId(id, state.keys[std::min(at, n - 1)]), window);
        }
    }
    state.count = n;

    // The popover of a stop. It holds a title and a row of controls under the picker, so it takes the
    // padding of a panel (spacing.md) rather than the slim one of a list.
    const ImGuiID level = ImHashStr("##stop", 0, id);
    internal::PopupOptions popup;
    popup.padding = ImVec2(Px(t.spacing.md), Px(t.spacing.md));
    const bool canOpen = !disabled && barItem.forced == internal::ForcedItemState::None;
    int forcedOpen = -1;
    for (int i = 0; i < n; ++i)
        if (handles[i].open)
            forcedOpen = i;
    if (openKey != 0 && canOpen)
    {
        const int at = GradientIndexOf(state, n, openKey);
        if (at >= 0)
        {
            state.popoverKey = openKey;
            state.before = data[at].color;
            internal::OpenPopupLevel(level, GradientStopAnchor(track, data[at].position, stopSize), popup);
        }
    }
    if (forcedOpen >= 0)
        state.popoverKey = state.keys[forcedOpen];
    int popoverAt = state.popoverKey != 0 ? GradientIndexOf(state, n, state.popoverKey) : -1;
    if (popoverAt < 0 && internal::IsPopupLevelOpen(level))
        internal::ClosePopupLevel(level);
    if (popoverAt >= 0)
    {
        const internal::PopupAnchor anchor = GradientStopAnchor(track, data[popoverAt].position, stopSize);
        if (forcedOpen >= 0)
        {
            state.before = data[popoverAt].color;
            internal::ForcePopupLevelOpen(level, anchor, popup);
        }
        internal::SetPopupLevelAnchor(level, anchor);
        if (internal::BeginPopupLevel(level))
        {
            Text(TextStyle::Headline, "%s", strings.colorStop);
            ColorPickerOptions picker;
            picker.alpha = options.alpha;
            picker.original = &state.before;
            picker.width = t.metrics.pickerWidth;
            picker.readOnly = readOnly;
            ImVec4 color = data[popoverAt].color;
            if (ColorPicker("##color", &color, picker) && !readOnly)
            {
                data[popoverAt].color = color;
                written = true;
            }
            float percent = std::clamp(data[popoverAt].position, 0.0f, 1.0f) * 100.0f;
            NumberInputOptions number;
            number.format = "%.0f";
            number.min = 0.0;
            number.max = 100.0;
            number.step = 1.0;
            number.unit = "%";
            number.width = t.metrics.pickerWidth;
            number.readOnly = readOnly;
            if (NumberInput(strings.position, &percent, number) && !readOnly)
            {
                data[popoverAt].position = std::clamp(percent / 100.0f, 0.0f, 1.0f);
                GradientSortStops(data, state.keys, n);
                popoverAt = GradientIndexOf(state, n, state.popoverKey);
                written = true;
            }
            ImGui::BeginDisabled(readOnly || n <= 1);
            if (Button(strings.remove, {.variant = ButtonVariant::Destructive}) && !readOnly && n > 1)
            {
                n = GradientRemoveStop(data, state.keys, n, popoverAt);
                state.count = n;
                state.popoverKey = 0;
                written = true;
                internal::ClosePopupLevel(level);
            }
            ImGui::EndDisabled();
            internal::EndPopupLevel();
        }
        if (internal::PopupLevelClosedReason(level) == internal::PopupCloseReason::Escape && !readOnly)
        {
            const int at = GradientIndexOf(state, n, state.popoverKey);
            if (at >= 0 && !internal::ColorSame(data[at].color, state.before, true))
            {
                data[at].color = state.before;
                written = true;
            }
        }
        if (!internal::IsPopupLevelOpen(level) && forcedOpen < 0)
            state.popoverKey = 0;
    }
    GradientOrder(data, n, order);
    state.bar = bar;
    state.drawn = n;
    for (int i = 0; i < n; ++i)
        state.stops[i] = GradientStopAnchor(track, data[i].position, stopSize).rect;

    // ---- Draw ----
    ImDrawList* drawList = window->DrawList;
    if (ImGui::IsRectVisible(frame.Min, frame.Max))
    {
        const float radius = Px(t.radii.element);
        const float share = disabled ? internal::kColorDisabledOpacity : 1.0f;
        if (options.alpha)
            FillCheckerboard(drawList, bar.Min, bar.Max, ToU32(WithAlpha(c.checker[0], c.checker[0].w * share)),
                             ToU32(WithAlpha(c.checker[1], c.checker[1].w * share)), Px(t.metrics.checkerCell), radius);
        DrawGradientBar(drawList, bar, radius, data, order, n, !options.alpha, share);
        StrokeRect(drawList, bar.Min, bar.Max, ToU32(disabled ? c.labelQuaternary : c.separator), radius,
                   HairlineThickness());

        // Markers, the active or focused one on top.
        int top = edit.activeHandle >= 0 ? edit.activeHandle : edit.focusedHandle;
        if (top >= n)
            top = -1;
        const ImVec4 boundary = ControlBoundaryColor(c);
        const float hairline = HairlineThickness();
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < n; ++i)
            {
                if ((pass == 1) != (i == top))
                    continue;
                const ImRect rect = GradientStopAnchor(track, data[i].position, stopSize).rect;
                // The item states follow the keys; after a sort or an add they belong to the stop with
                // the same key.
                const internal::ItemState* h = nullptr;
                for (int k = 0; k < n; ++k)
                    if (handles[k].id == GradientHandleId(id, state.keys[i]))
                        h = &handles[k];
                const float hover = h != nullptr ? std::max(h->hoverAmount, h->pressAmount) : 0.0f;
                const float press = h != nullptr ? h->pressAmount : 0.0f;
                const float focus = h != nullptr ? h->focusAmount : 0.0f;
                const bool going = state.removeArmed && state.pressKey == state.keys[i];
                const ImVec4 color = options.alpha ? data[i].color : WithAlpha(data[i].color, 1.0f);
                if (disabled || going)
                {
                    const float keep = disabled ? internal::kColorDisabledOpacity : kGradientRemoveOpacity;
                    FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(internal::ColorOnChecker(color), keep)), radius);
                }
                else
                {
                    internal::DrawColorSwatch(drawList, rect, color, radius, options.alpha);
                }
                // The stop whose popover is open keeps an accent outline (focusRing, 3:1 on every
                // surface), so it is clear which stop the picker edits.
                const bool chosen = state.popoverKey == state.keys[i];
                const ImVec4 outline = disabled ? c.labelTertiary : (chosen ? c.focusRing : Mix(boundary, c.label, hover));
                StrokeRect(drawList, rect.Min, rect.Max, ToU32(outline), radius,
                           chosen ? hairline * 2.0f : hairline * (1.0f + press));
                internal::DrawFocusRing(drawList, rect, radius, focus);
            }
        }
    }

    // The label is not drawn beside the editor: it is the tooltip on hover and keyboard focus.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    bool focusVisible = false;
    for (int i = 0; i < n; ++i)
        focusVisible = focusVisible || handles[i].focusVisible;
    // Only a real pointer counts for the tooltip: a hover forced by a capture has no pointer to follow.
    const bool hovered = (barItem.hovered && barItem.forced == internal::ForcedItemState::None) ||
                         (edit.track.hovered && edit.track.forced == internal::ForcedItemState::None);
    internal::CutTextTooltip(id, frame, hovered, focusVisible, label, labelEnd,
                             labelEnd != label);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (written)
        internal::MarkWidgetEdited(id);
    if (written && count != nullptr)
        *count = n;
    return written;
}

} // namespace

namespace internal
{

ImRect GradientEditorBarRect(ImGuiID id)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return ImRect();
    return GetState<GradientEditorState>(id).bar;
}

ImRect GradientEditorStopRect(ImGuiID id, int index, ImGuiID* itemId)
{
    if (itemId != nullptr)
        *itemId = 0;
    if (ImGui::GetCurrentContext() == nullptr)
        return ImRect();
    const GradientEditorState& state = GetState<GradientEditorState>(id);
    if (index < 0 || index >= state.drawn)
        return ImRect();
    if (itemId != nullptr)
        *itemId = GradientHandleId(id, state.keys[index]);
    return state.stops[index];
}

ImGuiID GradientEditorPopoverLevel(ImGuiID id)
{
    return ImHashStr("##stop", 0, id);
}

} // namespace internal

bool GradientEditor(const char* label, std::span<GradientStop> stops, int* count, const GradientEditorOptions& options)
{
    return GradientEditorImpl(label, stops.data(), internal::EditorsCapacity(stops.size()), count, nullptr, nullptr, options);
}

bool GradientEditor(const char* label, std::vector<GradientStop>* stops, const GradientEditorOptions& options)
{
    if (stops == nullptr)
        return GradientEditorImpl(label, nullptr, 0, nullptr, nullptr, nullptr, options);
    int count = internal::EditorsCapacity(stops->size());
    stops->resize(stops->capacity());  // within the capacity: no allocation
    const bool written = GradientEditorImpl(label, stops->data(), internal::EditorsCapacity(stops->size()), &count,
                                            &internal::EditorsGrowVector<GradientStop>, stops, options);
    stops->resize(static_cast<std::size_t>(std::max(0, count)));
    return written;
}

} // namespace facet
