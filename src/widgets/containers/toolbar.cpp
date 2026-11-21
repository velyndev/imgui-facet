// BeginToolbar, ToolbarButton, ToolbarSeparator, ToolbarSpacer and EndToolbar (containers.h): a
// row of plain tool buttons at one control height.
//
// Overflow: the buttons and separators of the last frame (widths, priorities, kinds) are kept
// under the toolbar id. BeginToolbar runs the overflow fitter (src/internal/overflow.h) over them
// and this frame's width, so each button knows at its call whether it shows. A button that does
// not show is listed, in toolbar order, in the "more" menu that EndToolbar puts at the trailing
// end; its menu row (src/internal/popup_menu.h) hands the activation to the button's next call,
// which returns true then. The fitter hides the lowest priority first and, among equal
// priorities, the button furthest to the end; a separator shows only between two visible items.
//
// Other widgets submitted between the Begin and End sit on the row after the previous item and
// never overflow; their width of the last frame is taken off the room the buttons have. The items
// after the last ToolbarSpacer sit at the trailing end, placed with their width of the last frame.

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
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/overflow.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/buttons/button_look.h"

namespace facet
{
namespace
{

// Buttons and separators one toolbar fits; later ones show while they fit and never move into
// the menu.
constexpr int kToolbarMaxItems = 48;
static_assert(kToolbarMaxItems <= internal::kOverflowInlineItems);
// Toolbars nested in toolbars (a toolbar in a popup opened from a toolbar).
constexpr int kToolbarDepth = 4;
// The visible label of a hidden button is copied for the overflow menu, because EndToolbar runs
// after the caller's string may be gone; a longer label is cut there.
constexpr int kToolbarLabelSize = 96;

constexpr Icon kToolbarMoreIcon("more-horizontal");

// What a toolbar remembers from its last frame, under its id.
struct ToolbarState
{
    internal::OverflowItem items[kToolbarMaxItems] = {};
    int count = 0;
    internal::OverflowState overflow;
    float foreignWidth = 0.0f;     // other widgets on the row, gaps included, pixels
    int spacerIndex = -1;          // the first item after the last spacer; -1 without a spacer
    float trailingForeign = 0.0f;  // other widgets after the last spacer, pixels
    ImGuiID pending = 0;        // a button activated from the menu; its next call returns true
    int pendingFrame = -1;
};

struct ToolbarHidden
{
    ImGuiID id = 0;
    Icon icon;
    bool selected = false;
    bool disabled = false;
    char label[kToolbarLabelSize] = {};
};

struct ToolbarEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    ControlSize size = ControlSize::Regular;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float height = 0.0f;
    float gap = 0.0f;
    float moreWidth = 0.0f;
    bool showMore = false;                // the fitter hides something
    bool decided[kToolbarMaxItems] = {};  // the visibility the fitter chose, per item
    int decidedCount = 0;
    int index = 0;                        // the next button or separator
    float expectedX = 0.0f;               // where the next item starts when nothing else came between
    float foreignWidth = 0.0f;
    bool knowsTrailing = false;           // the last frame had a spacer: trailingWidth is known
    float trailingWidth = 0.0f;           // the items after the spacer as the fitter shows them, pixels
    int spacerIndex = -1;                 // this frame's last spacer
    float foreignAtSpacer = 0.0f;
    float lastRight = 0.0f;              // right edge of the last item on the row
    bool lastShownIsSeparator = false;
    internal::OverflowItem items[kToolbarMaxItems] = {};  // this frame's measurements
    ToolbarHidden hidden[kToolbarMaxItems];
    int hiddenCount = 0;
};

struct ToolbarStack
{
    ToolbarEntry entries[kToolbarDepth];
    int depth = 0;
    int overflow = 0;
};

ToolbarStack& Toolbars()
{
    static ToolbarStack stack;
    return stack;
}

// The toolbar the next button belongs to: the innermost one, while it is submitted to the
// current window.
ToolbarEntry* CurrentToolbar()
{
    ToolbarStack& stack = Toolbars();
    if (stack.overflow > 0 || stack.depth == 0)
        return nullptr;
    ToolbarEntry& entry = stack.entries[stack.depth - 1];
    return entry.active && entry.window == GImGui->CurrentWindow ? &entry : nullptr;
}

// The x where the next toolbar item starts. A widget of the caller submitted since the last
// toolbar item moved the cursor to a new line: the row continues after it, and its width is
// noted for the next frame's fit.
float ToolbarNextX(ToolbarEntry& entry)
{
    ImGuiWindow* window = entry.window;
    float x = window->DC.CursorPos.x;
    if (window->DC.CursorPos.y > entry.top + 0.5f)
        x = window->DC.CursorPosPrevLine.x + entry.gap;
    if (x > entry.expectedX + 0.5f)
    {
        entry.foreignWidth += x - entry.expectedX;
        entry.lastRight = std::max(entry.lastRight, x - entry.gap);
        entry.lastShownIsSeparator = false;
    }
    return x;
}

// Ends a toolbar item: the next item, the caller's or a toolbar's, continues the row.
void ToolbarContinue(ToolbarEntry& entry, float right)
{
    entry.lastRight = right;
    entry.expectedX = right + entry.gap;
    ImGui::SameLine(0.0f, entry.gap);
}

// The right edge of the room for buttons: up to the more button when it shows.
float ToolbarLimit(const ToolbarEntry& entry)
{
    return entry.showMore ? entry.right - entry.moreWidth - entry.gap : entry.right;
}

// Whether item `index` shows: the fitter's decision when it knows the item, and in any case only
// while it fits (the row may have changed since the last frame).
bool ToolbarItemShows(const ToolbarEntry& entry, int index, float x, float width)
{
    if (index < entry.decidedCount && !entry.decided[index])
        return false;
    return x + width <= ToolbarLimit(entry) + 0.5f;
}

struct ToolButtonMeasure
{
    float width = 0.0f;
    bool showIcon = false;
    bool showLabel = false;
    float iconSize = 0.0f;
};

ToolButtonMeasure MeasureToolButton(const char* label, const char* labelEnd, Icon icon, bool iconOnly,
                                    const internal::ControlMetrics& m, const internal::ControlTextScope& text)
{
    ToolButtonMeasure look;
    look.iconSize = text.IconSize();
    look.showIcon = icon.IsValid();
    look.showLabel = !iconOnly || !look.showIcon;
    if (!look.showLabel)
    {
        look.width = m.height;  // square
        return look;
    }
    const float labelWidth = labelEnd != label ? ImGui::CalcTextSize(label, labelEnd).x : 0.0f;
    look.width = std::ceil(m.paddingX * 2.0f + (look.showIcon ? look.iconSize + m.iconGap : 0.0f) + labelWidth);
    return look;
}

// The fill and content color of a tool button: the plain button look, or while `selected` the
// accent's subtle fill under accent text, so an on button reads apart from its neighbours.
internal::ButtonColors ToolButtonColors(const internal::ItemState& item, bool selected)
{
    internal::ButtonColors colors = internal::ButtonLook(ButtonVariant::Plain, item);
    if (selected && !item.disabled)
    {
        const Colors& c = T().colors;
        colors.fill = internal::StateFill(c.accentSubtle, c.fill[1], c.fill[0], item);
        colors.content = c.accentText;
    }
    return colors;
}

// Draws one tool button and returns whether its label was cut.
bool DrawToolButton(ImDrawList* drawList, const internal::ItemState& item, const ToolButtonMeasure& look, Icon icon,
                    const char* label, const char* labelEnd, bool selected, const internal::ControlMetrics& m)
{
    const internal::ButtonColors colors = ToolButtonColors(item, selected);
    if (colors.fill.w > 0.0f)
        FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
    const ImU32 content = ToU32(colors.content);
    const ImRect& r = item.rect;
    const float centerY = r.GetCenter().y;
    bool cut = false;
    if (!look.showLabel)
    {
        internal::DrawControlIcon(drawList, icon, std::round(r.GetCenter().x - look.iconSize * 0.5f), centerY,
                                  look.iconSize, content);
    }
    else
    {
        float x = r.Min.x + m.paddingX;
        const float right = r.Max.x - m.paddingX;
        if (look.showIcon)
        {
            internal::DrawControlIcon(drawList, icon, x, centerY, look.iconSize, content);
            x += look.iconSize + m.iconGap;
        }
        const internal::FittedText shown = internal::FitText(label, labelEnd, std::max(0.0f, right - x));
        cut = shown.truncated;
        drawList->AddText(ImVec2(x, CenteredTextY(r.Min.y, r.GetHeight())), content, shown.text);
    }
    internal::DrawItemFocusRing(item, m.radius);
    return cut;
}

// The tooltip of a tool button: the label of an icon-only button, the whole label of a cut one.
void ToolButtonTooltip(const internal::ItemState& item, const char* label, const char* labelEnd, bool iconOnlyShown,
                       bool cut)
{
    if (!iconOnlyShown)
    {
        internal::CutTextTooltip(item, label, labelEnd, cut);
        return;
    }
    if (labelEnd == label)
        return;
    if (internal::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(label, labelEnd);
        internal::EndPopupTooltip();
    }
}

// The more button at the trailing end and its menu of the buttons that did not fit.
void ToolbarMoreMenu(ToolbarEntry& entry, ToolbarState& state)
{
    ImGuiWindow* window = entry.window;
    const internal::ControlMetrics m = internal::GetControlMetrics(entry.size);
    const ImGuiID moreId = window->GetID("##more");
    const ImGuiID menuId = window->GetID("##moremenu");
    ImGui::SetCursorScreenPos(ImVec2(std::max(entry.left, entry.right - entry.moreWidth), entry.top));
    const internal::ItemState item = internal::ItemBehavior(moreId, ImVec2(entry.moreWidth, m.height));
    const bool open = internal::IsPopupLevelOpen(menuId) || item.open;
    if (item.visible)
    {
        internal::ItemState look = item;
        if (open)
            look.hoverAmount = 1.0f;  // the button stays lit while its menu is open
        const internal::ControlTextScope text(m);
        const internal::ButtonColors colors = internal::ButtonLook(ButtonVariant::Plain, look);
        if (colors.fill.w > 0.0f)
            FillRect(window->DrawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
        internal::DrawControlIcon(window->DrawList, kToolbarMoreIcon,
                                  std::round(item.rect.GetCenter().x - text.IconSize() * 0.5f), item.rect.GetCenter().y,
                                  text.IconSize(), ToU32(colors.content));
        internal::DrawItemFocusRing(item, m.radius);
    }
    if (!open && internal::BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
    {
        ImGui::TextUnformatted(GetStrings().more);
        internal::EndPopupTooltip();
    }
    entry.lastRight = std::max(entry.lastRight, item.rect.Max.x);

    internal::PopupAnchor anchor;
    anchor.rect = item.rect;
    anchor.side = internal::PopupSide::Below;
    anchor.align = internal::PopupAlign::End;
    internal::PopupOptions popup;
    popup.minWidth = Px(T().overlay.menuMinWidth);
    if (item.open)
        internal::ForcePopupLevelOpen(menuId, anchor, popup);
    else if (item.pressed)
        internal::OpenPopupLevel(menuId, anchor, popup);
    if (internal::BeginPopupLevel(menuId))
    {
        internal::MenuLevelBegin(menuId);
        for (int i = 0; i < entry.hiddenCount; ++i)
        {
            const ToolbarHidden& hidden = entry.hidden[i];
            ImGui::PushID(static_cast<int>(hidden.id));
            internal::MenuRowDesc row;
            row.icon = hidden.icon;
            row.checked = hidden.selected;
            row.disabled = hidden.disabled;
            if (internal::MenuRow(menuId, hidden.label, row).pressed)
            {
                state.pending = hidden.id;
                state.pendingFrame = ImGui::GetFrameCount();
            }
            ImGui::PopID();
        }
        internal::EndPopupLevel();
    }
}

} // namespace

void BeginToolbar(const char* id, const ToolbarOptions& options)
{
    internal::PushScope("BeginToolbar");
    ToolbarStack& stack = Toolbars();
    if (stack.depth >= kToolbarDepth)
    {
        ++stack.overflow;
        return;
    }
    ToolbarEntry& entry = stack.entries[stack.depth++];
    entry.active = false;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const float width = internal::ResolveItemWidth(options.width, next, kFill);

    entry.active = true;
    entry.window = window;
    entry.id = window->GetID(id);
    entry.size = m.size;
    entry.left = window->DC.CursorPos.x;
    entry.top = window->DC.CursorPos.y;
    entry.right = entry.left + width;
    entry.height = m.height;
    entry.gap = Px(T().spacing.xs);
    entry.moreWidth = m.height;
    entry.index = 0;
    entry.expectedX = entry.left;
    entry.foreignWidth = 0.0f;
    entry.spacerIndex = -1;
    entry.foreignAtSpacer = 0.0f;
    entry.lastRight = entry.left;
    entry.lastShownIsSeparator = false;
    entry.hiddenCount = 0;

    ToolbarState& state = internal::GetState<ToolbarState>(entry.id);
    const float available = std::max(0.0f, width - state.foreignWidth);
    const internal::OverflowResult fit = internal::FitOverflow(
        std::span<const internal::OverflowItem>(state.items, static_cast<std::size_t>(state.count)), available,
        entry.gap, entry.moreWidth, Px(T().spacing.lg), &state.overflow, std::span<bool>(entry.decided));
    entry.decidedCount = state.count;
    entry.showMore = fit.showButton;
    // The width of what follows the spacer, as the fitter shows it, so the spacer can place it.
    entry.knowsTrailing = state.spacerIndex >= 0;
    entry.trailingWidth = state.trailingForeign;
    bool any = state.trailingForeign > 0.0f;
    for (int i = std::max(0, state.spacerIndex); entry.knowsTrailing && i < state.count; ++i)
    {
        if (!entry.decided[i])
            continue;
        entry.trailingWidth += (any ? entry.gap : 0.0f) + std::max(0.0f, state.items[i].width);
        any = true;
    }

    ImGui::BeginGroup();
    ImGui::PushOverrideID(entry.id);
    // EndGroup grows a group to the last item: an empty toolbar must not take the size of what came before.
    GImGui->LastItemData.Rect = ImRect(window->DC.CursorPos, window->DC.CursorPos);
}

void EndToolbar()
{
    if (!internal::PopScope("BeginToolbar"))
        return;
    ToolbarStack& stack = Toolbars();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    ToolbarEntry& entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    entry.active = false;
    ImGuiWindow* window = entry.window;
    if (GImGui->CurrentWindow != window)
        return;  // the caller ended the window inside the toolbar; Dear ImGui reports that itself

    (void)ToolbarNextX(entry);  // widgets after the last toolbar item
    ToolbarState& state = internal::GetState<ToolbarState>(entry.id);
    if (entry.hiddenCount > 0)
        ToolbarMoreMenu(entry, state);
    // A menu activation is read by the button's next call; one that no call took is dropped.
    if (state.pending != 0 && state.pendingFrame != ImGui::GetFrameCount())
        state.pending = 0;

    const int count = std::min(entry.index, kToolbarMaxItems);
    std::memcpy(state.items, entry.items, sizeof(internal::OverflowItem) * static_cast<std::size_t>(count));
    state.count = count;
    state.foreignWidth = entry.foreignWidth;
    state.spacerIndex = entry.spacerIndex;
    state.trailingForeign = entry.spacerIndex >= 0 ? entry.foreignWidth - entry.foreignAtSpacer : 0.0f;

    ImGui::PopID();
    // The group covers the whole row, so after EndToolbar the toolbar is one item of its full width.
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, entry.right);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, entry.top + entry.height);
    ImGui::EndGroup();
}

