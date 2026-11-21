// The public API of the date, time and other pickers (DatePicker, DateRangePicker, TimePicker,
// AnglePicker, IconPicker, ThumbnailPicker and FileBrowser in facet/widgets/pickers.h) while they
// are planned: every options struct follows the conventions, and every function is safe to call,
// submits one placeholder item that cannot be operated, leaves the Dear ImGui stacks as it found
// them, returns the neutral result and writes nothing. The color pickers have their own file
// (tests/test_api_pickers_color.cpp).

#include <span>
#include <string>

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

static_assert(OptionsFollowConventions<facet::DatePickerOptions>());
static_assert(OptionsFollowConventions<facet::DateRangePickerOptions>());
static_assert(OptionsFollowConventions<facet::TimePickerOptions>());
static_assert(OptionsFollowConventions<facet::AnglePickerOptions>());
static_assert(OptionsFollowConventions<facet::IconPickerOptions>());
static_assert(OptionsFollowConventions<facet::ThumbnailPickerOptions>());
static_assert(OptionsFollowConventions<facet::FileBrowserOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::Weekday{} == facet::Weekday::Monday);
static_assert(facet::DatePrecision{} == facet::DatePrecision::Day);
static_assert(facet::FileBrowserMode{} == facet::FileBrowserMode::Open);

// Every function of this group once, with the overloads, as an app would call it.
void CallDatePickerStubs(Values& v)
{
    using namespace facet;
    char* const buffer = v.buffer.data();
    const std::size_t bufferSize = v.buffer.size();
    FACET_CHECK_STUB(false, ThumbnailPicker("Thumbnails 1", &v.index, kThumbnails));
    FACET_CHECK_STUB(false, ThumbnailPicker("Thumbnails 2", std::span<bool>(v.thumbnailFlags), kThumbnails,
                                            {.state = DataState::Loading}));
    FACET_CHECK_STUB(false, FileBrowser("Browser 1", &v.text, {.mode = FileBrowserMode::Folder}));
    FACET_CHECK_STUB(false, FileBrowser("Browser 2", buffer, bufferSize, {.fileTypes = kFileTypes, .readOnly = true}));
}

// Number of FACET_CHECK_STUB calls in CallDatePickerStubs.
constexpr int kDatePickerStubCalls = 4;

} // namespace

TEST_CASE("Every date, time and other picker stub returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallDatePickerStubs, kDatePickerStubCalls);
}

TEST_CASE("Date and other picker stubs called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::DatePicker("Date 1", &values.date));
    CHECK_FALSE(facet::FileBrowser("Browser 1", &values.text));
    CHECK(Describe(values) == pristine);
}

TEST_CASE("AnglePicker fits a short row and follows the width rules")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float radians = 0.5f;
    float available = 0.0f;
    session.SetUi([&] {
        facet::AnglePicker("Angle##wide", &radians);
        session.Track("wide");
        // A row narrower than the dial and its degrees field.
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(400.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::AnglePicker("Angle", &radians);
        session.Track("short");
        ImGui::SetNextItemWidth(facet::Px(30.0f));
        facet::AnglePicker("Angle##next", &radians);
        session.Track("next");
        facet::AnglePicker("Angle##asked", &radians, {.width = 300.0f});
        session.Track("asked");
        ImGui::EndChild();
    });
    session.Frame();

    REQUIRE(available > 1.0f);
    CHECK(session.ItemRect("wide").GetWidth() > available);
    CHECK(session.ItemRect("short").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(facet::Px(30.0f)));
    CHECK(session.ItemRect("asked").GetWidth() == doctest::Approx(facet::Px(300.0f)));
    CHECK(radians == 0.5f);
}
