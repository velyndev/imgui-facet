// Scroll core (scroll.h): smooth scrolling of any window, wheel routing between nested windows,
// edge scroll for drags, and the scroll area with its overlay bars and edge fades.
//
// Per window the core keeps one record (keyed by the window id, found again with
// ImGui::FindWindowByID every frame, so no window pointer outlives its frame): the smooth scroll
// of each axis, the frames of the latest wheel turn, bar drag and edge scroll, and for scroll
// areas the geometry of the bars. The records live in one vector that grows only when more
// windows scroll at once than ever before.
//
// facet::NewFrame runs after ImGui::NewFrame, so the hook below sees this frame's input and
// hovered window but runs before any window is begun: the wheel is routed and every smooth scroll
// is stepped there, and the positions it writes with ImGui::SetScrollX/Y are applied by each
// window's Begin in the same frame. A target that someone else wrote in between (a stock call,
// keyboard navigation) or a position that changed by more than rounding cancels the motion.
//
// The module's end-of-frame hook also checks the list core's open row ranges
// (list_view.h, ListCoreEndFrame), because the list core has no entry of its own in the
// subsystem table.

#include "internal/scroll.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>

#include "core/anim_store.h"
#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/subsystem.h"

namespace facet::internal
{
namespace
{

constexpr int kAxisX = ImGuiAxis_X;
constexpr int kAxisY = ImGuiAxis_Y;

// Records whose window has not been seen for this many frames are dropped (when idle).
constexpr int kRecordLifetimeFrames = 600;

// Animation store sub keys under the bar ids.
constexpr int kBarAlphaSub = 1;

struct ScrollRecord
{
    ImGuiID window = 0;
    // Smooth scroll per axis.
    bool moving[2] = {false, false};
    bool fresh[2] = {false, false};  // started this frame: nothing written yet to compare with
    float pos[2] = {0.0f, 0.0f};
    float velocity[2] = {0.0f, 0.0f};
    float target[2] = {0.0f, 0.0f};
    float written[2] = {0.0f, 0.0f};
    float edgeRest[2] = {0.0f, 0.0f};  // edge scroll below one pixel, carried to the next frame
    // Activity, in Dear ImGui frame counts and the core clock.
    int seenFrame = -1;
    int wheelFrame = -1;
    int dragFrame = -1;
    int edgeFrame = -1;
    double lastActivity = -1.0e9;
    ImVec2 lastScroll = ImVec2(0.0f, 0.0f);
    // Scroll areas.
    bool area = false;
    bool horizontal = false;
    bool hasBar[2] = {false, false};
    ImRect track[2];
    ImRect thumb[2];
    int dragAxis = -1;   // the axis whose thumb is dragged, -1 for none
    int pageAxis = -1;   // the axis whose track is held for paging, -1 for none
    float grab = 0.0f;   // pointer offset from the start of the dragged thumb
};

struct OpenArea
{
    ImGuiID window = 0;
    ImVec4 surface;
    bool edgeFades = true;
    bool horizontal = false;
    bool clipPushed = false;
    float alpha = 0.0f;            // visibility of the bars
    bool hasBar[2] = {false, false};
    float expand[2] = {0.0f, 0.0f};  // 0 thin, 1 wide (pointer over the bar or dragging it)
    float active[2] = {0.0f, 0.0f};  // hovered or held, for the thumb color
};

struct ScrollCore
{
    ImGuiContext* context = nullptr;
    ImVector<ScrollRecord> records;
    OpenArea open[kScrollAreaDepth];
    int openDepth = 0;
    ImVec4 surfaces[kScrollFadeSurfaceDepth];
    int surfaceDepth = 0;
    int surfaceOverflow = 0;
    double clock = 0.0;
    int claimedFrame = -1000;  // frame in which the wheel was last claimed from Dear ImGui
    ImGuiID lockWindow = 0;    // the window the current wheel gesture scrolls
    double lockTime = 0.0;
    ImVec2 lockMouse = ImVec2(0.0f, 0.0f);
    float lockDirection[2] = {0.0f, 0.0f};  // sign of the gesture's last turn per axis
};

ScrollCore& Core()
{
    static ScrollCore core;
    return core;
}

ImGuiID WheelOwner()
{
    static const ImGuiID id = ImHashStr("##facet.scroll.wheel");
    return id;
}

ScrollRecord* FindRecord(ImGuiID window)
{
    ScrollCore& core = Core();
    for (ScrollRecord& record : core.records)
        if (record.window == window)
            return &record;
    return nullptr;
}

ScrollRecord& Record(ImGuiWindow* window)
{
    if (ScrollRecord* found = FindRecord(window->ID))
        return *found;
    ScrollCore& core = Core();
    ScrollRecord record;
    record.window = window->ID;
    record.lastScroll = window->Scroll;
    record.seenFrame = GImGui->FrameCount;
    core.records.push_back(record);
    return core.records.back();
}

void SetScrollAxis(ImGuiWindow* window, int axis, float value)
{
    if (axis == kAxisX)
        ImGui::SetScrollX(window, value);
    else
        ImGui::SetScrollY(window, value);
}

// Where Dear ImGui will put the axis at the window's next Begin: a pending ScrollTarget, else the
// current offset.
float PendingScroll(const ImGuiWindow* window, int axis)
{
    if (window->ScrollTarget[axis] >= FLT_MAX)
        return window->Scroll[axis];
    const float decoration = axis == kAxisX
                                 ? window->DecoOuterSizeX1 + window->DecoInnerSizeX1 + window->DecoOuterSizeX2
                                 : window->DecoOuterSizeY1 + window->DecoInnerSizeY1 + window->DecoOuterSizeY2;
    const float value =
        window->ScrollTarget[axis] - window->ScrollTargetCenterRatio[axis] * (window->SizeFull[axis] - decoration);
    return std::clamp(value, 0.0f, std::max(0.0f, window->ScrollMax[axis]));
}

float TargetOf(ImGuiWindow* window, int axis)
{
    if (const ScrollRecord* record = FindRecord(window->ID))
        if (record->moving[axis])
            return std::clamp(record->target[axis], 0.0f, std::max(0.0f, window->ScrollMax[axis]));
    return PendingScroll(window, axis);
}

void StopMotion(ImGuiWindow* window, int axis)
{
    if (ScrollRecord* record = FindRecord(window->ID))
        record->moving[axis] = false;
}

// Starts or retargets the smooth scroll of one axis. `target` is clamped to 0 here and to the
// scroll range at every step, so a range that grows this frame is still reached.
void AnimateAxis(ImGuiWindow* window, int axis, float target)
{
    ScrollRecord& record = Record(window);
    if (!record.moving[axis])
    {
        record.moving[axis] = true;
        record.fresh[axis] = true;
        record.pos[axis] = PendingScroll(window, axis);
        record.velocity[axis] = 0.0f;
    }
    record.target[axis] = std::max(0.0f, target);
    record.lastActivity = Core().clock;
}

// Whether `window` takes a wheel turn on `axis` at all (before looking at its scroll range).
bool TakesWheel(const ImGuiWindow* window, int axis)
{
    if ((window->Flags & ImGuiWindowFlags_NoMouseInputs) != 0)
        return false;
    if (const ScrollRecord* record = FindRecord(window->ID); record != nullptr && record->area)
        return axis == kAxisY || record->horizontal;
    return (window->Flags & ImGuiWindowFlags_NoScrollWithMouse) == 0;
}

bool CanScrollToward(ImGuiWindow* window, int axis, float delta)
{
    const float max = window->ScrollMax[axis];
    if (max <= 0.0f)
        return false;
    const float target = TargetOf(window, axis);
    return delta > 0.0f ? target < max - 0.5f : target > 0.5f;
}

// Ends the current wheel gesture once the pointer moves by more than io.MouseDragThreshold from
// where the last turn was, a mouse button is pressed, or scroll.barHideDelay passes without a turn.
void ExpireWheelLock(ImGuiContext& g)
{
    ScrollCore& core = Core();
    if (core.lockWindow == 0)
        return;
    const ImGuiIO& io = g.IO;
    const ImVec2 moved(io.MousePos.x - core.lockMouse.x, io.MousePos.y - core.lockMouse.y);
    const float drag = io.MouseDragThreshold;
    bool clicked = false;
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
        clicked = clicked || io.MouseClicked[button];
    if (clicked || core.clock - core.lockTime > T().scroll.barHideDelay || ImLengthSqr(moved) > drag * drag)
        core.lockWindow = 0;
}

// Hands this frame's wheel to the windows under the pointer, when Dear ImGui did not take it
// because the scroll core claimed it in the previous frame.
void RouteWheel(ImGuiContext& g)
{
    ScrollCore& core = Core();
    const ImGuiID owner = ImGui::GetKeyOwner(ImGuiKey_MouseWheelY);
    const bool claimed = core.claimedFrame == g.FrameCount - 1 &&
                         (owner == WheelOwner() || (g.WheelingWindow != nullptr && owner == g.WheelingWindow->ID));
    if (!claimed)
        return;
    const ImGuiIO& io = g.IO;
    ImVec2 wheel(io.MouseWheelH, io.MouseWheel);
    if ((wheel.x == 0.0f && wheel.y == 0.0f) || io.KeyCtrl)
        return;
    if (io.MouseWheelRequestAxisSwap)
        wheel = ImVec2(wheel.y, 0.0f);

    ImGuiWindow* lock = core.lockWindow != 0 ? ImGui::FindWindowByID(core.lockWindow) : nullptr;

    const float notch = Px(T().scroll.wheelDistance);
    for (int axis = kAxisX; axis <= kAxisY; ++axis)
    {
        if (wheel[axis] == 0.0f)
            continue;
        const float delta = -wheel[axis] * notch;
        ImGuiWindow* chosen = nullptr;
        // A turn the other way is a new gesture: it starts again at the window under the pointer.
        const bool sameWay = core.lockDirection[axis] * delta > 0.0f;
        if (lock != nullptr && sameWay && TakesWheel(lock, axis) && CanScrollToward(lock, axis, delta))
            chosen = lock;
        for (ImGuiWindow* window = g.HoveredWindow; chosen == nullptr && window != nullptr;
             window = (window->Flags & ImGuiWindowFlags_ChildWindow) != 0 ? window->ParentWindow : nullptr)
        {
            if (TakesWheel(window, axis) && CanScrollToward(window, axis, delta))
                chosen = window;
        }
        if (chosen == nullptr)
            continue;
        AnimateAxis(chosen, axis, TargetOf(chosen, axis) + delta);
        ScrollRecord& record = Record(chosen);
        record.wheelFrame = g.FrameCount;
        record.seenFrame = g.FrameCount;
        core.lockWindow = chosen->ID;
        core.lockTime = core.clock;
        core.lockMouse = io.MousePos;
        core.lockDirection[axis] = delta > 0.0f ? 1.0f : -1.0f;
        core.lockDirection[axis == kAxisX ? kAxisY : kAxisX] = 0.0f;
    }
}

void StepMotion(ImGuiContext& g, float dt)
{
    ScrollCore& core = Core();
    const Motion& motion = T().motion;
    const float settle = std::max(1.0e-4f, motion.durationStandard * motion.scale);
    const float omega = 2.0f * std::numbers::pi_v<float> / settle;
    const bool reduced = ReduceMotion();
    for (ScrollRecord& record : core.records)
    {
        if (!record.moving[kAxisX] && !record.moving[kAxisY])
            continue;
        ImGuiWindow* window = ImGui::FindWindowByID(record.window);
        if (window == nullptr)
        {
            record.moving[kAxisX] = record.moving[kAxisY] = false;
            continue;
        }
        for (int axis = kAxisX; axis <= kAxisY; ++axis)
        {
            if (!record.moving[axis])
                continue;
            if (!record.fresh[axis])
            {
                // Someone else scrolled since the last step: a new target, or a position that
                // differs from the one written by more than Dear ImGui's rounding.
                const float pending = window->ScrollTarget[axis];
                const bool foreignTarget = pending < FLT_MAX && pending != record.written[axis];
                const bool moved = pending >= FLT_MAX &&
                                   std::fabs(window->Scroll[axis] - std::round(record.written[axis])) > 1.5f;
                if (foreignTarget || moved)
                {
                    record.moving[axis] = false;
                    continue;
                }
            }
            record.fresh[axis] = false;
            const float goal = std::clamp(record.target[axis], 0.0f, std::max(0.0f, window->ScrollMax[axis]));
            bool done = reduced;
            if (!done)
            {
                float x = record.pos[axis] - goal;
                float v = record.velocity[axis];
                SpringStep(x, v, omega, 1.0f, dt);
                record.pos[axis] = goal + x;
                record.velocity[axis] = v;
                done = std::fabs(x) < 0.5f && std::fabs(v) * std::max(dt, 1.0e-3f) < 0.5f;
            }
            if (done)
            {
                record.pos[axis] = goal;
                record.velocity[axis] = 0.0f;
                record.moving[axis] = false;
            }
            SetScrollAxis(window, axis, record.pos[axis]);
            record.written[axis] = record.pos[axis];
            record.lastActivity = core.clock;
            record.seenFrame = g.FrameCount;
        }
    }
}

// Claims the wheel for the next frame while the pointer is over a scroll area.
void ClaimWheel(ImGuiContext& g)
{
    for (ImGuiWindow* window = g.HoveredWindow; window != nullptr;
         window = (window->Flags & ImGuiWindowFlags_ChildWindow) != 0 ? window->ParentWindow : nullptr)
    {
        const ScrollRecord* record = FindRecord(window->ID);
        if (record != nullptr && record->area && record->seenFrame >= g.FrameCount - 1)
        {
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, WheelOwner());
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, WheelOwner());
            Core().claimedFrame = g.FrameCount;
            return;
        }
    }
}

