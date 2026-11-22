// Demo page: Search and app controls. Calls every search, preset and app control composite of
// facet/widgets/composites.h once with neutral arguments. A planned widget draws the placeholder of
// its declared size; when it is implemented, its call here grows into a section with its variants
// and states, capture states and selftest steps. The theme controls are called with apply off, so
// they never fight the theme selector of the gallery. The inventory and cards and the editors have
// pages of their own (page_w_composites_cards.cpp and page_w_composites_editors.cpp).

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

// Height of the console view, in multiples of the xxxl spacing token.
constexpr float kViewSpans = 4.0f;
// Range of the example UI scale control (1 is 100 percent).
constexpr float kScaleMin = 0.75f;
constexpr float kScaleMax = 2.0f;

// Placeholder data: names like "Preset 1", nothing real.
const facet::LogEntry kConsoleLines[] = {{"Output line 1"}, {"Output line 2", facet::LogLevel::Warning}};
const char* const kCommands[] = {"command-1", "command-2"};
const facet::PresetItem kPresets[] = {{"Preset 1", false, true}, {"Preset 2", true}, {"Preset 3"}};

struct SettingsUi
{
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
    bool onboarding = false;
    int onboardingStep = 0;
};

SettingsUi ui;

} // namespace

FACET_DEMO_PAGE("composites-settings", "Search and app controls", "Widgets", 142)
{
    const facet::Theme& theme = facet::T();
    const float viewHeight = theme.spacing.xxxl * kViewSpans;

    facet::demo::PageHeader("Search and app controls",
                            "Search filter bars, settings search, key bind and preset lists, theme mode, accent and "
                            "UI scale controls, onboarding and console views.");
    facet::demo::PlannedFamilyNote("composites");

    SectionHeader("Search and settings");
    LabeledRow("Search filter bar");
    facet::SearchFilterBar("Search##filter bar", &ui.query, ui.filters);
    ctx.Probe("composites.search");
    LabeledRow("Settings search");
    ImGui::BeginGroup();
    facet::BeginSettingsSearch("settings", {.filter = ui.settingsQuery.c_str()});
    if (facet::SettingsSearchMatch("Setting 1", {.keywords = "placeholder"}))
        facet::StyledText("Setting 1", {.secondary = true});
    facet::EndSettingsSearch();
    ImGui::EndGroup();
    LabeledRow("Key bind list");
    facet::KeybindList("binds", ui.binds);
    LabeledRow("Preset list");
    facet::PresetList("presets", &ui.preset, kPresets, {.importExport = true});

    SectionHeader("App controls");
    LabeledRow("Theme mode control");
    facet::ThemeModeControl("Theme##composites", &ui.themeMode, {.apply = false});
    LabeledRow("Accent picker");
    facet::AccentPicker("Accent##composites", &ui.themeAccent, &ui.accent, {.apply = false});
    LabeledRow("UI scale control");
    facet::UiScaleControl("UI scale##composites", &ui.uiScale, kScaleMin, kScaleMax, {.apply = false});

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

    SectionHeader("Console");
    LabeledRow("Console view");
    facet::ConsoleView("console", &ui.command, kConsoleLines, {.commands = kCommands, .height = viewHeight});
}

FACET_DEMO_SELFTEST("composites-settings")
{
    test.Do([] { ui = SettingsUi{}; });
    // The search filter bar placeholder writes nothing.
    test.Click("composites.search");
    test.Expect("a click on the search filter bar placeholder leaves the query empty", [] { return ui.query.empty(); });

    // The stock button that shows the onboarding placeholder, by mouse and by keyboard.
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
}
