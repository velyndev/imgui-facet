// Popup host (popup_host.h): the level records on top of Dear ImGui's popup stack, placement,
// the surface, the open motion and the close motion that replays a level's last frame, the Escape
// and context menu key routing, focus return, the modal scrim and tooltips.
//
// Frame flow:
//   NewFrame  mirror Dear ImGui's popup stack (a click outside closed levels in its EndFrame),
//             apply an Escape press the host owns, note scrim clicks, update tooltip input.
//   frame     widgets open, submit and close levels; BeginPopupLevel places the window and draws
//             the surface behind the content.
//   EndFrame  close levels that were not submitted, mirror the stack again, copy the content of
//             every shown level (popup_replay.h), apply the open motion to the draw lists, draw
//             closing levels from their copies, claim Escape for the next frame, check that a
//             returned focus rests on a living item.

#include "internal/popup_host.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/device.h"
#include "internal/focus.h"
#include "internal/keys.h"
#include "internal/popup_place.h"
#include "internal/popup_replay.h"
#include "internal/subsystem.h"

namespace facet::internal
{
namespace
{

constexpr int kLevelCapacity = 32;
constexpr int kBeginCapacity = 16;
constexpr int kClaimCapacity = 16;
constexpr int kMenuTargetCapacity = 64;

enum class Phase
{
    Idle,     // not shown (a forced level lives here too: it shows on its forced frames only)
    Open,
    Closing,  // fading out from its snapshot
};

struct Level
{
    ImGuiID id = 0;
    PopupAnchor anchor;
    ImRect anchorRegion;         // the window that holds the anchor (AnchorRegion); empty for none
    PopupOptions options;
    Phase phase = Phase::Idle;
    ImGuiID parent = 0;

    std::uint64_t openedFrame = 0;
    std::uint64_t submittedFrame = 0;
    std::uint64_t forcedFrame = 0;
    std::uint64_t closeFrame = 0;
    std::uint64_t lastUseFrame = 0;
    std::uint64_t scrimClickFrame = 0;
    PopupCloseReason closeReason = PopupCloseReason::None;

    bool hasSide = false;
    PopupSide side = PopupSide::Below;
    ImRect rect;                 // placed rect in its last submission
    ImRect placedAnchor;         // the anchor rect of that placement
    bool coversAnchor = false;
    float openTime = 0.0f;       // seconds shown (from its first visible frame)
    float closeTime = 0.0f;      // seconds into the close motion
    std::uint64_t fadeStartFrame = 0;
    float wrapWidth = 0.0f;      // tooltips: text wrap width in pixels
    ImGuiID restoreWindow = 0;   // panels: the window that had the keyboard focus when it opened
    ImGuiID ownerWindow = 0;     // panels: the root window it was opened from, which it stays in front of
    int stackDepthAtOpen = 0;    // panels: the size of Dear ImGui's popup stack when it opened

    // This frame's submission.
    ImGuiWindow* window = nullptr;
    bool plainWindow = false;    // a tooltip or forced level: a plain window, not a Dear ImGui popup
    ImGuiID windowOwner = 0;     // the level that last drew in this slot's plain window (kept across ids)
    int surfaceCommands = 0;     // commands at the front of the root draw list that are the surface
    int scrimCommands = 0;       // of those, the scrim's (a Modal)
    int scrimVertexBegin = 0;
    int scrimVertexEnd = 0;