void Prune(ImGuiContext& g)
{
    ImVector<ScrollRecord>& records = Core().records;
    for (int i = records.Size - 1; i >= 0; --i)
    {
        const ScrollRecord& record = records[i];
        if (record.moving[kAxisX] || record.moving[kAxisY])
            continue;
        if (g.FrameCount - record.seenFrame > kRecordLifetimeFrames)
            records.erase(records.Data + i);
    }
}

void ResetCore(ImGuiContext* context)
{
    ScrollCore& core = Core();
    core.records.clear();
    core.context = context;
    core.openDepth = 0;
    core.surfaceDepth = 0;
    core.surfaceOverflow = 0;
    core.claimedFrame = -1000;
    core.lockWindow = 0;
}

void ScrollNewFrame()
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr)
        return;
    ScrollCore& core = Core();
    if (core.context != context)
        ResetCore(context);
    const float dt = std::max(0.0f, DeltaTime());
    core.clock += dt;
    ExpireWheelLock(*context);
    RouteWheel(*context);
    StepMotion(*context, dt);
    ClaimWheel(*context);
    Prune(*context);
}

void ScrollEndFrame()
{
    ScrollCore& core = Core();
    if (core.openDepth > 0)
        ReportUsageError("BeginScrollAreaCore without EndScrollAreaCore: %d area(s) still open at EndFrame",
                         core.openDepth);
    core.openDepth = 0;
    const int surfaces = core.surfaceDepth + core.surfaceOverflow;
    if (surfaces > 0)
        ReportUsageError("PushScrollFadeSurface without PopScrollFadeSurface: %d surface(s) still pushed at EndFrame",
                         surfaces);
    core.surfaceDepth = 0;
    core.surfaceOverflow = 0;
    ListCoreEndFrame();
}

