// Demo page: Search and app controls. Each widget of the search, preset and app control composites
// (facet/widgets/composites.h) has a section with its variants and states, capture states for its
// important looks and selftest steps that operate it by mouse and keyboard. The theme controls run
// with apply off, so they never fight the theme and scale selectors of the gallery: each shows what it chose next to
// it (the accent in a preview button under PushAccent). The inventory and cards and the editors
// have pages of their own (page_w_composites_cards.cpp and page_w_composites_editors.cpp).

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "selftest.h"
#include "widgets/composites/keybind_list.h"

namespace
{

using facet::demo::DemoWidth;
using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::MixedHeightRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Heights of the console views, in design units.
constexpr float kConsoleHeight = 200.0f;
constexpr float kConsoleReadOnlyHeight = 110.0f;
// Range of the example UI scale controls (1 is 100 percent).
constexpr float kScaleMin = 0.75f;
constexpr float kScaleMax = 2.0f;
// Widths of the example controls, in design units (DemoWidth keeps them inside the row).
constexpr float kThemeWidth = 260.0f;
constexpr float kThemeNarrowWidth = 96.0f;
constexpr float kScaleWidth = 320.0f;

// The log of the read-only console: placeholder lines.
const facet::LogEntry kConsoleReadOnly[] = {{"Placeholder line 1"}, {"Placeholder line 2", facet::LogLevel::Warning}};

// The page's presets: placeholder names, at most kMaxPresets, so creating one never allocates.
constexpr int kMaxPresets = 8;
struct DemoPreset
{
    char name[48] = "";
    bool modified = false;
    bool builtIn = false;
    ImGuiID id = 0;
};

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kAppFocus,    // the theme mode control focused, the accent swatches hovered
    kAccentOpen,  // the accent picker's custom color well with its popover open
    kSearchFocus, // the search filter bar's field focused
    kSettingsHits,  // a settings search query with one hit far down, revealed and highlighted
    kPresetMenu,    // the preset list's menu open
    kBindsFocus,    // the second row's capture field of the key bind list focused
    kConsoleFocus,  // the console's command field focused
    kOnboardingOpen,  // the onboarding panel open on its second step
};

// Steps of the page's onboarding panel.
constexpr int kOnboardingSteps = 3;

// Rows of the page's key bind list, and its width in design units.
constexpr int kBindCount = 5;
constexpr float kBindsWidth = 560.0f;

// Frames before the end of a capture's warm-up at which the settings search capture types its
// query: the reveal scroll settles and the highlight still holds when the image is taken.
constexpr int kSettingsCaptureFrames = 30;
// Height of the scrolling area of example settings rows, in design units.
constexpr float kSettingsAreaHeight = 150.0f;

// The console's log and history: fixed rings, so a command never allocates.
constexpr int kConsoleLines = 64;
constexpr int kConsoleLineText = 96;
constexpr int kConsoleHistory = 16;
constexpr int kGeneratedLines = 10000;

struct ConsoleLine
{
    char text[kConsoleLineText] = "";
    facet::LogLevel level = facet::LogLevel::Info;
};

struct SettingsUi
{
    // ConsoleView
    std::string command;
    ConsoleLine consoleLines[kConsoleLines];
    int consoleStart = 0;
    int consoleCount = 0;
    char history[kConsoleHistory][64] = {};
    int historyCount = 0;
    int consoleState = 0;  // DataState
    bool consoleGenerated = false;

    // SearchFilterBar
    std::string query;
    facet::FilterChip filters[3] = {{"Group A"}, {"Group B"}, {"Group C"}};
    char narrowQuery[64] = "";
    facet::FilterChip narrowFilters[4] = {{"Group A"}, {"Group B"}, {"Group C"}, {"Group D"}};
    int filterWrites = 0;
    int filterClickChip = 2;  // the chip under the probe "comp.filter.chipclick", set every frame
    ImGuiID filterClickChipId = 0;


    // SettingsSearch
    std::string settingsQuery;
    bool settingsFlags[10] = {true, false, false, false, true, false, false, false, false, true};
    float settingsValues[10] = {0.5f, 0.5f, 0.5f, 0.8f, 0.5f, 0.5f, 0.5f, 0.4f, 0.5f, 0.5f};


    // KeybindList (filled by DefaultBinds on first use)
    facet::KeybindListItem binds[kBindCount] = {};
    bool bindsReady = false;
    int bindState = 0;  // DataState
    std::string bindFilter;
    int bindWrites = 0;
    float registryHeadingX = 0.0f;  // the registry list's Primary heading, set every frame
    ImRect registryFirstField;      // and its first primary field

