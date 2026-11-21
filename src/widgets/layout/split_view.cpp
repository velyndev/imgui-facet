// BeginSplitView, SplitViewNext and EndSplitView (layout.h): two panes with a divider between them.
//
// Layout: the view is `width` x `height` (0 fills the available width and height; with no height
// left, metrics.panelHeight). The divider is a band of layout.dividerBand between the panes. The
// first pane gets *ratio of the room (the view less the band), kept between its minimum and the
// room less the second pane's minimum. When the room is smaller than both minimums together, the
// panes share it in the proportion of their minimums and the divider moves nothing. A collapsible
// pane collapses to nothing at a ratio of 0 (the first) or 1 (the second). In a Compact width a
// horizontal split shows its panes one above the other. The ratio is read every frame, so the
// panes keep their shares when the view is resized.
//
// Each pane is a scroll area of the scroll core (a child window with Facet's overlay bars, keyboard
// navigation flattened into the view's window), so its content scrolls and keeps its ids under
// the pane. A collapsed pane is a hidden child window: its content is submitted and clipped away,
// and takes neither the mouse nor the keyboard.
//
// The divider (layout_internal.h) is submitted between the panes, so Tab goes from the first pane
// to the divider to the second. Mouse: drag it (past half a minimum a collapsible pane collapses),
// double click to go back to the ratio the view first had. Keys while it is focused: the arrows of
// its axis move it by spacing.lg (Shift spacing.xs, Ctrl a coarse step), Home and End to the
// minimums, Enter or Space collapse the first pane of a collapsible view or bring a collapsed pane
// back to the ratio it had. Escape during a drag puts the ratio from the drag's start back. Every
// edit writes *ratio at once.
//
// Remembered ratios: the view keeps, under its id in the Dear ImGui ini file, the last ratio the
// user set, the ratio before a collapse and the ratio it first had. In a later run the first
// BeginSplitView writes the saved ratio to *ratio once, so the split survives a restart even when
// the caller starts from a constant.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// The ratios a split view remembers across frames and runs.
struct SplitViewMemory
{
    static constexpr const char* kStateName = "SplitView";
    float saved = -1.0f;    // the last ratio the user set; < 0 for none
    float initial = -1.0f;  // the ratio of the first frame the view was seen in; < 0 before that
    float restore = -1.0f;  // the ratio before the last collapse; < 0 for none
    bool loaded = false;    // read from the ini file in this run
    bool applied = false;   // the saved ratio was handed to the caller in this run

    void WriteIni(ImGuiTextBuffer& out) const
    {
        if (saved >= 0.0f)
            out.appendf("ratio=%.4f\n", saved);
        if (initial >= 0.0f)
            out.appendf("initial=%.4f\n", initial);
        if (restore >= 0.0f)
            out.appendf("restore=%.4f\n", restore);
    }
    void ReadIni(const char* key, const char* value)
    {
        const float number = static_cast<float>(std::atof(value));
        if (!(number >= 0.0f && number <= 1.0f))
            return;
        if (std::strcmp(key, "ratio") == 0)
            saved = number;
        else if (std::strcmp(key, "initial") == 0)
            initial = number;
        else if (std::strcmp(key, "restore") == 0)
            restore = number;
        loaded = true;
    }
};

// Where a mouse drag of the divider started, for Escape.
struct SplitViewDrag
{
    float startRatio = 0.5f;
};

struct SplitViewEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    float* ratio = nullptr;
    bool vertical = false;     // the panes sit one above the other
    ImVec2 origin = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
    float first = 0.0f;        // the first pane's length on the axis, pixels
    float room = 0.0f;         // the view's length less the band
    float band = 0.0f;
    float minFirst = 0.0f;
    float minSecond = 0.0f;
    bool collapsible = false;
    bool fits = true;          // the room holds both minimums
    int pane = 0;              // 0 none open, 1 the first, 2 the second
    int panesDone = 0;
    bool paneArea = false;     // the open pane is a scroll area (else a plain child window)
    bool paneClip = false;     // a clip rect was pushed into a collapsed pane
    ImGuiID dividerId = 0;
    internal::ItemState divider;
    bool dividerRun = false;
    internal::ForcedItemRequest forced;
};

