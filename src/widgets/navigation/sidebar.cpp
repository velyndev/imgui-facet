// BeginSidebar, SidebarItem, BeginSidebarSection and their Ends (navigation.h): a navigation
// column on the bgSecondary surface with the sliding selection pill, one Tab stop with Up and
// Down inside, sections that can collapse, count badges, and an icon rail for narrow widths.
//
// The column is a scroll area (scroll.h) of the sidebar's width and height, so a long list scrolls
// with Facet's overlay bars. Its width springs (motion.standard) between the expanded width and
// the rail width when the mode changes; the content takes the new mode at once and the column
// clips it while it moves.
//
// The pill is one SelectionPill for the whole column, begun in BeginSidebar and ended in
// EndSidebar, so it slides between items of different sections with the spring of
// SegmentedControl. It animates relative to the column's content origin (window position minus
// scroll), so scrolling moves it with its item.
//
// Rows: list.rowHeight tall, spacing.sm from the column edges. Expanded: icon, label (cut with an
// ellipsis and a tooltip), badge at the end. Rail: the icon centered in a square row, the badge on
// its top-right corner, the label as a tooltip on hover and keyboard focus. Section headers are
// Footnote text in labelSecondary; a collapsible header is a row of its own with a chevron, in the
// roving order with the items. In rail mode a header is a hairline and its items always show,
// because a header that is not drawn cannot be operated.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/navigation/navigation_parts.h"

