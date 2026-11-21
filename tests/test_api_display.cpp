// The public API of the display family (facet/widgets/display.h) without DataTable, which
// tests/test_api_content.cpp covers: every options struct follows the conventions, and every
// function is called once inside a real Dear ImGui frame, in a window and again inside
// ImGui::BeginDisabled. After each call the stacks must be as before, a widget must have
// submitted one item of non-zero size with the id of its label, the neutral result must come back
// and no value argument may have changed. Behaviour tests are in tests/test_display.cpp.

#include <cmath>
#include <cstddef>
#include <string>
#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/content_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

static_assert(OptionsFollowConventions<facet::StyledTextOptions>());
static_assert(OptionsFollowConventions<facet::TruncatedTextOptions>());
static_assert(OptionsFollowConventions<facet::SectionHeaderOptions>());
static_assert(OptionsFollowConventions<facet::BannerOptions>());
static_assert(OptionsFollowConventions<facet::InlineAlertOptions>());
static_assert(OptionsFollowConventions<facet::EmptyStateOptions>());
static_assert(OptionsFollowConventions<facet::DataStateViewOptions>());
static_assert(OptionsFollowConventions<facet::ProgressBarOptions>());
static_assert(OptionsFollowConventions<facet::ProgressRingOptions>());
static_assert(OptionsFollowConventions<facet::SpinnerOptions>());
static_assert(OptionsFollowConventions<facet::SkeletonOptions>());
static_assert(OptionsFollowConventions<facet::BadgeOptions>());
static_assert(OptionsFollowConventions<facet::CountBadgeOptions>());
static_assert(OptionsFollowConventions<facet::StatusDotOptions>());
static_assert(OptionsFollowConventions<facet::StatusPillOptions>());
static_assert(OptionsFollowConventions<facet::KeyCapOptions>());
static_assert(OptionsFollowConventions<facet::ShortcutHintOptions>());
static_assert(OptionsFollowConventions<facet::CharacterCounterOptions>());
static_assert(OptionsFollowConventions<facet::InlineCodeOptions>());
static_assert(OptionsFollowConventions<facet::CodeViewOptions>());
static_assert(OptionsFollowConventions<facet::LogViewOptions>());
static_assert(OptionsFollowConventions<facet::KeyValueListOptions>());
static_assert(OptionsFollowConventions<facet::KeyValueOptions>());
static_assert(OptionsFollowConventions<facet::StatTileOptions>());

static_assert(HasDataBlock<facet::LogViewOptions>());
// DataStateView takes the state as a parameter, so its options hold only the texts of the
// block, still after the widget members (rule 4).
static_assert(offsetof(facet::DataStateViewOptions, retryLabel) < offsetof(facet::DataStateViewOptions, emptyText));
static_assert(offsetof(facet::DataStateViewOptions, emptyText) < offsetof(facet::DataStateViewOptions, errorText));
static_assert(offsetof(facet::DataStateViewOptions, errorText) < offsetof(facet::DataStateViewOptions, height));

// One alignment enum for styled text and table columns.
static_assert(std::is_same_v<decltype(facet::StyledTextOptions::align), facet::Align>);

// Log rows have one height, so the view can read only the visible entries.
template <class T>
concept HasWrap = requires(T options) { options.wrap; };
static_assert(!HasWrap<facet::LogViewOptions>);
static_assert(HasWrap<facet::CodeViewOptions>);  // the check itself works