    PopupSnapshot snapshot;
};

struct BeginEntry
{
    Level* level = nullptr;
    bool imguiPopup = false;
    bool pushedHoverFlag = false;
    bool pushedWrap = false;
};

struct Claim
{
    ImGuiID owner = 0;
    ImGuiWindow* window = nullptr;
};

struct TooltipState
{
    ImGuiID hoverItem = 0;
    double hoverStart = 0.0;
    bool hoverImmediate = false;
    std::uint64_t hoverSeenFrame = 0;
    ImGuiID focusItem = 0;
    double focusStart = 0.0;
    bool focusImmediate = false;
    std::uint64_t focusSeenFrame = 0;
    ImGuiID shownItem = 0;
    std::uint64_t shownFrame = 0;
    double lastVisible = -1.0e9;
    ImGuiID pressSuppressed = 0;  // hidden until the pointer leaves this item
    bool keySuppressed = false;   // hidden until the pointer moves
    ImGuiID anchorItem = 0;
    bool anchorAtPointer = false;
    ImVec2 anchorPoint;
};

struct HostState
{
    Level levels[kLevelCapacity];
    BeginEntry begin[kBeginCapacity];
    int beginDepth = 0;
    ImGuiContext* context = nullptr;
    std::uint64_t frame = 0;          // facet frame of the last NewFrame hook
    bool inEndFrame = false;
    bool clickedLastFrame = false;      // any mouse button was clicked in the last frame
    bool leftClickedLastFrame = false;
    ImGuiID escapeWinner = 0;        // the shell that gets this frame's Escape press
    Claim claims[2][kClaimCapacity];
    int claimCount[2] = {0, 0};
    int claimCurrent = 0;
    ImGuiID menuTargets[2][kMenuTargetCapacity];
    int menuTargetCount[2] = {0, 0};
    std::uint64_t focusCheckFrame = 0;
    TooltipState tooltip;
};

HostState& State()
{
    static HostState state;
    return state;
}

const ImGuiID kEscapeOwner = ImHashStr("facet.popup_host.escape");
const ImGuiID kMouseOwner = ImHashStr("facet.popup_host.dismiss");
const ImGuiID kContextOwner = ImHashStr("facet.popup_host.context");

bool InFrame()
{
    const HostState& s = State();
    ImGuiContext* g = ImGui::GetCurrentContext();
    return IsInitialized() && g != nullptr && g->WithinFrameScope && g == s.context && s.frame == FrameIndex() &&
           s.frame != 0;
}

Level* FindLevel(ImGuiID id)
{
    if (id == 0)
        return nullptr;
    for (Level& level : State().levels)
    {
        if (level.id == id)
            return &level;
    }
    return nullptr;
}

void EndMotion(Level& level)
{
    if (level.phase == Phase::Closing)
        level.phase = Phase::Idle;
    level.snapshot.Clear();
}

// A record for `id`: its own, a free one, or the one used longest ago that is neither open nor
// closing; a closing one is ended when nothing else is free. Null when every record is open.
Level* FindOrAddLevel(ImGuiID id)
{
    if (Level* level = FindLevel(id))
        return level;
    HostState& s = State();
    Level* best = nullptr;
    for (Level& level : s.levels)
    {
        if (level.phase != Phase::Idle || (level.id != 0 && level.forcedFrame == s.frame))
            continue;
        if (level.id == 0)
        {
            best = &level;
            break;
        }
        if (best == nullptr || level.lastUseFrame < best->lastUseFrame)
            best = &level;
    }
    if (best == nullptr)
    {
        for (Level& level : s.levels)
        {
            if (level.phase == Phase::Closing && (best == nullptr || level.closeFrame < best->closeFrame))
                best = &level;
        }
    }
    if (best == nullptr)
        return nullptr;
    EndMotion(*best);
    best->id = id;
    best->anchor = {};
    best->options = {};
    best->phase = Phase::Idle;
    best->parent = 0;
    best->openedFrame = best->submittedFrame = best->forcedFrame = best->closeFrame = best->scrimClickFrame = 0;
    best->closeReason = PopupCloseReason::None;
    best->hasSide = false;
    best->rect = ImRect();
    best->placedAnchor = ImRect();
    best->openTime = best->closeTime = 0.0f;
    best->window = nullptr;
    best->wrapWidth = 0.0f;
    return best;
}

int ImGuiPopupIndex(ImGuiID id)
{
    const ImGuiContext& g = *GImGui;
    for (int i = 0; i < g.OpenPopupStack.Size; ++i)
    {
        if (g.OpenPopupStack[i].PopupId == id)
            return i;
    }
    return -1;
}

bool IsStackLevel(const Level& level)
{
    return level.options.kind != PopupKind::Tooltip && level.options.kind != PopupKind::Panel;
}

// A level drawn in a plain window of its record's slot instead of a Dear ImGui popup.
bool IsPlainKind(PopupKind kind)
{
    return kind == PopupKind::Tooltip || kind == PopupKind::Panel;
}

// A Panel that closes while the keyboard focus is inside it gives the focus back to the window
// that had it when it opened; Dear ImGui then restores that window's last focused item.
void RestorePanelFocus(const Level& level)
{
    ImGuiContext& g = *GImGui;
    if (level.options.kind != PopupKind::Panel || level.window == nullptr || g.NavWindow == nullptr ||
        g.NavWindow->RootWindow != level.window)
        return;
    ImGuiWindow* back = level.restoreWindow != 0 ? ImGui::FindWindowByID(level.restoreWindow) : nullptr;
    if (back != nullptr && back->WasActive && back != level.window)
        ImGui::FocusWindow(back, ImGuiFocusRequestFlags_RestoreFocusedChild);
    else
        ImGui::FocusWindow(nullptr);
}

float MotionScale()
{
    return std::max(0.0f, T().motion.scale);
}

// Starts the close motion of `level`, or ends it at once when there is nothing to replay.
void BeginClose(Level& level, PopupCloseReason reason, bool motion)
{
    HostState& s = State();
    level.phase = Phase::Closing;
    level.closeReason = reason;
    // A level closed in facet::EndFrame reports its reason to the widget in the next frame.
    level.closeFrame = s.inEndFrame ? s.frame + 1 : s.frame;
    level.closeTime = 0.0f;
    level.fadeStartFrame = level.submittedFrame == s.frame ? s.frame + 1 : s.frame;
    // A level submitted in this frame takes its snapshot at EndFrame; otherwise the copy of the
    // last frame it was shown must exist already.
    const bool canReplay = level.submittedFrame == s.frame || level.snapshot.Valid();
    if (!motion || ReduceMotion() || MotionScale() <= 0.0f || !canReplay || level.openTime <= 0.0f)
        EndMotion(level);
}

// True when screen point `p` lies inside a level that stays open and lets its own content take
// clicks while a child is open (a Popup: menus and popovers).
bool InsideRemainingPopupLevel(ImVec2 p)
{
    const ImGuiContext& g = *GImGui;
    for (const ImGuiPopupData& data : g.OpenPopupStack)
    {
        const Level* level = FindLevel(data.PopupId);
        if (level != nullptr && level->phase == Phase::Open && level->options.kind == PopupKind::Popup &&
            level->rect.Contains(p))
            return true;
    }
    return false;
}

// After a click outside closed levels, the left button belongs to the host until it is released,
// so the release does not press what lies under it (unless it lands inside a menu level that stays
// open, where the click is meant for its rows).
void HoldDismissingClick()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (InsideRemainingPopupLevel(io.MouseClickedPos[ImGuiMouseButton_Left]))
        return;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        ImGui::SetKeyOwner(ImGuiKey_MouseLeft, kMouseOwner, ImGuiInputFlags_LockUntilRelease);
}

bool AnyMouseClicked()
{
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
    {
        if (ImGui::IsMouseClicked(button, ImGuiInputFlags_None, ImGuiKeyOwner_Any))
            return true;
    }
    return false;
}

// Levels Dear ImGui closed on its own (a click outside, a popup opened at their depth by other
// code, CloseCurrentPopup) start their close motion here.
void SyncWithImGuiStack(bool clicked, bool leftClicked)
{
    HostState& s = State();
    bool closed = false;
    for (Level& level : s.levels)
    {
        if (level.phase != Phase::Open || !IsStackLevel(level) || ImGuiPopupIndex(level.id) >= 0)
            continue;
        const PopupCloseReason reason = clicked ? PopupCloseReason::ClickOutside : PopupCloseReason::Closed;
        BeginClose(level, reason, true);
        closed = true;
    }
    if (closed)
    {
        s.focusCheckFrame = s.frame + 1;
        if (leftClicked)
            HoldDismissingClick();
    }
}

// Closes `level` and every level above it in Dear ImGui's stack. `restoreFocus` gives the focus
// back to the window (and item) that had it when the level opened.
void CloseStackFrom(Level& level, PopupCloseReason reason, bool restoreFocus, bool motion)
{
    HostState& s = State();
    const int index = ImGuiPopupIndex(level.id);
    if (index < 0)
    {
        BeginClose(level, reason, motion);
        return;
    }
    const ImGuiContext& g = *GImGui;
    for (int i = g.OpenPopupStack.Size - 1; i >= index; --i)
    {
        Level* above = FindLevel(g.OpenPopupStack[i].PopupId);
        if (above != nullptr && above->phase == Phase::Open)
            BeginClose(*above, i == index ? reason : PopupCloseReason::Closed, motion);
    }
    ImGui::ClosePopupToLevel(index, restoreFocus);
    s.focusCheckFrame = s.frame + 1;
}

// A click on a Popup level that has open levels above it (a menu row while its submenu is open)
// closes those levels before the frame's widgets run, so the row takes the click. Dear ImGui
// does not do this itself: the row gives its window the focus without closing anything. Clicks
// anywhere else are left to Dear ImGui, which closes the levels in its EndFrame while the
// control under the click stays blocked.
void CloseLevelsAboveClickedPopup()
{
    const ImGuiContext& g = *GImGui;
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
    {
        if (!ImGui::IsMouseClicked(button, ImGuiInputFlags_None, ImGuiKeyOwner_Any))
            continue;
        const ImVec2 at = g.IO.MouseClickedPos[button];
        for (int i = g.OpenPopupStack.Size - 1; i >= 0; --i)
        {
            const Level* level = FindLevel(g.OpenPopupStack[i].PopupId);
            const bool ours = level != nullptr && level->phase == Phase::Open;
            const ImGuiWindow* window = g.OpenPopupStack[i].Window;
            const bool contains = ours ? level->rect.Contains(at) : (window != nullptr && window->Rect().Contains(at));
            if (!contains)
                continue;
            if (ours && level->options.kind == PopupKind::Popup && i + 1 < g.OpenPopupStack.Size)
            {
                Level* above = FindLevel(g.OpenPopupStack[i + 1].PopupId);
                if (above != nullptr && above->phase == Phase::Open)
                    CloseStackFrom(*above, PopupCloseReason::ClickOutside, true, true);
                else
                    ImGui::ClosePopupToLevel(i + 1, true);
            }
            return;
        }
        return;
    }
}

// Dear ImGui closes a popup on a click outside only while the keyboard focus is in it: an item of
// another window that takes the click also takes the mouse button, and Dear ImGui then sees no
// click on empty space. A Popup level that does not take the focus (a completion list, the list of
// a dropdown driven from its field) would stay open under such a click, so the host closes it here,
// before the frame's items run: a click outside the level and outside every level above it closes
// it and those levels. The left button is then held until its release, so the click presses
// nothing under it, except on the level's own anchor (a text field placing its caret); a widget
// that must not react to that click there reads PopupLevelClosedReason.
void CloseUnfocusedLevelsOnClickOutside()
{
    const ImGuiContext& g = *GImGui;
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
    {
        if (!ImGui::IsMouseClicked(button, ImGuiInputFlags_None, ImGuiKeyOwner_Any))
            continue;
        const ImVec2 at = g.IO.MouseClickedPos[button];
        for (int i = 0; i < g.OpenPopupStack.Size; ++i)
        {
            Level* level = FindLevel(g.OpenPopupStack[i].PopupId);
            if (level == nullptr || level->phase != Phase::Open || level->options.kind != PopupKind::Popup ||
                level->options.takeFocus)
                continue;
            bool inside = false;
            for (int j = i; j < g.OpenPopupStack.Size && !inside; ++j)
            {
                const Level* above = FindLevel(g.OpenPopupStack[j].PopupId);
                const ImGuiWindow* window = g.OpenPopupStack[j].Window;
                if (above != nullptr && above->phase == Phase::Open)
                    inside = above->rect.Contains(at);
                else if (above == nullptr && window != nullptr)
                    inside = window->Rect().Contains(at);
            }
            if (inside)
                continue;
            const bool onAnchor = level->anchor.rect.Contains(at);
            CloseStackFrom(*level, PopupCloseReason::ClickOutside, false, true);
            if (button == ImGuiMouseButton_Left && !onAnchor)
                HoldDismissingClick();
            return;
        }
    }
}

// True when screen point `p` lies in a popup level opened after Dear ImGui's popup stack had
// `depth` entries (a list opened from a panel's content), or in a panel opened from inside `owner`.
bool InsideLevelsOpenedAfter(ImVec2 p, int depth, ImGuiID owner)
{
    const ImGuiContext& g = *GImGui;
    for (int i = std::max(depth, 0); i < g.OpenPopupStack.Size; ++i)
    {
        const Level* level = FindLevel(g.OpenPopupStack[i].PopupId);
        const ImGuiWindow* window = g.OpenPopupStack[i].Window;
        if (level != nullptr && level->phase == Phase::Open ? level->rect.Contains(p)
                                                            : (window != nullptr && window->Rect().Contains(p)))
            return true;
    }
    for (const Level& level : State().levels)
    {
        if (level.phase == Phase::Open && level.options.kind == PopupKind::Panel && level.parent == owner && owner != 0 &&
            level.rect.Contains(p))
            return true;
    }
    return false;
}

// Panels with closeOnClickOutside close on a press of any button outside them, their anchor and
// the levels opened after them. The press is left alone, so it still reaches what is under it.
void ClosePanelsOnClickOutside()
{
    const ImGuiContext& g = *GImGui;
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
    {
        if (!ImGui::IsMouseClicked(button, ImGuiInputFlags_None, ImGuiKeyOwner_Any))
            continue;
        const ImVec2 at = g.IO.MouseClickedPos[button];
        for (Level& level : State().levels)
        {
            if (level.phase != Phase::Open || level.options.kind != PopupKind::Panel || !level.options.closeOnClickOutside)
                continue;
            if (level.rect.Contains(at) || level.anchor.rect.Contains(at) ||
                InsideLevelsOpenedAfter(at, level.stackDepthAtOpen, level.id))
                continue;
            BeginClose(level, PopupCloseReason::ClickOutside, true);
            State().focusCheckFrame = State().frame + 1;
        }
    }
}

// The panel Escape closes first: the newest open panel with closeOnEscape that no popup level was
// opened above since it opened. Null when there is none.
Level* TopEscapePanel()
{
    const ImGuiContext& g = *GImGui;
    Level* best = nullptr;
    for (Level& level : State().levels)
    {
        if (level.phase != Phase::Open || level.options.kind != PopupKind::Panel || !level.options.closeOnEscape ||
            level.stackDepthAtOpen < g.OpenPopupStack.Size)
            continue;
        if (best == nullptr || level.openedFrame > best->openedFrame ||
            (level.openedFrame == best->openedFrame && &level > best))
            best = &level;
    }
    return best;
}

// ---- Placement and surface ------------------------------------------------------------------

ImRect WorkBounds()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = Px(T().overlay.viewportMargin);
    ImRect bounds(ImVec2(std::ceil(viewport->WorkPos.x + margin), std::ceil(viewport->WorkPos.y + margin)),
                  ImVec2(std::floor(viewport->WorkPos.x + viewport->WorkSize.x - margin),
                         std::floor(viewport->WorkPos.y + viewport->WorkSize.y - margin)));
    bounds.Max.x = std::max(bounds.Max.x, bounds.Min.x);
    bounds.Max.y = std::max(bounds.Max.y, bounds.Min.y);
    return bounds;
}

