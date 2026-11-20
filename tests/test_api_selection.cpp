// The public API of the selection family (facet/widgets/selection.h): every options struct
// follows the conventions, the widgets give the neutral result outside a frame, and the one-line
// controls never outgrow a short row. Every widget of the family is implemented; their behaviour
// tests are in tests/test_selection.cpp.

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

} // namespace

TEST_CASE("Selection widgets called outside a frame return the neutral result and write nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    const bool flag = v.flag;
    const int index = v.index;
    const facet::BindMode mode = v.mode;
    CHECK_FALSE(facet::Toggle("Toggle 1", &v.flag));
    CHECK_FALSE(facet::Checkbox("Checkbox 1", &v.flag));
    CHECK_FALSE(facet::Checkbox("Checkbox 2", &v.check));
    CHECK_FALSE(facet::CheckboxGroup("Checkboxes", std::span<bool>(v.flags), kLabels, {.selectAll = true}));
    CHECK_FALSE(facet::RadioGroup("Radio", &v.index, kChoices));
    CHECK_FALSE(facet::SegmentedControl("Segments", &v.index, kLabels));
    CHECK_FALSE(facet::Chip("Chip", {.removable = true}));
    CHECK_FALSE(facet::ChipRemovePressed());
    CHECK_FALSE(facet::ChipGroup("Chips", &v.index, kLabels, {.removable = true}));
    CHECK(facet::ChipGroupRemovedItem() == -1);
    CHECK_FALSE(facet::CycleSelector("Cycle", &v.index, kChoices));
    CHECK_FALSE(facet::KeybindModeSelector("Mode", &v.mode));
    CHECK_FALSE(facet::OptionCardGroup("Cards", &v.index, kCards));
    CHECK(v.flag == flag);
    CHECK(v.index == index);
    CHECK(v.mode == mode);
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
