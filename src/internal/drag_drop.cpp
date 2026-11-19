// Drag and drop core (drag_drop.h): the drag in progress (pointer, keyboard or forced for
// captures), drop zones, the keyboard path between zones, edge scrolling, and the drag preview
// drawn on top of everything at facet::EndFrame. The pure helpers of ordered zones and the
// indicators are in drag_feedback.cpp.
//
// Pointer drags are Dear ImGui's own drag and drop: the source calls BeginDragDropSource and the
// zones BeginDragDropTargetCustom and AcceptDragDropPayload, so stock sources and targets take
// part. This module only remembers what the ghost needs, copies a delivered payload so it stays
// readable after Dear ImGui clears it, and watches for the end of the drag. Keyboard drags never
// touch Dear ImGui's drag state: the module owns a payload of its own and moves it between the
// zones recorded in the previous frame.

#include "internal/drag_drop.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/keys.h"
#include "internal/scroll.h"
#include "internal/subsystem.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

enum class DragKind
{
    None,
    Pointer,   // Dear ImGui's drag and drop, started by DragDropBeginSource
    Keyboard,  // picked up with Space, moved with the arrow keys
    Forced,    // shown without input for a capture (DragDropForceNextSource)
};

constexpr int kPreviewTextSize = 128;
constexpr int kMaxEdgeRequests = 4;

// A zone as submitted in a frame: what the keyboard path, the forced drag and edge scrolling need.
struct ZoneRecord
{
    ImGuiID id = 0;
    ImGuiID window = 0;
    ImRect rect;
    ImRect visible;    // the rect inside the window's clip rectangle
    ImGuiID type = 0;  // ImHashStr of the payload type
    int slotBegin = 0;
    int slotCount = 0;
    Orientation axis = Orientation::Vertical;
    bool accept = true;
    bool wrap = false;
};

// A place a keyboard drag can land: a gap of an ordered zone or a single zone.
struct CarryPlace
{
    int zone = -1;   // index into the previous frame's zone records
    int insert = -1; // gap index of an ordered zone, -1 for a single zone
    ImVec2 point;    // where the place is, as laid out (screen pixels)
};

struct HoverTimer
{
    ImGuiID id = 0;
    double since = 0.0;
    int lastFrame = -1;
};

struct DragState
{
    DragKind kind = DragKind::None;
    ImGuiID sourceId = 0;
    int startFrame = -1;
    bool delivered = false;

    // Our own payload: keyboard and forced drags (pointer drags use Dear ImGui's).
    ImGuiPayload payload;
    unsigned char data[kDragDropMaxPayloadSize] = {};

    // The preview, copied when the drag starts.
    char text[kPreviewTextSize] = {};
    Icon icon;
    ImageHandle image;
    int count = 0;
    DragDropDrawFn draw = nullptr;

    ImVec2 sourceSize = {0.0f, 0.0f};
    ImVec2 grabOffset = {0.0f, 0.0f};
    ImVec2 sourceCenter = {0.0f, 0.0f};
    ImVec2 pointer = {0.0f, 0.0f};  // forced: the pointer; keyboard: the center of the place

    // Keyboard drags.
    bool placed = false;
    ImGuiID carryZone = 0;
    int carryInsert = -1;
    ImVec2 carryPoint = {0.0f, 0.0f};
    ImGuiID carrySourceZone = 0;  // the ordered zone the item was picked up from
    int carrySource = -1;         // and its index there
    ImRect anchor;                // where the ghost sits
    int deliverFrame = -1;

    // Forced drags.
    int forcedFrame = -1;
};

struct ModuleState
{
    DragState drag;
    int canceledFrame = -1;

    // DragDropBeginSource / DragDropEndSource pairing.
    bool sourceOpen = false;
    bool imguiSourceOpen = false;

    // DragDropForceNextSource.
    bool forceRequested = false;
    ImVec2 forceOffset = {0.0f, 0.0f};
    ImGuiID forcedWinnerPrev = 0;
    ImGuiID forcedWinnerCurr = 0;
    float forcedWinnerArea = FLT_MAX;

    // The zones of this frame and of the previous one, with their slots.
    ImVector<ZoneRecord> zones[2];
    ImVector<ImRect> slots[2];
    int current = 0;
    ImVector<CarryPlace> places;

    ImVector<HoverTimer> timers;

    // A delivered pointer payload, copied before Dear ImGui clears its own.
    ImGuiPayload delivered;
    ImVector<unsigned char> deliveredData;

    bool rejected = false;  // a zone rejected the drag this frame
    DragDropEffect ghostEffect = DragDropEffect::Move;

    // Mouse buttons held back from every item after a cancel, until their release has passed.
    int lockedButtons = 0;

    ImGuiID edgeRequests[kMaxEdgeRequests] = {};
    int edgeRequestCount = 0;
    ImGuiID lastEdgeArea = 0;

    ImRect ghostRect;
};

ModuleState& State()
{
    static ModuleState state;
    return state;
}

bool FrameReady()
{
    const ImGuiContext* g = GImGui;
    return g != nullptr && g->WithinFrameScope && g->CurrentWindow != nullptr && IsInitialized();
}

ImGuiID LockOwner()
{
    return ImHashStr("facet.dragdrop.lock");
}

ImGuiID CancelHandlerOwner()
{
    // The owner Dear ImGui's own Escape handling uses while no item is active.
    return ImHashStr("##DragDropCancelHandler");
}

bool Vertical(Orientation axis)
{
    return axis == Orientation::Vertical;
}

// Copies `text` into `out` and cuts it at a character boundary when it does not fit.
void CopyText(char* out, int outSize, const char* text)
{
    out[0] = '\0';
    if (text == nullptr)
        return;
    int length = static_cast<int>(std::strlen(text));
    if (length >= outSize)
    {
        length = outSize - 1;
        while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
            --length;
    }
    std::memcpy(out, text, static_cast<std::size_t>(length));
    out[length] = '\0';
}

