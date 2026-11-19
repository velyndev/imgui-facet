// Drag and drop core (drag_drop.h): the pure helpers of ordered zones (where a drop lands, how
// entries move apart, the line and gap rectangles, the move a drop stands for) and the
// indicators owners draw. They keep no state except the springs of DragDropAnimatedPosition.

#include "internal/drag_drop.h"

#include <algorithm>
#include <cfloat>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet::internal
{
namespace
{

// Animation store sub key of the entry positions, under a key made from the zone and entry ids.
constexpr int kEntryPositionSub = 0x5D00;

bool Vertical(Orientation axis)
{
    return axis == Orientation::Vertical;
}

float AxisMin(const ImRect& rect, Orientation axis)
{
    return Vertical(axis) ? rect.Min.y : rect.Min.x;
}

float AxisMax(const ImRect& rect, Orientation axis)
{
    return Vertical(axis) ? rect.Max.y : rect.Max.x;
}

// A rect from a range along the axis and a range across it.
ImRect AxisRect(Orientation axis, float alongMin, float alongMax, float crossMin, float crossMax)
{
    if (Vertical(axis))
        return ImRect(crossMin, alongMin, crossMax, alongMax);
    return ImRect(alongMin, crossMin, alongMax, crossMax);
}

float CrossMin(const ImRect& rect, Orientation axis)
{
    return Vertical(axis) ? rect.Min.x : rect.Min.y;
}

float CrossMax(const ImRect& rect, Orientation axis)
{
    return Vertical(axis) ? rect.Max.x : rect.Max.y;
}

// The gap between two neighbouring slots along the axis, 0 for fewer than two.
float SlotSpacing(std::span<const ImRect> slots, Orientation axis)
{
    if (slots.size() < 2)
        return 0.0f;
    return std::max(0.0f, AxisMin(slots[1], axis) - AxisMax(slots[0], axis));
}

} // namespace

int DragDropInsertIndex(std::span<const ImRect> slots, Orientation axis, ImVec2 point)
{
    if (slots.empty())
        return -1;
    const float at = Vertical(axis) ? point.y : point.x;
    const int count = static_cast<int>(slots.size());
    for (int i = 0; i < count; ++i)
    {
        const float middle = (AxisMin(slots[i], axis) + AxisMax(slots[i], axis)) * 0.5f;
        if (at < middle)
            return i;
    }
    return count;
}

int DragDropInsertIndexWrapped(std::span<const ImRect> slots, Orientation axis, ImVec2 point)
{
    if (slots.empty())
        return -1;
    const Orientation cross = Vertical(axis) ? Orientation::Horizontal : Orientation::Vertical;
    const float along = Vertical(axis) ? point.y : point.x;
    const float across = Vertical(axis) ? point.x : point.y;
    const int count = static_cast<int>(slots.size());
    // Lines: a slot that starts no further along the axis than the one before it begins a new line.
    int bestBegin = 0;
    int bestEnd = 0;
    float bestDistance = FLT_MAX;
    int begin = 0;
    while (begin < count)
    {
        int end = begin + 1;
        while (end < count && AxisMin(slots[end], axis) > AxisMin(slots[end - 1], axis))
            ++end;
        float lineMin = FLT_MAX;
        float lineMax = -FLT_MAX;
        for (int i = begin; i < end; ++i)
        {
            lineMin = std::min(lineMin, AxisMin(slots[i], cross));
            lineMax = std::max(lineMax, AxisMax(slots[i], cross));
        }
        const float distance = across < lineMin ? lineMin - across : (across > lineMax ? across - lineMax : 0.0f);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestBegin = begin;
            bestEnd = end;
        }
        begin = end;
    }
    for (int i = bestBegin; i < bestEnd; ++i)
    {
        const float middle = (AxisMin(slots[i], axis) + AxisMax(slots[i], axis)) * 0.5f;
        if (along < middle)
            return i;
    }
    return bestEnd;
}

int DragDropGapSlot(int index, int source, int insertIndex)
{
    if (insertIndex < 0 || index < 0)
        return index;
    if (source < 0)
        return index >= insertIndex ? index + 1 : index;
    if (index == source)
        return index;
    if (insertIndex > source)
        return index > source && index < insertIndex ? index - 1 : index;
    return index >= insertIndex && index < source ? index + 1 : index;
}

