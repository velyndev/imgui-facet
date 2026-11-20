// The public API of the pickers family (facet/widgets/pickers.h) while its widgets are planned:
// every options struct follows the conventions, and every function is safe to call, submits one
// placeholder item that cannot be operated, leaves the Dear ImGui stacks as it found them, returns
// the neutral result and writes nothing.

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

static_assert(OptionsFollowConventions<facet::ColorWellOptions>());
static_assert(OptionsFollowConventions<facet::ColorPickerOptions>());
static_assert(OptionsFollowConventions<facet::ColorInputsOptions>());
static_assert(OptionsFollowConventions<facet::SwatchPaletteOptions>());
static_assert(OptionsFollowConventions<facet::SatValSquareOptions>());
static_assert(OptionsFollowConventions<facet::ColorChannelSliderOptions>());
static_assert(OptionsFollowConventions<facet::ColorModeEditorOptions>());
static_assert(OptionsFollowConventions<facet::DatePickerOptions>());
static_assert(OptionsFollowConventions<facet::DateRangePickerOptions>());
static_assert(OptionsFollowConventions<facet::TimePickerOptions>());
static_assert(OptionsFollowConventions<facet::AnglePickerOptions>());
static_assert(OptionsFollowConventions<facet::IconPickerOptions>());
static_assert(OptionsFollowConventions<facet::ThumbnailPickerOptions>());
static_assert(OptionsFollowConventions<facet::FileBrowserOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::ColorPickerMode{} == facet::ColorPickerMode::Square);
static_assert(facet::ColorModel{} == facet::ColorModel::Rgb);
static_assert(facet::ColorChannel{} == facet::ColorChannel::Hue);
static_assert(facet::ColorAnimation{} == facet::ColorAnimation::Static);
static_assert(facet::Weekday{} == facet::Weekday::Monday);
static_assert(facet::DatePrecision{} == facet::DatePrecision::Day);
static_assert(facet::FileBrowserMode{} == facet::FileBrowserMode::Open);

// Every function of the family once, with the overloads, as an app would call it.
void CallPickerStubs(Values& v)
{
    using namespace facet;
    char* const buffer = v.buffer.data();
    const std::size_t bufferSize = v.buffer.size();
    FACET_CHECK_STUB(false, ColorWell("Well 1", &v.color, {.alpha = true, .showHex = true}));
    FACET_CHECK_STUB(false, ColorPicker("Picker 1", &v.color, {.original = &v.color, .swatches = kSwatches}));
    FACET_CHECK_STUB(false, ColorInputs("Inputs 1", &v.color, {.model = ColorModel::Hsv}));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 1", &v.color, std::span(kSwatches), &v.history));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 2", &v.color, &v.swatches, nullptr));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 3", &v.color, std::span<ImVec4>(v.swatchStorage), &v.swatchCount, nullptr));
    FACET_CHECK_STUB(false, SatValSquare("Square 1", &v.color));
    FACET_CHECK_STUB(false, ColorChannelSlider("Channel 1", &v.color, {.channel = ColorChannel::Alpha}));
    FACET_CHECK_STUB(false, ColorModeEditor("Mode editor 1", &v.animated));
    CHECK(SampleAnimatedColor(v.animated, 1.0).x == v.animated.color.x);
    FACET_CHECK_STUB(false, DatePicker("Date 1", &v.date, {.inlineCalendar = true}));
    FACET_CHECK_STUB(false, DateRangePicker("Dates 1", &v.start, &v.end));
    FACET_CHECK_STUB(false, TimePicker("Time 1", &v.time, {.showSeconds = true}));
    FACET_CHECK_STUB(false, AnglePicker("Angle 1", &v.radians));
    FACET_CHECK_STUB(false, IconPicker("Icon 1", &v.icon));
    FACET_CHECK_STUB(false, IconPicker("Icon 2", &v.icon, kChoices));
    FACET_CHECK_STUB(false, ThumbnailPicker("Thumbnails 1", &v.index, kThumbnails));
    FACET_CHECK_STUB(false, ThumbnailPicker("Thumbnails 2", std::span<bool>(v.thumbnailFlags), kThumbnails,
                                            {.state = DataState::Loading}));
    FACET_CHECK_STUB(false, FileBrowser("Browser 1", &v.text, {.mode = FileBrowserMode::Folder}));
    FACET_CHECK_STUB(false, FileBrowser("Browser 2", buffer, bufferSize, {.fileTypes = kFileTypes, .readOnly = true}));
}

// Number of FACET_CHECK_STUB calls in CallPickerStubs.
constexpr int kPickerStubCalls = 19;

} // namespace

TEST_CASE("Every pickers stub returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallPickerStubs, kPickerStubCalls);
}

TEST_CASE("Pickers stubs called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::ColorPicker("Picker 1", &values.color));
    CHECK(Describe(values) == pristine);
}

TEST_CASE("The inline calendar placeholder fits a short row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Date date;
    float available = 0.0f;
    session.SetUi([&] {
        facet::DatePicker("Date##wide", &date, {.inlineCalendar = true});
        session.Track("date wide");
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(600.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::DatePicker("Date", &date, {.inlineCalendar = true});
        session.Track("date");
        ImGui::EndChild();
    });
    session.Frame();

    REQUIRE(available > 1.0f);
    CHECK(session.ItemRect("date wide").GetWidth() > available);
    CHECK(session.ItemRect("date").GetWidth() == doctest::Approx(available));
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