void FillPayload(ImGuiPayload& payload, unsigned char* buffer, const char* type, const void* data, std::size_t size,
                 ImGuiID source)
{
    payload.Clear();
    if (size > 0 && data != nullptr)
        std::memcpy(buffer, data, size);
    payload.Data = size > 0 ? buffer : nullptr;
    payload.DataSize = static_cast<int>(size);
    payload.SourceId = source;
    payload.SourceParentId = GImGui->CurrentWindow->IDStack.back();
    payload.DataFrameCount = GImGui->FrameCount;
    ImStrncpy(payload.DataType, type, IM_COUNTOF(payload.DataType));
}

void StartDrag(DragKind kind, ImGuiID id, const DragDropPreview& preview)
{
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    DragState& drag = s.drag;
    drag.kind = kind;
    drag.sourceId = id;
    drag.startFrame = g.FrameCount;
    drag.delivered = false;
    CopyText(drag.text, kPreviewTextSize, preview.previewText);
    drag.icon = preview.icon;
    drag.image = preview.image;
    drag.count = preview.count;
    drag.draw = preview.draw;
    const ImRect& rect = g.LastItemData.Rect;
    drag.sourceSize = rect.GetSize();
    drag.sourceCenter = rect.GetCenter();
    drag.grabOffset = ImVec2(drag.sourceSize.x * 0.5f, drag.sourceSize.y * 0.5f);
    drag.pointer = drag.sourceCenter;
    drag.placed = false;
    drag.carryZone = 0;
    drag.carryInsert = -1;
    drag.carryPoint = drag.sourceCenter;
    drag.carrySourceZone = 0;
    drag.carrySource = -1;
    drag.anchor = rect;
    drag.deliverFrame = -1;
    drag.forcedFrame = -1;
}

void LockButtons(int mask)
{
    State().lockedButtons |= mask;
}

void HoldLockedButtons()
{
    ModuleState& s = State();
    const ImGuiIO& io = ImGui::GetIO();
    for (int button = 0; button < 2; ++button)
    {
        const int bit = 1 << button;
        if ((s.lockedButtons & bit) == 0)
            continue;
        const ImGuiKey key = button == 0 ? ImGuiKey_MouseLeft : ImGuiKey_MouseRight;
        if (io.MouseDown[button])
            ImGui::SetKeyOwner(key, LockOwner(), ImGuiInputFlags_LockUntilRelease);
        else if (io.MouseReleased[button])
            ImGui::SetKeyOwner(key, LockOwner(), ImGuiInputFlags_LockThisFrame);
        else
            s.lockedButtons &= ~bit;
    }
}

// Ends the drag. A drag that ends without a drop is a cancel; the buttons still down then click
// nothing on their release.
void EndDrag(bool canceled)
{
    ModuleState& s = State();
    if (canceled)
    {
        s.canceledFrame = GImGui->FrameCount;
        const ImGuiIO& io = ImGui::GetIO();
        LockButtons((io.MouseDown[0] || io.MouseReleased[0] ? 1 : 0) | (io.MouseDown[1] || io.MouseReleased[1] ? 2 : 0));
        HoldLockedButtons();
    }
    s.drag.kind = DragKind::None;
    s.drag.sourceId = 0;
    s.drag.deliverFrame = -1;
}

// ---- Zones ----------------------------------------------------------------------------------

ImVector<ZoneRecord>& CurrentZones()
{
    return State().zones[State().current];
}

ImVector<ZoneRecord>& PreviousZones()
{
    return State().zones[1 - State().current];
}

std::span<const ImRect> RecordSlots(const ZoneRecord& zone, bool previous)
{
    ModuleState& s = State();
    const ImVector<ImRect>& pool = s.slots[previous ? 1 - s.current : s.current];
    if (zone.slotCount == 0)
        return {};
    return std::span<const ImRect>(pool.Data + zone.slotBegin, static_cast<std::size_t>(zone.slotCount));
}

void RecordZone(ImGuiID id, const ImRect& rect, const char* type, const DragDropZoneOptions& options)
{
    ModuleState& s = State();
    ImVector<ImRect>& pool = s.slots[s.current];
    ZoneRecord zone;
    zone.id = id;
    zone.window = GImGui->CurrentWindow->ID;
    zone.rect = rect;
    zone.visible = rect;
    zone.visible.ClipWith(GImGui->CurrentWindow->ClipRect);
    zone.type = ImHashStr(type);
    zone.slotBegin = pool.Size;
    zone.slotCount = static_cast<int>(options.slots.size());
    zone.axis = options.axis;
    zone.accept = options.accept;
    zone.wrap = options.wrap;
    for (const ImRect& slot : options.slots)
        pool.push_back(slot);
    CurrentZones().push_back(zone);
}

// Seconds a zone has been hovered without a break, counting this frame.
float HoverSeconds(ImGuiID id)
{
    ModuleState& s = State();
    const ImGuiContext& g = *GImGui;
    for (HoverTimer& timer : s.timers)
    {
        if (timer.id != id)
            continue;
        if (timer.lastFrame < g.FrameCount - 1)
            timer.since = g.Time;
        timer.lastFrame = g.FrameCount;
        return static_cast<float>(g.Time - timer.since);
    }
    HoverTimer timer;
    timer.id = id;
    timer.since = g.Time;
    timer.lastFrame = g.FrameCount;
    s.timers.push_back(timer);
    return 0.0f;
}

