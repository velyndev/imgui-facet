// TransferList (lists.h): two lists with move buttons between them.
//
// Each side is a framed list of the family (list_parts.h, BeginListsBox) with an optional filter
// field and a multiple selection of its own (a list view of the list core); the rows of both sides
// are drag sources and each side is a drop zone of the drag and drop core, so rows move by the
// buttons, by double click or Enter, and by dragging them across. The buttons are facet::Button.

#include "facet/widgets/lists.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

constexpr const char* kTransferRowType = "facet.transfer.row";

struct TransferRowPayload
{
    ImGuiID list = 0;
    int side = 0;   // 0 source, 1 target
    int item = -1;
};

struct TransferSide
{
    internal::ListsRows rows;
    ImGuiSelectionBasicStorage selection;
    char query[internal::kListsQueryCapacity] = {};
};

struct TransferState
{
    TransferSide sides[2];
    ImRect frames[2];  // the frames of the two lists in the last call (ListsTransferFrames)
};

// Selection keys are item indices plus one (the rows of a side change as items move).
ImGuiID TransferKey(int item)
{
    return static_cast<ImGuiID>(item) + 1u;
}

ImGuiID TransferStorageId(ImGuiSelectionBasicStorage* storage, int row)
{
    const internal::ListsRows* rows = static_cast<const internal::ListsRows*>(storage->UserData);
    const int item = rows != nullptr && row >= 0 && row < rows->Count() ? rows->rows[row] : -1;
    return item >= 0 ? TransferKey(item) : 0u;
}

// Moves every selected item of `side` (or only `item` when it is not selected) to the other side.
bool MoveSelected(TransferSide& side, int sideIndex, std::span<bool> inTarget, const internal::ListsSource& source,
                  int item)
{
    const bool toTarget = sideIndex == 0;
    const bool onlyOne = item >= 0 && !side.selection.Contains(TransferKey(item));
    bool changed = false;
    for (int i = 0; i < source.count; ++i)
    {
        const bool chosen = onlyOne ? i == item : side.selection.Contains(TransferKey(i));
        if (!chosen || i >= static_cast<int>(inTarget.size()))
            continue;
        ChoiceItem entry;
        if (!source.Get(i, &entry) || entry.disabled || inTarget[static_cast<std::size_t>(i)] == toTarget)
            continue;
        inTarget[static_cast<std::size_t>(i)] = toTarget;
        side.selection.SetItemSelected(TransferKey(i), false);
        changed = true;
    }
    return changed;
}

struct SideDesc
{
    const char* title = nullptr;
    int index = 0;
    float width = 0.0f;  // pixels
};

// One side: the title, the filter, the framed rows. Returns the item activated (double click,
// Enter) or dropped here from the other side through *dropped, and whether a drag delivered.
struct SideResult
{
    int activated = -1;
    TransferRowPayload dropped;
    bool delivered = false;
    ImRect frame;
};

