#pragma once

// Parts the widgets of the lists family share: one item source over the three item shapes of
// lists.h, the display rows with group headers and a fuzzy filter, the content of a row (check
// slot or checkbox, icon, label with match highlighting, detail), the field of the popup lists
// (Dropdown, MultiDropdown, BeginCombo) and the open list those fields show. Names carry the
// family ("Lists"), because every family links into one library.
//
//     const ListsSource source = ListsSourceOf(items);
//     ListsRows& rows = ...;                                 // kept in the widget's state
//     BuildListsRows(&rows, source, query, true);            // when the items or the filter change
//     ListsEntryContext entries{&source, &rows};
//     ListViewDesc desc{.rowCount = rows.Count(), .entry = &ListsEntry, .user = &entries};
//     ... ListViewRow(i) ... DrawListsRow(row, item, {.query = query}, size) ...

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"

namespace facet::internal
{

// ---- Item sources -----------------------------------------------------------------------------

// The items of a list widget in one of the three shapes of lists.h.
struct ListsSource
{
    std::span<const ChoiceItem> items;
    std::span<const char* const> labels;
    ChoiceGetter getter = nullptr;
    void* user = nullptr;
    int count = 0;

    // Fills `out` for item `index`. False for an index outside the items and for an entry the
    // getter skips. A null label becomes "". Strings from a getter stay valid until the next call.
    bool Get(int index, ChoiceItem* out) const;
};

ListsSource ListsSourceOf(std::span<const ChoiceItem> items);
ListsSource ListsSourceOf(std::span<const char* const> labels);
ListsSource ListsSourceOf(int count, ChoiceGetter getter, void* user);

// ---- Display rows ------------------------------------------------------------------------------

inline constexpr int kListsQueryCapacity = 128;
// The display row value of the select-all row of a multi-select list.
inline constexpr int kListsSelectAllRow = -0x7FFFFFFF;

// What a list shows, row by row: items, the group header before a run of items with equal group
// text, and an optional select-all row.
struct ListsRows
{
    ImVector<int> rows;              // an item index, -1 - index for the header before item `index`, or kListsSelectAllRow
    ImVector<FuzzyResult> matches;   // scratch of the filter, kept so a rebuild does not allocate again

