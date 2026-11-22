// Demo page: Inventory and cards. Shows the inventory, card and people composites and the public drag
// and drop calls of facet/widgets/composites.h with their variants and states. Every item, profile
// and text on the page is a labelled placeholder; the pictures are test patterns generated in code
// and built-in icons. The page applies every InventoryResult to its own placeholder bag, as an
// application would, so drags, swaps, merges and splits really change what the page shows. The
// search and app controls and the editors have pages of their own (page_w_composites_settings.cpp
// and page_w_composites_editors.cpp).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "selftest.h"

namespace
{

using facet::InventoryAction;
using facet::InventoryItem;
using facet::InventoryResult;
using facet::demo::DemoWidth;
using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Payload type of the public drag and drop example.
constexpr const char* kDemoPayload = "facet.demo.item";
constexpr int kBagSlots = 18;
constexpr int kLargeSlots = 10000;
constexpr int kEquipSlots = 2;
constexpr int kPatternSide = 48;
constexpr int kPatterns = 4;
// Probes on the first tiles of the bag, for the selftest.
constexpr int kProbedTiles = 8;
// The narrow media card and detail card, in design units: below composites.mediaCardHorizontalBelow.
constexpr float kNarrowCardUnits = 180.0f;
// The small tile size of the tile row, in design units.
constexpr float kSmallSlotUnits = 40.0f;

enum CaptureState
{
    kDefault,
    kFocused,
    kDragging,
    kOpen,
    kLoading,
};

const char* const kColumns[] = {"Fit width", "4 columns", "6 columns"};
const char* const kDataStates[] = {"Ready", "Loading", "Error", "Empty"};
const char* const kTags[] = {"Placeholder tag", "Second tag"};
const char* const kActions[] = {"Use", "Drop"};
const char* const kBadges[] = {"Placeholder", "Pattern"};
const facet::AvatarItem kProfiles[] = {
    {"Placeholder profile 1", {}, "Placeholder description"}, {"Placeholder profile 2"}, {"Placeholder profile 3"}};

// The placeholder bag, the equipment slots and the standalone tile, with every result applied.
struct CardsUi
{
    bool built = false;
    InventoryItem bag[kBagSlots];
    InventoryItem equip[kEquipSlots];
    InventoryItem tile;
    int selected = -1;
    int columns = 0;
    int bagState = 0;
    int bagColumns = 1;     // the columns the bag showed in the last frame (for the selftest)
    int actions = 0;
    char lastText[128] = "No action yet";
    int largeSelected = -1;
    int tileActivations = 0;
    bool compare = true;
    int detailAction = -1;
    int detailPresses = 0;
    int mediaPresses = 0;
    bool mediaSelected = false;
    int chipPresses = 0;
    int profile = 0;
    int profileState = 0;
    facet::ProfileSwitcherAction lastProfile = facet::ProfileSwitcherAction::None;
    int profileActions = 0;
    int drops = 0;
    ImGuiID bagId = 0;
    ImGuiID equipIds[kEquipSlots] = {};
    ImGuiID tileId = 0;
};

CardsUi ui;

struct CardsAssets
{
    bool built = false;
    bool dark = false;
    facet::ImageHandle patterns[kPatterns];
    facet::ImageHandle loading;  // recreated every frame in the loading capture
    facet::ImageHandle failed;   // bytes that are no image: the failed state once the decode gave up
};

CardsAssets assets;

InventoryItem LargeItems[kLargeSlots];

bool Is(const facet::demo::PageCtx& ctx, CaptureState state)
{
    return ctx.capture && ctx.captureState == state;
}

// A test pattern: a diagonal gradient between two chart colors with a ring, so the picture of a tile
// shows it is drawn and how it is fitted.
facet::ImageHandle MakePattern(int hue)
{
    const facet::Colors& c = facet::T().colors;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(kPatternSide) * kPatternSide * 4u);
    const ImVec4 from = c.chart[hue % facet::kChartPaletteSize];
    const ImVec4 to = c.chart[(hue + 3) % facet::kChartPaletteSize];
    const float center = 0.5f * kPatternSide;
    for (int y = 0; y < kPatternSide; ++y)
    {
        for (int x = 0; x < kPatternSide; ++x)
        {
            const float t = static_cast<float>(x + y) / static_cast<float>(2 * (kPatternSide - 1));
            ImVec4 color = facet::Mix(from, to, t);
            const float distance = std::hypot(x + 0.5f - center, y + 0.5f - center);
            if (std::fabs(distance - kPatternSide * 0.3f) < kPatternSide * 0.04f)
                color = c.bg;
            const ImU32 packed = facet::ToU32(color);
            unsigned char* p = &pixels[(static_cast<std::size_t>(y) * kPatternSide + x) * 4u];
            p[0] = static_cast<unsigned char>(packed >> IM_COL32_R_SHIFT);
            p[1] = static_cast<unsigned char>(packed >> IM_COL32_G_SHIFT);
            p[2] = static_cast<unsigned char>(packed >> IM_COL32_B_SHIFT);
            p[3] = static_cast<unsigned char>(packed >> IM_COL32_A_SHIFT);
        }
    }
    return facet::CreateImageRGBA(pixels.data(), kPatternSide, kPatternSide);
}