void PruneTimers()
{
    ModuleState& s = State();
    const int frame = GImGui->FrameCount;
    for (int i = 0; i < s.timers.Size;)
    {
        if (s.timers[i].lastFrame < frame)
        {
            s.timers[i] = s.timers[s.timers.Size - 1];
            s.timers.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

DragDropEffect EffectFromKeys(const DragDropZoneOptions& options)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && options.allowCopy)
        return DragDropEffect::Copy;
    if (io.KeyShift && options.allowSplit)
        return DragDropEffect::Split;
    return DragDropEffect::Move;
}

// Whether a smaller zone of the same type, under `point` in the previous frame, takes the drag
// instead of zone `id`. Dear ImGui names the winner a frame late; this keeps an outer zone from
// showing a hover for the frame the pointer enters an inner one.
bool SmallerZoneHolds(ImGuiID id, const ImRect& rect, const char* type, ImVec2 point)
{
    const ImGuiID typeHash = ImHashStr(type);
    const float area = rect.GetArea();
    for (const ZoneRecord& zone : PreviousZones())
    {
        if (zone.id != id && zone.type == typeHash && zone.accept && zone.visible.Contains(point) &&
            zone.rect.GetArea() < area)
            return true;
    }
    return false;
}

int ZoneInsertIndex(const DragDropZoneOptions& options, ImVec2 point)
{
    return options.wrap ? DragDropInsertIndexWrapped(options.slots, options.axis, point)
                        : DragDropInsertIndex(options.slots, options.axis, point);
}

// The payload of the drag in progress, whoever started it; null without one.
const ImGuiPayload* CurrentPayload()
{
    const DragState& drag = State().drag;
    if (drag.kind == DragKind::Keyboard || drag.kind == DragKind::Forced)
        return &drag.payload;
    return ImGui::GetDragDropPayload();
}

bool ContainsClipped(const ImRect& rect, ImVec2 point)
{
    ImRect visible = rect;
    visible.ClipWith(GImGui->CurrentWindow->ClipRect);
    return visible.Contains(point);
}

// ---- Keyboard drags -------------------------------------------------------------------------

constexpr ImGuiKey kCarryKeys[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow,
                                   ImGuiKey_Space, ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_Escape};

void OwnCarryKeys(ImGuiID owner)
{
    for (const ImGuiKey key : kCarryKeys)
        ImGui::SetKeyOwner(key, owner);
}

ImVec2 GapPoint(std::span<const ImRect> slots, int insert, Orientation axis, bool wrap)
{
    const int count = static_cast<int>(slots.size());
    if (wrap)
    {
        // In a wrapped grid a place is the leading edge of its slot (the trailing edge of the last).
        const ImRect& slot = slots[std::min(insert, count - 1)];
        const bool end = insert >= count;
        if (Vertical(axis))
            return ImVec2(slot.GetCenter().x, end ? slot.Max.y : slot.Min.y);
        return ImVec2(end ? slot.Max.x : slot.Min.x, slot.GetCenter().y);
    }
    const ImRect& cross = slots[std::min(insert, count - 1)];
    float along = 0.0f;
    if (insert == 0)
        along = Vertical(axis) ? slots[0].Min.y : slots[0].Min.x;
    else if (insert >= count)
        along = Vertical(axis) ? slots[count - 1].Max.y : slots[count - 1].Max.x;
    else
        along = Vertical(axis) ? (slots[insert - 1].Max.y + slots[insert].Min.y) * 0.5f
                               : (slots[insert - 1].Max.x + slots[insert].Min.x) * 0.5f;
    return Vertical(axis) ? ImVec2(cross.GetCenter().x, along) : ImVec2(along, cross.GetCenter().y);
}

// Lists the places of the payload's type in the previous frame's zones.
void BuildPlaces()
{
    ModuleState& s = State();
    DragState& drag = s.drag;
    s.places.resize(0);
    const ImGuiID type = ImHashStr(drag.payload.DataType);
    const ImVector<ZoneRecord>& zones = PreviousZones();
    for (int z = 0; z < zones.Size; ++z)
    {
        const ZoneRecord& zone = zones[z];
        if (zone.type != type || !zone.accept)
            continue;
        const std::span<const ImRect> slots = RecordSlots(zone, true);
        if (slots.empty())
        {
            s.places.push_back(CarryPlace{z, -1, zone.rect.GetCenter()});
            continue;
        }
        const int count = static_cast<int>(slots.size());
        for (int k = 0; k <= count; ++k)
        {
            // Before and after the item's own slot are the same place.
            if (zone.id == drag.carrySourceZone && k == drag.carrySource + 1)
                continue;
            s.places.push_back(CarryPlace{z, k, GapPoint(slots, k, zone.axis, zone.wrap)});
        }
    }
}

// The first place of a keyboard drag: the slot or single zone the item was picked up from.
void PlaceCarry()
{
    ModuleState& s = State();
    DragState& drag = s.drag;
    const ImGuiID type = ImHashStr(drag.payload.DataType);
    const ImVector<ZoneRecord>& zones = PreviousZones();
    float bestArea = FLT_MAX;
    for (const ZoneRecord& zone : zones)
    {
        if (zone.type != type)
            continue;
        const std::span<const ImRect> slots = RecordSlots(zone, true);
        for (int i = 0; i < static_cast<int>(slots.size()); ++i)
        {
            const float area = slots[i].GetArea();
            if (slots[i].Contains(drag.sourceCenter) && area < bestArea)
            {
                bestArea = area;
                drag.carrySourceZone = zone.id;
                drag.carrySource = i;
                drag.carryZone = zone.id;
                drag.carryInsert = i;
                drag.carryPoint = GapPoint(slots, i, zone.axis, zone.wrap);
                drag.placed = true;
            }
        }
    }
    if (drag.placed)
        return;
    for (const ZoneRecord& zone : zones)
    {
        if (zone.type != type || zone.slotCount != 0 || !zone.rect.Contains(drag.sourceCenter))
            continue;
        const float area = zone.rect.GetArea();
        if (area < bestArea)
        {
            bestArea = area;
            drag.carryZone = zone.accept ? zone.id : 0;
            drag.carryInsert = -1;
            drag.carryPoint = zone.rect.GetCenter();
            drag.placed = zone.accept;
        }
    }
}

// Moves a keyboard drag to the nearest place in direction `dir`, and scrolls it into view.
void MoveCarry(ImVec2 dir)
{
    ModuleState& s = State();
    DragState& drag = s.drag;
    BuildPlaces();
    const float epsilon = Px(T().metrics.hairline);
    int best = -1;
    float bestScore = FLT_MAX;
    for (int i = 0; i < s.places.Size; ++i)
    {
        const CarryPlace& place = s.places[i];
        const ImVec2 delta(place.point.x - drag.carryPoint.x, place.point.y - drag.carryPoint.y);
        const float along = delta.x * dir.x + delta.y * dir.y;
        if (along <= epsilon)
            continue;
        const float across = std::fabs(delta.x * dir.y - delta.y * dir.x);
        const float score = along + across * 2.0f;
        if (score < bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    if (best < 0)
        return;
    const CarryPlace& place = s.places[best];
    const ZoneRecord& zone = PreviousZones()[place.zone];
    drag.carryZone = zone.id;
    drag.carryInsert = place.insert;
    drag.carryPoint = place.point;
    drag.placed = true;
    if (ImGuiWindow* window = ImGui::FindWindowByID(zone.window))
    {
        const ImVec2 half(drag.sourceSize.x * 0.5f, drag.sourceSize.y * 0.5f);
        const ImRect reveal(place.point.x - half.x, place.point.y - half.y, place.point.x + half.x,
                            place.point.y + half.y);
        ScrollAreaRevealRect(window, reveal, ScrollAlign::Nearest);
    }
}

void UpdateKeyboardDrag()
{
    ModuleState& s = State();
    DragState& drag = s.drag;
    ImGuiContext& g = *GImGui;
    const ImGuiID owner = drag.sourceId;
    OwnCarryKeys(owner);

    bool clicked = false;
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
        clicked = clicked || g.IO.MouseClicked[button];
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, owner) || clicked || g.NavId != owner)
    {
        EndDrag(true);
        return;
    }
    if (!drag.placed)
        PlaceCarry();

    ImVec2 dir(0.0f, 0.0f);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, owner))
        dir = ImVec2(0.0f, -1.0f);
    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, owner))
        dir = ImVec2(0.0f, 1.0f);
    else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, owner))
        dir = ImVec2(-1.0f, 0.0f);
    else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, owner))
        dir = ImVec2(1.0f, 0.0f);
    if (dir.x != 0.0f || dir.y != 0.0f)
    {
        MoveCarry(dir);
        SetKeyboardModality(true);
    }

    const bool drop = ImGui::IsKeyPressed(ImGuiKey_Space, ImGuiInputFlags_None, owner) ||
                      ImGui::IsKeyPressed(ImGuiKey_Enter, ImGuiInputFlags_None, owner) ||
                      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, ImGuiInputFlags_None, owner);
    if (drop)
    {
        if (drag.placed && drag.carryZone != 0)
            drag.deliverFrame = g.FrameCount;
        else
            EndDrag(true);
    }
}