ImVec4 ViewportClip()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    return ImVec4(viewport->Pos.x, viewport->Pos.y, viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
}

ImVec2 LevelPadding(const PopupOptions& options)
{
    if (options.padding.x >= 0.0f && options.padding.y >= 0.0f)
        return options.padding;
    const Spacing& spacing = T().spacing;
    switch (options.kind)
    {
    case PopupKind::Popup:
        return Px(ImVec2(spacing.xs, spacing.xs));
    case PopupKind::Modal:
    case PopupKind::Panel:
        return Px(ImVec2(spacing.xl, spacing.xl));
    case PopupKind::Tooltip:
        return Px(ImVec2(spacing.sm, spacing.xs));
    }
    return ImVec2(0.0f, 0.0f);
}

int LevelElevation(const PopupOptions& options)
{
    if (options.elevation > 0)
        return std::min(options.elevation, 3);
    switch (options.kind)
    {
    case PopupKind::Popup:
    case PopupKind::Panel:
        return 2;
    case PopupKind::Modal:
        return 3;
    case PopupKind::Tooltip:
        return 1;
    }
    return 2;
}

float LevelRadius(const PopupOptions& options)
{
    if (options.radius >= 0.0f)
        return options.radius;
    const Radii& radii = T().radii;
    switch (options.kind)
    {
    case PopupKind::Popup:
    case PopupKind::Panel:
        return Px(radii.popover);
    case PopupKind::Modal:
        return Px(radii.window);
    case PopupKind::Tooltip:
        return Px(radii.control);
    }
    return Px(radii.popover);
}

bool AnchorIsEmpty(const PopupAnchor& anchor)
{
    return anchor.rect.Min.x == 0.0f && anchor.rect.Min.y == 0.0f && anchor.rect.Max.x == 0.0f && anchor.rect.Max.y == 0.0f;
}

// The window a level's anchor lies in, for the alignment rule of popup_place.h: the current window
// of the call that opens or moves a Popup level, when that window is part of the page (not itself
// a popup or tooltip, whose submenus keep their alignment). Empty otherwise.
ImRect AnchorRegion(const PopupOptions& options)
{
    const ImGuiWindow* window = GImGui->CurrentWindow;
    if ((options.kind != PopupKind::Popup && options.kind != PopupKind::Panel) || window == nullptr)
        return ImRect();
    const ImGuiWindow* root = window->RootWindow != nullptr ? window->RootWindow : window;
    if ((root->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0)
        return ImRect();
    return window->Rect();
}

void PlaceLevel(Level& level, ImVec2 wanted)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    // A level with a fixed rect (a drawer at a display edge) sits exactly there.
    if (level.options.placeRect.GetWidth() > 0.0f && level.options.placeRect.GetHeight() > 0.0f)
    {
        level.rect = level.options.placeRect;
        level.side = level.anchor.side;
        level.hasSide = true;
        level.coversAnchor = true;
        level.placedAnchor = level.anchor.rect;
        return;
    }
    PopupPlaceInput in;
    in.anchor = level.anchor.rect;
    in.region = level.anchorRegion;
    in.side = level.anchor.side;
    in.align = level.anchor.align;
    in.matchWidth = level.anchor.matchWidth;
    in.fallback = level.anchor.fallback;
    in.fallbackSide = level.anchor.fallbackSide;
    in.fallbackAlign = level.anchor.fallbackAlign;
    in.size = wanted;
    in.minWidth = level.options.minWidth;
    in.bounds = WorkBounds();
    in.gap = Px(T().overlay.anchorGap);
    in.minExtent = Px(T().metrics.controlHeight) * 2.0f;
    // The side sticks while only the content changes (a filtered list that grows and shrinks does
    // not jump between sides); when the anchor moves, the level is placed afresh.
    const ImRect& last = level.placedAnchor;
    const bool anchorStill = std::fabs(last.Min.x - in.anchor.Min.x) < 0.5f && std::fabs(last.Min.y - in.anchor.Min.y) < 0.5f &&
                             std::fabs(last.Max.x - in.anchor.Max.x) < 0.5f && std::fabs(last.Max.y - in.anchor.Max.y) < 0.5f;
    in.keepSide = level.hasSide && anchorStill;
    in.currentSide = level.side;
    level.placedAnchor = in.anchor;
    in.center = level.options.kind == PopupKind::Modal && AnchorIsEmpty(level.anchor);
    PopupPlacement placed = PlacePopup(in);
    // Content taller than the room scrolls; the scrollbar takes width from the content.
    if (placed.rect.GetHeight() + 0.5f < wanted.y && level.options.size.x <= 0.0f)
    {
        in.size.x = wanted.x + style.ScrollbarSize;
        in.keepSide = true;
        in.currentSide = placed.side;
        placed = PlacePopup(in);
    }
    level.side = placed.side;
    level.hasSide = true;
    level.rect = placed.rect;
    level.coversAnchor = placed.coversAnchor;
}

// Makes the next primitive start a command of its own.
void BreakCommand(ImDrawList* drawList)
{
    const ImDrawCmd& last = drawList->CmdBuffer.back();
    if (last.ElemCount != 0 || last.UserCallback != nullptr)
        drawList->AddDrawCmd();
}

// Where DrawSurface put its commands: [first, end) in all, [first, scrimEnd) the scrim.
struct SurfaceCommands
{
    int first = 0;
    int scrimEnd = 0;
    int end = 0;
};

// Draws the scrim (a Modal) and the surface of `level` at `rect` into `drawList`, and records
// the scrim's vertices in `level`. Starts and ends on command boundaries.
SurfaceCommands DrawSurface(ImDrawList* drawList, Level& level, const ImRect& rect)
{
    const Theme& theme = T();
    const PopupKind kind = level.options.kind;
    const ImVec4 clip = ViewportClip();
    SurfaceCommands commands;
    BreakCommand(drawList);
    commands.first = drawList->CmdBuffer.Size - 1;
    level.scrimVertexBegin = level.scrimVertexEnd = drawList->VtxBuffer.Size;
    if (kind == PopupKind::Modal)
    {
        drawList->PushClipRect(ImVec2(clip.x, clip.y), ImVec2(clip.z, clip.w), false);
        drawList->AddRectFilled(ImVec2(clip.x, clip.y), ImVec2(clip.z, clip.w), ToU32(theme.colors.scrim));
        drawList->PopClipRect();
        BreakCommand(drawList);
        level.scrimVertexEnd = drawList->VtxBuffer.Size;
    }
    commands.scrimEnd = drawList->CmdBuffer.Size - 1;
    const float radius = LevelRadius(level.options);
    const int elevation = LevelElevation(level.options);
    const ImDrawFlags corners = level.options.corners;
    drawList->PushClipRect(ImVec2(clip.x, clip.y), ImVec2(clip.z, clip.w), false);
    if (level.options.glass)
    {
        GlassSpec spec;
        spec.shadow = ElevationShadow(elevation);
        DrawGlass(drawList, rect.Min, rect.Max, radius, spec);
    }
    else
    {
        DrawElevation(drawList, rect.Min, rect.Max, radius, elevation);
        FillRect(drawList, rect.Min, rect.Max, ToU32(theme.colors.ElevatedSurface(elevation)), radius, corners);
        StrokeRect(drawList, rect.Min, rect.Max, ToU32(theme.colors.separator), radius, HairlineThickness(), corners);
    }
    drawList->PopClipRect();
    BreakCommand(drawList);
    commands.end = drawList->CmdBuffer.Size - 1;
    return commands;
}