void DisplayCalls()
{
    using namespace facet;
    Check("StyledText", Expect::Item, "Body text", [] { StyledText("Body text", {.tone = Tone::Accent}); });
    Check("TruncatedText", Expect::Item, "A long line", [] { TruncatedText("A long line", {.mode = TruncateMode::Middle}); });
    Check("SectionHeader", Expect::Item, "Section", [] {
        CHECK_FALSE(SectionHeader("Section", {.description = "Description", .actionLabel = "Action"}));
    });

    bool open = true;
    Check("Banner", Expect::Item, "Title##banner", [&] {
        CHECK(Banner("Title##banner", &open, "Message", {.primaryAction = "Action"}) == BannerAction::None);
    });
    CHECK(open);
    Check("Banner without close button", Expect::Item, "##banner2",
          [] { CHECK(Banner("##banner2", nullptr, "Message") == BannerAction::None); });
    Check("InlineAlert", Expect::Item, "Title##alert", [&] {
        CHECK(InlineAlert("Title##alert", &open, "Message", {.tone = Tone::Warning, .actionLabel = "Action"}) ==
              InlineAlertAction::None);
    });
    CHECK(open);
    Check("EmptyState", Expect::Item, "Nothing here", [] {
        CHECK_FALSE(EmptyState("Nothing here", {.message = "Message", .actionLabel = "Action"}));
    });

    // A data state view with content opens a group and a scope for it.
    Check("BeginDataStateView, Ready", Expect::NoItem, nullptr, [] {
        const bool content = BeginDataStateView("data", DataState::Ready, 3);
        CHECK(content);
        if (content)
        {
            CHECK(internal::OpenScopeCount() > 0);
            ImGui::TextUnformatted("content");
            EndDataStateView();
        }
    });
    for (const DataState state : {DataState::Loading, DataState::Error})
        Check("BeginDataStateView, Loading or Error", Expect::Item, "data",
              [state] { CHECK_FALSE(BeginDataStateView("data", state, 3, {.retryLabel = "Retry"})); });
    Check("BeginDataStateView, empty", Expect::Item, "data", [] { CHECK_FALSE(BeginDataStateView("data", DataState::Ready, 0)); });
    Check("DataStateViewRetryPressed", Expect::NoItem, nullptr, [] { CHECK_FALSE(DataStateViewRetryPressed()); });

    Check("ProgressBar", Expect::Item, "Progress", [] { ProgressBar("Progress", 0.4f, {.showPercent = true}); });
    Check("ProgressBar, indeterminate", Expect::Item, "Progress##2", [] { ProgressBar("Progress##2", kIndeterminate); });
    Check("ProgressRing", Expect::Item, "Ring", [] { ProgressRing("Ring", 0.4f, {.showPercent = true}); });
    Check("Spinner", Expect::Item, "Loading", [] { Spinner("Loading"); });
    Check("Skeleton, text", Expect::Item, nullptr, [] { Skeleton({.lines = 3}); });
    Check("Skeleton, circle", Expect::Item, nullptr, [] { Skeleton({.shape = SkeletonShape::Circle}); });
    Check("Badge", Expect::Item, "Label", [] { Badge("Label", {.tone = Tone::Success}); });
    Check("CountBadge", Expect::Item, nullptr, [] { CountBadge(120); });
    Check("CountBadge, zero", Expect::NoItem, nullptr, [] { CountBadge(0); });
    Check("StatusDot", Expect::Item, "Status", [] { StatusDot("Status", {.tone = Tone::Success}); });
    Check("StatusPill", Expect::Item, "Status##pill", [] { StatusPill("Status##pill", {.liveDot = true}); });
    Check("KeyCap", Expect::Item, nullptr, [] { KeyCap(ImGuiMod_Ctrl | ImGuiKey_S); });
    Check("ShortcutHint", Expect::Item, "Save", [] { ShortcutHint("Save", ImGuiMod_Ctrl | ImGuiKey_S); });
    Check("ShortcutHint, registered", Expect::Item, "Open", [] { ShortcutHint("Open", ShortcutId{}); });
    Check("CharacterCounter", Expect::Item, nullptr, [] { CharacterCounter(12, 40); });
    Check("CharacterCounter, ring", Expect::Item, nullptr, [] { CharacterCounter(12, 40, {.ring = true}); });
    Check("InlineCode", Expect::Item, "value", [] { CHECK_FALSE(InlineCode("value", {.copyOnActivate = true})); });
    Check("CodeView", Expect::Item, "code", [] { CHECK_FALSE(CodeView("code", "int x = 0;", {.language = CodeLanguage::Cpp})); });

    static const LogEntry entries[] = {{"Line 1"}, {"Line 2", LogLevel::Warning}};
    Check("LogView", Expect::Item, "log", [] { LogView("log", entries, {.height = 120.0f}); });
    Check("LogView, getter", Expect::Item, "log2", [] {
        LogView("log2", 2, [](void*, int index, LogEntry* out) {
            *out = entries[index];
            return true;
        }, nullptr, {.height = 120.0f});
    });

    Check("KeyValueList", Expect::NoItem, nullptr, [] {
        BeginKeyValueList("details", {.copyable = true});
        CHECK_FALSE(KeyValue("Key", "Value"));
        EndKeyValueList();
    });
    Check("KeyValue", Expect::Item, "Key 2", [] { CHECK_FALSE(KeyValue("Key 2", "Value")); });
    // The scope form; the list's group is the last item afterwards.
    Check("KeyValueListScope", Expect::Item, nullptr, [] {
        KeyValueListScope list("details2");
        CHECK_FALSE(KeyValue("Key 3", "Value"));
    });
    static const float trend[] = {1.0f, 2.0f, 3.0f};
    Check("StatTile", Expect::Item, "Tile", [] { StatTile("Tile", 42.0, {.delta = 1.5, .trend = trend}); });
}

} // namespace

