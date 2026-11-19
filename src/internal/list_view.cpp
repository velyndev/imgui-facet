// List views and trees (list_view.h) on the row ranges and selection of the list core, the scroll
// area and the item behaviour, focus ring and selection pill blocks.
//
// Rows are placed explicitly: row i of a view starts at the rows' top plus i row heights,
// computed in double precision with the window's cursor lossyness exactly as ImGuiListClipper
// seeks, so rows stay one pixel exact at the end of a million-row list and whatever a caller drew
// inside a row cannot shift the next one. Rows of a tree follow each other the same way.
//
// A row is the item behaviour's item with Dear ImGui's multi-select hooks around it:
// MultiSelectItemHeader before (it picks the press mode, so a press on a selected row can start a
// drag of the selection) and MultiSelectItemFooter after the button behaviour, as stock
// selectable items do. The header runs before ItemAdd here: it reads only the selection data set
// by SetNextItemSelectionUserData and the scope state, which ItemAdd does not change.

#include "internal/list_view.h"

#include <algorithm>
#include <cstring>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/list_internal.h"
#include "internal/loading.h"
#include "internal/scroll.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

constexpr int kTypedCapacity = 64;

// ---- Typeahead ------------------------------------------------------------------------------------

struct TypeaheadState
{
    char text[kTypedCapacity] = {};
    int length = 0;
    double time = -1.0e9;
};

// Adds this frame's typed characters. A pause of list.typeaheadTimeout starts a new text; a space
// counts only inside a text, so Space alone still activates the focused row. Returns true when the
// text changed.
bool UpdateTypeahead(TypeaheadState& typed)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.InputQueueCharacters.Size == 0 || io.KeyCtrl || io.KeyAlt || io.KeySuper)
        return false;
    const double now = ImGui::GetTime();
    if (now - typed.time > static_cast<double>(T().list.typeaheadTimeout))
        typed.length = 0;
    bool changed = false;
    for (const ImWchar c : io.InputQueueCharacters)
    {
        if (c < 0x20 || c == 0x7F || (c == ' ' && typed.length == 0))
            continue;
        char utf8[5] = {};
        const int bytes = ImTextCharToUtf8(utf8, c);
        if (typed.length + bytes >= kTypedCapacity)
            break;
        std::memcpy(typed.text + typed.length, utf8, static_cast<size_t>(bytes));
        typed.length += bytes;
        changed = true;
    }
    typed.text[typed.length] = '\0';
    if (changed)
        typed.time = now;
    return changed;
}

char FoldAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Whether the visible part of `label` (up to "##") starts with the first `length` bytes of
// `prefix`, ASCII case-insensitive.
bool StartsWith(const char* label, const char* prefix, int length)
{
    if (label == nullptr)
        return false;
    for (int i = 0; i < length; ++i)
    {
        const char c = label[i];
        if (c == '\0' || (c == '#' && label[i + 1] == '#'))
            return false;
        if (FoldAscii(c) != FoldAscii(prefix[i]))
            return false;
    }
    return true;
}

// The part of the typed text to search for: one code point when the text is a single code point
// or one repeated (cycling), else the whole text. `cycle` tells the search to start after the
// current entry.
int SearchLength(const char* typed, bool* cycle)
{
    const int length = static_cast<int>(std::strlen(typed));
    unsigned int c = 0;
    const int first = ImTextCharFromUtf8(&c, typed, typed + length);
    bool repeated = first > 0;
    for (int i = first; repeated && i < length; i += first)
        repeated = i + first <= length && std::memcmp(typed, typed + i, static_cast<size_t>(first)) == 0;
    *cycle = repeated;
    return repeated ? first : length;
}

// ---- Rows -----------------------------------------------------------------------------------------

// What a row needs from the view or tree it belongs to.
struct RowEnv
{
    ImGuiWindow* window = nullptr;
    bool multi = false;         // a multi-select scope of this list is open
    bool listFocused = false;
    SelectionPill* pill = nullptr;  // the chosen row targets the pill instead of a fill
    const ForcedItemRequest* forced = nullptr;
    ItemState* ring = nullptr;      // the focused row, for the ring drawn at the end
    bool* hasRing = nullptr;
};

