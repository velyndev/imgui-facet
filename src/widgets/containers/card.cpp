// BeginCard and EndCard (containers.h): a grouped surface in bgSecondary with the card radius,
// an optional header line (icon and title) and the caller's content inside spacing.lg padding.
//
// The content is laid out inside a group whose work rect is narrowed to the inner edge, so items
// that fill the available width stop at the padding. The fill is drawn at the End, under the
// content, through the backdrop split of container_layout.h. Ids are not scoped by the card.

#include "facet/widgets/containers.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

struct CardEntry
{
    bool active = false;    // drawn: inside a frame, in a window that is not skipped
    int backdrop = -1;
    bool pushedFade = false;
    float minHeight = 0.0f;
    internal::ContainerInset inset;
};

struct CardStack
{
    CardEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

CardStack& Cards()
{
    static CardStack stack;
    return stack;
}

// Draws the header line (icon, title) at `top`, between `left` and `right`. Returns whether
// the title was cut.
bool DrawCardHeader(ImDrawList* drawList, const char* label, const char* labelEnd, Icon icon, float left, float right,
                    float top, float lineHeight, bool disabled)
{
    const internal::ControlTextScope text(TextStyle::Headline);
    float x = left;
    const float centerY = top + lineHeight * 0.5f;
    if (icon.IsValid())
    {
        internal::DrawControlIcon(drawList, icon, x, centerY, text.IconSize(),
                                  ToU32(internal::ControlSecondaryColor(disabled)));
        x += text.IconSize() + Px(T().spacing.sm);
    }
    if (labelEnd == label)
        return false;
    const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, right - x));
    drawList->AddText(ImVec2(x, CenteredTextY(top, lineHeight)), ToU32(internal::ControlLabelColor(disabled)),
                      shown.text);
    return shown.truncated;
}

} // namespace

void BeginCard(const char* label, const CardOptions& options)
{
    internal::PushScope("BeginCard");
    CardStack& stack = Cards();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    CardEntry& entry = stack.entries[stack.depth++];
    entry = CardEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";

    ImGuiContext& g = *GImGui;
    const Theme& theme = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float padding = Px(theme.spacing.lg);
    const bool disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasHeader = labelEnd != label || options.icon.IsValid();

    entry.active = true;
    entry.minHeight = options.height > 0.0f ? Px(options.height) : 0.0f;
    entry.backdrop = internal::BeginContainerBackdrop(window->DrawList);
    internal::BeginContainerInset(&entry.inset, width, padding);

    // The header line, or the top padding alone. The item spacing after it counts toward the gap.
    const float spacing = g.Style.ItemSpacing.y;
    const ImVec2 start = entry.inset.start;
    float headerHeight = std::max(0.0f, padding - spacing);
    if (hasHeader)
    {
        float lineHeight = 0.0f;
        {
            const internal::ControlTextScope text(TextStyle::Headline);
            lineHeight = std::max(ImGui::GetTextLineHeight(), text.IconSize());
        }
        headerHeight = padding + lineHeight + std::max(0.0f, Px(theme.spacing.sm) - spacing);
        const ImRect line(ImVec2(start.x + padding, start.y + padding),
                          ImVec2(std::max(start.x + padding, entry.inset.right - padding), start.y + padding + lineHeight));
        const bool cut = window->ClipRect.Overlaps(line) &&
                         DrawCardHeader(window->DrawList, label, labelEnd, options.icon, line.Min.x, line.Max.x,
                                        line.Min.y, lineHeight, disabled);
        const bool hovered = g.HoveredWindow == window && ImGui::IsMouseHoveringRect(line.Min, line.Max);
        internal::CutTextTooltip(window->GetID(label), line, hovered, false, label, labelEnd, cut);
    }
    ImGui::Dummy(ImVec2(width, headerHeight));
    internal::OpenContainerInset(entry.inset);
    internal::PushScrollFadeSurface(theme.colors.bgSecondary);
    entry.pushedFade = true;
}

void EndCard()
{
    if (!internal::PopScope("BeginCard"))
        return;
    CardStack& stack = Cards();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    const CardEntry entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    if (entry.pushedFade)
        internal::PopScrollFadeSurface();
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow != entry.inset.window)
    {
        // The caller ended the window inside the card; Dear ImGui reports that itself.
        internal::RestoreContainerInset(entry.inset);
        internal::EndContainerBackdrop(entry.backdrop);
        return;
    }
    const ImRect frame = internal::EndContainerInset(entry.inset, Px(T().spacing.lg), entry.minHeight);
    internal::ContainerBackdropBehind(entry.backdrop);
    FillRect(entry.inset.window->DrawList, frame.Min, frame.Max, ToU32(T().colors.bgSecondary), Px(T().radii.card));
    internal::EndContainerBackdrop(entry.backdrop);
}

namespace internal
{

void ClearCardStack()
{
    CardStack& stack = Cards();
    for (int i = stack.depth - 1; i >= 0; --i)
        if (stack.entries[i].active)
            RestoreContainerInset(stack.entries[i].inset);
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
