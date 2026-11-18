// Demo page: Display. Calls every declared display widget once with neutral example
// arguments, grouped by purpose. Until Phase 2 implements a widget, its call draws the
// placeholder of the declared size; Phase 2 replaces the placeholders one by one.

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
    ImGuiSelectionBasicStorage selection;
};

DisplayUi ui;

} // namespace

FACET_DEMO_PAGE("display", "Display", "Widgets", 110)
{
    const facet::Theme& theme = facet::T();
    const float viewHeight = theme.spacing.xxxl * kViewSpans;

    facet::demo::PageHeader("Display",
                            "Text styles, alerts and empty states, progress and loading, badges and status marks, key "
                            "caps, code and log views, key-value lists, stat tiles and data tables. Every widget on "
                            "this page is a placeholder of its declared size until it is implemented in Phase 2.");

    SectionHeader("Text");
    LabeledRow("StyledText");
    facet::StyledText("Sample text", {.style = facet::TextStyle::Headline});
    LabeledRow("TruncatedText");
    facet::TruncatedText("A placeholder line that is cut with an ellipsis where it does not fit",
                         {.mode = facet::TruncateMode::Middle});
    LabeledRow("SectionHeader");
    facet::SectionHeader("Section title", {.description = "Description", .actionLabel = "Action", .divider = true});

    SectionHeader("Alerts and states");
    LabeledRow("Banner");
    facet::Banner("Banner title", &ui.bannerOpen, "Banner message.",
                  {.tone = facet::Tone::Warning, .primaryAction = "Action 1", .secondaryAction = "Action 2"});
    LabeledRow("InlineAlert");
    facet::InlineAlert("Alert title", &ui.alertOpen, "Alert message.", {.tone = facet::Tone::Accent});
    LabeledRow("EmptyState");
    facet::EmptyState("Nothing to show", {.message = "Placeholder message.", .actionLabel = "Action", .compact = true});
    LabeledRow("DataStateView");
    if (facet::BeginDataStateView("data view", facet::DataState::Loading, 0, {.skeletonRows = 2}))
        facet::EndDataStateView();

    SectionHeader("Progress and loading");
    LabeledRow("ProgressBar");
    facet::ProgressBar("Progress##bar", 0.4f, {.showPercent = true});
    LabeledRow("ProgressRing");
    facet::ProgressRing("Progress##ring", facet::kIndeterminate);
    LabeledRow("Spinner");
    facet::Spinner("Loading");
    LabeledRow("Skeleton");
    facet::Skeleton({.lines = 2});

    SectionHeader("Badges and status");
    LabeledRow("Badge");
    facet::Badge("Label", {.tone = facet::Tone::Accent});
    LabeledRow("CountBadge");
    facet::CountBadge(3);
    LabeledRow("StatusDot");
    facet::StatusDot("Status", {.tone = facet::Tone::Success});
    LabeledRow("StatusPill");
    facet::StatusPill("Status###pill", {.tone = facet::Tone::Warning, .liveDot = true});

    SectionHeader("Keys and text marks");
    LabeledRow("KeyCap");
    facet::KeyCap(ImGuiMod_Ctrl | ImGuiKey_S);
    LabeledRow("ShortcutHint");
    facet::ShortcutHint("Action", ImGuiMod_Ctrl | ImGuiKey_K);
    LabeledRow("CharacterCounter");
    facet::CharacterCounter(12, 40);
    LabeledRow("InlineCode");
    facet::InlineCode("value", {.copyOnActivate = true});

    SectionHeader("Code and logs");
    LabeledRow("CodeView");
    facet::CodeView("code", kCodeSample,
                    {.language = facet::CodeLanguage::Cpp, .highlightLines = kHighlightedLines, .height = viewHeight});
    LabeledRow("LogView");
    facet::LogView("log", kLogEntries, {.height = viewHeight});

    SectionHeader("Data");
    LabeledRow("KeyValueList");
    ImGui::BeginGroup();  // keeps the rows in the control column of the row
    facet::BeginKeyValueList("details", {.copyable = true});
    facet::KeyValue("Key 1", "Value 1");
    facet::KeyValue("Key 2", "Value 2");
    facet::EndKeyValueList();
    ImGui::EndGroup();
    LabeledRow("StatTile");
    facet::StatTile("Example value", 0.0);
    LabeledRow("DataTable");
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
