// Demo page: Composites. Calls every declared composite widget once with neutral arguments,
// grouped by purpose. Until Phase 2 implements a widget, its call draws the placeholder of the
// declared size; Phase 2 replaces the placeholders one by one. The theme controls are called
// with apply off, so they never fight the theme selector of the gallery.

#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Heights of the scrolling views on this page, in multiples of the xxxl spacing token.
constexpr float kViewSpans = 4.0f;
// Range of the example UI scale control (1 is 100 percent).
constexpr float kScaleMin = 0.75f;
constexpr float kScaleMax = 2.0f;
// Reference resolution of the example preview canvas, in reference pixels.
const ImVec2 kReferenceSize(1920.0f, 1080.0f);
// Payload type of the drag and drop example.
constexpr const char* kDemoPayload = "facet.demo.item";

// Placeholder data: names like "Item 1" and built-in icons, nothing real.
const facet::LogEntry kConsoleLines[] = {{"Output line 1"}, {"Output line 2", facet::LogLevel::Warning}};
const char* const kCommands[] = {"command-1", "command-2"};
const facet::PresetItem kPresets[] = {{"Preset 1", false, true}, {"Preset 2", true}, {"Preset 3"}};
const facet::InventoryItem kSlots[] = {
    {"Item 1", {}, facet::Icon("file"), 3},
    {"Item 2", {}, facet::Icon("folder")},
    {},
    {"Item 3", {}, facet::Icon("image"), 1, 0, 0, 0, true},
    {},
    {},
};
const facet::ItemStat kStats[] = {{"Stat 1", 4.0, 3.0}, {"Stat 2", 2.0}};
const char* const kTags[] = {"Tag 1", "Tag 2"};
const char* const kActions[] = {"Action 1", "Action 2"};
const facet::AvatarItem kProfiles[] = {{"Profile 1"}, {"Profile 2"}};

struct CompositesUi
{
    ImVec2 curve[8] = {{0.0f, 0.0f}, {0.5f, 0.7f}, {1.0f, 1.0f}};
    int curveCount = 3;
    facet::CubicBezier easing{{0.25f, 0.1f}, {0.25f, 1.0f}};
    facet::GradientStop stops[8] = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    int stopCount = 2;
    bool option = true;
    std::string command;
    std::string query;
    facet::FilterChip filters[3] = {{"Filter 1"}, {"Filter 2"}, {"Filter 3"}};
    std::string settingsQuery;
    facet::KeybindListItem binds[2] = {
        {"Action 1", "Group 1", {ImGuiMod_Ctrl | ImGuiKey_1}, {ImGuiMod_Ctrl | ImGuiKey_1}},
        {"Action 2", "Group 1", {ImGuiMod_Ctrl | ImGuiKey_2}, {ImGuiMod_Ctrl | ImGuiKey_2}},
    };
    int preset = 0;
    facet::ThemeMode themeMode = facet::ThemeMode::System;
    ImVec4 accent = {0.0f, 0.0f, 0.0f, 1.0f};
    float uiScale = 1.0f;
    int slot = 0;
    int profile = 0;
    facet::CanvasElementState element{facet::CanvasAnchor::TopLeft, {64.0f, 64.0f}, {480.0f, 120.0f}};
    facet::LayerItem layers[3] = {{"Layer 1"}, {"Layer 2", {}, 1}, {"Layer 3", {}, 0, false}};
    int layer = 0;
    bool onboarding = false;
    int onboardingStep = 0;
};

CompositesUi ui;

} // namespace