// The surface goes behind what Dear ImGui drew in Begin (a scrollbar): the commands
// [first, end) move to the front of the draw list. The command at `end` (the empty current one,
// whose index offset is the end of the index buffer) stays last, so drawing continues correctly.
void MoveCommandsToFront(ImDrawList* drawList, int first, int end)
{
    if (first <= 0 || end <= first)
        return;
    std::rotate(drawList->CmdBuffer.Data, drawList->CmdBuffer.Data + first, drawList->CmdBuffer.Data + end);
}

// ---- Motion ----------------------------------------------------------------------------------

struct Motion
{
    float alpha = 1.0f;
    float scale = 1.0f;
    ImVec2 pivot;
    ImVec2 translate;
};

// The look at `amount` (0 gone, 1 settled) for a level placed at `rect`.
Motion MotionAt(const Level& level, float amount)
{
    Motion m;
    amount = std::clamp(amount, 0.0f, 1.0f);
    m.alpha = amount;
    const ImRect& r = level.rect;
    if (level.options.slideIn)
    {
        // The whole level moves in from its display edge, at full size.
        m.scale = 1.0f;
        m.pivot = r.GetCenter();
        const float rest = 1.0f - amount;
        switch (level.options.slideEdge)
        {
        case PopupSide::Below:
            m.translate = ImVec2(0.0f, r.GetHeight() * rest);
            break;
        case PopupSide::Above:
            m.translate = ImVec2(0.0f, -r.GetHeight() * rest);
            break;
        case PopupSide::After:
            m.translate = ImVec2(r.GetWidth() * rest, 0.0f);
            break;
        case PopupSide::Before:
            m.translate = ImVec2(-r.GetWidth() * rest, 0.0f);
            break;
        }
        return m;
    }
    m.scale = T().motion.popupScaleFrom + (1.0f - T().motion.popupScaleFrom) * amount;
    const ImVec2 anchorCenter = level.anchor.rect.GetCenter();
    const float slide = Px(T().overlay.anchorGap) * (1.0f - amount);
    const bool centered =
        level.coversAnchor || (level.options.kind == PopupKind::Modal && AnchorIsEmpty(level.anchor));
    if (centered)
    {
        m.pivot = r.GetCenter();
        return m;
    }
    const float cx = std::clamp(anchorCenter.x, r.Min.x, r.Max.x);
    const float cy = std::clamp(anchorCenter.y, r.Min.y, r.Max.y);
    switch (level.side)
    {
    case PopupSide::Below:
        m.pivot = ImVec2(cx, r.Min.y);
        m.translate = ImVec2(0.0f, -slide);
        break;
    case PopupSide::Above:
        m.pivot = ImVec2(cx, r.Max.y);
        m.translate = ImVec2(0.0f, slide);
        break;
    case PopupSide::After:
        m.pivot = ImVec2(r.Min.x, cy);
        m.translate = ImVec2(-slide, 0.0f);
        break;
    case PopupSide::Before:
        m.pivot = ImVec2(r.Max.x, cy);
        m.translate = ImVec2(slide, 0.0f);
        break;
    }
    return m;
}

float OpenAmount(const Level& level)
{
    const float duration = T().motion.popupOpen * MotionScale();
    if (ReduceMotion() || duration <= 0.0f)
        return 1.0f;
    return EaseValue(Ease::OutCubic, level.openTime / duration);
}

float CloseAmount(const Level& level)
{
    const float duration = T().motion.popupClose * MotionScale();
    if (ReduceMotion() || duration <= 0.0f)
        return 0.0f;
    return 1.0f - EaseValue(Ease::OutCubic, level.closeTime / duration);
}

// Applies `m` to the root draw list from `vertexBegin` / `commandBegin` on, keeping the scrim in
// place (it only fades), and to every visible child window of `root`.
void ApplyMotion(ImDrawList* drawList, const Level& level, const Motion& m, int vertexBegin, int commandBegin,
                 int scrimCommandEnd)
{
    const ImVec4 clip = ViewportClip();
    PopupTransformVertices(drawList, vertexBegin, level.scrimVertexBegin, m.alpha, m.scale, m.pivot, m.translate);
    PopupTransformVertices(drawList, level.scrimVertexBegin, level.scrimVertexEnd, m.alpha, 1.0f, m.pivot, ImVec2(0.0f, 0.0f));
    PopupTransformVertices(drawList, level.scrimVertexEnd, drawList->VtxBuffer.Size, m.alpha, m.scale, m.pivot, m.translate);
    PopupTransformClipRects(drawList, std::max(commandBegin, scrimCommandEnd), drawList->CmdBuffer.Size, m.scale, m.pivot,
                            m.translate, clip);
}

void ApplyMotionToChildren(ImGuiWindow* root, const Motion& m)
{
    const ImVec4 clip = ViewportClip();
    ImGuiWindow* stack[32];
    int depth = 0;
    for (ImGuiWindow* child : root->DC.ChildWindows)
    {
        if (depth < IM_COUNTOF(stack))
            stack[depth++] = child;
    }
    while (depth > 0)
    {
        ImGuiWindow* child = stack[--depth];
        if (!child->Active || child->Hidden || (child->Flags & ImGuiWindowFlags_Popup) != 0)
            continue;
        PopupTransformVertices(child->DrawList, 0, child->DrawList->VtxBuffer.Size, m.alpha, m.scale, m.pivot, m.translate);
        PopupTransformClipRects(child->DrawList, 0, child->DrawList->CmdBuffer.Size, m.scale, m.pivot, m.translate, clip);
        for (ImGuiWindow* grandChild : child->DC.ChildWindows)
        {
            if (depth < IM_COUNTOF(stack))
                stack[depth++] = grandChild;
        }
    }
}

// Draws closing `level` from its snapshot in a window of its own that takes no input.
void DrawClosingLevel(Level& level, int slot)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    char name[40];
    std::snprintf(name, sizeof(name), "##FacetPopupFade%02d", slot);
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    const bool visible = ImGui::Begin(name, nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->Appearing)
        ImGui::BringWindowToDisplayFront(window);
    if (visible)
    {
        ImDrawList* drawList = window->DrawList;
        const int vertexBegin = drawList->VtxBuffer.Size;
        const SurfaceCommands commands = DrawSurface(drawList, level, level.rect);
        level.snapshot.Replay(drawList, ViewportClip());
        const Motion m = MotionAt(level, CloseAmount(level));
        // Everything moves and fades except the scrim, which only fades.
        PopupTransformVertices(drawList, vertexBegin, level.scrimVertexBegin, m.alpha, m.scale, m.pivot, m.translate);
        PopupTransformVertices(drawList, level.scrimVertexBegin, level.scrimVertexEnd, m.alpha, 1.0f, m.pivot,
                               ImVec2(0.0f, 0.0f));
        PopupTransformVertices(drawList, level.scrimVertexEnd, drawList->VtxBuffer.Size, m.alpha, m.scale, m.pivot,
                               m.translate);
        PopupTransformClipRects(drawList, commands.scrimEnd, drawList->CmdBuffer.Size, m.scale, m.pivot, m.translate,
                                ViewportClip());
    }
    ImGui::End();
}

// ---- Tooltip timing -------------------------------------------------------------------------

bool IsModifierKey(ImGuiKey key)
{
    return (key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper) || key == ImGuiKey_ReservedForModCtrl ||
           key == ImGuiKey_ReservedForModShift || key == ImGuiKey_ReservedForModAlt || key == ImGuiKey_ReservedForModSuper;
}

void TooltipNewFrame()
{
    HostState& s = State();
    TooltipState& t = s.tooltip;
    const ImGuiIO& io = ImGui::GetIO();
    bool keyPressed = io.InputQueueCharacters.Size > 0;
    for (int key = ImGuiKey_Keyboard_BEGIN; key < ImGuiKey_Keyboard_END && !keyPressed; ++key)
    {
        const ImGuiKey k = static_cast<ImGuiKey>(key);
        if (!IsModifierKey(k) && ImGui::IsKeyPressed(k, ImGuiInputFlags_None, ImGuiKeyOwner_Any))
            keyPressed = true;
    }
    if (keyPressed)
        t.keySuppressed = true;
    else if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)
        t.keySuppressed = false;
    if (t.hoverItem != 0 && ImGui::IsAnyMouseDown())
        t.pressSuppressed = t.hoverItem;
    if (t.hoverSeenFrame + 1 < s.frame)
        t.hoverItem = 0;
    if (t.focusSeenFrame + 1 < s.frame)
        t.focusItem = 0;
    if (t.shownFrame + 1 < s.frame)
        t.shownItem = 0;
}