// Bytes that are no image: an asynchronous load of them stays loading for the frame it starts in and
// fails once the worker gave up.
const std::vector<unsigned char>& NotAnImage()
{
    static const std::vector<unsigned char> bytes(static_cast<std::size_t>(kPatternSide) * kPatternSide * 4u, 0x80);
    return bytes;
}

// Builds the patterns for the current theme; again after a theme switch and after a restart of Facet.
// Items of the page that show a pattern take the new handle.
void EnsureAssets(bool dark)
{
    if (assets.built && assets.dark == dark && facet::GetImageStatus(assets.patterns[0]) != facet::ImageStatus::Invalid)
        return;
    facet::ImageHandle old[kPatterns];
    for (int i = 0; i < kPatterns; ++i)
    {
        old[i] = assets.patterns[i];
        facet::ReleaseImage(assets.patterns[i]);
        assets.patterns[i] = MakePattern(i);
    }
    facet::ReleaseImage(assets.failed);
    assets.failed = facet::LoadImageMemoryAsync(NotAnImage().data(), NotAnImage().size());
    const auto remap = [&](InventoryItem& item) {
        for (int i = 0; i < kPatterns; ++i)
            if (item.image.IsValid() && item.image == old[i])
                item.image = assets.patterns[i];
    };
    for (InventoryItem& item : ui.bag)
        remap(item);
    for (InventoryItem& item : ui.equip)
        remap(item);
    remap(ui.tile);
    assets.built = true;
    assets.dark = dark;
}

// In the loading capture an image is asked for again every frame, so it is always still loading.
facet::ImageHandle LoadingImage(const facet::demo::PageCtx& ctx)
{
    if (!Is(ctx, kLoading))
        return assets.patterns[1];
    facet::ReleaseImage(assets.loading);
    assets.loading = facet::LoadImageMemoryAsync(NotAnImage().data(), NotAnImage().size());
    return assets.loading;
}

InventoryItem Placeholder(const char* label, const char* icon, int count = 1, std::uint32_t key = 0, int maxStack = 0)
{
    InventoryItem item;
    item.label = label;
    item.icon = facet::Icon(icon);
    item.count = count;
    item.stackKey = key;
    item.maxStack = maxStack;
    return item;
}

void Reset()
{
    CardsUi fresh;
    ui = fresh;
    const facet::Colors& c = facet::T().colors;
    ui.bag[0] = Placeholder("Item 1", "file", 6, 1, 20);
    ui.bag[1] = Placeholder("Item 2", "folder");
    ui.bag[3] = Placeholder("Item 1", "file", 3, 1, 20);
    ui.bag[4] = Placeholder("Item 3", "image");
    ui.bag[4].image = assets.patterns[0];
    ui.bag[4].tint = facet::ToU32(c.chart[0]);
    ui.bag[5] = Placeholder("Item 4", "folder");
    ui.bag[5].locked = true;
    ui.bag[6] = Placeholder("Item 5 with a title far too long for one slot", "star");
    ui.bag[8] = Placeholder("Item 6", "clock");
    ui.bag[8].cooldown = 0.4f;
    ui.bag[8].tint = facet::ToU32(c.chart[2]);
    ui.bag[9] = Placeholder("Item 7", "heart", 12, 2, 0);
    ui.bag[11] = Placeholder("Item 8", "image");
    ui.bag[11].image = assets.patterns[2];
    ui.tile = Placeholder("Placeholder tile", "star", 2);
    ui.tile.image = assets.patterns[3];
    ui.built = true;
}

void BuildLarge()
{
    static bool built = false;
    if (built)
        return;
    const char* const icons[] = {"file", "folder", "image", "star"};
    for (int i = 0; i < kLargeSlots; ++i)
        LargeItems[i] = Placeholder("Item", icons[i % 4], 1 + i % 5);
    built = true;
}

const char* ActionName(InventoryAction action)
{
    switch (action)
    {
    case InventoryAction::Activated:
        return "Activated";
    case InventoryAction::Moved:
        return "Moved";
    case InventoryAction::Swapped:
        return "Swapped";
    case InventoryAction::Merged:
        return "Merged";
    case InventoryAction::Split:
        return "Split";
    default:
        return "None";
    }
}

