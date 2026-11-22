// Demo page: Display. Shows every display widget with its variants, grouped by purpose: text,
// alerts and states, progress and loading, badges and status marks, keys and text marks, code and
// logs, and data. Every value on the page is a labelled placeholder or generated (the log rows, the
// stat tile's trend). DataTable has a page of its own (page_w_data_table.cpp).

#include <cmath>
#include <cstring>
#include <iterator>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "widgets/display/display_look.h"

namespace
{

using facet::demo::DemoWidth;
using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::SectionHeader;

// Heights of the scrolling views on this page, in multiples of the xxxl spacing token.
constexpr float kViewSpans = 6.0f;
// Rows of the generated log, enough to show that only the visible rows cost anything.
constexpr int kLogRows = 100000;
// Samples of the generated trend line of the stat tile.
constexpr int kTrendSamples = 24;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kAlerts,
    kProgress,
    kMarks,
    kCode,
    kData,
    kLoading,
    kError,
    kDisabled,
};

// Example data: neutral placeholders, not real content.
const int kHighlightedLines[] = {3};
constexpr const char* kCodeSample =
    "// Placeholder code: a small function and a loop.\n"
    "#include <cstdio>\n"
    "\n"
    "int Sum(const int* values, int count)\n"
    "{\n"
    "    int total = 0;  /* the running sum */\n"
    "    for (int i = 0; i < count; ++i)\n"
    "        total += values[i];\n"
    "    return total;\n"
    "}\n";
constexpr const char* kJsonSample = "{\n  \"name\": \"placeholder\",\n  \"count\": 3,\n  \"enabled\": true\n}\n";
constexpr const char* kIniSample = "; Placeholder settings\n[window]\nwidth = 1280\ntitle = \"Placeholder\"\n";
constexpr const char* kLongText = "A placeholder line long enough to be cut with an ellipsis where it does not fit";
constexpr const char* kPathText = "C:/Placeholder/Folder/Another folder/placeholder file name.txt";
constexpr const char* kWrappedText =
    "Placeholder paragraph in the Body style. It wraps at the width of the item and each line is aligned inside it, "
    "here centered.";

struct DisplayUi
{
    // Text
    int sectionActions = 0;
    bool truncatedTip = false;
    ImGuiID truncatedId = 0;
    // Alerts and states
    bool bannerOpen = true;
    bool alertOpen = true;
    int bannerPrimary = 0;
    int bannerSecondary = 0;
    int alertActions = 0;
    int emptyActions = 0;
    facet::DataState dataState = facet::DataState::Ready;
    int dataCount = 3;
    int retries = 0;
    // Progress
    float progress = 0.4f;
    bool progressHovered = false;
    ImGuiID progressId = 0;
    // Badges and status
    bool pillWarning = false;
    // Keys and marks
    char counterText[64] = "Placeholder";
    int inlineCopies = 0;
    // Code and logs
    int codeCopies = 0;
    int logRows = kLogRows;
    ImGuiID logId = 0;
    char logLine[96] = {};
    // Data
    int valueCopies = 0;
    double statValue = 40.0;
    double statDelta = 2.0;
    float trend[kTrendSamples] = {};
};

DisplayUi ui;

bool Is(const facet::demo::PageCtx& ctx, CaptureState state)
{
    return ctx.capture && ctx.captureState == state;
}

// Generated log rows: a text with its index, a level and a time that grow with the index, so the
// view shows its columns without invented content.
bool GeneratedLogEntry(void*, int index, facet::LogEntry* out)
{
    ImFormatString(ui.logLine, sizeof(ui.logLine), "Generated entry %d of the placeholder log", index + 1);
    out->text = ui.logLine;
    static const facet::LogLevel kLevels[] = {facet::LogLevel::Info, facet::LogLevel::Debug, facet::LogLevel::Info,
                                              facet::LogLevel::Trace, facet::LogLevel::Warning, facet::LogLevel::Info,
                                              facet::LogLevel::Info, facet::LogLevel::Error};
    out->level = kLevels[index % static_cast<int>(std::size(kLevels))];
    out->time = static_cast<double>(index) * 0.25;
    out->source = index % 3 == 0 ? "generated" : nullptr;
    return true;
}

void TextSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Text");
    LabeledRow("Styled text");
    ImGui::BeginGroup();
    facet::StyledText("Title 2", {.style = facet::TextStyle::Title2});
    facet::StyledText("Headline", {.style = facet::TextStyle::Headline});
    facet::StyledText("Body text in the label color");
    facet::StyledText("Footnote in the secondary label color", {.style = facet::TextStyle::Footnote, .secondary = true});
    ImGui::EndGroup();
    LabeledRow("Tones");
    ImGui::BeginGroup();
    facet::StyledText("Accent");
    ImGui::SameLine();
    facet::StyledText("Accent text", {.tone = facet::Tone::Accent});
    ImGui::SameLine();
    facet::StyledText("Success text", {.tone = facet::Tone::Success});
    ImGui::SameLine();
    facet::StyledText("Warning text", {.tone = facet::Tone::Warning});
    ImGui::SameLine();
    facet::StyledText("Destructive text", {.tone = facet::Tone::Destructive});
    ImGui::EndGroup();
    LabeledRow("Wrapped, centered");
    facet::StyledText(kWrappedText, {.wrap = true, .align = facet::Align::Center});
    ctx.Probe("display.styled");

    LabeledRow("Truncated, end");
    facet::TruncatedText(kLongText, {.width = DemoWidth(260.0f)});
    ctx.Probe("display.truncated");
    ui.truncatedId = ImGui::GetItemID();
    ui.truncatedTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ui.truncatedId));
    LabeledRow("Truncated, middle");
    facet::TruncatedText(kPathText, {.mode = facet::TruncateMode::Middle, .width = DemoWidth(220.0f)});
    LabeledRow("Truncated, start");
    facet::TruncatedText(kPathText, {.mode = facet::TruncateMode::Start, .secondary = true, .width = DemoWidth(220.0f)});
    LabeledRow("Two lines, focusable");
    facet::TruncatedText(kWrappedText, {.maxLines = 2, .focusable = true, .width = DemoWidth(260.0f)});
    ctx.Probe("display.truncatedFocus");

    LabeledRow("Section header");
    ImGui::BeginGroup();
    if (facet::SectionHeader("Section title##action", {.actionLabel = "Edit"}))
        ++ui.sectionActions;
    ctx.Probe("display.section");
    facet::SectionHeader("Section with a description",
                         {.description = "A placeholder description line under the heading.", .divider = true});
    ImGui::EndGroup();
    Note("Action activated %d times.", ui.sectionActions);
}

void AlertsSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Alerts and states");
    LabeledRow("Banner");
    ImGui::BeginGroup();
    if (Is(ctx, kAlerts))
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    switch (facet::Banner("Placeholder banner##banner", &ui.bannerOpen,
                          "A message for the whole view. It stays until it is dismissed.",
                          {.tone = facet::Tone::Warning, .primaryAction = "Action 1", .secondaryAction = "Action 2"}))
    {
    case facet::BannerAction::Primary:
        ++ui.bannerPrimary;
        break;
    case facet::BannerAction::Secondary:
        ++ui.bannerSecondary;
        break;
    default:
        break;
    }
    ctx.Probe("display.banner");
    if (!ui.bannerOpen && facet::Button("Show the banner again"))
        ui.bannerOpen = true;
    ctx.Probe("display.bannerAgain");
    facet::Banner("##info", nullptr, "A banner without a close button, in the accent tone for information.",
                  {.tone = facet::Tone::Accent});
    ImGui::EndGroup();
    Note("Action 1: %d, action 2: %d.", ui.bannerPrimary, ui.bannerSecondary);

    LabeledRow("Inline alert");
    ImGui::BeginGroup();
    if (Is(ctx, kAlerts))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::InlineAlert("Placeholder alert##alert", &ui.alertOpen, "A message about the fields around it.",
                           {.tone = facet::Tone::Destructive, .actionLabel = "Action"}) == facet::InlineAlertAction::Action)
        ++ui.alertActions;
    ctx.Probe("display.alert");
    if (!ui.alertOpen && facet::Button("Show the alert again"))
        ui.alertOpen = true;
    ImGui::EndGroup();
    if (Is(ctx, kAlerts))
        ctx.RevealInCapture(0.4f);

    LabeledRow("Empty state");
    ImGui::BeginGroup();
    if (facet::EmptyState("Nothing here yet", {.message = "A placeholder message under the title.",
                                               .icon = facet::Icon("folder"), .actionLabel = "Action"}))
        ++ui.emptyActions;
    ctx.Probe("display.empty");
    facet::EmptyState("No results", {.message = "Compact, for lists and search results.", .icon = facet::Icon("search"),
                                     .compact = true});
    ImGui::EndGroup();
    Note("Empty state action: %d.", ui.emptyActions);

    LabeledRow("Data state view");
    ImGui::BeginGroup();
    static const char* const kStates[] = {"Ready", "Loading", "Error", "Empty"};
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            ImGui::SameLine();
        ImGui::PushID(i);
        if (facet::Button(kStates[i], {.size = facet::ControlSize::Small}))
        {
            ui.dataState = i == 1 ? facet::DataState::Loading : (i == 2 ? facet::DataState::Error : facet::DataState::Ready);
            ui.dataCount = i == 3 ? 0 : 3;
        }
        ImGui::PopID();
    }
    facet::DataState state = ui.dataState;
    if (Is(ctx, kLoading))
        state = facet::DataState::Loading;
    else if (Is(ctx, kError))
        state = facet::DataState::Error;
    if (facet::BeginDataStateView("data view", state, ui.dataCount, {.retryLabel = "Retry"}))
    {
        for (int i = 0; i < ui.dataCount; ++i)
        {
            ImGui::PushID(i);
            facet::StyledText("Placeholder row");
            ImGui::PopID();
        }
        facet::EndDataStateView();
    }
    ctx.Probe("display.dataView");
    if (facet::DataStateViewRetryPressed())
    {
        ++ui.retries;
        ui.dataState = facet::DataState::Loading;
    }
    ImGui::EndGroup();
    if (Is(ctx, kLoading) || Is(ctx, kError))
        ctx.RevealInCapture(0.5f);
}

void ProgressSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Progress and loading");
    // The slider sets the value, so the eased fill can be seen.
    LabeledRow("Progress value");
    facet::Slider("##progress value", &ui.progress, 0.0f, 1.0f, {.format = "%.2f"});
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
    if (Is(ctx, kProgress))
        ctx.RevealInCapture(0.3f);
    LabeledRow("Progress ring");
    facet::ProgressRing("Copying##ring", ui.progress, {.showPercent = true, .diameter = 40.0f});
    ImGui::SameLine();
    facet::ProgressRing("Small##ring", ui.progress, {.tone = facet::Tone::Success, .size = facet::ControlSize::Small});
    ImGui::SameLine();
    facet::ProgressRing("Waiting##ring", facet::kIndeterminate);
    LabeledRow("Spinner");
    facet::Spinner("Loading");
    ImGui::SameLine();
    facet::Spinner("Accent##spin", {.tone = facet::Tone::Accent});
    ImGui::SameLine();
    facet::Spinner("Large##spin", {.size = facet::ControlSize::Large});
    LabeledRow("Skeleton");
    ImGui::BeginGroup();
    facet::Skeleton({.shape = facet::SkeletonShape::Circle, .height = 40.0f});
    ImGui::SameLine();
    ImGui::BeginGroup();
    facet::Skeleton({.lines = 2, .width = DemoWidth(260.0f)});
    ImGui::EndGroup();
    facet::Skeleton({.shape = facet::SkeletonShape::Rect, .corners = facet::Radius::Card, .width = DemoWidth(320.0f), .height = 56.0f});
    ImGui::EndGroup();
}

void MarksSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Badges and status");
    LabeledRow("Badge");
    ImGui::BeginGroup();
    facet::Badge("Neutral");
    ImGui::SameLine();
    facet::Badge("Accent", {.tone = facet::Tone::Accent});
    ImGui::SameLine();
    facet::Badge("Success", {.tone = facet::Tone::Success, .icon = facet::Icon("check")});
    ImGui::SameLine();
    facet::Badge("Warning", {.tone = facet::Tone::Warning, .icon = facet::Icon("warning")});
    ImGui::SameLine();
    facet::Badge("Destructive", {.tone = facet::Tone::Destructive, .solid = true});
    ImGui::SameLine();
    facet::Badge("Solid accent", {.tone = facet::Tone::Accent, .solid = true});
    ImGui::EndGroup();
    LabeledRow("In a sentence");
    // Body text and the marks share one baseline.
    ImGui::BeginGroup();
    facet::StyledText("A line of text with");
    ImGui::SameLine();
    facet::Badge("a badge", {.tone = facet::Tone::Accent});
    ImGui::SameLine();
    facet::StyledText("a count");
    ImGui::SameLine();
    facet::CountBadge(3);
    ImGui::SameLine();
    facet::StyledText("and");
    ImGui::SameLine();
    facet::InlineCode("inline_code");
    ImGui::EndGroup();
    LabeledRow("Count badge");
    facet::CountBadge(3);
    ImGui::SameLine();
    facet::CountBadge(120);
    ImGui::SameLine();
    facet::CountBadge(0, {.tone = facet::Tone::Neutral, .showZero = true});
    ImGui::SameLine();
    facet::CountBadge(7, {.tone = facet::Tone::Destructive});
    LabeledRow("Status dot");
    facet::StatusDot("Neutral##dot");
    ImGui::SameLine();
    facet::StatusDot("Success##dot", {.tone = facet::Tone::Success});
    ImGui::SameLine();
    facet::StatusDot("Live##dot", {.tone = facet::Tone::Destructive, .pulse = true});
    ImGui::SameLine();
    facet::StatusDot("##A dot alone names itself on hover", {.tone = facet::Tone::Warning});
    LabeledRow("Status pill");
    ImGui::BeginGroup();
    const facet::Tone pillTone = ui.pillWarning ? facet::Tone::Warning : facet::Tone::Success;
    facet::StatusPill(ui.pillWarning ? "Waiting###pill" : "Running###pill",
                      {.tone = pillTone, .icon = facet::Icon(ui.pillWarning ? "pause" : "play")});
    ImGui::SameLine();
    facet::StatusPill("Live##pill", {.tone = facet::Tone::Destructive, .liveDot = true});
    ImGui::SameLine();
    facet::StatusPill("Small##pill", {.tone = facet::Tone::Accent, .size = facet::ControlSize::Small});
    ImGui::SameLine();
    facet::StatusPill("Large##pill", {.size = facet::ControlSize::Large});
    // The pills share the text baseline of the line; the button goes under them, so its frame
    // does not hang from the top of a line of marks.
    if (facet::Button("Change the state", {.size = facet::ControlSize::Small}))
        ui.pillWarning = !ui.pillWarning;
    ctx.Probe("display.pillToggle");
    ImGui::EndGroup();

    SectionHeader("Keys and text marks");
    LabeledRow("Key cap");
    facet::KeyCap(ImGuiMod_Ctrl | ImGuiKey_S);
    ImGui::SameLine();
    facet::KeyCap(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P);
    ImGui::SameLine();
    facet::KeyCap(ImGuiKey_F5, {.live = true});
    ImGui::SameLine();
    facet::KeyCap(ImGuiKey_None);
    ImGui::SameLine();
    facet::KeyCap(ImGuiKey_Enter, {.size = facet::ControlSize::Small});
    Note("The F5 cap shows the held look while F5 is down.");
    LabeledRow("Shortcut hint");
    ImGui::BeginGroup();
    facet::ShortcutHint("Placeholder command", ImGuiMod_Ctrl | ImGuiKey_K, {.width = DemoWidth(320.0f)});
    facet::ShortcutHint("Narrow: the caps give way first", ImGuiMod_Ctrl | ImGuiKey_K, {.width = DemoWidth(140.0f)});
    ImGui::EndGroup();
    if (Is(ctx, kMarks))
        ctx.RevealInCapture(0.4f);
    LabeledRow("Character counter");
    ImGui::BeginGroup();
    // No maxLength, so the text can run past the limit and the counters show it.
    facet::TextField("##counter input", ui.counterText, sizeof(ui.counterText),
                     {.width = DemoWidth(facet::T().metrics.cardWidth)});
    ctx.Probe("display.counterInput");
    const int length = ImTextCountCharsFromUtf8(ui.counterText, nullptr);
    facet::CharacterCounter(length, 24, {.warnRemaining = 6});
    ctx.Probe("display.counter");
    ImGui::SameLine();
    facet::CharacterCounter(length, 24, {.ring = true, .warnRemaining = 6});
    ImGui::SameLine();
    facet::CharacterCounter(Is(ctx, kError) ? 30 : length, 24);
    ImGui::EndGroup();
    LabeledRow("Inline code");
    if (Is(ctx, kMarks))
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    if (facet::InlineCode("copied_value##inline", {.copyOnActivate = true}))
        ++ui.inlineCopies;
    ctx.Probe("display.inline");
    ImGui::SameLine();
    facet::demo::NoteBeside("Click, Enter or Space copies it (%d).", ui.inlineCopies);
}