void ScrollShutdown()
{
    ResetCore(nullptr);
    ListCoreShutdown();
}

ImVec4 ResolveSurface(const ImVec4& requested, const ImGuiWindow* parent)
{
    if (requested.w > 0.0f)
        return requested;
    const ScrollCore& core = Core();
    if (core.surfaceDepth > 0)
        return core.surfaces[core.surfaceDepth - 1];
    const ImGuiWindow* root = parent->RootWindow != nullptr ? parent->RootWindow : parent;
    if ((root->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0)
        return T().colors.bgElevated;
    return T().colors.bg;
}

struct BarGeometry
{
    ImRect track;
    ImRect thumb;
};

// The track and thumb of `axis` for a bar `thickness` pixels wide at scroll offset `scroll`.
// `otherBar` leaves the corner free for the bar of the other axis.
BarGeometry Bar(const ImGuiWindow* window, int axis, float thickness, bool otherBar, float scroll)
{
    const ScrollTokens& tokens = T().scroll;
    const float inset = Px(tokens.barInset);
    const float corner = otherBar ? inset + Px(tokens.barThicknessHover) : 0.0f;
    const ImRect r = window->Rect();
    BarGeometry bar;
    if (axis == kAxisY)
        bar.track = ImRect(r.Max.x - inset - thickness, r.Min.y + inset, r.Max.x - inset, r.Max.y - inset - corner);
    else
        bar.track = ImRect(r.Min.x + inset, r.Max.y - inset - thickness, r.Max.x - inset - corner, r.Max.y - inset);
    const float length = std::max(0.0f, bar.track.Max[axis] - bar.track.Min[axis]);
    const float visible = r.Max[axis] - r.Min[axis];
    const float max = std::max(0.0f, window->ScrollMax[axis]);
    const float content = std::max(1.0f, visible + max);
    const float minimum = std::min(Px(tokens.minThumbLength), length);
    const float thumbLength = std::clamp(length * visible / content, minimum, length);
    const float t = max > 0.0f ? std::clamp(scroll / max, 0.0f, 1.0f) : 0.0f;
    const float start = bar.track.Min[axis] + t * (length - thumbLength);
    bar.thumb = bar.track;
    bar.thumb.Min[axis] = start;
    bar.thumb.Max[axis] = start + thumbLength;
    return bar;
}

void DrawFades(ImDrawList* drawList, const ImGuiWindow* window, const ImRect& clip, const OpenArea& open)
{
    const float length = Px(T().scroll.edgeFade);
    if (length <= 0.0f)
        return;
    const ImRect r = window->Rect();
    const ImU32 clear = ToU32(WithAlpha(open.surface, 0.0f));
    const auto strength = [&](float distance) { return std::clamp(distance / length, 0.0f, 1.0f); };
    const auto solid = [&](float amount) { return ToU32(WithAlpha(open.surface, open.surface.w * amount)); };
    drawList->PushClipRect(clip.Min, clip.Max, false);
    const float top = strength(window->Scroll.y);
    const float bottom = strength(window->ScrollMax.y - window->Scroll.y);
    if (top > 0.0f)
        FillGradientRect(drawList, r.Min, ImVec2(r.Max.x, r.Min.y + length), solid(top), clear, GradientDir::Vertical);
    if (bottom > 0.0f)
        FillGradientRect(drawList, ImVec2(r.Min.x, r.Max.y - length), r.Max, clear, solid(bottom), GradientDir::Vertical);
    if (open.horizontal)
    {
        const float left = strength(window->Scroll.x);
        const float right = strength(window->ScrollMax.x - window->Scroll.x);
        if (left > 0.0f)
            FillGradientRect(drawList, r.Min, ImVec2(r.Min.x + length, r.Max.y), solid(left), clear,
                             GradientDir::Horizontal);
        if (right > 0.0f)
            FillGradientRect(drawList, ImVec2(r.Max.x - length, r.Min.y), r.Max, clear, solid(right),
                             GradientDir::Horizontal);
    }
    drawList->PopClipRect();
}

} // namespace