// The item of a place: a bag slot, an equipment slot or the standalone tile.
InventoryItem* Place(ImGuiID owner, int index)
{
    if (owner == ui.bagId && index >= 0 && index < kBagSlots)
        return &ui.bag[index];
    for (int i = 0; i < kEquipSlots; ++i)
        if (owner == ui.equipIds[i] && owner != 0)
            return &ui.equip[i];
    if (owner == ui.tileId && owner != 0)
        return &ui.tile;
    return nullptr;
}

// Applies a result returned by the widget `target` (the bag, or a slot) as an application would.
void Apply(const InventoryResult& r, ImGuiID target)
{
    if (r.action == InventoryAction::None)
        return;
    ++ui.actions;
    std::snprintf(ui.lastText, sizeof(ui.lastText), "Last action: %s, %d item(s), from %d to %d", ActionName(r.action),
                  r.count, r.from, r.to);
    InventoryItem* from = Place(r.source, r.from);
    InventoryItem* to = Place(target, r.to);
    if (from == nullptr || to == nullptr)
        return;
    switch (r.action)
    {
    case InventoryAction::Moved:
        *to = *from;
        *from = {};
        break;
    case InventoryAction::Swapped:
        std::swap(*to, *from);
        break;
    case InventoryAction::Merged:
        to->count += r.count;
        from->count -= r.count;
        if (from->count <= 0)
            *from = {};
        break;
    case InventoryAction::Split:
        *to = *from;
        to->count = r.count;
        from->count -= r.count;
        break;
    default:
        break;
    }
}

// The tile size of a grid whose item rect is grid with columns columns of the default slot size:
// the same geometry the grid lays out (scroll area padding, slot size, xs gap, tiles that shrink
// when the columns do not fit).
float TileSide(const ImRect& grid, int columns)
{
    const facet::Theme& t = facet::T();
    const float outset = facet::internal::FocusRingOutset();
    const float gap = facet::Px(t.spacing.xs);
    const float shared = std::floor((grid.GetWidth() - outset * 2.0f - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns));
    return std::max(1.0f, std::min(std::floor(facet::Px(t.metrics.slotSize)), shared));
}

ImRect TileRect(const ImRect& grid, int index, int columns)
{
    const float outset = facet::internal::FocusRingOutset();
    const float tile = TileSide(grid, columns);
    const float pitch = tile + facet::Px(facet::T().spacing.xs);
    const float x = grid.Min.x + outset + static_cast<float>(index % columns) * pitch;
    const float y = grid.Min.y + outset + static_cast<float>(index / columns) * pitch;
    return ImRect(x, y, x + tile, y + tile);
}

// The columns of the bag: the set count, or as many default slots as its width holds.
int ColumnsOf(const ImRect& grid, int setColumns)
{
    if (setColumns > 0)
        return setColumns;
    const facet::Theme& t = facet::T();
    const float outset = facet::internal::FocusRingOutset();
    const float pitch = std::floor(facet::Px(t.metrics.slotSize)) + facet::Px(t.spacing.xs);
    return std::max(1, static_cast<int>(std::lround((grid.GetWidth() - outset * 2.0f + facet::Px(t.spacing.xs)) / pitch)));
}

// ---- Sections ------------------------------------------------------------------------------------