// ---- Edge scrolling ---------------------------------------------------------------------------

bool IsSameOrAncestor(const ImGuiWindow* ancestor, const ImGuiWindow* window)
{
    for (const ImGuiWindow* w = window; w != nullptr; w = w->ParentWindow)
        if (w == ancestor)
            return true;
    return false;
}

// Whether a zone of `type` was submitted in `window` or in one of its children this frame.
bool HoldsZoneOf(const ImGuiWindow* window, ImGuiID type)
{
    for (const ZoneRecord& zone : CurrentZones())
    {
        if (zone.type != type || !zone.accept)
            continue;
        if (IsSameOrAncestor(window, ImGui::FindWindowByID(zone.window)))
            return true;
    }
    return false;
}

// Whether `window` would scroll toward an edge for a drag at `point`: the point lies in the
// band along an edge, or past it, and the window can still scroll that way.
bool WantsEdgeScroll(const ImGuiWindow* window, ImVec2 point, bool allowPast)
{
    const float band = Px(T().drag.autoScrollZone);
    const ImRect& inner = window->InnerRect;
    bool wants = false;
    const bool acrossY = point.x >= inner.Min.x && point.x <= inner.Max.x;
    const bool acrossX = point.y >= inner.Min.y && point.y <= inner.Max.y;
    if (window->ScrollMax.y > 0.0f && acrossY)
    {
        const bool insideY = point.y >= inner.Min.y && point.y <= inner.Max.y;
        if ((insideY || allowPast) && point.y < inner.Min.y + band && window->Scroll.y > 0.0f)
            wants = true;
        if ((insideY || allowPast) && point.y > inner.Max.y - band && window->Scroll.y < window->ScrollMax.y)
            wants = true;
    }
    if (window->ScrollMax.x > 0.0f && acrossX)
    {
        const bool insideX = point.x >= inner.Min.x && point.x <= inner.Max.x;
        if ((insideX || allowPast) && point.x < inner.Min.x + band && window->Scroll.x > 0.0f)
            wants = true;
        if ((insideX || allowPast) && point.x > inner.Max.x - band && window->Scroll.x < window->ScrollMax.x)
            wants = true;
    }
    return wants;
}

void RequestEdgeScroll(ImGuiWindow* window, ImVec2 point)
{
    ModuleState& s = State();
    if (s.edgeRequestCount < kMaxEdgeRequests)
        s.edgeRequests[s.edgeRequestCount++] = window->ID;
    s.lastEdgeArea = window->ID;
    RequestScrollAreaEdgeScroll(window, point);
}

void UpdateEdgeScroll()
{
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    s.edgeRequestCount = 0;
    if (!g.DragDropActive || s.drag.kind == DragKind::Keyboard || s.drag.kind == DragKind::Forced ||
        g.DragDropPayload.DataFrameCount == -1)
    {
        s.lastEdgeArea = 0;
        return;
    }
    const ImVec2 point = g.IO.MousePos;
    const ImGuiID type = ImHashStr(g.DragDropPayload.DataType);
    // The innermost area under the pointer that holds a zone of the type and can still scroll
    // toward the edge the pointer is near.
    for (ImGuiWindow* window = g.HoveredWindow; window != nullptr; window = window->ParentWindow)
    {
        if (HoldsZoneOf(window, type) && WantsEdgeScroll(window, point, false))
        {
            RequestEdgeScroll(window, point);
            return;
        }
    }
    // Past the edge of the area that scrolled last: the drag left it on the side it scrolls to.
    if (ImGuiWindow* last = s.lastEdgeArea != 0 ? ImGui::FindWindowByID(s.lastEdgeArea) : nullptr)
    {
        if (last->Active && !last->InnerRect.Contains(point) && WantsEdgeScroll(last, point, true))
        {
            RequestEdgeScroll(last, point);
            return;
        }
    }
    s.lastEdgeArea = 0;
}