// Applies the tooltip rules of popup_host.h for one item; fills *anchor when it should show.
// `insideTip`: the pointer is over the item's interactive tooltip or on its way there, which counts
// as hovering the item, and a press there does not hide the tooltip.
bool TooltipWanted(ImGuiID itemId, const ImRect& itemRect, bool hovered, bool focusVisible, PopupAnchor* anchor,
                   bool insideTip = false)
{
    HostState& s = State();
    TooltipState& t = s.tooltip;
    const Theme& theme = T();
    const double now = ImGui::GetTime();
    const double delay = static_cast<double>(theme.overlay.tooltipDelay * MotionScale());
    const double grace = static_cast<double>(theme.overlay.tooltipGrace * MotionScale());
    bool want = false;
    bool atPointer = false;
    if (insideTip)
    {
        // Working in the tooltip keeps it: no press or key hides it there.
        if (t.pressSuppressed == itemId)
            t.pressSuppressed = 0;
        t.keySuppressed = false;
    }
    if (hovered || insideTip)
    {
        if (t.hoverItem != itemId)
        {
            t.hoverItem = itemId;
            t.hoverStart = now;
            t.hoverImmediate = now - t.lastVisible <= grace;
            if (t.pressSuppressed != itemId)
                t.pressSuppressed = 0;
        }
        t.hoverSeenFrame = s.frame;
        if (ImGui::IsAnyMouseDown() && !insideTip)
            t.pressSuppressed = itemId;
        if (t.pressSuppressed != itemId && !t.keySuppressed && (t.hoverImmediate || now - t.hoverStart >= delay))
        {
            want = true;
            // At the pointer only while it is really there: a forced hover state (captures, tests)
            // has the pointer elsewhere, so its tooltip goes below the item like a keyboard one.
            atPointer = insideTip || itemRect.Contains(ImGui::GetIO().MousePos);
        }
    }
    else if (t.hoverItem == itemId)
    {
        t.hoverItem = 0;
        if (t.pressSuppressed == itemId)
            t.pressSuppressed = 0;
    }
    if (focusVisible)
    {
        if (t.focusItem != itemId)
        {
            t.focusItem = itemId;
            t.focusStart = now;
            t.focusImmediate = now - t.lastVisible <= grace;
        }
        t.focusSeenFrame = s.frame;
        if (!want && (t.focusImmediate || now - t.focusStart >= delay))
            want = true;
    }
    else if (t.focusItem == itemId)
    {
        t.focusItem = 0;
    }
    // One tooltip at a time: the first item that wants one in a frame keeps it.
    if (want && t.shownFrame == s.frame && t.shownItem != itemId)
        want = false;
    if (!want)
        return false;
    t.shownItem = itemId;
    t.shownFrame = s.frame;
    t.lastVisible = now;
    if (atPointer)
    {
        if (t.anchorItem != itemId || !t.anchorAtPointer)
            t.anchorPoint = ImGui::GetIO().MousePos;
        // The arrow cursor covers about this much below and after the hot spot.
        const ImVec2 cursor = Px(ImVec2(theme.spacing.lg, theme.spacing.xl));
        anchor->rect = ImRect(t.anchorPoint, ImVec2(t.anchorPoint.x + cursor.x, t.anchorPoint.y + cursor.y));
        anchor->rect.Min.x = anchor->rect.Max.x;  // the tooltip starts after the cursor's width
    }
    else
    {
        anchor->rect = itemRect;
    }
    anchor->side = PopupSide::Below;
    anchor->align = PopupAlign::Start;
    anchor->matchWidth = false;
    t.anchorItem = itemId;
    t.anchorAtPointer = atPointer;
    return true;
}

// ---- Escape and claims ------------------------------------------------------------------------

Level* TopStackLevel(bool* hostPopupOnTop)
{
    const ImGuiContext& g = *GImGui;
    if (hostPopupOnTop != nullptr)
        *hostPopupOnTop = false;
    if (g.OpenPopupStack.Size == 0)
        return nullptr;
    Level* level = FindLevel(g.OpenPopupStack.back().PopupId);
    if (level == nullptr || level->phase != Phase::Open)
    {
        if (hostPopupOnTop != nullptr)
            *hostPopupOnTop = true;
        return nullptr;
    }
    return level;
}

void ApplyEscape()
{
    HostState& s = State();
    s.escapeWinner = 0;
    if (ImGui::GetKeyOwner(ImGuiKey_Escape) != kEscapeOwner ||
        !ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, kEscapeOwner))
        return;
    // The press is the host's: nothing else in this frame reads it.
    ImGui::SetKeyOwner(ImGuiKey_Escape, kEscapeOwner, ImGuiInputFlags_LockThisFrame);
    if (Level* panel = TopEscapePanel())
    {
        RestorePanelFocus(*panel);
        BeginClose(*panel, PopupCloseReason::Escape, true);
        s.focusCheckFrame = s.frame + 1;
        ImGui::SetNavCursorVisible(true);
        return;
    }
    bool hostPopupOnTop = false;
    if (Level* top = TopStackLevel(&hostPopupOnTop))
    {
        if (top->options.closeOnEscape)
        {
            CloseStackFrom(*top, PopupCloseReason::Escape, true, true);
            // The focus came back by a key: show it, so Enter and Space act on the item again as
            // after any keyboard navigation.
            ImGui::SetNavCursorVisible(true);
        }
        return;
    }
    if (hostPopupOnTop || GImGui->OpenPopupStack.Size > 0)
        return;
    // A shell: the front-most window among last frame's claims.
    const int previous = 1 - s.claimCurrent;
    int bestOrder = -1;
    for (int i = 0; i < s.claimCount[previous]; ++i)
    {
        const Claim& claim = s.claims[previous][i];
        const int order = claim.window != nullptr ? ImGui::FindWindowDisplayIndex(claim.window) : -1;
        if (s.escapeWinner == 0 || order > bestOrder)
        {
            s.escapeWinner = claim.owner;
            bestOrder = order;
        }
    }
}

void ClaimEscapeForNextFrame()
{
    HostState& s = State();
    ImGuiContext& g = *GImGui;
    bool hostPopupOnTop = false;
    const bool levelOnTop = TopStackLevel(&hostPopupOnTop) != nullptr;
    const bool shellsOnly = g.OpenPopupStack.Size == 0 && s.claimCount[s.claimCurrent] > 0;
    const bool panel = TopEscapePanel() != nullptr;
    if (!(levelOnTop || shellsOnly || panel) || g.ActiveId != 0)
        return;
    const ImGuiKeyOwnerData* owner = ImGui::GetKeyOwnerData(&g, ImGuiKey_Escape);
    if (owner->OwnerNext != ImGuiKeyOwner_NoOwner && owner->OwnerNext != kEscapeOwner)
        return;
    ImGui::SetKeyOwner(ImGuiKey_Escape, kEscapeOwner);
}

// ---- Context menu keys -----------------------------------------------------------------------

bool ContextKeyPressed()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_Menu, ImGuiInputFlags_None, ImGuiKeyOwner_NoOwner))
        return true;
    return io.KeyMods == ImGuiMod_Shift && ImGui::IsKeyPressed(ImGuiKey_F10, ImGuiInputFlags_None, ImGuiKeyOwner_NoOwner);
}

void ConsumeContextKeys()
{
    if (ImGui::IsKeyDown(ImGuiKey_Menu))
        ImGui::SetKeyOwner(ImGuiKey_Menu, kContextOwner, ImGuiInputFlags_LockUntilRelease);
    if (ImGui::IsKeyDown(ImGuiKey_F10))
        ImGui::SetKeyOwner(ImGuiKey_F10, kContextOwner, ImGuiInputFlags_LockUntilRelease);
}

bool IsMenuTarget(ImGuiID id)
{
    const HostState& s = State();
    for (int list = 0; list < 2; ++list)
    {
        for (int i = 0; i < s.menuTargetCount[list]; ++i)
        {
            if (s.menuTargets[list][i] == id)
                return true;
        }
    }
    return false;
}

void RecordMenuTarget(ImGuiID id)
{
    HostState& s = State();
    const int current = s.claimCurrent;
    for (int i = 0; i < s.menuTargetCount[current]; ++i)
    {
        if (s.menuTargets[current][i] == id)
            return;
    }
    if (s.menuTargetCount[current] < kMenuTargetCapacity)
        s.menuTargets[current][s.menuTargetCount[current]++] = id;
}

// ---- Lifetime ---------------------------------------------------------------------------------

void ReleaseKeysWeOwn()
{
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || g != State().context)
        return;
    const ImGuiID owners[] = {kEscapeOwner, kMouseOwner, kContextOwner};
    ReleaseKeyOwners(owners);
}

void EndAllMotion()
{
    for (Level& level : State().levels)
        EndMotion(level);
}

void ResetHost(bool releaseBuffers)
{
    HostState& s = State();
    for (Level& level : s.levels)
    {
        if (releaseBuffers)
            level.snapshot.Release();
        else
            level.snapshot.Clear();
        level.id = 0;
        level.phase = Phase::Idle;
        level.window = nullptr;
        level.forcedFrame = level.openedFrame = level.submittedFrame = level.closeFrame = 0;
    }
    s.beginDepth = 0;
    s.frame = 0;
    s.clickedLastFrame = false;
    s.leftClickedLastFrame = false;
    s.escapeWinner = 0;
    s.claimCount[0] = s.claimCount[1] = 0;
    s.menuTargetCount[0] = s.menuTargetCount[1] = 0;
    s.focusCheckFrame = 0;
    s.tooltip = TooltipState{};
}

