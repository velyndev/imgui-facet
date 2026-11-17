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
    ImGuiID keys[kMaxHandles] = {};  // the handles' keys, all 0 without keys
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
    bool pointerLost = false;  // the mouse position was invalid on a frame of the drag
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

// Writes into `order` the indices of `positions` sorted ascending; equal positions keep their
// index order. At most kMaxHandles entries, no allocation.
template <typename T>
void SortByPosition(std::span<const T> positions, int* order)
{
    const int n = std::min(static_cast<int>(positions.size()), kMaxHandles);
    for (int i = 0; i < n; ++i)
    {
        int j = i;
        while (j > 0 && positions[static_cast<size_t>(order[j - 1])] > positions[static_cast<size_t>(i)])
        {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = i;
    }
}

bool Mirrored(const HandleConstraints& c, int count)
{
    return c.mirror && count == 2 && c.collision != CollisionMode::Free;
}

// The key of handle `index`, 0 without keys.
ImGuiID KeyOf(std::span<const ImGuiID> keys, int index)
{
    return index < static_cast<int>(keys.size()) ? keys[static_cast<size_t>(index)] : 0;
}

void TakeSnapshot(ImGuiID owner, const double* values, int count, std::span<const ImGuiID> keys)
{
    EditSnapshot& snapshot = Snapshot();
    snapshot.owner = owner;
    snapshot.count = count;
    for (int i = 0; i < count; ++i)
    {
        snapshot.values[i] = values[i];
        snapshot.keys[i] = KeyOf(keys, i);
    }
}

// Restores the values of the snapshot when it belongs to this widget and the same handles (a
// handle added or removed during the edit makes it stale).
bool RestoreSnapshot(ImGuiID owner, std::span<double> values, std::span<const ImGuiID> keys)
{
    const EditSnapshot& snapshot = Snapshot();
    if (snapshot.owner != owner || snapshot.count != static_cast<int>(values.size()))
        return false;
    for (int i = 0; i < snapshot.count; ++i)
    {
        if (snapshot.keys[i] != KeyOf(keys, i))
            return false;
    }
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
    if (Mirrored(c, n))
    {
        const double center = std::clamp(c.center, c.min, c.max);
        *low = index == 0 ? std::max(c.min, 2.0 * center - c.max) : center + gap * 0.5;
        *high = index == 0 ? center - gap * 0.5 : std::min(c.max, 2.0 * center - c.min);
    }
    else if (c.collision == CollisionMode::Free)
    {
        *low = c.min;
        *high = c.max;
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
    if (count < 2 || !(c.minGap > 0.0) || c.collision == CollisionMode::Free)
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
    if (c.collision == CollisionMode::Free)
    {
        // No order and no gap: only the range.
        for (double& v : values)
            v = std::isnan(v) ? lo : std::clamp(v, lo, hi);
        return;
    }
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

    if (Mirrored(c, n))
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
    else if (c.collision == CollisionMode::Free)
    {
        at(index) = std::clamp(target, c.min, c.max);
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
    if (first > last || std::isnan(delta) || delta == 0.0 || Mirrored(c, n) || c.collision == CollisionMode::Free)
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
    if (c.collision == CollisionMode::Free)
    {
        // No order to keep: the new handle goes last.
        storage[static_cast<size_t>(count)] = value;
        return count;
    }
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
    const int n = std::min({static_cast<int>(values.size()), static_cast<int>(order.size()), kMaxHandles});
    const double eps = ValueEpsilon(c);
    // Handles by position (index order among equal values), so the rules below also hold for
    // free handles whose values are in any order.
    int byValue[kMaxHandles];
    SortByPosition(values.first(static_cast<size_t>(n)), byValue);
    int written = 0;
    const auto push = [&](int i) {
        if (i != top)
            order[static_cast<size_t>(written++)] = i;
    };
    // Handles inside the range by position, then those at the start (the last one ends on top),
    // then those at the end in reverse (the first one ends on top), then `top`.
    for (int k = 0; k < n; ++k)
    {
        const double v = values[static_cast<size_t>(byValue[k])];
        if (v > c.min + eps && v < c.max - eps)
            push(byValue[k]);
    }
    for (int k = 0; k < n; ++k)
    {
        if (values[static_cast<size_t>(byValue[k])] <= c.min + eps)
            push(byValue[k]);
    }
    for (int k = n - 1; k >= 0; --k)
    {
        const double v = values[static_cast<size_t>(byValue[k])];
        if (v >= c.max - eps && v > c.min + eps)
            push(byValue[k]);
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
    const bool free = constraints.collision == CollisionMode::Free;
    const bool spanEnabled = options.spanDrag && n >= 2 && !Mirrored(constraints, n) && !free;
    const ForcedItemRequest forced = TakeForcedItemState();
    // Keys give each handle its identity; without them the index does.
    const std::span<const ImGuiID> keys =
        options.keys.size() >= static_cast<size_t>(n) ? options.keys.first(static_cast<size_t>(n)) : std::span<const ImGuiID>();

    ImGui::PushOverrideID(id);
    ImGuiID handleIds[kMaxHandles];
    for (int i = 0; i < n; ++i)
    {
        const ImGuiID key = KeyOf(keys, i);
        handleIds[i] = keys.empty() ? ImGui::GetID(i) : ImHashData(&key, sizeof(key), id);
    }
    const ImGuiID spanId = ImGui::GetID("##span");
    const ImGuiID trackId = ImGui::GetID("##track");
    ImGui::PopID();

    // Handle positions along the track in pixels, growing with the value, and the handles in
    // the order of their positions (the index order, unless the handles are free).
    float unsorted[kMaxHandles];
    for (int i = 0; i < n; ++i)
        unsorted[i] = static_cast<float>(ValueToFraction(mapping, v[static_cast<size_t>(i)])) * length;
    int byPosition[kMaxHandles];
    SortByPosition(std::span<const float>(unsorted, static_cast<size_t>(n)), byPosition);
    float positions[kMaxHandles];
    for (int k = 0; k < n; ++k)
        positions[k] = unsorted[byPosition[k]];

    ImGui::ItemSize(widgetRect.GetSize());

    // The track takes every mouse press and hands it to the part it picks.
    ItemOptions trackOptions;
    trackOptions.press = PressMode::Click;
    trackOptions.navigable = false;
    trackOptions.part = -2;
    trackOptions.forced = &forced;
    result.track = PartBehavior(trackId, widgetRect, trackOptions);
    const bool readOnly = options.drag.readOnly || result.track.readOnly;

    // The focused handle, as its place in position order (PickHandle prefers it in a stack).
    int focusedSorted = -1;
    for (int k = 0; k < n; ++k)
    {
        if (g.NavId == handleIds[byPosition[k]])
            focusedSorted = k;
    }

    const float pointer = static_cast<float>(TrackToFraction(track, orientation, g.IO.MousePos)) * length;
    int pick = -1;        // handle index
    int pickSorted = -1;  // its place in position order
    bool pickSpan = false;
    bool onHandle = false;
    if (result.track.hovered || result.track.pressed)
    {
        pickSorted = PickHandle(std::span<const float>(positions, static_cast<size_t>(n)), pointer, 0.0f, length, focusedSorted);
        pick = byPosition[pickSorted];
        onHandle = std::fabs(positions[pickSorted] - pointer) <= size * 0.5f;
        pickSpan = !onHandle && spanEnabled && pointer > positions[0] && pointer < positions[n - 1];
    }
    PendingPick& pending = CurrentPendingPick();
    if (result.track.pressed && !result.track.disabled && !readOnly && result.track.forced == ForcedItemState::None)
    {
        TakeSnapshot(id, before, n, keys);
        // A press on handles at one position (a press beside them moves the one PickHandle
        // chose to the pointer at once).
        int first = pickSorted;
        int last = pickSorted;
        if (!pickSpan && onHandle)
        {
            while (first > 0 && std::fabs(positions[first - 1] - positions[pickSorted]) <= kSamePosition)
                --first;
            while (last + 1 < n && std::fabs(positions[last + 1] - positions[pickSorted]) <= kSamePosition)
                ++last;
        }
        if (pickSpan)
            TransferMousePress(spanId, window, false);
        else if (first != last)
            pending = {id, byPosition[first], byPosition[last], pick, pointer};
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
            if (ImGui::IsMousePosValid(&g.IO.MousePos) && std::fabs(moved) > kSamePosition)
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
            TakeSnapshot(id, before, n, keys);
        if (edit.canceled)
        {
            // Every handle back where the edit began; written only when one had moved.
            if (RestoreSnapshot(id, v, keys))
            {
                ImGui::MarkItemEdited(handleIds[i]);
                result.changed = true;
            }
            result.canceled = true;
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
        // The track takes the pointer, so the span reports the hover a press would grab it with.
        result.span.hovered = result.span.hovered || result.spanHovered;
        result.span.hoverAmount = std::max(result.span.hoverAmount, Tween(spanId, kHandleHoverSub,
                                                                          result.spanHovered ? 1.0f : 0.0f,
                                                                          T().motion.durationFast));

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
            else if (!ImGui::IsMousePosValid(&mouse))
            {
                // The pointer is unknown: the span stays and the drag goes on from where it returns.
                spanDrag.pointerLost = true;
            }
            else if (spanDrag.pointerLost)
            {
                spanDrag.pointerLost = false;
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
                if (RestoreSnapshot(id, v, keys))
                {
                    ImGui::MarkItemEdited(spanId);
                    result.changed = true;
                }
                MarkEditCanceled(spanId);
                ImGui::ClearActiveID();
                spanDrag = {};
                result.canceled = true;
            }
        }
        result.spanActive = g.ActiveId == spanId;
    }

    // Report a change only when the values really differ from the frame's start.
    if (result.changed)
    {
        bool differs = false;
        for (int i = 0; i < n; ++i)
            differs = differs || before[i] != v[static_cast<size_t>(i)];
        result.changed = differs;
    }
    return result;
}

} // namespace facet::internal