void InventorySection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Inventory grid");
    Paragraph("Slots of placeholder items. Drag an item onto an empty slot to move it, onto the same kind to merge, onto "
              "another item to swap; hold Shift to move half a stack. A locked item stays put. The arrows move the "
              "selection, Ctrl and Shift extend it, Alt and an arrow move the item, Space picks it up for a keyboard "
              "drag that the arrows carry and Space drops (Escape puts it back), Enter activates.");
    LabeledRow("Columns");
    facet::SegmentedControl("##columns", &ui.columns, kColumns, {.width = DemoWidth(300.0f)});
    ctx.Probe("composites.bag.columns");
    LabeledRow("Data");
    facet::SegmentedControl("##state", &ui.bagState, kDataStates, {.width = DemoWidth(300.0f)});
    ctx.Probe("composites.bag.state");

    const int columns = ui.columns == 1 ? 4 : (ui.columns == 2 ? 6 : 0);
    const facet::DataState state = ui.bagState == 1 ? facet::DataState::Loading
                                   : ui.bagState == 2 ? facet::DataState::Error
                                                      : facet::DataState::Ready;
    const std::span<const InventoryItem> slots =
        ui.bagState == 3 ? std::span<const InventoryItem>() : std::span<const InventoryItem>(ui.bag, kBagSlots);
    if (Is(ctx, kFocused))
    {
        ui.selected = 4;
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Focused, 4);
    }
    if (Is(ctx, kDragging))
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Dragging, 1);
    LabeledRow("Bag");
    ui.bagId = ImGui::GetID("bag");
    const InventoryResult result = facet::InventoryGrid(
        "bag", &ui.selected, slots,
        {.columns = columns, .state = state, .emptyText = "No placeholder items", .errorText = "Placeholder load error"});
    ctx.Probe("composites.bag");
    const ImRect grid(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ui.bagColumns = ColumnsOf(grid, columns);
    for (int i = 0; i < kProbedTiles; ++i)
    {
        char name[48];
        std::snprintf(name, sizeof(name), "composites.bag.%d", i);
        const ImRect tile = TileRect(grid, i, ui.bagColumns);
        facet::demo::ProbeRect(ctx, name, 0, tile.Min, tile.Max);
    }
    if (Is(ctx, kFocused) || Is(ctx, kDragging))
        ctx.RevealInCapture(0.3f);
    Apply(result, ui.bagId);

    LabeledRow("Equipment");
    const float right = facet::demo::ContentRight();
    for (int i = 0; i < kEquipSlots; ++i)
    {
        char label[32];
        std::snprintf(label, sizeof(label), "Placeholder slot %d", i + 1);
        ui.equipIds[i] = ImGui::GetID(label);
        const InventoryResult slot =
            facet::InventorySlot(label, ui.equip[i], {.placeholderIcon = facet::Icon(i == 0 ? "user" : "grid")});
        char probe[32];
        std::snprintf(probe, sizeof(probe), "composites.slot.%d", i);
        ctx.Probe(probe);
        Apply(slot, ui.equipIds[i]);
        facet::demo::SameLineIfFits(right, facet::Px(facet::T().metrics.slotSize));
    }
    ui.tileId = ImGui::GetID("tile");
    if (facet::InventoryTile("tile", ui.tile))
        ++ui.tileActivations;
    ctx.Probe("composites.tile");
    ImGui::SameLine();
    facet::demo::NoteBeside("Tile activated %d times; drag it into the bag", ui.tileActivations);
    LabeledRow("Result");
    facet::StyledText(ui.lastText, {.style = facet::TextStyle::Footnote, .secondary = true});
    ctx.Probe("composites.bag.result");

    LabeledRow("States");
    const float stateRight = facet::demo::ContentRight();
    const float stateWidth = DemoWidth(200.0f);
    int none = -1;
    (void)facet::InventoryGrid("empty", &none, {}, {.emptyText = "Empty placeholder bag", .width = stateWidth});
    facet::demo::SameLineIfFits(stateRight, facet::Px(stateWidth));
    (void)facet::InventoryGrid("loading", &none, std::span<const InventoryItem>(ui.bag, 6),
                               {.state = facet::DataState::Loading, .width = stateWidth});
    facet::demo::SameLineIfFits(stateRight, facet::Px(stateWidth));
    (void)facet::InventoryGrid("failed", &none, {}, {.state = facet::DataState::Error, .errorText = "Placeholder error",
                                                     .width = stateWidth});
    facet::demo::SameLineIfFits(stateRight, facet::Px(stateWidth));
    ImGui::BeginDisabled();
    (void)facet::InventoryGrid("disabled", &none, std::span<const InventoryItem>(ui.bag, 3), {.width = stateWidth});
    ImGui::EndDisabled();
    ImGui::SameLine();
    facet::demo::NoteBeside("Empty, loading, failed, disabled");
}

void LargeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Ten thousand slots");
    Paragraph("The same grid with 10000 placeholder slots: only the visible tiles are submitted, so a frame costs what "
              "a short grid costs. It scrolls inside, at most as tall as the window.");
    BuildLarge();
    LabeledRow("Large bag");
    (void)facet::InventoryGrid("large", &ui.largeSelected, LargeItems, {.slotSize = 48.0f, .readOnly = false});
    ctx.Probe("composites.large");
}