void CodeSection(facet::demo::PageCtx& ctx)
{
    const float viewHeight = facet::T().spacing.xxxl * kViewSpans;
    SectionHeader("Code and logs");
    LabeledRow("Code view");
    if (Is(ctx, kCode))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::CodeView("code", kCodeSample, {.language = facet::CodeLanguage::Cpp, .highlightLines = kHighlightedLines}))
        ++ui.codeCopies;
    ctx.Probe("display.code");
    if (Is(ctx, kCode))
        ctx.RevealInCapture(0.3f);
    LabeledRow("JSON and INI");
    ImGui::BeginGroup();
    facet::CodeView("json", kJsonSample, {.language = facet::CodeLanguage::Json, .lineNumbers = false, .copyButton = false});
    facet::CodeView("ini", kIniSample, {.language = facet::CodeLanguage::Ini, .wrap = true, .copyButton = false, .width = DemoWidth(260.0f)});
    ImGui::EndGroup();
    LabeledRow("Log view");
    ImGui::BeginGroup();
    facet::DataState logState = facet::DataState::Ready;
    if (Is(ctx, kLoading))
        logState = facet::DataState::Loading;
    else if (Is(ctx, kError))
        logState = facet::DataState::Error;
    ui.logId = ImGui::GetID("log");
    facet::LogView("log", ui.logRows, &GeneratedLogEntry, nullptr, {.state = logState, .height = viewHeight});
    ctx.Probe("display.log");
    if (facet::Button("Add 10 entries", {.size = facet::ControlSize::Small}))
        ui.logRows += 10;
    ctx.Probe("display.logAdd");
    ImGui::SameLine();
    facet::demo::NoteBeside("%d generated entries; only the visible ones are read.", ui.logRows);
    ImGui::EndGroup();
}

void DataSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Data");
    LabeledRow("Key value list");
    ImGui::BeginGroup();  // keeps the rows in the control column of the row
    facet::BeginKeyValueList("details", {.copyable = true});
    if (Is(ctx, kData))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::KeyValue("Key 1", "Placeholder value one, copied on activation"))
        ++ui.valueCopies;
    ctx.Probe("display.value");
    {
        // The start of the first value line, where KeyValue places it: after the key column of
        // the labeled row layout, or under the key when the row stacks.
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const facet::LabeledRowLayout columns = facet::LayoutLabeledRow(max.x - min.x);
        const float line = facet::MeasureText(facet::TextStyle::Body, "Ag").y;
        const float top = min.y + (columns.stacked ? line + facet::Px(facet::T().spacing.xs) : 0.0f);
        const float left = min.x + columns.controlOffset;
        facet::demo::ProbeRect(ctx, "display.value.text", 0, ImVec2(left, top),
                               ImVec2(left + facet::Px(facet::T().spacing.xxxl), top + line));
    }
    if (facet::KeyValue("Key 2", "Placeholder value with a tone", {.tone = facet::Tone::Success}))
        ++ui.valueCopies;
    if (facet::KeyValue("A longer key that is cut", kWrappedText))
        ++ui.valueCopies;
    facet::EndKeyValueList();
    ImGui::EndGroup();
    if (Is(ctx, kData))
        ctx.RevealInCapture(0.4f);
    LabeledRow("Stat tile");
    ImGui::BeginGroup();
    facet::DataState tileState = facet::DataState::Ready;
    if (Is(ctx, kLoading))
        tileState = facet::DataState::Loading;
    else if (Is(ctx, kError))
        tileState = facet::DataState::Error;
    facet::StatTile("Generated value", ui.statValue,
                    {.delta = ui.statDelta, .trend = ui.trend, .state = tileState, .width = DemoWidth(260.0f)});
    ctx.Probe("display.stat");
    ImGui::SameLine();
    facet::StatTile("Generated, lower is better", ui.statValue * 0.5,
                    {.format = "%.1f", .delta = ui.statDelta, .invertDelta = true, .state = tileState, .width = DemoWidth(220.0f)});
    if (facet::Button("Add 5", {.size = facet::ControlSize::Small}))
    {
        ui.statValue += 5.0;
        ui.statDelta = 5.0;
    }
    ctx.Probe("display.statAdd");
    ImGui::SameLine();
    if (facet::Button("Remove 5", {.size = facet::ControlSize::Small}))
    {
        ui.statValue -= 5.0;
        ui.statDelta = -5.0;
    }
    ImGui::EndGroup();
    Note("DataTable has a page of its own: Widgets, Data table.");
}

} // namespace

FACET_DEMO_PAGE("display", "Display", "Widgets", 110, "default", "alerts", "progress", "marks", "code", "data", "loading",
                "error", "disabled")
{
    for (int i = 0; i < kTrendSamples; ++i)
        ui.trend[i] = 0.5f + 0.4f * std::sin(static_cast<float>(i) * 0.5f);  // generated, not data

    facet::demo::PageHeader("Display",
                            "Text styles, alerts and empty states, progress and loading, badges and status marks, "
                            "key caps, code and log views, key-value lists and stat tiles.");
    facet::demo::ReadyFamilyNote("display");

    const bool disabled = Is(ctx, kDisabled);
    if (disabled)
        ImGui::BeginDisabled();
    TextSection(ctx);
    AlertsSection(ctx);
    ProgressSection(ctx);
    MarksSection(ctx);
    CodeSection(ctx);
    DataSection(ctx);
    if (disabled)
        ImGui::EndDisabled();
}

