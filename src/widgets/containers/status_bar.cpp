// BeginStatusBar, StatusItem and EndStatusBar (containers.h): a strip of status segments.
//
// The strip is containers.statusBarHeight tall on bgSecondary with a separator hairline along its
// top, so it reads as window chrome apart from the content above it. Segments run from the leading
// edge with spacing.lg between them: a status dot or the icon in the segment's tone, then the text
// in Footnote and labelSecondary. The widths and priorities of the last frame are kept under the
// id; BeginStatusBar runs the overflow fitter (src/internal/overflow.h) over them, so the segments
// that do not fit are hidden, lowest priority first and the last of equals first. Other widgets
// submitted between the Begin and End sit after the previous segment and are never hidden.

#include "facet/widgets/containers.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/overflow.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"

namespace facet
{
namespace
{

constexpr int kStatusBarMaxItems = 32;
static_assert(kStatusBarMaxItems <= internal::kOverflowInlineItems);
constexpr int kStatusBarDepth = 2;

struct StatusBarState
{
    internal::OverflowItem items[kStatusBarMaxItems] = {};
    int count = 0;
    float foreignWidth = 0.0f;
    internal::OverflowState overflow;
};

struct StatusBarEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    ImRect bar;
    float gap = 0.0f;
    bool decided[kStatusBarMaxItems] = {};
    int decidedCount = 0;
    int index = 0;
    float expectedX = 0.0f;
    float foreignWidth = 0.0f;
    internal::OverflowItem items[kStatusBarMaxItems] = {};
    bool disabled = false;
};

struct StatusBarStack
{
    StatusBarEntry entries[kStatusBarDepth];
    int depth = 0;
    int overflow = 0;
};

StatusBarStack& StatusBars()
{
    static StatusBarStack stack;
    return stack;
}

StatusBarEntry* CurrentStatusBar()
{
    StatusBarStack& stack = StatusBars();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    StatusBarEntry& entry = stack.entries[stack.depth - 1];
    return entry.active && entry.window == GImGui->CurrentWindow ? &entry : nullptr;
}

ImVec4 StatusToneColor(Tone tone)
{
    const Colors& c = T().colors;
    switch (tone)
    {
    case Tone::Accent: return c.accentText;
    case Tone::Success: return c.success;
    case Tone::Warning: return c.warning;
    case Tone::Destructive: return c.destructive;
    case Tone::Neutral:
    default: return c.labelSecondary;
    }
}

// The x of the next segment; notes the width of the caller's widgets since the last segment.
float StatusNextX(StatusBarEntry& entry)
{
    ImGuiWindow* window = entry.window;
    float x = window->DC.CursorPos.x;
    if (window->DC.CursorPos.y > entry.bar.Min.y + 0.5f)
        x = window->DC.CursorPosPrevLine.x + entry.gap;
    if (x > entry.expectedX + 0.5f)
        entry.foreignWidth += x - entry.expectedX;
    return x;
}

} // namespace

void BeginStatusBar(const char* id, const StatusBarOptions& options)
{
    internal::PushScope("BeginStatusBar");
    StatusBarStack& stack = StatusBars();
    if (stack.depth >= kStatusBarDepth)
    {
        ++stack.overflow;
        return;
    }
    StatusBarEntry& entry = stack.entries[stack.depth++];
    entry.active = false;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float height = Px(t.containers.statusBarHeight);
    const float pad = Px(t.spacing.md);

    entry.active = true;
    entry.window = window;
    entry.id = window->GetID(id);
    const ImVec2 origin = window->DC.CursorPos;
    entry.bar = ImRect(origin, ImVec2(origin.x + width, origin.y + height));
    entry.gap = Px(t.spacing.lg);
    entry.index = 0;
    entry.expectedX = origin.x + pad;
    entry.foreignWidth = 0.0f;
    entry.disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    StatusBarState& state = internal::GetState<StatusBarState>(entry.id);
    const float available = std::max(0.0f, width - pad * 2.0f - state.foreignWidth);
    (void)internal::FitOverflowWithoutButton(
        std::span<const internal::OverflowItem>(state.items, static_cast<std::size_t>(state.count)), available, entry.gap,
        Px(t.spacing.lg), &state.overflow, std::span<bool>(entry.decided));
    entry.decidedCount = state.count;

    if (ImGui::IsRectVisible(entry.bar.Min, entry.bar.Max))
    {
        FillRect(window->DrawList, entry.bar.Min, entry.bar.Max, ToU32(t.colors.bgSecondary), 0.0f);
        HairlineH(window->DrawList, entry.bar.Min.x, entry.bar.Max.x, entry.bar.Min.y, ToU32(t.colors.separator));
    }
    ImGui::BeginGroup();
    GImGui->LastItemData.Rect = ImRect(origin, origin);  // an empty bar must not grow to an earlier item
    window->DC.CursorPos = ImVec2(entry.expectedX, entry.bar.Min.y);
}