ListRowState RunRow(const RowEnv& env, ImGuiID id, const ImRect& rect, ImGuiSelectionUserData data, bool selected,
                    bool disabled, bool tabStop, int part, float indent)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = env.window;
    const Theme& theme = T();
    window->DC.CursorPos = rect.Min;

    const bool inMulti = env.multi && !disabled && g.CurrentMultiSelect != nullptr;
    ImGuiButtonFlags buttonFlags = 0;
    if (inMulti)
    {
        ImGui::SetNextItemSelectionUserData(data);
        ImGui::MultiSelectItemHeader(id, &selected, &buttonFlags);
    }
    ItemOptions options;
    options.press = (buttonFlags & ImGuiButtonFlags_PressedOnClick) != 0 ? PressMode::Click : PressMode::ClickRelease;
    options.buttonFlags = buttonFlags & ~(ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_PressedOnClickRelease);
    options.disabled = disabled;
    options.navigable = !disabled;
    options.tabStop = tabStop;
    options.hitTarget = false;
    options.baseline = std::max(0.0f, (rect.GetHeight() - g.FontSize) * 0.5f);
    options.part = part;
    options.forced = env.forced;
    ItemState item = ItemBehavior(id, rect.GetSize(), options);
    if (inMulti && item.visible)
        ImGui::MultiSelectItemFooter(id, &selected, &item.pressed);

    ListRowState row;
    row.item = item;
    row.rect = rect;
    row.selected = selected;
    row.disabled = item.disabled;
    row.focused = item.focused;
    const bool doubleClicked = item.hovered && !item.disabled && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const bool enter = item.pressed && item.keyboardPressed && g.NavActivateId == id &&
                       (g.NavActivateFlags & ImGuiActivateFlags_PreferInput) != 0;
    row.activated = doubleClicked || enter;

    const float pad = Px(theme.spacing.sm);
    row.content = ImRect(rect.Min.x + indent + pad, rect.Min.y, std::max(rect.Min.x + indent + pad, rect.Max.x - pad),
                         rect.Max.y);

    if (selected && env.pill != nullptr)
        env.pill->Target(rect);
    if (item.visible)
    {
        const Colors& c = theme.colors;
        ImDrawList* drawList = window->DrawList;
        const float radius = Px(theme.radii.control);
        ImRect fill = rect;
        const float gap = HairlineThickness();
        fill.Min.y += gap;
        fill.Max.y -= gap;
        if (selected && env.pill == nullptr)
            FillRect(drawList, fill.Min, fill.Max, ToU32(env.listFocused ? c.accentSubtle : c.fill[1]), radius);
        const bool quiet = IsScrollAreaScrolling(window);
        const float feedback = item.disabled ? 0.0f : std::max(quiet ? 0.0f : item.hoverAmount, item.pressAmount);
        if (!selected && feedback > 0.0f)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], item.pressAmount);
            FillRect(drawList, fill.Min, fill.Max, ToU32(WithAlpha(tint, tint.w * feedback)), radius);
        }
    }
    if (item.focusAmount > 0.0f && env.ring != nullptr)
    {
        *env.ring = item;
        *env.hasRing = true;
    }

    // The caller's content follows on the row's line, centered on it.
    ImGui::SameLine(0.0f, 0.0f);
    window->DC.CursorPos.x = row.content.Min.x;
    return row;
}

// Moves the keyboard focus to a row that may not be submitted this frame, as navigation would.
void FocusRowAt(ImGuiWindow* window, ImGuiID id, ImGuiID focusScope, const ImRect& rect)
{
    ImGuiContext& g = *GImGui;
    if (g.NavWindow == nullptr)
        ImGui::FocusWindow(window);
    ImGuiWindow* navWindow = g.NavWindow != nullptr ? g.NavWindow : window;
    ImGui::SetNavID(id, ImGuiNavLayer_Main, focusScope, ImGui::WindowRectAbsToRel(navWindow, rect));
    ImGui::SetNavCursorVisible(true);
    SetKeyboardModality(true);
}

// After a move made by the list itself (typeahead, Left to the parent): the selection follows the
// focus as after a plain arrow key, and a later Shift range starts there.
bool SelectOnly(ListSelection* selection, ImGuiID storageId, ImGuiID focusScope, ImGuiSelectionUserData data)
{
    if (selection == nullptr)
        return false;
    const bool same = selection->Size == 1 && selection->Contains(storageId);
    if (!same)
    {
        selection->Clear();
        selection->SetItemSelected(storageId, true);
    }
    if (ImGuiMultiSelectState* state = GImGui->MultiSelectStorage.GetByKey(focusScope))
    {
        state->RangeSrcItem = data;
        state->RangeSelected = 1;
        state->NavIdItem = data;
        state->NavIdSelected = 1;
    }
    return !same;
}

// ---- Views ----------------------------------------------------------------------------------------

struct ViewState
{
    TypeaheadState typed;
    int focusIndex = -1;
    ImGuiID focusedId = 0;
    int revealRow = -1;
    ImGuiID anchorId = 0;      // storage id of the Shift range start at the end of the last frame
    bool anchorKnown = false;
};

struct ViewFrame
{
    ImGuiID id = 0;
    ImGuiWindow* window = nullptr;
    ListViewDesc desc;
    ViewState* state = nullptr;
    float rowHeight = 0.0f;
    double rowsTop = 0.0;
    float rowsLeft = 0.0f;
    float rowsRight = 0.0f;
    ImGuiID seed = 0;
    ImGuiID focusScope = 0;
    bool listFocused = false;
    bool multi = false;
    bool choiceScope = false;
    ForcedItemRequest forced;
    int tabStop = -1;
    bool tabStopGiven = false;
    SelectionPill pill;
    bool pillOpen = false;
    ItemState ring;
    bool hasRing = false;
    bool changed = false;
    int activated = -1;
};

struct ViewStack
{
    ViewFrame frames[kListViewDepth];
    int depth = 0;
};

ViewStack& Views()
{
    static ViewStack views;
    return views;
}

void ResetFrame(ViewFrame& frame)
{
    frame.id = 0;
    frame.window = nullptr;
    frame.desc = ListViewDesc();
    frame.state = nullptr;
    frame.listFocused = false;
    frame.multi = false;
    frame.choiceScope = false;
    frame.forced = {};
    frame.tabStop = -1;
    frame.tabStopGiven = false;
    frame.pillOpen = false;
    frame.ring = {};
    frame.hasRing = false;
    frame.changed = false;
    frame.activated = -1;
}

ImRect RowRect(const ViewFrame& frame, int index)
{
    const float top = static_cast<float>(frame.rowsTop + static_cast<double>(index) * frame.rowHeight);
    return ImRect(frame.rowsLeft, top, frame.rowsRight, top + frame.rowHeight);
}

ListEntryInfo EntryOf(const ListViewDesc& desc, int index)
{
    ListEntryInfo info;
    if (desc.entry != nullptr)
        desc.entry(desc.user, index, &info);
    return info;
}

ImGuiID KeyOf(const ListViewDesc& desc, const ListEntryInfo& info, int index)
{
    if (info.id != 0)
        return info.id;
    if (desc.selection != nullptr)
        return desc.selection->GetStorageIdFromIndex(index);
    return static_cast<ImGuiID>(index);
}

ImGuiID RowId(const ViewFrame& frame, ImGuiID key)
{
    return ImHashData(&key, sizeof(key), frame.seed);
}

