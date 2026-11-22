// Showcase: a sample settings menu (tabs, a search over the settings, rows of toggles, sliders,
// a range slider, dropdowns, key binds, a color well and a preset list) next to an inventory
// grid with a detail card, built only from Facet widgets. Every setting, item, user and value is
// a placeholder and says so. The page the app opens on.
//
// Capture states: the page itself, a search, the keyboard focus on the range slider, and three
// states of the app around it: the theme editor open, the editor with an edit in every group,
// and overlay mode.

#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "page_helpers.h"
#include "pages/pages.h"

namespace
{

using namespace facet::demo;

enum ShowcaseState
{
    kDefault,
    kSearch,
    kFocused,
    kEditor,
    kEdited,
    kOverlay,
};

constexpr const char* kSearchId = "showcase.settings";
constexpr int kSlotCount = 16;
constexpr int kMaxPresets = 8;

// The tabs of the sample menu; the search also uses them as section names.
constexpr const char* kSections[] = {"General", "Display", "Controls", "Presets"};
constexpr int kSectionCount = 4;
constexpr const char* const kDensity[] = {"Compact", "Regular", "Spacious"};
constexpr const char* const kRefresh[] = {"Placeholder option 1", "Placeholder option 2", "Placeholder option 3"};
constexpr const char* const kInventoryData[] = {"Items", "Loading", "Error", "Empty"};

// Placeholder items: numbered names, icons from the built-in set.
constexpr const char* kItemNames[kSlotCount] = {
    "Item 1", "Item 2", "Item 3", nullptr,  "Item 5",  "Item 6",  nullptr, "Item 8",
    "Item 9", nullptr,  "Item 11", "Item 12", nullptr, "Item 14", "Item 15", nullptr,
};
constexpr const char* kItemIcons[kSlotCount] = {
    "heart", "bell", "folder", "", "image", "clock", "", "file", "lock", "", "pin", "palette", "", "layers", "keyboard", "",
};

struct ShowcasePage
{
    int tab = 0;
    std::string search;
    bool tooltips = true;
    bool confirmClose = false;
    int density = 1;
    float contrast = 0.6f;
    float rangeLow = 200.0f;
    float rangeHigh = 800.0f;
    int refresh = 0;
    bool colorSet = false;
    ImVec4 highlight = {0.0f, 0.0f, 0.0f, 1.0f};
    facet::Keybind openMenu{.primary = ImGuiKey_F1};
    facet::Keybind quickAction{.primary = ImGuiKey_Q};
    float pointer = 1.0f;
    int activePreset = 0;
    std::array<std::string, kMaxPresets> presetNames = {"Preset 1 (placeholder)", "Preset 2 (placeholder)",
                                                        "Preset 3 (placeholder)"};
    int presetCount = 3;
    int selectedSlot = 2;
    std::array<bool, kSlotCount> slotFilled = {};
    bool slotsKnown = false;
    int inventoryData = 0;
    int itemUses = 0;
};

ShowcasePage page;

// A settings row that the search can hide. Returns true when the row is submitted; the caller
// then submits the control and calls facet::EndSettingsRow.
bool ShowcaseRow(const char* label, const char* section, const char* keywords = nullptr)
{
    facet::SettingsSearchMatchOptions match;
    match.section = section;
    match.keywords = keywords;
    if (!facet::SettingsSearchMatch(label, match))
        return false;
    facet::BeginSettingsRow(label);
    return true;
}

void GeneralRows(PageCtx& ctx)
{
    const char* section = kSections[0];
    if (ShowcaseRow("Show tooltips", section, "hints help"))
    {
        facet::Toggle("##value", &page.tooltips);
        ctx.Probe("showcase.tooltips");
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Confirm on close", section, "quit exit"))
    {
        facet::Toggle("##value", &page.confirmClose);
        ctx.Probe("showcase.confirm");
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Layout density", section, "spacing size"))
    {
        facet::Dropdown("##value", &page.density, kDensity);
        ctx.Probe("showcase.density");
        facet::EndSettingsRow();
    }
}

void DisplayRows(PageCtx& ctx)
{
    const char* section = kSections[1];
    if (ShowcaseRow("Text contrast", section, "readability"))
    {
        facet::SliderOptions slider;
        slider.format = "%.0f %%";
        float percent = page.contrast * 100.0f;
        if (facet::Slider("##value", &percent, 0.0f, 100.0f, slider))
            page.contrast = percent / 100.0f;
        ctx.Probe("showcase.contrast");
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Visible range", section, "distance minimum maximum"))
    {
        facet::RangeSliderOptions range;
        range.format = "%.0f";
        range.minGap = 50.0;
        range.step = 10.0;
        if (ctx.capture && ctx.captureState == kFocused)
            ctx.ForceNextItemState(ForcedItemState::Focused, 1);
        facet::RangeSlider("##value", &page.rangeLow, &page.rangeHigh, 0.0f, 1000.0f, range);
        ctx.Probe("showcase.range");
        if (ctx.capture && ctx.captureState == kFocused)
            ctx.RevealInCapture();
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Refresh rate", section, "frames"))
    {
        facet::Dropdown("##value", &page.refresh, kRefresh);
        ctx.Probe("showcase.refresh");
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Highlight color", section, "accent colour"))
    {
        if (!page.colorSet)
        {
            // The first value is a theme color, so the well starts on a color of the palette.
            page.highlight = facet::T().colors.chart[2];
            page.colorSet = true;
        }
        facet::ColorWell("##value", &page.highlight);
        ctx.Probe("showcase.color");
        facet::EndSettingsRow();
    }
}

void ControlsRows(PageCtx& ctx)
{
    const char* section = kSections[2];
    if (ShowcaseRow("Open the menu", section, "key shortcut bind"))
    {
        facet::KeybindCapture("##value", &page.openMenu);
        ctx.Probe("showcase.keybind");
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Quick action", section, "key shortcut bind"))
    {
        facet::KeybindCaptureOptions bind;
        bind.showMode = true;
        facet::KeybindCapture("##value", &page.quickAction, bind);
        ctx.Probe("showcase.quick");
        facet::EndSettingsRow();
    }
    if (ShowcaseRow("Pointer speed", section, "mouse sensitivity"))
    {
        facet::SliderOptions slider;
        slider.format = "%.2f\xC3\x97";
        slider.step = 0.05;
        facet::Slider("##value", &page.pointer, 0.25f, 3.0f, slider);
        ctx.Probe("showcase.pointer");
        facet::EndSettingsRow();
    }
}

void PresetRows(PageCtx& ctx)
{
    if (!facet::SettingsSearchMatch("Presets", {.keywords = "profile save load", .section = kSections[3]}))
        return;
    facet::PresetItem items[kMaxPresets];
    for (int i = 0; i < page.presetCount; ++i)
        items[i].label = page.presetNames[static_cast<std::size_t>(i)].c_str();
    facet::PresetListOptions options;
    options.emptyText = "No presets. Create one from the menu.";
    const facet::PresetListResult result =
        facet::PresetList("##presets", &page.activePreset, std::span<const facet::PresetItem>(items, page.presetCount), options);
    ctx.Probe("showcase.presets");
    switch (result.action)
    {
    case facet::PresetAction::Create:
    case facet::PresetAction::Duplicate:
        if (page.presetCount < kMaxPresets)
        {
            page.presetNames[static_cast<std::size_t>(page.presetCount)] =
                result.action == facet::PresetAction::Create && result.name != nullptr
                    ? std::string(result.name)
                    : page.presetNames[static_cast<std::size_t>(result.index)] + " copy";
            page.activePreset = page.presetCount++;
        }
        break;
    case facet::PresetAction::Rename:
        if (result.index >= 0 && result.name != nullptr)
            page.presetNames[static_cast<std::size_t>(result.index)] = result.name;
        break;
    case facet::PresetAction::Delete:
        if (result.index >= 0 && result.index < page.presetCount)
        {
            for (int i = result.index; i + 1 < page.presetCount; ++i)
                page.presetNames[static_cast<std::size_t>(i)] = page.presetNames[static_cast<std::size_t>(i + 1)];
            --page.presetCount;
            if (page.activePreset >= page.presetCount)
                page.activePreset = page.presetCount - 1;
        }
        break;
    default:
        break;
    }
}

void SettingsMenu(PageCtx& ctx)
{
    facet::BeginCard("Settings (sample menu)");
    // Field widgets fill the card's inner width (the grid cell pushes the cell's whole width).
    ImGui::PushItemWidth(facet::kFill);
    facet::UserChipOptions user;
    user.description = "Placeholder account";
    facet::UserChip("Placeholder user", facet::ImageHandle{}, user);
    ctx.Probe("showcase.user");

    facet::SearchFieldOptions search;
    search.placeholder = "Search the settings";
    facet::SearchField("##settings-search", &page.search, search);
    ctx.Probe("showcase.search");

    // The tabs show the hits of the last frame while a search runs.
    char counts[kSectionCount][16] = {};
    facet::ChoiceItem tabs[kSectionCount];
    const bool searching = !page.search.empty();
    for (int i = 0; i < kSectionCount; ++i)
    {
        tabs[i].label = kSections[i];
        if (searching)
        {
            std::snprintf(counts[i], sizeof(counts[i]), "%d", facet::SettingsSearchHitCount(kSearchId, kSections[i]));
            tabs[i].detail = counts[i];
        }
    }
    facet::TabBar("##tabs", &page.tab, tabs);
    ctx.Probe("showcase.tabs");

    // Without a search the tab's section shows; a search lists the matching rows of every section.
    facet::BeginSettingsSearch(kSearchId, {.filter = page.search.c_str()});
    void (*const sections[kSectionCount])(PageCtx&) = {GeneralRows, DisplayRows, ControlsRows, PresetRows};
    for (int i = 0; i < kSectionCount; ++i)
    {
        if (!searching && page.tab != i)
            continue;
        if (searching && facet::SettingsSearchHitCount(kSearchId, kSections[i]) > 0)
            facet::SeparatorText(kSections[i]);
        ImGui::PushID(i);
        sections[i](ctx);
        ImGui::PopID();
    }
    facet::EndSettingsSearch();
    if (searching && facet::SettingsSearchHitCount(kSearchId, nullptr) == 0)
    {
        facet::EmptyStateOptions empty;
        empty.message = "No setting name or keyword contains the search.";
        empty.icon = facet::Icon("search");
        empty.actionLabel = "Clear the search";
        empty.compact = true;
        if (facet::EmptyState("No match###settings-empty", empty))
            page.search.clear();
        ctx.Probe("showcase.no-match");
    }
    ImGui::PopItemWidth();
    facet::EndCard();
}

void Inventory(PageCtx& ctx)
{
    facet::BeginCard("Inventory (placeholder items)");
    ImGui::PushItemWidth(facet::kFill);
    if (!page.slotsKnown)
    {
        for (int i = 0; i < kSlotCount; ++i)
            page.slotFilled[static_cast<std::size_t>(i)] = kItemNames[i] != nullptr;
        page.slotsKnown = true;
    }
    facet::SegmentedControl("Data", &page.inventoryData, kInventoryData);
    ctx.Probe("showcase.data");

    const facet::Colors& c = facet::T().colors;
    facet::InventoryItem slots[kSlotCount];
    for (int i = 0; i < kSlotCount; ++i)
    {
        if (!page.slotFilled[static_cast<std::size_t>(i)])
            continue;
        facet::InventoryItem& item = slots[i];
        item.label = kItemNames[i];
        item.icon = facet::Icon(kItemIcons[i]);
        item.count = i % 5 == 1 ? 5 : 1;
        item.tint = facet::ToU32(c.chart[i % facet::kChartPaletteSize]);
        item.locked = i == 8;
        item.cooldown = i == 5 ? 0.4f : 0.0f;
        item.id = static_cast<ImGuiID>(i + 1);
    }
    facet::InventoryGridOptions grid;
    grid.state = page.inventoryData == 1 ? facet::DataState::Loading
                                         : (page.inventoryData == 2 ? facet::DataState::Error : facet::DataState::Ready);
    grid.emptyText = "No items (placeholder inventory).";
    grid.errorText = "The items could not be loaded (a placeholder error).";
    const int count = page.inventoryData == 3 ? 0 : kSlotCount;
    const facet::InventoryResult result =
        facet::InventoryGrid("##inventory", &page.selectedSlot, std::span<const facet::InventoryItem>(slots, count), grid);
    ctx.Probe("showcase.inventory");
    if (result.action == facet::InventoryAction::Moved && result.from >= 0 && result.to >= 0)
    {
        // The demo's slots know only whether they hold their item; a move keeps the item names in
        // their slots, so it swaps the filled flags.
        std::swap(page.slotFilled[static_cast<std::size_t>(result.from)], page.slotFilled[static_cast<std::size_t>(result.to)]);
        page.selectedSlot = result.to;
    }

    const bool ready = page.inventoryData == 0;
    const int selected = page.selectedSlot;
    if (ready && selected >= 0 && selected < kSlotCount && page.slotFilled[static_cast<std::size_t>(selected)])
    {
        static const facet::InventoryStat stats[] = {
            {"Placeholder value A", 12.0, 10.0},
            {"Placeholder value B", 3.0},
        };
        static const char* const tags[] = {"Placeholder tag"};
        static const char* const actions[] = {"Use", "Drop"};
        facet::InventoryDetailCardOptions card;
        card.description = "Placeholder description of the selected item.";
        card.stats = stats;
        card.tags = tags;
        card.actions = actions;
        const int action = facet::InventoryDetailCard(kItemNames[selected], facet::ImageHandle{}, card);
        ctx.Probe("showcase.detail");
        if (action == 0)
            ++page.itemUses;
        else if (action == 1)
            page.slotFilled[static_cast<std::size_t>(selected)] = false;
        Note("Use: %d time(s). Drop empties the slot.", page.itemUses);
    }
    else if (ready)
    {
        Note("Select an item to see its detail card.");
    }
    ImGui::PopItemWidth();
    facet::EndCard();
}

// The capture states of the app around the page. They are set again in every frame of every
// pass, and fail the capture when the part they show did not appear.
void AppState(PageCtx& ctx)
{
    if (!ctx.capture)
        return;
    GalleryState* gallery = CurrentGallery();
    if (gallery == nullptr)
        return;
    const int state = ctx.captureState;
    if (state == kEditor || state == kEdited)
    {
        gallery->editorOpen = true;
        if (state == kEdited)
        {
            ThemeEdits edits;
            edits.factor[static_cast<int>(ThemeEditGroup::Radii)] = 1.5f;
            edits.factor[static_cast<int>(ThemeEditGroup::Spacing)] = 1.25f;
            edits.factor[static_cast<int>(ThemeEditGroup::Type)] = 1.1f;
            edits.factor[static_cast<int>(ThemeEditGroup::Heights)] = 1.15f;
            edits.factor[static_cast<int>(ThemeEditGroup::Shadow)] = 1.5f;
            edits.themeDefaultAccent = false;
            edits.accent = facet::AccentPresets()[3].color;
            gallery->edits = edits;
        }
        if (ctx.captureFramesLeft == 0 && !gallery->editorOpen)
            ctx.captureError = "the theme editor did not open";
        if (ctx.captureFramesLeft == 0 && state == kEdited && !(gallery->appliedEdits == gallery->edits))
            ctx.captureError = "the theme edits were not applied";
    }
    if (state == kOverlay)
    {
        gallery->overlayMode = true;
        gallery->overlayVisible = true;
        if (ctx.captureFramesLeft == 0 && gallery->shellMax.x <= gallery->shellMin.x)
            ctx.captureError = "the overlay shell did not show";
    }
    if (state == kSearch)
        page.search = "key";
    else if (ctx.captureFramesLeft > 0 && state != kFocused)
        page.search.clear();
}

} // namespace

FACET_DEMO_PAGE("showcase", "Showcase", "Start", 0, "default", "search", "focused", "editor", "edited", "overlay")
{
    if (ctx.capture)
    {
        // Every pass starts from the same page state.
        if (ctx.captureState != kSearch)
            page.search.clear();
        page.tab = ctx.captureState == kFocused ? 1 : 0;
    }
    AppState(ctx);
    PageHeader("Showcase",
               "A sample settings menu and an inventory, assembled only from Facet widgets. The settings change "
               "nothing outside this page; every name, item and value is a placeholder.");

    facet::GridOptions grid;
    grid.minCellWidth = facet::T().spacing.xxxl * 11.0f;
    grid.maxColumns = 2;
    facet::BeginGrid("##showcase", grid);
    facet::GridNext();
    SettingsMenu(ctx);
    facet::GridNext();
    Inventory(ctx);
    facet::EndGrid();
}

FACET_DEMO_SELFTEST("showcase")
{
    using T = facet::demo::Target;
    test.Do([] { page = ShowcasePage{}; });

    // Mouse: a toggle on the first tab.
    test.Click("showcase.tooltips");
    test.Expect("a click on the toggle turns tooltips off", [] { return !page.tooltips; });
    test.Click("showcase.tooltips");
    test.Expect("a second click turns them on again", [] { return page.tooltips; });

    // Keyboard: Right on the focused tab bar opens the next tab, the Display section.
    test.MouseMove(T::Item("showcase.tabs", 0.04f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the first tab keeps General open", [] { return page.tab == 0; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right on the tab bar opens the next tab", [] { return page.tab == 1; });
    test.Wait(2);

    // Mouse on the Display tab: a slider and the range slider.
    test.Drag({T::Item("showcase.contrast", 0.5f), T::Item("showcase.contrast", 0.95f)}, 0.3f);
    test.Expect("dragging the contrast slider raises it", [] { return page.contrast > 0.6f; });
    test.Drag({T::Item("showcase.range", 0.5f), T::Item("showcase.range", 0.6f)}, 0.3f);
    test.Expect("dragging the range slider moves its span or a handle",
                [] { return page.rangeLow != 200.0f || page.rangeHigh != 800.0f; });
    test.Expect("the handles keep the minimum gap", [] { return page.rangeHigh - page.rangeLow >= 50.0f - 0.01f; });
    // Keyboard on the range slider: Tab reaches its first handle from the slider above, and the
    // arrows step the focused handle.
    test.Click("showcase.contrast");
    test.Do([] {
        page.rangeLow = 200.0f;
        page.rangeHigh = 800.0f;
    });
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus where it is
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right steps the low handle by the slider's step", [] { return page.rangeLow == 210.0f; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("Tab reaches the high handle, Left steps it down", [] { return page.rangeHigh == 790.0f; });

    // Search over every tab: a word finds the key bind rows, the other rows go.
    test.Click("showcase.search");
    test.Type("key");
    test.Wait(2);
    test.ExpectVisible("showcase.keybind");
    test.ExpectNotVisible("showcase.contrast");
    test.Expect("the search counts hits in the Controls section",
                [] { return facet::SettingsSearchHitCount("showcase.settings", "Controls") >= 2; });
    test.Type("zzzz");
    test.Wait(2);
    test.ExpectVisible("showcase.no-match");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape in the search field clears the search", [] { return page.search.empty(); });

    // Inventory: a click on the first slot selects it, the arrows move the selection, and the
    // detail card follows.
    test.Do([] { page.selectedSlot = -1; });
    test.MouseMove(T::Item("showcase.inventory", 0.04f, 0.1f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.Expect("a click selects the first slot", [] { return page.selectedSlot == 0; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right moves the selection to the next slot", [] { return page.selectedSlot == 1; });
    test.Reveal("showcase.detail");
    test.ExpectVisible("showcase.detail");

    // The data states of the inventory.
    test.Do([] { page.inventoryData = 3; });
    test.Wait(2);
    test.ExpectNotVisible("showcase.detail");
    test.Do([] { page.inventoryData = 0; });
}