struct SplitViewStack
{
    SplitViewEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

SplitViewStack& Splits()
{
    static SplitViewStack stack;
    return stack;
}

internal::LayoutDividerRecord& LastDivider()
{
    static internal::LayoutDividerRecord record;
    return record;
}

float SanitizeRatio(float ratio)
{
    return ratio >= 0.0f && ratio <= 1.0f ? ratio : (ratio > 1.0f ? 1.0f : (ratio < 0.0f ? 0.0f : 0.5f));
}

// The first pane's length for `ratio`, by the rules at the top of this file.
float SplitFirstLength(const SplitViewEntry& e, float ratio)
{
    if (!e.fits)
    {
        const float sum = e.minFirst + e.minSecond;
        return sum > 0.0f ? e.room * e.minFirst / sum : e.room * 0.5f;
    }
    if (e.collapsible && ratio <= 0.0f)
        return 0.0f;
    if (e.collapsible && ratio >= 1.0f)
        return e.room;
    return std::clamp(ratio * e.room, e.minFirst, e.room - e.minSecond);
}

ImRect SplitPaneRect(const SplitViewEntry& e, int pane)
{
    const float start = pane == 1 ? 0.0f : e.first + e.band;
    const float length = pane == 1 ? e.first : std::max(0.0f, e.room - e.first);
    if (e.vertical)
        return ImRect(ImVec2(e.origin.x, e.origin.y + start), ImVec2(e.origin.x + e.size.x, e.origin.y + start + length));
    return ImRect(ImVec2(e.origin.x + start, e.origin.y), ImVec2(e.origin.x + start + length, e.origin.y + e.size.y));
}

ImRect SplitBandRect(const SplitViewEntry& e)
{
    if (e.vertical)
        return ImRect(ImVec2(e.origin.x, e.origin.y + e.first), ImVec2(e.origin.x + e.size.x, e.origin.y + e.first + e.band));
    return ImRect(ImVec2(e.origin.x + e.first, e.origin.y), ImVec2(e.origin.x + e.first + e.band, e.origin.y + e.size.y));
}

void OpenSplitPane(SplitViewEntry& e, int pane)
{
    const ImRect rect = SplitPaneRect(e, pane);
    const ImGuiID paneId = ImHashStr(pane == 1 ? "##first" : "##second", 0, e.id);
    ImGui::SetCursorScreenPos(rect.Min);
    e.pane = pane;
    e.paneClip = false;
    const float length = e.vertical ? rect.GetHeight() : rect.GetWidth();
    if (length < 1.0f)
    {
        // Collapsed: a hidden child window, so the content keeps its ids but shows nothing.
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::BeginChild(paneId, ImVec2(1.0f, 1.0f), ImGuiChildFlags_None, flags);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::PushClipRect(at, at, true);
        e.paneClip = true;
        e.paneArea = false;
        return;
    }
    internal::ScrollAreaDesc desc;
    desc.size = rect.GetSize();
    e.paneArea = internal::BeginScrollAreaCore(paneId, desc);
    if (!e.paneArea)
    {
        // Out of sight: a plain child window of the same size skips the content's items.
        ImGui::SetCursorScreenPos(rect.Min);
        ImGui::BeginChild(ImHashStr("##hidden", 0, paneId), rect.GetSize(), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
    }
}

void CloseSplitPane(SplitViewEntry& e)
{
    if (e.pane == 0)
        return;
    if (e.paneClip)
        ImGui::PopClipRect();
    if (e.paneArea)
        internal::EndScrollAreaCore();
    else
        ImGui::EndChild();
    e.pane = 0;
    e.paneClip = false;
    ++e.panesDone;
}

void WriteSplitRatio(SplitViewEntry& e, SplitViewMemory& memory, float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    if (e.ratio == nullptr || *e.ratio == value)
        return;
    *e.ratio = value;
    memory.saved = value;
    internal::MarkStateDirty();
}

// Runs the divider between the panes and applies its input to the ratio; lays out the rest of the
// view with the result.
void RunSplitDivider(SplitViewEntry& e)
{
    e.dividerRun = true;
    const ImGuiAxis axis = e.vertical ? ImGuiAxis_Y : ImGuiAxis_X;
    const float origin = e.vertical ? e.origin.y : e.origin.x;
    internal::ItemOptions options;
    options.forced = &e.forced;
    options.part = 0;
    const internal::LayoutDividerInput input = internal::LayoutDivider(e.dividerId, SplitBandRect(e), axis, e.first, origin, options);
    e.divider = input.item;
    SplitViewMemory& memory = internal::GetPersistentState<SplitViewMemory>(e.id);
    SplitViewDrag& drag = internal::GetState<SplitViewDrag>(e.id);
    const float current = e.ratio != nullptr ? SanitizeRatio(*e.ratio) : 0.5f;
    const bool collapsed = e.collapsible && (current <= 0.0f || current >= 1.0f);
    const float room = std::max(1.0f, e.room);
    if (e.ratio != nullptr && e.fits)
    {
        if (input.started)
            drag.startRatio = current;
        if (input.canceled)
            WriteSplitRatio(e, memory, drag.startRatio);
        else if (input.reset)
            WriteSplitRatio(e, memory, memory.initial >= 0.0f ? memory.initial : 0.5f);
        else if (input.dragging)
        {
            float target = input.dragTarget;
            if (e.collapsible && target < e.minFirst * 0.5f)
                target = 0.0f;
            else if (e.collapsible && target > e.room - e.minSecond * 0.5f)
                target = e.room;
            else
                target = std::clamp(target, e.minFirst, e.room - e.minSecond);
            if (std::fabs(target - e.first) >= 0.5f)
                WriteSplitRatio(e, memory, target / room);
        }
        else if (input.steps != 0)
        {
            float from = e.first;
            if (collapsed && current <= 0.0f && input.steps > 0)
                from = e.minFirst - input.stepPixels;
            if (collapsed && current >= 1.0f && input.steps < 0)
                from = e.room - e.minSecond + input.stepPixels;
            const float target = std::clamp(from + input.stepPixels * static_cast<float>(input.steps), e.minFirst,
                                            e.room - e.minSecond);
            WriteSplitRatio(e, memory, target / room);
        }
        else if (input.home)
            WriteSplitRatio(e, memory, e.minFirst / room);
        else if (input.end)
            WriteSplitRatio(e, memory, (e.room - e.minSecond) / room);
        else if (input.toggle && e.collapsible)
        {
            if (collapsed)
                WriteSplitRatio(e, memory, memory.restore >= 0.0f ? memory.restore : (memory.initial >= 0.0f ? memory.initial : 0.5f));
            else
            {
                memory.restore = current;
                WriteSplitRatio(e, memory, 0.0f);
            }
        }
    }
    e.first = SplitFirstLength(e, e.ratio != nullptr ? SanitizeRatio(*e.ratio) : 0.5f);
}

SplitViewEntry* TopSplit()
{
    SplitViewStack& stack = Splits();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    return &stack.entries[stack.depth - 1];
}

} // namespace

void BeginSplitView(const char* id, float* ratio, const SplitViewOptions& options)
{
    internal::PushScope("BeginSplitView");
    SplitViewStack& stack = Splits();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    SplitViewEntry& e = stack.entries[stack.depth++];
    e = SplitViewEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";

    const Theme& theme = T();
    e.active = true;
    e.window = window;
    e.id = window->GetID(id);
    e.dividerId = ImHashStr("##divider", 0, e.id);
    e.ratio = ratio;
    e.forced = internal::TakeForcedItemState();
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    e.size.x = internal::ResolveItemWidth(options.width, next, kFill);
    e.size.y = options.height > 0.0f ? Px(options.height) : (available.y >= 1.0f ? available.y : Px(theme.metrics.panelHeight));
    e.vertical = options.orientation == Orientation::Vertical ||
                 WidthClassOf(e.size.x / Scale()) == WidthClass::Compact;
    e.collapsible = options.collapsible;
    e.band = Px(theme.layout.dividerBand);
    e.room = std::max(0.0f, (e.vertical ? e.size.y : e.size.x) - e.band);
    e.minFirst = Px(options.minFirst > 0.0f ? options.minFirst : theme.layout.minRowControl);
    e.minSecond = Px(options.minSecond > 0.0f ? options.minSecond : theme.layout.minRowControl);
    e.fits = e.room >= e.minFirst + e.minSecond;

    // The ratio the view first had, and the saved ratio of an earlier run.
    SplitViewMemory& memory = internal::GetPersistentState<SplitViewMemory>(e.id);
    if (ratio != nullptr)
    {
        if (memory.loaded && !memory.applied && memory.saved >= 0.0f)
            *ratio = memory.saved;
        if (memory.initial < 0.0f)
        {
            memory.initial = SanitizeRatio(*ratio);
            internal::MarkStateDirty();
        }
    }
    memory.applied = true;
    e.first = SplitFirstLength(e, ratio != nullptr ? SanitizeRatio(*ratio) : 0.5f);

    ImGui::BeginGroup();
    e.origin = window->DC.CursorPos;
    OpenSplitPane(e, 1);
}

void SplitViewNext()
{
    SplitViewEntry* e = TopSplit();
    const char* innermost = internal::InnermostScope();
    if (e == nullptr || innermost == nullptr || std::strcmp(innermost, "BeginSplitView") != 0)
    {
        internal::ReportUsageError("SplitViewNext outside a split view (or inside a container nested in one)");
        return;
    }
    if (!e->active || e->pane != 1)
        return;
    CloseSplitPane(*e);
    if (GImGui->CurrentWindow != e->window)
        return;
    RunSplitDivider(*e);
    OpenSplitPane(*e, 2);
}

void EndSplitView()
{
    if (!internal::PopScope("BeginSplitView"))
        return;
    SplitViewStack& stack = Splits();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    SplitViewEntry& e = stack.entries[--stack.depth];
    if (!e.active)
        return;
    CloseSplitPane(e);
    if (GImGui->CurrentWindow != e.window)
        return;  // the caller ended a window inside the view; Dear ImGui reports that itself
    if (!e.dividerRun)
    {
        // Only one pane was submitted: the divider and an empty second pane still follow.
        RunSplitDivider(e);
        OpenSplitPane(e, 2);
        CloseSplitPane(e);
    }
    internal::DrawLayoutDivider(e.divider, SplitBandRect(e), e.vertical ? ImGuiAxis_Y : ImGuiAxis_X);
    internal::LayoutDividerRecord& record = LastDivider();
    record.id = e.dividerId;
    record.band = SplitBandRect(e);
    record.vertical = e.vertical;
    ImGui::SetCursorScreenPos(e.origin);
    ImGui::Dummy(e.size);
    ImGui::EndGroup();
}

namespace internal
{

const LayoutDividerRecord& LastSplitViewDivider()
{
    return LastDivider();
}

void ClearSplitViewStack()
{
    SplitViewStack& stack = Splits();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