namespace facet
{
namespace
{

using namespace internal;

// Sidebars nested inside one another (a sidebar in a popup of a sidebar).
constexpr int kSidebarDepth = 4;
// Animation sub key of the column width (below kItemAnimSubBase).
constexpr int kSidebarWidthSub = 1;

struct SidebarState
{
    NavigationRoving roving;
    ImGuiID revealId = 0;  // the item an arrow key moved the focus to, scrolled into view when drawn
};

struct SidebarSectionState
{
    bool open = true;
    bool known = false;  // the open state was set from defaultOpen
};

struct SidebarEntry
{
    ImGuiID id = 0;
    ImGuiWindow* window = nullptr;  // the column's scroll area, or null when it is out of sight
    bool rail = false;
    float rowHeight = 0.0f;
    SelectionPill pill;
    int sectionDepth = 0;
};

struct SidebarStack
{
    SidebarEntry entries[kSidebarDepth];
    int depth = 0;
    int overflow = 0;
};

SidebarStack& Sidebars()
{
    static SidebarStack stack;
    return stack;
}

// The sidebar the next item belongs to: the innermost one, while its column is the current window.
SidebarEntry* CurrentSidebar()
{
    SidebarStack& stack = Sidebars();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    SidebarEntry& entry = stack.entries[stack.depth - 1];
    return entry.window != nullptr && entry.window == GImGui->CurrentWindow ? &entry : nullptr;
}

bool SidebarRailFor(SidebarMode mode)
{
    if (mode == SidebarMode::Rail)
        return true;
    if (mode == SidebarMode::Expanded)
        return false;
    return AvailableWidthClass() != WidthClass::Expanded;
}

// The tooltip of a row: the label in rail mode, else the whole text of a cut label.
void SidebarRowTooltip(const ItemState& row, const char* label, const char* end, bool rail, bool cut)
{
    if (!rail)
    {
        CutTextTooltip(row, label, end, cut);
        return;
    }
    if (end != label && BeginPopupTooltip(row.id, row.rect, row.hovered, row.focusVisible))
    {
        ImGui::TextUnformatted(label, end);
        EndPopupTooltip();
    }
}

} // namespace

bool BeginSidebar(const char* id, const SidebarOptions& options)
{
    if (options.mode == SidebarMode::Hidden)
        return false;
    ImGuiWindow* parent = ControlWindow();
    if (parent == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    (void)next;
    SidebarStack& stack = Sidebars();
    PushScope("BeginSidebar");
    if (stack.depth >= kSidebarDepth)
    {
        ++stack.overflow;
        return true;
    }
    const Theme& t = T();
    const ImGuiID sidebarId = parent->GetID(id);
    const bool rail = SidebarRailFor(options.mode);
    const float expanded = Px(options.width > 0.0f ? options.width : t.containers.sidebarWidth);
    const float railWidth = Px(t.containers.railWidth);
    const float width = std::round(Spring(sidebarId, kSidebarWidthSub, rail ? railWidth : expanded, t.motion.standard));
    const float rowHeight = std::round(Px(t.list.rowHeight));
    const float available = ImGui::GetContentRegionAvail().y;
    const float height = std::round(options.height > 0.0f ? Px(options.height) : std::max(rowHeight, available));

    // The surface behind the column.
    const ImVec2 origin(std::round(parent->DC.CursorPos.x), std::round(parent->DC.CursorPos.y));
    FillRect(parent->DrawList, origin, ImVec2(origin.x + width, origin.y + height), ToU32(t.colors.bgSecondary),
             Px(t.radii.card));

    SidebarEntry& entry = stack.entries[stack.depth++];
    entry.id = sidebarId;
    entry.window = nullptr;
    entry.rail = rail;
    entry.rowHeight = rowHeight;
    entry.sectionDepth = 0;
    ScrollAreaDesc desc;
    desc.size = ImVec2(width, height);
    desc.fadeSurface = t.colors.bgSecondary;
    desc.padding = Px(t.spacing.sm);
    if (!BeginScrollAreaCore(sidebarId, desc))
        return true;  // out of sight: the items submit nothing until EndSidebar
    ImGuiWindow* column = ImGui::GetCurrentWindow();
    entry.window = column;
    GetState<SidebarState>(sidebarId).roving.BeginFrame();
    entry.pill.Begin(sidebarId, column->DrawList, ImVec2(column->Pos.x - column->Scroll.x, column->Pos.y - column->Scroll.y));
    // Rows sit flush, one list row apart, as in a list; the pill and the hover fill keep to their row.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    return true;
}

void EndSidebar()
{
    if (!PopScope("BeginSidebar"))
        return;
    SidebarStack& stack = Sidebars();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    SidebarEntry& entry = stack.entries[--stack.depth];
    if (entry.window == nullptr)
        return;
    const bool here = GImGui->CurrentWindow == entry.window;
    const ImGuiItemFlags flags = GImGui->CurrentItemFlags;
    const Colors& c = T().colors;
    entry.pill.End(ToU32((flags & ImGuiItemFlags_Disabled) != 0 ? c.fill[1] : c.accentSubtle), Px(T().radii.control));
    entry.window = nullptr;
    if (!here)
        return;  // the caller ended the column inside the sidebar; Dear ImGui reports that itself
    ImGui::PopStyleVar();
    EndScrollAreaCore();
}

bool SidebarItem(const char* label, Icon icon, const SidebarItemOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const char* end = ImGui::FindRenderedTextEnd(label);
    SidebarEntry* entry = CurrentSidebar();
    if (entry == nullptr && Sidebars().depth > 0 && Sidebars().entries[Sidebars().depth - 1].window == nullptr)
        return false;  // the column is out of sight
    const Theme& t = T();
    const Colors& c = t.colors;
    const NextItemData next = TakeNextItemData();
    const bool rail = entry != nullptr && entry->rail;
    const float rowHeight = entry != nullptr ? entry->rowHeight : std::round(Px(t.list.rowHeight));
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const ImGuiID id = window->GetID(label);
    SidebarState* state = entry != nullptr ? &GetState<SidebarState>(entry->id) : nullptr;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    ItemOptions itemOptions;
    itemOptions.hitTarget = false;
    itemOptions.tabStop = state == nullptr || state->roving.IsTabStop(id);
    itemOptions.baseline = std::max(0.0f, (rowHeight - ImGui::GetFontSize()) * 0.5f);
    ApplyNextItemData(next);
    const ItemState row = ItemBehavior(id, ImVec2(width, rowHeight), itemOptions);
    if (state != nullptr)
    {
        state->roving.Add(id, !row.disabled, options.selected);
        if (row.focused && row.forced == ForcedItemState::None)
        {
            const ImGuiID moved = state->roving.Step(id, true);
            if (moved != 0)
                state->revealId = moved;
        }
        // The item that took the focus comes later in this frame (Down) or in the next one (Up);
        // either way it is scrolled into view when it is laid out, so a held key never walks
        // past the visible rows.
        if (state->revealId == id)
        {
            ScrollAreaRevealRect(window, row.rect, ScrollAlign::Nearest, false);
            state->revealId = 0;
        }
    }
    const float radius = Px(t.radii.control);
    if (options.selected && entry != nullptr)
        entry->pill.Target(row.rect);

    bool cut = false;
    if (row.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const float feedback = row.disabled ? 0.0f : std::max(row.hoverAmount, row.pressAmount);
        if (feedback > 0.0f)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], row.pressAmount);
            FillRect(drawList, row.rect.Min, row.rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), radius);
        }
        // A selected item outside a sidebar has no pill to show it: it takes the pill's still look.
        if (options.selected && entry == nullptr)
            FillRect(drawList, row.rect.Min, row.rect.Max, ToU32(disabled ? c.fill[1] : c.accentSubtle), radius);
        ControlTextScope text(TextStyle::Body);
        const float iconSize = text.IconSize();
        const ImU32 ink = ToU32(ControlLabelColor(row.disabled));
        const float pad = Px(t.spacing.sm);
        const float centerY = row.rect.GetCenter().y;
        char badge[16] = {};
        if (options.badge > 0)
            FormatNavigationCount(options.badge, badge, sizeof(badge));
        const NavigationBadgeMetrics badgeSize = options.badge > 0 ? MeasureNavigationBadge(badge) : NavigationBadgeMetrics{};
        if (rail)
        {
            if (icon.IsValid())
                DrawControlIcon(drawList, icon, std::round(row.rect.GetCenter().x - iconSize * 0.5f), centerY, iconSize, ink);
            if (options.badge > 0)
            {
                // On the corner of the square around the icon, so it never covers the icon's middle.
                const float x = std::min(row.rect.Max.x - badgeSize.width, std::round(row.rect.GetCenter().x + iconSize * 0.25f));
                DrawNavigationBadge(drawList, x, row.rect.Min.y + badgeSize.height * 0.5f, badge, nullptr, options.badgeTone,
                                    row.disabled);
            }
        }
        else
        {
            float x = row.rect.Min.x + pad;
            float right = row.rect.Max.x - pad;
            if (icon.IsValid())
            {
                DrawControlIcon(drawList, icon, x, centerY, iconSize, ink);
                x += iconSize + Px(t.spacing.sm);
            }
            if (options.badge > 0)
            {
                DrawNavigationBadge(drawList, right - badgeSize.width, centerY, badge, nullptr, options.badgeTone, row.disabled);
                right -= badgeSize.width + Px(t.spacing.sm);
            }
            PushPillLabelStyle(TextStyle::Body, options.selected);
            const FittedText shown = FitText(label, end, std::max(0.0f, right - x));
            cut = shown.truncated;
            drawList->AddText(ImVec2(x, CenteredTextY(row.rect.Min.y, rowHeight)), ink, shown.text);
            PopTextStyle();
        }
        DrawFocusRing(drawList, row.rect, radius, row.focusAmount);
    }
    SidebarRowTooltip(row, label, end, rail, cut);
    return row.pressed;
}

