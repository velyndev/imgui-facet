// CommandPalette (overlays.h): a floating launcher with a search field over the app's commands or
// the registered shortcuts.
//
// The palette is a Popup level of the popup host (popup_host.h), centered overlay.commandPaletteTop
// below the top of the display: Escape and a click outside close it, the focus moves in when it opens
// and returns to the item that had it when it closes. Inside, a frameless search field of the text
// field core (text_field.h) drives a list view of the list core (list_view.h): the field keeps the
// keyboard focus and hands the list its keys (completionPopup, keyOwner), so typing filters, Up and
// Down move the active row, Page Up and Down move by a page, and Enter or Tab picks it. A click on a
// row picks it; a click anywhere else in the palette gives the focus back to the field.
//
// Rows: while the field is empty, the entries in their order with a header before each run of equal
// ChoiceItem::group, and with recentFirst the recent picks first under Strings::recent; while it has
// text, the fuzzy matches (fuzzy.h) best first, their matched characters highlighted. Each row shows
// the icon, the label and ChoiceItem::detail at the end (list_parts.h); a shortcut row shows its chord
// as key caps instead. The list is as tall as its rows, maxVisibleRows at most, and shows emptyText
// when nothing matches. Recent picks are kept under the id in the ini file, by the label (or the
// shortcut name), so they survive a reorder of the entries.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/fuzzy.h"
#include "internal/key_caps.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_util.h"
#include "widgets/lists/list_parts.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

constexpr int kPaletteRecentCount = 5;
constexpr int kPaletteQuerySize = 128;
// The row value of the Strings::recent header.
constexpr int kPaletteRecentHeader = -0x7FFFFFFF;

// Recent picks, newest first, by the hash of their label or shortcut name; saved in the ini file.
struct PaletteRecent
{
    static constexpr const char* kStateName = "CommandPalette";
    ImGuiID keys[kPaletteRecentCount] = {};
    int count = 0;

    void WriteIni(ImGuiTextBuffer& out) const
    {
        for (int i = 0; i < count; ++i)
            out.appendf("recent=%08X\n", keys[i]);
    }
    void ReadIni(const char* key, const char* value)
    {
        if (std::strcmp(key, "recent") == 0 && count < kPaletteRecentCount)
            keys[count++] = static_cast<ImGuiID>(std::strtoul(value, nullptr, 16));
    }
    void Add(ImGuiID key)
    {
        int at = count < kPaletteRecentCount ? count : kPaletteRecentCount - 1;
        for (int i = 0; i < count; ++i)
        {
            if (keys[i] == key)
            {
                at = i;
                break;
            }
        }
        for (int i = at; i > 0; --i)
            keys[i] = keys[i - 1];
        keys[0] = key;
        if (at == count && count < kPaletteRecentCount)
            ++count;
    }
};

struct PaletteState
{
    char query[kPaletteQuerySize] = {};
    ImVector<int> rows;                   // an item index, -1 - index for a group header, or kPaletteRecentHeader
    ImVector<internal::FuzzyResult> matches;
    int highlight = -1;
    int builtCount = -1;
    bool built = false;
};

// What the palette lists: one of the item shapes, and how a row is drawn.
struct PaletteSource
{
    internal::ListsSource items;
    bool shortcuts = false;  // the entries are the enabled shortcuts of the registry, by registry index
};

// A query of spaces only filters nothing, as the fuzzy block treats it.
bool PaletteQueryBlank(const char* text)
{
    for (; *text != '\0'; ++text)
    {
        if (*text != ' ')
            return false;
    }
    return true;
}

bool ShortcutItem(void*, int index, ChoiceItem* out)
{
    ShortcutInfo info;
    if (!GetShortcut(index, &info) || !info.enabled)
        return false;
    out->label = info.label;
    out->group = info.group[0] != '\0' ? info.group : nullptr;
    return true;
}

ImGuiID RecentKey(const PaletteSource& source, int index)
{
    if (source.shortcuts)
    {
        ShortcutInfo info;
        return GetShortcut(index, &info) ? ImHashStr(info.name) : 0;
    }
    ChoiceItem item;
    if (!source.items.Get(index, &item))
        return 0;
    return ImHashStr(item.label, static_cast<std::size_t>(ImGui::FindRenderedTextEnd(item.label) - item.label));
}

struct PaletteEntryContext
{
    const PaletteSource* source = nullptr;
    const PaletteState* state = nullptr;
};

