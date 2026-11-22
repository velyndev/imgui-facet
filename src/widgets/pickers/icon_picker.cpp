// IconPicker: a field showing an icon and its name that opens a searchable grid of icons in a
// popover.
//
// The field is the field of the popup lists (widgets/lists/list_parts.h, ListsField): the icon, the
// name, the chevron; a click, Enter, Space, Down or Alt+Down opens. The popover holds a SearchField
// (inputs.h) over the tile grid of icon_picker_parts.h: the recently picked icons first under
// Strings::recent, then every icon, in sections by ChoiceItem::group. Typing filters by the fuzzy
// matcher on the names; Down in the search field moves into the grid, where the arrows move in two
// dimensions and Enter, Space or a click picks, writes and closes. Only the visible lines of the grid
// are submitted, so a set of any size costs the same per frame; the tiles are rebuilt only when the
// filter, the item count or the columns change.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"
#include "widgets/pickers/icon_picker_parts.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

constexpr int kIconQuerySize = 64;
constexpr int kIconRecentCapacity = 8;
// Lines of tiles the popover shows at most, and columns it offers at least without `columns`.
constexpr int kIconVisibleLines = 6;
constexpr int kIconMinColumns = 6;
// The glyph takes this share of a tile's side.
constexpr float kIconGlyphShare = 0.5f;

// The icons offered: every registered icon, or the caller's items.
struct IconSource
{
    std::span<const ChoiceItem> items;
    bool registry = false;
    int count = 0;

    Icon IconAt(int index) const
    {
        if (registry)
        {
            const char* name = IconNameAt(index);
            return name != nullptr ? Icon(name) : Icon();
        }
        return items[static_cast<std::size_t>(index)].icon;
    }
    const char* NameAt(int index) const
    {
        if (registry)
        {
            const char* name = IconNameAt(index);
            return name != nullptr ? name : "";
        }
        const char* label = items[static_cast<std::size_t>(index)].label;
        return label != nullptr ? label : "";
    }
    const char* GroupAt(int index) const
    {
        return registry ? nullptr : items[static_cast<std::size_t>(index)].group;
    }
    bool DisabledAt(int index) const
    {
        return !registry && items[static_cast<std::size_t>(index)].disabled;
    }
    int IndexOf(Icon icon) const
    {
        if (!icon.IsValid())
            return -1;
        for (int i = 0; i < count; ++i)
            if (IconAt(i) == icon)
                return i;
        return -1;
    }
};

const char* IconSourceName(int index, void* user)
{
    return static_cast<const IconSource*>(user)->NameAt(index);
}

struct IconPickerState
{
    char query[kIconQuerySize] = {};
    Icon recent[kIconRecentCapacity] = {};
    int recentCount = 0;
    bool mixedCleared = false;
    bool focusGrid = false;          // move the focus from the search field into the grid next frame
    bool opened = false;             // the popover opened and its grid was not drawn yet
    // The tiles: item indices, the recent ones first, built from these inputs.
    ImVector<int> tiles;
    int recentTiles = 0;
    ImVector<internal::FuzzyResult> matches;
    internal::PickerGridRows rows;
    char builtQuery[kIconQuerySize] = {};
    int builtCount = -1;
    int builtColumns = -1;
    int builtRecent = -1;
    const void* builtSource = nullptr;
    // The item of the current icon and its tile, found once per change instead of every frame.
    Icon chosenIcon;
    int chosenItem = -1;
    int chosenTile = -1;
    int chosenCount = -1;
    const void* chosenSource = nullptr;
    bool chosenTileValid = false;
};

// The item index of `icon`, looked up only when the icon or the items changed.
int IconChosenItem(IconPickerState& state, const IconSource& source, Icon icon)
{
    const void* key = source.registry ? nullptr : source.items.data();
    if (!(state.chosenIcon == icon) || state.chosenCount != source.count || state.chosenSource != key)
    {
        state.chosenIcon = icon;
        state.chosenCount = source.count;
        state.chosenSource = key;
        state.chosenItem = source.IndexOf(icon);
        state.chosenTileValid = false;
    }
    return state.chosenItem;
}