    // PresetList
    DemoPreset presets[kMaxPresets] = {{"Preset 1", false, true, 1}, {"Preset 2", true, false, 2}, {"Preset 3", false, false, 3}};
    int presetCount = 3;
    int preset = 0;
    ImGuiID nextPresetId = 4;
    int presetState = 0;  // DataState
    facet::PresetAction presetLast = facet::PresetAction::None;


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


    // Onboarding
    bool onboarding = false;
    int onboardingStep = 0;
    bool onboardingSkippable = true;
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

// The page's own shortcuts: the zoom keys of the UI scale control, registered once, so the key
// bind list over the registry has entries whatever page was shown before.
struct PageShortcuts
{
    facet::ShortcutId zoomIn;
    facet::ShortcutId zoomOut;
    facet::ShortcutId zoomReset;
};

const PageShortcuts& Shortcuts()
{
    static const PageShortcuts shortcuts = [] {
        PageShortcuts s;
        const facet::ShortcutOptions group{.group = "Search and app controls page"};
        facet::ShortcutOptions in = group;
        in.label = "Zoom the example scale in";
        facet::ShortcutOptions out = group;
        out.label = "Zoom the example scale out";
        facet::ShortcutOptions reset = group;
        reset.label = "Reset the example scale";
        s.zoomIn = facet::RegisterShortcut("demo.settings.zoom in", ImGuiMod_Ctrl | ImGuiKey_Equal, in);
        s.zoomOut = facet::RegisterShortcut("demo.settings.zoom out", ImGuiMod_Ctrl | ImGuiKey_Minus, out);
        s.zoomReset = facet::RegisterShortcut("demo.settings.zoom reset", ImGuiMod_Ctrl | ImGuiKey_0, reset);
        return s;
    }();
    return shortcuts;
}

void UiScaleSection(facet::demo::PageCtx& ctx)
{
    const PageShortcuts& shortcuts = Shortcuts();
    SectionHeader("UiScaleControl");
    Paragraph("The user UI scale in steps, shown as percent, with zoom out and zoom in buttons. With apply on a "
              "drag applies once on release and every other change at once; here apply is off.");
    LabeledRow("UI scale");
    if (facet::UiScaleControl("##scale", &ui.uiScale, kScaleMin, kScaleMax,
                              {.zoomInShortcut = shortcuts.zoomIn, .zoomOutShortcut = shortcuts.zoomOut,
                               .zoomResetShortcut = shortcuts.zoomReset, .wheelZoom = ui.wheelZoom, .apply = false,
                               .width = DemoWidth(kScaleWidth)}))
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

// Example settings rows for the settings search. They change only the demo values in ui.
struct ExampleSetting
{
    const char* label;
    const char* keywords;
    const char* section;
};

const ExampleSetting kExampleSettings[] = {
    {"Vertical sync", "vsync tearing", "Display"},
    {"Brightness", "gamma light", "Display"},
    {"Show frame counter", "fps overlay", "Display"},
    {"Master volume", "sound level", "Audio"},
    {"Mute when inactive", "sound background focus", "Audio"},
    {"Subtitles", "captions text", "Audio"},
    {"Invert vertical axis", "mouse look", "Controls"},
    {"Mouse sensitivity", "speed pointer", "Controls"},
    {"Reduce motion", "animation accessibility", "General"},
    {"Confirm before quitting", "exit dialog", "General"},
};
const char* const kExampleSections[] = {"Display", "Audio", "Controls", "General"};
constexpr const char* kSettingsSearchName = "demo settings";

void SettingsSearchSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("SettingsSearch");
    Paragraph("Decides which settings rows are submitted. The example rows below change only this page. When the "
              "query changes, the first hit is scrolled into view and highlighted for a moment; the counts per "
              "section come from the last frame, so a navigation drawn before the rows can show them.");
    // The capture state types a query near the end of the warm-up, so the highlight still shows.
    if (ctx.capture && ctx.captureState == kSettingsHits)
        ui.settingsQuery = ctx.captureFramesLeft > kSettingsCaptureFrames ? "" : "sens";
    LabeledRow("Search");
    facet::SearchField("##settings search", &ui.settingsQuery, {.width = DemoWidth(kScaleWidth)});
    ctx.Probe("comp.settings.field");

    LabeledRow("Hits per section");
    {
        char text[160];
        int length = 0;
        for (const char* section : kExampleSections)
            length += std::snprintf(text + length, sizeof(text) - static_cast<std::size_t>(length), "%s%s %d",
                                    length > 0 ? ", " : "", section, facet::SettingsSearchHitCount(kSettingsSearchName, section));
        facet::StyledText(text, {.secondary = true, .wrap = true});
    }

    LabeledRow("Rows");
    if (facet::BeginScrollArea("##settings rows", {.width = DemoWidth(kScaleWidth + 160.0f), .height = kSettingsAreaHeight}))
    {
        facet::BeginSettingsSearch(kSettingsSearchName, {.filter = ui.settingsQuery.c_str()});
        int shown = 0;
        for (int i = 0; i < static_cast<int>(std::size(kExampleSettings)); ++i)
        {
            const ExampleSetting& row = kExampleSettings[i];
            if (!facet::SettingsSearchMatch(row.label, {.keywords = row.keywords, .section = row.section}))
                continue;
            ++shown;
            facet::BeginSettingsRow(row.label);
            if (i % 3 == 1)
                facet::Slider("##value", &ui.settingsValues[i], 0.0f, 1.0f);
            else
                facet::Toggle("##value", &ui.settingsFlags[i]);
            facet::EndSettingsRow();
            char probe[48];
            std::snprintf(probe, sizeof(probe), "comp.settings.row.%d", i);
            ctx.Probe(probe);
        }
        if (shown == 0)
            facet::StyledText(facet::GetStrings().noResults, {.secondary = true});
        ctx.Probe("comp.settings.empty");
        facet::EndSettingsSearch();
        facet::EndScrollArea();
    }
    if (ctx.capture && ctx.captureState == kSettingsHits)
    {
        ctx.RevealInCapture(0.3f);
        if (ctx.captureFramesLeft == 0 && facet::SettingsSearchHitCount(kSettingsSearchName, nullptr) != 1)
            ctx.captureError = "the settings search did not find the one row the query names";
    }
}

const char* PresetActionName(facet::PresetAction action)
{
    switch (action)
    {
    case facet::PresetAction::None: return "none";
    case facet::PresetAction::Selected: return "selected";
    case facet::PresetAction::Create: return "create";
    case facet::PresetAction::Save: return "save";
    case facet::PresetAction::Rename: return "rename";
    case facet::PresetAction::Duplicate: return "duplicate";
    case facet::PresetAction::Delete: return "delete";
    case facet::PresetAction::Import: return "import (the demo has no files)";
    case facet::PresetAction::Export: return "export (the demo has no files)";
    }
    return "";
}

// A name for a copy of preset `index`: its name with the first free number.
void DemoPresetCopyName(int index, char* out, std::size_t size)
{
    for (int n = 2; n < 100; ++n)
    {
        std::snprintf(out, size, "%s %d", ui.presets[index].name, n);
        bool taken = false;
        for (int i = 0; i < ui.presetCount; ++i)
            taken = taken || std::strcmp(ui.presets[i].name, out) == 0;
        if (!taken)
            return;
    }
}

// Applies what the preset list reported to the demo's presets, as an application would.
void ApplyPresetResult(const facet::PresetListResult& r)
{
    if (r.action != facet::PresetAction::None)
        ui.presetLast = r.action;
    switch (r.action)
    {
    case facet::PresetAction::Create:
        if (ui.presetCount < kMaxPresets && r.name != nullptr)
        {
            DemoPreset& p = ui.presets[ui.presetCount];
            p = DemoPreset{};
            std::snprintf(p.name, sizeof(p.name), "%s", r.name);
            p.id = ui.nextPresetId++;
            ui.preset = ui.presetCount++;
        }
        break;
    case facet::PresetAction::Save: ui.presets[r.index].modified = false; break;
    case facet::PresetAction::Rename: std::snprintf(ui.presets[r.index].name, sizeof(DemoPreset::name), "%s", r.name); break;
    case facet::PresetAction::Duplicate:
        if (ui.presetCount < kMaxPresets)
        {
            DemoPreset copy = ui.presets[r.index];
            DemoPresetCopyName(r.index, copy.name, sizeof(copy.name));
            copy.builtIn = false;
            copy.id = ui.nextPresetId++;
            for (int i = ui.presetCount; i > r.index + 1; --i)
                ui.presets[i] = ui.presets[i - 1];
            ui.presets[r.index + 1] = copy;
            ++ui.presetCount;
        }
        break;
    case facet::PresetAction::Delete:
        for (int i = r.index; i + 1 < ui.presetCount; ++i)
            ui.presets[i] = ui.presets[i + 1];
        --ui.presetCount;
        if (ui.preset >= r.index)
            ui.preset = std::max(0, ui.preset - 1);
        if (ui.presetCount == 0)
            ui.preset = -1;
        break;
    default: break;
    }
}

void PresetListSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("PresetList");
    Paragraph("Named presets with the active one marked. Enter or a click chooses, F2 renames in place, Delete asks "
              "first; the menu button holds create, save, rename, duplicate, import, export and delete. The page "
              "applies every reported action to its own placeholder presets.");
    static const char* const kStates[] = {"Ready", "Loading", "Error"};
    LabeledRow("Data state");
    facet::SegmentedControl("##preset state", &ui.presetState, kStates);
    LabeledRow("Unsaved change");
    if (facet::Button("Change a setting") && ui.preset >= 0 && ui.preset < ui.presetCount)
        ui.presets[ui.preset].modified = true;
    ctx.Probe("comp.presets.change");
    ImGui::SameLine();
    facet::demo::NoteBeside("Marks the active preset modified; Save in the menu clears it.");