void ScrollAreaTo(ImGuiWindow* window, ImVec2 target, bool animate)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr)
        return;
    const bool smooth = animate && !ReduceMotion();
    for (int axis = kAxisX; axis <= kAxisY; ++axis)
    {
        if (target[axis] < 0.0f)
            continue;
        if (smooth)
        {
            AnimateAxis(window, axis, target[axis]);
        }
        else
        {
            StopMotion(window, axis);
            SetScrollAxis(window, axis, target[axis]);
            if (ScrollRecord* record = FindRecord(window->ID))
                record->lastActivity = Core().clock;
        }
    }
}

void ScrollAreaRevealRect(ImGuiWindow* window, const ImRect& rect, ScrollAlign align, bool animate)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr)
        return;
    ImVec2 target(-1.0f, -1.0f);
    ImRect shown = rect;  // where the rect will be once this window has scrolled
    for (int axis = kAxisX; axis <= kAxisY; ++axis)
    {
        // The rect in content coordinates (0 is the top of the scrolled content).
        const float origin = window->InnerRect.Min[axis] - window->Scroll[axis];
        const float lo = rect.Min[axis] - origin;
        const float hi = rect.Max[axis] - origin;
        const float pad = window->WindowPadding[axis];
        const float view = std::max(0.0f, window->InnerRect.Max[axis] - window->InnerRect.Min[axis] - 2.0f * pad);
        const float current = TargetOf(window, axis);
        float wanted = current;
        const bool larger = hi - lo > view;
        switch (align)
        {
        case ScrollAlign::Start:
            wanted = lo - pad;
            break;
        case ScrollAlign::Center:
            wanted = larger ? lo - pad : (lo + hi) * 0.5f - (view * 0.5f + pad);
            break;
        case ScrollAlign::End:
            wanted = larger ? lo - pad : hi - view - pad;
            break;
        case ScrollAlign::Nearest:
            if (larger || lo - pad < current)
                wanted = lo - pad;
            else if (hi + pad > current + view + 2.0f * pad)
                wanted = hi - view - pad;
            break;
        }
        wanted = std::clamp(wanted, 0.0f, std::max(0.0f, window->ScrollMax[axis]));
        if (std::fabs(wanted - current) >= 0.5f)
            target[axis] = wanted;
        const float shift = window->Scroll[axis] - (target[axis] >= 0.0f ? wanted : current);
        shown.Min[axis] += shift;
        shown.Max[axis] += shift;
    }
    if (target.x >= 0.0f || target.y >= 0.0f)
        ScrollAreaTo(window, target, animate);
    // The windows around a child window bring the part of the rect it shows into their view too,
    // as keyboard navigation does, so a revealed row is on screen even when its area is not.
    ImGuiWindow* parent = window->ParentWindow;
    if ((window->Flags & ImGuiWindowFlags_ChildWindow) == 0 || (window->Flags & ImGuiWindowFlags_Popup) != 0 ||
        parent == nullptr)
        return;
    shown.ClipWithFull(window->InnerRect);
    if (shown.GetWidth() > 0.0f && shown.GetHeight() > 0.0f)
        ScrollAreaRevealRect(parent, shown, ScrollAlign::Nearest, animate);
}