bool BeginSidebarSection(const char* label, const SidebarSectionOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    SidebarEntry* entry = CurrentSidebar();
    if (entry == nullptr && Sidebars().depth > 0 && Sidebars().entries[Sidebars().depth - 1].window == nullptr)
        return false;
    const Theme& t = T();
    const Colors& c = t.colors;
    const char* end = ImGui::FindRenderedTextEnd(label);
    const ImGuiID id = window->GetID(label);
    SidebarSectionState& section = GetState<SidebarSectionState>(id);
    if (!section.known)
    {
        section.open = options.defaultOpen;
        section.known = true;
    }
    const bool rail = entry != nullptr && entry->rail;
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float pad = Px(t.spacing.sm);
    bool open = !options.collapsible || section.open || rail;

    if (rail)
    {
        // A hairline between groups; the first section of the column needs none.
        const ImVec2 at = window->DC.CursorPos;
        const float band = Px(t.spacing.md);
        ImGui::Dummy(ImVec2(width, band));
        if (at.y > window->ContentRegionRect.Min.y + 0.5f)
            HairlineH(window->DrawList, at.x + pad, at.x + width - pad, std::round(at.y + band * 0.5f), ToU32(c.separator));
    }
    else if (!options.collapsible)
    {
        ControlTextScope text(TextStyle::Footnote);
        const float height = std::round(ImGui::GetFontSize() + Px(t.spacing.md));
        const ImVec2 at = window->DC.CursorPos;
        ImGui::Dummy(ImVec2(width, height));
        const FittedText shown = FitText(label, end, std::max(0.0f, width - pad * 2.0f));
        window->DrawList->AddText(ImVec2(at.x + pad, std::round(at.y + height - ImGui::GetFontSize() - Px(t.spacing.xs))),
                                  ToU32(ControlSecondaryColor((GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0)),
                                  shown.text);
        if (shown.truncated)
        {
            const ImRect rect(at, ImVec2(at.x + width, at.y + height));
            CutTextTooltip(ImGui::GetIDWithSeed("header", nullptr, id), rect,
                           ImGui::IsMouseHoveringRect(rect.Min, rect.Max) && GImGui->HoveredWindow == window, false, label,
                           end, true);
        }
    }
    else
    {
        // A collapsible header: a row in the roving order, with the chevron at the end.
        const float rowHeight = entry != nullptr ? entry->rowHeight : std::round(Px(t.list.rowHeight));
        SidebarState* state = entry != nullptr ? &GetState<SidebarState>(entry->id) : nullptr;
        ItemOptions itemOptions;
        itemOptions.hitTarget = false;
        itemOptions.tabStop = state == nullptr || state->roving.IsTabStop(id);
        const ItemState row = ItemBehavior(id, ImVec2(width, rowHeight), itemOptions);
        if (state != nullptr)
        {
            state->roving.Add(id, !row.disabled, false);
            if (row.focused && row.forced == ForcedItemState::None)
                (void)state->roving.Step(id, true);
        }
        if (row.pressed && row.forced == ForcedItemState::None)
            section.open = !section.open;
        // Left closes and Right opens the focused header, as in a tree.
        if (row.focused && row.forced == ForcedItemState::None)
        {
            ImGui::SetKeyOwner(ImGuiKey_LeftArrow, id);
            ImGui::SetKeyOwner(ImGuiKey_RightArrow, id);
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, 0, id))
                section.open = false;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, 0, id))
                section.open = true;
        }
        open = section.open;
        bool cut = false;
        if (row.visible)
        {
            ImDrawList* drawList = window->DrawList;
            const float radius = Px(t.radii.control);
            const float feedback = row.disabled ? 0.0f : std::max(row.hoverAmount, row.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], row.pressAmount);
                FillRect(drawList, row.rect.Min, row.rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), radius);
            }
            ControlTextScope text(TextStyle::Footnote);
            const float iconSize = text.IconSize();
            const ImU32 ink = ToU32(ControlSecondaryColor(row.disabled));
            const float right = row.rect.Max.x - pad - iconSize;
            DrawControlIcon(drawList, Icon(open ? "chevron-down" : "chevron-right"), right, row.rect.GetCenter().y, iconSize, ink);
            const FittedText shown = FitText(label, end, std::max(0.0f, right - pad - (row.rect.Min.x + pad)));
            cut = shown.truncated;
            drawList->AddText(ImVec2(row.rect.Min.x + pad, CenteredTextY(row.rect.Min.y, rowHeight)), ink, shown.text);
            DrawFocusRing(drawList, row.rect, radius, row.focusAmount);
        }
        CutTextTooltip(row, label, end, cut);
    }
    if (!open)
        return false;
    PushScope("BeginSidebarSection");
    ImGui::PushID(id);
    if (entry != nullptr)
        ++entry->sectionDepth;
    return true;
}

void EndSidebarSection()
{
    if (!PopScope("BeginSidebarSection"))
        return;
    ImGui::PopID();
    if (SidebarEntry* entry = CurrentSidebar())
        entry->sectionDepth = std::max(0, entry->sectionDepth - 1);
}

} // namespace facet