    facet::PresetItem items[kMaxPresets];
    for (int i = 0; i < ui.presetCount; ++i)
        items[i] = {ui.presets[i].name, ui.presets[i].modified, ui.presets[i].builtIn, ui.presets[i].id};
    LabeledRow("Presets");
    if (ctx.capture && ctx.captureState == kPresetMenu)
        ctx.ForceNextItemState(ForcedItemState::Open, ui.presetCount);
    const facet::PresetListResult result =
        facet::PresetList("presets", &ui.preset, std::span<const facet::PresetItem>(items, static_cast<std::size_t>(ui.presetCount)),
                          {.importExport = true, .state = static_cast<facet::DataState>(ui.presetState),
                           .width = DemoWidth(kScaleWidth)});
    ctx.Probe("comp.presets");
    if (ctx.probes != nullptr)
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float row = facet::internal::ListRowHeight(facet::ControlSize::Inherit);
        char name[48];
        for (int i = 0; i < ui.presetCount; ++i)
        {
            std::snprintf(name, sizeof(name), "comp.presets.row.%d", i);
            facet::demo::ProbeRect(ctx, name, 0, ImVec2(min.x, min.y + row * static_cast<float>(i)),
                                   ImVec2(max.x, min.y + row * static_cast<float>(i + 1)));
        }
        const float button = facet::ControlHeight(facet::ControlSize::Inherit);
        facet::demo::ProbeRect(ctx, "comp.presets.menu", 0, ImVec2(max.x - button, max.y - button), max);
    }
    if (ctx.capture && ctx.captureState == kPresetMenu)
    {
        ctx.RevealInCapture(0.3f);
        const ImGuiID level = ImHashStr("##menu level", 0, ImGui::GetID("presets"));
        if (ctx.captureFramesLeft == 0 && !facet::internal::IsPopupLevelOpen(level))
            ctx.captureError = "the preset menu did not open";
    }
    ApplyPresetResult(result);
    LabeledRow("Last action");
    facet::StyledText(PresetActionName(ui.presetLast), {.secondary = true});
    LabeledRow("Read-only");
    int readOnlyActive = 1;
    facet::PresetList("presets read-only", &readOnlyActive,
                      std::span<const facet::PresetItem>(items, static_cast<std::size_t>(std::min(2, ui.presetCount))),
                      {.width = DemoWidth(kScaleWidth), .readOnly = true});
}