ImVec2 ScrollAreaTarget(ImGuiWindow* window)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr)
        return ImVec2(0.0f, 0.0f);
    return ImVec2(TargetOf(window, kAxisX), TargetOf(window, kAxisY));
}

bool IsScrollAreaScrolling(ImGuiWindow* window)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr)
        return false;
    const ScrollRecord* record = FindRecord(window->ID);
    if (record == nullptr)
        return false;
    const int frame = GImGui->FrameCount;
    return record->moving[kAxisX] || record->moving[kAxisY] || record->wheelFrame == frame ||
           (record->dragAxis >= 0 && record->dragFrame >= frame - 1) || record->edgeFrame == frame;
}

ImGuiWindow* ScrollAreaAt(ImVec2 point)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return nullptr;
    ImGuiWindow* hovered = nullptr;
    ImGuiWindow* underMoving = nullptr;
    ImGui::FindHoveredWindowEx(point, false, &hovered, &underMoving);
    for (ImGuiWindow* window = hovered; window != nullptr;
         window = (window->Flags & ImGuiWindowFlags_ChildWindow) != 0 ? window->ParentWindow : nullptr)
    {
        if ((window->Flags & ImGuiWindowFlags_NoMouseInputs) != 0)
            continue;
        if (window->ScrollMax.x > 0.0f || window->ScrollMax.y > 0.0f)
            return window;
    }
    return nullptr;
}

