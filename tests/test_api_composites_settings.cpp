// The declared API of the search, preset and app control composites (facet/widgets/composites.h)
// while they are placeholders: every function is called once inside a real Dear ImGui frame, in a
// window and again inside ImGui::BeginDisabled. After each call the id, window, group and Facet
// scope stacks must be as before, a widget must have submitted one item of non-zero size with the
// id of its label, the neutral result must come back and no value argument may have changed. Every
// options struct is checked against the conventions at compile time. The other composites have
// files of their own (tests/test_api_composites_cards.cpp and test_api_composites_editors.cpp); the
// behaviour tests of these widgets go into tests/test_composites_settings.cpp.

#include <cstring>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/content_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

// Entries that can change order carry a stable id (rule 2 of the conventions).
static_assert(HasEntryId<facet::FilterChip>);
static_assert(HasEntryId<facet::KeybindListItem>);
static_assert(HasEntryId<facet::PresetItem>);

// Views of caller data have the data block of rule 7 (HasDataBlock in support/content_stub_check.h).
static_assert(HasDataBlock<facet::ConsoleViewOptions>());
static_assert(HasDataBlock<facet::KeybindListOptions>());
static_assert(HasDataBlock<facet::PresetListOptions>());

// Every widget that writes a value, a selection or item members has readOnly (rule 7).
static_assert(HasReadOnly<facet::ConsoleViewOptions>);
static_assert(HasReadOnly<facet::SearchFilterBarOptions>);
static_assert(HasReadOnly<facet::KeybindListOptions>);
static_assert(HasReadOnly<facet::PresetListOptions>);
static_assert(HasReadOnly<facet::ThemeModeControlOptions>);
static_assert(HasReadOnly<facet::AccentPickerOptions>);
static_assert(HasReadOnly<facet::UiScaleControlOptions>);

static_assert(OptionsFollowConventions<facet::ConsoleViewOptions>());
static_assert(OptionsFollowConventions<facet::SearchFilterBarOptions>());
static_assert(OptionsFollowConventions<facet::SettingsSearchOptions>());
static_assert(OptionsFollowConventions<facet::SettingsSearchMatchOptions>());
static_assert(OptionsFollowConventions<facet::KeybindListOptions>());
static_assert(OptionsFollowConventions<facet::PresetListOptions>());
static_assert(OptionsFollowConventions<facet::OnboardingOptions>());
static_assert(OptionsFollowConventions<facet::ThemeModeControlOptions>());
static_assert(OptionsFollowConventions<facet::AccentPickerOptions>());
static_assert(OptionsFollowConventions<facet::UiScaleControlOptions>());

void SettingsCalls()
{
    using namespace facet;
    static const LogEntry lines[] = {{"Line 1"}};
    static const char* const commands[] = {"command"};
    const LogGetter getter = [](void*, int index, LogEntry* out) {
        *out = lines[index];
        return true;
    };
    std::string command = "text";
    char buffer[16] = "text";
    Check("ConsoleView", Expect::Item, "console", [&] {
        CHECK_FALSE(ConsoleView("console", &command, lines, {.commands = commands, .height = 80.0f}));
    });
    Check("ConsoleView, getter", Expect::Item, "console 2",
          [&] { CHECK_FALSE(ConsoleView("console 2", &command, 1, getter, nullptr, {.height = 80.0f})); });
    Check("ConsoleView, buffer", Expect::Item, "console 3",
          [&] { CHECK_FALSE(ConsoleView("console 3", buffer, sizeof(buffer), lines, {.height = 80.0f})); });
    Check("ConsoleView, buffer and getter", Expect::Item, "console 4", [&] {
        CHECK_FALSE(ConsoleView("console 4", buffer, sizeof(buffer), 1, getter, nullptr, {.height = 80.0f}));
    });
    CHECK(command == "text");
    CHECK(std::strcmp(buffer, "text") == 0);

    // The settings search is ready; it submits no item of its own and keeps every stack.
    Check("SettingsSearch", Expect::NoItem, nullptr, [] {
        BeginSettingsSearch("settings", {.filter = "query"});
        CHECK_FALSE(SettingsSearchMatch("Row", {.keywords = "word", .section = "Section"}));
        CHECK(SettingsSearchMatch("Query row", {.section = "Section"}));
        EndSettingsSearch();
    });
    Check("SettingsSearchScope", Expect::NoItem, nullptr, [] { SettingsSearchScope search("settings 2"); });
    Check("SettingsSearchHitCount", Expect::NoItem, nullptr,
          [] { CHECK(SettingsSearchHitCount("settings", nullptr) == 0); });

    Check("KeybindList, registry", Expect::Item, "binds", [] { CHECK_FALSE(KeybindList("binds", {.filter = "x"})); });
    KeybindListItem binds[] = {{"Action", "Group", {ImGuiKey_A}, {ImGuiKey_B}}};
    const Snapshot bindsBefore(binds);
    Check("KeybindList, items", Expect::Item, "binds 2", [&] { CHECK_FALSE(KeybindList("binds 2", binds)); });
    CHECK(bindsBefore.Unchanged());

    static const PresetItem presets[] = {{"Preset 1"}, {"Preset 2", true}};
    int active = 1;
    Check("PresetList", Expect::Item, "presets", [&] {
        const PresetListResult result = PresetList("presets", &active, presets);
        CHECK(result.action == PresetAction::None);
        CHECK(result.index == -1);
        CHECK(result.name == nullptr);
    });
    CHECK(active == 1);

    // A shown onboarding panel draws its placeholder and reports closed; a hidden one draws nothing.
    bool open = true;
    int step = 1;
    Check("BeginOnboarding, open", Expect::Item, "Welcome", [&] { CHECK_FALSE(BeginOnboarding("Welcome", &open, &step, 3)); });
    CHECK(open);
    CHECK(step == 1);
    bool closed = false;
    Check("BeginOnboarding, closed", Expect::NoItem, nullptr,
          [&] { CHECK_FALSE(BeginOnboarding("Welcome 2", &closed, &step, 3)); });
    CHECK_FALSE(closed);
    Check("OnboardingScope", Expect::NoItem, nullptr, [&] {
        OnboardingScope onboarding("Welcome 3", &closed, &step, 3);
        CHECK_FALSE(static_cast<bool>(onboarding));
    });
}

} // namespace

TEST_CASE("The search, preset and app control stubs submit one item each and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &SettingsCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("The onboarding stub follows kFill and widths relative to the available width")
{
    // Rule 6: kFill is the available width, any other negative width the available width minus
    // that many units, for dialog stubs too.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        for (const float units : {kFill, -10.0f})
        {
            INFO("width " << units);
            const float expected = ImGui::GetContentRegionAvail().x + (units == kFill ? 0.0f : Px(units));
            bool open = true;
            int step = 0;
            if (BeginOnboarding("Welcome", &open, &step, 2, {.width = units}))
                EndOnboarding();
            CHECK(ImGui::GetItemRectSize().x == doctest::Approx(expected));
            ++checked;
        }
    });
    session.Frame();
    CHECK(checked == 2);
}

TEST_CASE("A settings search or onboarding End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndSettingsSearch();
        facet::EndOnboarding();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 2);
}
