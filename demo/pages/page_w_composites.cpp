// Demo page: Composites. Calls every declared composite widget once with neutral arguments,
// grouped by purpose. Until Phase 2 implements a widget, its call draws the placeholder of the
// declared size; Phase 2 replaces the placeholders one by one. The theme controls are called
// with apply off, so they never fight the theme selector of the gallery.

#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"
#include "selftest.h"

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
const facet::InventoryStat kStats[] = {{"Stat 1", 4.0, 3.0}, {"Stat 2", 2.0}};
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
    bool themeAccent = true;
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
                            "cards, people, preview canvases, layer lists, onboarding and the drag and drop calls.");
    facet::demo::PlannedFamilyNote("composites");

    SectionHeader("Editors");
    LabeledRow("Curve editor");
    facet::CurveEditor("Response", ui.curve, &ui.curveCount, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.marker = 0.5f});
    LabeledRow("Bezier editor");
    facet::BezierEditor("Easing", &ui.easing);
    LabeledRow("Gradient editor");
    facet::GradientEditor("Gradient", ui.stops, &ui.stopCount, {.alpha = true});

    SectionHeader("Inspectors");
    LabeledRow("Property grid");
    ImGui::BeginGroup();  // keeps the grid in the control column of the row
    facet::BeginPropertyGrid("properties");
    if (facet::BeginPropertyCategory("Category 1"))
    {
        if (facet::BeginPropertyRow("Property 1", {.tooltip = "Placeholder description"}))
        {
            ImGui::Checkbox("##value", &ui.option);
            facet::ItemCursor();
            ctx.Probe("composites.property");
            facet::EndPropertyRow();
        }
        facet::EndPropertyCategory();
    }
    facet::EndPropertyGrid();
    ImGui::EndGroup();
    LabeledRow("Console view");
    facet::ConsoleView("console", &ui.command, kConsoleLines, {.commands = kCommands, .height = viewHeight});
    LabeledRow("Key bind list");
    facet::KeybindList("binds", ui.binds);

    SectionHeader("Search and settings");
    LabeledRow("Search filter bar");
    facet::SearchFilterBar("Search##filter bar", &ui.query, ui.filters);
    LabeledRow("Settings search");
    ImGui::BeginGroup();
    facet::BeginSettingsSearch("settings", {.filter = ui.settingsQuery.c_str()});
    if (facet::SettingsSearchMatch("Setting 1", {.keywords = "placeholder"}))
        facet::StyledText("Setting 1", {.secondary = true});
    facet::EndSettingsSearch();
    ImGui::EndGroup();
    LabeledRow("Preset list");
    facet::PresetList("presets", &ui.preset, kPresets, {.importExport = true});
    LabeledRow("Theme mode control");
    facet::ThemeModeControl("Theme##composites", &ui.themeMode, {.apply = false});
    LabeledRow("Accent picker");
    facet::AccentPicker("Accent##composites", &ui.themeAccent, &ui.accent, {.apply = false});
    LabeledRow("UI scale control");
    facet::UiScaleControl("UI scale##composites", &ui.uiScale, kScaleMin, kScaleMax, {.apply = false});

    SectionHeader("Inventory");
    LabeledRow("Inventory grid");
    facet::InventoryGrid("inventory", &ui.slot, kSlots, {.columns = 3});
    LabeledRow("Inventory tile");
    facet::InventoryTile("tile", kSlots[0], {.selected = true});
    LabeledRow("Inventory slot");
    facet::InventorySlot("Slot 1", facet::InventoryItem{}, {.placeholderIcon = facet::Icon("grid")});
    LabeledRow("Inventory detail card");
    facet::InventoryDetailCard("Item 1", facet::ImageHandle{},
                          {.description = "Placeholder description.", .stats = kStats, .tags = kTags,
                           .actions = kActions});

    SectionHeader("Cards and people");
    LabeledRow("Media card");
    facet::MediaCard("Title", facet::ImageHandle{}, {.description = "Detail", .badges = kTags});
    LabeledRow("User chip");
    facet::UserChip("Profile 1", facet::ImageHandle{}, {.description = "Placeholder"});
    LabeledRow("Profile switcher");
    facet::ProfileSwitcher("profiles", &ui.profile, kProfiles, {.addLabel = "Add profile"});

    SectionHeader("Canvas and layers");
    LabeledRow("Preview canvas");
    ImGui::BeginGroup();
    facet::BeginPreviewCanvas("canvas", kReferenceSize, {.safeArea = 0.05f});
    facet::CanvasElement("Element 1", &ui.element);
    facet::EndPreviewCanvas();
    ImGui::EndGroup();
    LabeledRow("Layer list");
    facet::LayerList("layers", &ui.layer, ui.layers);

    SectionHeader("Onboarding");
    LabeledRow("Onboarding");
    // The text changes with the state, so "###" keeps one id and the button keeps its focus.
    if (ImGui::Button(ui.onboarding ? "Hide onboarding###onboarding" : "Show onboarding###onboarding"))
        ui.onboarding = !ui.onboarding;
    ctx.Probe("composites.onboarding");
    facet::ItemCursor();
    if (facet::BeginOnboarding("Welcome", &ui.onboarding, &ui.onboardingStep, 3))
    {
        facet::StyledText("Step content");
        facet::EndOnboarding();
    }

    SectionHeader("Drag and drop");
    facet::demo::Paragraph("BeginDragSource and BeginDropTarget mark the last item. They return false until Phase 2 builds "
                           "them on the drag and drop core.");
    LabeledRow("Drag source");
    facet::StyledText("Drag source");
    if (facet::BeginDragSource(kDemoPayload, &ui.slot, sizeof(ui.slot), {.previewText = "Item 1"}))
        facet::EndDragSource();
    LabeledRow("Drop target");
    facet::StyledText("Drop target");
    if (facet::BeginDropTarget())
    {
        facet::AcceptDrop(kDemoPayload, {.indicator = facet::DropIndicator::Box});
        facet::EndDropTarget();
    }
}

FACET_DEMO_SELFTEST("composites")
{
    test.Do([] { ui.onboarding = false; });
    test.Click("composites.onboarding");
    test.Expect("the button shows the onboarding placeholder", [] { return ui.onboarding; });
    test.ExpectFocused("composites.onboarding");
    // Shift+Tab and Tab come back to the button with the keyboard cursor shown.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.onboarding");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused button hides it again", [] { return !ui.onboarding; });
    test.ExpectFocused("composites.onboarding");
    test.Do([] { ui.onboarding = false; });

    // The checkbox inside the property row, by mouse and by keyboard.
    test.Do([] { ui.option = false; });
    test.Click("composites.property");
    test.Expect("a click checks the property checkbox", [] { return ui.option; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.property");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on it clears it again", [] { return !ui.option; });
}