// The default binds of the page's key bind list: placeholder actions in two groups, the third and
// fourth on one chord, so the conflict mark shows from the start.
void DefaultBinds(facet::KeybindListItem* binds)
{
    const ImGuiKeyChord chords[kBindCount] = {ImGuiMod_Ctrl | ImGuiKey_1, ImGuiMod_Ctrl | ImGuiKey_2,
                                              ImGuiMod_Ctrl | ImGuiKey_3, ImGuiMod_Ctrl | ImGuiKey_3,
                                              ImGuiMod_Ctrl | ImGuiKey_5};
    static const char* const kLabels[kBindCount] = {"Action 1", "Action 2", "Action 3", "Action 4", "Action 5"};
    for (int i = 0; i < kBindCount; ++i)
    {
        facet::KeybindListItem& item = binds[i];
        item.label = kLabels[i];
        item.group = i < 3 ? "Group 1" : "Group 2";
        item.bind = {.primary = chords[i]};
        item.defaultBind = item.bind;
    }
    // Action 4 was rebound from its own default, so its reset button shows.
    binds[3].defaultBind.primary = ImGuiMod_Ctrl | ImGuiKey_4;
}

void KeybindListSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("KeybindList");
    Paragraph("Actions with a capture field per chord. A click or Enter on a field listens for the next chord, "
              "Escape cancels, Backspace or Delete clears; the reset button returns a changed row to its default; "
              "a chord two actions share is marked on both. The first list edits placeholder actions, the second "
              "the shortcut registry.");
    if (!ui.bindsReady)
    {
        DefaultBinds(ui.binds);
        ui.bindsReady = true;
    }
    static const char* const kStates[] = {"Ready", "Loading", "Error"};
    LabeledRow("Data state");
    facet::SegmentedControl("##bind state", &ui.bindState, kStates);
    LabeledRow("Filter");
    facet::SearchField("##bind filter", &ui.bindFilter, {.width = DemoWidth(kScaleWidth)});
    ctx.Probe("comp.binds.filter");
    LabeledRow("Placeholder actions");
    if (ctx.capture && ctx.captureState == kBindsFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused, 1);
    if (facet::KeybindList("##binds", ui.binds,
                           {.filter = ui.bindFilter.c_str(), .state = static_cast<facet::DataState>(ui.bindState),
                            .width = DemoWidth(kBindsWidth)}))
        ++ui.bindWrites;
    ctx.Probe("comp.binds");
    if (ctx.probes != nullptr)
    {
        // The first capture field, under the column headings and the first group heading, by the
        // list's own layout.
        const ImRect first = facet::internal::KeybindListFirstField(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()),
                                                                    true, true, 0);
        facet::demo::ProbeRect(ctx, "comp.binds.first", 0, first.Min, first.Max);
    }
    if (ctx.capture && ctx.captureState == kBindsFocus)
        ctx.RevealInCapture(0.3f);
    LabeledRow("Changes");
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%d", ui.bindWrites);
        facet::StyledText(text, {.secondary = true});
    }

    LabeledRow("Registry");
    facet::KeybindList("##registry", {.filter = ui.bindFilter.c_str(), .alternate = false,
                                      .width = DemoWidth(kBindsWidth), .height = kSettingsAreaHeight});
    ctx.Probe("comp.registry");
    ui.registryHeadingX = facet::internal::KeybindListLastProbe().primaryHeadingX;
    ui.registryFirstField = facet::internal::KeybindListLastProbe().firstPrimary;
    LabeledRow("Read-only");
    facet::KeybindList("##readonly", std::span<facet::KeybindListItem>(ui.binds, 2),
                       {.groups = false, .width = DemoWidth(kBindsWidth), .readOnly = true});
}

