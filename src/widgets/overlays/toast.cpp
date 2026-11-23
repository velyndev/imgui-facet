// ShowToast and the toast queries (overlays.h): queued notifications that facet::EndFrame draws in
// a corner of the display.
//
// The queue: a fixed array of kToastQueueCapacity entries that copies every text, so nothing is
// allocated and the caller's strings need to live only during the call. When it is full, the oldest
// toast is dropped at once. overlay.toastMaxCount toasts show at a time, in the order they were
// queued; a toast that is still leaving counts, so the next one waits until it is gone.
//
// The stack: newest nearest the corner, overlay.toastStackGap apart, overlay.toastMargin from the
// corner of the display's work area, each as wide as its content and at most overlay.toastWidth
// (less on a narrow display), with its edge on the corner's side of the stack. Each
// toast is a window of its own (one per queue slot), drawn after every page and kept in front of the
// page windows but behind open popups. A toast that leaves keeps its place while it fades, then the
// others move into the room on a spring, so the stack never jumps. A watermark strip in the half of
// the display the stack grows from that crosses the stack's column (OverlayToastAvoidRect) moves
// the stack past it, so on a narrow display the strip does not cover a toast.
//
// A toast: its icon (the tone's icon unless one is given; Neutral has none), the text in Body, the
// description in Footnote, an optional progress bar (facet::ProgressBar), an optional action button
// and a close button (facet::Button and IconButton, Small). The timer (overlay.toastDuration, or the
// toast's own duration; < 0 until dismissed; times motion.scale) runs only while the toast shows and
// pauses while the pointer is over it, while it has keyboard focus and while it is dragged. A
// horizontal drag toward the near display edge past half its width, or a flick faster than
// drag.flickSpeed that way, dismisses it; a shorter drag springs back.
//
// Keyboard: toast windows take part in Dear ImGui's window switching (Ctrl+Tab with
// NavEnableKeyboard), which moves the focus to the newest toast's first button; Tab moves between its
// buttons, and Escape dismisses it. When a focused toast goes, Dear ImGui gives the focus back to the
// window under it.
//
// Motion: a toast fades and slides in from the edge over motion.popupOpen and fades out over
// motion.popupClose; under reduced motion it appears, leaves and moves at once.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/display.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_replay.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

constexpr int kToastQueueCapacity = 16;
constexpr int kToastTextSize = 256;
constexpr int kToastDescriptionSize = 512;
constexpr int kToastActionSize = 64;
constexpr int kToastSpringSub = 1;
// Corner rects (watermark strips) the stack keeps clear of in one frame; later ones are ignored.
constexpr int kToastAvoidRects = 8;
// The share of its width a toast travels while it slides in from the edge.
constexpr float kToastSlideShare = 0.25f;
// The share of its width a drag must reach toward the edge to dismiss the toast.
constexpr float kToastSwipeShare = 0.5f;
// A drag away from the edge moves the toast this much of the pointer's way: it resists.
constexpr float kToastSwipeResist = 0.25f;

enum class ToastPhase
{
    Free,
    Waiting,  // queued, not shown yet
    Shown,
    Leaving,  // fading out; still holds its place
};

struct ToastEntry
{
    std::uint32_t id = 0;
    std::uint64_t sequence = 0;  // queue order
    ToastPhase phase = ToastPhase::Free;
    char text[kToastTextSize] = {};
    char description[kToastDescriptionSize] = {};
    char action[kToastActionSize] = {};
    Tone tone = Tone::Neutral;
    Icon icon;
    float duration = 0.0f;
    bool progressBar = false;
    float progress = kIndeterminate;

    float remaining = 0.0f;  // seconds of the timer left
    float appear = 0.0f;     // 0 to 1, linear in time
    float leave = 0.0f;      // 0 to 1, linear in time
    bool placed = false;     // the stack position spring was started
    float height = 0.0f;     // pixels, measured this frame
    float swipe = 0.0f;      // pixels toward (+) or away from (-) the near edge
    float swipeVelocity = 0.0f;  // pixels per second toward the edge while dragged
    float pressX = 0.0f;     // pointer x where the drag started, minus the swipe then
    bool dragging = false;
};

