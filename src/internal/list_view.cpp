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
#include "internal/control.h"
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

// ---- Typeahead ------------------------------------------------------------------------------------

using TypeaheadState = ListTypeahead;

bool UpdateTypeahead(TypeaheadState& typed)
{
    return typed.Update();
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
    SelectionPill* pill = nullptr;  // the chosen row targets the pill instead of a fill
    const ForcedItemRequest* forced = nullptr;
    ItemState* ring = nullptr;      // the focused row, for the ring drawn at the end; its rect is the row's fill
    bool* hasRing = nullptr;
    // Set with the ring: true when the ring belongs to the current row of a driven list rather than
    // to the keyboard focus, so the owner shows it only while the keys put that row there.
    bool* ringFromHighlight = nullptr;
};

// How a row is placed beyond its rectangle (ListRowDesc::inPlace, allowOverlap, ownRing).
struct RowPlacement
{
    bool inPlace = false;
    bool allowOverlap = false;
    bool ownRing = false;
    bool tile = false;  // the row look of a tile (ListRowDesc::tile)
};

ListRowState RunRow(const RowEnv& env, ImGuiID id, const ImRect& rect, ImGuiSelectionUserData data, bool selected,
                    bool disabled, bool tabStop, int part, float indent, bool highlighted = false,
                    const RowPlacement& placement = {})
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = env.window;
    const Theme& theme = T();
    const ImVec2 cursorBefore = window->DC.CursorPos;
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
    options.allowOverlap = placement.allowOverlap;
    ItemState item = placement.inPlace ? PartBehavior(id, rect, options) : ItemBehavior(id, rect.GetSize(), options);
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
    // Rows are inset one hairline top and bottom, so adjacent fills stay apart.
    const ImRect fill = ListRowFillRect(rect);
    if (item.visible)
    {
        const bool quiet = IsScrollAreaScrolling(window);
        ListRowLook look;
        look.hoverAmount = quiet ? 0.0f : item.hoverAmount;
        look.pressAmount = item.pressAmount;
        look.highlighted = highlighted;
        look.selected = selected && env.pill == nullptr;
        look.disabled = item.disabled;
        look.tile = placement.tile;
        DrawListRowFill(window->DrawList, fill, Px(theme.radii.control), look);
    }
    if (env.ring != nullptr && !placement.ownRing &&
        (item.focusAmount > 0.0f || (highlighted && item.visible && !item.disabled)))
    {
        // The current row of a driven list may show the ring the keyboard focus would, when the keys
        // put it there; the owner decides with ringFromHighlight. The ring sits inside the fill.
        *env.ring = item;
        env.ring->rect = fill;
        const bool fromHighlight = highlighted && item.focusAmount <= 0.0f;
        if (highlighted)
            env.ring->focusAmount = 1.0f;
        *env.hasRing = true;
        if (env.ringFromHighlight != nullptr)
            *env.ringFromHighlight = fromHighlight;
    }

    if (placement.inPlace)
    {
        // The container laid the row out itself: the cursor goes back where it was.
        window->DC.CursorPos = cursorBefore;
        return row;
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

// ---- Tab ------------------------------------------------------------------------------------------
//
// Rows inside a BeginMultiSelect scope cannot be reached or left with Tab by Dear ImGui alone (its
// tabbing only visits items of the focus scope that holds the focus). See "Tab" in list_view.h.

struct TabExit
{
    bool pending = false;
    int dir = 0;          // +1 Tab, -1 Shift+Tab
    ImGuiID proxy = 0;    // the tab stop the focus was put on
};

TabExit& PendingTabExit()
{
    static TabExit exit;
    return exit;
}

ImGuiID TabProxyId(ImGuiID listId)
{
    return ImHashStr("##facet.tab", 0, listId);
}

// Submits the tab stop of a list in the surrounding focus scope (not interactive, not hoverable).
// Returns true when the focus has just landed on it from outside, so the list moves it on to a row.
bool SubmitTabProxy(ImGuiID proxyId, const ImRect& rect)
{
    ImGuiContext& g = *GImGui;
    // Read before ItemAdd: a backward tabbing request stops scoring when it reaches this item.
    const TabExit& exit = PendingTabExit();
    const bool leaving = (exit.pending && exit.proxy == proxyId) ||
                         ((g.NavMoveSubmitted || g.NavMoveScoringItems) && (g.NavMoveFlags & ImGuiNavMoveFlags_IsTabbing) != 0);
    // Not a default focus candidate: a window that picks its first item to focus (when it appears
    // or gets the keyboard) takes a real item, so only Tab moves the focus into the list.
    ImGui::ItemAdd(rect, proxyId, nullptr, ImGuiItemFlags_NoNavDefaultFocus);
    return g.NavId == proxyId && !leaving;
}

// While a row of the list has the focus: the list owns Tab, and a press puts the focus on its tab
// stop, from where ListCoreNewFrame starts Dear ImGui's tabbing in the next frame.
void ClaimTabExit(ImGuiWindow* window, ImGuiID owner, ImGuiID proxyId, ImGuiID outerScope, const ImRect& proxyRect)
{
    ImGuiContext& g = *GImGui;
    ImGui::SetKeyOwner(ImGuiKey_Tab, owner);
    if (!ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_Repeat, owner) || g.IO.KeyCtrl || g.IO.KeyAlt)
        return;
    ImGuiWindow* navWindow = g.NavWindow != nullptr ? g.NavWindow : window;
    ImGui::SetNavID(proxyId, ImGuiNavLayer_Main, outerScope, ImGui::WindowRectAbsToRel(navWindow, proxyRect));
    ImGui::ItemAdd(proxyRect, proxyId);  // alive in this frame, so navigation keeps it as the focus
    TabExit& exit = PendingTabExit();
    exit.pending = true;
    exit.dir = g.IO.KeyShift ? -1 : 1;
    exit.proxy = proxyId;
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
    ListRangeAnchor anchor;    // the Shift range start at the end of the last frame
    // A driven list's current row: whether the keys (or the driving item) put it where it is, as
    // opposed to the pointer; the row at the end of the last frame; and the last frame the view
    // was shown, so a list opened anew starts from the modality of its opening.
    bool highlightByKeys = false;
    int lastHighlight = -1;
    int shownFrame = -1;
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
    bool ringFromHighlight = false;
    bool changed = false;
    int activated = -1;
    bool driven = false;  // desc.highlight is set
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
    frame.ringFromHighlight = false;
    frame.changed = false;
    frame.activated = -1;
    frame.driven = false;
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

// Home and End, through the row block's ListJumpToEnd (see there why the view moves the focus
// itself). A single choice follows the focus as after an arrow key.
void JumpToEnd(ViewFrame& frame, bool end)
{
    const ListViewDesc& desc = frame.desc;
    ListJumpDesc jump;
    jump.rowCount = desc.rowCount;
    jump.selection = frame.multi ? desc.selection : nullptr;
    jump.single = desc.single;
    jump.readOnly = desc.readOnly;
    jump.entry = desc.entry;
    jump.user = desc.user;
    jump.focusScope = frame.focusScope;
    jump.focusIndex = frame.state->focusIndex;
    bool changed = false;
    const int target = ListJumpToEnd(jump, end, &changed);
    if (target < 0)
        return;
    frame.changed |= changed;
    if (!frame.multi && !desc.readOnly && desc.choice != nullptr && *desc.choice != target)
    {
        *desc.choice = target;
        frame.changed = true;
    }
    const ListEntryInfo info = EntryOf(desc, target);
    const ImGuiID id = RowId(frame, KeyOf(desc, info, target));
    const ImRect rect = RowRect(frame, target);
    FocusRowAt(frame.window, id, frame.focusScope, rect);
    frame.state->focusIndex = target;
    frame.state->focusedId = id;
    ScrollAreaRevealRect(frame.window, rect, ScrollAlign::Nearest, false);
}

// The next enabled entry from `from` (included) in direction `step`, -1 when there is none.
int EnabledFrom(const ListViewDesc& desc, int from, int step)
{
    for (int i = from; i >= 0 && i < desc.rowCount; i += step)
        if (!EntryOf(desc, i).disabled)
            return i;
    return -1;
}

// Activates the active row of a driven list: the result reports it and a choice takes it.
void ActivateHighlight(ViewFrame& frame, int index)
{
    const ListViewDesc& desc = frame.desc;
    if (index < 0 || index >= desc.rowCount || EntryOf(desc, index).disabled)
        return;
    frame.activated = index;
    *desc.highlight = index;
    if (!desc.readOnly && desc.selection == nullptr && desc.choice != nullptr && *desc.choice != index)
    {
        *desc.choice = index;
        frame.changed = true;
    }
}

// The keys of a driven list, read through desc.keyOwner (see "Driven lists" in list_view.h).
void DriveHighlight(ViewFrame& frame)
{
    const ListViewDesc& desc = frame.desc;
    const ImGuiID owner = desc.keyOwner;
    int& active = *desc.highlight;
    const auto pressed = [owner](ImGuiKey key) { return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, owner); };
    const int page = std::max(1, static_cast<int>(frame.window->InnerRect.GetHeight() / frame.rowHeight));
    int target = active;
    bool jump = false;
    if (pressed(ImGuiKey_DownArrow) && !pressed(ImGuiKey_UpArrow))
    {
        const int next = EnabledFrom(desc, active < 0 ? 0 : active + 1, 1);
        target = next >= 0 ? next : active;
    }
    else if (pressed(ImGuiKey_UpArrow))
    {
        const int next = EnabledFrom(desc, active < 0 ? desc.rowCount - 1 : active - 1, -1);
        target = next >= 0 ? next : active;
    }
    else if (pressed(ImGuiKey_PageDown) || pressed(ImGuiKey_PageUp))
    {
        const int step = pressed(ImGuiKey_PageDown) ? 1 : -1;
        const int from = std::clamp(std::max(active, 0) + step * page, 0, desc.rowCount - 1);
        int next = EnabledFrom(desc, from, step);
        if (next < 0)
            next = EnabledFrom(desc, from, -step);
        target = next >= 0 ? next : active;
    }
    else if (pressed(ImGuiKey_Home) != pressed(ImGuiKey_End))
    {
        ListJumpDesc ends;
        ends.rowCount = desc.rowCount;
        ends.entry = desc.entry;
        ends.user = desc.user;
        const int next = ListJumpToEnd(ends, pressed(ImGuiKey_End), nullptr);
        target = next >= 0 ? next : active;
        jump = true;
    }
    if (target != active && target >= 0)
    {
        active = target;
        frame.state->highlightByKeys = true;
        ScrollAreaRevealRect(frame.window, RowRect(frame, target), ScrollAlign::Nearest, !jump);
    }
    const bool activate = ImGui::IsKeyPressed(ImGuiKey_Enter, ImGuiInputFlags_None, owner) ||
                          ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, ImGuiInputFlags_None, owner) ||
                          ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_None, owner);
    if (activate)
        ActivateHighlight(frame, active);
}