void ConsolePrint(facet::LogLevel level, const char* fmt, ...) IM_FMTARGS(2);
void ConsolePrint(facet::LogLevel level, const char* fmt, ...)
{
    // A full ring drops its oldest line.
    int index = 0;
    if (ui.consoleCount < kConsoleLines)
    {
        index = (ui.consoleStart + ui.consoleCount) % kConsoleLines;
        ++ui.consoleCount;
    }
    else
    {
        index = ui.consoleStart;
        ui.consoleStart = (ui.consoleStart + 1) % kConsoleLines;
    }
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(ui.consoleLines[index].text, kConsoleLineText, fmt, args);
    va_end(args);
    ui.consoleLines[index].level = level;
}

bool ConsoleLineGetter(void*, int index, facet::LogEntry* out)
{
    const ConsoleLine& line = ui.consoleLines[(ui.consoleStart + index) % kConsoleLines];
    out->text = line.text;
    out->level = line.level;
    return true;
}

bool GeneratedLineGetter(void* user, int index, facet::LogEntry* out)
{
    char* text = static_cast<char*>(user);
    std::snprintf(text, kConsoleLineText, "Generated placeholder line %d", index + 1);
    out->text = text;
    out->level = index % 50 == 49 ? facet::LogLevel::Warning : facet::LogLevel::Info;
    return true;
}

const char* const kConsoleCommands[] = {"clear", "echo", "help", "lines"};