bool RequestScrollAreaEdgeScroll(ImGuiWindow* window, ImVec2 point)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr)
        return false;
    const DragTokens& tokens = T().drag;
    const float zone = std::max(1.0f, Px(tokens.autoScrollZone));
    const float speed = Px(tokens.autoScrollSpeed);
    const float dt = std::max(0.0f, DeltaTime());
    ScrollRecord& record = Record(window);
    const bool horizontalAllowed = record.area ? record.horizontal : window->ScrollMax.x > 0.0f;
    bool scrolled = false;
    for (int axis = kAxisX; axis <= kAxisY; ++axis)
    {
        if (axis == kAxisX && !horizontalAllowed)
            continue;
        const float max = window->ScrollMax[axis];
        if (max <= 0.0f)
            continue;
        const float lo = window->InnerRect.Min[axis];
        const float hi = window->InnerRect.Max[axis];
        float depth = 0.0f;
        float direction = 0.0f;
        if (point[axis] < lo + zone)
        {
            depth = std::clamp((lo + zone - point[axis]) / zone, 0.0f, 1.0f);
            direction = -1.0f;
        }
        else if (point[axis] > hi - zone)
        {
            depth = std::clamp((point[axis] - (hi - zone)) / zone, 0.0f, 1.0f);
            direction = 1.0f;
        }
        const float current = window->Scroll[axis];
        if (depth <= 0.0f || (direction < 0.0f && current <= 0.0f) || (direction > 0.0f && current >= max))
        {
            record.edgeRest[axis] = 0.0f;
            continue;
        }
        record.edgeRest[axis] += direction * speed * depth * dt;
        const float whole = std::trunc(record.edgeRest[axis]);
        if (whole == 0.0f)
            continue;
        record.edgeRest[axis] -= whole;
        record.moving[axis] = false;
        SetScrollAxis(window, axis, std::clamp(current + whole, 0.0f, max));
        scrolled = true;
    }
    if (scrolled)
    {
        record.edgeFrame = GImGui->FrameCount;
        record.lastActivity = Core().clock;
    }
    record.seenFrame = GImGui->FrameCount;
    return scrolled;
}

bool BeginScrollAreaCore(const char* id, const ScrollAreaDesc& desc)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    return BeginScrollAreaCore(ImGui::GetID(id != nullptr ? id : ""), desc);
}