void PaletteEntry(void* user, int row, internal::ListEntryInfo* out)
{
    const PaletteEntryContext& context = *static_cast<const PaletteEntryContext*>(user);
    const int value = context.state->rows[row];
    if (value < 0)
    {
        out->disabled = true;  // headers are not choices
        out->id = static_cast<ImGuiID>(row) | 0x80000000u;
        return;
    }
    ChoiceItem item;
    if (!context.source->items.Get(value, &item))
    {
        out->disabled = true;
        return;
    }
    out->label = item.label;
    out->disabled = item.disabled;
    out->id = item.id != 0 ? item.id : static_cast<ImGuiID>(value) + 1u;
}

const char* PaletteLabel(int index, void* user)
{
    ChoiceItem item;
    if (!static_cast<const PaletteSource*>(user)->items.Get(index, &item))
        return nullptr;
    return item.label;
}

void BuildRows(PaletteState& state, const PaletteSource& source, const PaletteRecent& recent, bool recentFirst)
{
    using namespace internal;
    state.rows.resize(0);
    const int count = source.items.count;
    if (!PaletteQueryBlank(state.query))
    {
        if (state.matches.Size < count)
            state.matches.resize(count);
        const int shown = FuzzyFilter(state.query, count, &PaletteLabel, const_cast<PaletteSource*>(&source),
                                      std::span<FuzzyResult>(state.matches.Data, static_cast<std::size_t>(count)));
        for (int i = 0; i < shown; ++i)
            state.rows.push_back(state.matches[i].index);
        return;
    }
    // Recent picks first, newest first; the rest keep their order and groups.
    int recentShown = 0;
    if (recentFirst && recent.count > 0)
    {
        for (int r = 0; r < recent.count; ++r)
        {
            for (int i = 0; i < count; ++i)
            {
                ChoiceItem item;
                if (source.items.Get(i, &item) && RecentKey(source, i) == recent.keys[r])
                {
                    if (recentShown == 0)
                        state.rows.push_back(kPaletteRecentHeader);
                    state.rows.push_back(i);
                    ++recentShown;
                    break;
                }
            }
        }
    }
    const char* group = nullptr;
    for (int i = 0; i < count; ++i)
    {
        ChoiceItem item;
        if (!source.items.Get(i, &item))
            continue;
        if (recentShown > 0)
        {
            bool isRecent = false;
            const ImGuiID key = RecentKey(source, i);
            for (int r = 0; r < recent.count && !isRecent; ++r)
                isRecent = recent.keys[r] == key;
            if (isRecent)
                continue;
        }
        const char* itemGroup = item.group != nullptr && item.group[0] != '\0' ? item.group : nullptr;
        if (itemGroup != nullptr && (group == nullptr || std::strcmp(group, itemGroup) != 0))
            state.rows.push_back(-1 - i);
        group = itemGroup;
        // A run without a group after a grouped run starts without a header.
        state.rows.push_back(i);
    }
}

int FirstEnabledRow(const PaletteSource& source, const PaletteState& state)
{
    for (int row = 0; row < state.rows.Size; ++row)
    {
        ChoiceItem item;
        if (state.rows[row] >= 0 && source.items.Get(state.rows[row], &item) && !item.disabled)
            return row;
    }
    return -1;
}

// Draws a palette row: the list row content, and for a shortcut its chord as key caps at the end.
void DrawPaletteRow(const internal::ListRowState& row, const PaletteSource& source, int index, const char* query)
{
    using namespace internal;
    ChoiceItem item;
    if (!source.items.Get(index, &item))
        return;
    ListsRowLook look;
    look.query = query;
    ListRowState shown = row;
    if (source.shortcuts)
    {
        ShortcutInfo info;
        if (GetShortcut(index, &info) && info.primary != ImGuiKey_None)
        {
            const float caps = MeasureKeyCaps(info.primary, ControlSize::Small);
            if (row.item.visible)
                DrawKeyCaps(ImGui::GetWindowDrawList(), info.primary,
                            ImVec2(row.content.Max.x - caps, row.content.GetCenter().y), ControlSize::Small,
                            row.disabled);
            shown.content.Max.x = std::max(shown.content.Min.x, shown.content.Max.x - caps - Px(T().spacing.sm));
        }
    }
    DrawListsRow(shown, item, look, ControlSize::Regular);
}