// Runs a command of the page's small interpreter, which only prints into the page's log.
void ConsoleRun(const std::string& command)
{
    ConsolePrint(facet::LogLevel::Debug, "> %s", command.c_str());
    const std::size_t space = command.find(' ');
    const std::string name = command.substr(0, space);
    if (name == "clear")
        ui.consoleCount = ui.consoleStart = 0;
    else if (name == "echo")
        ConsolePrint(facet::LogLevel::Info, "%s", space == std::string::npos ? "" : command.c_str() + space + 1);
    else if (name == "help")
        ConsolePrint(facet::LogLevel::Info, "Commands: clear, echo <text>, help, lines");
    else if (name == "lines")
        ConsolePrint(facet::LogLevel::Info, "%d lines in the log", ui.consoleCount);
    else
        ConsolePrint(facet::LogLevel::Error, "Unknown command: %s", name.c_str());
    // The history keeps the newest commands, oldest first.
    if (ui.historyCount == kConsoleHistory)
    {
        for (int i = 1; i < kConsoleHistory; ++i)
            std::memcpy(ui.history[i - 1], ui.history[i], sizeof(ui.history[i]));
        --ui.historyCount;
    }
    std::snprintf(ui.history[ui.historyCount++], sizeof(ui.history[0]), "%s", command.c_str());
}

void ConsoleSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("ConsoleView");
    Paragraph("A log with a command field. Enter runs a command of this page's small interpreter (help lists "
              "them), Up and Down walk the history, Tab completes a command name. The generated log has ten "
              "thousand placeholder lines; only the visible ones are read each frame.");
    static const char* const kStates[] = {"Ready", "Loading", "Error"};
    LabeledRow("Data state");
    facet::SegmentedControl("##console state", &ui.consoleState, kStates);
    LabeledRow("Generated log");
    facet::Toggle("##generated", &ui.consoleGenerated);
    ctx.Probe("comp.console.generated");
    const char* history[kConsoleHistory];
    for (int i = 0; i < ui.historyCount; ++i)
        history[i] = ui.history[i];
    const facet::ConsoleViewOptions options{
        .history = std::span<const char* const>(history, static_cast<std::size_t>(ui.historyCount)),
        .commands = kConsoleCommands,
        .placeholder = "Placeholder command, try help",
        .state = static_cast<facet::DataState>(ui.consoleState),
        .width = DemoWidth(kBindsWidth),
        .height = kConsoleHeight,
    };
    LabeledRow("Console");
    if (ctx.capture && ctx.captureState == kConsoleFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused, 1);
    static char generatedText[kConsoleLineText];
    const bool submitted = ui.consoleGenerated
                               ? facet::ConsoleView("##console", &ui.command, kGeneratedLines, &GeneratedLineGetter,
                                                    generatedText, options)
                               : facet::ConsoleView("##console", &ui.command, ui.consoleCount, &ConsoleLineGetter, nullptr,
                                                    options);
    ctx.Probe("comp.console");
    if (ctx.probes != nullptr)
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float line = facet::ControlHeight(facet::ControlSize::Inherit);
        facet::demo::ProbeRect(ctx, "comp.console.field", 0, ImVec2(min.x, max.y - line), ImVec2(min.x + facet::Px(120.0f), max.y));
    }
    if (ctx.capture && ctx.captureState == kConsoleFocus)
        ctx.RevealInCapture(0.5f);
    if (submitted)
    {
        ConsoleRun(ui.command);
        ui.command.clear();
    }
    LabeledRow("Read-only");
    std::string readOnlyCommand = "help";
    facet::ConsoleView("##console read-only", &readOnlyCommand, std::span<const facet::LogEntry>(kConsoleReadOnly),
                       {.width = DemoWidth(kBindsWidth), .height = kConsoleReadOnlyHeight, .readOnly = true});
}

void OnboardingSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Onboarding");
    Paragraph("A short sequence of steps in a dialog over a scrim, with progress dots and the step number. Next "
              "has the keyboard on each step, so Enter steps through; Back goes back; Skip, Finish, Escape and a "
              "click on the scrim close it, and with Skippable off only Finish does. The step content below is a "
              "placeholder.");
    LabeledRow("Skippable");
    facet::Toggle("##skippable", &ui.onboardingSkippable);
    LabeledRow("Show");
    if (facet::Button("Show onboarding"))
    {
        ui.onboarding = true;
        ui.onboardingStep = 0;
    }
    ctx.Probe("comp.onboarding.show");
    ImGui::SameLine();
    facet::demo::NoteBeside(ui.onboarding ? "Open on step %d" : "Closed on step %d", ui.onboardingStep + 1);
    // The capture state sets itself up again in every pass.
    if (ctx.capture && ctx.captureState == kOnboardingOpen)
    {
        ui.onboarding = true;
        ui.onboardingStep = 1;
    }
    static const char* const kStepTitles[kOnboardingSteps] = {"Placeholder step 1", "Placeholder step 2",
                                                              "Placeholder step 3"};
    if (facet::BeginOnboarding("Welcome", &ui.onboarding, &ui.onboardingStep, kOnboardingSteps,
                               {.skippable = ui.onboardingSkippable}))
    {
        const int step = std::clamp(ui.onboardingStep, 0, kOnboardingSteps - 1);
        facet::StyledText(kStepTitles[step], {.style = facet::TextStyle::Headline});
        facet::StyledText("The application's content for this step goes here. This placeholder only shows where.",
                          {.secondary = true, .wrap = true});
        facet::demo::FloatingProbe(ctx, "comp.onboarding.content",
                                   ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
        facet::EndOnboarding();
    }
    if (ctx.capture && ctx.captureState == kOnboardingOpen && ctx.captureFramesLeft == 0 && !ui.onboarding)
        ctx.captureError = "the onboarding panel did not open";
}

} // namespace

