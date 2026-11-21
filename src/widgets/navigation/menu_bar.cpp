// BeginMenuBar and EndMenuBar (navigation.h): a row of menu titles at one control height, with a
// hairline under it, the titles that do not fit in a "more" menu, one Tab stop with Left and Right
// inside, and Alt to reach it from the keyboard. The titles themselves are BeginMenu calls
// (menu.cpp); the shared stacks are in menu_parts.h.
//
// Alt: a press and release of Alt alone, while the bar's window has the keyboard focus and no
// popup is open, moves the focus to the bar's first enabled title, with the focus ring.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
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
#include "internal/state_storage.h"
#include "widgets/buttons/button_look.h"
#include "widgets/navigation/menu_parts.h"

namespace facet
{
namespace internal
{
namespace
{

// Bars nested in bars (a bar in a popup of a bar).
constexpr int kMenuBarDepth = 4;

struct MenuBarStack
{
    MenuBarFrame frames[kMenuBarDepth];
    int depth = 0;
    int overflow = 0;
};

MenuBarStack& Bars()
{
    static MenuBarStack stack;
    return stack;
}

struct MenuStack
{
    MenuFrame frames[kMenuStackDepth];
    int depth = 0;
    int overflow = 0;
};

MenuStack& Menus()
{
    static MenuStack stack;
    return stack;
}

PopupOptions MenuPopupOptions()
{
    PopupOptions popup;
    popup.minWidth = Px(T().overlay.menuMinWidth);
    return popup;
}

PopupAnchor MenuAnchor(const ImRect& rect)
{
    return PopupAnchor{rect, PopupSide::Below, PopupAlign::Start};
}

// Alt pressed and released alone moves the focus to the first enabled title.
void MenuBarAltKey(MenuBarFrame& bar, MenuBarState& state)
{
    const ImGuiContext& g = *GImGui;
    const ImGuiIO& io = g.IO;
    const bool altPressed = ImGui::IsKeyPressed(ImGuiKey_LeftAlt, false) || ImGui::IsKeyPressed(ImGuiKey_RightAlt, false);
    const bool altReleased = ImGui::IsKeyReleased(ImGuiKey_LeftAlt) || ImGui::IsKeyReleased(ImGuiKey_RightAlt);
    if (altPressed && !io.KeyCtrl && !io.KeyShift && !io.KeySuper)
    {
        state.altArmed = true;
        return;
    }
    if (!state.altArmed)
        return;
    // Any other key, typed text or a mouse button while Alt is held makes it a chord, not a tap.
    const bool other = (g.LastKeyboardKeyPressTime == g.Time && !altReleased) || io.InputQueueCharacters.Size > 0 ||
                       io.KeyCtrl || io.KeyShift || io.KeySuper || ImGui::IsAnyMouseDown();
    if (other)
    {
        state.altArmed = false;
        return;
    }
    if (!altReleased)
        return;
    state.altArmed = false;
    const bool windowFocused = g.NavWindow != nullptr && g.NavWindow->RootWindow == bar.window->RootWindow;
    if (!windowFocused || TopPopupLevel() != 0 || g.ActiveId != 0)
        return;
    // The first bar of the window takes the release; later bars of the same window leave it.
    static int claimedFrame = -1;
    static ImGuiID claimedWindow = 0;
    if (claimedFrame == ImGui::GetFrameCount() && claimedWindow == bar.window->RootWindow->ID)
        return;
    claimedFrame = ImGui::GetFrameCount();
    claimedWindow = bar.window->RootWindow->ID;
    for (int i = 0; i < state.roving.lastCount; ++i)
    {
        if (!state.roving.lastEnabled[i])
            continue;
        ImGui::SetFocusID(state.roving.last[i], bar.window);
        ImGui::SetNavCursorVisible(true);
        SetKeyboardModality(true);
        break;
    }
}

// The "more" button after the last visible title, and its menu of the titles that did not fit.
void MenuBarMore(MenuBarFrame& bar, MenuBarState& state)
{
    ImGuiWindow* window = bar.window;
    const ControlMetrics m = GetControlMetrics(ControlSize::Regular);
    const ImGuiID moreId = ImGui::GetIDWithSeed("##more", nullptr, bar.id);
    const ImGuiID menuId = ImGui::GetIDWithSeed("##moremenu", nullptr, bar.id);
    const float x = std::min(bar.x, bar.right - bar.moreWidth);
    const ImRect rect(std::round(x), bar.top, std::round(x) + bar.moreWidth, bar.top + bar.height);
    state.moreRect = rect;
    state.moreLevel = menuId;
    ItemOptions options;
    options.tabStop = state.roving.IsTabStop(moreId);
    const ItemState item = PartBehavior(moreId, rect, options);
    state.roving.Add(moreId, !item.disabled, false);
    NavigationRecordPart(bar.id, kNavigationPartMore, rect);
    if (item.focused && item.forced == ForcedItemState::None)
        (void)state.roving.Step(moreId, false);
    const bool open = IsPopupLevelOpen(menuId) || item.open;
    const bool keyOpen = item.focused && ImGui::IsKeyPressed(ImGuiKey_DownArrow, false);
    // With a menu of the bar open, resting the pointer on the button switches to its menu.
    const bool rawHover = GImGui->HoveredWindow == window && rect.Contains(GImGui->IO.MousePos);
    if (item.open)
        ForceMenuLevel(menuId, rect);
    else if (!open && !item.disabled && (item.pressed || keyOpen || (bar.open && rawHover)))
        OpenMenuLevel(menuId, rect);
    PlaceMenuLevel(menuId, rect);
    if (item.visible)
    {
        ItemState look = item;
        if (open)
            look.hoverAmount = 1.0f;  // the button stays lit while its menu is open
        const ControlTextScope text(m);
        const ButtonColors colors = ButtonLook(ButtonVariant::Plain, look);
        if (colors.fill.w > 0.0f)
            FillRect(window->DrawList, rect.Min, rect.Max, ToU32(colors.fill), m.radius);
        DrawControlIcon(window->DrawList, Icon("more-horizontal"), std::round(rect.GetCenter().x - text.IconSize() * 0.5f),
                        rect.GetCenter().y, text.IconSize(), ToU32(colors.content));
        DrawItemFocusRing(item, m.radius);
    }
    if (!open && BeginPopupTooltip(item.id, rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(GetStrings().more);
        EndPopupTooltip();
    }
    if (BeginPopupLevel(menuId))
    {
        MenuLevelBegin(menuId);
        for (int i = 0; i < bar.hiddenCount; ++i)
        {
            const int title = bar.hidden[i];
            ImGui::PushID(title);
            MenuRowDesc row;
            row.disabled = !bar.enabled[title];
            const MenuRowResult result = MenuRow(menuId, bar.hiddenLabels[i], row);
            NavigationRecordPart(bar.id, NavigationMenuRowPart(title), result.item.rect);
            if (result.pressed)
            {
                // The chain is closed already; the title's own call opens its menu in the next frame.
                state.pendingOpen = title;
                state.pendingFrame = ImGui::GetFrameCount();
            }
            ImGui::PopID();
        }
        EndPopupLevel();
    }
}

} // namespace

MenuBarFrame* CurrentMenuBar()
{
    MenuBarStack& stack = Bars();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    MenuBarFrame& frame = stack.frames[stack.depth - 1];
    return frame.active && frame.window == GImGui->CurrentWindow ? &frame : nullptr;
}

MenuBarState& MenuBarStateOf(ImGuiID barId)
{
    return GetState<MenuBarState>(barId);
}

void PushMenuFrame(const MenuFrame& frame)
{
    MenuStack& stack = Menus();
    if (stack.depth >= kMenuStackDepth)
    {
        ReportUsageError("Menus nested deeper than %d", kMenuStackDepth);
        ++stack.overflow;
        return;
    }
    stack.frames[stack.depth++] = frame;
}

MenuFrame* CurrentMenuFrame()
{
    MenuStack& stack = Menus();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    return &stack.frames[stack.depth - 1];
}

MenuFrame PopMenuFrame()
{
    MenuStack& stack = Menus();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return {};
    }
    if (stack.depth == 0)
        return {};
    return stack.frames[--stack.depth];
}

void OpenMenuLevel(ImGuiID level, const ImRect& anchor)
{
    OpenPopupLevel(level, MenuAnchor(anchor), MenuPopupOptions());
}

void PlaceMenuLevel(ImGuiID level, const ImRect& anchor)
{
    SetPopupLevelAnchor(level, MenuAnchor(anchor));
}

void ForceMenuLevel(ImGuiID level, const ImRect& anchor)
{
    ForcePopupLevelOpen(level, MenuAnchor(anchor), MenuPopupOptions());
}

} // namespace internal

bool BeginMenuBar(const char* id, const MenuBarOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const ControlMetrics m = GetControlMetrics(ControlSize::Regular);
    const float width = ResolveItemWidth(options.width, next, kFill);
    const ImGuiID barId = window->GetID(id);
    MenuBarState& state = MenuBarStateOf(barId);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect row(origin, ImVec2(origin.x + width, origin.y + m.height));

    bool open = false;
    for (int i = 0; i < state.count; ++i)
        open = open || IsPopupLevelOpen(state.titleLevels[i]);
    open = open || (state.moreLevel != 0 && IsPopupLevelOpen(state.moreLevel));
    // Out of sight with no menu open: nothing to submit (rule 8).
    if (!open && !ImGui::IsRectVisible(row.Min, row.Max))
    {
        // Still one item of the bar's size, so the item queries and scrolling to it work.
        ImGui::ItemSize(row.GetSize());
        (void)ImGui::ItemAdd(row, barId, nullptr, ImGuiItemFlags_NoNav);
        return false;
    }

    MenuBarStack& stack = Bars();
    PushScope("BeginMenuBar");
    if (stack.depth >= kMenuBarDepth)
    {
        ++stack.overflow;
        return true;
    }
    MenuBarFrame& bar = stack.frames[stack.depth++];
    bar.active = true;
    bar.window = window;
    bar.id = barId;
    bar.left = origin.x;
    bar.top = origin.y;
    bar.right = origin.x + width;
    bar.height = m.height;
    bar.gap = Px(T().spacing.xs);
    bar.moreWidth = m.height;
    bar.x = origin.x;
    bar.index = 0;
    bar.hiddenCount = 0;
    bar.open = open;
    const OverflowResult fit = FitOverflow(std::span<const OverflowItem>(state.items, static_cast<std::size_t>(state.count)),
                                           width, bar.gap, bar.moreWidth, Px(T().spacing.lg), &state.overflow,
                                           std::span<bool>(bar.decided));
    bar.showMore = fit.showButton;
    bar.decidedCount = state.count;
    state.roving.BeginFrame();
    MenuBarAltKey(bar, state);
    // A pending open whose title never came is dropped.
    if (state.pendingOpen >= 0 && state.pendingFrame < ImGui::GetFrameCount() - 1)
        state.pendingOpen = -1;

    ImGui::BeginGroup();
    ImGui::PushOverrideID(barId);
    HairlineH(window->DrawList, row.Min.x, row.Max.x, row.Max.y - HairlineThickness(), ToU32(T().colors.separator));
    // EndGroup grows a group to the last item: an empty bar must not take the size of what came before.
    GImGui->LastItemData.Rect = ImRect(origin, origin);
    return true;
}

void EndMenuBar()
{
    using namespace internal;
    if (!PopScope("BeginMenuBar"))
        return;
    MenuBarStack& stack = Bars();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    MenuBarFrame& bar = stack.frames[--stack.depth];
    if (!bar.active)
        return;
    bar.active = false;
    ImGuiWindow* window = bar.window;
    if (GImGui->CurrentWindow != window)
        return;  // the caller ended the window inside the bar; Dear ImGui reports that itself

    MenuBarState& state = MenuBarStateOf(bar.id);
    if (bar.hiddenCount > 0)
        MenuBarMore(bar, state);
    else
        state.moreLevel = 0;
    const int count = std::min(bar.index, kMenuBarMaxTitles);
    std::memcpy(state.items, bar.items, sizeof(OverflowItem) * static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        state.titleRects[i] = bar.rects[i];
        state.titleLevels[i] = bar.levels[i];
        state.titleEnabled[i] = bar.enabled[i];
    }
    state.count = count;

    ImGui::PopID();
    window->DC.CursorPos = ImVec2(bar.left, bar.top);
    ImGui::ItemSize(ImVec2(bar.right - bar.left, bar.height));
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, bar.right);
    ImGui::EndGroup();
}

} // namespace facet