void MoveViewFocus(ViewFrame& frame, int target, bool animate)
{
    const ListViewDesc& desc = frame.desc;
    const ListEntryInfo info = EntryOf(desc, target);
    const ImGuiID id = RowId(frame, KeyOf(desc, info, target));
    const ImRect rect = RowRect(frame, target);
    FocusRowAt(frame.window, id, frame.focusScope, rect);
    frame.state->focusIndex = target;
    frame.state->focusedId = id;
    if (!desc.readOnly)
    {
        if (frame.multi)
            frame.changed |= SelectOnly(desc.selection, desc.selection->GetStorageIdFromIndex(target), frame.focusScope,
                                        target);
        else if (desc.choice != nullptr && *desc.choice != target)
        {
            *desc.choice = target;
            frame.changed = true;
        }
    }
    ScrollAreaRevealRect(frame.window, rect, ScrollAlign::Nearest, animate);
}

// Home and End. The view moves the focus itself: Dear ImGui's navigation measures the ends in
// float window coordinates, which lose whole rows at the end of a million rows. Shift extends the
// selection from the range start as navigation would, Ctrl keeps the rest of it.
void JumpToEnd(ViewFrame& frame, bool end)
{
    const ListViewDesc& desc = frame.desc;
    const int step = end ? -1 : 1;
    int target = end ? desc.rowCount - 1 : 0;
    while (target >= 0 && target < desc.rowCount && EntryOf(desc, target).disabled)
        target += step;
    if (target < 0 || target >= desc.rowCount)
        return;
    const ImGuiIO& io = ImGui::GetIO();
    if (!frame.multi || !io.KeyShift || desc.single || desc.readOnly)
    {
        MoveViewFocus(frame, target, false);
        return;
    }
    ImGuiMultiSelectState* multi = GImGui->MultiSelectStorage.GetByKey(frame.focusScope);
    ImS64 anchor = target;
    if (multi != nullptr && multi->RangeSrcItem >= 0 && multi->RangeSrcItem < desc.rowCount)
        anchor = multi->RangeSrcItem;
    else if (frame.state->focusIndex >= 0)
        anchor = frame.state->focusIndex;
    static ImGuiMultiSelectIO requests;
    requests.Requests.resize(0);
    requests.ItemsCount = desc.rowCount;
    if (!io.KeyCtrl)
        requests.Requests.push_back({ImGuiSelectionRequestType_SetAll, false, 0, 0, 0});
    ImGuiSelectionRequest range;
    range.Type = ImGuiSelectionRequestType_SetRange;
    range.Selected = true;
    range.RangeDirection = static_cast<ImS8>(target >= anchor ? 1 : -1);
    range.RangeFirstItem = std::min<ImS64>(anchor, target);
    range.RangeLastItem = std::max<ImS64>(anchor, target);
    requests.Requests.push_back(range);
    const std::uint64_t before = ListSelectionFingerprint(*desc.selection);
    ApplyListRequests(desc.selection, &requests, desc.rowCount, desc.entry, desc.user);
    frame.changed |= ListSelectionFingerprint(*desc.selection) != before;
    if (multi != nullptr)
    {
        multi->RangeSrcItem = anchor;
        multi->NavIdItem = target;
    }
    const ListEntryInfo info = EntryOf(desc, target);
    const ImGuiID id = RowId(frame, KeyOf(desc, info, target));
    const ImRect rect = RowRect(frame, target);
    FocusRowAt(frame.window, id, frame.focusScope, rect);
    frame.state->focusIndex = target;
    frame.state->focusedId = id;
    ScrollAreaRevealRect(frame.window, rect, ScrollAlign::Nearest, false);
}

// Dear ImGui keeps the start of a Shift range as an index. When the caller sorted or filtered since
// the last frame, the index points at another entry: move it to the entry it was set on, found by
// its storage id (one scan, only on the frame the order changed), or drop it when the entry is gone.
void RemapAnchor(ViewFrame& frame)
{
    ViewState& state = *frame.state;
    const ListViewDesc& desc = frame.desc;
    ImGuiMultiSelectState* multi = GImGui->MultiSelectStorage.GetByKey(frame.focusScope);
    if (multi == nullptr || !state.anchorKnown || multi->RangeSrcItem < 0)
        return;
    const ImS64 index = multi->RangeSrcItem;
    if (index < desc.rowCount && desc.selection->GetStorageIdFromIndex(static_cast<int>(index)) == state.anchorId)
        return;
    ImGuiSelectionUserData found = ImGuiSelectionUserData_Invalid;
    for (int i = 0; i < desc.rowCount; ++i)
    {
        if (desc.selection->GetStorageIdFromIndex(i) == state.anchorId)
        {
            found = i;
            break;
        }
    }
    multi->RangeSrcItem = found;
    if (multi->NavIdItem == index)
        multi->NavIdItem = found;
}

void RecordAnchor(ViewFrame& frame)
{
    ViewState& state = *frame.state;
    const ImGuiMultiSelectState* multi = GImGui->MultiSelectStorage.GetByKey(frame.focusScope);
    state.anchorKnown = multi != nullptr && multi->RangeSrcItem >= 0 && multi->RangeSrcItem < frame.desc.rowCount;
    if (state.anchorKnown)
        state.anchorId = frame.desc.selection->GetStorageIdFromIndex(static_cast<int>(multi->RangeSrcItem));
}

void ApplyChoice(ViewFrame& frame, const ImGuiMultiSelectIO* io)
{
    const ListViewDesc& desc = frame.desc;
    if (io == nullptr || desc.readOnly || desc.choice == nullptr)
        return;
    for (const ImGuiSelectionRequest& request : io->Requests)
    {
        if (request.Type != ImGuiSelectionRequestType_SetRange || !request.Selected)
            continue;
        const int index = static_cast<int>(request.RangeFirstItem);
        if (index < 0 || index >= desc.rowCount || EntryOf(desc, index).disabled)
            continue;
        if (*desc.choice != index)
        {
            *desc.choice = index;
            frame.changed = true;
        }
    }
}