void TilesSection(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    SectionHeader("Tiles and slots");
    Paragraph("A tile shows the picture (an image with its loading and failed states, or an icon), the count, the lock, "
              "a cooldown sweep, the category bar and the title, cut with an ellipsis; the full name is the tooltip. "
              "Empty slots show their placeholder icon faintly.");
    const InventoryItem plain = Placeholder("Placeholder item", "file");
    InventoryItem stack = Placeholder("Placeholder stack", "folder", 24);
    InventoryItem locked = Placeholder("Placeholder locked", "image");
    locked.locked = true;
    InventoryItem cooldown = Placeholder("Placeholder cooldown", "clock");
    cooldown.cooldown = 0.7f;
    cooldown.tint = facet::ToU32(t.colors.chart[1]);
    InventoryItem picture = Placeholder("Placeholder pattern", "image");
    picture.image = LoadingImage(ctx);
    InventoryItem failed = Placeholder("Placeholder image that failed", "image");
    failed.image = assets.failed;
    InventoryItem longTitle = Placeholder("Placeholder with a title far too long for any slot", "star", 3);

    if (Is(ctx, kLoading))
        ctx.RevealInCapture(0.05f);
    LabeledRow("Looks");
    const float right = facet::demo::ContentRight();
    const float slot = facet::Px(t.metrics.slotSize);
    const InventoryItem* looks[] = {&plain, &stack, &locked, &cooldown, &picture, &failed, &longTitle};
    int index = 0;
    for (const InventoryItem* item : looks)
    {
        ImGui::PushID(index);
        (void)facet::InventoryTile("##look", *item, {.selected = index == 1, .dragSource = false});
        ImGui::PopID();
        if (++index < static_cast<int>(std::size(looks)))
            facet::demo::SameLineIfFits(right, slot);
    }
    LabeledRow("Sizes");
    const float sizeRight = facet::demo::ContentRight();
    (void)facet::InventoryTile("##size.small", longTitle, {.dragSource = false, .slotSize = kSmallSlotUnits});
    facet::demo::SameLineIfFits(sizeRight, slot * 2.0f);
    (void)facet::InventoryTile("##size.large", longTitle, {.dragSource = false, .slotSize = t.metrics.slotSize * 2.0f});
    facet::demo::SameLineIfFits(sizeRight, slot);
    ImGui::BeginDisabled();
    (void)facet::InventoryTile("##size.disabled", stack, {.dragSource = false});
    ImGui::EndDisabled();
    ImGui::SameLine();
    facet::demo::NoteBeside("Small, twice the slot size, disabled");
}

void DetailSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Inventory detail card");
    Paragraph("A Card with the title, the picture beside the description, the stats with the difference to a compared "
              "value (the sign says which way, the tone whether it is better), tags as badges and actions as buttons, "
              "the first one primary.");
    static const facet::InventoryStat kStats[] = {{"Placeholder stat 1", 12.0, 9.0},
                                                  {"Placeholder stat 2", 3.5, 4.0},
                                                  {"Placeholder stat 3, lower is better", 2.0, 5.0, true},
                                                  {"Placeholder stat 4", 7.0}};
    static const facet::InventoryStat kPlain[] = {{"Placeholder stat 1", 12.0}, {"Placeholder stat 2", 3.5},
                                                  {"Placeholder stat 3, lower is better", 2.0}, {"Placeholder stat 4", 7.0}};
    LabeledRow("Compare");
    facet::Toggle("Show differences##compare", &ui.compare);
    ctx.Probe("composites.detail.compare");
    LabeledRow("Card");
    const float right = facet::demo::ContentRight();
    const int pressed = facet::InventoryDetailCard(
        "Placeholder item 3", LoadingImage(ctx),
        {.description = "Placeholder description of the item, long enough to wrap onto a second line.",
         .stats = ui.compare ? std::span<const facet::InventoryStat>(kStats) : std::span<const facet::InventoryStat>(kPlain),
         .statFormat = "%.1f",
         .tags = kTags,
         .actions = kActions,
         .width = DemoWidth(facet::T().metrics.cardWidth)});
    ctx.Probe("composites.detail");
    if (pressed >= 0)
    {
        ui.detailAction = pressed;
        ++ui.detailPresses;
    }
    facet::demo::SameLineIfFits(right, facet::Px(kNarrowCardUnits));
    (void)facet::InventoryDetailCard("Placeholder narrow card", assets.patterns[1],
                                     {.description = "Narrow placeholder.", .stats = kPlain, .actions = kActions,
                                      .width = DemoWidth(kNarrowCardUnits)});
    LabeledRow("");
    facet::demo::Note("Action pressed %d times; last: %s", ui.detailPresses,
                      ui.detailAction >= 0 ? kActions[ui.detailAction] : "none");
}