struct ToastSystem
{
    ToastEntry entries[kToastQueueCapacity];
    std::uint32_t nextId = 1;
    std::uint64_t sequence = 0;
    ScreenCorner corner = ScreenCorner::BottomRight;
    ScreenCorner pendingCorner = ScreenCorner::BottomRight;
    bool inFrame = false;               // between the family's NewFrame and EndFrame hooks
    std::uint32_t actionToast = 0;      // the toast whose action button was activated
    std::uint64_t actionFrame = 0;      // the facet frame on which ToastActionPressed reports it
    // Rects drawn in a corner this frame (watermark strips) that the stack keeps clear of.
    ImRect avoid[kToastAvoidRects];
    int avoidCount = 0;
};

ToastSystem& System()
{
    static ToastSystem system;
    return system;
}

ToastEntry* FindToast(ToastId toast)
{
    if (!toast.IsValid())
        return nullptr;
    for (ToastEntry& entry : System().entries)
    {
        if (entry.phase != ToastPhase::Free && entry.id == toast.value)
            return &entry;
    }
    return nullptr;
}

bool CanQueue()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return IsInitialized() && g != nullptr && g->WithinFrameScope && System().inFrame;
}

void CopyText(char* out, std::size_t size, const char* text)
{
    ImStrncpy(out, text != nullptr ? text : "", size);
}

void FillToast(ToastEntry& entry, const char* text, const ToastOptions& options)
{
    CopyText(entry.text, sizeof(entry.text), text);
    CopyText(entry.description, sizeof(entry.description), options.description);
    CopyText(entry.action, sizeof(entry.action), options.actionLabel);
    entry.tone = options.tone;
    entry.icon = options.icon;
    entry.duration = options.duration;
    entry.progressBar = options.progressBar;
    entry.progress = options.progress;
    const float scale = T().motion.scale > 0.0f ? T().motion.scale : 1.0f;
    const float seconds = options.duration == 0.0f ? T().overlay.toastDuration : options.duration;
    entry.remaining = seconds * scale;
}

Icon ToneIcon(Tone tone)
{
    switch (tone)
    {
    case Tone::Success:
        return Icon("success");
    case Tone::Warning:
        return Icon("warning");
    case Tone::Destructive:
        return Icon("error");
    case Tone::Accent:
        return Icon("info");
    case Tone::Neutral:
        break;
    }
    return Icon();
}

ImVec4 ToneColor(Tone tone)
{
    const Colors& c = T().colors;
    switch (tone)
    {
    case Tone::Success:
        return c.success;
    case Tone::Warning:
        return c.warning;
    case Tone::Destructive:
        return c.destructive;
    case Tone::Accent:
        return c.accentText;
    case Tone::Neutral:
        break;
    }
    return c.labelSecondary;
}

bool CornerIsRight(ScreenCorner corner)
{
    return corner == ScreenCorner::TopRight || corner == ScreenCorner::BottomRight;
}

bool CornerIsBottom(ScreenCorner corner)
{
    return corner == ScreenCorner::BottomLeft || corner == ScreenCorner::BottomRight;
}

// Where the parts of a toast go, measured in the toast's own text styles.
struct ToastLayout
{
    float width = 0.0f;        // of the toast
    float padding = 0.0f;
    float iconSize = 0.0f;
    float textX = 0.0f;        // from the toast's left edge
    float textWidth = 0.0f;
    float textHeight = 0.0f;
    float descriptionHeight = 0.0f;
    float progressTop = 0.0f;  // from the toast's top; 0 without a bar
    float actionWidth = 0.0f;
    bool actionBelow = false;  // the action button gets a row of its own under the text
    float actionTop = 0.0f;
    float height = 0.0f;
};

float SmallButtonWidth(const char* label)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Small);
    internal::ControlTextScope text(m);
    return std::ceil(m.paddingX * 2.0f + ImGui::CalcTextSize(label, ImGui::FindRenderedTextEnd(label)).x);
}