// ---- Trees ----------------------------------------------------------------------------------------

struct TreeState
{
    TypeaheadState typed;
    ImVector<ImGuiID> order[2];         // keys of the visible nodes in submission order
    ImVector<unsigned char> disabled[2];
    int current = 0;                    // the order being built this frame
    int focusPos = -1;                  // position of the focused node in last frame's order
    ImGuiID focusedId = 0;
    bool focusedSeen = false;           // the focused node was submitted in the last frame
};

struct TreeLevel
{
    ImGuiID id = 0;
    ImGuiID key = 0;
    ImRect rect;
    int position = 0;
    float guideX = 0.0f;
    bool loading = false;
};

struct TypeaheadMatch
{
    ImGuiID id = 0;
    ImGuiID key = 0;
    ImRect rect;
    bool found = false;
};

struct TreeFrame
{
    ImGuiID id = 0;
    ImGuiWindow* window = nullptr;
    ListTreeDesc desc;
    TreeState* state = nullptr;
    float rowHeight = 0.0f;
    double top = 0.0;
    float left = 0.0f;
    float right = 0.0f;
    ImGuiID scope = 0;
    bool listFocused = false;
    bool multi = false;
    ForcedItemRequest forced;
    TreeLevel levels[kListTreeDepth];
    int depth = 0;
    int position = 0;
    bool deepReported = false;
    bool typed = false;
    int typedLength = 0;
    bool typedCycle = false;
    TypeaheadMatch matchAfter;
    TypeaheadMatch matchAny;
    bool focusNext = false;
    bool navHandled = false;  // a Left or Right request was handled this frame
    ImGuiID tabStopId = 0;
    bool tabStopGiven = false;
    bool focusedSeen = false;
    ItemState ring;
    bool hasRing = false;
    bool fingerprinted = false;
    std::uint64_t before = 0;
    bool changed = false;
    ImGuiID activated = 0;
};

struct TreeStack
{
    TreeFrame frames[kListViewDepth];
    int depth = 0;
};

TreeStack& Trees()
{
    static TreeStack trees;
    return trees;
}

int FindKey(const ImVector<ImGuiID>& order, ImGuiID key)
{
    for (int i = 0; i < order.Size; ++i)
        if (order[i] == key)
            return i;
    return -1;
}

// Applies selection requests to a tree's storage over `order` (the visible keys).
void ApplyTree(TreeFrame& frame, const ImGuiMultiSelectIO* io, const ImVector<ImGuiID>& order,
               const ImVector<unsigned char>& disabled)
{
    ListSelection* selection = frame.desc.selection;
    if (io == nullptr || selection == nullptr || frame.desc.readOnly || io->Requests.Size == 0)
        return;
    if (!frame.fingerprinted)
    {
        frame.before = ListSelectionFingerprint(*selection);
        frame.fingerprinted = true;
    }
    for (const ImGuiSelectionRequest& request : io->Requests)
    {
        if (request.Type == ImGuiSelectionRequestType_SetAll)
        {
            if (!request.Selected)
            {
                selection->Clear();
                continue;
            }
            for (int i = 0; i < order.Size; ++i)
                if (i >= disabled.Size || disabled[i] == 0)
                    selection->SetItemSelected(order[i], true);
        }
        else if (request.Type == ImGuiSelectionRequestType_SetRange)
        {
            int a = FindKey(order, static_cast<ImGuiID>(request.RangeFirstItem));
            int b = FindKey(order, static_cast<ImGuiID>(request.RangeLastItem));
            if (a < 0 && b < 0)
                continue;
            if (a < 0)
                a = b;
            if (b < 0)
                b = a;
            for (int i = std::min(a, b); i <= std::max(a, b); ++i)
                if (i >= disabled.Size || disabled[i] == 0)
                    selection->SetItemSelected(order[i], request.Selected);
        }
    }
}

void MoveTreeFocus(TreeFrame& frame, ImGuiID id, ImGuiID key, const ImRect& rect)
{
    FocusRowAt(frame.window, id, frame.scope, rect);
    frame.state->focusedId = id;
    const ImGuiIO& io = ImGui::GetIO();
    if (frame.multi && !frame.desc.readOnly && !io.KeyCtrl && !io.KeyShift)
        frame.changed |= SelectOnly(frame.desc.selection, key, frame.scope, static_cast<ImGuiSelectionUserData>(key));
    ScrollAreaRevealRect(frame.window, rect, ScrollAlign::Nearest, true);
}

ImRect TreeRowRect(const TreeFrame& frame, int position)
{
    const float top = static_cast<float>(frame.top + static_cast<double>(position) * frame.rowHeight);
    return ImRect(frame.left, top, frame.right, top + frame.rowHeight);
}

// A row that is not an item: the loading row under the children of a node.
void DrawLoadingRow(const TreeFrame& frame, const ImRect& rect, float indent)
{
    const Theme& theme = T();
    ImDrawList* drawList = frame.window->DrawList;
    const float size = Px(theme.spacing.lg);
    const float x = rect.Min.x + indent + Px(theme.spacing.sm);
    const ImVec2 center(x + size * 0.5f, (rect.Min.y + rect.Max.y) * 0.5f);
    DrawSpinner(drawList, center, size * 0.35f, ToU32(theme.colors.labelSecondary), HairlineThickness() * 2.0f);
    PushTextStyle(TextStyle::Body);
    drawList->AddText(ImVec2(x + size + Px(theme.spacing.sm), CenteredTextY(rect.Min.y, rect.GetHeight())),
                      ToU32(theme.colors.labelSecondary), GetStrings().loading);
    PopTextStyle();
}

} // namespace