// The start of a Shift range across a sort or filter (ListRangeAnchor in list_view.h).
void RemapAnchor(ViewFrame& frame)
{
    frame.state->anchor.Remap(frame.desc.selection, frame.focusScope, frame.desc.rowCount);
}

void RecordAnchor(ViewFrame& frame)
{
    frame.state->anchor.Record(frame.desc.selection, frame.focusScope, frame.desc.rowCount);
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
    bool enterByTab = false;  // Tab landed on the tree's tab stop: the node Tab lands on takes the focus
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

// ---- Row block ------------------------------------------------------------------------------------

bool ListTypeahead::Update()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.InputQueueCharacters.Size == 0 || io.KeyCtrl || io.KeyAlt || io.KeySuper)
        return false;
    const double now = ImGui::GetTime();
    if (now - time > static_cast<double>(T().list.typeaheadTimeout))
        length = 0;
    bool changed = false;
    for (const ImWchar c : io.InputQueueCharacters)
    {
        if (c < 0x20 || c == 0x7F || (c == ' ' && length == 0))
            continue;
        char utf8[5] = {};
        const int bytes = ImTextCharToUtf8(utf8, c);
        if (length + bytes >= kCapacity)
            break;
        std::memcpy(text + length, utf8, static_cast<size_t>(bytes));
        length += bytes;
        changed = true;
    }
    text[length] = '\0';
    if (changed)
        time = now;
    return changed;
}