bool BeginScrollAreaCore(ImGuiID id, const ScrollAreaDesc& desc)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || id == 0)
        return false;
    ImGuiContext& g = *context;
    ScrollCore& core = Core();
    if (core.context != context)
        ResetCore(context);
    ImGuiWindow* parent = g.CurrentWindow;
    if (parent == nullptr || parent->SkipItems)
        return false;
    if (core.openDepth >= kScrollAreaDepth)
    {
        ReportUsageError("BeginScrollAreaCore nested deeper than %d; the area is not opened", kScrollAreaDepth);
        return false;
    }
    // A forced look for the bars, taken whether or not the area shows (one request per widget).
    const ForcedItemRequest forced = TakeForcedItemState();
    const ImVec4 surface = ResolveSurface(desc.fadeSurface, parent);
    const float padding = desc.padding < 0.0f ? FocusRingOutset() : desc.padding;

    ImGuiChildFlags childFlags = ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened;
    if (desc.fitHeight)
        childFlags |= ImGuiChildFlags_AutoResizeY;
    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
    ImVec2 size = desc.size;
    if (desc.fitHeight)
        size.y = 0.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    const bool visible = ImGui::BeginChild(id, size, childFlags, windowFlags);
    ImGui::PopStyleVar();
    if (!visible)
    {
        ImGui::EndChild();
        return false;
    }

    ImGuiWindow* window = g.CurrentWindow;
    ScrollRecord& record = Record(window);
    record.area = true;
    record.horizontal = desc.horizontal;
    record.seenFrame = g.FrameCount;
    if (window->Scroll.x != record.lastScroll.x || window->Scroll.y != record.lastScroll.y)
        record.lastActivity = core.clock;
    record.lastScroll = window->Scroll;
    // Without horizontal scrolling an offset on x would hide content with no bar to bring it back.
    if (!desc.horizontal && window->Scroll.x != 0.0f)
        ImGui::SetScrollX(window, 0.0f);

    OpenArea& open = core.open[core.openDepth++];
    open = OpenArea();
    open.window = window->ID;
    open.surface = surface;
    open.edgeFades = desc.edgeFades;
    open.horizontal = desc.horizontal;

    // The content is clipped at the area's own rectangle (not inset by half the padding as
    // Dear ImGui does), within what the parent shows, so focus rings and shadows of content near
    // the edge stay whole.
    ImRect clip = window->Rect();
    clip.ClipWithFull(parent->ClipRect);
    ImGui::PushClipRect(clip.Min, clip.Max, false);
    open.clipPushed = true;

    // The bars take the mouse before the content, so a row under a bar does not take the drag.
    const ScrollTokens& tokens = T().scroll;
    const ImGuiIO& io = g.IO;
    const bool bars[2] = {desc.horizontal && window->ScrollMax.x > 0.0f, window->ScrollMax.y > 0.0f};
    bool anyActive = false;
    for (int axis = kAxisY; axis >= kAxisX; --axis)
    {
        open.hasBar[axis] = bars[axis];
        record.hasBar[axis] = bars[axis];
        if (!bars[axis])
        {
            if (record.dragAxis == axis)
                record.dragAxis = -1;
            if (record.pageAxis == axis)
                record.pageAxis = -1;
            continue;
        }
        const bool other = bars[axis == kAxisY ? kAxisX : kAxisY];
        const BarGeometry hit = Bar(window, axis, Px(tokens.barThicknessHover), other, window->Scroll[axis]);
        ItemOptions options;
        options.press = PressMode::Click;
        options.repeat = true;
        options.tabStop = false;
        options.navigable = false;
        options.hitTarget = false;
        options.handCursor = false;
        options.part = axis == kAxisY ? 0 : 1;
        options.forced = &forced;
        const ItemState bar = PartBehavior(window->GetID(axis == kAxisY ? "##facet.vbar" : "##facet.hbar"),
                                           hit.track, options);
        const float mouse = io.MousePos[axis];
        const bool real = bar.forced == ForcedItemState::None;
        if (real && bar.activated)
        {
            StopMotion(window, axis);
            if (mouse >= hit.thumb.Min[axis] && mouse <= hit.thumb.Max[axis])
            {
                record.dragAxis = axis;
                record.grab = mouse - hit.thumb.Min[axis];
            }
            else
            {
                record.pageAxis = axis;
            }
        }
        if (real && bar.held && record.dragAxis == axis)
        {
            const float length = hit.track.Max[axis] - hit.track.Min[axis];
            const float travel = length - (hit.thumb.Max[axis] - hit.thumb.Min[axis]);
            const float t = travel > 0.0f ? std::clamp((mouse - record.grab - hit.track.Min[axis]) / travel, 0.0f, 1.0f)
                                          : 0.0f;
            const float value = std::round(t * window->ScrollMax[axis]);
            if (value != window->Scroll[axis])
                SetScrollAxis(window, axis, value);
            record.dragFrame = g.FrameCount;
            record.lastActivity = core.clock;
        }
        if (real && bar.pressed && record.pageAxis == axis)
        {
            // Page toward the pointer until the thumb (where it is going) reaches it. A page is
            // the visible length less one edge fade, so the line at the old edge stays in view.
            const float goal = TargetOf(window, axis);
            const BarGeometry atGoal = Bar(window, axis, Px(tokens.barThicknessHover), other, goal);
            const float page = std::max(0.0f, window->InnerRect.Max[axis] - window->InnerRect.Min[axis] -
                                                  Px(tokens.edgeFade));
            ImVec2 target(-1.0f, -1.0f);
            if (mouse < atGoal.thumb.Min[axis])
                target[axis] = std::max(0.0f, goal - page);
            else if (mouse > atGoal.thumb.Max[axis])
                target[axis] = goal + page;
            if (target[axis] >= 0.0f)
                ScrollAreaTo(window, target, true);
            record.lastActivity = core.clock;
        }
        if (!bar.held)
        {
            if (record.dragAxis == axis)
                record.dragAxis = -1;
            if (record.pageAxis == axis)
                record.pageAxis = -1;
        }
        open.expand[axis] = std::max(bar.hoverAmount, bar.pressAmount);
        open.active[axis] = bar.pressAmount > 0.0f ? 1.0f : bar.hoverAmount;
        anyActive = anyActive || bar.hovered || bar.held;
    }

    // The bars show while the area is hovered or scrolled, and fade out scroll.barHideDelay after
    // the last scroll once the pointer has left.
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy);
    const bool recent = core.clock - record.lastActivity < tokens.barHideDelay;
    const bool show = hovered || anyActive || recent || IsScrollAreaScrolling(window);
    const ImGuiID barId = window->GetID("##facet.vbar");
    open.alpha = Tween(barId, kBarAlphaSub, show ? 1.0f : 0.0f, T().motion.durationStandard);
    return true;
}