SideResult DrawSide(ImGuiID listId, TransferSide& side, const SideDesc& sideDesc, std::span<bool> inTarget,
                    const internal::ListsSource& source, const TransferListOptions& options, bool readOnly)
{
    using namespace internal;
    SideResult result;
    ImGuiWindow* window = GImGui->CurrentWindow;
    const Theme& theme = T();
    ImGui::PushID(sideDesc.index);
    const ImGuiID sideId = ImGui::GetID("##side");

    ListsBoxDesc box;
    box.width = sideDesc.width / Px(1.0f);
    box.visibleRows = options.visibleRows;
    box.readOnly = readOnly;
    box.headerHeight = options.searchable ? ControlHeight(ControlSize::Inherit) + Px(theme.spacing.sm) : 0.0f;
    const ListsBoxLayout layout = BeginListsBox(sideDesc.title != nullptr ? sideDesc.title : "##title", box);
    result.frame = layout.frame;

    if (options.searchable)
    {
        const ImVec2 frameStart = window->DC.CursorPos;
        window->DC.CursorPos = layout.header.Min;
        TextFieldParams params;
        params.placeholder = GetStrings().search;
        params.icon = Icon("search");
        params.clearButton = true;
        params.width = sideDesc.width / Px(1.0f);
        TextFieldBehavior("##filter", side.query, sizeof(side.query), params);
        window->DC.CursorPos = frameStart;
    }

    // The rows of this side: items whose inTarget entry says so, filtered. Rebuilt every frame, so
    // they follow the caller's flags at once (TransferList takes spans only).
    ListsRowsDesc rowsDesc;
    rowsDesc.query = side.query;
    rowsDesc.groups = false;
    rowsDesc.useExclude = true;
    rowsDesc.exclude = inTarget;
    rowsDesc.excludeValue = sideDesc.index == 0;
    BuildListsRows(&side.rows, source, rowsDesc);
    side.selection.UserData = &side.rows;
    side.selection.AdapterIndexToStorageId = &TransferStorageId;

    const ListsEntryContext entries{&source, &side.rows};
    ListViewDesc view;
    view.rowCount = side.rows.Count();
    view.areaSize = layout.areaSize;
    view.selection = &side.selection;
    // A drag from a row carries it to the other list. With box selection Dear ImGui would turn a
    // drag that starts on an unselected row into a box selection instead.
    view.boxSelect = false;
    view.readOnly = readOnly;
    view.entry = &ListsEntry;
    view.user = const_cast<ListsEntryContext*>(&entries);
    view.state = options.state;
    view.emptyText = options.emptyText != nullptr ? options.emptyText
                                                  : (side.query[0] != '\0' ? GetStrings().noResults : GetStrings().empty);
    view.errorText = options.errorText;
    if (BeginListView(sideId, view))
    {
        RowRange range;
        while (NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                const ListRowState row = ListViewRow(i);
                const int item = side.rows.rows[i];
                ChoiceItem entry;
                source.Get(item, &entry);
                ListsRowLook look;
                look.selected = row.selected;
                look.query = side.query;
                DrawListsRow(row, entry, look, ControlSize::Inherit);
                if (!readOnly && !row.disabled)
                {
                    const TransferRowPayload payload{listId, sideDesc.index, item};
                    DragDropPreview preview;
                    preview.previewText = entry.label;
                    preview.icon = entry.icon;
                    preview.count = side.selection.Contains(TransferKey(item)) ? side.selection.Size : 1;
                    if (DragDropBeginSource(kTransferRowType, &payload, sizeof(payload), preview))
                        DragDropEndSource();
                }
            }
        }
        const ListViewResult list = EndListView();
        if (list.activated >= 0 && list.activated < side.rows.Count())
            result.activated = side.rows.rows[list.activated];
    }

    // The frame takes rows dragged from the other side of this list.
    DragDropZoneOptions zone;
    zone.accept = !readOnly && options.state == DataState::Ready;
    const DragDropZoneResult drop = DragDropZone(ImHashStr("##zone", 0, sideId), layout.frame, kTransferRowType, zone);
    const ImGuiPayload* payload = drop.payload;
    if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(TransferRowPayload)))
    {
        const TransferRowPayload* row = static_cast<const TransferRowPayload*>(payload->Data);
        const bool fromOther = row->list == listId && row->side != sideDesc.index;
        if (fromOther && drop.hovered && !drop.delivered)
            DragDropDrawTarget(window->DrawList, layout.frame, Px(theme.radii.control));
        if (fromOther && drop.delivered)
        {
            result.delivered = true;
            result.dropped = *row;
        }
    }
    EndListsBox(sideId, layout, box);
    ImGui::PopID();
    return result;
}

// The width of the move buttons: the wider of the two at the regular size, so both line up.
float TransferButtonWidth()
{
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    internal::ControlTextScope text(m);
    const Strings& strings = GetStrings();
    const float label = std::max(ImGui::CalcTextSize(strings.add, nullptr, true).x,
                                 ImGui::CalcTextSize(strings.remove, nullptr, true).x);
    return std::ceil(m.paddingX * 2.0f + text.IconSize() + m.iconGap + label);
}