    int Count() const { return rows.Size; }
    bool IsHeader(int row) const { return row >= 0 && row < rows.Size && rows[row] < 0 && rows[row] != kListsSelectAllRow; }
    bool IsSelectAll(int row) const { return row >= 0 && row < rows.Size && rows[row] == kListsSelectAllRow; }
    // The item a row shows (a header: the item it stands before); -1 for the select-all row.
    int ItemAt(int row) const;
    // The row of item `item`, -1 when it is not shown.
    int RowOf(int item) const;
};

struct ListsRowsDesc
{
    const char* query = nullptr;  // fuzzy filter on the visible label part; null or empty for all
    bool groups = true;           // group headers from ChoiceItem::group
    bool selectAll = false;       // a select-all row first (only when an item is shown)
    bool useExclude = false;      // leave out the items whose `exclude` entry equals excludeValue
    std::span<const bool> exclude = {};  // missing entries count as false
    bool excludeValue = true;
    std::span<const bool> first = {};    // items whose entry is true come before the others (checkedFirst)
};

// Builds the rows of `source`: every item that is not skipped, in item order, with a header before
// each run of equal group text. A query keeps the fuzzy matches; without groups they are ordered
// best first, with groups they keep the item order so each match stays under its header. The
// matched counts and scratch are reused, so a rebuild allocates only when the list grows.
void BuildListsRows(ListsRows* out, const ListsSource& source, const ListsRowsDesc& desc);

// The ListEntryFn (list_view.h) over display rows: an item row gives its visible label for
// typeahead, its disabled flag and its id; header rows are disabled without a label.
struct ListsEntryContext
{
    const ListsSource* source = nullptr;
    const ListsRows* rows = nullptr;
};
void ListsEntry(void* user, int row, ListEntryInfo* out);
// The ListEntryFn over the items themselves, one row per item (user: a ListsSource). A skipped
// getter entry is a disabled row without a label.
void ListsItemEntry(void* user, int index, ListEntryInfo* out);

// ---- Row content -------------------------------------------------------------------------------

struct ListsRowLook
{
    bool checkSlot = false;       // a leading slot showing the check glyph while `selected` (popup lists)
    bool checkbox = false;        // a leading checkbox showing `check`
    CheckState check = CheckState::Off;
    bool selected = false;        // the label in the semibold pill style (and the check glyph in the slot)
    bool readOnly = false;        // the checkbox shows the read-only look
    const char* query = nullptr;  // the characters the query matches are highlighted
};

// The side of a check slot, checkbox or row icon for rows of `size`, in pixels.
float ListsGlyphSize(ControlSize size);

// Draws the content of a row inside `content` (the row less its padding, screen pixels) of the
// current window: the check slot or checkbox, the icon, the label (cut with an ellipsis) and the
// detail at the end in labelSecondary. `item` is the row's item state (disabled, hover for the
// checkbox). Returns whether the label was cut; the row then shows the full label as a tooltip.
bool DrawListsRowContent(const ImRect& content, const ItemState& item, const ChoiceItem& entry,
                         const ListsRowLook& look, ControlSize size);
// The same for a row of a list view, followed by the tooltip of a cut label.
void DrawListsRow(const ListRowState& row, const ChoiceItem& entry, const ListsRowLook& look, ControlSize size);
// A group header row: the group text in Footnote and labelSecondary inside `content`.
void DrawListsHeader(const ImRect& content, const char* group);
// Checkboxes in rows are the selection family's mark (DrawSelectionCheckbox in
// widgets/selection/selection_parts.h), so a checked row and a Checkbox look the same.

// The select-all row of a multi-select list: the checkbox showing `state` and Strings::selectAll.
void DrawListsSelectAllRow(const ListRowState& row, CheckState state, bool readOnly, ControlSize size);
// What the select-all row shows: On when every shown enabled item is checked, Off when none is,
// Mixed otherwise (missing entries count as unchecked).
CheckState ListsSelectAllState(const ListsSource& source, const ListsRows& rows, std::span<const bool> checked);
// Checks (`value` true) or clears every shown enabled item that has an entry; returns whether one changed.
bool ListsApplySelectAll(const ListsSource& source, const ListsRows& rows, std::span<bool> checked, bool value);
// Checked entries among the items (entries beyond the items are not counted).
int ListsCheckedCount(std::span<const bool> checked, int itemCount);

// ---- The field of a popup list -----------------------------------------------------------------

struct ListsFieldDesc
{
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units, the options' width
    const NextItemData* next = nullptr;  // taken at the widget's entry
    const ForcedItemRequest* forced = nullptr;  // taken at the widget's entry
    Icon icon;                         // before the value text
    const char* text = "";             // the value text [text, textEnd)
    const char* textEnd = nullptr;     // null: zero terminated
    bool secondaryText = false;        // the text is a placeholder or Strings::mixed (labelSecondary)
    bool open = false;                 // the list is open: the active frame
    bool itemsError = false;           // the items failed to load: the error glyph before the chevron
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

struct ListsFieldResult
{
    ItemState item;  // the field item (the whole widget width)
    ImRect frame;    // the frame without the label in front, screen pixels
    // The field was activated in a way that opens the list: a click whose press did not start while
    // the list was open (that press closed it), Space or Enter. Not while read-only or forced.
    bool openRequest = false;
};

// The field: the label in front (rule 6), the frame (DrawFieldFrame), the icon, the value text cut
// with an ellipsis (tooltip on hover and focus), the error glyph, the chevron, the focus ring around
// the frame and the message line. One group; the field id `id` is the last item after the call.
ListsFieldResult ListsField(ImGuiID id, const char* label, const ListsFieldDesc& desc);

// The popup level of the field `fieldId`, and where it opens: below the frame, at least as wide.
ImGuiID ListsLevelId(ImGuiID fieldId);
PopupAnchor ListsFieldAnchor(const ImRect& frame);

// The keys a field gives its open list while the list does not have a text field of its own: the
// arrows, Page Up and Down, Home and End, Enter, Space and Tab, owned by `level` (call every frame
// the list is open), so navigation does not move the focus off the field.
void ListsClaimListKeys(ImGuiID level, bool tab);

// ---- Framed inline lists (ListBox, SelectableList, SearchableChecklist, TransferList) --------------

struct ListsBoxDesc
{
    ControlSize size = ControlSize::Inherit;  // the row height
    float width = 0.0f;                // units, the options' width; 0 fills
    const NextItemData* next = nullptr;
    int visibleRows = 0;               // 0 for list.inlineRows
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
    const char* trailing = nullptr;    // text at the end of the label line (a count); null for none
    float headerHeight = 0.0f;         // pixels between the label and the frame for content of the widget (a filter field)
};

struct ListsBoxLayout
{
    ImVec2 origin;        // top-left of the widget
    float width = 0.0f;   // pixels
    ImRect header;        // the room of headerHeight under the label, empty without one
    ImRect frame;         // the outlined frame the rows scroll in
    ImVec2 areaSize;      // for ListViewDesc::areaSize: the frame
    bool disabled = false;
};

// Opens the group of a framed list at the cursor: the label above it (rule 6: a list is taller than
// a label line), cut with an ellipsis, with `trailing` at the end of its line; then headerHeight of
// room; then the frame of visibleRows rows (no fill, the boundary hairline, destructive with
// `error`, fill[3] while disabled). The cursor is left at the frame's top-left corner for the list
// view. Close with EndListsBox.
ListsBoxLayout BeginListsBox(const char* label, const ListsBoxDesc& desc);
// The message line under the frame (help text or error, recorded under `id`), then the group, so
// item `id` covers the whole widget.
void EndListsBox(ImGuiID id, const ListsBoxLayout& layout, const ListsBoxDesc& desc);

// ---- The open list of Dropdown and MultiDropdown --------------------------------------------------

struct ListsPopupDesc
{
    ImGuiID level = 0;
    ListsSource source;
    ControlSize size = ControlSize::Inherit;
    int maxVisibleRows = 0;            // 0 for list.popupRows
    float width = 0.0f;                // pixels: the field frame width; the level is at least as wide
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;
    const char* errorText = nullptr;
    bool searchable = false;           // a filter field above the rows
    bool multi = false;                // rows show a checkbox from `checked`; activation toggles it
    std::span<bool> checked = {};
    bool selectAllRow = false;         // multi: a first row that checks or clears every shown enabled item
    int chosen = -1;                   // single: the chosen item (the check slot)
    bool readOnly = false;
    bool opened = false;               // the level opened on this frame: reveal the chosen row, read no keys
    bool forced = false;               // a capture shows the level: read no input
};

struct ListsPopupResult
{
    int activated = -1;     // single: the item chosen this frame, -1 for none
    bool byKey = false;     // the activation came from a key (Enter, Space, Tab)
    bool changed = false;   // multi: an entry of `checked` was written this frame
    bool close = false;     // the list asks to close (Escape in the filter field)
};

// The content of the open level (call between BeginPopupLevel and EndPopupLevel). State under the
// level id: the rows, the active row, the filter text and the typeahead text.
ListsPopupResult ListsPopupList(const ListsPopupDesc& desc);

// The filter text of the open list of `level`, or "" (for tests and captures).
const char* ListsPopupQuery(ImGuiID level);

// ---- Shared item helpers -----------------------------------------------------------------------

// Whether entry `index` of a span of flags is true; false outside it.
inline bool ListsFlag(std::span<const bool> flags, int index)
{
    return index >= 0 && index < static_cast<int>(flags.size()) && flags[static_cast<std::size_t>(index)];
}

// Closes `level` after a key closed it (Escape in a filter field, Enter on a row): the focus goes
// back to the field with the navigation cursor shown, so Enter and Space act on it at once, as
// after an Escape the popup host handles itself.
void ListsCloseByKey(ImGuiID level);

// The frames of the source and target lists of TransferList `id` (the widget id: ImGui::GetID of its
// id string) in its last call, screen pixels; false before the first call. For demo selftests,
// which click rows wherever the lists are laid out (side by side or stacked).
bool ListsTransferFrames(ImGuiID id, ImRect* source, ImRect* target);

// The frame hooks of the family (lists_frame.cpp): reset the stacks of open Begin / End pairs.
void ListsComboEndFrame();

// Marks the last item as edited this frame (ImGui::IsItemEdited), for a widget that wrote its value.
void ListsMarkEdited();

} // namespace facet::internal