void PeopleSection(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    SectionHeader("Media cards");
    Paragraph("A card with a cover, the title cut to one line, a description and badges; one Tab stop that activates. "
              "Below composites.mediaCardHorizontalBelow the cover goes beside the text.");
    LabeledRow("Cards");
    const float right = facet::demo::ContentRight();
    if (Is(ctx, kFocused))
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Focused);
    if (facet::MediaCard("Placeholder card title", LoadingImage(ctx),
                         {.description = "Placeholder description under the title.", .badges = kBadges,
                          .selected = ui.mediaSelected, .width = DemoWidth(t.metrics.cardWidth)}))
    {
        ++ui.mediaPresses;
        ui.mediaSelected = !ui.mediaSelected;
    }
    ctx.Probe("composites.media");
    facet::demo::SameLineIfFits(right, facet::Px(kNarrowCardUnits));
    (void)facet::MediaCard("Placeholder narrow card", assets.patterns[2],
                           {.description = "Placeholder.", .width = DemoWidth(kNarrowCardUnits)});
    facet::demo::SameLineIfFits(right, facet::Px(kNarrowCardUnits));
    (void)facet::MediaCard("Placeholder card whose cover failed", facet::ImageHandle{},
                           {.description = "The failed state of the media family.", .width = DemoWidth(kNarrowCardUnits)});
    LabeledRow("");
    facet::demo::Note("Activated %d times; activating toggles the accent ring", ui.mediaPresses);

    SectionHeader("User chip");
    Paragraph("The signed-in account: avatar (initials without a picture), name and a description line in a pill; "
              "compact shows the avatar alone with the name as its tooltip.");
    const float rowHeight = facet::ControlHeight(facet::ControlSize::Large) * 1.5f;
    LabeledRow("Sizes", rowHeight);
    {
        facet::demo::MixedHeightRow row(rowHeight);
        const facet::ControlSize sizes[] = {facet::ControlSize::Small, facet::ControlSize::Regular, facet::ControlSize::Large};
        int index = 0;
        for (const facet::ControlSize size : sizes)
        {
            ImGui::PushID(index++);
            row.Item(facet::ControlHeight(size) * 1.5f);
            if (Is(ctx, kFocused) && size == facet::ControlSize::Regular)
                ctx.ForceNextItemState(facet::demo::ForcedItemState::Focused);
            if (facet::UserChip("Placeholder profile 1", size == facet::ControlSize::Regular ? assets.patterns[0] : facet::ImageHandle{},
                                {.description = "Placeholder", .showStatus = true, .status = facet::Tone::Success, .size = size}))
                ++ui.chipPresses;
            if (size == facet::ControlSize::Regular)
                ctx.Probe("composites.chip");
            ImGui::PopID();
        }
    }
    LabeledRow("Compact and states");
    const float chipRight = facet::demo::ContentRight();
    (void)facet::UserChip("Placeholder profile 2", facet::ImageHandle{}, {.showStatus = true, .status = facet::Tone::Warning, .compact = true});
    facet::demo::SameLineIfFits(chipRight, facet::ControlHeight(facet::ControlSize::Regular));
    (void)facet::UserChip("Placeholder profile 3", LoadingImage(ctx), {});
    facet::demo::SameLineIfFits(chipRight, facet::Px(t.metrics.cardWidth));
    ImGui::BeginDisabled();
    (void)facet::UserChip("Placeholder profile, disabled", facet::ImageHandle{}, {});
    ImGui::EndDisabled();
    facet::demo::SameLineIfFits(chipRight, facet::Px(t.metrics.cardWidth));
    ImGui::SetNextItemWidth(facet::Px(DemoWidth(120.0f)));
    (void)facet::UserChip("Placeholder profile with a long name##narrow", facet::ImageHandle{}, {.description = "Placeholder"});
    LabeledRow("");
    facet::demo::Note("Chip activated %d times", ui.chipPresses);

    SectionHeader("Profile switcher");
    Paragraph("The current profile in a button that opens the list: the arrows move, Enter, Space or a click switches, "
              "the current profile has a check and a semibold name. Closing puts the focus back on the button.");
    LabeledRow("Data");
    facet::SegmentedControl("##profileState", &ui.profileState, kDataStates, {.width = DemoWidth(300.0f)});
    ctx.Probe("composites.profiles.state");
    const facet::DataState profileState = ui.profileState == 1 ? facet::DataState::Loading
                                          : ui.profileState == 2 ? facet::DataState::Error
                                                                 : facet::DataState::Ready;
    LabeledRow("Profiles");
    if (Is(ctx, kOpen))
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Open);
    const facet::ProfileSwitcherAction action = facet::ProfileSwitcher(
        "profiles", &ui.profile, ui.profileState == 3 ? std::span<const facet::AvatarItem>() : std::span<const facet::AvatarItem>(kProfiles),
        {.addLabel = "Add placeholder profile", .manageLabel = "Manage placeholder profiles", .state = profileState,
         .emptyText = "No placeholder profiles", .errorText = "Placeholder load error", .width = DemoWidth(t.metrics.cardWidth)});
    ctx.Probe("composites.profiles");
    if (action != facet::ProfileSwitcherAction::None)
    {
        ui.lastProfile = action;
        ++ui.profileActions;
    }
    {
        // The popover level and its first rows, for the selftest: the level id is the switcher's
        // "##profiles" under its id.
        ImGui::PushID("profiles");
        const ImGuiID level = ImGui::GetID("##profiles");
        ImGui::PopID();
        const ImRect rect = facet::internal::PopupLevelRect(level);
        if (rect.GetWidth() > 0.0f)
        {
            const float top = rect.Min.y + facet::Px(t.spacing.md) + facet::internal::FocusRingOutset();
            const float rowH = facet::Px(t.list.rowHeightLarge);
            for (int i = 0; i < 3; ++i)
            {
                char name[40];
                std::snprintf(name, sizeof(name), "composites.profiles.row%d", i);
                facet::demo::FloatingProbeRect(ctx, name, 0, ImVec2(rect.Min.x + facet::Px(t.spacing.md), top + rowH * i),
                                               ImVec2(rect.Max.x - facet::Px(t.spacing.md), top + rowH * (i + 1)));
            }
        }
        if (Is(ctx, kOpen))
        {
            ctx.RevealInCapture(0.2f);
            if (ctx.captureFramesLeft == 0 && !facet::internal::IsPopupLevelOpen(level))
                ctx.captureError = "the profile switcher's list did not open";
        }
    }
    LabeledRow("");
    const char* names[] = {"none", "Switched", "Add", "Manage"};
    facet::demo::Note("Current: %s; last action: %s (%d)", ui.profile >= 0 && ui.profile < 3 ? kProfiles[ui.profile].label : "none",
                      names[static_cast<int>(ui.lastProfile)], ui.profileActions);
}

void DragDropSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Drag and drop calls");
    Paragraph("BeginDragSource and BeginDropTarget make any item a source or a target. Drag the source onto the target, "
              "or focus it, press Ctrl+Space, move down with the arrows and press Space. The other target takes "
              "another payload type and refuses the drag.");
    LabeledRow("Source");
    (void)facet::Button("Placeholder source##dnd", {.icon = facet::Icon("grip")});
    ctx.Probe("composites.dnd.source");
    if (facet::BeginDragSource(kDemoPayload, &ui.drops, sizeof(ui.drops), {.previewText = "Placeholder source", .icon = facet::Icon("grip")}))
    {
        facet::StyledText("Placeholder extra preview", {.style = facet::TextStyle::Footnote, .secondary = true});
        facet::EndDragSource();
    }
    LabeledRow("Target");
    (void)facet::Button("Placeholder target##dnd", {.variant = facet::ButtonVariant::Plain, .icon = facet::Icon("download")});
    ctx.Probe("composites.dnd.target");
    if (facet::BeginDropTarget())
    {
        if (facet::AcceptDrop(kDemoPayload) != nullptr)
            ++ui.drops;
        facet::EndDropTarget();
    }
    ImGui::SameLine();
    facet::demo::NoteBeside("Dropped %d times", ui.drops);
    LabeledRow("Refusing target");
    (void)facet::Button("Placeholder other type##dnd", {.variant = facet::ButtonVariant::Plain});
    ctx.Probe("composites.dnd.other");
    if (facet::BeginDropTarget())
    {
        (void)facet::AcceptDrop("facet.demo.other", {.indicator = facet::DropIndicator::LineAfter});
        facet::EndDropTarget();
    }
}

} // namespace

FACET_DEMO_PAGE("composites-cards", "Inventory and cards", "Widgets", 140, "default", "focused", "dragging", "open",
                "loading")
{
    EnsureAssets(ctx.dark);
    if (!ui.built)
        Reset();
    facet::demo::PageHeader("Inventory and cards",
                            "Inventory grids, item tiles, slots and detail cards, media cards, user chips, profile "
                            "switchers and the drag and drop calls. Every item and profile here is a placeholder.");
    facet::demo::ReadyFamilyNote("composites");
    InventorySection(ctx);
    TilesSection(ctx);
    DetailSection(ctx);
    PeopleSection(ctx);
    DragDropSection(ctx);
    LargeSection(ctx);
}

