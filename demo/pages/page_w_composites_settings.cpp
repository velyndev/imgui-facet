// Demo page: Search and app controls. Each ready widget of the search, preset and app control
// composites (facet/widgets/composites.h) has a section with its variants and states, capture
// states for its important looks and selftest steps that operate it by mouse and keyboard; the
// widgets that are still planned draw their placeholder. The theme controls run with apply off, so
// they never fight the theme and scale selectors of the gallery: each shows what it chose next to
// it (the accent in a preview button under PushAccent). The inventory and cards and the editors
// have pages of their own (page_w_composites_cards.cpp and page_w_composites_editors.cpp).

#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "selftest.h"

namespace
{

using facet::demo::DemoWidth;
using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::MixedHeightRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Height of the console view, in multiples of the xxxl spacing token.
constexpr float kViewSpans = 4.0f;
// Range of the example UI scale controls (1 is 100 percent).
constexpr float kScaleMin = 0.75f;
constexpr float kScaleMax = 2.0f;
// Widths of the example controls, in design units (DemoWidth keeps them inside the row).
constexpr float kThemeWidth = 260.0f;
constexpr float kThemeNarrowWidth = 96.0f;
constexpr float kScaleWidth = 320.0f;

// Placeholder data: names like "Preset 1", nothing real.
const facet::LogEntry kConsoleLines[] = {{"Output line 1"}, {"Output line 2", facet::LogLevel::Warning}};
const char* const kCommands[] = {"command-1", "command-2"};
const facet::PresetItem kPresets[] = {{"Preset 1", false, true}, {"Preset 2", true}, {"Preset 3"}};

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kAppFocus,    // the theme mode control focused, the accent swatches hovered
    kAccentOpen,  // the accent picker's custom color well with its popover open
    kSearchFocus, // the search filter bar's field focused
};

struct SettingsUi
{
    std::string command;

    // SearchFilterBar
    std::string query;
    facet::FilterChip filters[3] = {{"Group A"}, {"Group B"}, {"Group C"}};
    char narrowQuery[64] = "";
    facet::FilterChip narrowFilters[4] = {{"Group A"}, {"Group B"}, {"Group C"}, {"Group D"}};
    int filterWrites = 0;
    int filterClickChip = 2;  // the chip under the probe "comp.filter.chipclick", set every frame
    ImGuiID filterClickChipId = 0;

    std::string settingsQuery;
    facet::KeybindListItem binds[2] = {
        {"Action 1", "Group 1", {ImGuiMod_Ctrl | ImGuiKey_1}, {ImGuiMod_Ctrl | ImGuiKey_1}},
        {"Action 2", "Group 1", {ImGuiMod_Ctrl | ImGuiKey_2}, {ImGuiMod_Ctrl | ImGuiKey_2}},
    };
    int preset = 0;

    // ThemeModeControl
    facet::ThemeMode themeMode = facet::ThemeMode::System;
    facet::ThemeMode themeSizes[3] = {facet::ThemeMode::Light, facet::ThemeMode::Dark, facet::ThemeMode::System};
    facet::ThemeMode themeNarrow = facet::ThemeMode::Dark;
    int themeWrites = 0;

    // AccentPicker
    // The well starts on the first preset, so it never shows a color nobody chose.
    bool themeAccent = true;
    ImVec4 accent = facet::AccentPresets()[0].color;
    ImVec4 accentPlain = facet::AccentPresets()[1].color;
    int accentWrites = 0;
    int previewPresses = 0;

    // UiScaleControl
    float uiScale = 1.0f;
    float uiScaleSmall = 1.25f;
    bool wheelZoom = false;
    int scaleWrites = 0;

    bool onboarding = false;
    int onboardingStep = 0;
};

SettingsUi ui;