// A toast is as wide as its content on one line, so a short message is a short toast. Content that
// needs more than `maxWidth` gets that width and its text wraps. A progress bar has no width of its
// own: a toast with one takes `maxWidth`.
ToastLayout MeasureToast(const ToastEntry& entry, float maxWidth)
{
    const Theme& theme = T();
    ToastLayout layout;
    layout.padding = Px(theme.spacing.md);
    const float gap = Px(theme.spacing.sm);
    const float small = ControlHeight(ControlSize::Small);
    const Icon icon = entry.icon.IsValid() ? entry.icon : ToneIcon(entry.tone);
    {
        internal::ControlTextScope body(TextStyle::Body);
        layout.iconSize = icon.IsValid() ? body.IconSize() : 0.0f;
    }
    layout.textX = layout.padding + (layout.iconSize > 0.0f ? layout.iconSize + gap : 0.0f);
    const float closeRoom = small + gap;
    const bool hasAction = entry.action[0] != '\0';
    if (hasAction)
        layout.actionWidth = SmallButtonWidth(entry.action);
    // The widest line of the texts when nothing wraps, and what stands beside it on the first line.
    float line = MeasureText(TextStyle::Body, entry.text).x;
    if (entry.description[0] != '\0')
        line = std::max(line, MeasureText(TextStyle::Footnote, entry.description).x);
    const float beside = layout.textX + closeRoom + layout.padding + (hasAction ? layout.actionWidth + gap : 0.0f);
    // One pixel more than measured, so rounding never wraps the last word.
    const float natural = std::ceil(beside + line) + 1.0f;
    const bool oneLine = !entry.progressBar && natural <= maxWidth;
    const float width = oneLine ? natural : maxWidth;
    layout.width = width;
    float right = width - layout.padding - closeRoom;
    // The action stays on the first line when everything fits on it, else while the text keeps at
    // least half the toast width.
    if (hasAction && (oneLine || right - layout.actionWidth - gap - layout.textX >= width * 0.5f))
        right -= layout.actionWidth + gap;
    else
        layout.actionBelow = hasAction;
    layout.textWidth = std::max(1.0f, right - layout.textX);
    layout.textHeight = MeasureText(TextStyle::Body, entry.text, nullptr, layout.textWidth).y;
    float y = layout.padding + layout.textHeight;
    if (entry.description[0] != '\0')
    {
        layout.descriptionHeight = MeasureText(TextStyle::Footnote, entry.description, nullptr, layout.textWidth).y;
        y += Px(theme.spacing.xs) + layout.descriptionHeight;
    }
    if (entry.progressBar)
    {
        y += gap;
        layout.progressTop = y;
        y += Px(theme.metrics.progressHeightSmall);
    }
    if (layout.actionBelow)
    {
        y += gap;
        layout.actionTop = y;
        y += small;
    }
    layout.height = std::ceil(std::max(y, layout.padding + small) + layout.padding);
    return layout;
}