int ListTypeahead::Find(int count, int from, ListEntryFn entry, void* user) const
{
    return ListTypeaheadFind(text, count, from, entry, user);
}

ImRect ListRowFillRect(const ImRect& row)
{
    const float gap = HairlineThickness();
    return ImRect(row.Min.x, row.Min.y + gap, row.Max.x, std::max(row.Min.y + gap, row.Max.y - gap));
}

void DrawListRowFill(ImDrawList* drawList, const ImRect& rect, float radius, const ListRowLook& look)
{
    if (drawList == nullptr)
        return;
    const Colors& c = T().colors;
    if (look.selected)
    {
        FillRect(drawList, rect.Min, rect.Max, ToU32(c.accentSubtle), radius);
        if (look.tile)
            DrawOutline(drawList, rect, radius, ControlOutlineColor({.selected = true, .disabled = look.disabled}));
        return;
    }
    if (look.disabled)
        return;
    const float feedback = look.highlighted ? 1.0f : std::max(look.hoverAmount, look.pressAmount);
    if (feedback <= 0.0f)
        return;
    const ImVec4 tint = look.pressAmount > 0.0f ? Mix(c.fill[2], c.fill[1], std::min(look.pressAmount, 1.0f)) : c.fill[2];
    FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(tint, tint.w * std::min(feedback, 1.0f))), radius);
}

