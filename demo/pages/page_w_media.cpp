// Demo page: Media. Calls every declared media widget once with neutral arguments, grouped by
// purpose. No image is loaded here: every call gets an empty ImageHandle, which the finished
// widgets draw as their failed state. Until Phase 2 implements a widget, its call draws the
// placeholder of the declared size; Phase 2 replaces the placeholders one by one.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Box sizes on this page, in multiples of the xxxl spacing token.
constexpr float kImageSpans = 4.0f;
constexpr float kViewerSpans = 5.0f;
// Length of the example timeline in seconds.
constexpr double kTimelineSeconds = 10.0;

// Placeholder entries: labelled as placeholders, without pictures.
const facet::AvatarItem kAvatars[] = {{"Placeholder 1"}, {"Placeholder 2"}, {"Placeholder 3"}, {"Placeholder 4"}};
const facet::ImageItem kImages[] = {{"Image 1"}, {"Image 2"}, {"Image 3"}};
const char* const kAcceptTypes[] = {"facet.demo.file"};

struct MediaUi
{
    int thumbnail = 0;
    int slide = 0;
    float split = 0.5f;
    facet::ImageViewerState view;
    double playhead = 2.0;
    double loopIn = 1.0;
    double loopOut = 6.0;
};

MediaUi ui;

} // namespace

FACET_DEMO_PAGE("media", "Media", "Widgets", 130)
{
    const facet::Theme& theme = facet::T();
    const float imageSide = theme.spacing.xxxl * kImageSpans;
    const float viewerHeight = theme.spacing.xxxl * kViewerSpans;
    const facet::ImageHandle none;

    facet::demo::PageHeader("Media",
                            "Images in design units, avatars, thumbnail strips, carousels, image viewers, before and "
                            "after comparisons, timeline scrubbers and drop zones. The pixel-sized core items Image "
                            "and IconGlyph are shown on the Images and Icons pages. Every widget on this page is a "
                            "placeholder of its declared size until it is implemented in Phase 2.");

    SectionHeader("Images");
    LabeledRow("ImageView");
    facet::ImageView("Placeholder image", none,
                     {.fit = facet::ImageFit::Cover, .corners = facet::Radius::Card, .retryLabel = "Retry",
                      .width = imageSide, .height = imageSide});
    LabeledRow("ImageViewer");
    facet::ImageViewer("Placeholder image##viewer", none, &ui.view, {.minimap = true, .height = viewerHeight});
    LabeledRow("ImageCompare");
    facet::ImageCompare("Comparison", &ui.split, none, none,
                        {.beforeLabel = "Before", .afterLabel = "After", .height = viewerHeight});

    SectionHeader("People");
    LabeledRow("Avatar");
    facet::Avatar("Placeholder", none, {.showStatus = true, .status = facet::Tone::Success});
    LabeledRow("AvatarGroup");
    facet::AvatarGroup("Members", kAvatars, {.maxVisible = 3});

    SectionHeader("Collections");
    LabeledRow("ThumbnailStrip");
    facet::ThumbnailStrip("Thumbnails", &ui.thumbnail, kImages, {.captions = true});
    LabeledRow("Carousel");
    facet::Carousel("Slides", &ui.slide, kImages, {.captions = true, .height = viewerHeight});

    SectionHeader("Time and files");
    LabeledRow("TimelineScrubber");
    facet::TimelineScrubber("Timeline", &ui.playhead, &ui.loopIn, &ui.loopOut, kTimelineSeconds);
    LabeledRow("DropZone");
    facet::DropZone("Drop zone", kAcceptTypes, {.hint = "Drop a file here", .icon = facet::Icon("upload")});
}