bool PopupInit()
{
    ResetHost(false);
    State().context = ImGui::GetCurrentContext();
    return true;
}

void PopupShutdown()
{
    HostState& s = State();
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g != nullptr && g == s.context)
    {
        // Leave no level of ours in Dear ImGui's popup stack and no key with our owner id.
        int lowest = -1;
        for (int i = 0; i < g->OpenPopupStack.Size && lowest < 0; ++i)
        {
            const Level* level = FindLevel(g->OpenPopupStack[i].PopupId);
            if (level != nullptr && level->phase == Phase::Open)
                lowest = i;
        }
        if (lowest >= 0)
            ImGui::ClosePopupToLevel(lowest, false);
        ReleaseKeysWeOwn();
    }
    ResetHost(true);
    s.context = nullptr;
}

void PopupNewFrame()
{
    HostState& s = State();
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr)
        return;
    if (g != s.context)
    {
        // A new Dear ImGui context: nothing recorded belongs to it.
        ResetHost(false);
        s.context = g;
    }
    s.frame = FrameIndex();
    s.beginDepth = 0;
    s.inEndFrame = false;
    // The button held after a dismissing click is let go of in the frame after its release.
    ImGuiKeyOwnerData* mouse = ImGui::GetKeyOwnerData(g, ImGuiKey_MouseLeft);
    if (mouse->OwnerCurr == kMouseOwner && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !mouse->LockUntilRelease)
        mouse->OwnerCurr = ImGuiKeyOwner_NoOwner;
    s.claimCurrent = 1 - s.claimCurrent;
    s.claimCount[s.claimCurrent] = 0;
    s.menuTargetCount[s.claimCurrent] = 0;

    SyncWithImGuiStack(s.clickedLastFrame, s.leftClickedLastFrame);
    CloseLevelsAboveClickedPopup();
    CloseUnfocusedLevelsOnClickOutside();
    ClosePanelsOnClickOutside();
    ApplyEscape();

    // A click outside the topmost Modal lands on its scrim.
    bool hostPopupOnTop = false;
    if (Level* top = TopStackLevel(&hostPopupOnTop))
    {
        if (top->options.kind == PopupKind::Modal)
        {
            const ImGuiIO& io = g->IO;
            for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
            {
                if (ImGui::IsMouseClicked(button, ImGuiInputFlags_None, ImGuiKeyOwner_Any) &&
                    !top->rect.Contains(io.MouseClickedPos[button]))
                {
                    top->scrimClickFrame = s.frame;
                    // The scrim takes the press: also when the widget closes the dialog on it, the
                    // press reaches nothing under it and does not clear the focus given back.
                    if (button == ImGuiMouseButton_Left)
                        HoldDismissingClick();
                }
            }
        }
    }
    TooltipNewFrame();
}

void PopupEndFrame()
{
    HostState& s = State();
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || g != s.context || s.frame != FrameIndex())
        return;
    if (s.beginDepth != 0)
    {
        ReportUsageError("BeginPopupLevel without EndPopupLevel (%d open)", s.beginDepth);
        s.beginDepth = 0;
    }
    s.inEndFrame = true;

    // Levels whose widget did not submit them this frame are gone with their anchor.
    for (Level& level : s.levels)
    {
        if (level.phase != Phase::Open || level.submittedFrame == s.frame || level.openedFrame == s.frame)
            continue;
        if (IsStackLevel(level))
            CloseStackFrom(level, PopupCloseReason::Closed, false, false);
        else
            BeginClose(level, PopupCloseReason::Closed, false);
    }
    const bool clicked = AnyMouseClicked();
    const bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left, ImGuiInputFlags_None, ImGuiKeyOwner_Any);
    SyncWithImGuiStack(clicked, leftClicked);

    // Copy and move every level shown this frame.
    const float dt = DeltaTime();
    for (Level& level : s.levels)
    {
        if (level.submittedFrame != s.frame || level.window == nullptr || level.forcedFrame == s.frame)
            continue;
        ImGuiWindow* window = level.window;
        if (window->Hidden || !window->Active)
            continue;
        level.snapshot.Capture(window, level.surfaceCommands);
        level.openTime += dt;
        const float amount = OpenAmount(level);
        if (amount < 1.0f)
        {
            const Motion m = MotionAt(level, amount);
            ApplyMotion(window->DrawList, level, m, 0, 0, level.scrimCommands);
            ApplyMotionToChildren(window, m);
        }
    }

    // Levels playing their close motion.
    for (int slot = 0; slot < kLevelCapacity; ++slot)
    {
        Level& level = s.levels[slot];
        if (level.phase != Phase::Closing || s.frame < level.fadeStartFrame)
            continue;
        level.closeTime += dt;
        if (CloseAmount(level) <= 0.0f || !level.snapshot.Valid())
        {
            EndMotion(level);
            continue;
        }
        DrawClosingLevel(level, slot);
    }

    ClaimEscapeForNextFrame();

    // A focus given back after a close must rest on an item that still exists.
    if (s.focusCheckFrame == s.frame && g->NavWindow != nullptr && g->NavId != 0 && !g->NavIdIsAlive &&
        g->NavWindow->Active)
        ImGui::SetNavID(0, g->NavLayer, g->NavFocusScopeId, ImRect());

    s.clickedLastFrame = clicked;
    s.leftClickedLastFrame = leftClicked;
    s.inEndFrame = false;
    for (Level& level : s.levels)
    {
        if (level.id != 0 && (level.phase != Phase::Idle || level.forcedFrame == s.frame))
            level.lastUseFrame = s.frame;
    }
}

void PopupInvalidateDeviceObjects()
{
    EndAllMotion();
    for (Level& level : State().levels)
        level.snapshot.Clear();
}

void PopupScaleChanged(float)
{
    EndAllMotion();
}

void PopupThemeChanged()
{
    EndAllMotion();
}

} // namespace

// ---- Levels -------------------------------------------------------------------------------------

PopupAnchor PanelAnchor(const ImRect& rect)
{
    PopupAnchor anchor;
    anchor.rect = rect;
    anchor.side = PopupSide::After;
    anchor.align = PopupAlign::Center;
    anchor.fallback = true;
    anchor.fallbackSide = PopupSide::Below;
    anchor.fallbackAlign = PopupAlign::Start;
    return anchor;
}

void OpenPopupLevel(ImGuiID id, const PopupAnchor& anchor, const PopupOptions& options)
{
    if (!InFrame() || id == 0)
        return;
    HostState& s = State();
    Level* level = FindOrAddLevel(id);
    if (level == nullptr)
        return;
    level->lastUseFrame = s.frame;
    if (IsPlainKind(options.kind))
    {
        if (level->phase != Phase::Open)
        {
            level->snapshot.Clear();
            level->phase = Phase::Open;
            level->openedFrame = s.frame;
            level->openTime = 0.0f;
            level->hasSide = false;
            level->closeReason = PopupCloseReason::None;
            const ImGuiWindow* nav = GImGui->NavWindow;
            level->restoreWindow = nav != nullptr ? nav->RootWindow->ID : 0;
            const ImGuiWindow* current = GImGui->CurrentWindow;
            level->ownerWindow = current != nullptr ? current->RootWindow->ID : 0;
            level->stackDepthAtOpen = GImGui->OpenPopupStack.Size;
            level->parent = CurrentPopupLevel();
        }
        level->anchor = anchor;
        level->options = options;
        level->anchorRegion = AnchorRegion(options);
        return;
    }
    if (level->phase == Phase::Open && ImGuiPopupIndex(id) >= 0)
    {
        level->anchor = anchor;
        level->anchorRegion = AnchorRegion(level->options);
        return;
    }
    level->snapshot.Clear();
    level->anchor = anchor;
    level->anchorRegion = AnchorRegion(options);
    level->options = options;
    level->parent = CurrentPopupLevel();
    level->phase = Phase::Open;
    level->openedFrame = s.frame;
    level->openTime = 0.0f;
    level->hasSide = false;
    level->closeReason = PopupCloseReason::None;
    ImGui::OpenPopupEx(id, ImGuiPopupFlags_None);
}

void SetPopupLevelAnchor(ImGuiID id, const PopupAnchor& anchor)
{
    if (!InFrame())
        return;
    Level* level = FindLevel(id);
    if (level != nullptr && level->phase == Phase::Open)
    {
        level->anchor = anchor;
        level->anchorRegion = AnchorRegion(level->options);
    }
}

void SetPopupLevelLayout(ImGuiID id, ImVec2 size, const ImRect& placeRect)
{
    if (!InFrame())
        return;
    Level* level = FindLevel(id);
    if (level == nullptr || (level->phase != Phase::Open && level->forcedFrame != State().frame))
        return;
    level->options.size = size;
    level->options.placeRect = placeRect;
}

void ClosePopupLevel(ImGuiID id)
{
    if (!InFrame())
        return;
    Level* level = FindLevel(id);
    if (level == nullptr || level->phase != Phase::Open)
        return;
    if (IsStackLevel(*level))
    {
        CloseStackFrom(*level, PopupCloseReason::Closed, true, true);
    }
    else
    {
        RestorePanelFocus(*level);
        BeginClose(*level, PopupCloseReason::Closed, true);
    }
}