float ListRowHeight(ControlSize size)
{
    const ListTokens& list = T().list;
    switch (ResolveControlSize(size))
    {
    case ControlSize::Small:
        return Px(list.rowHeightSmall);
    case ControlSize::Large:
        return Px(list.rowHeightLarge);
    default:
        return Px(list.rowHeight);
    }
}

int ListTypeaheadFind(const char* typed, int count, int from, ListEntryFn entry, void* user)
{
    if (typed == nullptr || typed[0] == '\0' || count <= 0 || entry == nullptr)
        return -1;
    bool cycle = false;
    const int length = SearchLength(typed, &cycle);
    const int start = cycle ? from + 1 : std::max(from, 0);
    for (int step = 0; step < count; ++step)
    {
        const int index = ((start + step) % count + count) % count;
        ListEntryInfo info;
        entry(user, index, &info);
        if (!info.disabled && StartsWith(info.label, typed, length))
            return index;
    }
    return -1;
}

bool BeginListView(ImGuiID id, const ListViewDesc& desc)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || id == 0)
        return false;
    ImGuiContext& g = *context;
    ViewStack& views = Views();
    if (views.depth >= kListViewDepth)
    {
        ReportUsageError("BeginListView nested deeper than %d; the view is not opened", kListViewDepth);
        return false;
    }
    // The forced look belongs to the rows, not to the area's bars.
    const ForcedItemRequest forced = TakeForcedItemState();
    ScrollAreaDesc area;
    area.size = desc.areaSize;
    area.fadeSurface = desc.fadeSurface;
    if (!BeginScrollAreaCore(id, area))
        return false;
    ImGuiWindow* window = g.CurrentWindow;
    if (desc.state != DataState::Ready || desc.rowCount <= 0)
    {
        DrawListState(window->InnerRect, desc.state, desc.emptyText, desc.errorText);
        EndScrollAreaCore();
        return false;
    }

    ViewFrame& frame = views.frames[views.depth++];
    ResetFrame(frame);
    frame.id = id;
    frame.window = window;
    frame.desc = desc;
    frame.state = &GetState<ViewState>(id);
    frame.forced = forced;
    frame.rowHeight = desc.rowHeight > 0.0f ? desc.rowHeight : ListRowHeight(desc.size);
    frame.rowsTop = static_cast<double>(window->DC.CursorPos.y) + static_cast<double>(window->DC.CursorStartPosLossyness.y);
    frame.rowsLeft = window->DC.CursorPos.x;
    frame.rowsRight = std::max(frame.rowsLeft + 1.0f, window->WorkRect.Max.x);
    frame.seed = window->IDStack.back();
    frame.multi = desc.selection != nullptr;
    frame.choiceScope = !frame.multi && desc.choice != nullptr;
    frame.focusScope = (frame.multi || frame.choiceScope) ? window->IDStack.back() : g.CurrentFocusScopeId;
    ViewState& state = *frame.state;
    if (state.focusIndex >= desc.rowCount)
        state.focusIndex = -1;
    frame.listFocused = g.NavId != 0 && (g.NavId == state.focusedId || g.NavWindow == window ||
                                         ((frame.multi || frame.choiceScope) && g.NavFocusScopeId == frame.focusScope));

    if (frame.listFocused && desc.entry != nullptr && UpdateTypeahead(state.typed))
    {
        const int target = ListTypeaheadFind(state.typed.text, desc.rowCount, state.focusIndex, desc.entry, desc.user);
        if (target >= 0 && target != state.focusIndex)
            MoveViewFocus(frame, target, false);
    }
    if (frame.listFocused)
    {
        // Owned while the view has focus, so navigation leaves Home and End to the view.
        ImGui::SetKeyOwner(ImGuiKey_Home, id);
        ImGui::SetKeyOwner(ImGuiKey_End, id);
        const bool home = ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_Repeat, id);
        const bool end = ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_Repeat, id);
        if (home != end)
            JumpToEnd(frame, end);
    }

    if (frame.multi)
    {
        RemapAnchor(frame);
        ListSelectionOptions options;
        options.single = desc.single;
        options.boxSelect = desc.boxSelect;
        options.readOnly = desc.readOnly;
        BeginListSelectionFiltered(desc.selection, desc.rowCount, options, desc.entry, desc.user);
    }
    else if (frame.choiceScope)
    {
        const ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_SingleSelect | ImGuiMultiSelectFlags_NoAutoClear;
        ApplyChoice(frame, ImGui::BeginMultiSelect(flags, *desc.choice >= 0 ? 1 : 0, desc.rowCount));
    }

    // The row Tab lands on stays submitted: the focused row, else the chosen one.
    if (state.focusIndex >= 0)
        frame.tabStop = state.focusIndex;
    else if (desc.choice != nullptr && *desc.choice >= 0 && *desc.choice < desc.rowCount)
        frame.tabStop = *desc.choice;
    BeginListRows({.rowCount = desc.rowCount, .rowHeight = frame.rowHeight, .columns = 1, .keepAlive = frame.tabStop});

    if (desc.revealRow >= 0 && desc.revealRow < desc.rowCount)
        state.revealRow = desc.revealRow;
    if (state.revealRow >= 0 && state.revealRow < desc.rowCount)
    {
        const ImRect rect = RowRect(frame, state.revealRow);
        ScrollAreaRevealRect(window, rect, ScrollAlign::Nearest, false);
        // Until the content height is known, the scroll range cannot reach the row yet.
        if (window->ScrollMax.y > 0.0f || window->InnerRect.Contains(rect))
            state.revealRow = -1;
    }
    else
    {
        state.revealRow = -1;
    }

    if (desc.pill && frame.choiceScope)
    {
        frame.pill.Begin(window->GetID("##facet.pill"), window->DrawList,
                         ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
        frame.pillOpen = true;
    }
    return true;
}