bool PaletteImpl(const char* label, bool* open, const PaletteSource& source, const CommandPaletteOptions& options,
                 bool canPick, int* pickedIndex)
{
    using namespace internal;
    *pickedIndex = -1;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr || open == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& theme = T();
    const Strings& strings = GetStrings();
    const ImGuiID level = window->GetID(label);
    PaletteState& state = GetState<PaletteState>(level);
    PaletteRecent& recent = GetPersistentState<PaletteRecent>(level);

    if (!*open && options.openShortcut.IsValid() && ShortcutPressed(options.openShortcut))
        *open = true;

    // Geometry, from tokens only, so the level size is known before its content runs.
    const ImRect work = OverlayWorkArea();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = std::min(Px(theme.overlay.commandPaletteWidth), work.GetWidth());
    const float padding = Px(theme.spacing.sm);
    const float inner = std::max(1.0f, width - padding * 2.0f);
    const char* headingEnd = ImGui::FindRenderedTextEnd(label);
    const bool heading = headingEnd != label;
    const float footnote = MeasureText(TextStyle::Footnote, "Ag").y;
    const float headingHeight = heading ? footnote + Px(theme.spacing.xs) : 0.0f;
    const float fieldHeight = ControlHeight(ControlSize::Large);
    const float separatorGap = Px(theme.spacing.xs);
    const float rowHeight = ListRowHeight(ControlSize::Regular);
    const int maxRows = options.maxVisibleRows > 0 ? options.maxVisibleRows : theme.list.popupRows;
    const int visibleRows = std::clamp(state.rows.Size, 1, std::max(1, maxRows));
    const float listHeight = rowHeight * static_cast<float>(visibleRows) + FocusRingOutset() * 2.0f;
    const float top = viewport->WorkPos.y + Px(theme.overlay.commandPaletteTop);
    // The host keeps overlay.anchorGap between the anchor point and the palette.
    const float roomBelow = std::max(rowHeight, work.Max.y - top - Px(theme.overlay.anchorGap) - padding * 2.0f -
                                                    headingHeight - fieldHeight - separatorGap * 2.0f);
    const float listArea = std::min(listHeight, roomBelow);
    const ImVec2 size(width, padding * 2.0f + headingHeight + fieldHeight + separatorGap * 2.0f + listArea);

    PopupAnchor anchor;
    const float centerX = work.GetCenter().x;
    anchor.rect = ImRect(ImVec2(centerX, top), ImVec2(centerX, top));
    anchor.side = PopupSide::Below;
    anchor.align = PopupAlign::Center;
    PopupOptions popup;
    popup.kind = PopupKind::Popup;
    popup.size = size;
    popup.padding = ImVec2(padding, padding);
    const bool forcedOpen = Has(forced.state, ForcedItemState::Open);
    if (forcedOpen)
        ForcePopupLevelOpen(level, anchor, popup);
    const OverlayLayerSync sync = OverlaySyncLayer(level, open, anchor, popup);
    if (sync.opened || (forcedOpen && !state.built))
    {
        state.query[0] = '\0';
        state.built = false;
        state.highlight = -1;
    }
    if (sync.open || forcedOpen)
    {
        SetPopupLevelAnchor(level, anchor);
        SetPopupLevelLayout(level, size, ImRect());
    }
    if (!BeginPopupLevel(level))
        return false;

    ImGuiWindow* palette = GImGui->CurrentWindow;
    ImGui::PushID(level);
    const ImGuiID fieldId = palette->GetID("##facet.query");
    if (sync.opened)
        TextFieldRequestFocus(fieldId, TextFieldFocus::SelectAll);
    const ImVec2 origin = palette->DC.CursorPos;
    float y = origin.y;
    if (heading)
    {
        PushTextStyle(TextStyle::Footnote);
        const FittedText shown = FitText(label, headingEnd, inner);
        palette->DrawList->AddText(ImVec2(origin.x, y), ToU32(theme.colors.labelSecondary), shown.text);
        PopTextStyle();
        y += headingHeight;
    }

    // The search field: frameless, a hairline under it.
    const bool input = !forcedOpen;
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    TextFieldParams params;
    params.placeholder = options.placeholder != nullptr ? options.placeholder : strings.search;
    params.icon = Icon("search");
    params.completionPopup = level;
    params.size = ControlSize::Large;
    params.width = inner / Px(1.0f);
    params.frameless = true;
    const TextFieldResult field = TextFieldBehavior("##facet.query", state.query, sizeof(state.query), params);
    y += fieldHeight + separatorGap;
    HairlineH(palette->DrawList, origin.x, origin.x + inner, y, ToU32(theme.colors.separator));
    y += separatorGap;
    bool closeByKey = input && field.active && ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, level);

    // Rows: rebuilt when the text or the entries change.
    const PaletteEntryContext entries{&source, &state};
    int reveal = -1;
    if (!state.built || field.edited || state.builtCount != source.items.count)
    {
        BuildRows(state, source, recent, options.recentFirst);
        state.highlight = FirstEnabledRow(source, state);
        reveal = std::max(state.highlight, 0);
        state.built = true;
        state.builtCount = source.items.count;
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    ListViewDesc view;
    view.rowCount = state.rows.Size;
    view.size = ControlSize::Regular;
    view.areaSize = ImVec2(inner, listArea);
    view.fadeSurface = theme.colors.ElevatedSurface(2);
    view.entry = &PaletteEntry;
    view.user = const_cast<PaletteEntryContext*>(&entries);
    view.emptyText = options.emptyText != nullptr ? options.emptyText : strings.noResults;
    view.revealRow = reveal;
    view.highlight = &state.highlight;
    view.keyOwner = input && !sync.opened ? level : 0;
    int activated = -1;
    if (BeginListView(palette->GetID("##facet.rows"), view))
    {
        const char* query = PaletteQueryBlank(state.query) ? nullptr : state.query;
        RowRange range;
        while (NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                const ListRowState row = ListViewRow(i);
                const int value = state.rows[i];
                if (value < 0)
                {
                    if (row.item.visible)
                    {
                        if (value == kPaletteRecentHeader)
                        {
                            DrawListsHeader(row.content, strings.recent);
                        }
                        else
                        {
                            ChoiceItem item;
                            source.items.Get(-1 - value, &item);
                            DrawListsHeader(row.content, item.group);
                        }
                    }
                    if (ImGui::IsMouseHoveringRect(row.rect.Min, row.rect.Max) && ImGui::IsWindowHovered())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                    continue;
                }
                DrawPaletteRow(row, source, value, query);
                OverlayRecordRects(OverlayPartKey("row", level) + static_cast<ImGuiID>(value), &row.rect, 1);
            }
        }
        const ListViewResult result = EndListView();
        activated = result.activated;
    }
    const bool byKey = activated >= 0 && !ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    // A click in the palette that is not on the field or a row gives the keyboard back to the field.
    if (input && activated < 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !field.hovered)
        TextFieldRequestFocus(fieldId, TextFieldFocus::End);
    ImGui::PopID();
    EndPopupLevel();

    if (input && activated >= 0 && activated < state.rows.Size && state.rows[activated] >= 0 && canPick)
    {
        const int index = state.rows[activated];
        *pickedIndex = index;
        recent.Add(RecentKey(source, index));
        MarkStateDirty();
        *open = false;
        if (byKey)
            closeByKey = true;
        else
            ClosePopupLevel(level);
    }
    if (closeByKey)
    {
        *open = false;
        ClosePopupLevel(level);
        ImGui::SetNavCursorVisible(true);
        SetKeyboardModality(true);
    }
    return *pickedIndex >= 0;
}

} // namespace