void ClosePopupLevelChain(ImGuiID id)
{
    if (!InFrame())
        return;
    Level* level = FindLevel(id);
    if (level == nullptr || level->phase != Phase::Open || !IsStackLevel(*level))
    {
        ClosePopupLevel(id);
        return;
    }
    // Walk down Dear ImGui's stack while the levels below are Popup levels of ours.
    const ImGuiContext& g = *GImGui;
    int index = ImGuiPopupIndex(id);
    while (index > 0)
    {
        const Level* below = FindLevel(g.OpenPopupStack[index - 1].PopupId);
        if (below == nullptr || below->phase != Phase::Open || below->options.kind != PopupKind::Popup)
            break;
        --index;
    }
    if (level->options.kind == PopupKind::Modal)
        index = ImGuiPopupIndex(id);
    Level* root = index >= 0 ? FindLevel(g.OpenPopupStack[index].PopupId) : level;
    CloseStackFrom(root != nullptr ? *root : *level, PopupCloseReason::Closed, true, true);
}

bool IsPopupLevelOpen(ImGuiID id)
{
    if (!InFrame())
        return false;
    const Level* level = FindLevel(id);
    if (level == nullptr)
        return false;
    if (level->forcedFrame == State().frame)
        return true;
    if (level->phase != Phase::Open)
        return false;
    return !IsStackLevel(*level) || ImGuiPopupIndex(id) >= 0;
}

bool IsPopupLevelClosing(ImGuiID id)
{
    const Level* level = FindLevel(id);
    return level != nullptr && level->phase == Phase::Closing;
}

bool BeginPopupLevel(ImGuiID id)
{
    if (!InFrame())
        return false;
    HostState& s = State();
    Level* level = FindLevel(id);
    if (level == nullptr || s.beginDepth >= kBeginCapacity)
        return false;
    const bool forced = level->forcedFrame == s.frame;
    if (!forced)
    {
        if (level->phase != Phase::Open)
            return false;
        if (IsStackLevel(*level) && !ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
            return false;
    }
    const PopupKind kind = level->options.kind;
    const bool plain = forced || IsPlainKind(kind);

    // A plain level (a tooltip or a forced level) draws in the window of its record's slot, not
    // in a window per id: Dear ImGui never destroys windows, so a window per hovered item would
    // grow the window list without limit. When the slot's window last showed another level,
    // it is reset the way Dear ImGui recycles its own tooltip windows: hidden for one frame
    // while it measures the new content, so nothing shows at the old size.
    char name[40];
    std::snprintf(name, sizeof(name), "##FacetPopupPlain%02d", static_cast<int>(level - s.levels));
    ImGuiWindow* existing = plain ? ImGui::FindWindowByName(name) : nullptr;
    if (existing != nullptr && level->windowOwner != id)
    {
        existing->HiddenFramesCannotSkipItems = 1;
        existing->Size = existing->SizeFull = ImVec2(0.0f, 0.0f);
        existing->ContentSize = existing->ContentSizeIdeal = ImVec2(0.0f, 0.0f);
        existing = nullptr;
    }
    if (plain)
        level->windowOwner = id;
    if (!plain)
    {
        std::snprintf(name, sizeof(name), "##Popup_%08x", id);
        existing = ImGui::FindWindowByName(name);
    }
    const ImVec2 padding = LevelPadding(level->options);
    ImVec2 wanted(0.0f, 0.0f);
    if (existing != nullptr)
    {
        // The size Dear ImGui would give the window this frame, so placement and size agree.
        const ImVec2 savedPadding = existing->WindowPadding;
        existing->WindowPadding = padding;
        wanted = ImGui::CalcWindowNextAutoFitSize(existing);
        existing->WindowPadding = savedPadding;
    }
    if (level->options.size.x > 0.0f)
        wanted.x = level->options.size.x;
    if (level->options.size.y > 0.0f)
        wanted.y = level->options.size.y;
    PlaceLevel(*level, wanted);

    ImGui::SetNextWindowPos(level->rect.Min);
    const bool fixedRect = level->options.placeRect.GetWidth() > 0.0f && level->options.placeRect.GetHeight() > 0.0f;
    if (existing != nullptr || fixedRect)
        ImGui::SetNextWindowSize(level->rect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;
    bool shown = false;
    if (plain)
    {
        if (kind == PopupKind::Panel && !forced)
        {
            // A panel is a window the user works in: it takes input and the keyboard.
            if (!level->options.takeFocus)
                flags |= ImGuiWindowFlags_NoFocusOnAppearing;
        }
        else if (kind == PopupKind::Tooltip && level->options.interactive && !forced)
        {
            // An interactive tooltip takes the mouse but never the keyboard.
            flags |= ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
        }
        else
        {
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
            if (kind == PopupKind::Tooltip)
                flags |= ImGuiWindowFlags_Tooltip;
        }
        std::snprintf(name, sizeof(name), "##FacetPopupPlain%02d", static_cast<int>(level - s.levels));
        shown = ImGui::Begin(name, nullptr, flags);
        if (!shown)
            ImGui::End();
    }
    else
    {
        if (kind == PopupKind::Modal)
            flags |= ImGuiWindowFlags_Modal;
        if (kind == PopupKind::Popup && !level->options.takeFocus)
            flags |= ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
        shown = ImGui::BeginPopupEx(id, flags);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!shown)
        return false;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (plain && window->Appearing && (kind != PopupKind::Tooltip || level->options.interactive))
        ImGui::BringWindowToDisplayFront(window);
    if (kind == PopupKind::Panel && !forced && !window->Appearing)
    {
        // A click on the page brings the page window to the front; the panel goes back in front of
        // it, and the open popups go back in front of the panel.
        ImGuiWindow* owner = level->ownerWindow != 0 ? ImGui::FindWindowByID(level->ownerWindow) : nullptr;
        if (owner != nullptr && owner != window && ImGui::IsWindowAbove(owner, window))
        {
            ImGui::BringWindowToDisplayFront(window);
            for (const ImGuiPopupData& data : GImGui->OpenPopupStack)
            {
                if (data.Window != nullptr && data.Window->WasActive)
                    ImGui::BringWindowToDisplayFront(data.Window);
            }
        }
    }
    level->window = window;
    level->plainWindow = plain;
    level->submittedFrame = s.frame;
    level->lastUseFrame = s.frame;

    // The surface, behind what Dear ImGui drew in Begin.
    ImDrawList* drawList = window->DrawList;
    const SurfaceCommands commands = DrawSurface(drawList, *level, level->rect);
    MoveCommandsToFront(drawList, commands.first, commands.end);
    level->surfaceCommands = commands.end - commands.first;
    level->scrimCommands = commands.scrimEnd - commands.first;

    BeginEntry& entry = s.begin[s.beginDepth++];
    entry = BeginEntry{};
    entry.level = level;
    entry.imguiPopup = !plain;
    // While a child level is open, the rows of a Popup level stay hoverable, so the pointer can
    // move back to them (menus) and a click on one is meant for it.
    if (!plain && kind == PopupKind::Popup)
    {
        const int index = ImGuiPopupIndex(id);
        if (index >= 0 && index < GImGui->OpenPopupStack.Size - 1)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);
            entry.pushedHoverFlag = true;
        }
    }
    if (kind == PopupKind::Tooltip)
    {
        const float wrap = level->wrapWidth > 0.0f ? level->wrapWidth : Px(T().overlay.tooltipMaxWidth);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
        entry.pushedWrap = true;
    }
    return true;
}

void EndPopupLevel()
{
    HostState& s = State();
    if (s.beginDepth <= 0)
    {
        ReportUsageError("EndPopupLevel without BeginPopupLevel");
        return;
    }
    const BeginEntry entry = s.begin[--s.beginDepth];
    if (entry.pushedWrap)
        ImGui::PopTextWrapPos();
    if (entry.pushedHoverFlag)
        ImGui::PopItemFlag();
    if (entry.imguiPopup)
        ImGui::EndPopup();
    else
        ImGui::End();
}

PopupCloseReason PopupLevelClosedReason(ImGuiID id)
{
    if (!InFrame())
        return PopupCloseReason::None;
    const Level* level = FindLevel(id);
    if (level == nullptr || level->closeFrame != State().frame)
        return PopupCloseReason::None;
    return level->closeReason;
}

ImGuiID TopPopupLevel()
{
    if (!InFrame())
        return 0;
    const ImGuiContext& g = *GImGui;
    for (int i = g.OpenPopupStack.Size - 1; i >= 0; --i)
    {
        const Level* level = FindLevel(g.OpenPopupStack[i].PopupId);
        if (level != nullptr && level->phase == Phase::Open)
            return level->id;
    }
    return 0;
}

ImGuiID CurrentPopupLevel()
{
    const HostState& s = State();
    return s.beginDepth > 0 ? s.begin[s.beginDepth - 1].level->id : 0;
}