// ---- The ghost --------------------------------------------------------------------------------

void DrawGhostContent(ImDrawList* drawList, const ImRect& rect)
{
    const Theme& t = T();
    const DragState& drag = State().drag;
    const float inset = Px(t.spacing.md);
    float x = rect.Min.x + inset;
    const float height = rect.GetHeight();
    if (drag.image.IsValid())
    {
        const float side = std::max(0.0f, height - Px(t.spacing.sm) * 2.0f);
        const ImVec2 min(x, rect.Min.y + (height - side) * 0.5f);
        ImageOptions options;
        options.fit = ImageFit::Cover;
        options.radius = Px(t.radii.element);
        DrawImage(drawList, drag.image, min, ImVec2(min.x + side, min.y + side), options);
        x += side + Px(t.spacing.sm);
    }
    PushTextStyle(TextStyle::Body);
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    if (drag.icon.IsValid())
    {
        const float iconSize = size;
        DrawIcon(drawList, drag.icon, ImVec2(x, rect.Min.y + (height - iconSize) * 0.5f), iconSize,
                 ToU32(t.colors.labelSecondary));
        x += iconSize + Px(t.spacing.sm);
    }
    if (drag.text[0] != '\0')
    {
        char cut[kPreviewTextSize * 2];
        const float room = rect.Max.x - inset - x;
        const TruncateResult fit = TruncateText(font, size, drag.text, nullptr, room, TruncateMode::End, cut, sizeof(cut));
        if (fit.length > 0)
            drawList->AddText(font, size, ImVec2(x, CenteredTextY(rect.Min.y, height)), ToU32(t.colors.label), cut);
    }
    PopTextStyle();
}

void DrawBadge(ImDrawList* drawList, ImVec2 center, const char* text, Icon icon)
{
    const Theme& t = T();
    PushTextStyle(TextStyle::Footnote);
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const ImVec2 textSize = text != nullptr ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, text) : ImVec2(size, size);
    const float height = size + Px(t.spacing.xs) * 2.0f;
    const float width = std::max(height, textSize.x + Px(t.spacing.sm) * 2.0f);
    const ImVec2 min(center.x - width * 0.5f, center.y - height * 0.5f);
    const ImVec2 max(min.x + width, min.y + height);
    FillRect(drawList, min, max, ToU32(t.colors.accent), height * 0.5f);
    if (text != nullptr)
        drawList->AddText(font, size, ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                          ToU32(t.colors.onAccent), text);
    else
        DrawIcon(drawList, icon, ImVec2(center.x - size * 0.5f, center.y - size * 0.5f), size, ToU32(t.colors.onAccent));
    PopTextStyle();
}

void DrawGhost()
{
    ModuleState& s = State();
    DragState& drag = s.drag;
    ImGuiContext& g = *GImGui;
    s.ghostRect = ImRect(0.0f, 0.0f, 0.0f, 0.0f);

    const ImGuiPayload* payload = nullptr;
    ImVec2 min;
    switch (drag.kind)
    {
    case DragKind::Pointer:
        if (!g.DragDropActive || g.DragDropPayload.SourceId != drag.sourceId)
            return;
        payload = &g.DragDropPayload;
        min = ImVec2(g.IO.MousePos.x - drag.grabOffset.x, g.IO.MousePos.y - drag.grabOffset.y);
        break;
    case DragKind::Forced:
        payload = &drag.payload;
        min = ImVec2(drag.pointer.x - drag.grabOffset.x, drag.pointer.y - drag.grabOffset.y);
        break;
    case DragKind::Keyboard:
    {
        payload = &drag.payload;
        // The ghost floats a little above the place it would land and springs to each new place.
        const float lift = Px(T().spacing.xs);
        const ImGuiID key = ImHashData(&drag.startFrame, sizeof(drag.startFrame), ImHashStr("facet.dragdrop.ghost"));
        min.x = Spring(key, 0, drag.anchor.Min.x + lift, T().motion.standard);
        min.y = Spring(key, 1, drag.anchor.Min.y - lift, T().motion.standard);
        break;
    }
    default:
        return;
    }

    const Theme& t = T();
    ImVec2 size = drag.sourceSize;
    if (size.x < 1.0f || size.y < 1.0f)
        size = ImVec2(Px(t.metrics.controlHeight), Px(t.metrics.controlHeight));
    const ImRect rect(min, ImVec2(min.x + size.x, min.y + size.y));
    const float radius = Px(t.radii.control);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Everything of the item is drawn first, then faded to the ghost opacity as one.
    const int firstVertex = drawList->VtxBuffer.Size;
    DrawElevation(drawList, rect.Min, rect.Max, radius, 3);
    FillRect(drawList, rect.Min, rect.Max, ToU32(t.colors.ElevatedSurface(3)), radius);
    StrokeRect(drawList, rect.Min, rect.Max, ToU32(t.colors.separator), radius, HairlineThickness());
    drawList->PushClipRect(rect.Min, rect.Max, true);
    if (drag.draw != nullptr)
        drag.draw(drawList, rect, *payload);
    else
        DrawGhostContent(drawList, rect);
    drawList->PopClipRect();
    const float opacity = std::clamp(t.drag.ghostOpacity, 0.0f, 1.0f);
    for (int i = firstVertex; i < drawList->VtxBuffer.Size; ++i)
        drawList->VtxBuffer[i].col = ScaleAlpha(drawList->VtxBuffer[i].col, opacity);

    // What stays fully readable: the keyboard focus, the count and the copy mark.
    if (drag.kind == DragKind::Keyboard)
        DrawFocusRing(drawList, rect, radius);
    if (drag.count > 1)
    {
        char count[16];
        std::snprintf(count, sizeof(count), "%d", drag.count);
        DrawBadge(drawList, ImVec2(rect.Max.x, rect.Min.y), count, Icon());
    }
    if (s.ghostEffect == DragDropEffect::Copy)
        DrawBadge(drawList, rect.Max, nullptr, Icon("plus"));
    s.ghostRect = rect;
}

