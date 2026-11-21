// The declared API of the media family (facet/widgets/media.h): every options struct follows the
// conventions, and every function is called once inside a real Dear ImGui frame without input, in a
// window and again inside ImGui::BeginDisabled. After each call the id, window, group and Facet scope
// stacks must be as before, a widget must have submitted one item of non-zero size with the id of its
// label, the neutral result must come back and no value argument may have changed. The behaviour of
// each widget is tested in tests/test_media.cpp.

#include <cmath>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/scope_stack.h"
#include "support/content_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

static_assert(HasEntryId<facet::AvatarItem>);
static_assert(HasEntryId<facet::ImageItem>);
// Views of caller data have the data block of rule 7.
static_assert(HasDataBlock<facet::ThumbnailStripOptions>());
static_assert(HasDataBlock<facet::CarouselOptions>());
// Every widget that writes a value or a selection has readOnly (rule 7).
static_assert(HasReadOnly<facet::ThumbnailStripOptions>);
static_assert(HasReadOnly<facet::ImageCompareOptions>);
static_assert(HasReadOnly<facet::TimelineScrubberOptions>);

static_assert(OptionsFollowConventions<facet::ImageViewOptions>());
static_assert(OptionsFollowConventions<facet::AvatarOptions>());
static_assert(OptionsFollowConventions<facet::AvatarGroupOptions>());
static_assert(OptionsFollowConventions<facet::ThumbnailStripOptions>());
static_assert(OptionsFollowConventions<facet::CarouselOptions>());
static_assert(OptionsFollowConventions<facet::ImageViewerOptions>());
static_assert(OptionsFollowConventions<facet::ImageCompareOptions>());
static_assert(OptionsFollowConventions<facet::TimelineScrubberOptions>());
static_assert(OptionsFollowConventions<facet::DropZoneOptions>());

void MediaCalls()
{
    using namespace facet;
    const ImageHandle none;
    static const AvatarItem avatars[] = {{"Name 1"}, {"Name 2"}};
    static const ImageItem images[] = {{"Image 1"}, {"Image 2"}};
    static const char* const types[] = {"test.payload"};

    Check("ImageView", Expect::Item, "Image", [&] { CHECK_FALSE(ImageView("Image", none, {.activatable = true})); });
    Check("ImageView, sized", Expect::Item, "Image 2",
          [&] { CHECK_FALSE(ImageView("Image 2", none, {.width = 64.0f, .height = 32.0f})); });
    Check("ImageView, texture", Expect::Item, "Texture", [] {
        CHECK_FALSE(ImageView("Texture", static_cast<ID3D11ShaderResourceView*>(nullptr)));
    });
    Check("ImageView, retry", Expect::Item, "Image 3",
          [&] { CHECK_FALSE(ImageView("Image 3", none, {.activatable = true, .retryLabel = "Retry"})); });
    Check("ImageViewRetryPressed", Expect::NoItem, nullptr, [] { CHECK_FALSE(ImageViewRetryPressed()); });
    Check("Avatar", Expect::Item, "Name", [&] { CHECK_FALSE(Avatar("Name", none, {.activatable = true})); });
    Check("AvatarGroup", Expect::Item, "Group", [] { CHECK_FALSE(AvatarGroup("Group", avatars, {.maxVisible = 1})); });

    int selected = 1;
    Check("ThumbnailStrip", Expect::Item, "Strip", [&] { CHECK_FALSE(ThumbnailStrip("Strip", &selected, images)); });
    Check("ThumbnailStrip, getter", Expect::Item, "Strip 2", [&] {
        CHECK_FALSE(ThumbnailStrip("Strip 2", &selected, 2, [](void*, int index, ImageItem* out) {
            *out = images[index];
            return true;
        }, nullptr));
    });
    Check("Carousel", Expect::Item, "Slides", [&] { CHECK_FALSE(Carousel("Slides", &selected, images, {.loop = true})); });
    CHECK(selected == 1);

    ImageViewerState view{.zoom = 2.0f, .center = ImVec2(3.0f, 4.0f)};
    const Snapshot viewBefore(view);
    Check("ImageViewer", Expect::Item, "Viewer", [&] { CHECK_FALSE(ImageViewer("Viewer", none, &view, {.height = 100.0f})); });
    CHECK(viewBefore.Unchanged());

    float split = 0.25f;
    Check("ImageCompare", Expect::Item, "Compare", [&] { CHECK_FALSE(ImageCompare("Compare", &split, none, none)); });
    CHECK(split == 0.25f);

    double playhead = 1.0;
    double loopIn = 0.5;
    double loopOut = 2.0;
    Check("TimelineScrubber", Expect::Item, "Time", [&] { CHECK_FALSE(TimelineScrubber("Time", &playhead, 5.0)); });
    Check("TimelineScrubber, loop", Expect::Item, "Time 2",
          [&] { CHECK_FALSE(TimelineScrubber("Time 2", &playhead, &loopIn, &loopOut, 5.0)); });
    CHECK(playhead == 1.0);
    CHECK(loopIn == 0.5);
    CHECK(loopOut == 2.0);

    Check("DropZone", Expect::Item, "Drop", [] { CHECK(DropZone("Drop", types) == DropZoneAction::None); });
    Check("DropZonePayload", Expect::NoItem, nullptr, [] { CHECK(DropZonePayload() == nullptr); });
}

} // namespace

TEST_CASE("The media widgets submit one item each, return the neutral result without input and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &MediaCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("The buffered band of a timeline scrubber defaults to NaN, outside every time")
{
    // -1 would be a valid time for a clock that includes it.
    CHECK(std::isnan(facet::TimelineScrubberOptions{}.buffered));
}