bool TransferListImpl(const char* id, std::span<bool> inTarget, const internal::ListsSource& source,
                      const TransferListOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const Theme& theme = T();
    const float width = ResolveItemWidth(options.width, next, kFill);
    const bool stacked = WidthClassOf(width / Px(1.0f)) == WidthClass::Compact;
    const float gap = Px(theme.spacing.md);
    const float buttonWidth = TransferButtonWidth();
    const float sideWidth = stacked ? width : std::max(1.0f, (width - buttonWidth - gap * 2.0f) * 0.5f);

    const ImGuiID listId = window->GetID(id);
    TransferState& state = GetState<TransferState>(listId);
    ImGui::PushID(id);
    ImGui::BeginGroup();
    const ImVec2 start = window->DC.CursorPos;

    SideResult results[2];
    results[0] = DrawSide(listId, state.sides[0], {options.sourceTitle, 0, sideWidth}, inTarget, source, options,
                          readOnly);
    const ImRect sourceRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

    // The move buttons: between the lists, or in a row under the source list when stacked.
    const bool ready = options.state == DataState::Ready;
    bool add = false;
    bool remove = false;
    {
        const Strings& strings = GetStrings();
        if (stacked)
            ImGui::SetCursorScreenPos(ImVec2(start.x, sourceRect.Max.y + gap));
        else
            ImGui::SetCursorScreenPos(ImVec2(start.x + sideWidth + gap,
                                             sourceRect.GetCenter().y - ControlHeight(ControlSize::Inherit) - gap * 0.5f));
        const float column = ImGui::GetCursorScreenPos().x;
        ImGui::BeginDisabled(readOnly || !ready || state.sides[0].selection.Size == 0);
        add = facet::Button(strings.add, {.icon = Icon(stacked ? "arrow-down" : "arrow-right"),
                                          .width = buttonWidth / Px(1.0f)});
        ImGui::EndDisabled();
        if (stacked)
            ImGui::SameLine(0.0f, gap);
        else
            ImGui::SetCursorScreenPos(ImVec2(column, ImGui::GetCursorScreenPos().y + gap * 0.5f));
        ImGui::BeginDisabled(readOnly || !ready || state.sides[1].selection.Size == 0);
        remove = facet::Button(strings.remove, {.icon = Icon(stacked ? "arrow-up" : "arrow-left"),
                                                .width = buttonWidth / Px(1.0f)});
        ImGui::EndDisabled();
    }
    const float buttonsBottom = ImGui::GetItemRectMax().y;

    if (stacked)
        ImGui::SetCursorScreenPos(ImVec2(start.x, buttonsBottom + gap));
    else
        ImGui::SetCursorScreenPos(ImVec2(start.x + sideWidth + buttonWidth + gap * 2.0f, start.y));
    results[1] = DrawSide(listId, state.sides[1], {options.targetTitle, 1, sideWidth}, inTarget, source, options,
                          readOnly);
    const ImRect targetRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    window->DC.CursorPos = ImVec2(start.x, std::max(sourceRect.Max.y, targetRect.Max.y));
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::EndGroup();
    ImGui::PopID();
    FinishWidgetGroup(listId);
    state.frames[0] = results[0].frame;
    state.frames[1] = results[1].frame;

    if (readOnly || !ready)
        return false;
    bool changed = false;
    if (add)
        changed |= MoveSelected(state.sides[0], 0, inTarget, source, -1);
    if (remove)
        changed |= MoveSelected(state.sides[1], 1, inTarget, source, -1);
    for (int s = 0; s < 2; ++s)
    {
        if (results[s].activated >= 0)
            changed |= MoveSelected(state.sides[s], s, inTarget, source, results[s].activated);
        if (results[s].delivered)
        {
            const int from = results[s].dropped.side;
            changed |= MoveSelected(state.sides[from], from, inTarget, source, results[s].dropped.item);
        }
    }
    if (changed)
        ListsMarkEdited();
    return changed;
}

} // namespace

namespace internal
{

bool ListsTransferFrames(ImGuiID id, ImRect* source, ImRect* target)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const TransferState& state = GetState<TransferState>(id);
    *source = state.frames[0];
    *target = state.frames[1];
    return source->GetWidth() > 0.0f && target->GetWidth() > 0.0f;
}

} // namespace internal

bool TransferList(const char* id, std::span<bool> inTarget, std::span<const ChoiceItem> items,
                  const TransferListOptions& options)
{
    return TransferListImpl(id, inTarget, internal::ListsSourceOf(items), options);
}

bool TransferList(const char* id, std::span<bool> inTarget, std::span<const char* const> items,
                  const TransferListOptions& options)
{
    return TransferListImpl(id, inTarget, internal::ListsSourceOf(items), options);
}

} // namespace facet