ImRect PopupLevelRect(ImGuiID id)
{
    const Level* level = FindLevel(id);
    if (level == nullptr || (level->submittedFrame + 1 < State().frame && level->phase != Phase::Closing))
        return ImRect();
    return level->rect;
}

bool PopupLevelSurface(const ImGuiWindow* window, ImVec4* surface)
{
    if (window == nullptr || !InFrame())
        return false;
    const HostState& s = State();
    for (const Level& level : s.levels)
    {
        // A plain window is shared by the levels of one slot, so only the level that submitted it
        // in this frame owns it.
        if (level.id == 0 || level.window != window || level.submittedFrame != s.frame)
            continue;
        if (surface != nullptr)
            *surface = level.options.glass ? T().colors.bgElevated
                                           : T().colors.ElevatedSurface(LevelElevation(level.options));
        return true;
    }
    return false;
}

PopupSide PopupLevelSide(ImGuiID id)
{
    const Level* level = FindLevel(id);
    if (level == nullptr)
        return PopupSide::Below;
    return level->hasSide ? level->side : level->anchor.side;
}

bool PopupLevelScrimClicked(ImGuiID id)
{
    if (!InFrame())
        return false;
    const Level* level = FindLevel(id);
    return level != nullptr && level->scrimClickFrame == State().frame && level->phase == Phase::Open;
}

void ForcePopupLevelOpen(ImGuiID id, const PopupAnchor& anchor, const PopupOptions& options)
{
    if (!InFrame() || id == 0)
        return;
    Level* level = FindOrAddLevel(id);
    if (level == nullptr)
        return;
    level->forcedFrame = State().frame;
    level->lastUseFrame = State().frame;
    level->anchor = anchor;
    level->anchorRegion = AnchorRegion(options);
    level->options = options;
    level->openTime = 1.0e6f;  // forced levels show settled
}

bool PopupPointerAimsAt(const ImRect& target)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsMousePosValid(&io.MousePos) || (io.MouseDelta.x == 0.0f && io.MouseDelta.y == 0.0f))
        return false;
    const ImVec2 now = io.MousePos;
    const ImVec2 before(now.x - io.MouseDelta.x, now.y - io.MouseDelta.y);
    ImVec2 a;
    ImVec2 b;
    if (target.Min.x >= before.x)
    {
        a = ImVec2(target.Min.x, target.Min.y);
        b = ImVec2(target.Min.x, target.Max.y);
    }
    else if (target.Max.x <= before.x)
    {
        a = ImVec2(target.Max.x, target.Min.y);
        b = ImVec2(target.Max.x, target.Max.y);
    }
    else if (target.Min.y >= before.y)
    {
        a = ImVec2(target.Min.x, target.Min.y);
        b = ImVec2(target.Max.x, target.Min.y);
    }
    else
    {
        a = ImVec2(target.Min.x, target.Max.y);
        b = ImVec2(target.Max.x, target.Max.y);
    }
    return target.Contains(now) || ImTriangleContainsPoint(before, a, b, now);
}

// ---- Escape and context keys -------------------------------------------------------------------

bool ClaimEscape(ImGuiID owner, EscapeLayer layer)
{
    (void)layer;
    if (!InFrame() || owner == 0)
        return false;
    HostState& s = State();
    const int current = s.claimCurrent;
    if (s.claimCount[current] < kClaimCapacity)
    {
        ImGuiWindow* window = GImGui->CurrentWindow;
        s.claims[current][s.claimCount[current]++] = Claim{owner, window != nullptr ? window->RootWindow : nullptr};
    }
    return s.escapeWinner == owner;
}

bool ContextMenuRequested(ContextRequestTarget target, ImVec2* at)
{
    if (at != nullptr)
        *at = ImVec2(0.0f, 0.0f);
    if (!InFrame())
        return false;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window == nullptr)
        return false;
    const ImGuiIO& io = g.IO;
    if (target == ContextRequestTarget::LastItem)
    {
        const ImGuiID id = g.LastItemData.ID;
        if (id == 0)
            return false;
        RecordMenuTarget(id);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        {
            if (at != nullptr)
                *at = io.MousePos;
            return true;
        }
        if (g.NavId == id && g.NavWindow == window && ContextKeyPressed())
        {
            ConsumeContextKeys();
            if (at != nullptr)
                *at = ImVec2(g.LastItemData.Rect.Min.x, g.LastItemData.Rect.Max.y);
            return true;
        }
        return false;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_ChildWindows) &&
        !ImGui::IsAnyItemHovered())
    {
        if (at != nullptr)
            *at = io.MousePos;
        return true;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && (g.NavId == 0 || !IsMenuTarget(g.NavId)) &&
        ContextKeyPressed())
    {
        ConsumeContextKeys();
        if (at != nullptr)
        {
            if (g.NavId != 0 && g.NavWindow != nullptr)
            {
                const ImRect r = ImGui::WindowRectRelToAbs(g.NavWindow, g.NavWindow->NavRectRel[g.NavLayer]);
                *at = ImVec2(r.Min.x, r.Max.y);
            }
            else
            {
                *at = window->DC.CursorStartPos;
            }
        }
        return true;
    }
    return false;
}

// ---- Tooltips -----------------------------------------------------------------------------------

ImGuiID PopupTooltipLevel(ImGuiID itemId)
{
    return ImHashStr("##facet.tooltip", 0, itemId);
}

bool BeginPopupTooltip(ImGuiID itemId, const ImRect& itemRect, bool hovered, bool focusVisible, float maxWidth)
{
    PopupTooltipDesc desc;
    desc.maxWidth = maxWidth;
    return BeginPopupTooltip(itemId, itemRect, hovered, focusVisible, desc);
}

bool BeginPopupTooltip(ImGuiID itemId, const ImRect& itemRect, bool hovered, bool focusVisible,
                       const PopupTooltipDesc& desc)
{
    if (!InFrame() || itemId == 0)
        return false;
    const ImGuiID tipId = PopupTooltipLevel(itemId);
    // An interactive tooltip shown in the last frame stays while the pointer is over it, or moves
    // from the item toward it.
    bool insideTip = false;
    if (desc.interactive)
    {
        const Level* tip = FindLevel(tipId);
        if (tip != nullptr && tip->phase == Phase::Open && tip->submittedFrame + 1 >= State().frame)
        {
            // The way there: the box from where the pointer was when the tooltip showed to the
            // tooltip itself (it opens below and after the pointer).
            const TooltipState& t = State().tooltip;
            ImRect way = tip->rect;
            if (t.anchorItem == itemId && t.anchorAtPointer)
                way.Add(t.anchorPoint);
            const ImVec2 pointer = ImGui::GetIO().MousePos;
            insideTip = way.Contains(pointer) || (!hovered && PopupPointerAimsAt(tip->rect));
        }
    }
    // An item scrolled or clipped out of its window's view shows no tooltip: it would float over
    // unrelated content with nothing to point at.
    const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    const bool inView = window == nullptr || itemRect.Overlaps(window->ClipRect);
    PopupAnchor anchor;
    if (!inView || !TooltipWanted(itemId, itemRect, hovered, focusVisible, &anchor, insideTip))
    {
        if (IsPopupLevelOpen(tipId))
            ClosePopupLevel(tipId);
        return false;
    }
    PopupOptions options;
    options.kind = PopupKind::Tooltip;
    options.takeFocus = false;
    options.interactive = desc.interactive;
    if (!IsPopupLevelOpen(tipId))
        OpenPopupLevel(tipId, anchor, options);
    else
        SetPopupLevelAnchor(tipId, anchor);
    if (Level* level = FindLevel(tipId))
    {
        level->wrapWidth = desc.maxWidth;
        level->options.interactive = desc.interactive;
    }
    return BeginPopupLevel(tipId);
}

void EndPopupTooltip()
{
    EndPopupLevel();
}

PopupHostStats GetPopupHostStats()
{
    PopupHostStats stats;
    const HostState& s = State();
    for (const Level& level : s.levels)
    {
        if (level.id != 0 && (level.phase != Phase::Idle || level.snapshot.VertexCapacity() > 0))
            ++stats.trackedLevels;
        if (level.phase == Phase::Closing)
            ++stats.closingLevels;
        stats.replayVertices += level.snapshot.VertexCapacity();
        stats.replayViews += level.snapshot.TextureReferences();
    }
    if (ImGuiContext* g = ImGui::GetCurrentContext())
        stats.ownsEscape = ImGui::GetKeyOwnerData(g, ImGuiKey_Escape)->OwnerCurr == kEscapeOwner;
    return stats;
}

Subsystem PopupHostSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "popup_host";
    subsystem.init = &PopupInit;
    subsystem.shutdown = &PopupShutdown;
    subsystem.newFrame = &PopupNewFrame;
    subsystem.endFrame = &PopupEndFrame;
    subsystem.scaleChanged = &PopupScaleChanged;
    subsystem.themeChanged = &PopupThemeChanged;
    subsystem.invalidateDeviceObjects = &PopupInvalidateDeviceObjects;
    return subsystem;
}

} // namespace facet::internal