void EndScrollAreaCore()
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr)
        return;
    ScrollCore& core = Core();
    ImGuiWindow* window = context->CurrentWindow;
    if (core.openDepth == 0)
    {
        ReportUsageError("EndScrollAreaCore without BeginScrollAreaCore");
        return;
    }
    const OpenArea open = core.open[--core.openDepth];
    if (window == nullptr || window->ID != open.window)
    {
        ReportUsageError("EndScrollAreaCore called in another window than its BeginScrollAreaCore");
        return;
    }
    // Fades and bars go on top of everything in the area, child windows included: into the draw
    // list of its front-most visible child window (its own one when it has none), which Dear ImGui
    // renders last, clipped to the area.
    ImDrawList* drawList = ImGui::FindFrontMostVisibleChildWindow(window)->DrawList;
    ImRect clip = window->Rect();
    if (window->ParentWindow != nullptr)
        clip.ClipWithFull(window->ParentWindow->ClipRect);
    if (open.edgeFades)
        DrawFades(drawList, window, clip, open);
    drawList->PushClipRect(clip.Min, clip.Max, false);

    ScrollRecord& record = Record(window);
    const Theme& theme = T();
    const ScrollTokens& tokens = theme.scroll;
    for (int axis = kAxisX; axis <= kAxisY; ++axis)
    {
        record.hasBar[axis] = open.hasBar[axis];
        if (!open.hasBar[axis])
            continue;
        const float thickness =
            Px(tokens.barThickness) + (Px(tokens.barThicknessHover) - Px(tokens.barThickness)) * open.expand[axis];
        const bool other = open.hasBar[axis == kAxisY ? kAxisX : kAxisY];
        const BarGeometry bar = Bar(window, axis, thickness, other, window->Scroll[axis]);
        record.track[axis] = bar.track;
        record.thumb[axis] = bar.thumb;
        if (open.alpha <= 0.0f)
            continue;
        const float radius = std::min(Px(theme.radii.element), thickness * 0.5f);
        if (open.expand[axis] > 0.0f)
        {
            const ImVec4& track = theme.colors.fill[3];
            FillRect(drawList, bar.track.Min, bar.track.Max,
                     ToU32(WithAlpha(track, track.w * open.alpha * open.expand[axis])), radius);
        }
        const ImVec4 thumb = Mix(theme.colors.scrollThumb, theme.colors.scrollThumbHover, open.active[axis]);
        FillRect(drawList, bar.thumb.Min, bar.thumb.Max, ToU32(WithAlpha(thumb, thumb.w * open.alpha)), radius);
    }
    drawList->PopClipRect();
    if (open.clipPushed)
        ImGui::PopClipRect();
    ImGui::EndChild();
}

bool ScrollAreaBarRects(ImGuiWindow* window, ImGuiAxis axis, ImRect* track, ImRect* thumb)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr || (axis != ImGuiAxis_X && axis != ImGuiAxis_Y))
        return false;
    const ScrollRecord* record = FindRecord(window->ID);
    if (record == nullptr || !record->area || !record->hasBar[axis])
        return false;
    if (track != nullptr)
        *track = record->track[axis];
    if (thumb != nullptr)
        *thumb = record->thumb[axis];
    return true;
}

void PushScrollFadeSurface(const ImVec4& surface)
{
    ScrollCore& core = Core();
    if (core.surfaceDepth >= kScrollFadeSurfaceDepth)
    {
        if (core.surfaceOverflow == 0)
            ReportUsageError("PushScrollFadeSurface nested deeper than %d; the extra surfaces are ignored",
                             kScrollFadeSurfaceDepth);
        ++core.surfaceOverflow;
        return;
    }
    core.surfaces[core.surfaceDepth++] = surface;
}

void PopScrollFadeSurface()
{
    ScrollCore& core = Core();
    if (core.surfaceOverflow > 0)
    {
        --core.surfaceOverflow;
        return;
    }
    if (core.surfaceDepth == 0)
    {
        ReportUsageError("PopScrollFadeSurface without PushScrollFadeSurface");
        return;
    }
    --core.surfaceDepth;
}

// Routes the wheel and steps the smooth scrolls in NewFrame, checks open areas, fade surfaces and
// the list core's row ranges in EndFrame.
Subsystem ScrollSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "scroll";
    subsystem.newFrame = &ScrollNewFrame;
    subsystem.endFrame = &ScrollEndFrame;
    subsystem.shutdown = &ScrollShutdown;
    return subsystem;
}

} // namespace facet::internal