// ---- Hooks ------------------------------------------------------------------------------------

void DragDropNewFrame()
{
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    DragState& drag = s.drag;
    HoldLockedButtons();

    if (drag.kind == DragKind::Pointer)
    {
        if (!g.DragDropActive || g.DragDropPayload.SourceId != drag.sourceId)
        {
            // Dear ImGui ended it: a delivery, Escape (its own handling), or a release where no
            // zone took the payload.
            EndDrag(!drag.delivered);
        }
        else if (g.IO.MouseClicked[ImGuiMouseButton_Right] && g.DragDropMouseButton != ImGuiMouseButton_Right)
        {
            if (g.ActiveId == drag.sourceId)
                ImGui::ClearActiveID();
            ImGui::ClearDragDrop();
            EndDrag(true);
        }
        else if (g.ActiveId == 0)
        {
            // The source stopped being submitted: Escape still cancels before any popup sees it.
            ImGui::SetKeyOwner(ImGuiKey_Escape, CancelHandlerOwner());
        }
    }
    else if (drag.kind == DragKind::Keyboard)
    {
        UpdateKeyboardDrag();
    }
}

void DragDropEndFrame()
{
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    DragState& drag = s.drag;

    UpdateEdgeScroll();

    if (s.rejected && g.DragDropActive && g.DragDropAcceptIdCurr == 0 && drag.kind != DragKind::Keyboard &&
        drag.kind != DragKind::Forced)
        ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);

    if (drag.kind == DragKind::Forced && drag.forcedFrame != g.FrameCount)
        drag.kind = DragKind::None;

    DrawGhost();

    if (drag.kind == DragKind::Keyboard && drag.deliverFrame == g.FrameCount)
        EndDrag(!drag.delivered);

    s.forcedWinnerPrev = s.forcedWinnerCurr;
    s.forcedWinnerCurr = 0;
    s.forcedWinnerArea = FLT_MAX;
    s.forceRequested = false;
    s.rejected = false;
    s.ghostEffect = DragDropEffect::Move;
    if (s.sourceOpen)
    {
        ReportUsageError("DragDropBeginSource without DragDropEndSource at EndFrame");
        if (s.imguiSourceOpen)
            ImGui::EndDragDropSource();
        s.sourceOpen = false;
        s.imguiSourceOpen = false;
    }
    PruneTimers();

    // This frame's zones become the previous frame's.
    s.current = 1 - s.current;
    s.zones[s.current].resize(0);
    s.slots[s.current].resize(0);
}

void DragDropShutdown()
{
    ModuleState& s = State();
    if (ImGuiContext* g = ImGui::GetCurrentContext())
    {
        // A drag in progress ends in Dear ImGui too, and the keys the core owns (Escape, the carry
        // keys, the held mouse buttons) go back to it.
        const ImGuiID source = s.drag.sourceId;
        if (s.drag.kind == DragKind::Pointer && g->DragDropActive && g->DragDropPayload.SourceId == source)
            ImGui::ClearDragDrop();
        const ImGuiID owners[] = {LockOwner(), CancelHandlerOwner(), source};
        ReleaseKeyOwners(std::span<const ImGuiID>(owners, source != 0 ? 3 : 2));
    }
    s.drag = DragState{};
    s.sourceOpen = false;
    s.imguiSourceOpen = false;
    s.forceRequested = false;
    s.lockedButtons = 0;
    s.edgeRequestCount = 0;
    s.lastEdgeArea = 0;
    s.ghostRect = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 2; ++i)
    {
        s.zones[i].clear();
        s.slots[i].clear();
    }
    s.places.clear();
    s.timers.clear();
    s.deliveredData.clear();
}

bool ValidPayload(const char* type, const void* data, std::size_t size)
{
    if (type == nullptr || type[0] == '\0' || std::strlen(type) > 32)
    {
        ReportUsageError("DragDropBeginSource: the payload type must have 1 to 32 characters");
        return false;
    }
    if (size > kDragDropMaxPayloadSize)
    {
        ReportUsageError("DragDropBeginSource: a payload of %d bytes is over the limit of %d", static_cast<int>(size),
                         static_cast<int>(kDragDropMaxPayloadSize));
        return false;
    }
    if (size > 0 && data == nullptr)
    {
        ReportUsageError("DragDropBeginSource: null data with a size of %d bytes", static_cast<int>(size));
        return false;
    }
    return true;
}

} // namespace