// What the grid callbacks see.
struct IconGridContext
{
    const IconSource* source = nullptr;
    const IconPickerState* state = nullptr;
    ControlSize size = ControlSize::Inherit;
};

const char* IconGridGroup(void* user, int tile)
{
    const IconGridContext& c = *static_cast<const IconGridContext*>(user);
    if (tile < c.state->recentTiles)
        return GetStrings().recent;
    return c.source->GroupAt(c.state->tiles[tile]);
}

void IconGridInfo(void* user, int tile, internal::PickerGridTileInfo* out)
{
    const IconGridContext& c = *static_cast<const IconGridContext*>(user);
    const int item = c.state->tiles[tile];
    // Recent tiles get ids of their own, so an icon shown twice is two items.
    out->id = static_cast<ImGuiID>(item) * 2u + (tile < c.state->recentTiles ? 1u : 0u) + 1u;
    out->disabled = c.source->DisabledAt(item);
}

void IconGridDraw(void* user, const internal::PickerGridTile& tile)
{
    using namespace internal;
    const IconGridContext& c = *static_cast<const IconGridContext*>(user);
    const int item = c.state->tiles[tile.index];
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (tile.item.visible)
    {
        const float side = std::round(tile.rect.GetWidth() * kIconGlyphShare);
        const ImVec2 at(std::round(tile.rect.GetCenter().x - side * 0.5f), std::round(tile.rect.GetCenter().y - side * 0.5f));
        // The chosen icon shows by the selected tile look the grid draws (icon_picker_parts.cpp).
        DrawIcon(drawList, c.source->IconAt(item), at, side, ToU32(ControlLabelColor(tile.disabled)));
    }
    // The name of the icon on hover and keyboard focus.
    if (BeginPopupTooltip(tile.item.id, tile.rect, tile.item.hovered, tile.item.focusVisible))
    {
        facet::Text(TextStyle::Footnote, "%s", c.source->NameAt(item));
        EndPopupTooltip();
    }
}

// Rebuilds the tiles when the filter, the count, the columns or the recent icons changed.
void IconBuildTiles(IconPickerState& state, const IconSource& source, int columns, bool showRecent)
{
    const int recent = showRecent ? state.recentCount : 0;
    if (state.builtCount == source.count && state.builtColumns == columns && state.builtRecent == recent &&
        state.builtSource == (source.registry ? nullptr : source.items.data()) &&
        std::strcmp(state.builtQuery, state.query) == 0)
        return;
    state.tiles.resize(0);
    state.recentTiles = 0;
    if (state.query[0] == '\0')
    {
        for (int i = 0; i < recent; ++i)
        {
            const int index = source.IndexOf(state.recent[i]);
            if (index >= 0)
                state.tiles.push_back(index);
        }
        state.recentTiles = state.tiles.Size;
        for (int i = 0; i < source.count; ++i)
            state.tiles.push_back(i);
    }
    else
    {
        state.matches.resize(std::max(1, source.count));
        const int found = internal::FuzzyFilter(state.query, source.count, &IconSourceName, const_cast<IconSource*>(&source),
                                                std::span(state.matches.Data, static_cast<std::size_t>(state.matches.Size)));
        for (int i = 0; i < found; ++i)
            state.tiles.push_back(state.matches[i].index);
        // With sections, matches keep the item order so each stays under its group.
        bool grouped = false;
        for (int i = 0; i < source.count && !grouped; ++i)
            grouped = source.GroupAt(i) != nullptr && source.GroupAt(i)[0] != '\0';
        if (grouped)
            std::sort(state.tiles.begin(), state.tiles.end());
    }
    IconGridContext context{&source, &state};
    internal::PickerGridBuildRows(&state.rows, state.tiles.Size, columns, &IconGridGroup, &context);
    std::memcpy(state.builtQuery, state.query, sizeof(state.builtQuery));
    state.builtCount = source.count;
    state.builtColumns = columns;
    state.builtRecent = recent;
    state.builtSource = source.registry ? nullptr : source.items.data();
    state.chosenTileValid = false;
}