void ListRowGroup::Begin(const ForcedItemRequest* forcedRequest)
{
    window = ImGui::GetCurrentContext() != nullptr ? GImGui->CurrentWindow : nullptr;
    forced = forcedRequest;
    hasRing = false;
    ringFromHighlight = false;
    ring = {};
}

ListRowState ListRowGroup::Row(ImGuiID id, const ImRect& rect, const ListRowDesc& desc)
{
    if (window == nullptr || GImGui->CurrentWindow != window)
    {
        ReportUsageError("ListRowGroup::Row outside its Begin and End, or in another window");
        return {};
    }
    static const ForcedItemRequest kNoForce;
    RowEnv env;
    env.window = window;
    env.multi = desc.selection != ImGuiSelectionUserData_Invalid;
    env.forced = forced != nullptr ? forced : &kNoForce;
    env.ring = &ring;
    env.hasRing = &hasRing;
    env.ringFromHighlight = &ringFromHighlight;
    RowPlacement placement;
    placement.inPlace = desc.inPlace;
    placement.allowOverlap = desc.allowOverlap;
    placement.ownRing = desc.ownRing;
    placement.tile = desc.tile;
    return RunRow(env, id, rect, desc.selection, desc.selected, desc.disabled, desc.tabStop, desc.part, desc.indent,
                  desc.highlighted, placement);
}

void ListRowGroup::End()
{
    // A highlighted row (ListRowDesc::highlighted) shows the ring only while the keyboard modality
    // is on; the container moves that row with the keys, the pointer moves only its fill.
    const bool ringShown = !ringFromHighlight || KeyboardModality();
    if (window != nullptr && hasRing && ringShown && GImGui->CurrentWindow == window)
        DrawItemFocusRingInside(ring, Px(T().radii.control));
    window = nullptr;
    hasRing = false;
    ringFromHighlight = false;
}

