// BeginStack, StackSpacer and EndStack (layout.h): items in a column or a row with a gap token.
//
// The items are laid out by Dear ImGui itself: a vertical stack keeps the window's vertical layout
// with the gap as the vertical item spacing; a horizontal stack switches the window to Dear ImGui's
// horizontal layout type (as a menu bar does), so every item goes on the line after the previous
// one with the gap between them, and restores both at the End. Spacers split the items into runs.
// Each run is a group: the runs are placed along the main axis one after the other, and every
// spacer takes an equal share of the free space (the stack's set length less the runs and gaps, as
// measured in the last frame and kept under the id). On the cross axis each run sits at the start,
// center or end of the stack's cross size by `align`, from its size in the last frame; in a
// vertical stack, Stretch pushes the stack width as the item width, so fields fill it. A vertical
// stack also narrows the window's work rect to its width, so items that fill the available width
// stop at its edge. The whole stack is one group: the last item after EndStack.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/scale.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// Runs of items between spacers that a stack measures; spacers past the last one join their
// items to the last run.
constexpr int kStackRuns = 16;

// The run sizes of the last frame, under the stack's id.
struct StackMeasure
{
    float main[kStackRuns] = {};   // pixels along the main axis
    float cross[kStackRuns] = {};  // pixels across it
    int runs = 0;
    float scale = 0.0f;
};

struct StackEntry
{
    bool active = false;
    ImGuiID id = 0;
    ImGuiWindow* window = nullptr;
    bool horizontal = false;
    LayoutAlign align = LayoutAlign::Start;
    ImVec2 origin = {0.0f, 0.0f};
    float gap = 0.0f;
    float mainSize = 0.0f;   // pixels; 0 fits the runs
    float crossSize = 0.0f;  // pixels
    float spacer = 0.0f;     // pixels each spacer takes this frame
    float cursor = 0.0f;     // where the next run starts on the main axis (offset from origin)
    int run = 0;
    bool runOpen = false;
    ImGuiLayoutType savedLayout = ImGuiLayoutType_Vertical;
    bool pushedWidth = false;
    internal::LayoutWorkRect work;
    StackMeasure measured;   // this frame's sizes
};

struct StackStack
{
    StackEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

StackStack& Stacks()
{
    static StackStack stack;
    return stack;
}

float AlignShare(LayoutAlign align)
{
    switch (align)
    {
    case LayoutAlign::Center:
        return 0.5f;
    case LayoutAlign::End:
        return 1.0f;
    default:
        return 0.0f;
    }
}

void OpenStackRun(StackEntry& entry, const StackMeasure& last)
{
    const int run = std::min(entry.run, kStackRuns - 1);
    const float cross = last.runs > run ? last.cross[run] : 0.0f;
    const float offset = std::max(0.0f, (entry.crossSize - cross) * AlignShare(entry.align));
    const ImVec2 pos = entry.horizontal ? ImVec2(entry.origin.x + entry.cursor, entry.origin.y + offset)
                                        : ImVec2(entry.origin.x + offset, entry.origin.y + entry.cursor);
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginGroup();
    entry.runOpen = true;
}

void CloseStackRun(StackEntry& entry)
{
    if (!entry.runOpen)
        return;
    ImGui::EndGroup();
    entry.runOpen = false;
    const ImRect rect = GImGui->LastItemData.Rect;
    const int run = std::min(entry.run, kStackRuns - 1);
    const float main = entry.horizontal ? rect.GetWidth() : rect.GetHeight();
    const float cross = entry.horizontal ? rect.GetHeight() : rect.GetWidth();
    StackMeasure& m = entry.measured;
    m.main[run] = std::max(m.main[run], main);
    m.cross[run] = std::max(m.cross[run], cross);
    m.runs = std::max(m.runs, run + 1);
    const float end = entry.horizontal ? rect.Max.x - entry.origin.x : rect.Max.y - entry.origin.y;
    entry.cursor = std::max(entry.cursor, end);
}

StackEntry* TopStack()
{
    StackStack& stack = Stacks();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    return &stack.entries[stack.depth - 1];
}

} // namespace

