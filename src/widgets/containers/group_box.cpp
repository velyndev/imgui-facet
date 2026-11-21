// BeginGroupBox and EndGroupBox (containers.h): a title (Headline), an optional description
// (Footnote, labelSecondary, wrapped) and the caller's controls inside a separator hairline with
// the card radius and spacing.md padding. A collapsible group box makes its title row a toggle
// (chevron, Enter, Space or a click) and opens and closes its region with an animated height
// (container_layout.h); the open state is kept under the id and saved in the ini file.
//
// The whole box is one group, so after EndGroupBox it is the last item.

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
#include "internal/list_view.h"
#include "internal/scope_stack.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

struct GroupBoxEntry
{
    ImGuiWindow* window = nullptr;
    bool collapsible = false;
    internal::ContainerCollapse collapse;
    internal::ContainerInset inset;
};

struct GroupBoxStack
{
    GroupBoxEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

GroupBoxStack& Boxes()
{
    static GroupBoxStack stack;
    return stack;
}

// A line of text (title or description) as an item without an id, wrapped at `width`.
void GroupBoxTextLine(TextStyle style, const char* text, const char* end, float indent, float width, const ImVec4& color)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const internal::ControlTextScope scope(style);
    const float wrap = std::max(1.0f, width - indent);
    const ImVec2 size = ImGui::CalcTextSize(text, end, false, wrap);
    const ImVec2 pos(window->DC.CursorPos.x + indent, window->DC.CursorPos.y);
    const ImRect box(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(ImVec2(size.x + indent, size.y));
    if (ImGui::ItemAdd(box, 0))
        window->DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos, ToU32(color), text, end, wrap);
}

} // namespace

bool BeginGroupBox(const char* label, const GroupBoxOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";

    ImGuiContext& g = *GImGui;
    const Theme& theme = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const ImGuiID id = window->GetID(label);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    GroupBoxEntry entry;
    entry.window = window;
    entry.collapsible = options.collapsible;
    ImGui::BeginGroup();

    // The title: a toggle row when collapsible, a line of text otherwise.
    bool open = true;
    float textIndent = 0.0f;
    ImGuiLastItemData headerItem{};  // the title row of a collapsible box: the last item after the Begin
    if (options.collapsible)
    {
        open = internal::ContainerOpenState(id, options.defaultOpen, next);
        internal::ContainerHeaderDesc header;
        header.label = label;
        header.labelEnd = labelEnd;
        header.icon = options.icon;
        header.style = TextStyle::Headline;
        header.open = open;
        header.width = width;
        header.height = internal::ListRowHeight(ControlSize::Inherit);
        internal::ApplyNextItemData(next);
        g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasOpen;
        const internal::ItemState item = internal::ContainerHeader(id, header);
        headerItem = g.LastItemData;
        if (item.pressed && !item.readOnly)
        {
            open = !open;
            internal::SetContainerOpenState(id, open);
        }
        textIndent = internal::ContainerHeaderLabelOffset(TextStyle::Headline);
    }
    else if (labelEnd != label || options.icon.IsValid())
    {
        // Icon and title on one line; the title wraps under itself when it is long.
        float iconSlot = 0.0f;
        if (options.icon.IsValid())
        {
            const internal::ControlTextScope text(TextStyle::Headline);
            iconSlot = text.IconSize() + Px(theme.spacing.sm);
            const ImVec2 pos = window->DC.CursorPos;
            if (window->ClipRect.Overlaps(ImRect(pos, ImVec2(pos.x + iconSlot, pos.y + ImGui::GetTextLineHeight()))))
                internal::DrawControlIcon(window->DrawList, options.icon, pos.x, pos.y + ImGui::GetTextLineHeight() * 0.5f,
                                          text.IconSize(), ToU32(internal::ControlSecondaryColor(disabled)));
        }
        if (labelEnd != label)
            GroupBoxTextLine(TextStyle::Headline, label, labelEnd, iconSlot, width, internal::ControlLabelColor(disabled));
        else
            ImGui::Dummy(ImVec2(iconSlot, ImGui::GetTextLineHeight()));
        textIndent = 0.0f;
    }
    if (options.description != nullptr && options.description[0] != '\0')
        GroupBoxTextLine(TextStyle::Footnote, options.description, nullptr, textIndent, width,
                         internal::ControlSecondaryColor(disabled));

    if (options.collapsible && !internal::BeginContainerCollapse(id, open, &entry.collapse))
    {
        ImGui::EndGroup();
        g.LastItemData = headerItem;
        return false;
    }

    // The bordered region: the item spacing above the content counts toward the padding.
    const float padding = Px(theme.spacing.md);
    internal::BeginContainerInset(&entry.inset, width, padding);
    ImGui::Dummy(ImVec2(width, std::max(0.0f, padding - g.Style.ItemSpacing.y)));
    internal::OpenContainerInset(entry.inset);
    // After the Begin the title row is the last item, as after a stock tree node, so
    // ImGui::IsItemHovered and the activity queries describe the toggle.
    if (options.collapsible)
        g.LastItemData = headerItem;

    internal::PushScope("BeginGroupBox");
    GroupBoxStack& stack = Boxes();
    if (stack.depth >= internal::kScopeStackDepth)
        ++stack.overflow;
    else
        stack.entries[stack.depth++] = entry;
    return true;
}

void EndGroupBox()
{
    if (!internal::PopScope("BeginGroupBox"))
        return;
    GroupBoxStack& stack = Boxes();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    const GroupBoxEntry entry = stack.entries[--stack.depth];
    if (GImGui->CurrentWindow != entry.window)
    {
        internal::RestoreContainerInset(entry.inset);
        return;
    }
    const Theme& theme = T();
    const ImRect frame = internal::EndContainerInset(entry.inset, Px(theme.spacing.md));
    if (ImGui::IsRectVisible(frame.Min, frame.Max))
        StrokeRect(entry.window->DrawList, frame.Min, frame.Max, ToU32(theme.colors.separator), Px(theme.radii.card),
                   HairlineThickness());
    if (entry.collapsible)
        internal::EndContainerCollapse(entry.collapse);
    ImGui::EndGroup();
}

namespace internal
{

void ClearGroupBoxStack()
{
    GroupBoxStack& stack = Boxes();
    for (int i = stack.depth - 1; i >= 0; --i)
        RestoreContainerInset(stack.entries[i].inset);
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