ReorderMove DragDropReorderMove(int source, int insertIndex)
{
    if (source < 0 || insertIndex < 0)
        return {};
    ReorderMove move;
    move.from = source;
    move.to = insertIndex > source ? insertIndex - 1 : insertIndex;
    return move;
}

float DragDropGapShift(int index, int source, int insertIndex, float extent)
{
    if (insertIndex < 0 || index < 0)
        return 0.0f;
    if (source < 0)
        return index >= insertIndex ? extent : 0.0f;
    if (index == source)
        return 0.0f;
    if (insertIndex > source)
        return index > source && index < insertIndex ? -extent : 0.0f;
    return index >= insertIndex && index < source ? extent : 0.0f;
}

ImRect DragDropGapRect(std::span<const ImRect> slots, int source, int insertIndex, Orientation axis, float size)
{
    const int count = static_cast<int>(slots.size());
    if (count == 0 || insertIndex < 0)
        return ImRect(0.0f, 0.0f, 0.0f, 0.0f);
    const int at = std::min(insertIndex, count);
    const bool fromHere = source >= 0 && source < count;

    float start = 0.0f;
    int crossSlot = std::min(at, count - 1);
    if (fromHere && (at == source || at == source + 1))
    {
        start = AxisMin(slots[source], axis);
        crossSlot = source;
    }
    else if (fromHere && at < source)
    {
        start = AxisMin(slots[at], axis);
    }
    else if (fromHere)
    {
        // The entries between the source and the insertion point close up toward the source,
        // so the gap ends where the entry before the insertion point used to end.
        start = AxisMax(slots[at - 1], axis) - size;
        crossSlot = at - 1;
    }
    else if (at < count)
    {
        start = AxisMin(slots[at], axis);
    }
    else
    {
        start = AxisMax(slots[count - 1], axis) + SlotSpacing(slots, axis);
    }
    const ImRect& cross = slots[crossSlot];
    return AxisRect(axis, start, start + size, CrossMin(cross, axis), CrossMax(cross, axis));
}

float DragDropAnimatedPosition(ImGuiID zoneId, ImGuiID key, float target)
{
    if (GImGui == nullptr)
        return target;
    const ImGuiID id = ImHashData(&key, sizeof(key), zoneId);
    return Spring(id, kEntryPositionSub, target, T().motion.standard);
}

ImRect DragDropInsertLineRect(std::span<const ImRect> slots, int insertIndex, Orientation axis)
{
    const int count = static_cast<int>(slots.size());
    if (count == 0 || insertIndex < 0)
        return ImRect(0.0f, 0.0f, 0.0f, 0.0f);
    const int at = std::min(insertIndex, count);
    float center = 0.0f;
    if (at == 0)
        center = AxisMin(slots[0], axis);
    else if (at == count)
        center = AxisMax(slots[count - 1], axis);
    else
        center = (AxisMax(slots[at - 1], axis) + AxisMin(slots[at], axis)) * 0.5f;
    const float half = Px(T().metrics.focusRingWidth) * 0.5f;
    const ImRect& cross = slots[std::min(at, count - 1)];
    return AxisRect(axis, center - half, center + half, CrossMin(cross, axis), CrossMax(cross, axis));
}

ImRect DragDropIntoRect(const ImRect& row, Orientation axis)
{
    const float length = AxisMax(row, axis) - AxisMin(row, axis);
    const float quarter = length * 0.25f;
    return AxisRect(axis, AxisMin(row, axis) + quarter, AxisMax(row, axis) - quarter, CrossMin(row, axis),
                    CrossMax(row, axis));
}

void DragDropDrawInsertLine(ImDrawList* drawList, const ImRect& line)
{
    if (drawList == nullptr || line.GetWidth() <= 0.0f || line.GetHeight() <= 0.0f)
        return;
    const float radius = std::min(line.GetWidth(), line.GetHeight()) * 0.5f;
    FillRect(drawList, line.Min, line.Max, ToU32(T().colors.focusRing), radius);
}

void DragDropDrawTarget(ImDrawList* drawList, const ImRect& rect, float radius)
{
    if (drawList == nullptr || rect.GetWidth() <= 0.0f || rect.GetHeight() <= 0.0f)
        return;
    const Colors& c = T().colors;
    FillRect(drawList, rect.Min, rect.Max, ToU32(c.accentSubtle), radius);
    StrokeRect(drawList, rect.Min, rect.Max, ToU32(c.focusRing), radius, Px(T().metrics.focusRingWidth));
}

} // namespace facet::internal