bool DragDropBeginSource(const char* type, const void* data, std::size_t size, const DragDropPreview& preview)
{
    if (!FrameReady())
        return false;
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    if (s.sourceOpen)
    {
        ReportUsageError("DragDropBeginSource called again before DragDropEndSource");
        return false;
    }
    const ImGuiID id = g.LastItemData.ID;
    if (id == 0 || !ValidPayload(type, data, size))
        return false;
    DragState& drag = s.drag;

    // A capture's forced drag.
    if (s.forceRequested)
    {
        s.forceRequested = false;
        if (drag.kind != DragKind::Forced || drag.sourceId != id)
            StartDrag(DragKind::Forced, id, preview);
        FillPayload(drag.payload, drag.data, type, data, size, id);
        drag.pointer = ImVec2(g.LastItemData.Rect.GetCenter().x + s.forceOffset.x,
                              g.LastItemData.Rect.GetCenter().y + s.forceOffset.y);
        drag.forcedFrame = g.FrameCount;
        s.sourceOpen = true;
        return true;
    }
    if (drag.kind == DragKind::Forced && drag.sourceId == id)
    {
        if (drag.forcedFrame != g.FrameCount)
            return false;
        s.sourceOpen = true;
        return true;
    }

    // A keyboard drag of this item.
    if (drag.kind == DragKind::Keyboard && drag.sourceId == id)
    {
        OwnCarryKeys(id);
        s.sourceOpen = true;
        return true;
    }

    const ImGuiItemFlags flags = g.LastItemData.ItemFlags;
    constexpr ImGuiItemFlags kBlocked = static_cast<ImGuiItemFlags>(ImGuiItemFlags_Disabled) |
                                        static_cast<ImGuiItemFlags>(ImGuiItemFlags_ReadOnly);
    if ((flags & kBlocked) != 0)
        return false;

    // A pointer drag: only from the item that holds the mouse, never from a text field.
    if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse && ImGui::GetInputTextState(id) == nullptr &&
        drag.kind != DragKind::Keyboard)
    {
        // Dear ImGui starts the drag after io.MouseDragThreshold; for this call alone it is the
        // threshold token at the current scale.
        const float saved = g.IO.MouseDragThreshold;
        g.IO.MouseDragThreshold = Px(T().drag.startThreshold);
        const bool begun = ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip);
        g.IO.MouseDragThreshold = saved;
        if (begun)
        {
            if (drag.kind != DragKind::Pointer || drag.sourceId != id)
            {
                StartDrag(DragKind::Pointer, id, preview);
                const ImGuiMouseButton button = g.DragDropMouseButton >= 0 ? g.DragDropMouseButton : ImGuiMouseButton_Left;
                const ImVec2 press = g.IO.MouseClickedPos[button];
                drag.grabOffset = ImVec2(press.x - g.LastItemData.Rect.Min.x, press.y - g.LastItemData.Rect.Min.y);
            }
            ImGui::SetDragDropPayload(type, size > 0 ? data : nullptr, size, ImGuiCond_Once);
            ImGui::SetKeyOwner(ImGuiKey_Escape, id);
            s.sourceOpen = true;
            s.imguiSourceOpen = true;
            return true;
        }
    }
    // The frame of the release: Dear ImGui no longer reports the source, the drag still runs.
    if (drag.kind == DragKind::Pointer && drag.sourceId == id && g.DragDropActive && g.DragDropPayload.SourceId == id)
    {
        s.sourceOpen = true;
        return true;
    }

    // The pick-up chord on the focused item picks it up. Its key is owned only while exactly the
    // chord's modifiers are held, so a chord with a modifier leaves the plain key to Dear ImGui's
    // activation.
    const ImGuiKey pickUpKey = static_cast<ImGuiKey>(preview.pickUpChord & ~ImGuiMod_Mask_);
    const ImGuiKeyChord pickUpMods = preview.pickUpChord & ImGuiMod_Mask_;
    if (preview.keyboard && pickUpKey != ImGuiKey_None && drag.kind == DragKind::None && !g.DragDropActive &&
        g.NavId == id && g.IO.KeyMods == pickUpMods)
    {
        ImGui::SetKeyOwner(pickUpKey, id);
        if (ImGui::IsKeyPressed(pickUpKey, ImGuiInputFlags_None, id))
        {
            StartDrag(DragKind::Keyboard, id, preview);
            FillPayload(drag.payload, drag.data, type, data, size, id);
            OwnCarryKeys(id);
            SetKeyboardModality(true);
            s.sourceOpen = true;
            return true;
        }
    }
    return false;
}

void DragDropEndSource()
{
    if (GImGui == nullptr)
        return;
    ModuleState& s = State();
    if (!s.sourceOpen)
    {
        ReportUsageError("DragDropEndSource without a DragDropBeginSource that returned true");
        return;
    }
    if (s.imguiSourceOpen)
        ImGui::EndDragDropSource();
    s.sourceOpen = false;
    s.imguiSourceOpen = false;
}

bool DragDropActive(const char* type, DragDropInfo* info)
{
    if (info != nullptr)
        *info = DragDropInfo{};
    if (!FrameReady())
        return false;
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    const DragState& drag = s.drag;

    DragDropInfo found;
    if (drag.kind == DragKind::Keyboard || drag.kind == DragKind::Forced)
    {
        found.payload = &drag.payload;
        found.pointer = drag.kind == DragKind::Keyboard ? drag.anchor.GetCenter() : drag.pointer;
        found.grabOffset = drag.grabOffset;
        found.sourceSize = drag.sourceSize;
        found.keyboard = drag.kind == DragKind::Keyboard;
        found.facetSource = true;
    }
    else if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
    {
        found.payload = payload;
        found.pointer = g.IO.MousePos;
        const bool ours = drag.kind == DragKind::Pointer && drag.sourceId == payload->SourceId;
        if (ours)
        {
            found.grabOffset = drag.grabOffset;
            found.sourceSize = drag.sourceSize;
        }
        found.facetSource = ours;
    }
    else
    {
        return false;
    }
    if (type != nullptr && !found.payload->IsDataType(type))
        return false;
    if (info != nullptr)
        *info = found;
    return true;
}

