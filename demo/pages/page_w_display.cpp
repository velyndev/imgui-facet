// Demo page: Display. Calls every declared display widget once with neutral example
// arguments, grouped by purpose. ProgressBar is implemented and shown in several forms; a
// planned widget draws the placeholder of its declared size.

#include <iterator>

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

// Example data: neutral placeholders, not real content.
const facet::LogEntry kLogEntries[] = {
    {"Log line 1", facet::LogLevel::Info, 0.0},
    {"Log line 2", facet::LogLevel::Warning, 1.0},
    {"Log line 3", facet::LogLevel::Error, 2.0},
};
const facet::DataTableColumn kColumns[] = {
    {"Column 1"},
    {"Column 2"},
    {"Column 3", 0.0f, facet::Align::End},
};
constexpr int kTableRows = 3;
const int kHighlightedLines[] = {2};
constexpr const char* kCodeSample = "// Placeholder code\nint value = 0;\nvalue += 1;\n";

struct DisplayUi
{
    bool bannerOpen = true;
    bool alertOpen = true;
    float progress = 0.4f;
    bool progressHovered = false;
    ImGuiID progressId = 0;
    ImGuiSelectionBasicStorage selection;
};

DisplayUi ui;

} // namespace

FACET_DEMO_PAGE("display", "Display", "Widgets", 110)
{
    const facet::Theme& theme = facet::T();
    const float viewHeight = theme.spacing.xxxl * kViewSpans;

    facet::demo::PageHeader("Display",
                            "Text styles, alerts and empty states, progress and loading, badges and status marks, "
                            "key caps, code and log views, key-value lists, stat tiles and data tables.");
    facet::demo::PlannedFamilyNote("display");

    SectionHeader("Text");
    LabeledRow("Styled text");
    facet::StyledText("Sample text", {.style = facet::TextStyle::Headline});
    LabeledRow("Truncated text");
    facet::TruncatedText("A placeholder line that is cut with an ellipsis where it does not fit",
                         {.mode = facet::TruncateMode::Middle});
    LabeledRow("Section header");
    facet::SectionHeader("Section title", {.description = "Description", .actionLabel = "Action", .divider = true});

    SectionHeader("Alerts and states");
    LabeledRow("Banner");
    facet::Banner("Banner title", &ui.bannerOpen, "Banner message.",
                  {.tone = facet::Tone::Warning, .primaryAction = "Action 1", .secondaryAction = "Action 2"});
    LabeledRow("Inline alert");
    facet::InlineAlert("Alert title", &ui.alertOpen, "Alert message.", {.tone = facet::Tone::Accent});
    LabeledRow("Empty state");
    facet::EmptyState("Nothing to show", {.message = "Placeholder message.", .actionLabel = "Action", .compact = true});
    LabeledRow("Data state view");
    if (facet::BeginDataStateView("data view", facet::DataState::Loading, 0, {.skeletonRows = 2}))
        facet::EndDataStateView();

    SectionHeader("Progress and loading");
    // ProgressBar is implemented: the stock slider sets its value, so the eased fill can be seen.
    LabeledRow("Progress value");
    ImGui::SliderFloat("##progress value", &ui.progress, 0.0f, 1.0f, "%.2f");
    ctx.Probe("display.progressValue");
    LabeledRow("Progress bar");
    facet::ProgressBar("Example task##bar", ui.progress, {.showPercent = true, .buffer = ui.progress + 0.2f});
    ctx.Probe("display.progress");
    ui.progressHovered = ImGui::IsItemHovered();
    ui.progressId = ImGui::GetItemID();
    LabeledRow("Without a label");
    facet::ProgressBar("##bar plain", ui.progress, {.tone = facet::Tone::Success});
    LabeledRow("Sizes");
    ImGui::BeginGroup();  // keeps the bars in the control column of the row
    facet::ProgressBar("Small##bar", ui.progress, {.tone = facet::Tone::Neutral, .size = facet::ControlSize::Small});
    facet::ProgressBar("Large##bar", ui.progress, {.tone = facet::Tone::Destructive, .size = facet::ControlSize::Large});
    ImGui::EndGroup();
    LabeledRow("Indeterminate");
    facet::ProgressBar("Waiting##bar", facet::kIndeterminate);
    LabeledRow("Progress ring");
    facet::ProgressRing("Progress##ring", facet::kIndeterminate);
    LabeledRow("Spinner");
    facet::Spinner("Loading");
    LabeledRow("Skeleton");
    facet::Skeleton({.lines = 2});

    SectionHeader("Badges and status");
    LabeledRow("Badge");
    facet::Badge("Label", {.tone = facet::Tone::Accent});
    LabeledRow("Count badge");
    facet::CountBadge(3);
    LabeledRow("Status dot");
    facet::StatusDot("Status", {.tone = facet::Tone::Success});
    LabeledRow("Status pill");
    facet::StatusPill("Status###pill", {.tone = facet::Tone::Warning, .liveDot = true});

    SectionHeader("Keys and text marks");
    LabeledRow("Key cap");
    facet::KeyCap(ImGuiMod_Ctrl | ImGuiKey_S);
    LabeledRow("Shortcut hint");
    facet::ShortcutHint("Action", ImGuiMod_Ctrl | ImGuiKey_K);
    LabeledRow("Character counter");
    facet::CharacterCounter(12, 40);
    LabeledRow("Inline code");
    facet::InlineCode("value", {.copyOnActivate = true});

    SectionHeader("Code and logs");
    LabeledRow("Code view");
    facet::CodeView("code", kCodeSample,
                    {.language = facet::CodeLanguage::Cpp, .highlightLines = kHighlightedLines, .height = viewHeight});
    LabeledRow("Log view");
    facet::LogView("log", kLogEntries, {.height = viewHeight});

    SectionHeader("Data");
    LabeledRow("Key value list");
    ImGui::BeginGroup();  // keeps the rows in the control column of the row
    facet::BeginKeyValueList("details", {.copyable = true});
    facet::KeyValue("Key 1", "Value 1");
    facet::KeyValue("Key 2", "Value 2");
    facet::EndKeyValueList();
    ImGui::EndGroup();
    LabeledRow("Stat tile");
    facet::StatTile("Example value", 0.0);
    LabeledRow("Data table");
    if (facet::BeginDataTable("table", kColumns, kTableRows, &ui.selection,
                              {.sortable = true, .frozenColumns = 1, .height = viewHeight}))
    {
        facet::RowRange rows;
        while (facet::NextVisibleRows(&rows))
        {
            for (int row = rows.first; row < rows.last; ++row)
            {
                facet::DataTableRow(row);
                for (int column = 0; column < static_cast<int>(std::size(kColumns)); ++column)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("Row %d, column %d", row + 1, column + 1);
                }
            }
        }
        facet::EndDataTable();
    }
}

FACET_DEMO_SELFTEST("display")
{
    using T = facet::demo::Target;
    test.Do([] {
        ui.progress = 0.4f;
        ui.progressHovered = false;
    });
    // The progress bar follows its value and is not operable: a click on it changes nothing and
    // Tab passes from the value slider over it.
    test.Reveal("display.progress");
    test.ExpectVisible("display.progress");
    test.Hover(T::Item("display.progress"), 0.2f);
    test.Expect("the progress bar answers ImGui::IsItemHovered", [] { return ui.progressHovered; });
    test.Click("display.progress");
    test.ExpectNear("a click on the bar leaves the value", [] { return ui.progress; }, 0.4, 0.0001);
    test.Drag({T::Item("display.progressValue", 0.5f), T::Item("display.progressValue", 1.2f)}, 0.25f);
    test.ExpectNear("the stock slider sets the value the bar shows", [] { return ui.progress; }, 1.0, 0.0001);
    test.Click("display.progressValue");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.Expect("Tab passes over the progress bar",
                [] { return ui.progressId != 0 && ImGui::GetFocusID() != ui.progressId; });
}