bool ToolbarButton(const char* label, Icon icon, const ToolbarButtonOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    IM_ASSERT_USER_ERROR(labelEnd != label, "ToolbarButton needs a visible label: it is the tooltip and the menu name");
    const internal::NextItemData next = internal::TakeNextItemData();
    ToolbarEntry* entry = CurrentToolbar();
    const internal::ControlMetrics m = internal::GetControlMetrics(entry != nullptr ? entry->size : ControlSize::Inherit);
    const internal::ControlTextScope text(m);
    const ToolButtonMeasure look = MeasureToolButton(label, labelEnd, icon, options.iconOnly, m, text);
    const ImGuiID id = window->GetID(label);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    // An activation from the overflow menu in the last frame.
    bool activated = false;
    if (entry != nullptr)
    {
        ToolbarState& state = internal::GetState<ToolbarState>(entry->id);
        if (state.pending == id)
        {
            state.pending = 0;
            activated = !disabled;
        }
        const float x = ToolbarNextX(*entry);
        const int index = entry->index++;
        if (index < kToolbarMaxItems)
            entry->items[index] = {look.width, options.priority, internal::OverflowKind::Item};
        if (!ToolbarItemShows(*entry, index, x, look.width))
        {
            if (entry->hiddenCount < kToolbarMaxItems)
            {
                ToolbarHidden& hidden = entry->hidden[entry->hiddenCount++];
                hidden.id = id;
                hidden.icon = icon;
                hidden.selected = options.selected;
                hidden.disabled = disabled;
                const std::size_t length = std::min(static_cast<std::size_t>(labelEnd - label),
                                                    static_cast<std::size_t>(kToolbarLabelSize - 1));
                std::memcpy(hidden.label, label, length);
                hidden.label[length] = '\0';
            }
            return activated;
        }
        ImGui::SetCursorScreenPos(ImVec2(x, entry->top));
        entry->lastShownIsSeparator = false;
    }

    const float top = window->DC.CursorPos.y;
    internal::ItemOptions itemOptions;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(look.width, m.height), itemOptions);
    bool cut = false;
    if (item.visible)
        cut = DrawToolButton(window->DrawList, item, look, icon, label, labelEnd, options.selected, m);
    ToolButtonTooltip(item, label, labelEnd, !look.showLabel, cut);
    if (entry != nullptr)
        ToolbarContinue(*entry, item.rect.Max.x);
    return activated || item.pressed;
}