void BeginStack(const char* id, const StackOptions& options)
{
    internal::PushScope("BeginStack");
    StackStack& stack = Stacks();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    StackEntry& entry = stack.entries[stack.depth++];
    entry = StackEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";

    ImGuiContext& g = *GImGui;
    entry.active = true;
    entry.id = window->GetID(id);
    entry.window = window;
    entry.horizontal = options.orientation == Orientation::Horizontal;
    entry.align = options.align;
    entry.gap = internal::LayoutGapPixels(options.gap);
    const internal::NextItemData next = internal::TakeNextItemData();
    const StackMeasure& last = internal::GetState<StackMeasure>(entry.id);
    StackMeasure known = last.scale == Scale() ? last : StackMeasure{};

    float width = 0.0f;
    if (!entry.horizontal || options.width != 0.0f || next.hasWidth)
        width = internal::ResolveItemWidth(options.width, next, kFill);
    const float height = options.height > 0.0f ? Px(options.height) : 0.0f;
    float widestCross = 0.0f;
    for (int i = 0; i < known.runs; ++i)
        widestCross = std::max(widestCross, known.cross[i]);
    if (entry.horizontal)
    {
        entry.mainSize = width;
        entry.crossSize = height > 0.0f ? height : widestCross;
    }
    else
    {
        entry.mainSize = height;
        entry.crossSize = width;
    }
    // The free space each spacer takes, from the run sizes of the last frame.
    const int spacers = known.runs - 1;
    if (entry.mainSize > 0.0f && spacers > 0)
    {
        float used = entry.gap * static_cast<float>(spacers);
        for (int i = 0; i < known.runs; ++i)
            used += known.main[i];
        entry.spacer = std::max(0.0f, entry.mainSize - used) / static_cast<float>(spacers);
    }

    ImGui::BeginGroup();
    entry.origin = window->DC.CursorPos;
    entry.savedLayout = window->DC.LayoutType;
    window->DC.LayoutType = entry.horizontal ? ImGuiLayoutType_Horizontal : ImGuiLayoutType_Vertical;
    const ImVec2 spacing = g.Style.ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        entry.horizontal ? ImVec2(entry.gap, spacing.y) : ImVec2(spacing.x, entry.gap));
    const float right = entry.origin.x + (entry.horizontal ? entry.mainSize : entry.crossSize);
    if (!entry.horizontal || entry.mainSize > 0.0f)
        entry.work = internal::NarrowLayoutWorkRect(window, right);
    if (!entry.horizontal && entry.align == LayoutAlign::Stretch)
    {
        ImGui::PushItemWidth(std::max(1.0f, entry.crossSize));
        entry.pushedWidth = true;
    }
    OpenStackRun(entry, known);
}

void StackSpacer()
{
    StackEntry* entry = TopStack();
    const char* innermost = internal::InnermostScope();
    if (entry == nullptr || innermost == nullptr || std::strcmp(innermost, "BeginStack") != 0)
    {
        internal::ReportUsageError("StackSpacer outside a stack (or inside a container nested in one)");
        return;
    }
    if (!entry->active || GImGui->CurrentWindow != entry->window)
        return;
    if (entry->run >= kStackRuns - 1)
        return;  // further spacers join their items to the last run
    CloseStackRun(*entry);
    const StackMeasure& last = internal::GetState<StackMeasure>(entry->id);
    entry->cursor += entry->gap + entry->spacer;
    ++entry->run;
    OpenStackRun(*entry, last.scale == Scale() ? last : StackMeasure{});
}

void EndStack()
{
    if (!internal::PopScope("BeginStack"))
        return;
    StackStack& stack = Stacks();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    StackEntry& entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    ImGuiWindow* window = entry.window;
    if (GImGui->CurrentWindow != window)
    {
        internal::RestoreLayoutWorkRect(entry.work);
        return;  // the caller ended the window inside the stack; Dear ImGui reports that itself
    }
    CloseStackRun(entry);
    if (entry.pushedWidth)
        ImGui::PopItemWidth();
    internal::RestoreLayoutWorkRect(entry.work);
    ImGui::PopStyleVar();
    window->DC.LayoutType = entry.savedLayout;

    // The stack's own size: its set lengths, else what its runs took.
    float crossUsed = 0.0f;
    for (int i = 0; i < entry.measured.runs; ++i)
        crossUsed = std::max(crossUsed, entry.measured.cross[i]);
    const float main = std::max(entry.mainSize, entry.cursor);
    const float cross = std::max(entry.crossSize, crossUsed);
    ImGui::SetCursorScreenPos(entry.origin);
    ImGui::Dummy(entry.horizontal ? ImVec2(main, cross) : ImVec2(cross, main));
    ImGui::EndGroup();

    entry.measured.scale = Scale();
    internal::GetState<StackMeasure>(entry.id) = entry.measured;
}

namespace internal
{

void ClearStackStack()
{
    StackStack& stack = Stacks();
    for (int i = stack.depth - 1; i >= 0; --i)
        if (stack.entries[i].active)
        {
            RestoreLayoutWorkRect(stack.entries[i].work);
            if (stack.entries[i].window != nullptr)
                stack.entries[i].window->DC.LayoutType = stack.entries[i].savedLayout;
        }
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