// Records "<probe>.<i>" for the `count` segments of the SegmentedControl-based widget `label` just
// submitted: equal segments over the item rect (the widgets are given a width), with the ids the
// segmented control gives its segments (the index hashed with the widget id).
void ProbeSegments(facet::demo::PageCtx& ctx, const char* label, const char* probe, int count)
{
    if (ctx.probes == nullptr)
        return;
    const ImGuiID widget = ImGui::GetID(label);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float w = (max.x - min.x) / static_cast<float>(count);
    char name[64];
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(name, sizeof(name), "%s.%d", probe, i);
        facet::demo::ProbeRect(ctx, name, ImHashData(&i, sizeof(i), widget),
                               ImVec2(min.x + w * static_cast<float>(i), min.y),
                               ImVec2(min.x + w * static_cast<float>(i + 1), max.y));
    }
}

// Records "<probe>.<i>" for the first `count` swatches of the AccentPicker just submitted, which
// has no visible label: swatches one control height square, xs apart, from the left edge.
void ProbeSwatches(facet::demo::PageCtx& ctx, const char* probe, int count, facet::ControlSize size)
{
    if (ctx.probes == nullptr)
        return;
    const ImVec2 min = ImGui::GetItemRectMin();
    const float side = std::round(facet::ControlHeight(size));
    const float gap = std::round(facet::Px(facet::T().spacing.xs));
    char name[64];
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(name, sizeof(name), "%s.%d", probe, i);
        const float x = min.x + static_cast<float>(i) * (side + gap);
        facet::demo::ProbeRect(ctx, name, 0, ImVec2(x, min.y), ImVec2(x + side, min.y + side));
    }
}

// Records "<probe>.out" and "<probe>.in" for the two buttons at the end of the UiScaleControl just
// submitted, with the ids the control gives them ("###zoomOut" and "###zoomIn" under its id).
void ProbeZoomButtons(facet::demo::PageCtx& ctx, const char* label, const char* probe, facet::ControlSize size)
{
    if (ctx.probes == nullptr)
        return;
    const ImGuiID widget = ImGui::GetID(label);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float side = std::round(facet::ControlHeight(size));
    const float gap = std::round(facet::Px(facet::T().spacing.xs));
    char name[64];
    std::snprintf(name, sizeof(name), "%s.in", probe);
    facet::demo::ProbeRect(ctx, name, ImHashStr("###zoomIn", 0, widget), ImVec2(max.x - side, min.y),
                           ImVec2(max.x, min.y + side));
    std::snprintf(name, sizeof(name), "%s.out", probe);
    facet::demo::ProbeRect(ctx, name, ImHashStr("###zoomOut", 0, widget), ImVec2(max.x - 2.0f * side - gap, min.y),
                           ImVec2(max.x - side - gap, min.y + side));
}

const char* ThemeModeName(facet::ThemeMode mode)
{
    const facet::Strings& s = facet::GetStrings();
    switch (mode)
    {
    case facet::ThemeMode::Light: return s.themeLight;
    case facet::ThemeMode::Dark: return s.themeDark;
    case facet::ThemeMode::System: return s.themeSystem;
    }
    return "";
}

void ThemeModeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("ThemeModeControl");
    Paragraph("Light, Dark and System as a segmented control. With apply on it calls SetThemeMode once per "
              "change and the core animates the switch; here apply is off and the choice shows beside it.");
    const auto count = [](bool written) {
        if (written)
            ++ui.themeWrites;
    };
    LabeledRow("Theme");
    if (ctx.capture && ctx.captureState == kAppFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    count(facet::ThemeModeControl("##theme", &ui.themeMode, {.apply = false, .width = DemoWidth(kThemeWidth)}));
    ctx.Probe("comp.theme");
    ProbeSegments(ctx, "##theme", "comp.theme", 3);
    ImGui::SameLine();
    facet::demo::NoteBeside("Chosen: %s, %d changes", ThemeModeName(ui.themeMode), ui.themeWrites);

    const float large = facet::ControlHeight(facet::ControlSize::Large);
    LabeledRow("Sizes", large);
    {
        MixedHeightRow row(large);
        const facet::ControlSize sizes[3] = {facet::ControlSize::Small, facet::ControlSize::Regular, facet::ControlSize::Large};
        for (int i = 0; i < 3; ++i)
        {
            ImGui::PushID(i);
            row.Item(facet::ControlHeight(sizes[i]));
            count(facet::ThemeModeControl("##size", &ui.themeSizes[i], {.apply = false, .size = sizes[i]}));
            ImGui::PopID();
        }
    }
    LabeledRow("Narrow, icons");
    count(facet::ThemeModeControl("##narrow", &ui.themeNarrow, {.apply = false, .width = DemoWidth(kThemeNarrowWidth)}));
    ctx.Probe("comp.theme.narrow");
    LabeledRow("Read-only");
    facet::ThemeMode readOnly = facet::ThemeMode::Dark;
    facet::ThemeModeControl("##readonly", &readOnly, {.apply = false, .width = DemoWidth(kThemeWidth), .readOnly = true});
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::ThemeMode disabled = facet::ThemeMode::Light;
    facet::ThemeModeControl("##disabled", &disabled, {.apply = false, .width = DemoWidth(kThemeWidth)});
    ImGui::EndDisabled();
}

void AccentSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("AccentPicker");
    Paragraph("The accent presets as swatches plus a well for any color. The first swatch is each theme's own "
              "accent, a value of its own. With apply on it calls ClearAccent or SetAccent; here the preview "
              "button shows the choice under PushAccent.");
    const int presetCount = static_cast<int>(facet::AccentPresets().size());
    LabeledRow("Accent");
    if (ctx.capture && ctx.captureState == kAppFocus)
        ctx.ForceNextItemState(ForcedItemState::Hovered, 2);
    if (ctx.capture && ctx.captureState == kAccentOpen)
        ctx.ForceNextItemState(ForcedItemState::Open, presetCount);
    if (facet::AccentPicker("##accent", &ui.themeAccent, &ui.accent, {.apply = false}))
        ++ui.accentWrites;
    ctx.Probe("comp.accent");
    ProbeSwatches(ctx, "comp.accent", presetCount, facet::ControlSize::Inherit);
    if (ctx.capture && ctx.captureState == kAccentOpen)
    {
        ctx.RevealInCapture(0.2f);
        // The well is "##custom" under the picker's id, its popover "##popover" under the well's.
        const ImGuiID well = ImHashStr("##custom", 0, ImGui::GetID("##accent"));
        if (ctx.captureFramesLeft == 0 && !facet::internal::IsPopupLevelOpen(ImHashStr("##popover", 0, well)))
            ctx.captureError = "the custom color popover did not open";
    }

    LabeledRow("Preview");
    if (!ui.themeAccent)
        facet::PushAccent(ui.accent);
    if (facet::Button("Preview button", {.variant = facet::ButtonVariant::Primary}))
        ++ui.previewPresses;
    ctx.Probe("comp.accent.preview");
    if (!ui.themeAccent)
        facet::PopAccent();
    ImGui::SameLine();
    facet::demo::NoteBeside(ui.themeAccent ? "Theme default, %d changes" : "Custom accent, %d changes", ui.accentWrites);

    LabeledRow("No theme default");
    facet::AccentPicker("##plain", nullptr, &ui.accentPlain, {.customColor = false, .apply = false});
    ctx.Probe("comp.accent.plain");
    LabeledRow("Small");
    bool smallDefault = ui.themeAccent;
    ImVec4 smallAccent = ui.accent;
    facet::AccentPicker("##small", &smallDefault, &smallAccent, {.apply = false, .size = facet::ControlSize::Small, .readOnly = true});
    Note("Small and read-only: it mirrors the first picker.");
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    bool disabledDefault = true;
    ImVec4 disabledAccent = ui.accent;
    facet::AccentPicker("##disabled", &disabledDefault, &disabledAccent, {.apply = false});
    ImGui::EndDisabled();
}

void UiScaleSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("UiScaleControl");
    Paragraph("The user UI scale in steps, shown as percent, with zoom out and zoom in buttons. With apply on a "
              "drag applies once on release and every other change at once; here apply is off.");
    LabeledRow("UI scale");
    if (facet::UiScaleControl("##scale", &ui.uiScale, kScaleMin, kScaleMax,
                              {.wheelZoom = ui.wheelZoom, .apply = false, .width = DemoWidth(kScaleWidth)}))
        ++ui.scaleWrites;
    ctx.Probe("comp.scale");
    ProbeZoomButtons(ctx, "##scale", "comp.scale", facet::ControlSize::Inherit);
    LabeledRow("Ctrl+wheel");
    facet::Toggle("##wheel", &ui.wheelZoom);
    ctx.Probe("comp.scale.wheel");
    ImGui::SameLine();
    facet::demo::NoteBeside("Ctrl+wheel anywhere changes the scale above, %d changes", ui.scaleWrites);
    LabeledRow("Small, step 0.05");
    facet::UiScaleControl("##fine", &ui.uiScaleSmall, kScaleMin, kScaleMax,
                          {.step = 0.05f, .apply = false, .size = facet::ControlSize::Small, .width = DemoWidth(kScaleWidth)});
    LabeledRow("Read-only");
    float readOnly = 1.5f;
    facet::UiScaleControl("##readonly", &readOnly, kScaleMin, kScaleMax,
                          {.apply = false, .width = DemoWidth(kScaleWidth), .readOnly = true});
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    float disabled = 1.0f;
    facet::UiScaleControl("##disabled", &disabled, kScaleMin, kScaleMax, {.apply = false, .width = DemoWidth(kScaleWidth)});
    ImGui::EndDisabled();
}

// The placeholder rows the search filter bar filters: "Item 1" to "Item 12", each in one of three
// groups (the item number modulo 3), which the chips "Group A" to "Group C" select.
constexpr int kFilterItems = 12;

bool FilterItemShown(int index, const std::string& query, const facet::FilterChip* chips, int chipCount)
{
    char label[32];
    std::snprintf(label, sizeof(label), "Item %d", index + 1);
    bool anyChip = false;
    bool inChip = false;
    for (int c = 0; c < chipCount; ++c)
    {
        anyChip = anyChip || chips[c].active;
        inChip = inChip || (chips[c].active && index % chipCount == c);
    }
    return (!anyChip || inChip) && facet::PassesFilter(query, {}, label);
}

int FilterItemCount(const std::string& query, const facet::FilterChip* chips, int chipCount)
{
    int count = 0;
    for (int i = 0; i < kFilterItems; ++i)
        count += FilterItemShown(i, query, chips, chipCount) ? 1 : 0;
    return count;
}

void SearchFilterBarSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("SearchFilterBar");
    Paragraph("A search field with filter chips and a result count. It filters the placeholder rows Item 1 to "
              "Item 12; the chips pick their group. When the field would get narrower than its minimum the chips "
              "move under it, and chips that do not fit their line move into a More menu.");
    const int chipCount = static_cast<int>(std::size(ui.filters));
    const int count = FilterItemCount(ui.query, ui.filters, chipCount);
    LabeledRow("Search");
    if (ctx.capture && ctx.captureState == kSearchFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::SearchFilterBar("##filter", &ui.query, ui.filters, {.resultCount = count}))
        ++ui.filterWrites;
    ctx.Probe("comp.filter");
    if (ctx.capture && ctx.captureState == kSearchFocus)
        ctx.RevealInCapture(0.3f);
    if (ctx.probes != nullptr)
    {
        // The chips' ids: their index hashed with the chip group "##chips" under the bar's id.
        const ImGuiID chips = ImHashStr("##chips", 0, ImGui::GetID("##filter"));
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        char name[64];
        for (int i = 0; i < chipCount; ++i)
        {
            std::snprintf(name, sizeof(name), "comp.filter.chip.%d", i);
            facet::demo::ProbeRect(ctx, name, ImHashData(&i, sizeof(i), chips), min, max);
        }
        // Points the script clicks: the start of the field, and a chip that is easy to find from the
        // bar's edges (the last chip at the end of a one-line bar, else the first chip of the second
        // line).
        const float line = facet::ControlHeight(facet::ControlSize::Inherit);
        const float inset = facet::Px(16.0f);
        const bool twoLines = max.y - min.y > line * 1.5f;
        ui.filterClickChip = twoLines ? 0 : chipCount - 1;
        ui.filterClickChipId = ImHashData(&ui.filterClickChip, sizeof(int), chips);
        const ImVec2 chip = twoLines ? ImVec2(min.x + inset, max.y - line * 0.5f) : ImVec2(max.x - inset, min.y + line * 0.5f);
        facet::demo::ProbeRect(ctx, "comp.filter.chipclick", 0, ImVec2(chip.x - 1.0f, chip.y - 1.0f),
                               ImVec2(chip.x + 1.0f, chip.y + 1.0f));
        facet::demo::ProbeRect(ctx, "comp.filter.field", ImGui::GetID("##filter"), min,
                               ImVec2(min.x + inset * 4.0f, min.y + line));
    }
    LabeledRow("Rows");
    {
        char text[256] = "";
        int length = 0;
        for (int i = 0; i < kFilterItems; ++i)
        {
            if (FilterItemShown(i, ui.query, ui.filters, chipCount))
                length += std::snprintf(text + length, sizeof(text) - static_cast<std::size_t>(length), "%sItem %d",
                                        length > 0 ? ", " : "", i + 1);
        }
        facet::StyledText(length > 0 ? text : facet::GetStrings().noResults, {.secondary = true, .wrap = true});
    }
    LabeledRow("Narrow");
    facet::SearchFilterBar("##narrow", ui.narrowQuery, sizeof(ui.narrowQuery), ui.narrowFilters,
                           {.placeholder = "Placeholder search", .width = DemoWidth(260.0f)});
    ctx.Probe("comp.filter.narrow");
    LabeledRow("Read-only");
    std::string readOnlyQuery = "Item";
    facet::FilterChip readOnlyChips[] = {{"Group A", {}, true}, {"Group B"}};
    facet::SearchFilterBar("##readonly", &readOnlyQuery, readOnlyChips, {.resultCount = 1, .readOnly = true});
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    std::string disabledQuery;
    facet::FilterChip disabledChips[] = {{"Group A"}, {"Group B"}};
    facet::SearchFilterBar("##disabled", &disabledQuery, disabledChips);
    ImGui::EndDisabled();
}

} // namespace

