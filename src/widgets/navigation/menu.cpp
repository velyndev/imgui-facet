// BeginMenu and EndMenu (navigation.h): a menu title in a menu bar (or on its own in a window),
// or a submenu row inside another menu, each opening a popup level of the popup host (popup_host.h)
// whose rows are menu rows (popup_menu.h).
//
// A title is a plain button at one control height: with an icon and no bar the icon comes before
// the label; in a bar an icon replaces the label, which becomes the tooltip. A press, Enter, Space
// or Down opens its menu below it, and the menu takes the keyboard focus; Escape closes it and the
// focus goes back to the title. While a menu of the bar is open, the pointer moving onto another
// title (or the "more" button) opens that one instead. Inside a menu of a bar, Left and Right open
// the previous and next title's menu, unless Right opens the submenu of the focused row.
//
// A submenu row is MenuRow with a submenu: it opens after the pointer rests on it, on a click,
// Enter, Space or Right, and stays open while the pointer travels toward it (the aim test of
// PopupPointerAimsAt); Left inside it closes it.

#include "facet/widgets/navigation.h"

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
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/scope_stack.h"
#include "widgets/buttons/button_look.h"
#include "widgets/navigation/menu_parts.h"

namespace facet
{
namespace
{

using namespace internal;

struct TitleLook
{
    float width = 0.0f;
    bool iconOnly = false;
};

TitleLook MeasureTitle(const char* label, const char* end, Icon icon, bool inBar, const ControlMetrics& m,
                       const ControlTextScope& text)
{
    TitleLook look;
    look.iconOnly = inBar && icon.IsValid();
    if (look.iconOnly || end == label)
    {
        look.width = m.height;
        return look;
    }
    const float iconSlot = icon.IsValid() ? text.IconSize() + m.iconGap : 0.0f;
    look.width = std::ceil(m.paddingX * 2.0f + iconSlot + ImGui::CalcTextSize(label, end).x);
    return look;
}

// A title: its item, its look, and opening its menu. Returns the title's item state.
ItemState MenuTitle(ImGuiWindow* window, ImGuiID level, const char* label, const char* end, const MenuOptions& options,
                    MenuBarFrame* bar, MenuBarState* barState, int titleIndex, const NextItemData& next)
{
    const ControlMetrics m = GetControlMetrics(ControlSize::Regular);
    const ControlTextScope text(m);
    const TitleLook look = MeasureTitle(label, end, options.icon, bar != nullptr, m, text);
    const ImGuiID id = window->GetID(label);
    if (bar != nullptr)
        ImGui::SetCursorScreenPos(ImVec2(bar->x, bar->top));
    ItemOptions itemOptions;
    itemOptions.press = PressMode::Click;  // menus open on the press, as a pull-down
    itemOptions.baseline = CenteredTextY(window->DC.CursorPos.y, m.height) - window->DC.CursorPos.y;
    if (barState != nullptr)
        itemOptions.tabStop = barState->roving.IsTabStop(id);
    ApplyNextItemData(next);
    const ItemState item = ItemBehavior(id, ImVec2(look.width, m.height), itemOptions);
    if (barState != nullptr)
    {
        barState->roving.Add(id, !item.disabled, false);
        NavigationRecordPart(bar->id, titleIndex, item.rect);
        if (item.focused && item.forced == ForcedItemState::None)
            (void)barState->roving.Step(id, false);
    }
    const bool open = IsPopupLevelOpen(level) || item.open;
    const bool keyOpen = item.focused && ImGui::IsKeyPressed(ImGuiKey_DownArrow, false);
    const ImGuiContext& g = *GImGui;
    const bool rawHover = g.HoveredWindow == window && item.rect.Contains(g.IO.MousePos) &&
                          (g.IO.MouseDelta.x != 0.0f || g.IO.MouseDelta.y != 0.0f);
    bool pending = false;
    if (barState != nullptr && barState->pendingOpen == titleIndex)
    {
        pending = true;
        barState->pendingOpen = -1;
    }
    if (item.open)
        ForceMenuLevel(level, item.rect);
    else if (!item.disabled && !open && (item.pressed || keyOpen || pending || (bar != nullptr && bar->open && rawHover)))
        OpenMenuLevel(level, item.rect);
    PlaceMenuLevel(level, item.rect);
    if (item.visible)
    {
        ItemState shown = item;
        if (open)
            shown.hoverAmount = 1.0f;  // the title stays lit while its menu is open
        const ButtonColors colors = ButtonLook(ButtonVariant::Plain, shown);
        ImDrawList* drawList = window->DrawList;
        if (colors.fill.w > 0.0f)
            FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
        const ImU32 ink = ToU32(colors.content);
        const float centerY = item.rect.GetCenter().y;
        if (look.iconOnly)
        {
            DrawControlIcon(drawList, options.icon, std::round(item.rect.GetCenter().x - text.IconSize() * 0.5f), centerY,
                            text.IconSize(), ink);
        }
        else
        {
            float x = item.rect.Min.x + m.paddingX;
            if (options.icon.IsValid())
            {
                DrawControlIcon(drawList, options.icon, x, centerY, text.IconSize(), ink);
                x += text.IconSize() + m.iconGap;
            }
            const FittedText fitted = FitText(label, end, std::max(0.0f, item.rect.Max.x - m.paddingX - x));
            drawList->AddText(ImVec2(x, CenteredTextY(item.rect.Min.y, m.height)), ink, fitted.text);
        }
        DrawItemFocusRing(item, m.radius);
    }
    if (look.iconOnly && !open && BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(label, end);
        EndPopupTooltip();
    }
    if (bar != nullptr)
    {
        bar->x = item.rect.Max.x + bar->gap;
        ImGui::SetCursorScreenPos(ImVec2(bar->x, bar->top));
    }
    return item;
}

} // namespace

bool BeginMenu(const char* label, const MenuOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const char* end = ImGui::FindRenderedTextEnd(label);
    const NextItemData next = TakeNextItemData();
    const ImGuiID level = window->GetID(label);
    // A bar being submitted to this window makes it a title; inside a popup level it is a row.
    MenuBarFrame* bar = CurrentMenuBar();
    const ImGuiID parentLevel = CurrentPopupLevel();

    MenuFrame frame;
    frame.level = level;
    if (bar == nullptr && parentLevel != 0)
    {
        // A submenu row inside an open menu.
        MenuRowDesc row;
        row.icon = options.icon;
        row.submenu = level;
        row.disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
        const MenuRowResult result = MenuRow(parentLevel, label, row);
        if (result.item.focused)
            if (MenuFrame* parent = CurrentMenuFrame(); parent != nullptr && parent->level == parentLevel)
                parent->focusedSubmenuRow = true;
    }
    else if (bar != nullptr)
    {
        // A title of the bar: shown when the fitter keeps it, else listed in the "more" menu.
        MenuBarState& state = MenuBarStateOf(bar->id);
        const int index = bar->index++;
        IM_ASSERT_USER_ERROR(index < kMenuBarMaxTitles, "A menu bar shows at most 32 menus.");
        if (index >= kMenuBarMaxTitles)
            return false;
        const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
        const ControlMetrics m = GetControlMetrics(ControlSize::Regular);
        float width = 0.0f;
        {
            const ControlTextScope text(m);
            width = MeasureTitle(label, end, options.icon, true, m, text).width;
        }
        bar->items[index] = {width, 0, OverflowKind::Item};
        bar->levels[index] = level;
        bar->enabled[index] = !disabled;
        frame.bar = bar->id;
        frame.title = index;
        // The fitter's choice from the last frame, and in any case only while it fits (the row may
        // have changed since).
        const float limit = bar->showMore ? bar->right - bar->moreWidth - bar->gap : bar->right;
        const bool shows = (index >= bar->decidedCount || bar->decided[index]) && bar->hiddenCount == 0 &&
                           bar->x + width <= limit + 0.5f;
        if (shows)
        {
            const ItemState item = MenuTitle(window, level, label, end, options, bar, &state, index, next);
            bar->rects[index] = item.rect;
        }
        else
        {
            const int hidden = bar->hiddenCount++;
            bar->hidden[hidden] = index;
            const std::size_t length = std::min(static_cast<std::size_t>(end - label), static_cast<std::size_t>(kMenuBarLabelSize - 1));
            std::memcpy(bar->hiddenLabels[hidden], label, length);
            bar->hiddenLabels[hidden][length] = '\0';
            bar->rects[index] = state.moreRect;
            // A hidden menu opens in place of the "more" menu, from its row there or a key switch.
            if (state.pendingOpen == index && !disabled)
            {
                state.pendingOpen = -1;
                OpenMenuLevel(level, state.moreRect);
            }
            PlaceMenuLevel(level, state.moreRect);
        }
    }
    else
    {
        // A title on its own: a button that opens its menu below it.
        (void)MenuTitle(window, level, label, end, options, nullptr, nullptr, -1, next);
    }

    if (!BeginPopupLevel(level))
        return false;
    MenuLevelBegin(level);
    PushScope("BeginMenu");
    PushMenuFrame(frame);
    return true;
}

void EndMenu()
{
    using namespace internal;
    if (!PopScope("BeginMenu"))
        return;
    const MenuFrame frame = PopMenuFrame();
    // In a menu of a bar, Left and Right open the neighbouring title's menu.
    int switchTo = -1;
    // The menu opened by a switch is submitted later in the same frame and sees the same key press:
    // one switch per frame.
    if (frame.bar != 0 && ImGui::IsWindowFocused() && GImGui->ActiveId == 0 &&
        MenuBarStateOf(frame.bar).switchFrame != ImGui::GetFrameCount())
    {
        MenuBarState& state = MenuBarStateOf(frame.bar);
        const bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false);
        const bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) && !frame.focusedSubmenuRow;
        if (left != right && state.count > 1)
        {
            const int step = right ? 1 : -1;
            for (int n = 1; n < state.count && switchTo < 0; ++n)
            {
                const int candidate = ((frame.title + step * n) % state.count + state.count) % state.count;
                if (state.titleEnabled[candidate])
                    switchTo = candidate;
            }
        }
    }
    EndPopupLevel();
    if (switchTo >= 0)
    {
        MenuBarState& state = MenuBarStateOf(frame.bar);
        state.switchFrame = ImGui::GetFrameCount();
        // Opened at the same depth as the bar's titles, so it replaces the open menu.
        OpenMenuLevel(state.titleLevels[switchTo], state.titleRects[switchTo]);
    }
}

} // namespace facet