FACET_DEMO_SELFTEST("display")
{
    using T = facet::demo::Target;
    test.Do([] {
        ui.progress = 0.4f;
        ui.progressHovered = false;
        ui.sectionActions = 0;
        ui.bannerOpen = true;
        ui.alertOpen = true;
        ui.bannerPrimary = 0;
        ui.bannerSecondary = 0;
        ui.alertActions = 0;
        ui.emptyActions = 0;
        ui.dataState = facet::DataState::Ready;
        ui.dataCount = 3;
        ui.retries = 0;
        ui.inlineCopies = 0;
        ui.codeCopies = 0;
        ui.valueCopies = 0;
        ui.logRows = kLogRows;
        ui.statValue = 40.0;
        ui.pillWarning = false;
        ImStrncpy(ui.counterText, "Placeholder", sizeof(ui.counterText));
    });

    // Text: a cut line shows its whole text on hover; the section action works by mouse and keys.
    test.Hover(T::Item("display.truncated"), 0.8f);
    test.Expect("the cut line shows its whole text on hover", [] { return ui.truncatedTip; });
    test.Click("display.section");  // the heading itself: nothing happens
    test.Expect("a click on the heading does nothing", [] { return ui.sectionActions == 0; });
    // The action ends the row; a few units in from the end land on it at every scale.
    test.MouseMove(T::Item("display.section", 1.0f, 0.5f).Offset(-6.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the action activates it", [] { return ui.sectionActions == 1; });
    test.Click("display.truncatedFocus");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("display.section");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the focused action activates it", [] { return ui.sectionActions == 2; });

    // Banner: Tab to its actions, Space and Enter activate them; Escape dismisses; a click on the
    // close button dismisses; the page brings it back.
    test.Reveal("display.banner");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.MouseMove(T::Item("display.section", 1.0f, 0.5f).Offset(-6.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);  // Action 1
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on action 1 activates it", [] { return ui.bannerPrimary == 1; });
    test.KeyPress(ImGuiKey_Tab);  // Action 2
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on action 2 activates it", [] { return ui.bannerSecondary == 1; });
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape on a focused action dismisses the banner", [] { return !ui.bannerOpen; });
    test.WaitSeconds(0.4f);
    test.Click("display.bannerAgain");
    test.Expect("the page shows the banner again", [] { return ui.bannerOpen; });
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("display.banner", 1.0f, 0.0f).Offset(-23.0f, 20.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the close button dismisses the banner", [] { return !ui.bannerOpen; });
    test.WaitSeconds(0.4f);
    test.Click("display.bannerAgain");

    // Inline alert: its action by keyboard, the close button by Escape.
    test.Reveal("display.alert");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.Click("display.bannerAgain");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("display.alert", 0.0f, 1.0f).Offset(60.0f, -24.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the alert action activates it", [] { return ui.alertActions == 1; });
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus on the action
    test.KeyPress(ImGuiKey_Tab);  // the close button
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the close button dismisses the alert", [] { return !ui.alertOpen; });

    // Empty state and data state view (after the alert has collapsed).
    test.WaitSeconds(0.4f);
    test.Reveal("display.empty");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.MouseMove(T::Item("display.empty", 0.5f, 1.0f).Offset(0.0f, -30.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the empty state action activates it", [] { return ui.emptyActions == 1; });
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused action activates it again", [] { return ui.emptyActions == 2; });
    test.Do([] { ui.dataState = facet::DataState::Error; });
    test.Wait(2);
    test.Reveal("display.dataView");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.MouseMove(T::Item("display.dataView", 1.0f, 0.5f).Offset(-24.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("Retry in the error state is reported", [] { return ui.retries == 1 && ui.dataState == facet::DataState::Loading; });

    // The progress bar follows its value and is not operable: a click on it changes nothing and
    // Tab passes from the value slider over it.
    test.Reveal("display.progress");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.ExpectVisible("display.progress");
    test.Hover(T::Item("display.progress"), 0.2f);
    test.Expect("the progress bar answers ImGui::IsItemHovered", [] { return ui.progressHovered; });
    test.Click("display.progress");
    test.ExpectNear("a click on the bar leaves the value", [] { return ui.progress; }, 0.4, 0.0001);
    test.Drag({T::Item("display.progressValue", 0.5f), T::Item("display.progressValue", 1.2f)}, 0.25f);
    test.ExpectNear("the slider sets the value the bar shows", [] { return ui.progress; }, 1.0, 0.0001);
    test.Click("display.progressValue");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.Expect("Tab passes over the progress bar",
                [] { return ui.progressId != 0 && ImGui::GetFocusID() != ui.progressId; });

    // Status pill: the button changes its state.
    test.Click("display.pillToggle");
    test.Expect("the pill shows the other state", [] { return ui.pillWarning; });

    // Character counter: typing changes the count it shows.
    test.Click("display.counterInput");
    test.KeyPress(ImGuiKey_End);
    test.Type(" text");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the counter follows the typed text", [] { return std::strcmp(ui.counterText, "Placeholder text") == 0; });

    // Inline code copies by click and by keyboard.
    test.SetClipboard("");
    test.Click("display.inline");
    test.ExpectClipboard("a click copies the inline code", "copied_value");
    test.SetClipboard("");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.ExpectClipboard("Space copies the focused inline code", "copied_value");
    test.Expect("both copies are reported", [] { return ui.inlineCopies == 2; });

    // Code view: a click focuses it, Ctrl+C copies the whole text, Page Down scrolls it.
    test.Reveal("display.code");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.SetClipboard("");
    test.Click("display.code");
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("Ctrl+C copies the code", kCodeSample);
    test.Expect("the copy is reported", [] { return ui.codeCopies == 1; });

    // Log view: a click selects a row and Ctrl+C copies it; scrolling up stops following, the jump
    // button and the End key resume it.
    test.Reveal("display.log");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.Expect("the log follows its end", [] { return facet::internal::DisplayLogViewFollowing(ui.logId); });
    test.Wheel(T::Item("display.log"), 3.0f);
    test.WaitSeconds(0.4f);
    test.Expect("scrolling up stops following", [] { return !facet::internal::DisplayLogViewFollowing(ui.logId); });
    test.Click("display.logAdd");
    test.Expect("new entries leave the view where it is", [] { return !facet::internal::DisplayLogViewFollowing(ui.logId); });
    test.MouseMove(T::Item("display.log", 1.0f, 1.0f).Offset(-40.0f, -11.0f));
    test.MouseDown();
    test.MouseUp();
    test.WaitSeconds(0.2f);
    test.Expect("the jump button resumes following", [] { return facet::internal::DisplayLogViewFollowing(ui.logId); });
    test.Wheel(T::Item("display.log"), 3.0f);
    test.WaitSeconds(0.4f);
    test.SetClipboard("");
    test.Click("display.log");
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.Expect("Ctrl+C copies the selected line", [] {
        const char* text = ImGui::GetClipboardText();
        return text != nullptr && std::strstr(text, "Generated entry") != nullptr;
    });
    test.KeyPress(ImGuiKey_End);
    test.WaitSeconds(0.4f);
    test.Expect("End resumes following", [] { return facet::internal::DisplayLogViewFollowing(ui.logId); });

    // Key-value list: values copy by click and by keyboard.
    test.Reveal("display.value");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.SetClipboard("");
    // The start of the first line of the value: beside its key on a wide page, under it on a
    // narrow one.
    test.MouseMove(T::Item("display.value.text"));
    test.MouseDown();
    test.MouseUp();
    test.ExpectClipboard("a click copies the value", "Placeholder value one, copied on activation");
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectClipboard("Enter copies the next value", "Placeholder value with a tone");
    test.Expect("both copies are reported", [] { return ui.valueCopies == 2; });

    // Stat tile: a new value counts to it.
    test.Click("display.statAdd");
    test.WaitSeconds(1.0f);
    test.ExpectNear("the tile shows the new value", [] { return ui.statValue; }, 45.0, 0.0001);
}