FACET_DEMO_PAGE("composites-settings", "Search and app controls", "Widgets", 142, "default", "app controls focused",
                "accent popover", "search focused")
{
    const facet::Theme& theme = facet::T();
    const float viewHeight = theme.spacing.xxxl * kViewSpans;

    facet::demo::PageHeader("Search and app controls",
                            "Search filter bars, settings search, key bind and preset lists, theme mode, accent and "
                            "UI scale controls, onboarding and console views.");
    facet::demo::PlannedFamilyNote("composites");

    ThemeModeSection(ctx);
    AccentSection(ctx);
    UiScaleSection(ctx);

    SearchFilterBarSection(ctx);

    SectionHeader("Search and settings");
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
    using T = facet::demo::Target;
    test.Do([] { ui = SettingsUi{}; });

    // ThemeModeControl: a click on the middle segment, then the arrow keys.
    test.Click("comp.theme.1");
    test.Expect("a click on Dark writes Dark", [] { return ui.themeMode == facet::ThemeMode::Dark; });
    test.Expect("the click wrote once", [] { return ui.themeWrites == 1; });
    test.ExpectFocused("comp.theme.1");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right moves to System", [] { return ui.themeMode == facet::ThemeMode::System; });
    test.ExpectFocused("comp.theme.2");
    test.KeyPress(ImGuiKey_Home);
    test.Expect("Home moves to Light", [] { return ui.themeMode == facet::ThemeMode::Light; });
    test.Expect("three changes so far", [] { return ui.themeWrites == 3; });

    // AccentPicker: a preset by mouse, then the theme default by keyboard.
    test.Click("comp.accent.3");
    test.Expect("a click on the fourth swatch writes its color", [] {
        return !ui.themeAccent && facet::ToU32(ui.accent) == facet::ToU32(facet::AccentPresets()[3].color);
    });
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Home and Enter choose the theme default", [] { return ui.themeAccent; });
    test.Expect("the custom color stays when the theme default is chosen",
                [] { return facet::ToU32(ui.accent) == facet::ToU32(facet::AccentPresets()[3].color); });
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Right twice and Space choose the third swatch", [] {
        return !ui.themeAccent && facet::ToU32(ui.accent) == facet::ToU32(facet::AccentPresets()[2].color);
    });
    test.Expect("three accent changes", [] { return ui.accentWrites == 3; });
    test.Click("comp.accent.preview");
    test.Expect("the preview button works under the chosen accent", [] { return ui.previewPresses == 1; });

    // UiScaleControl: the buttons by mouse, the slider by keyboard, then Ctrl+wheel.
    test.Click("comp.scale.in");
    test.ExpectNear("zoom in steps to 125 %", [] { return ui.uiScale; }, 1.25, 0.001);
    test.Click("comp.scale.out");
    test.Click("comp.scale.out");
    test.ExpectNear("zoom out twice steps to 75 %", [] { return ui.uiScale; }, 0.75, 0.001);
    test.Click("comp.scale.out");
    test.ExpectNear("the minimum holds", [] { return ui.uiScale; }, 0.75, 0.001);
    test.ExpectFocused("comp.scale.out");
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("comp.scale");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End on the slider goes to 200 %", [] { return ui.uiScale; }, 2.0, 0.001);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left steps back to 175 %", [] { return ui.uiScale; }, 1.75, 0.001);
    test.Click("comp.scale.wheel");
    test.Expect("the toggle turns Ctrl+wheel on", [] { return ui.wheelZoom; });
    test.KeyDown(ImGuiMod_Ctrl);
    test.Wheel(T::Item("comp.accent.preview"), -1.0f);
    test.KeyUp(ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+wheel down steps to 150 %", [] { return ui.uiScale; }, 1.5, 0.001);

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

    // SearchFilterBar: typing, Escape, a chip by mouse and by keyboard.
    test.Click("comp.filter.field");
    test.Type("1");
    test.ExpectText("typing writes the query", [] { return ui.query; }, "1");
    test.Expect("four rows match 1", [] { return FilterItemCount(ui.query, ui.filters, 3) == 4; });
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape restores the query from activation", [] { return ui.query; }, "");
    test.Click("comp.filter.chipclick");
    test.Expect("a click on a chip turns its group on", [] {
        int on = 0;
        for (const facet::FilterChip& chip : ui.filters)
            on += chip.active ? 1 : 0;
        return on == 1 && ui.filters[ui.filterClickChip].active;
    });
    test.Expect("one group has four rows", [] { return FilterItemCount(ui.query, ui.filters, 3) == 4; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus on the chip
    test.Expect("the focus shows on the clicked chip", [] { return GImGui->NavId == ui.filterClickChipId; });
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the clicked chip turns its group off again",
                [] { return !ui.filters[0].active && !ui.filters[1].active && !ui.filters[2].active; });
    test.Click("comp.filter.field");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("comp.filter.chip.0");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Tab from the field reaches Group A, Space turns it on", [] { return ui.filters[0].active; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter turns Group A off again", [] { return !ui.filters[0].active; });
    test.Expect("six writes in the bar: a key, Escape and four chips", [] { return ui.filterWrites == 6; });
}