TEST_CASE("A log entry without a time holds NaN, outside every clock")
{
    // -1 would be a valid time for a clock that includes it.
    CHECK(std::isnan(facet::LogEntry{}.time));
}

TEST_CASE("Every display widget submits one item, returns the neutral result and keeps every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &DisplayCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Display widgets without a label submit an item whose id is not the window id")
{
    // An empty label hashes to the id of the window, which Dear ImGui asserts on for an item at
    // the root of a window. Each call runs first in a visible window, where ItemAdd checks it.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    const auto first = [&](const char* name, auto&& call) {
        INFO("call: " << std::string(name));
        session.SetUi([&] {
            call();
            CHECK(ImGui::GetItemID() != 0);
            CHECK(ImGui::GetItemID() != ImGui::GetCurrentWindow()->ID);
            CHECK(ImGui::IsItemVisible());
            ++checked;
        });
        session.Frame();
    };
    first("Skeleton", [] { facet::Skeleton(); });
    first("Skeleton, circle", [] { facet::Skeleton({.shape = facet::SkeletonShape::Circle}); });
    first("CharacterCounter, ring", [] { facet::CharacterCounter(1, 0, {.ring = true}); });
    first("Badge, empty text", [] { facet::Badge(""); });
    first("StyledText, null text", [] { facet::StyledText(nullptr); });
    CHECK(checked == 5);
}

TEST_CASE("Display placeholders are at least as tall as a small control")
{
    // The placeholder text is Footnote; a box of one line of Body or less cuts its descenders.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const float minimum = ControlHeight(ControlSize::Small);
        const auto tall = [&](const char* widget, auto&& call) {
            INFO("widget: " << std::string(widget));
            call();
            CHECK(ImGui::GetItemRectSize().y >= minimum);
            ++checked;
        };
        tall("Skeleton, one line", [] { Skeleton({.lines = 1}); });
        tall("Skeleton, low", [] { Skeleton({.shape = SkeletonShape::Rect, .height = 4.0f}); });
    });
    session.Frame();
    CHECK(checked == 2);
}

TEST_CASE("Content-sized display placeholders are wide enough to name the widget and say planned")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const auto wide = [&](const char* widget, auto&& call) {
            INFO("widget: " << std::string(widget));
            call();
            char text[64];
            ImFormatString(text, sizeof(text), "%s: planned", widget);
            CHECK(ImGui::GetItemRectSize().x >= MeasureText(TextStyle::Footnote, text).x);
            ++checked;
        };
        wide("ProgressRing", [] { ProgressRing("##ring", 0.5f); });
        wide("Spinner", [] { Spinner("##spinner"); });
        wide("KeyCap", [] { KeyCap(ImGuiKey_A); });
        wide("CharacterCounter", [] { CharacterCounter(1, 0); });
        wide("CharacterCounter", [] { CharacterCounter(1, 0, {.ring = true}); });
        wide("InlineCode", [] { InlineCode("a"); });
    });
    session.Frame();
    CHECK(checked == 6);
}

TEST_CASE("A display End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndDataStateView();
        facet::EndKeyValueList();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 2);
}
