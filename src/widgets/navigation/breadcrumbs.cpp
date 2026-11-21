// Breadcrumbs (navigation.h): a path trail of plain segments with chevrons between them. Every
// segment but the last is activatable; the last is the current place, drawn in label and not a
// link. Segments that do not fit collapse, from the second one on, into a "more" button in their
// place whose menu lists them; the first and the last segment stay longest, and the last one cuts
// its label when even it does not fit. One Tab stop for the trail, Left and Right, Home and End
// move the focus between the segments and the "more" button, Enter or Space activates.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/overflow.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/state_storage.h"
#include "widgets/navigation/navigation_parts.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

using namespace internal;

constexpr int kCrumbMax = kOverflowInlineItems;

struct CrumbState
{
    OverflowState overflow;
    NavigationRoving roving;
};

struct CrumbScratch
{
    OverflowItem fit[kCrumbMax];
    bool visible[kCrumbMax] = {};
    float natural[kCrumbMax] = {};
};

CrumbScratch& Scratch()
{
    static CrumbScratch scratch;  // one trail is laid out at a time; no allocation per frame
    return scratch;
}

int BreadcrumbsImpl(const char* id, const SelectionItems& items, const BreadcrumbsOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return -1;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID trailId = window->GetID(id);
    CrumbState& state = GetState<CrumbState>(trailId);
    IM_ASSERT_USER_ERROR(items.Count() <= kCrumbMax, "Breadcrumbs show at most 64 segments.");
    const int count = std::min(items.Count(), kCrumbMax);
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    ControlTextScope text(m);
    const float iconSize = text.IconSize();
    // A segment is plain text with a small padding for its hover fill; the chevron slot between two.
    const float pad = std::round(m.paddingX * 0.5f);
    const float chevron = iconSize + Px(t.spacing.xs) * 2.0f;
    const float moreWidth = m.height;
    CrumbScratch& s = Scratch();
    for (int i = 0; i < count; ++i)
    {
        const ChoiceItem item = items.At(i);
        float w = pad * 2.0f + ImGui::CalcTextSize(item.label, ImGui::FindRenderedTextEnd(item.label)).x;
        if (item.icon.IsValid())
            w += iconSize + m.iconGap;
        s.natural[i] = std::ceil(w);
    }
    float total = 0.0f;
    for (int i = 0; i < count; ++i)
        total += s.natural[i] + (i > 0 ? chevron : 0.0f);
    const float width = ResolveItemWidth(options.width, next, kFill);
    for (int i = 0; i < count; ++i)
        s.visible[i] = true;
    bool showMore = false;
    float lastWidth = count > 0 ? s.natural[count - 1] : 0.0f;
    if (total > width + 0.5f && count > 1)
    {
        for (int i = 0; i < count; ++i)
        {
            // The last segment stays longest, then the first; among the middle ones the later stay.
            const int priority = i == count - 1 ? count + 1 : (i == 0 ? count : i);
            s.fit[i] = {s.natural[i], priority, OverflowKind::Item};
        }
        lastWidth = std::min(lastWidth, std::max(1.0f, std::floor(width - moreWidth - chevron)));
        s.fit[count - 1].width = lastWidth;
        const OverflowResult fit = FitOverflow(std::span<const OverflowItem>(s.fit, static_cast<std::size_t>(count)), width,
                                               chevron, moreWidth, Px(t.spacing.lg), &state.overflow,
                                               std::span<bool>(s.visible, static_cast<std::size_t>(count)));
        showMore = fit.showButton;
        s.visible[count - 1] = true;
    }
    else
    {
        state.overflow.hidden = 0;
        if (count > 0)
            lastWidth = std::min(lastWidth, width);
    }

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, m.height), CenteredTextY(origin.y, m.height) - origin.y);
    ImDrawList* drawList = window->DrawList;
    state.roving.BeginFrame();
    const ImGuiID moreId = ImGui::GetIDWithSeed("more", nullptr, trailId);
    const ImGuiID menuId = ImGui::GetIDWithSeed("more menu", nullptr, trailId);
    int activated = -1;
    ImGuiID editor = trailId;
    float x = origin.x;
    bool moreDone = false;
    const float right = origin.x + width;
    const auto drawChevron = [&](float at) {
        DrawControlIcon(drawList, Icon("chevron-right"), std::round(at + (chevron - iconSize) * 0.5f), origin.y + m.height * 0.5f,
                        iconSize, ToU32(ControlSecondaryColor(groupDisabled)));
    };
    for (int i = 0; i < count; ++i)
    {
        const ChoiceItem item = items.At(i);
        const bool last = i == count - 1;
        if (!s.visible[i])
        {
            if (moreDone || !showMore)
                continue;
            // The "more" button stands where the first hidden segment would be.
            moreDone = true;
            if (i > 0)
            {
                drawChevron(x);
                x += chevron;
            }
            const ImRect rect(x, origin.y, x + moreWidth, origin.y + m.height);
            ItemOptions moreOptions;
            moreOptions.part = count;
            moreOptions.forced = &forced;
            moreOptions.disabled = groupDisabled;
            moreOptions.tabStop = state.roving.IsTabStop(moreId);
            const ItemState more = PartBehavior(moreId, rect, moreOptions);
            state.roving.Add(moreId, !more.disabled, false);
            NavigationRecordPart(trailId, kNavigationPartMore, rect);
            if (more.focused && more.forced == ForcedItemState::None)
                (void)state.roving.Step(moreId, false);
            const PopupAnchor anchor{rect, PopupSide::Below, PopupAlign::Start};
            PopupOptions popup;
            popup.minWidth = Px(t.overlay.menuMinWidth);
            const bool open = IsPopupLevelOpen(menuId) || more.open;
            if (more.open)
                ForcePopupLevelOpen(menuId, anchor, popup);
            else if (more.pressed && more.forced == ForcedItemState::None && !open)
                OpenPopupLevel(menuId, anchor, popup);
            SetPopupLevelAnchor(menuId, anchor);
            if (more.visible)
            {
                const float feedback = more.disabled ? 0.0f : std::max({more.hoverAmount, more.pressAmount, open ? 1.0f : 0.0f});
                if (feedback > 0.0f)
                {
                    const ImVec4 tint = Mix(c.fill[2], c.fill[1], more.pressAmount);
                    FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), m.radius);
                }
                DrawControlIcon(drawList, Icon("more-horizontal"), std::round(rect.GetCenter().x - iconSize * 0.5f),
                                rect.GetCenter().y, iconSize, ToU32(ControlSecondaryColor(more.disabled)));
                DrawFocusRing(drawList, rect, m.radius, more.focusAmount);
            }
            if (!open && BeginPopupTooltip(moreId, rect, more.hovered, more.focusVisible))
            {
                ImGui::TextUnformatted(GetStrings().more);
                EndPopupTooltip();
            }
            if (BeginPopupLevel(menuId))
            {
                MenuLevelBegin(menuId);
                for (int h = 0; h < count; ++h)
                {
                    if (s.visible[h])
                        continue;
                    const ChoiceItem hidden = items.At(h);
                    ImGui::PushID(h);
                    MenuRowDesc row;
                    row.icon = hidden.icon;
                    row.disabled = hidden.disabled || groupDisabled;
                    const MenuRowResult result = MenuRow(menuId, hidden.label, row);
                    NavigationRecordPart(trailId, NavigationMenuRowPart(h), result.item.rect);
                    if (result.pressed)
                        activated = h;
                    ImGui::PopID();
                }
                EndPopupLevel();
            }
            x = rect.Max.x;
            continue;
        }
        if (i > 0)
        {
            drawChevron(x);
            x += chevron;
        }
        const float w = last ? std::min(lastWidth, std::max(1.0f, right - x)) : s.natural[i];
        const ImRect rect(x, origin.y, std::round(x + w), origin.y + m.height);
        x = rect.Max.x;
        const char* labelEnd = ImGui::FindRenderedTextEnd(item.label);
        const ImGuiID crumbId = SelectionEntryId(trailId, item.id, i);
        ItemState crumb;
        bool link = !last;
        if (link)
        {
            ItemOptions crumbOptions;
            crumbOptions.part = i;
            crumbOptions.forced = &forced;
            crumbOptions.disabled = item.disabled || groupDisabled;
            crumbOptions.tabStop = state.roving.IsTabStop(crumbId);
            crumb = PartBehavior(crumbId, rect, crumbOptions);
            state.roving.Add(crumbId, !crumb.disabled, false);
            if (crumb.focused && crumb.forced == ForcedItemState::None)
                (void)state.roving.Step(crumbId, false);
            if (crumb.pressed && crumb.forced == ForcedItemState::None)
            {
                activated = i;
                editor = crumbId;
            }
        }
        else
        {
            crumb.rect = rect;
            crumb.visible = ImGui::IsRectVisible(rect.Min, rect.Max);
            crumb.disabled = groupDisabled;
            crumb.id = crumbId;
            crumb.hovered = ImGui::IsMouseHoveringRect(rect.Min, rect.Max) && GImGui->HoveredWindow == window;
        }
        NavigationRecordPart(trailId, i, rect);
        bool cut = false;
        if (crumb.visible)
        {
            const float feedback = crumb.disabled || !link ? 0.0f : std::max(crumb.hoverAmount, crumb.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], crumb.pressAmount);
                FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), m.radius);
            }
            // Ancestors in labelSecondary (label while hovered), the current place in label.
            const ImU32 ink = ToU32(crumb.disabled ? c.labelTertiary
                                                   : (link && feedback <= 0.0f ? c.labelSecondary : c.label));
            float cx = rect.Min.x + pad;
            if (item.icon.IsValid())
            {
                DrawControlIcon(drawList, item.icon, cx, rect.GetCenter().y, iconSize, ink);
                cx += iconSize + m.iconGap;
            }
            const FittedText shown = FitText(item.label, labelEnd, std::max(0.0f, rect.Max.x - pad - cx));
            cut = shown.truncated;
            drawList->AddText(ImVec2(cx, CenteredTextY(rect.Min.y, m.height)), ink, shown.text);
            if (link)
                DrawFocusRing(drawList, rect, m.radius, crumb.focusAmount);
        }
        if (link)
            CutTextTooltip(crumb, item.label, labelEnd, cut);
        else
            CutTextTooltip(crumbId, rect, crumb.hovered, false, item.label, labelEnd, cut);
    }
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, origin.y + m.height));
    ImGui::EndGroup();
    FinishWidgetGroup(trailId);
    (void)editor;
    return groupDisabled ? -1 : activated;
}

} // namespace

int Breadcrumbs(const char* id, std::span<const ChoiceItem> path, const BreadcrumbsOptions& options)
{
    return BreadcrumbsImpl(id, {path, {}}, options);
}

int Breadcrumbs(const char* id, std::span<const char* const> path, const BreadcrumbsOptions& options)
{
    return BreadcrumbsImpl(id, {{}, path}, options);
}

} // namespace facet