// Draws one toast in its window and runs its parts. Returns true when it asks to leave.
bool DrawToast(ToastEntry& entry, const ToastLayout& layout, ImGuiWindow* window, bool* actionPressed)
{
    using namespace internal;
    const Theme& theme = T();
    ImDrawList* drawList = window->DrawList;
    const ImRect rect(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
    const float radius = Px(theme.radii.popover);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    drawList->PushClipRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                           false);
    DrawElevation(drawList, rect.Min, rect.Max, radius, 2);
    FillRect(drawList, rect.Min, rect.Max, ToU32(theme.colors.ElevatedSurface(2)), radius);
    StrokeRect(drawList, rect.Min, rect.Max, ToU32(theme.colors.separator), radius, HairlineThickness());
    drawList->PopClipRect();

    // The body takes the drag; the buttons, submitted after it, take their own clicks.
    ItemOptions body;
    body.press = PressMode::Click;
    body.navigable = false;
    body.hitTarget = false;
    body.allowOverlap = true;
    body.handCursor = false;
    const ItemState drag = PartBehavior(window->GetID("##facet.body"), rect, body);

    const Icon icon = entry.icon.IsValid() ? entry.icon : ToneIcon(entry.tone);
    const float firstLine = rect.Min.y + layout.padding;
    if (layout.iconSize > 0.0f)
    {
        const float lineHeight = MeasureText(TextStyle::Body, "Ag").y;
        DrawControlIcon(drawList, icon, rect.Min.x + layout.padding, firstLine + lineHeight * 0.5f, layout.iconSize,
                        ToU32(ToneColor(entry.tone)));
    }
    const float textX = rect.Min.x + layout.textX;
    PushTextStyle(TextStyle::Body);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textX, firstLine), ToU32(theme.colors.label),
                      entry.text, nullptr, layout.textWidth);
    PopTextStyle();
    if (entry.description[0] != '\0')
    {
        PushTextStyle(TextStyle::Footnote);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                          ImVec2(textX, firstLine + layout.textHeight + Px(theme.spacing.xs)),
                          ToU32(theme.colors.labelSecondary), entry.description, nullptr, layout.textWidth);
        PopTextStyle();
    }
    if (entry.progressBar)
    {
        ImGui::SetCursorScreenPos(ImVec2(textX, rect.Min.y + layout.progressTop));
        ProgressBar("##facet.progress", entry.progress,
                    {.size = ControlSize::Small, .width = layout.textWidth / std::max(Px(1.0f), 1.0e-3f)});
    }

    bool leave = false;
    ImRect actionRect;
    const float small = ControlHeight(ControlSize::Small);
    const float gap = Px(theme.spacing.sm);
    const float closeX = rect.Max.x - layout.padding - small;
    const float buttonY = std::round(firstLine + (MeasureText(TextStyle::Body, "Ag").y - small) * 0.5f);
    if (entry.action[0] != '\0')
    {
        const ImVec2 at = layout.actionBelow ? ImVec2(textX, rect.Min.y + layout.actionTop)
                                             : ImVec2(closeX - gap - layout.actionWidth, buttonY);
        ImGui::SetCursorScreenPos(at);
        char label[kToastActionSize + 16];
        std::snprintf(label, sizeof(label), "%s###facet.action", entry.action);
        if (Button(label, {.variant = ButtonVariant::Secondary, .size = ControlSize::Small}))
        {
            *actionPressed = true;
            leave = true;
        }
        actionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }
    ImGui::SetCursorScreenPos(ImVec2(closeX, buttonY));
    char close[96];
    std::snprintf(close, sizeof(close), "%s###facet.close", GetStrings().close);
    if (IconButton(close, Icon("close"), {.variant = ButtonVariant::Plain, .size = ControlSize::Small}))
        leave = true;
    const ImRect parts[3] = {rect, actionRect, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())};
    OverlayRecordRects(OverlayPartKey("toast", entry.id), parts, 3);

    // The swipe: the pointer's way while the body is held, toward the near edge.
    const float toward = CornerIsRight(System().corner) ? 1.0f : -1.0f;
    const ImGuiIO& io = ImGui::GetIO();
    const float dt = std::max(DeltaTime(), 1.0e-4f);
    if (drag.held && !entry.dragging && drag.activated)
    {
        entry.dragging = true;
        entry.pressX = io.MousePos.x - entry.swipe * toward;
        entry.swipeVelocity = 0.0f;
    }
    if (entry.dragging)
    {
        if (drag.held)
        {
            float way = (io.MousePos.x - entry.pressX) * toward;
            if (way < 0.0f)
                way *= kToastSwipeResist;
            const float velocity = io.MouseDelta.x * toward / dt;
            // A short running average, so the speed at release is not one frame's jitter.
            entry.swipeVelocity += (velocity - entry.swipeVelocity) * std::min(1.0f, dt / 0.05f);
            entry.swipe = way;
        }
        else
        {
            entry.dragging = false;
            const float width = rect.GetWidth();
            if (entry.swipe > width * kToastSwipeShare || entry.swipeVelocity > Px(theme.drag.flickSpeed))
                leave = true;
        }
    }
    return leave;
}