FACET_DEMO_SELFTEST("composites-cards")
{
    using T = facet::demo::Target;
    test.Do([] {
        EnsureAssets(facet::T().dark);
        Reset();
        ui.columns = 2;  // six columns, so the rows are known at every size
    });

    // The bag: click, arrows in two dimensions, Alt and an arrow, Enter.
    test.Reveal("composites.bag");
    test.WaitSeconds(0.4f);
    test.Click("composites.bag.0");
    test.Expect("a click selects the first slot", [] { return ui.selected == 0; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right moves the selection to the second slot", [] { return ui.selected == 1; });
    test.KeyPress(ImGuiKey_DownArrow);
    test.Expect("Down moves the selection one row down", [] { return ui.selected == 1 + ui.bagColumns; });
    test.Click("composites.bag.1");
    test.KeyPress(ImGuiMod_Alt | ImGuiKey_RightArrow);
    test.Expect("Alt+Right moves item 2 into the empty third slot",
                [] { return ui.bag[2].label != nullptr && ui.bag[1].label == nullptr && ui.selected == 2; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter activates the slot", [] { return ui.actions == 2; });

    // Pointer drags: a merge of the two stacks of item 1, a refused drop on the locked item, Escape.
    test.WaitSeconds(0.4f);
    test.Drag({T::Item("composites.bag.3"), T::Item("composites.bag.2"), T::Item("composites.bag.0")}, 0.4f);
    test.Expect("dropping a stack on the same kind merges it", [] { return ui.bag[0].count == 9 && ui.bag[3].label == nullptr; });
    test.Drag({T::Item("composites.bag.0"), T::Item("composites.bag.5")}, 0.4f);
    test.Expect("the locked item refuses the drop", [] { return ui.bag[0].count == 9 && ui.bag[5].locked; });
    test.MouseMove(T::Item("composites.bag.0"));
    test.MouseDown();
    test.MouseMove(T::Item("composites.bag.1"));
    test.WaitSeconds(0.2f);
    test.KeyPress(ImGuiKey_Escape);
    test.MouseUp();
    test.Expect("Escape cancels a drag", [] { return ui.bag[0].count == 9 && ui.bag[1].label == nullptr; });
    test.KeyDown(ImGuiMod_Shift);
    test.Drag({T::Item("composites.bag.0"), T::Item("composites.bag.1")}, 0.4f);
    test.KeyUp(ImGuiMod_Shift);
    test.Expect("Shift moves half a stack", [] { return ui.bag[1].count == 4 && ui.bag[0].count == 5; });

    // A keyboard drag: Space picks the first slot up, Right carries it onto slot 2 (a swap), Space drops.
    test.Click("composites.bag.0");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("a keyboard drag merges the stack into the second slot", [] {
        return ui.bag[0].label == nullptr && ui.bag[1].count == 9;
    });

    // Into the equipment slot by pointer, and back by keyboard.
    test.Drag({T::Item("composites.bag.4"), T::Item("composites.slot.0")}, 0.5f);
    test.Expect("the item moves into the empty equipment slot", [] { return ui.equip[0].label != nullptr && ui.bag[4].label == nullptr; });
    test.Click("composites.slot.0");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space, Up and Space move it back into the bag", [] {
        return ui.equip[0].label == nullptr || std::strcmp(ui.equip[0].label, "Item 3") != 0;
    });

    // The standalone tile: click, then Space and Enter after Tab shows the focus.
    test.Click("composites.tile");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.tile");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the tile activates by click, Space and Enter", [] { return ui.tileActivations == 3; });

    // The detail card: its actions follow the toggle in Tab order.
    test.Reveal("composites.detail.compare");
    test.WaitSeconds(0.4f);
    test.Click("composites.detail.compare");
    test.Expect("the toggle hides the differences", [] { return !ui.compare; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the first action returns it", [] { return ui.detailAction == 0; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the second action returns it", [] { return ui.detailAction == 1 && ui.detailPresses == 2; });

    // The media card and the user chip.
    test.Reveal("composites.media");
    test.WaitSeconds(0.4f);
    test.Click("composites.media");
    test.Expect("a click activates the media card", [] { return ui.mediaPresses == 1 && ui.mediaSelected; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.media");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space activates it again", [] { return ui.mediaPresses == 2 && !ui.mediaSelected; });
    test.Reveal("composites.chip");
    test.WaitSeconds(0.4f);
    test.Click("composites.chip");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.chip");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the chip activates by click and Enter", [] { return ui.chipPresses == 2; });

    // The profile switcher: a click opens, Down and Enter switch, the focus comes back.
    test.Reveal("composites.profiles");
    test.WaitSeconds(0.4f);
    test.Click("composites.profiles");
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.4f);
    test.Expect("Down and Enter switch to the second profile",
                [] { return ui.profile == 1 && ui.lastProfile == facet::ProfileSwitcherAction::Switched; });
    test.ExpectFocused("composites.profiles");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.4f);
    test.Click("composites.profiles.row2");
    test.WaitSeconds(0.4f);
    test.Expect("a click on the third row switches to it", [] { return ui.profile == 2; });
    test.ExpectFocused("composites.profiles");
    test.Click("composites.profiles");
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.4f);
    test.Expect("Escape closes the list without switching", [] { return ui.profile == 2 && ui.profileActions == 2; });
    test.ExpectFocused("composites.profiles");

    // The public drag and drop calls, by pointer and by keyboard.
    test.Reveal("composites.dnd.source");
    test.WaitSeconds(0.4f);
    test.Drag({T::Item("composites.dnd.source"), T::Item("composites.dnd.other")}, 0.4f);
    test.Expect("the target of another type refuses the drag", [] { return ui.drops == 0; });
    test.Drag({T::Item("composites.dnd.source"), T::Item("composites.dnd.target")}, 0.4f);
    test.Expect("the target takes the drop", [] { return ui.drops == 1; });
    test.Click("composites.dnd.source");
    test.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Space);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Ctrl+Space, Down and Space drop it by keyboard", [] { return ui.drops == 2; });
}