DragDropZoneResult DragDropZone(ImGuiID id, const ImRect& rect, const char* type, const DragDropZoneOptions& options)
{
    DragDropZoneResult result;
    if (!FrameReady() || id == 0 || type == nullptr || type[0] == '\0')
        return result;
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    DragState& drag = s.drag;
    RecordZone(id, rect, type, options);
    const int slotCount = static_cast<int>(options.slots.size());

    // Keyboard drags: the zone holding the drag reports it as hovered.
    if (drag.kind == DragKind::Keyboard)
    {
        if (id != drag.carryZone || !drag.payload.IsDataType(type))
            return result;
        result.hovered = true;
        result.keyboard = true;
        result.payload = &drag.payload;
        result.effect = EffectFromKeys(options);
        result.insertIndex = slotCount > 0 ? std::clamp(drag.carryInsert, 0, slotCount) : -1;
        result.hoverOpen = HoverSeconds(id) >= T().drag.hoverOpenDelay;
        if (slotCount > 0)
        {
            const int source = drag.carrySourceZone == id ? drag.carrySource : -1;
            const float size = options.axis == Orientation::Vertical ? drag.sourceSize.y : drag.sourceSize.x;
            if (options.wrap)
            {
                // Where the entry is laid out after the move.
                const int to = source >= 0 && result.insertIndex > source ? result.insertIndex - 1 : result.insertIndex;
                drag.anchor = options.slots[static_cast<std::size_t>(std::min(to, slotCount - 1))];
            }
            else
            {
                drag.anchor = DragDropGapRect(options.slots, source, result.insertIndex, options.axis, size);
            }
        }
        else
        {
            const ImVec2 center = rect.GetCenter();
            drag.anchor = ImRect(center.x - drag.sourceSize.x * 0.5f, center.y - drag.sourceSize.y * 0.5f,
                                 center.x + drag.sourceSize.x * 0.5f, center.y + drag.sourceSize.y * 0.5f);
        }
        if (drag.deliverFrame == g.FrameCount && options.accept)
        {
            result.delivered = true;
            drag.payload.Delivery = true;
            drag.delivered = true;
        }
        s.ghostEffect = result.effect;
        return result;
    }

    // Captures: the forced pointer; the smallest zone under it in the previous frame wins.
    if (drag.kind == DragKind::Forced)
    {
        if (!ContainsClipped(rect, drag.pointer))
            return result;
        if (!options.accept || !drag.payload.IsDataType(type))
        {
            result.rejected = true;
            s.rejected = true;
            return result;
        }
        const float area = rect.GetArea();
        if (area < s.forcedWinnerArea)
        {
            s.forcedWinnerArea = area;
            s.forcedWinnerCurr = id;
        }
        if (id == s.forcedWinnerPrev && !SmallerZoneHolds(id, rect, type, drag.pointer))
        {
            result.hovered = true;
            result.payload = &drag.payload;
            result.insertIndex = ZoneInsertIndex(options, drag.pointer);
        }
        return result;
    }

    // Pointer drags, Facet's and stock ones alike.
    if (!g.DragDropActive || !ImGui::BeginDragDropTargetCustom(rect, id))
        return result;
    const ImGuiPayload* current = ImGui::GetDragDropPayload();
    if (current == nullptr)
    {
        ImGui::EndDragDropTarget();
        return result;
    }
    if (!options.accept || !current->IsDataType(type))
    {
        result.rejected = true;
        s.rejected = true;
        ImGui::EndDragDropTarget();
        return result;
    }
    const ImGuiPayload* accepted = ImGui::AcceptDragDropPayload(
        type, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
    // Dear ImGui picks the smallest zone that accepted in a frame; that one hovers in the next.
    const bool winner = g.DragDropAcceptIdPrev == id && !SmallerZoneHolds(id, rect, type, g.IO.MousePos);
    if (accepted != nullptr && (winner || accepted->Delivery))
    {
        result.hovered = true;
        result.payload = accepted;
        result.effect = EffectFromKeys(options);
        result.insertIndex = ZoneInsertIndex(options, g.IO.MousePos);
        result.hoverOpen = HoverSeconds(id) >= T().drag.hoverOpenDelay;
        const bool ours = drag.kind == DragKind::Pointer && drag.sourceId == accepted->SourceId;
        if (ours)
            s.ghostEffect = result.effect;
        if (accepted->Delivery)
        {
            // Dear ImGui clears its payload in EndDragDropTarget; the result keeps a copy.
            s.delivered = *accepted;
            s.deliveredData.resize(accepted->DataSize);
            if (accepted->DataSize > 0)
                std::memcpy(s.deliveredData.Data, accepted->Data, static_cast<std::size_t>(accepted->DataSize));
            s.delivered.Data = accepted->DataSize > 0 ? s.deliveredData.Data : nullptr;
            result.payload = &s.delivered;
            result.delivered = true;
            if (ours)
                drag.delivered = true;
        }
    }
    ImGui::EndDragDropTarget();
    return result;
}

DragDropZoneResult DragDropZone(ImGuiID id, const ImRect& rect, std::span<const char* const> types,
                                const DragDropZoneOptions& options)
{
    if (!FrameReady() || types.empty())
        return {};
    // The zone stands for the type of the drag when it is one of `types`, else for the first.
    const char* type = types[0];
    if (const ImGuiPayload* payload = CurrentPayload())
    {
        for (const char* candidate : types)
        {
            if (candidate != nullptr && payload->IsDataType(candidate))
            {
                type = candidate;
                break;
            }
        }
    }
    return DragDropZone(id, rect, type, options);
}

bool DragDropCanceled()
{
    return GImGui != nullptr && State().canceledFrame == GImGui->FrameCount;
}

void DragDropCancel()
{
    if (GImGui == nullptr)
        return;
    ModuleState& s = State();
    ImGuiContext& g = *GImGui;
    switch (s.drag.kind)
    {
    case DragKind::Pointer:
        if (g.DragDropActive && g.DragDropPayload.SourceId == s.drag.sourceId)
        {
            if (g.ActiveId == s.drag.sourceId)
                ImGui::ClearActiveID();
            ImGui::ClearDragDrop();
        }
        EndDrag(true);
        break;
    case DragKind::Keyboard:
        EndDrag(true);
        break;
    default:
        break;
    }
}

void DragDropForceNextSource(ImVec2 pointerOffset)
{
    ModuleState& s = State();
    s.forceRequested = true;
    s.forceOffset = pointerOffset;
}

ImRect DragDropGhostRect()
{
    return State().ghostRect;
}

bool DragDropRequestedEdgeScroll(ImGuiID windowId)
{
    const ModuleState& s = State();
    for (int i = 0; i < s.edgeRequestCount; ++i)
        if (s.edgeRequests[i] == windowId)
            return true;
    return false;
}

Subsystem DragDropSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "drag_drop";
    subsystem.newFrame = &DragDropNewFrame;
    subsystem.endFrame = &DragDropEndFrame;
    subsystem.shutdown = &DragDropShutdown;
    return subsystem;
}

} // namespace facet::internal