FACET_DEMO_PAGE("composites-settings", "Search and app controls", "Widgets", 142, "default", "app controls focused",
                "accent popover", "search focused", "settings hits", "preset menu", "binds focused", "console focused",
                "onboarding")
{

    facet::demo::PageHeader("Search and app controls",
                            "Search filter bars, settings search, key bind and preset lists, theme mode, accent and "
                            "UI scale controls, onboarding and console views.");
    facet::demo::ReadyFamilyNote("composites");

    ThemeModeSection(ctx);
    AccentSection(ctx);
    UiScaleSection(ctx);

    SearchFilterBarSection(ctx);
    SettingsSearchSection(ctx);

    PresetListSection(ctx);

    KeybindListSection(ctx);

    OnboardingSection(ctx);

    ConsoleSection(ctx);
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

    // Onboarding: opened by mouse; Enter steps and finishes, Back and Escape by keyboard; the scrim
    // by mouse.
    test.Click("comp.onboarding.show");
    test.WaitSeconds(0.4f);
    test.Expect("the button opens the onboarding on its first step", [] { return ui.onboarding && ui.onboardingStep == 0; });
    test.ExpectVisible("comp.onboarding.content");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the focused Next goes to step 2", [] { return ui.onboardingStep == 1; });
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Shift+Tab and Space on Back go back to step 1", [] { return ui.onboardingStep == 0; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.4f);
    test.Expect("Escape closes the onboarding", [] { return !ui.onboarding; });
    test.Click("comp.onboarding.show");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Display(0.02f, 0.98f));
    test.MouseDown();
    test.MouseUp();
    test.WaitSeconds(0.4f);
    test.Expect("a click on the scrim closes it", [] { return !ui.onboarding; });
    test.Click("comp.onboarding.show");
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.4f);
    test.Expect("Enter on Finish closes it on the last step", [] { return !ui.onboarding && ui.onboardingStep == 2; });
    // Not skippable: Escape and the scrim leave it open, only Finish closes it.
    test.Do([] { ui.onboardingSkippable = false; });
    test.Click("comp.onboarding.show");
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.4f);
    test.Expect("without Skippable Escape keeps it open", [] { return ui.onboarding; });
    test.MouseMove(T::Display(0.02f, 0.98f));
    test.MouseDown();
    test.MouseUp();
    test.WaitSeconds(0.4f);
    test.Expect("without Skippable a click on the scrim keeps it open", [] { return ui.onboarding; });
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.4f);
    test.Expect("without Skippable Finish still closes it", [] { return !ui.onboarding && ui.onboardingStep == 2; });
    test.Do([] { ui.onboardingSkippable = true; });

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

    // SettingsSearch: one hit far down the rows, no hits, every row, and the Escape order.
    test.Click("comp.settings.field");
    test.Type("sens");
    test.WaitSeconds(0.2f);
    test.Expect("sens has one hit, in Controls", [] {
        return facet::SettingsSearchHitCount(kSettingsSearchName, nullptr) == 1 &&
               facet::SettingsSearchHitCount(kSettingsSearchName, "Controls") == 1;
    });
    test.Reveal("comp.settings.row.7");
    test.ExpectVisible("comp.settings.row.7");
    test.Type("zz");
    test.Expect("sensz has no hits", [] { return facet::SettingsSearchHitCount(kSettingsSearchName, nullptr) == 0; });
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("the first Escape clears the query, so every row is a hit", [] {
        return ui.settingsQuery.empty() && facet::SettingsSearchHitCount(kSettingsSearchName, nullptr) == 10;
    });
    test.Expect("the field still has the keyboard", [] { return ImGui::GetActiveID() != 0; });
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("the second Escape leaves the field", [] { return ImGui::GetActiveID() == 0; });

    // PresetList: choose, rename, delete declined and confirmed, create from the menu.
    test.Click("comp.presets.row.2");
    test.Expect("a click chooses Preset 3", [] { return ui.preset == 2; });
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("Up moves only the focus", [] { return ui.preset == 2; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter chooses Preset 2", [] { return ui.preset == 1; });
    test.KeyPress(ImGuiKey_F2);
    test.Wait(2);
    test.Type("Renamed");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("F2, typing and Enter rename it", [] { return std::string(ui.presets[1].name); }, "Renamed");
    test.KeyPress(ImGuiKey_F2);
    test.Wait(2);
    test.Type("Dropped");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape keeps the name", [] { return std::string(ui.presets[1].name); }, "Renamed");
    test.KeyPress(ImGuiKey_Delete);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.3f);
    test.Expect("Escape in the confirmation keeps the preset", [] { return ui.presetCount == 3; });
    test.KeyPress(ImGuiKey_Delete);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.Expect("Delete confirmed removes it", [] {
        return ui.presetCount == 2 && std::strcmp(ui.presets[1].name, "Preset 3") == 0;
    });
    // Tab leaves the rows for the menu button; Enter opens the menu with its first row focused.
    test.Click("comp.presets.row.0");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(3);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("New preset in the menu creates one with the default name", [] {
        return ui.presetCount == 3 && std::strcmp(ui.presets[2].name, facet::GetStrings().newPreset) == 0 &&
               ui.preset == 2;
    });

    // KeybindList: capture by mouse, clear, cancel and reset by keyboard.
    test.Do([] { ui.bindsReady = false; });
    // The start of the field: its clear button sits at the end.
    test.MouseMove(T::Item("comp.binds.first", 0.15f));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    test.Wait(2);
    test.Expect("a click listens and Ctrl+K binds Action 1", [] { return ui.binds[0].bind.primary == (ImGuiMod_Ctrl | ImGuiKey_K); });
    test.KeyPress(ImGuiKey_Backspace);
    test.Expect("Backspace on the focused field clears it", [] { return ui.binds[0].bind.primary == ImGuiKey_None; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus on the field
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_J | ImGuiMod_Ctrl | ImGuiMod_Shift);
    test.Expect("Enter listens and the next chord binds", [] {
        return ui.binds[0].bind.primary == (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_J);
    });
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.Expect("Escape while listening keeps the chord", [] {
        return ui.binds[0].bind.primary == (ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_J);
    });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Tab reaches the reset button and Space resets the row",
                [] { return ui.binds[0].bind.primary == (ImGuiMod_Ctrl | ImGuiKey_1); });
    test.Expect("four writes in the list", [] { return ui.bindWrites == 4; });
    // The registry list has a height: its rows scroll inside a padded area, and the column
    // headings above take the same inset.
    test.Reveal("comp.registry");
    test.Wait(2);
    test.Expect("the registry list's first field starts under its Primary heading", [] {
        return ui.registryFirstField.GetWidth() > 0.0f && std::abs(ui.registryFirstField.Min.x - ui.registryHeadingX) < 0.5f;
    });

    // The page's zoom shortcuts drive the example UI scale control from anywhere.
    test.Do([] { ui.uiScale = 1.0f; });
    test.KeyPress(ImGuiKey_Equal | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+= zooms the example scale in", [] { return ui.uiScale; }, 1.25, 0.001);
    test.KeyPress(ImGuiKey_0 | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+0 resets it", [] { return ui.uiScale; }, 1.0, 0.001);

    // ConsoleView: run commands, complete with Tab, walk the history.
    test.Click("comp.console.field");
    test.Type("echo hi");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter runs echo into the log and the page clears the field", [] {
        return ui.consoleCount == 2 && std::strcmp(ui.consoleLines[1].text, "hi") == 0 && ui.command.empty();
    });
    test.Type("he");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectText("Tab completes the one candidate", [] { return ui.command; }, "help ");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the field kept the keyboard: help ran", [] { return ui.consoleCount == 4 && ui.historyCount == 2; });
    test.Type("zz");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectText("Tab without a candidate leaves the text", [] { return ui.command; }, "zz");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectText("Up shows the newest command", [] { return ui.command; }, "help ");
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectText("Up stops at the oldest command", [] { return ui.command; }, "echo hi");
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectText("Down past the newest gives the typed text back", [] { return ui.command; }, "zz");
    test.Click("comp.console.generated");
    test.Expect("the toggle shows the generated log", [] { return ui.consoleGenerated; });
}
