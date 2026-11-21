// BeginCombo, ComboItem and EndCombo (lists.h): the field of a combo whose rows the caller submits.
//
// The field is the one of Dropdown (list_parts.h). The open list is a level of the popup host that
// takes the keyboard focus (the rows are the caller's items, so Dear ImGui's navigation moves between
// them), with a scroll area at most maxVisibleRows rows tall. ComboItem draws a row as the open
// lists of Dropdown do: the check slot, the icon, the label and the detail.

#include "facet/widgets/lists.h"

#include <algorithm>

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
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

constexpr int kComboDepth = 4;

struct ComboState
{
    int rowCount = 0;  // rows submitted while the list was last open, for its height
};

// An open combo between BeginCombo and EndCombo.
struct ComboFrame
{
    ImGuiID id = 0;
    ImGuiID level = 0;
    ControlSize size = ControlSize::Regular;
    bool opened = false;  // the list opened on this frame: the selected row takes the focus
    int rowCount = 0;
};

struct ComboStack
{
    ComboFrame frames[kComboDepth];
    int depth = 0;
};

ComboStack& Combos()
{
    static ComboStack stack;
    return stack;
}

} // namespace

namespace internal
{

void ListsComboEndFrame()
{
    Combos().depth = 0;
}

} // namespace internal

bool BeginCombo(const char* label, const char* preview, const ComboOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    const ImGuiID level = ListsLevelId(id);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool open = IsPopupLevelOpen(level);

    ListsFieldDesc field;
    field.size = options.size;
    field.width = options.width;
    field.next = &next;
    field.forced = &forced;
    field.icon = options.icon;
    field.text = preview != nullptr ? preview : GetStrings().select;
    field.secondaryText = preview == nullptr;
    field.open = open;
    field.readOnly = readOnly;
    field.error = options.error;
    field.helpText = options.helpText;
    const ListsFieldResult result = ListsField(id, label, field);
    const ItemState& item = result.item;

    const bool canOpen = !readOnly && !item.disabled && item.forced == ForcedItemState::None;
    bool openNow = canOpen && !open && result.openRequest;
    if (canOpen && !open && item.focused)
    {
        ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, id))
            openNow = true;
    }
    const PopupAnchor anchor = ListsFieldAnchor(result.frame);
    PopupOptions popup;
    popup.takeFocus = true;
    if (openNow)
        OpenPopupLevel(level, anchor, popup);
    if (item.open)
        ForcePopupLevelOpen(level, anchor, popup);
    SetPopupLevelAnchor(level, anchor);

    ComboStack& combos = Combos();
    if (combos.depth >= kComboDepth)
    {
        ReportUsageError("BeginCombo nested deeper than %d; the list is not opened", kComboDepth);
        return false;
    }
    if (!BeginPopupLevel(level))
        return false;

    // The rows scroll in an area at most maxVisibleRows rows tall, as tall as the rows submitted
    // while the list was last open.
    const Theme& theme = T();
    ComboState& state = GetState<ComboState>(id);
    const int maxRows = std::max(1, options.maxVisibleRows > 0 ? options.maxVisibleRows : theme.list.popupRows);
    const int rows = std::clamp(state.rowCount > 0 ? state.rowCount : maxRows, 1, maxRows);
    ScrollAreaDesc area;
    area.size = ImVec2(std::max(1.0f, result.frame.GetWidth() - Px(theme.spacing.xs) * 2.0f),
                       ListRowHeight(options.size) * static_cast<float>(rows) + FocusRingOutset() * 2.0f);
    area.fadeSurface = theme.colors.ElevatedSurface(2);
    if (!BeginScrollAreaCore("##facet.rows", area))
    {
        EndPopupLevel();
        return false;
    }
    ComboFrame& frame = combos.frames[combos.depth++];
    frame = ComboFrame();
    frame.id = id;
    frame.level = level;
    frame.size = ResolveControlSize(options.size);
    frame.opened = openNow || item.open;
    PushScope("BeginCombo");
    return true;
}

void EndCombo()
{
    using namespace internal;
    if (!PopScope("BeginCombo"))
        return;
    ComboStack& combos = Combos();
    if (combos.depth == 0)
        return;
    const ComboFrame frame = combos.frames[--combos.depth];
    GetState<ComboState>(frame.id).rowCount = frame.rowCount;
    EndScrollAreaCore();
    EndPopupLevel();
}

bool ComboItem(const char* label, bool selected, const ComboItemOptions& options)
{
    using namespace internal;
    ComboStack& combos = Combos();
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    if (combos.depth == 0)
    {
        ReportUsageError("ComboItem outside BeginCombo / EndCombo");
        return false;
    }
    if (label == nullptr)
        label = "";
    ComboFrame& frame = combos.frames[combos.depth - 1];
    ++frame.rowCount;
    ImGuiWindow* window = GImGui->CurrentWindow;
    const Theme& theme = T();
    const Colors& c = theme.colors;
    const float rowHeight = ListRowHeight(frame.size);
    const float width = std::max(1.0f, window->WorkRect.Max.x - window->DC.CursorPos.x);

    // Rows follow each other without the item spacing, as the rows of a list view do.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    ItemOptions itemOptions;
    itemOptions.hitTarget = false;
    PushTextStyle(ControlTextStyle(frame.size));
    itemOptions.baseline = std::max(0.0f, (rowHeight - ImGui::GetFontSize()) * 0.5f);
    PopTextStyle();
    const ItemState item = ItemBehavior(window->GetID(label), ImVec2(width, rowHeight), itemOptions);
    ImGui::PopStyleVar();

    if (selected && frame.opened)
    {
        ImGui::SetItemDefaultFocus();
        ScrollAreaRevealRect(window, item.rect, ScrollAlign::Center, false);
    }
    bool truncated = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const float radius = Px(theme.radii.control);
        ImRect fill = item.rect;
        fill.Min.y += HairlineThickness();
        fill.Max.y -= HairlineThickness();
        const float feedback = item.disabled || IsScrollAreaScrolling(window) ? 0.0f
                                                                              : std::max(item.hoverAmount, item.pressAmount);
        if (feedback > 0.0f)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], item.pressAmount);
            FillRect(drawList, fill.Min, fill.Max, ToU32(WithAlpha(tint, tint.w * feedback)), radius);
        }
        const float pad = Px(theme.spacing.sm);
        const ImRect content(item.rect.Min.x + pad, item.rect.Min.y, std::max(item.rect.Min.x + pad, item.rect.Max.x - pad),
                             item.rect.Max.y);
        ChoiceItem entry;
        entry.label = label;
        entry.icon = options.icon;
        entry.detail = options.detail;
        ListsRowLook look;
        look.checkSlot = true;
        look.selected = selected;
        truncated = DrawListsRowContent(content, item, entry, look, frame.size);
        DrawItemFocusRing(item, radius);
    }
    CutTextTooltip(item, label, ImGui::FindRenderedTextEnd(label), truncated);
    if (!item.pressed)
        return false;
    if (item.keyboardPressed)
        ListsCloseByKey(frame.level);
    else
        ClosePopupLevel(frame.level);
    return true;
}

} // namespace facet