bool CommandPalette(const char* label, bool* open, int* picked, std::span<const ChoiceItem> commands,
                    const CommandPaletteOptions& options)
{
    PaletteSource source;
    source.items = internal::ListsSourceOf(commands);
    int index = -1;
    if (!PaletteImpl(label, open, source, options, picked != nullptr, &index))
        return false;
    *picked = index;
    return true;
}

bool CommandPalette(const char* label, bool* open, int* picked, std::span<const char* const> commands,
                    const CommandPaletteOptions& options)
{
    PaletteSource source;
    source.items = internal::ListsSourceOf(commands);
    int index = -1;
    if (!PaletteImpl(label, open, source, options, picked != nullptr, &index))
        return false;
    *picked = index;
    return true;
}

bool CommandPalette(const char* label, bool* open, ShortcutId* picked, const CommandPaletteOptions& options)
{
    PaletteSource source;
    source.items = internal::ListsSourceOf(ShortcutCount(), &ShortcutItem, nullptr);
    source.shortcuts = true;
    int index = -1;
    if (!PaletteImpl(label, open, source, options, picked != nullptr, &index))
        return false;
    ShortcutInfo info;
    if (!GetShortcut(index, &info))
        return false;
    *picked = info.id;
    TriggerShortcut(info.id);
    return true;
}

} // namespace facet
