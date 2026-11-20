// The public API of the selection family (facet/widgets/selection.h): every options struct
// follows the conventions, and every widget that is still a placeholder is safe to call, submits
// one item that cannot be operated, leaves the Dear ImGui stacks as it found them, returns the
// neutral result and writes nothing. Behaviour tests of implemented widgets go into
// tests/test_selection.cpp.

#include <span>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/controls_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::controls;

static_assert(OptionsFollowConventions<facet::ToggleOptions>());
static_assert(OptionsFollowConventions<facet::CheckboxOptions>());
static_assert(OptionsFollowConventions<facet::CheckboxGroupOptions>());
static_assert(OptionsFollowConventions<facet::RadioGroupOptions>());
static_assert(OptionsFollowConventions<facet::SegmentedControlOptions>());
static_assert(OptionsFollowConventions<facet::ChipOptions>());
static_assert(OptionsFollowConventions<facet::ChipGroupOptions>());
static_assert(OptionsFollowConventions<facet::CycleSelectorOptions>());
static_assert(OptionsFollowConventions<facet::KeybindModeSelectorOptions>());
static_assert(OptionsFollowConventions<facet::OptionCardGroupOptions>());

// Every placeholder of the family once, with the overloads, as an app would call it.
void CallSelectionStubs(Values& v)
{
    using namespace facet;
    const std::span<bool> flags(v.flags);
    FACET_CHECK_STUB(false, CheckboxGroup("Checkboxes 1", flags, kChoices, {.selectAll = true}));
    FACET_CHECK_STUB(false, CheckboxGroup("Checkboxes 2", flags, kLabels));
    FACET_CHECK_STUB(false, CycleSelector("Cycle 1", &v.index, kChoices));
    FACET_CHECK_STUB(false, CycleSelector("Cycle 2", &v.index, kLabels, {.pageDots = true}));
    FACET_CHECK_STUB(false, KeybindModeSelector("Mode 1", &v.mode, {.allowOff = false}));
    FACET_CHECK_STUB(false, OptionCardGroup("Cards 1", &v.index, kCards));
    FACET_CHECK_STUB(false, OptionCardGroup("Cards 2", std::span<bool>(v.thumbnailFlags), kCards, {.columns = 2}));
}

// Number of FACET_CHECK_STUB calls in CallSelectionStubs.
constexpr int kSelectionStubCalls = 7;

} // namespace

TEST_CASE("Every selection placeholder returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallSelectionStubs, kSelectionStubCalls);
}

TEST_CASE("Selection placeholders called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool flag = true;
    CHECK_FALSE(facet::Checkbox("Checkbox 1", &flag));
    CHECK(flag);
}

TEST_CASE("Controls that narrow themselves never outgrow a short row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::BindMode mode = facet::BindMode::Hold;
    int index = 0;
    float available = 0.0f;
    float wideSegments = 0.0f;
    session.SetUi([&] {
        // In a wide row the controls keep their content width.
        facet::SegmentedControl("Segments##wide", &index, kLabels);
        wideSegments = GImGui->LastItemData.Rect.GetWidth();
        // A row far narrower than the segments, the mode names or the buttons.
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(200.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::SegmentedControl("Segments", &index, kLabels);
        session.Track("segments");
        facet::KeybindModeSelector("Mode", &mode);
        session.Track("mode");
        facet::ToggleButtonGroup("Toggles", &index, kChoices, {.iconOnly = false});
        session.Track("toggles");
        // A width the caller asks for is kept.
        facet::SegmentedControl("Segments##asked", &index, kLabels, {.width = 300.0f});
        session.Track("asked");
        ImGui::EndChild();
    });
    session.Frame();

    REQUIRE(available > 1.0f);
    CHECK(wideSegments > available);
    CHECK(session.ItemRect("segments").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("mode").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("toggles").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("asked").GetWidth() == doctest::Approx(facet::Px(300.0f)));
}