void ToolbarSeparator()
{
    ToolbarEntry* entry = CurrentToolbar();
    if (entry == nullptr || internal::ControlWindow() == nullptr)
        return;
    const float x = ToolbarNextX(*entry);
    const int index = entry->index++;
    const float slot = Px(T().spacing.sm);
    if (index < kToolbarMaxItems)
        entry->items[index] = {slot, 0, internal::OverflowKind::Separator};
    // The fitter decides for the row it knows (never at either end, never two in a row); a
    // separator it does not know yet shows only after a visible item.
    bool shows = index < entry->decidedCount ? entry->decided[index]
                                             : entry->lastRight > entry->left && !entry->lastShownIsSeparator;
    shows = shows && x + slot <= ToolbarLimit(*entry);
    if (!shows)
        return;
    ImGuiWindow* window = entry->window;
    ImGui::SetCursorScreenPos(ImVec2(x, entry->top));
    const ImRect bb(ImVec2(x, entry->top), ImVec2(x + slot, entry->top + entry->height));
    ImGui::ItemSize(bb.GetSize());
    if (ImGui::ItemAdd(bb, 0))
    {
        // A hairline over the middle of the control height: it divides the groups without
        // reaching the row's edges.
        const float inset = Px(T().spacing.xs);
        const float cx = std::floor(bb.GetCenter().x);
        FillRect(window->DrawList, ImVec2(cx, bb.Min.y + inset), ImVec2(cx + HairlineThickness(), bb.Max.y - inset),
                 ToU32(T().colors.separator), 0.0f);
    }
    entry->lastShownIsSeparator = true;
    ToolbarContinue(*entry, bb.Max.x);
}

void ToolbarSpacer()
{
    ToolbarEntry* entry = CurrentToolbar();
    if (entry == nullptr || internal::ControlWindow() == nullptr)
        return;
    const float x = ToolbarNextX(*entry);
    // The items after the spacer end at the trailing edge (before the more button when it shows),
    // with their widths of the last frame. In the first frame with a spacer they follow inline.
    const float trailingX =
        entry->knowsTrailing ? std::max(x, std::floor(ToolbarLimit(*entry) - entry->trailingWidth)) : x;
    ImGui::SetCursorScreenPos(ImVec2(trailingX, entry->top));
    entry->expectedX = trailingX;
    entry->spacerIndex = entry->index;
    entry->foreignAtSpacer = entry->foreignWidth;
    entry->lastShownIsSeparator = true;  // an unknown separator right after the spacer stays hidden
}

} // namespace facet