void EndStatusBar()
{
    if (!internal::PopScope("BeginStatusBar"))
        return;
    StatusBarStack& stack = StatusBars();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    StatusBarEntry& entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    entry.active = false;
    ImGuiWindow* window = entry.window;
    if (GImGui->CurrentWindow != window)
        return;
    (void)StatusNextX(entry);
    StatusBarState& state = internal::GetState<StatusBarState>(entry.id);
    const int count = std::min(entry.index, kStatusBarMaxItems);
    std::memcpy(state.items, entry.items, sizeof(internal::OverflowItem) * static_cast<std::size_t>(count));
    state.count = count;
    state.foreignWidth = entry.foreignWidth;
    // The strip is one item of its whole size.
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, entry.bar.Max.x);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, entry.bar.Max.y);
    ImGui::EndGroup();
}

void StatusItem(const char* text, const StatusItemOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (text == nullptr)
        text = "";
    const Theme& t = T();
    StatusBarEntry* entry = CurrentStatusBar();
    const internal::ControlTextScope scope(TextStyle::Footnote);
    const float iconSize = scope.IconSize();
    const float dot = Px(t.metrics.statusDotSize);
    const bool hasIcon = options.icon.IsValid();
    const bool hasDot = !hasIcon && options.tone != Tone::Neutral;
    const float mark = hasIcon ? iconSize : (hasDot ? dot : 0.0f);
    const float markGap = mark > 0.0f ? Px(t.spacing.xs) : 0.0f;
    const float width = std::ceil(mark + markGap + ImGui::CalcTextSize(text).x);
    const bool disabled =
        entry != nullptr ? entry->disabled : (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

    float x = window->DC.CursorPos.x;
    float top = window->DC.CursorPos.y;
    float height = ImGui::GetTextLineHeight();
    float limit = window->WorkRect.Max.x;
    if (entry != nullptr)
    {
        x = StatusNextX(*entry);
        const int index = entry->index++;
        if (index < kStatusBarMaxItems)
            entry->items[index] = {width, options.priority, internal::OverflowKind::Item};
        if (index < entry->decidedCount && !entry->decided[index])
            return;
        top = entry->bar.Min.y;
        height = entry->bar.GetHeight();
        limit = entry->bar.Max.x - Px(t.spacing.md);
        if (x >= limit)
            return;  // the row changed since the last frame and leaves no room at all
    }
    const ImRect rect(ImVec2(x, top), ImVec2(x + std::min(width, std::max(0.0f, limit - x)), top + height));
    window->DC.CursorPos = rect.Min;
    ImGui::ItemSize(rect.GetSize());
    bool cut = false;
    if (ImGui::ItemAdd(rect, 0))
    {
        ImDrawList* drawList = window->DrawList;
        const float centerY = rect.GetCenter().y;
        const ImU32 toneColor = ToU32(disabled ? t.colors.labelTertiary : StatusToneColor(options.tone));
        float cx = rect.Min.x;
        if (hasIcon)
            internal::DrawControlIcon(drawList, options.icon, cx, centerY, iconSize, toneColor);
        else if (hasDot)
            drawList->AddCircleFilled(ImVec2(cx + dot * 0.5f, centerY), dot * 0.5f, toneColor);
        cx += mark + markGap;
        const internal::FittedText fitted = internal::FitText(text, nullptr, std::max(0.0f, rect.Max.x - cx));
        cut = fitted.truncated;
        drawList->AddText(ImVec2(cx, CenteredTextY(rect.Min.y, rect.GetHeight())),
                          ToU32(internal::ControlSecondaryColor(disabled)), fitted.text);
    }
    const bool hovered = GImGui->HoveredWindow == window && ImGui::IsMouseHoveringRect(rect.Min, rect.Max);
    // Its own id, apart from an item of the caller with the same text.
    internal::CutTextTooltip(ImHashStr("##status", 0, window->GetID(text)), rect, hovered, false, text, nullptr, cut);
    if (entry != nullptr)
    {
        entry->expectedX = rect.Max.x + entry->gap;
        ImGui::SameLine(0.0f, entry->gap);
    }
}

} // namespace facet