int ListJumpToEnd(const ListJumpDesc& desc, bool end, bool* changed)
{
    if (changed != nullptr)
        *changed = false;
    if (ImGui::GetCurrentContext() == nullptr)
        return -1;
    const auto disabledAt = [&](int index) {
        ListEntryInfo info;
        if (desc.entry != nullptr)
            desc.entry(desc.user, index, &info);
        return info.disabled;
    };
    const int step = end ? -1 : 1;
    int target = end ? desc.rowCount - 1 : 0;
    while (target >= 0 && target < desc.rowCount && disabledAt(target))
        target += step;
    if (target < 0 || target >= desc.rowCount)
        return -1;
    ListSelection* selection = desc.selection;
    if (selection == nullptr || desc.readOnly)
        return target;
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyShift || desc.single)
    {
        const bool moved = SelectOnly(selection, selection->GetStorageIdFromIndex(target), desc.focusScope, target);
        if (changed != nullptr)
            *changed = moved;
        return target;
    }
    ImGuiMultiSelectState* multi = GImGui->MultiSelectStorage.GetByKey(desc.focusScope);
    ImS64 anchor = target;
    if (multi != nullptr && multi->RangeSrcItem >= 0 && multi->RangeSrcItem < desc.rowCount)
        anchor = multi->RangeSrcItem;
    else if (desc.focusIndex >= 0 && desc.focusIndex < desc.rowCount)
        anchor = desc.focusIndex;
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
    const std::uint64_t before = ListSelectionFingerprint(*selection);
    ApplyListRequests(selection, &requests, desc.rowCount, desc.entry, desc.user);
    if (changed != nullptr)
        *changed = ListSelectionFingerprint(*selection) != before;
    if (multi != nullptr)
    {
        multi->RangeSrcItem = anchor;
        multi->NavIdItem = target;
    }
    return target;
}

void ListFocusRow(ImGuiWindow* window, ImGuiID id, ImGuiID focusScope, const ImRect& rect)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr || id == 0)
        return;
    FocusRowAt(window, id, focusScope, rect);
}

bool ListTabProxy(ImGuiID listId, const ImRect& rect)
{
    if (ImGui::GetCurrentContext() == nullptr || GImGui->CurrentWindow == nullptr || listId == 0)
        return false;
    return SubmitTabProxy(TabProxyId(listId), rect);
}

void ListClaimTabExit(ImGuiWindow* window, ImGuiID owner, ImGuiID listId, ImGuiID outerScope, const ImRect& rect)
{
    if (window == nullptr || ImGui::GetCurrentContext() == nullptr || listId == 0 || owner == 0)
        return;
    ClaimTabExit(window, owner, TabProxyId(listId), outerScope, rect);
}

void ListRangeAnchor::Remap(ListSelection* selection, ImGuiID focusScope, int rowCount)
{
    // Dear ImGui keeps the start of a Shift range as an index. When the caller sorted or filtered
    // since the last frame, the index points at another entry: move it to the entry it was set on,
    // found by its storage id (one scan, only on the frame the order changed), or drop it when the
    // entry is gone.
    if (selection == nullptr || ImGui::GetCurrentContext() == nullptr)
        return;
    ImGuiMultiSelectState* multi = GImGui->MultiSelectStorage.GetByKey(focusScope);
    if (multi == nullptr || !known || multi->RangeSrcItem < 0)
        return;
    const ImS64 index = multi->RangeSrcItem;
    if (index < rowCount && selection->GetStorageIdFromIndex(static_cast<int>(index)) == id)
        return;
    ImGuiSelectionUserData found = ImGuiSelectionUserData_Invalid;
    for (int i = 0; i < rowCount; ++i)
    {
        if (selection->GetStorageIdFromIndex(i) == id)
        {
            found = i;
            break;
        }
    }
    multi->RangeSrcItem = found;
    if (multi->NavIdItem == index)
        multi->NavIdItem = found;
}