FACET_DEMO_PAGE("composites", "Composites", "Widgets", 140)
{
    const facet::Theme& theme = facet::T();
    const float viewHeight = theme.spacing.xxxl * kViewSpans;

    facet::demo::PageHeader("Composites",
                            "Editors for curves, easing and gradients, property grids, console views, search and "
                            "settings search, key bind and preset lists, theme controls, inventory grids and item "
                            "cards, people, preview canvases, layer lists, onboarding and the drag and drop calls. "
                            "Every widget on this page is a placeholder of its declared size until it is implemented "
                            "in Phase 2.");

    SectionHeader("Editors");
    LabeledRow("CurveEditor");
    facet::CurveEditor("Response", ui.curve, &ui.curveCount, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.marker = 0.5f});
    LabeledRow("BezierEditor");
    facet::BezierEditor("Easing", &ui.easing);
    LabeledRow("GradientEditor");
    facet::GradientEditor("Gradient", ui.stops, &ui.stopCount, {.alpha = true});

    SectionHeader("Inspectors");
    LabeledRow("PropertyGrid");
    ImGui::BeginGroup();  // keeps the grid in the control column of the row
    facet::BeginPropertyGrid("properties");
    if (facet::BeginPropertyCategory("Category 1"))
    {
        if (facet::BeginPropertyRow("Property 1", {.description = "Placeholder description"}))
        {
            ImGui::Checkbox("##value", &ui.option);
            facet::EndPropertyRow();
        }
        facet::EndPropertyCategory();
    }
    facet::EndPropertyGrid();
    ImGui::EndGroup();
    LabeledRow("ConsoleView");
    facet::ConsoleView("console", &ui.command, kConsoleLines, {.commands = kCommands, .height = viewHeight});
    LabeledRow("KeybindList");
    facet::KeybindList("binds", ui.binds);

    SectionHeader("Search and settings");
    LabeledRow("SearchFilterBar");
    facet::SearchFilterBar("Search##filter bar", &ui.query, ui.filters);
    LabeledRow("SettingsSearch");
    ImGui::BeginGroup();
    facet::BeginSettingsSearch("settings", ui.settingsQuery.c_str());
    if (facet::SettingsSearchMatch("Setting 1", {.keywords = "placeholder"}))
        facet::StyledText("Setting 1", {.secondary = true});
    facet::EndSettingsSearch();
    ImGui::EndGroup();
    LabeledRow("PresetList");
    facet::PresetList("presets", &ui.preset, kPresets, {.importExport = true});
    LabeledRow("ThemeModeControl");
    facet::ThemeModeControl("Theme##composites", &ui.themeMode, {.apply = false});
    LabeledRow("AccentPicker");
    facet::AccentPicker("Accent##composites", &ui.accent, {.apply = false});
    LabeledRow("UiScaleControl");
    facet::UiScaleControl("UI scale##composites", &ui.uiScale, kScaleMin, kScaleMax, {.apply = false});

    SectionHeader("Inventory");
    LabeledRow("InventoryGrid");
    facet::InventoryGrid("inventory", &ui.slot, kSlots, {.columns = 3});
    LabeledRow("ItemTile");
    facet::ItemTile("tile", kSlots[0], {.selected = true});
    LabeledRow("ItemSlot");
    facet::ItemSlot("Slot 1", facet::InventoryItem{}, {.placeholderIcon = facet::Icon("grid")});
    LabeledRow("ItemDetailCard");
    facet::ItemDetailCard("Item 1", facet::ImageHandle{},
                          {.description = "Placeholder description.", .stats = kStats, .tags = kTags,
                           .actions = kActions});

    SectionHeader("Cards and people");
    LabeledRow("MediaCard");
    facet::MediaCard("Title", facet::ImageHandle{}, {.detail = "Detail", .badges = kTags});
    LabeledRow("UserChip");
    facet::UserChip("Profile 1", facet::ImageHandle{}, {.detail = "Placeholder"});
    LabeledRow("ProfileSwitcher");
    facet::ProfileSwitcher("profiles", &ui.profile, kProfiles, {.addLabel = "Add profile"});

    SectionHeader("Canvas and layers");
    LabeledRow("PreviewCanvas");
    ImGui::BeginGroup();
    facet::BeginPreviewCanvas("canvas", kReferenceSize, {.safeArea = 0.05f});
    facet::CanvasElement("Element 1", &ui.element);
    facet::EndPreviewCanvas();
    ImGui::EndGroup();
    LabeledRow("LayerList");
    facet::LayerList("layers", &ui.layer, ui.layers);

    SectionHeader("Onboarding");
    LabeledRow("Onboarding");
    if (ImGui::Button(ui.onboarding ? "Hide onboarding" : "Show onboarding"))
        ui.onboarding = !ui.onboarding;
    facet::ItemCursor();
    if (facet::BeginOnboarding("Welcome", &ui.onboarding, &ui.onboardingStep, 3))
    {
        facet::StyledText("Step content");
        facet::EndOnboarding();
    }

    SectionHeader("Drag and drop");
    facet::demo::Paragraph("BeginDragSource and BeginDropTarget mark the last item. They return false until the "
                           "drag and drop core exists.");
    LabeledRow("BeginDragSource");
    facet::StyledText("Drag source");
    if (facet::BeginDragSource(kDemoPayload, &ui.slot, sizeof(ui.slot), {.previewText = "Item 1"}))
        facet::EndDragSource();
    LabeledRow("BeginDropTarget");
    facet::StyledText("Drop target");
    if (facet::BeginDropTarget())
    {
        facet::AcceptDrop(kDemoPayload, {.indicator = facet::DropIndicator::Box});
        facet::EndDropTarget();
    }
}