ListRowState ListViewRow(int index, const ListRowOptions& options)
{
    ViewStack& views = Views();
    if (views.depth == 0)
    {
        ReportUsageError("ListViewRow outside BeginListView / EndListView");
        return {};
    }
    ViewFrame& frame = views.frames[views.depth - 1];
    const ListViewDesc& desc = frame.desc;
    if (index < 0 || index >= desc.rowCount || GImGui->CurrentWindow != frame.window)
        return {};
    const ListEntryInfo info = EntryOf(desc, index);
    const bool disabled = options.disabled || info.disabled;
    const ImGuiID id = RowId(frame, KeyOf(desc, info, index));
    const bool selected = frame.multi ? IsListEntrySelected(desc.selection, index)
                                      : (desc.choice != nullptr && *desc.choice == index);
    bool tabStop = false;
    if (frame.tabStop >= 0)
        tabStop = index == frame.tabStop;
    else if (!frame.tabStopGiven && !disabled)
        tabStop = true;
    frame.tabStopGiven = frame.tabStopGiven || tabStop;

    RowEnv env;
    env.window = frame.window;
    env.multi = frame.multi || frame.choiceScope;
    env.listFocused = frame.listFocused;
    env.pill = frame.pillOpen ? &frame.pill : nullptr;
    env.forced = &frame.forced;
    env.ring = &frame.ring;
    env.hasRing = &frame.hasRing;
    const ListRowState row = RunRow(env, id, RowRect(frame, index), index, selected, disabled, tabStop, index, 0.0f);
    if (row.focused)
    {
        frame.state->focusIndex = index;
        frame.state->focusedId = id;
    }
    if (row.activated)
        frame.activated = index;
    return row;
}

ListViewResult EndListView()
{
    ViewStack& views = Views();
    if (views.depth == 0)
    {
        ReportUsageError("EndListView without BeginListView");
        return {};
    }
    ViewFrame& frame = views.frames[views.depth - 1];
    if (GImGui->CurrentWindow != frame.window)
    {
        ReportUsageError("EndListView called in another window than its BeginListView");
        --views.depth;
        return {};
    }
    EndListRows();
    if (frame.multi)
    {
        frame.changed |= EndListSelection();
        RecordAnchor(frame);
    }
    else if (frame.choiceScope)
        ApplyChoice(frame, ImGui::EndMultiSelect());
    const float radius = Px(T().radii.control);
    if (frame.pillOpen)
    {
        frame.pill.End(ToU32(T().colors.accentSubtle), radius);
        frame.pillOpen = false;
    }
    if (frame.hasRing)
        DrawItemFocusRing(frame.ring, radius);
    EndScrollAreaCore();
    const ListViewResult result{frame.changed, frame.activated};
    --views.depth;
    return result;
}

bool BeginListTree(ImGuiID id, const ListTreeDesc& desc)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || id == 0)
        return false;
    ImGuiContext& g = *context;
    TreeStack& trees = Trees();
    if (trees.depth >= kListViewDepth)
    {
        ReportUsageError("BeginListTree nested deeper than %d; the tree is not opened", kListViewDepth);
        return false;
    }
    const ForcedItemRequest forced = TakeForcedItemState();
    ScrollAreaDesc area;
    area.size = desc.areaSize;
    area.fitHeight = desc.fitHeight;
    area.fadeSurface = desc.fadeSurface;
    if (!BeginScrollAreaCore(id, area))
        return false;
    ImGuiWindow* window = g.CurrentWindow;
    if (desc.state != DataState::Ready)
    {
        DrawListState(window->InnerRect, desc.state, desc.emptyText, desc.errorText);
        EndScrollAreaCore();
        return false;
    }

    TreeFrame& frame = trees.frames[trees.depth++];
    frame = TreeFrame();
    frame.id = id;
    frame.window = window;
    frame.desc = desc;
    frame.forced = forced;
    frame.state = &GetState<TreeState>(id);
    TreeState& state = *frame.state;
    state.current ^= 1;
    state.order[state.current].resize(0);
    state.disabled[state.current].resize(0);
    const ImVector<ImGuiID>& previous = state.order[state.current ^ 1];
    frame.rowHeight = ListRowHeight(desc.size);
    frame.top = static_cast<double>(window->DC.CursorPos.y) + static_cast<double>(window->DC.CursorStartPosLossyness.y);
    frame.left = window->DC.CursorPos.x;
    frame.right = std::max(frame.left + 1.0f, window->WorkRect.Max.x);
    frame.multi = desc.selection != nullptr;
    frame.scope = frame.multi ? window->IDStack.back() : g.CurrentFocusScopeId;
    frame.listFocused = g.NavId != 0 && (g.NavId == state.focusedId || g.NavWindow == window ||
                                         (frame.multi && g.NavFocusScopeId == frame.scope));
    // Tab lands on the focused node while it is shown, else on the first node.
    frame.tabStopId = state.focusedSeen ? state.focusedId : 0;
    if (frame.listFocused && UpdateTypeahead(state.typed))
    {
        frame.typed = true;
        frame.typedLength = SearchLength(state.typed.text, &frame.typedCycle);
    }
    if (frame.multi)
    {
        const ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
        ApplyTree(frame, ImGui::BeginMultiSelect(flags, desc.selection->Size, -1), previous,
                  state.disabled[state.current ^ 1]);
    }
    return true;
}