void ListRangeAnchor::Record(ListSelection* selection, ImGuiID focusScope, int rowCount)
{
    if (selection == nullptr || ImGui::GetCurrentContext() == nullptr)
    {
        known = false;
        return;
    }
    const ImGuiMultiSelectState* multi = GImGui->MultiSelectStorage.GetByKey(focusScope);
    known = multi != nullptr && multi->RangeSrcItem >= 0 && multi->RangeSrcItem < rowCount;
    if (known)
        id = selection->GetStorageIdFromIndex(static_cast<int>(multi->RangeSrcItem));
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

    if (desc.highlight != nullptr)
    {
        frame.driven = true;
        if (*desc.highlight < -1 || *desc.highlight >= desc.rowCount)
            *desc.highlight = -1;
        // Who put the current row there: a list opened anew starts from the modality of its
        // opening (Enter or Alt+Down show the ring at once, a click does not); a row the driving
        // item moved since the last frame (typeahead, a filter) counts as moved by the keys.
        const int frameCount = ImGui::GetFrameCount();
        if (state.shownFrame != frameCount - 1)
            state.highlightByKeys = KeyboardModality();
        else if (*desc.highlight != state.lastHighlight)
            state.highlightByKeys = true;
        state.shownFrame = frameCount;
        if (desc.keyOwner != 0)
            DriveHighlight(frame);
    }
    // The tab stop of a list whose rows are in a selection scope of their own (see "Tab").
    bool enterByTab = false;
    if ((frame.multi || frame.choiceScope) && !frame.driven)
    {
        // Submitted only while no row has the focus, so the arrows between rows never land on it.
        const ImGuiID proxyId = TabProxyId(id);
        const bool rowFocused = g.NavId != 0 && g.NavFocusScopeId == frame.focusScope;
        if (!rowFocused)
            enterByTab = SubmitTabProxy(proxyId, window->InnerRect);
        else if (frame.listFocused)
            ClaimTabExit(window, id, proxyId, g.CurrentFocusScopeId, window->InnerRect);
    }
    if (frame.listFocused && !frame.driven && desc.entry != nullptr && UpdateTypeahead(state.typed))
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
    if (enterByTab)
    {
        // The focus moves on from the tab stop to the row Tab lands on; the selection stays.
        const int target = frame.tabStop >= 0 ? frame.tabStop : EnabledFrom(desc, 0, 1);
        if (target >= 0)
        {
            const ImGuiID rowId = RowId(frame, KeyOf(desc, EntryOf(desc, target), target));
            const ImRect rect = RowRect(frame, target);
            FocusRowAt(window, rowId, frame.focusScope, rect);
            state.focusIndex = target;
            state.focusedId = rowId;
            frame.tabStop = target;
            frame.listFocused = true;
            ScrollAreaRevealRect(window, rect, ScrollAlign::Nearest, false);
        }
    }
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
    env.pill = frame.pillOpen ? &frame.pill : nullptr;
    env.forced = &frame.forced;
    env.ring = &frame.ring;
    env.hasRing = &frame.hasRing;
    env.ringFromHighlight = &frame.ringFromHighlight;
    const bool highlighted = frame.driven && *desc.highlight == index;
    ListRowState row =
        RunRow(env, id, RowRect(frame, index), index, selected, disabled, tabStop, index, 0.0f, highlighted);
    if (row.focused)
    {
        frame.state->focusIndex = index;
        frame.state->focusedId = id;
    }
    if (frame.driven && !row.disabled)
    {
        // The pointer moving over a row makes it the active one; a click activates it.
        const ImGuiIO& io = ImGui::GetIO();
        if (row.item.hovered && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
        {
            *desc.highlight = index;
            frame.state->highlightByKeys = false;
        }
        if (row.item.pressed && !row.item.keyboardPressed)
            ActivateHighlight(frame, index);
        // A click already activates a row of a driven list, so a double click is two activations
        // (one per click), not a third one on its second press.
        row.activated = frame.activated == index;
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
    // The current row of a driven list shows the ring only while the keys put it there: a row the
    // pointer moved to shows its fill, never the ring.
    const bool ringShown = !frame.ringFromHighlight || (frame.state->highlightByKeys && KeyboardModality());
    if (frame.hasRing && ringShown)
        DrawItemFocusRingInside(frame.ring, radius);
    if (frame.driven)
        frame.state->lastHighlight = *frame.desc.highlight;
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
        // A tree that fits its height keeps the room of one row for the state.
        ImRect room = window->InnerRect;
        if (desc.fitHeight)
        {
            const ImVec2 at = window->DC.CursorPos;
            room = ImRect(at, ImVec2(std::max(at.x + 1.0f, window->WorkRect.Max.x), at.y + ListRowHeight(desc.size)));
            ImGui::Dummy(room.GetSize());
        }
        DrawListState(room, desc.state, desc.emptyText, desc.errorText);
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
        // The tab stop in the surrounding scope (see "Tab"); the node Tab lands on takes the focus
        // when it is submitted.
        const ImGuiID proxyId = TabProxyId(id);
        const bool nodeFocused = g.NavId != 0 && g.NavFocusScopeId == frame.scope;
        if (!nodeFocused)
            frame.enterByTab = SubmitTabProxy(proxyId, window->InnerRect);
        else if (frame.listFocused)
            ClaimTabExit(window, id, proxyId, g.CurrentFocusScopeId, window->InnerRect);
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
    if (frame.enterByTab && tabStop)
    {
        frame.enterByTab = false;
        FocusRowAt(window, id, frame.scope, rect);
        state.focusedId = id;
        frame.listFocused = true;
        ScrollAreaRevealRect(window, rect, ScrollAlign::Nearest, false);
    }

    RowEnv env;
    env.window = window;
    env.multi = frame.multi;
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
    // An empty tree shows the empty state in the room of one row, so a tree that fits its height
    // still has room for it.
    if (frame.position == 0)
    {
        const ImRect row = TreeRowRect(frame, 0);
        const ImRect room = frame.desc.fitHeight ? row : frame.window->InnerRect;
        DrawListState(room, DataState::Ready, frame.desc.emptyText, nullptr);
    }
    const float radius = Px(T().radii.control);
    if (frame.hasRing)
        DrawItemFocusRingInside(frame.ring, radius);
    // The content ends below the last row (the loading rows are not items), or below the room of
    // the empty state.
    frame.window->DC.CursorPos = ImVec2(frame.left, TreeRowRect(frame, std::max(frame.position, 1)).Min.y);
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

void ListCoreNewFrame()
{
    TabExit& exit = PendingTabExit();
    if (!exit.pending)
        return;
    exit.pending = false;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr)
        return;
    ImGuiContext& g = *context;
    // Dear ImGui's own tabbing from the list's tab stop, as if Tab had been pressed there: the
    // items of this frame are scored and the result applies at the next ImGui::NewFrame.
    if (g.NavWindow == nullptr || g.NavId != exit.proxy || g.NavMoveSubmitted)
        return;
    g.NavTabbingDir = exit.dir;
    ImGui::NavMoveRequestSubmit(ImGuiDir_None, exit.dir < 0 ? ImGuiDir_Up : ImGuiDir_Down,
                                ImGuiNavMoveFlags_IsTabbing | ImGuiNavMoveFlags_Activate,
                                ImGuiScrollFlags_KeepVisibleEdgeX | ImGuiScrollFlags_KeepVisibleEdgeY);
    g.NavTabbingCounter = -1;
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
    PendingTabExit() = TabExit();
    ListRowsShutdown();
    Views().depth = 0;
    Trees().depth = 0;
}

} // namespace facet::internal