bool IconPickerImpl(const char* label, Icon* icon, const IconSource& source, const IconPickerOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    const ImGuiID level = ImHashStr("##icons", 0, id);
    IconPickerState& state = GetState<IconPickerState>(id);
    const Strings& strings = GetStrings();
    const bool readOnly = options.readOnly || icon == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool mixedFlag = options.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;
    if (!mixedFlag)
        state.mixedCleared = false;
    const bool mixed = mixedFlag && !state.mixedCleared;
    const bool open = IsPopupLevelOpen(level);

    // The field: the icon and its name, the placeholder, or Strings::mixed.
    const Icon current = icon != nullptr ? *icon : Icon();
    ListsFieldDesc field;
    field.size = options.size;
    field.width = options.width;
    field.next = &next;
    field.forced = &forced;
    if (mixed)
    {
        field.text = strings.mixed;
        field.secondaryText = true;
    }
    else if (current.IsValid())
    {
        const int index = IconChosenItem(state, source, current);
        const char* name = index >= 0 ? source.NameAt(index) : IconName(current);
        field.icon = current;
        field.text = name != nullptr ? name : "";
        field.textEnd = ImGui::FindRenderedTextEnd(field.text);
    }
    else
    {
        field.text = options.placeholder != nullptr ? options.placeholder : strings.select;
        field.secondaryText = true;
    }
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

    // The popover: width for the columns, at least the field's.
    const ControlMetrics tileMetrics = GetControlMetrics(ControlSize::Large);
    const float tileSide = tileMetrics.height;
    const float gap = PxSnapped(t.spacing.xs);
    const float step = tileSide + gap;
    const float pad = FocusRingOutset();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float room = std::max(step, viewport->WorkSize.x - Px(t.overlay.viewportMargin) * 2.0f - Px(t.spacing.xs) * 2.0f - pad * 2.0f);
    int columns = options.columns;
    if (columns <= 0)
    {
        const float wanted = std::max(result.frame.GetWidth() - pad * 2.0f, kIconMinColumns * step - gap);
        columns = static_cast<int>((std::min(wanted, room) + gap) / step);
    }
    columns = std::max(1, std::min(columns, static_cast<int>((room + gap) / step)));
    const float gridWidth = columns * step - gap;

    const PopupAnchor anchor = PanelAnchor(result.frame);
    PopupOptions popup;
    if (openNow)
    {
        state.query[0] = '\0';
        state.focusGrid = false;
        state.opened = true;
        OpenPopupLevel(level, anchor, popup);
    }
    if (item.open)
        ForcePopupLevelOpen(level, anchor, popup);
    SetPopupLevelAnchor(level, anchor);

    bool written = false;
    if (BeginPopupLevel(level))
    {
        const bool live = !item.open;
        IconBuildTiles(state, source, columns, options.showRecent);
        // A grid with more lines than the area shows scrolls: the popover is wider by the gutter of
        // the bar then, so the tiles keep their columns beside it.
        const int gridLines = (source.count + columns - 1) / columns + (state.recentTiles > 0 ? 2 : 0);
        const float barRoom = gridLines > kIconVisibleLines ? std::max(0.0f, ScrollBarGutter() - pad) : 0.0f;
        const float areaWidth = gridWidth + pad * 2.0f + barRoom;
        if (options.searchable)
        {
            // Typing goes to the search field from the moment the popover opens.
            if (state.opened && live)
                ImGui::SetKeyboardFocusHere();
            SearchField("##search", state.query, sizeof(state.query), {.width = areaWidth / Scale()});
            const ImGuiID searchId = ImGui::GetItemID();
            if (live && ImGui::IsItemFocused())
            {
                // Down moves from the search field into the grid.
                ImGui::SetKeyOwner(ImGuiKey_DownArrow, searchId);
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, searchId))
                {
                    // The typing ends; the keys go to the grid.
                    if (ImGui::GetActiveID() == searchId)
                        ImGui::ClearActiveID();
                    state.focusGrid = true;
                }
            }
            IconBuildTiles(state, source, columns, options.showRecent);
        }
        // The grid area keeps the height of the unfiltered grid, so typing does not resize the popover.
        const int allLines = (source.count + columns - 1) / columns + (state.recentTiles > 0 ? 2 : 0);
        const int lines = std::clamp(allLines, 1, kIconVisibleLines);
        const ImVec2 areaSize(areaWidth, lines * step - gap + pad * 2.0f);
        if (state.tiles.Size == 0)
        {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Dummy(areaSize);
            DrawListState(ImRect(at, ImVec2(at.x + areaSize.x, at.y + areaSize.y)), DataState::Ready,
                          source.count == 0 ? strings.empty : strings.noResults, nullptr);
        }
        else if (BeginScrollAreaCore(ImHashStr("##grid", 0, level), {.size = areaSize, .fadeSurface = t.colors.bgElevated}))
        {
            const int chosenItem = IconChosenItem(state, source, current);
            if (!state.chosenTileValid)
            {
                state.chosenTile = -1;
                for (int i = state.recentTiles; i < state.tiles.Size && state.chosenTile < 0 && chosenItem >= 0; ++i)
                    if (state.tiles[i] == chosenItem)
                        state.chosenTile = i;
                state.chosenTileValid = true;
            }
            const int chosenTile = state.chosenTile;
            IconGridContext context{&source, &state, options.size};
            PickerGridDesc grid;
            grid.rows = std::span(state.rows.rows.Data, static_cast<std::size_t>(state.rows.rows.Size));
            grid.tileCount = state.tiles.Size;
            grid.columns = columns;
            grid.tile = ImVec2(tileSide, tileSide);
            grid.gap = gap;
            grid.width = gridWidth;
            grid.chosen = chosenTile;
            grid.info = &IconGridInfo;
            grid.draw = &IconGridDraw;
            grid.user = &context;
            grid.readOnly = readOnly;
            grid.live = live;
            grid.reveal = state.opened || item.open ? chosenTile : -1;
            grid.takeFocus = state.focusGrid && live;
            grid.defaultFocus = !options.searchable;
            grid.resetFocus = state.opened;
            state.focusGrid = false;
            state.opened = false;
            const PickerGridResult picked = PickerGrid(ImHashStr("##tiles", 0, level), grid);
            EndScrollAreaCore();
            if (picked.activated >= 0 && icon != nullptr && !readOnly)
            {
                const Icon chosen = source.IconAt(state.tiles[picked.activated]);
                if (!(chosen == *icon))
                {
                    *icon = chosen;
                    written = true;
                }
                state.mixedCleared = true;
                // The recent icons, newest first, each once.
                int keep = 0;
                Icon recent[kIconRecentCapacity] = {chosen};
                for (int i = 0; i < state.recentCount && keep + 1 < kIconRecentCapacity; ++i)
                    if (!(state.recent[i] == chosen))
                        recent[++keep] = state.recent[i];
                std::memcpy(state.recent, recent, sizeof(recent));
                state.recentCount = keep + 1;
                if (picked.byKey)
                    ListsCloseByKey(level);
                else
                    ClosePopupLevel(level);
            }
        }
        EndPopupLevel();
    }
    if (written)
        ListsMarkEdited();
    return written;
}

} // namespace

bool IconPicker(const char* label, Icon* icon, const IconPickerOptions& options)
{
    IconSource source;
    source.registry = true;
    source.count = IconCount();
    return IconPickerImpl(label, icon, source, options);
}

bool IconPicker(const char* label, Icon* icon, std::span<const ChoiceItem> icons, const IconPickerOptions& options)
{
    IconSource source;
    source.items = icons;
    source.count = static_cast<int>(icons.size());
    return IconPickerImpl(label, icon, source, options);
}

} // namespace facet