ListTreeNodeState ListTreeNode(const char* label, const ListTreeNodeOptions& options)
{
    TreeStack& trees = Trees();
    if (trees.depth == 0)
    {
        ReportUsageError("ListTreeNode outside BeginListTree / EndListTree");
        return {};
    }
    TreeFrame& frame = trees.frames[trees.depth - 1];
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = frame.window;
    if (g.CurrentWindow != window)
        return {};
    if (label == nullptr)
        label = "";
    const Theme& theme = T();
    TreeState& state = *frame.state;

    const ImGuiID id = window->GetID(label);
    const ImGuiID key = options.key != 0 ? options.key : id;
    const int depth = frame.depth;
    const int position = frame.position++;
    const ImRect rect = TreeRowRect(frame, position);
    const float indentStep = Px(theme.list.indent);
    const float indent = static_cast<float>(depth) * indentStep;
    const ImRect expander(rect.Min.x + Px(theme.spacing.xs) + indent, rect.Min.y,
                          rect.Min.x + Px(theme.spacing.xs) + indent + indentStep, rect.Max.y);

    const ImGuiTreeNodeFlags openFlags = options.defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    bool open = options.leaf ? false : ImGui::TreeNodeUpdateNextOpen(id, openFlags);
    if (open && depth >= kListTreeDepth)
    {
        if (!frame.deepReported)
            ReportUsageError("ListTreeNode nested deeper than %d levels; deeper nodes are shown closed", kListTreeDepth);
        frame.deepReported = true;
        open = false;
    }

    // The expander takes the pointer before the row, so a click on it toggles without selecting.
    bool toggled = false;
    ItemState chevron;
    if (!options.leaf)
    {
        static const ForcedItemRequest kNoForce;
        ItemOptions expanderOptions;
        expanderOptions.press = PressMode::Click;
        expanderOptions.tabStop = false;
        expanderOptions.navigable = false;
        expanderOptions.hitTarget = false;
        expanderOptions.disabled = options.disabled;
        expanderOptions.forced = &kNoForce;
        chevron = PartBehavior(ImHashStr("##facet.expander", 0, id), expander, expanderOptions);
        toggled = chevron.pressed;
    }

    bool tabStop = false;
    if (frame.tabStopId != 0)
        tabStop = id == frame.tabStopId;
    else if (!frame.tabStopGiven && !options.disabled)
        tabStop = true;
    frame.tabStopGiven = frame.tabStopGiven || tabStop;
    frame.focusedSeen = frame.focusedSeen || id == state.focusedId;

    RowEnv env;
    env.window = window;
    env.multi = frame.multi;
    env.listFocused = frame.listFocused;
    env.forced = &frame.forced;
    env.ring = &frame.ring;
    env.hasRing = &frame.hasRing;
    const bool selected = frame.multi && frame.desc.selection->Contains(key);
    ListRowState row = RunRow(env, id, rect, static_cast<ImGuiSelectionUserData>(key), selected, options.disabled,
                              tabStop, position, indent + indentStep);

    if (row.focused)
    {
        state.focusPos = position;
        state.focusedId = id;
    }
    // The first child after Right on an open node.
    if (frame.focusNext && !options.disabled)
    {
        frame.focusNext = false;
        ImGui::SetFocusID(id, window);
        ImGui::SetNavCursorVisible(true);
        SetKeyboardModality(true);
        state.focusedId = id;
        const ImGuiIO& io = g.IO;
        if (frame.multi && !frame.desc.readOnly && !io.KeyCtrl && !io.KeyShift)
            frame.changed |= SelectOnly(frame.desc.selection, key, frame.scope, static_cast<ImGuiSelectionUserData>(key));
        ScrollAreaRevealRect(window, rect, ScrollAlign::Nearest, true);
    }
    // Left and Right on the focused node open, close and move, before navigation looks for an
    // item beside it.
    // One press is handled once: the node that receives the focus later in this frame does not see
    // the same request again.
    const bool sideways = g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right;
    if (g.NavId == id && !options.disabled && !frame.navHandled && sideways)
    {
        frame.navHandled = true;
        if (g.NavMoveDir == ImGuiDir_Right && !options.leaf)
        {
            if (!open)
                toggled = true;
            else
                frame.focusNext = true;
            ImGui::NavMoveRequestCancel();
        }
        else if (g.NavMoveDir == ImGuiDir_Left)
        {
            if (open && !options.leaf)
            {
                toggled = true;
            }
            else if (depth > 0)
            {
                const TreeLevel& parent = frame.levels[depth - 1];
                MoveTreeFocus(frame, parent.id, parent.key, parent.rect);
            }
            ImGui::NavMoveRequestCancel();
        }
    }
    if (row.activated)
    {
        if (options.leaf)
            frame.activated = key;
        else
            toggled = true;
    }
    if (toggled && !options.leaf && !(depth >= kListTreeDepth && !open))
    {
        open = !open;
        window->DC.StateStorage->SetInt(id, open ? 1 : 0);
    }
    if (!options.leaf)
    {
        if (toggled)
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
        if (chevron.visible || row.item.visible)
        {
            const float size = Px(theme.spacing.lg);
            const ImVec4& color = options.disabled ? theme.colors.labelTertiary : theme.colors.labelSecondary;
            DrawIcon(window->DrawList, Icon(open ? "chevron-down" : "chevron-right"),
                     ImVec2(expander.GetCenter().x - size * 0.5f, expander.GetCenter().y - size * 0.5f), size,
                     ToU32(color));
        }
    }

    state.order[state.current].push_back(key);
    state.disabled[state.current].push_back(options.disabled ? 1 : 0);

    if (frame.typed && !options.disabled && StartsWith(label, state.typed.text, frame.typedLength))
    {
        const bool after = frame.typedCycle ? position > state.focusPos : position >= state.focusPos;
        const TypeaheadMatch match{id, key, rect, true};
        if (!frame.matchAny.found)
            frame.matchAny = match;
        if (after && !frame.matchAfter.found)
            frame.matchAfter = match;
    }

    if (open && !options.leaf)
    {
        TreeLevel& level = frame.levels[frame.depth++];
        level.id = id;
        level.key = key;
        level.rect = rect;
        level.position = position;
        level.guideX = expander.GetCenter().x;
        level.loading = options.loading;
        ImGui::PushOverrideID(id);
    }
    ListTreeNodeState result;
    result.row = row;
    result.key = key;
    result.open = open && !options.leaf;
    result.toggled = toggled;
    return result;
}

