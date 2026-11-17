// Multi-handle model (multi_handle.h): constraint solving, picking, draw order and the items of
// a widget with several handles on one track.

#include "internal/multi_handle.h"

#include <algorithm>
#include <cmath>

#include "facet/core/anim.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet::internal
{
namespace
{

// Positions closer than this many pixels count as the same position when picking.
constexpr float kSamePosition = 0.5f;
// Animation sub key of the hover amount of a handle (the track takes the pointer for all of
// them, so the handle's own hover amount stays 0).
constexpr int kHandleHoverSub = kItemAnimSubBase + 8;
// Bisection steps for the furthest logarithmic span shift that keeps the minimum gap: halves
// the fraction each step, far below a pixel after this many.
constexpr int kGapBisections = 40;

// The values a cancel restores, taken when an edit of a widget's part begins.
struct EditSnapshot
{
    ImGuiID owner = 0;
    int count = 0;
    double values[kMaxHandles] = {};
};

EditSnapshot& Snapshot()
{
    static EditSnapshot snapshot;
    return snapshot;
}

// The drag of a span.
struct SpanDrag
{
    ImGuiID id = 0;
    DragTracker tracker;
    ImVec2 lastMouse;
};

SpanDrag& CurrentSpanDrag()
{
    static SpanDrag drag;
    return drag;
}

// A press on a run of handles at one position: which of them moves depends on the direction
// of the drag, so the track keeps the press until the pointer moves along the track.
struct PendingPick
{
    ImGuiID owner = 0;
    int first = 0;        // the run, as handle indices
    int last = 0;
    int fallback = 0;     // the handle that gets the focus when the press ends without moving
    float pressAlong = 0; // the press position along the track, pixels
};

PendingPick& CurrentPendingPick()
{
    static PendingPick pick;
    return pick;
}

double ValueEpsilon(const HandleConstraints& c)
{
    return std::max(1e-12, (c.max - c.min) * 1e-9);
}

void TakeSnapshot(ImGuiID owner, const double* values, int count)
{
    EditSnapshot& snapshot = Snapshot();
    snapshot.owner = owner;
    snapshot.count = count;
    for (int i = 0; i < count; ++i)
        snapshot.values[i] = values[i];
}

bool RestoreSnapshot(ImGuiID owner, std::span<double> values)
{
    const EditSnapshot& snapshot = Snapshot();
    if (snapshot.owner != owner || snapshot.count != static_cast<int>(values.size()))
        return false;
    bool changed = false;
    for (int i = 0; i < snapshot.count; ++i)
    {
        changed = changed || values[static_cast<size_t>(i)] != snapshot.values[i];
        values[static_cast<size_t>(i)] = snapshot.values[i];
    }
    return changed;
}

// The values handle `index` can take by itself, the way MoveHandle resolves the constraints: so
// the value drag core never writes a value that MoveHandle would move back (an edit that cannot
// move then ends without an edit).
void HandleLimits(std::span<const double> values, int index, const HandleConstraints& c, double* low, double* high)
{
    const int n = static_cast<int>(values.size());
    const double gap = EffectiveGap(c, n);
    const auto at = [&](int i) { return values[static_cast<size_t>(i)]; };
    if (c.mirror && n == 2)
    {
        const double center = std::clamp(c.center, c.min, c.max);
        *low = index == 0 ? std::max(c.min, 2.0 * center - c.max) : center + gap * 0.5;
        *high = index == 0 ? center - gap * 0.5 : std::min(c.max, 2.0 * center - c.min);
    }
    else if (c.collision == CollisionMode::Clamp)
    {
        *low = std::max(c.min, index > 0 ? at(index - 1) + gap : c.min);
        *high = std::min(c.max, index < n - 1 ? at(index + 1) - gap : c.max);
    }
    else
    {
        *low = c.min + gap * index;
        *high = std::max(*low, c.max - gap * (n - 1 - index));
    }
}

// Gives the active item to `id`, as if the mouse press had landed on it.
void TransferMousePress(ImGuiID id, ImGuiWindow* window, bool focus)
{
    ImGuiContext& g = *GImGui;
    ImGui::SetActiveID(id, window);
    g.ActiveIdMouseButton = ImGuiMouseButton_Left;
    ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
    if (focus)
        ImGui::SetFocusID(id, window);
}

} // namespace

void ResetMultiHandleState()
{
    Snapshot() = {};
    CurrentSpanDrag() = {};
    CurrentPendingPick() = {};
}

double EffectiveGap(const HandleConstraints& c, int count)
{
    if (count < 2 || !(c.minGap > 0.0))
        return 0.0;
    const double room = std::max(0.0, c.max - c.min) / static_cast<double>(count - 1);
    return std::min(c.minGap, room);
}

void ConstrainHandles(std::span<double> values, const HandleConstraints& c)
{
    const int n = static_cast<int>(values.size());
    if (n == 0)
        return;
    const double lo = std::min(c.min, c.max);
    const double hi = std::max(c.min, c.max);
    HandleConstraints bounds = c;
    bounds.min = lo;
    bounds.max = hi;
    const double gap = EffectiveGap(bounds, n);
    for (int i = 0; i < n; ++i)
    {
        double& v = values[static_cast<size_t>(i)];
        const double low = lo + gap * i;
        const double high = hi - gap * (n - 1 - i);
        if (std::isnan(v))
            v = low;
        v = std::clamp(v, low, std::max(low, high));
    }
    for (int i = 1; i < n; ++i)
        values[static_cast<size_t>(i)] = std::max(values[static_cast<size_t>(i)], values[static_cast<size_t>(i - 1)] + gap);
    for (int i = n - 1; i >= 0; --i)
    {
        double& v = values[static_cast<size_t>(i)];
        const double high = hi - gap * (n - 1 - i);
        v = std::min(v, high);
        if (i + 1 < n)
            v = std::min(v, values[static_cast<size_t>(i + 1)] - gap);
    }
    if (c.mirror && n == 2)
    {
        // Symmetric around the center: keep the handle nearer to its mirrored place.
        const double center = std::clamp(c.center, lo, hi);
        const double half = std::max(center - values[0], values[1] - center);
        const double maxHalf = std::min(center - lo, hi - center);
        const double clamped = std::clamp(half, gap * 0.5, std::max(gap * 0.5, maxHalf));
        values[0] = center - clamped;
        values[1] = center + clamped;
    }
}

bool MoveHandle(std::span<double> values, int index, double target, const HandleConstraints& c)
{
    const int n = static_cast<int>(values.size());
    if (index < 0 || index >= n || std::isnan(target))
        return false;
    const double gap = EffectiveGap(c, n);
    double before[kMaxHandles];
    const int kept = std::min(n, kMaxHandles);
    for (int i = 0; i < kept; ++i)
        before[i] = values[static_cast<size_t>(i)];
    const auto at = [&](int i) -> double& { return values[static_cast<size_t>(i)]; };

    if (c.mirror && n == 2)
    {
        const double center = std::clamp(c.center, c.min, c.max);
        if (index == 0)
        {
            const double low = std::max(c.min, 2.0 * center - c.max);
            const double high = center - gap * 0.5;
            if (low > high)
                return false;
            at(0) = std::clamp(target, low, high);
            at(1) = 2.0 * center - at(0);
        }
        else
        {
            const double low = center + gap * 0.5;
            const double high = std::min(c.max, 2.0 * center - c.min);
            if (low > high)
                return false;
            at(1) = std::clamp(target, low, high);
            at(0) = 2.0 * center - at(1);
        }
    }
    else if (c.collision == CollisionMode::Clamp)
    {
        const double low = std::max(c.min, index > 0 ? at(index - 1) + gap : c.min);
        const double high = std::min(c.max, index < n - 1 ? at(index + 1) - gap : c.max);
        if (low > high)
            return false;
        at(index) = std::clamp(target, low, high);
    }
    else
    {
        const double low = c.min + gap * index;
        const double high = c.max - gap * (n - 1 - index);
        at(index) = std::clamp(target, low, std::max(low, high));
        for (int j = index + 1; j < n; ++j)
            at(j) = std::min(std::max(at(j), at(j - 1) + gap), c.max - gap * (n - 1 - j));
        for (int j = index - 1; j >= 0; --j)
            at(j) = std::max(std::min(at(j), at(j + 1) - gap), c.min + gap * j);
    }

    for (int i = 0; i < kept; ++i)
    {
        if (before[i] != values[static_cast<size_t>(i)])
            return true;
    }
    return false;
}

bool MoveSpan(std::span<double> values, int first, int last, double delta, const HandleConstraints& c)
{
    const int n = static_cast<int>(values.size());
    first = std::max(0, first);
    last = std::min(n - 1, last);
    if (first > last || std::isnan(delta) || delta == 0.0 || (c.mirror && n == 2))
        return false;
    const double gap = EffectiveGap(c, n);
    const auto at = [&](int i) -> double& { return values[static_cast<size_t>(i)]; };
    const double lowBound = first > 0 ? at(first - 1) + gap : c.min;
    const double highBound = last < n - 1 ? at(last + 1) - gap : c.max;
    const double lowDelta = lowBound - at(first);
    const double highDelta = highBound - at(last);
    if (lowDelta > highDelta)
        return false;
    const double d = std::clamp(delta, std::min(0.0, lowDelta), std::max(0.0, highDelta));
    if (d == 0.0)
        return false;
    for (int i = first; i <= last; ++i)
        at(i) += d;
    // Exactly on the bound that stopped the run.
    if (d == lowDelta)
        at(first) = lowBound;
    if (d == highDelta)
        at(last) = highBound;
    return true;
}

int InsertHandle(std::span<double> storage, int count, double value, const HandleConstraints& c)
{
    count = std::clamp(count, 0, static_cast<int>(storage.size()));
    if (count >= static_cast<int>(storage.size()) || count >= kMaxHandles || std::isnan(value))
        return -1;
    value = std::clamp(value, c.min, c.max);
    const double gap = EffectiveGap(c, count + 1);
    int index = 0;
    while (index < count && storage[static_cast<size_t>(index)] <= value)
        ++index;
    if (index > 0 && value - storage[static_cast<size_t>(index - 1)] < gap)
        return -1;
    if (index < count && storage[static_cast<size_t>(index)] - value < gap)
        return -1;
    for (int i = count; i > index; --i)
        storage[static_cast<size_t>(i)] = storage[static_cast<size_t>(i - 1)];
    storage[static_cast<size_t>(index)] = value;
    return index;
}

int RemoveHandle(std::span<double> values, int count, int index, int minCount)
{
    count = std::clamp(count, 0, static_cast<int>(values.size()));
    if (count <= std::max(0, minCount) || index < 0 || index >= count)
        return count;
    for (int i = index; i + 1 < count; ++i)
        values[static_cast<size_t>(i)] = values[static_cast<size_t>(i + 1)];
    return count - 1;
}

int PickHandle(std::span<const float> positions, float pointer, float trackStart, float trackEnd, int preferred)
{
    const int n = static_cast<int>(positions.size());
    if (n == 0)
        return -1;
    int best = 0;
    float bestDistance = std::fabs(positions[0] - pointer);
    for (int i = 1; i < n; ++i)
    {
        const float distance = std::fabs(positions[static_cast<size_t>(i)] - pointer);
        if (distance <= bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    // The run of handles at the same position as the best one.
    const float at = positions[static_cast<size_t>(best)];
    int first = best;
    while (first > 0 && std::fabs(positions[static_cast<size_t>(first - 1)] - at) <= kSamePosition)
        --first;
    int last = best;
    while (last + 1 < n && std::fabs(positions[static_cast<size_t>(last + 1)] - at) <= kSamePosition)
        ++last;
    if (first == last)
        return best;
    if (pointer > at + kSamePosition)
        return last;
    if (pointer < at - kSamePosition)
        return first;
    if (std::fabs(at - trackEnd) <= kSamePosition)
        return first;
    if (std::fabs(at - trackStart) <= kSamePosition)
        return last;
    if (preferred >= first && preferred <= last)
        return preferred;
    return last;
}

int PickHandle2D(std::span<const ImVec2> positions, ImVec2 pointer, float maxDistance)
{
    int best = -1;
    float bestDistance = maxDistance * maxDistance;
    for (int i = 0; i < static_cast<int>(positions.size()); ++i)
    {
        const ImVec2 d(positions[static_cast<size_t>(i)].x - pointer.x, positions[static_cast<size_t>(i)].y - pointer.y);
        const float distance = d.x * d.x + d.y * d.y;
        if (distance <= bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

void HandleDrawOrder(std::span<const double> values, const HandleConstraints& c, int top, std::span<int> order)
{
    const int n = std::min(static_cast<int>(values.size()), static_cast<int>(order.size()));
    const double eps = ValueEpsilon(c);
    int written = 0;
    const auto push = [&](int i) {
        if (i != top)
            order[static_cast<size_t>(written++)] = i;
    };
    // Handles inside the range in index order, then those at the start (the last one ends on
    // top), then those at the end in reverse (the first one ends on top), then `top`.
    for (int i = 0; i < n; ++i)
    {
        const double v = values[static_cast<size_t>(i)];
        if (v > c.min + eps && v < c.max - eps)
            push(i);
    }
    for (int i = 0; i < n; ++i)
    {
        if (values[static_cast<size_t>(i)] <= c.min + eps)
            push(i);
    }
    for (int i = n - 1; i >= 0; --i)
    {
        const double v = values[static_cast<size_t>(i)];
        if (v >= c.max - eps && v > c.min + eps)
            push(i);
    }
    if (top >= 0 && top < n)
        order[static_cast<size_t>(written++)] = top;
}

ImRect HandleRect(const ImRect& track, Orientation orientation, const ValueMapping& mapping, double value, float size)
{
    const float along = FractionToTrack(track, orientation, ValueToFraction(mapping, value));
    const float half = size * 0.5f;
    if (orientation == Orientation::Vertical)
    {
        const float x = (track.Min.x + track.Max.x) * 0.5f;
        return ImRect(x - half, along - half, x + half, along + half);
    }
    const float y = (track.Min.y + track.Max.y) * 0.5f;
    return ImRect(along - half, y - half, along + half, y + half);
}

MultiHandleResult MultiHandleBehavior(ImGuiID id, const ImRect& widgetRect, const ImRect& track,
                                      std::span<double> values, std::span<ItemState> handles,
                                      const MultiHandleOptions& options)
{
    MultiHandleResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    const int n = std::min({static_cast<int>(values.size()), static_cast<int>(handles.size()), kMaxHandles});
    if (context == nullptr || n == 0)
        return result;
    ImGuiContext& g = *context;
    ImGuiWindow* window = g.CurrentWindow;
    const std::span<double> v = values.first(static_cast<size_t>(n));

    const ValueMapping mapping = NormalizeMapping(options.drag.mapping);
    HandleConstraints constraints = options.constraints;
    constraints.min = mapping.min;
    constraints.max = mapping.max;
    ConstrainHandles(v, constraints);
    double before[kMaxHandles];
    for (int i = 0; i < n; ++i)
        before[i] = v[static_cast<size_t>(i)];

    const Orientation orientation = options.drag.orientation;
    const bool vertical = orientation == Orientation::Vertical;
    const float size = options.handleSize > 0.0f ? options.handleSize : Px(T().metrics.sliderKnobSize);
    const float length = std::max(1.0f, vertical ? track.GetHeight() : track.GetWidth());
    const bool spanEnabled = options.spanDrag && n >= 2 && !(constraints.mirror && n == 2);
    const ForcedItemRequest forced = TakeForcedItemState();

    ImGui::PushOverrideID(id);
    ImGuiID handleIds[kMaxHandles];
    for (int i = 0; i < n; ++i)
        handleIds[i] = ImGui::GetID(i);
    const ImGuiID spanId = ImGui::GetID("##span");
    const ImGuiID trackId = ImGui::GetID("##track");
    ImGui::PopID();

    // Handle positions along the track in pixels, growing with the value.
    float positions[kMaxHandles];
    for (int i = 0; i < n; ++i)
        positions[i] = static_cast<float>(ValueToFraction(mapping, v[static_cast<size_t>(i)])) * length;

    ImGui::ItemSize(widgetRect.GetSize());

    // The track takes every mouse press and hands it to the part it picks.
    ItemOptions trackOptions;
    trackOptions.press = PressMode::Click;
    trackOptions.navigable = false;
    trackOptions.part = -2;
    trackOptions.forced = &forced;
    result.track = PartBehavior(trackId, widgetRect, trackOptions);
    const bool readOnly = options.drag.readOnly || result.track.readOnly;

    int focusedIndex = -1;
    for (int i = 0; i < n; ++i)
    {
        if (g.NavId == handleIds[i])
            focusedIndex = i;
    }

    const float pointer = static_cast<float>(TrackToFraction(track, orientation, g.IO.MousePos)) * length;
    int pick = -1;
    bool pickSpan = false;
    bool onHandle = false;
    if (result.track.hovered || result.track.pressed)
    {
        pick = PickHandle(std::span<const float>(positions, static_cast<size_t>(n)), pointer, 0.0f, length, focusedIndex);
        onHandle = std::fabs(positions[pick] - pointer) <= size * 0.5f;
        pickSpan = !onHandle && spanEnabled && pointer > positions[0] && pointer < positions[n - 1];
    }
    PendingPick& pending = CurrentPendingPick();
    if (result.track.pressed && !result.track.disabled && !readOnly && result.track.forced == ForcedItemState::None)
    {
        TakeSnapshot(id, before, n);
        // A press on handles at one position (a press beside them moves the one PickHandle
        // chose to the pointer at once).
        int first = pick;
        int last = pick;
        if (!pickSpan && onHandle)
        {
            while (first > 0 && std::fabs(positions[first - 1] - positions[pick]) <= kSamePosition)
                --first;
            while (last + 1 < n && std::fabs(positions[last + 1] - positions[pick]) <= kSamePosition)
                ++last;
        }
        if (pickSpan)
            TransferMousePress(spanId, window, false);
        else if (first != last)
            pending = {id, first, last, pick, pointer};
        else
            TransferMousePress(handleIds[pick], window, true);
    }
    else if (pending.owner == id)
    {
        if (g.ActiveId == trackId)
        {
            // The first movement along the track decides: toward max the last handle of the run
            // moves, toward min the first, so the one that can move in that direction takes it.
            const float moved = pointer - pending.pressAlong;
            if (std::fabs(moved) > kSamePosition)
            {
                const int chosen = moved > 0.0f ? pending.last : pending.first;
                pending = {};
                TransferMousePress(handleIds[chosen], window, true);
            }
        }
        else
        {
            // Released without moving: the handle the press picked gets the focus.
            if (g.ActiveId == 0 && pending.fallback < n)
                ImGui::SetFocusID(handleIds[pending.fallback], window);
            pending = {};
        }
    }
    const bool partActive = g.ActiveId == spanId || std::find(handleIds, handleIds + n, g.ActiveId) != handleIds + n;
    if (result.track.hovered && !partActive)
    {
        result.hoveredHandle = pickSpan ? -1 : pick;
        result.spanHovered = pickSpan;
    }

    // Handles.
    ValueDragOptions drag = options.drag;
    drag.mapping = mapping;
    drag.grabRadius = size * 0.5f;
    for (int i = 0; i < n; ++i)
    {
        ItemOptions handleOptions;
        handleOptions.press = PressMode::Click;
        handleOptions.hitTarget = false;
        handleOptions.part = i;
        handleOptions.forced = &forced;
        handleOptions.readOnly = options.drag.readOnly;
        const ImRect rect = HandleRect(track, orientation, mapping, v[static_cast<size_t>(i)], size);
        ItemState& handle = handles[static_cast<size_t>(i)];
        const bool wasActive = g.ActiveId == handleIds[i];
        handle = PartBehavior(handleIds[i], rect, handleOptions);
        double value = v[static_cast<size_t>(i)];
        HandleLimits(v, i, constraints, &drag.clampLow, &drag.clampHigh);
        const ValueDragResult edit = ValueDragBehavior(handle, &value, track, drag);
        // A key press started an edit of this handle: remember where every handle was.
        if (!wasActive && g.ActiveId == handleIds[i])
            TakeSnapshot(id, before, n);
        if (edit.canceled)
        {
            RestoreSnapshot(id, v);
            result.canceled = true;
            result.changed = true;
        }
        else if (edit.changed)
        {
            result.changed = MoveHandle(v, i, value, constraints) || result.changed;
        }
        if (g.ActiveId == handleIds[i])
            result.activeHandle = i;
        if (handle.focused)
            result.focusedHandle = i;
        handle.hovered = handle.hovered || i == result.hoveredHandle;
        handle.hoverAmount = std::max(handle.hoverAmount,
                                      Tween(handleIds[i], kHandleHoverSub, i == result.hoveredHandle ? 1.0f : 0.0f,
                                            T().motion.durationFast));
        if (options.removeKeys && handle.focused && g.NavId == handleIds[i] && !readOnly &&
            handle.forced == ForcedItemState::None)
        {
            ImGui::SetKeyOwner(ImGuiKey_Delete, handleIds[i]);
            ImGui::SetKeyOwner(ImGuiKey_Backspace, handleIds[i]);
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, ImGuiInputFlags_None, handleIds[i]) ||
                ImGui::IsKeyPressed(ImGuiKey_Backspace, ImGuiInputFlags_None, handleIds[i]))
                result.removeRequested = i;
        }
    }

    // The span between the first and the last handle.
    if (spanEnabled)
    {
        const float first = vertical ? track.Max.y - positions[0] : track.Min.x + positions[0];
        const float last = vertical ? track.Max.y - positions[n - 1] : track.Min.x + positions[n - 1];
        const float half = size * 0.5f;
        ImRect spanRect;
        if (vertical)
        {
            const float x = (track.Min.x + track.Max.x) * 0.5f;
            spanRect = ImRect(x - half, std::min(first, last), x + half, std::max(first, last));
        }
        else
        {
            const float y = (track.Min.y + track.Max.y) * 0.5f;
            spanRect = ImRect(std::min(first, last), y - half, std::max(first, last), y + half);
        }
        ItemOptions spanOptions;
        spanOptions.press = PressMode::Click;
        spanOptions.navigable = false;
        spanOptions.hitTarget = false;
        spanOptions.part = n;
        spanOptions.forced = &forced;
        result.span = PartBehavior(spanId, spanRect, spanOptions);

        SpanDrag& spanDrag = CurrentSpanDrag();
        if (spanDrag.id == spanId && g.ActiveId != spanId)
            spanDrag = {};
        if (g.ActiveId == spanId && result.span.held && result.span.forced == ForcedItemState::None)
        {
            const ImVec2 mouse = g.IO.MousePos;
            if (spanDrag.id != spanId)
            {
                spanDrag = {};
                spanDrag.id = spanId;
                spanDrag.tracker.Begin(0.0, length);
                spanDrag.lastMouse = mouse;
            }
            else
            {
                const double delta = vertical ? -(mouse.y - spanDrag.lastMouse.y) : mouse.x - spanDrag.lastMouse.x;
                spanDrag.lastMouse = mouse;
                spanDrag.tracker.Move(delta, g.IO.KeyShift ? kFineDragFactor : 1.0);
                // Always from the values at the start of the drag, so nothing accumulates.
                double moved[kMaxHandles];
                const EditSnapshot& snapshot = Snapshot();
                const bool haveStart = snapshot.owner == id && snapshot.count == n;
                for (int i = 0; i < n; ++i)
                    moved[i] = haveStart ? snapshot.values[i] : v[static_cast<size_t>(i)];
                const std::span<double> movedSpan(moved, static_cast<size_t>(n));
                const double fraction = spanDrag.tracker.Fraction();
                if (mapping.scale == ValueScale::Logarithmic)
                {
                    // Even on screen: every handle moves by the same fraction of the track. The
                    // gaps in value units then shrink toward min, so the shift stops where the
                    // smallest gap would fall below the minimum (found by bisection: the steps
                    // of a stepped mapping make it uneven).
                    double f[kMaxHandles];
                    double start[kMaxHandles];
                    for (int i = 0; i < n; ++i)
                    {
                        start[i] = moved[i];
                        f[i] = ValueToFraction(mapping, moved[i]);
                    }
                    const double gap = EffectiveGap(constraints, n);
                    const double eps = ValueEpsilon(constraints);
                    const auto place = [&](double shift) {
                        for (int i = 0; i < n; ++i)
                            moved[i] = shift == 0.0 ? start[i] : FractionToValue(mapping, f[i] + shift);
                    };
                    const auto keepsGap = [&]() {
                        for (int i = 1; i < n; ++i)
                        {
                            if (moved[i] - moved[i - 1] < gap - eps)
                                return false;
                        }
                        return true;
                    };
                    const double d = std::clamp(fraction, -f[0], 1.0 - f[n - 1]);
                    place(d);
                    if (!keepsGap())
                    {
                        double good = 0.0;
                        double bad = d;
                        for (int k = 0; k < kGapBisections; ++k)
                        {
                            const double middle = (good + bad) * 0.5;
                            place(middle);
                            if (keepsGap())
                                good = middle;
                            else
                                bad = middle;
                        }
                        place(good);
                    }
                }
                else
                {
                    double valueDelta = fraction * (mapping.max - mapping.min);
                    if (mapping.step > 0.0)
                        valueDelta = std::round(valueDelta / mapping.step) * mapping.step;
                    MoveSpan(movedSpan, 0, n - 1, valueDelta, constraints);
                }
                bool changed = false;
                for (int i = 0; i < n; ++i)
                {
                    changed = changed || moved[i] != v[static_cast<size_t>(i)];
                    v[static_cast<size_t>(i)] = moved[i];
                }
                if (changed)
                {
                    result.changed = true;
                    ImGui::MarkItemEdited(spanId);
                }
            }
            ImGui::SetKeyOwner(ImGuiKey_Escape, spanId);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, spanId))
            {
                RestoreSnapshot(id, v);
                MarkEditCanceled(spanId);
                ImGui::MarkItemEdited(spanId);
                ImGui::ClearActiveID();
                spanDrag = {};
                result.canceled = true;
                result.changed = true;
            }
        }
        result.spanActive = g.ActiveId == spanId;
    }

    // Report a change only when the values really differ from the frame's start.
    if (result.changed && !result.canceled)
    {
        bool differs = false;
        for (int i = 0; i < n; ++i)
            differs = differs || before[i] != v[static_cast<size_t>(i)];
        result.changed = differs;
    }
    return result;
}

} // namespace facet::internal