// Puts a toast window right below the focused window in Dear ImGui's focus order, so the first
// Ctrl+Tab from there reaches the newest toast.
void PlaceBelowFocusedWindow(ImGuiWindow* window)
{
    ImGuiContext& g = *GImGui;
    if (g.NavWindow == nullptr)
        return;
    const ImGuiWindow* focused = g.NavWindow->RootWindow;
    const int from = window->FocusOrder;
    const int to = focused->FocusOrder;
    if (focused == window || from < 0 || to < 0 || from < to || from >= g.WindowsFocusOrder.Size)
        return;
    for (int i = from; i > to; --i)
    {
        g.WindowsFocusOrder[i] = g.WindowsFocusOrder[i - 1];
        g.WindowsFocusOrder[i]->FocusOrder = static_cast<short>(i);
    }
    g.WindowsFocusOrder[to] = window;
    window->FocusOrder = static_cast<short>(to);
}

void LayOutAndDrawToasts()
{
    ToastSystem& s = System();
    const Theme& theme = T();
    s.corner = s.pendingCorner;
    const int maxShown = std::max(1, theme.overlay.toastMaxCount);
    const bool reduce = ReduceMotion();
    const float motionScale = std::max(0.0f, theme.motion.scale);
    const float dt = DeltaTime();

    // Promote waiting toasts, oldest first, while there is room.
    int shown = 0;
    for (const ToastEntry& entry : s.entries)
        shown += entry.phase == ToastPhase::Shown || entry.phase == ToastPhase::Leaving ? 1 : 0;
    while (shown < maxShown)
    {
        ToastEntry* oldest = nullptr;
        for (ToastEntry& entry : s.entries)
        {
            if (entry.phase == ToastPhase::Waiting && (oldest == nullptr || entry.sequence < oldest->sequence))
                oldest = &entry;
        }
        if (oldest == nullptr)
            break;
        oldest->phase = ToastPhase::Shown;
        oldest->appear = 0.0f;
        oldest->placed = false;
        ++shown;
    }
    if (shown == 0)
        return;

    // Newest first: nearest the corner.
    int order[kToastQueueCapacity];
    int count = 0;
    for (int i = 0; i < kToastQueueCapacity; ++i)
    {
        const ToastPhase phase = s.entries[i].phase;
        if (phase == ToastPhase::Shown || phase == ToastPhase::Leaving)
            order[count++] = i;
    }
    std::sort(order, order + count, [&](int a, int b) { return s.entries[a].sequence > s.entries[b].sequence; });

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = Px(theme.overlay.toastMargin);
    const float gapY = Px(theme.overlay.toastStackGap);
    const float maxWidth = std::max(1.0f, std::min(Px(theme.overlay.toastWidth), viewport->WorkSize.x - margin * 2.0f));
    const bool right = CornerIsRight(s.corner);
    const bool bottom = CornerIsBottom(s.corner);
    // The column the stack takes: as wide as its widest toast, against the corner's edge.
    ToastLayout layouts[kToastQueueCapacity];
    float stackWidth = 0.0f;
    for (int k = 0; k < count; ++k)
    {
        layouts[k] = MeasureToast(s.entries[order[k]], maxWidth);
        stackWidth = std::max(stackWidth, layouts[k].width);
    }
    const float columnX = right ? viewport->WorkPos.x + viewport->WorkSize.x - margin - stackWidth : viewport->WorkPos.x + margin;
    float offset = 0.0f;  // from the corner edge, pixels
    // A strip in the half of the display the stack grows from, across the stack's column (a
    // watermark on a narrow display), moves the stack past it, one stack gap away.
    const float stackEdge = bottom ? viewport->WorkPos.y + viewport->WorkSize.y - margin : viewport->WorkPos.y + margin;
    const float middleY = viewport->WorkPos.y + viewport->WorkSize.y * 0.5f;
    for (int i = 0; i < s.avoidCount; ++i)
    {
        const ImRect& avoid = s.avoid[i];
        if (avoid.Max.x <= columnX || avoid.Min.x >= columnX + stackWidth || (avoid.GetCenter().y > middleY) != bottom)
            continue;
        offset = std::max(offset, bottom ? stackEdge - avoid.Min.y + gapY : avoid.Max.y + gapY - stackEdge);
    }
    const float toward = right ? 1.0f : -1.0f;
    const ImGuiContext& g = *GImGui;

    for (int k = 0; k < count; ++k)
    {
        const int slot = order[k];
        ToastEntry& entry = s.entries[slot];
        const ToastLayout& layout = layouts[k];
        entry.height = layout.height;
        // Every toast keeps to the corner's edge, whatever its width.
        const float width = layout.width;
        const float baseX = right ? viewport->WorkPos.x + viewport->WorkSize.x - margin - width : viewport->WorkPos.x + margin;
        const float targetY = bottom ? viewport->WorkPos.y + viewport->WorkSize.y - margin - offset - layout.height
                                     : viewport->WorkPos.y + margin + offset;
        offset += layout.height + gapY;

        // Motion: the place on a spring, the appearance and leave in time.
        const ImGuiID key = ImHashStr("##facet.toast", 0, entry.id);
        const bool appearing = !entry.placed;
        if (appearing)
        {
            Spring(key, kToastSpringSub, targetY, theme.motion.standard);  // the first query starts at the target
            entry.placed = true;
        }
        const float y = Spring(key, kToastSpringSub, targetY, theme.motion.standard);
        const float openTime = theme.motion.popupOpen * motionScale;
        const float closeTime = theme.motion.popupClose * motionScale;
        entry.appear = reduce || openTime <= 0.0f ? 1.0f : std::min(1.0f, entry.appear + dt / openTime);
        if (entry.phase == ToastPhase::Leaving)
            entry.leave = reduce || closeTime <= 0.0f ? 1.0f : std::min(1.0f, entry.leave + dt / closeTime);
        if (!entry.dragging && entry.phase == ToastPhase::Shown)
        {
            // A released drag that did not dismiss springs back.
            entry.swipe = reduce ? 0.0f : entry.swipe * std::exp(-dt / std::max(1.0e-3f, theme.motion.durationFast));
            if (std::fabs(entry.swipe) < 0.5f)
                entry.swipe = 0.0f;
        }
        const float appear = EaseValue(Ease::OutCubic, entry.appear);
        const float leave = EaseValue(Ease::OutCubic, entry.leave);
        const float slide = (1.0f - appear) * width * kToastSlideShare + entry.swipe + leave * width * kToastSlideShare;
        const float alpha = appear * (1.0f - leave);

        char name[32];
        std::snprintf(name, sizeof(name), "##FacetToast%02d", slot);
        ImGui::SetNextWindowPos(ImVec2(std::round(baseX + slide * toward), std::round(y)));
        ImGui::SetNextWindowSize(ImVec2(width, layout.height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse;
        const bool visible = ImGui::Begin(name, nullptr, flags);
        ImGui::PopStyleVar(2);
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (appearing)
            PlaceBelowFocusedWindow(window);
        // In front of the page windows, behind the popups that are open.
        ImGui::BringWindowToDisplayFront(window);
        for (const ImGuiPopupData& data : g.OpenPopupStack)
        {
            if (data.Window != nullptr && data.Window->WasActive)
                ImGui::BringWindowToDisplayFront(data.Window);
        }
        if (visible)
        {
            const int vertexBegin = window->DrawList->VtxBuffer.Size;
            bool actionPressed = false;
            const bool wantsLeave = DrawToast(entry, layout, window, &actionPressed) && entry.phase == ToastPhase::Shown;
            const bool focused = g.NavWindow != nullptr && g.NavWindow->RootWindow == window;
            const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            if (entry.phase == ToastPhase::Shown)
            {
                if (wantsLeave || (focused && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
                    entry.phase = ToastPhase::Leaving;
                else if (entry.duration >= 0.0f && !hovered && !focused && !entry.dragging)
                    entry.remaining -= dt;
                if (entry.phase == ToastPhase::Shown && entry.duration >= 0.0f && entry.remaining <= 0.0f)
                    entry.phase = ToastPhase::Leaving;
            }
            if (actionPressed)
            {
                s.actionToast = entry.id;
                s.actionFrame = FrameIndex() + 1;
            }
            if (alpha < 1.0f)
                internal::PopupTransformVertices(window->DrawList, vertexBegin, window->DrawList->VtxBuffer.Size, alpha,
                                                 1.0f, window->Pos, ImVec2(0.0f, 0.0f));
        }
        ImGui::End();
    }

    // Toasts whose leave finished give their slot back.
    for (ToastEntry& entry : s.entries)
    {
        if (entry.phase == ToastPhase::Leaving && entry.leave >= 1.0f)
            entry = ToastEntry{};
    }
}

} // namespace

namespace internal
{

void OverlayToastNewFrame()
{
    System().inFrame = true;
    System().avoidCount = 0;
}

void OverlayToastAvoidRect(const ImRect& rect)
{
    ToastSystem& s = System();
    if (s.inFrame && s.avoidCount < kToastAvoidRects)
        s.avoid[s.avoidCount++] = rect;
}

void OverlayToastEndFrame()
{
    ToastSystem& s = System();
    if (!s.inFrame)
        return;
    if (ImGui::GetCurrentContext() != nullptr && GImGui->WithinFrameScope)
        LayOutAndDrawToasts();
    s.inFrame = false;
}

void OverlayToastShutdown()
{
    ToastSystem& s = System();
    for (ToastEntry& entry : s.entries)
        entry = ToastEntry{};
    s.inFrame = false;
    s.actionToast = 0;
    s.actionFrame = 0;
    s.avoidCount = 0;
    s.corner = s.pendingCorner = ScreenCorner::BottomRight;
}

} // namespace internal

ToastId ShowToast(const char* text, const ToastOptions& options)
{
    if (!CanQueue())
        return ToastId{};
    ToastSystem& s = System();
    ToastEntry* slot = nullptr;
    for (ToastEntry& entry : s.entries)
    {
        if (entry.phase == ToastPhase::Free)
        {
            slot = &entry;
            break;
        }
    }
    if (slot == nullptr)
    {
        // Full: the oldest toast goes at once.
        for (ToastEntry& entry : s.entries)
        {
            if (slot == nullptr || entry.sequence < slot->sequence)
                slot = &entry;
        }
    }
    *slot = ToastEntry{};
    slot->id = s.nextId++;
    if (s.nextId == 0)
        s.nextId = 1;
    slot->sequence = ++s.sequence;
    slot->phase = ToastPhase::Waiting;
    FillToast(*slot, text, options);
    return ToastId{slot->id};
}

void UpdateToast(ToastId toast, const char* text, const ToastOptions& options)
{
    ToastEntry* entry = FindToast(toast);
    if (entry == nullptr || entry->phase == ToastPhase::Leaving)
        return;
    FillToast(*entry, text, options);
}

bool ToastActionPressed(ToastId toast)
{
    const ToastSystem& s = System();
    return toast.IsValid() && IsInitialized() && s.actionToast == toast.value && s.actionFrame == FrameIndex();
}

bool IsToastVisible(ToastId toast)
{
    const ToastEntry* entry = FindToast(toast);
    return entry != nullptr && (entry->phase == ToastPhase::Waiting || entry->phase == ToastPhase::Shown);
}

void DismissToast(ToastId toast)
{
    ToastEntry* entry = FindToast(toast);
    if (entry == nullptr)
        return;
    if (entry->phase == ToastPhase::Waiting)
        *entry = ToastEntry{};
    else if (entry->phase == ToastPhase::Shown)
        entry->phase = ToastPhase::Leaving;
}

void SetToastCorner(ScreenCorner corner)
{
    System().pendingCorner = corner;
}

} // namespace facet