void ListTreePop()
{
    TreeStack& trees = Trees();
    if (trees.depth == 0)
    {
        ReportUsageError("ListTreePop outside BeginListTree / EndListTree");
        return;
    }
    TreeFrame& frame = trees.frames[trees.depth - 1];
    if (frame.depth == 0)
    {
        ReportUsageError("ListTreePop without an open node");
        return;
    }
    const TreeLevel level = frame.levels[--frame.depth];
    ImGui::PopID();
    const Theme& theme = T();
    const float indentStep = Px(theme.list.indent);
    if (level.loading)
    {
        const int position = frame.position++;
        DrawLoadingRow(frame, TreeRowRect(frame, position), static_cast<float>(frame.depth + 1) * indentStep + indentStep);
    }
    if (frame.desc.indentGuides && frame.position > level.position + 1)
    {
        const float bottom = TreeRowRect(frame, frame.position - 1).Max.y;
        HairlineV(frame.window->DrawList, level.guideX, level.rect.Max.y, bottom, ToU32(theme.colors.separator));
    }
}

ListTreeResult EndListTree()
{
    TreeStack& trees = Trees();
    if (trees.depth == 0)
    {
        ReportUsageError("EndListTree without BeginListTree");
        return {};
    }
    TreeFrame& frame = trees.frames[trees.depth - 1];
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow != frame.window)
    {
        ReportUsageError("EndListTree called in another window than its BeginListTree");
        --trees.depth;
        return {};
    }
    if (frame.depth > 0)
    {
        ReportUsageError("BeginListTree: %d open node(s) without ListTreePop at EndListTree", frame.depth);
        while (frame.depth > 0)
        {
            --frame.depth;
            ImGui::PopID();
        }
    }
    TreeState& state = *frame.state;
    state.focusedSeen = frame.focusedSeen;
    if (frame.multi)
        ApplyTree(frame, ImGui::EndMultiSelect(), state.order[state.current], state.disabled[state.current]);
    if (frame.typed)
    {
        const TypeaheadMatch& match = frame.matchAfter.found ? frame.matchAfter : frame.matchAny;
        if (match.found && match.id != g.NavId)
            MoveTreeFocus(frame, match.id, match.key, match.rect);
    }
    if (frame.position == 0)
        DrawListState(frame.window->InnerRect, DataState::Ready, frame.desc.emptyText, nullptr);
    const float radius = Px(T().radii.control);
    if (frame.hasRing)
        DrawItemFocusRing(frame.ring, radius);
    // The content ends below the last row (the loading rows are not items).
    frame.window->DC.CursorPos = ImVec2(frame.left, TreeRowRect(frame, frame.position).Min.y);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    EndScrollAreaCore();
    if (frame.multi && frame.fingerprinted)
        frame.changed |= ListSelectionFingerprint(*frame.desc.selection) != frame.before;
    const ListTreeResult result{frame.changed, frame.activated};
    --trees.depth;
    return result;
}

void DrawListState(const ImRect& rect, DataState state, const char* emptyText, const char* errorText)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr)
        return;
    const Theme& theme = T();
    const Colors& c = theme.colors;
    const Strings& strings = GetStrings();
    ImDrawList* drawList = context->CurrentWindow->DrawList;
    const char* text = nullptr;
    ImU32 textColor = ToU32(c.labelSecondary);
    if (state == DataState::Loading)
        text = strings.loading;
    else if (state == DataState::Error)
    {
        text = errorText != nullptr ? errorText : strings.loadFailed;
        textColor = ToU32(c.label);
    }
    else
        text = emptyText != nullptr ? emptyText : strings.empty;

    PushTextStyle(TextStyle::Body);
    const float gap = Px(theme.spacing.sm);
    const float glyph = state == DataState::Ready ? 0.0f : Px(theme.spacing.lg);
    const float glyphSpan = glyph > 0.0f ? glyph + gap : 0.0f;
    const float available = std::max(0.0f, rect.GetWidth() - 2.0f * gap - glyphSpan);
    char shown[256];
    const TruncateResult fitted = TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), text, nullptr, available,
                                               TruncateMode::End, shown, sizeof(shown));
    float x = std::floor(rect.GetCenter().x - (fitted.width + glyphSpan) * 0.5f);
    const float centerY = rect.GetCenter().y;
    if (state == DataState::Loading)
        DrawSpinner(drawList, ImVec2(x + glyph * 0.5f, centerY), glyph * 0.35f, ToU32(c.labelSecondary),
                    HairlineThickness() * 2.0f);
    else if (state == DataState::Error)
        DrawIcon(drawList, Icon("error"), ImVec2(x, std::floor(centerY - glyph * 0.5f)), glyph, ToU32(c.destructive));
    x += glyphSpan;
    drawList->AddText(ImVec2(x, CenteredTextY(rect.Min.y, rect.GetHeight())), textColor, shown);
    PopTextStyle();
}

void ListCoreEndFrame()
{
    ListRowsEndFrame();
    ViewStack& views = Views();
    TreeStack& trees = Trees();
    if (views.depth > 0)
        ReportUsageError("BeginListView without EndListView: %d view(s) still open at EndFrame", views.depth);
    if (trees.depth > 0)
        ReportUsageError("BeginListTree without EndListTree: %d tree(s) still open at EndFrame", trees.depth);
    views.depth = 0;
    trees.depth = 0;
}

void ListCoreShutdown()
{
    ListRowsShutdown();
    Views().depth = 0;
    Trees().depth = 0;
}

} // namespace facet::internal
