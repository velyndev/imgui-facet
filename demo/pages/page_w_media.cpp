// Demo page: Media. Shows every media widget with its variants, one section per widget. Every
// picture is a test pattern generated here from the theme colors (gradients, grids, rings and
// corner marks that show how a fit crops or stretches); none is a photo, and every name is a
// labelled placeholder.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "pages/pages.h"

// The encoder is compiled with internal linkage in this file as well, like in page_images.cpp.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace
{

using facet::demo::DemoWidth;
using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Pattern sizes in texels: pixel data, not layout lengths.
constexpr int kWideWidth = 320;
constexpr int kWideHeight = 180;
constexpr int kTallWidth = 120;
constexpr int kTallHeight = 200;
constexpr int kTinySide = 8;
constexpr int kTileSide = 48;      // shown at its own size, one texel per pixel at scale 1
constexpr int kViewerWidth = 640;
constexpr int kViewerHeight = 400;
constexpr int kAsyncWidth = 1280;  // large enough that decoding takes a visible moment
constexpr int kAsyncHeight = 720;
constexpr int kThumbnails = 12;
constexpr int kSlides = 4;
// Opacity of the background color laid over the grid lines of a pattern.
constexpr float kGridLineOpacity = 0.35f;
// Box sizes on this page, in multiples of the xxxl spacing token.
constexpr float kBoxSpans = 3.0f;
constexpr float kViewerSpans = 7.0f;
constexpr float kCarouselSpans = 12.0f;
// The example timeline: a placeholder clip of 90 seconds.
constexpr double kTimelineSeconds = 90.0;
// The playback of the timeline example runs at real time.
constexpr float kPlaybackRate = 1.0f;
// Seconds per slide of the autoplay example.
constexpr float kAutoplaySeconds = 3.0f;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kPeople,
    kCollections,
    kViewer,
    kTimeline,
    kStates,
    kDisabled,
};

const char* const kFits[] = {"Contain", "Cover", "Fill"};
const char* const kCorners[] = {"None", "Element", "Card"};
const char* const kDataStates[] = {"Ready", "Loading", "Error", "Empty"};
const char* const kOrientations[] = {"Horizontal", "Vertical"};
const facet::Radius kCornerTokens[] = {facet::Radius::None, facet::Radius::Element, facet::Radius::Card};
const char* const kAccepted[] = {"facet.demo.file"};
const char* const kRefusing[] = {"facet.demo.other"};

// Placeholder members of the avatar group: numbered, no names.
const facet::AvatarItem kMembers[] = {{"Placeholder 1"}, {"Placeholder 2"}, {"Placeholder 3"}, {"Placeholder 4"},
                                      {"Placeholder 5"}, {"Placeholder 6"}, {"Placeholder 7"}};

struct MediaUi
{
    int fit = 1;
    int corners = 2;
    bool tint = false;
    int imageActivations = 0;
    int retries = 0;
    bool avatarSelected = false;
    int maxVisible = 3;
    int groupActivations = 0;
    int thumbnail = 0;
    bool captions = true;
    bool stripReadOnly = false;
    int stripState = 0;
    int slide = 0;
    bool loop = false;
    bool autoplay = false;
    int carouselFit = 1;
    int carouselState = 0;
    facet::ImageViewerState view;
    bool minimap = true;
    float split = 0.5f;
    int orientation = 0;
    bool compareReadOnly = false;
    double playhead = 12.0;
    double loopIn = 20.0;
    double loopOut = 50.0;
    float buffered = 40.0f;
    bool loopMarkers = true;
    bool playing = false;
    int drops = 0;
    int zoneActivations = 0;
    // Item ids and rects for the selftest, from the latest frame.
    ImGuiID carouselId = 0;
};

MediaUi ui;

struct Assets
{
    bool built = false;
    bool dark = false;
    facet::ImageHandle wide;
    facet::ImageHandle tall;
    facet::ImageHandle tiny;
    facet::ImageHandle tile;
    facet::ImageHandle viewer;
    facet::ImageHandle muted;      // the second image of the comparison
    facet::ImageHandle async;      // decoded on a worker thread, reloaded by a button
    facet::ImageHandle thumbs[kThumbnails];
    std::vector<unsigned char> png;
};

Assets assets;

bool Is(const facet::demo::PageCtx& ctx, CaptureState state)
{
    return ctx.capture && ctx.captureState == state;
}

// --- Patterns ---------------------------------------------------------------------------

void Put(std::vector<unsigned char>& pixels, int width, int x, int y, const ImVec4& color)
{
    const ImU32 packed = facet::ToU32(color);
    unsigned char* p = &pixels[(static_cast<std::size_t>(y) * width + x) * 4u];
    p[0] = static_cast<unsigned char>(packed >> IM_COL32_R_SHIFT);
    p[1] = static_cast<unsigned char>(packed >> IM_COL32_G_SHIFT);
    p[2] = static_cast<unsigned char>(packed >> IM_COL32_B_SHIFT);
    p[3] = static_cast<unsigned char>(packed >> IM_COL32_A_SHIFT);
}

// A test pattern: a diagonal gradient between two chart colors, a grid, a ring in the middle and
// one colored mark per corner, so cropping, stretching and orientation are easy to see. With
// `transparent`, the background is see-through and only the ring and the marks are drawn, so the
// checkerboard behind it shows.
std::vector<unsigned char> MakePattern(const facet::Colors& c, int width, int height, int hue, bool transparent = false,
                                       bool muted = false)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4u);
    const ImVec4 from = muted ? c.labelTertiary : c.chart[hue % facet::kChartPaletteSize];
    const ImVec4 to = muted ? c.labelSecondary : c.chart[(hue + 3) % facet::kChartPaletteSize];
    const int cell = std::max(4, std::min(width, height) / 9);
    const int corner = std::max(1, std::min(width, height) / 6);
    const float cx = 0.5f * width;
    const float cy = 0.5f * height;
    const float ring = 0.3f * std::min(width, height);
    const float ringHalfWidth = std::max(0.75f, 0.03f * std::min(width, height));
    const ImVec4 marks[] = {c.destructive, c.warning, c.success, c.label};
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float t = (static_cast<float>(x) / std::max(1, width - 1) + static_cast<float>(y) / std::max(1, height - 1)) * 0.5f;
            ImVec4 color = transparent ? ImVec4(0.0f, 0.0f, 0.0f, 0.0f) : facet::Mix(from, to, t);
            if (!transparent && (x % cell == 0 || y % cell == 0))
                color = facet::Over(facet::WithAlpha(c.bg, kGridLineOpacity), color);
            if (std::fabs(std::hypot(x + 0.5f - cx, y + 0.5f - cy) - ring) <= ringHalfWidth)
                color = transparent ? facet::Mix(from, to, t) : c.bg;
            const bool left = x < corner;
            const bool right = x >= width - corner;
            const bool top = y < corner;
            const bool bottom = y >= height - corner;
            if ((left || right) && (top || bottom))
                color = marks[(top ? 0 : 2) + (left ? 0 : 1)];
            Put(pixels, width, x, y, color);
        }
    }
    return pixels;
}

void AppendBytes(void* context, void* data, int size)
{
    auto* out = static_cast<std::vector<unsigned char>*>(context);
    const auto* bytes = static_cast<const unsigned char*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

void ReleaseAll()
{
    for (facet::ImageHandle* handle : {&assets.wide, &assets.tall, &assets.tiny, &assets.tile, &assets.viewer, &assets.muted,
                                       &assets.async})
    {
        facet::ReleaseImage(*handle);
        *handle = {};
    }
    for (facet::ImageHandle& handle : assets.thumbs)
    {
        facet::ReleaseImage(handle);
        handle = {};
    }
}

facet::ImageHandle Create(const std::vector<unsigned char>& pixels, int width, int height)
{
    return facet::CreateImageRGBA(pixels.data(), width, height);
}

void Reload()
{
    facet::ReleaseImage(assets.async);
    assets.async = facet::LoadImageMemoryAsync(assets.png.data(), assets.png.size());
}

// Builds every image for the current theme; again after a theme switch and after a restart of
// Facet (capture and selftest runs start a fresh session per page state).
void EnsureAssets(bool dark)
{
    if (assets.built && assets.dark == dark && facet::GetImageStatus(assets.wide) != facet::ImageStatus::Invalid)
        return;
    ReleaseAll();
    const facet::Colors& c = facet::T().colors;
    assets.wide = Create(MakePattern(c, kWideWidth, kWideHeight, 0), kWideWidth, kWideHeight);
    assets.tall = Create(MakePattern(c, kTallWidth, kTallHeight, 2), kTallWidth, kTallHeight);
    assets.tiny = Create(MakePattern(c, kTinySide, kTinySide, 4), kTinySide, kTinySide);
    assets.tile = Create(MakePattern(c, kTileSide, kTileSide, 1), kTileSide, kTileSide);
    assets.viewer = Create(MakePattern(c, kViewerWidth, kViewerHeight, 5, true), kViewerWidth, kViewerHeight);
    assets.muted = Create(MakePattern(c, kWideWidth, kWideHeight, 0, false, true), kWideWidth, kWideHeight);
    for (int i = 0; i < kThumbnails; ++i)
        assets.thumbs[i] = Create(MakePattern(c, kTileSide * 2, kTileSide * 2, i), kTileSide * 2, kTileSide * 2);
    const std::vector<unsigned char> large = MakePattern(c, kAsyncWidth, kAsyncHeight, 6);
    assets.png.clear();
    stbi_write_png_to_func(&AppendBytes, &assets.png, kAsyncWidth, kAsyncHeight, 4, large.data(), kAsyncWidth * 4);
    Reload();
    assets.built = true;
    assets.dark = dark;
}

// --- Sections -----------------------------------------------------------------------------

// Wraps the next widget of a row: on the same line while it fits, else on a new one.
void Next(float right, float width)
{
    facet::demo::SameLineIfFits(right, width);
}

void ImagesSection(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    SectionHeader("Image view");
    Paragraph("An image in design units. Contain letterboxes, Cover crops, Fill stretches; corners come from the "
              "radius tokens and never let the picture show past them. The boxes below are outlined by the page.");
    LabeledRow("Fit");
    facet::SegmentedControl("##fit", &ui.fit, kFits);
    ctx.Probe("media.fit");
    LabeledRow("Corners");
    facet::SegmentedControl("##corners", &ui.corners, kCorners);
    ctx.Probe("media.corners");
    LabeledRow("Tint");
    facet::Toggle("Accent tint##tint", &ui.tint);
    ctx.Probe("media.tint");

    const float side = DemoWidth(t.spacing.xxxl * kBoxSpans);
    const facet::ImageViewOptions box{.fit = static_cast<facet::ImageFit>(ui.fit), .corners = kCornerTokens[ui.corners],
                                      .tint = ui.tint ? facet::ToU32(t.colors.accent) : IM_COL32_WHITE,
                                      .width = side, .height = side};
    const auto outlined = [&](const char* label, facet::ImageHandle image, const facet::ImageViewOptions& options) {
        facet::ImageView(label, image, options);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        facet::StrokeRect(ImGui::GetWindowDrawList(), min, max, facet::ToU32(t.colors.separator), 0.0f,
                          facet::HairlineThickness());
    };
    LabeledRow("Landscape, portrait, tiny");
    const float right = facet::demo::ContentRight();
    outlined("Placeholder pattern, landscape", assets.wide, box);
    ctx.Probe("media.image.wide");
    Next(right, facet::Px(side));
    outlined("Placeholder pattern, portrait", assets.tall, box);
    Next(right, facet::Px(side));
    outlined("Placeholder pattern, eight pixels", assets.tiny, box);
    LabeledRow("Own size");
    facet::ImageView("Placeholder pattern at its own size", assets.tile);
    ctx.Probe("media.image.own");
    ImGui::SameLine();
    facet::demo::NoteBeside("%d by %d texels, one per pixel at scale 1", kTileSide, kTileSide);

    LabeledRow("Activatable");
    if (facet::ImageView("Placeholder pattern, activatable", assets.wide,
                         {.fit = facet::ImageFit::Cover, .corners = facet::Radius::Card, .activatable = true,
                          .width = side * 1.5f, .height = side}))
        ++ui.imageActivations;
    ctx.Probe("media.image.active");
    ImGui::SameLine();
    facet::demo::NoteBeside("Activated %d times", ui.imageActivations);

    LabeledRow("Loading and failed");
    const float stateRight = facet::demo::ContentRight();
    facet::ImageView("Placeholder pattern, decoded in the background", assets.async,
                     {.fit = facet::ImageFit::Cover, .corners = facet::Radius::Card, .width = side * 1.5f, .height = side});
    Next(stateRight, facet::Px(side * 1.5f));
    facet::ImageView("Image that failed to load", facet::ImageHandle{},
                     {.corners = facet::Radius::Card, .retryLabel = "Retry", .width = side * 1.5f, .height = side});
    {
        // The retry button: bottom middle of the box.
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float h = facet::ControlHeight(facet::ControlSize::Small);
        const float gap = facet::PxSnapped(t.spacing.xs);
        facet::demo::ProbeRect(ctx, "media.image.retry", 0, ImVec2(min.x + (max.x - min.x) * 0.4f, max.y - gap - h),
                               ImVec2(min.x + (max.x - min.x) * 0.6f, max.y - gap));
    }
    if (facet::ImageViewRetryPressed())
    {
        ++ui.retries;
        Reload();
    }
    LabeledRow("");
    if (facet::Button("Load again##reload", {.size = facet::ControlSize::Small}))
        Reload();
    ctx.Probe("media.image.reload");
    ImGui::SameLine();
    facet::demo::NoteBeside("Retry pressed %d times; both buttons decode the left image again", ui.retries);
}

void PeopleSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Avatar");
    Paragraph("A round picture with the initials of its name, or the user icon, while there is no ready image. The "
              "names here are numbered placeholders.");
    const float large = facet::ControlHeight(facet::ControlSize::Large);
    LabeledRow("Sizes and fallbacks", large);
    {
        facet::demo::MixedHeightRow row(large);
        row.Item(facet::ControlHeight(facet::ControlSize::Small));
        facet::Avatar("Placeholder small", facet::ImageHandle{}, {.size = facet::ControlSize::Small});
        row.Item(facet::ControlHeight(facet::ControlSize::Regular));
        facet::Avatar("Placeholder regular", assets.tile);
        row.Item(large);
        facet::Avatar("Placeholder large", facet::ImageHandle{}, {.size = facet::ControlSize::Large});
        row.Item(large);
        facet::Avatar("##no name", facet::ImageHandle{}, {.size = facet::ControlSize::Large});
        row.Item(large);
        facet::Avatar("Placeholder loading", assets.async, {.size = facet::ControlSize::Large});
    }
    LabeledRow("Status and shape", large);
    {
        facet::demo::MixedHeightRow row(large);
        row.Item(large);
        facet::Avatar("Placeholder online", facet::ImageHandle{},
                      {.showStatus = true, .status = facet::Tone::Success, .size = facet::ControlSize::Large});
        row.Item(large);
        facet::Avatar("Placeholder away", assets.tile,
                      {.showStatus = true, .status = facet::Tone::Warning, .size = facet::ControlSize::Large});
        row.Item(large);
        facet::Avatar("Placeholder device", facet::ImageHandle{},
                      {.squircle = true, .showStatus = true, .status = facet::Tone::Neutral, .size = facet::ControlSize::Large});
    }
    LabeledRow("Selectable", large);
    if (Is(ctx, kPeople))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::Avatar("Placeholder, press to select", assets.tile,
                      {.selected = ui.avatarSelected, .activatable = true, .size = facet::ControlSize::Large}))
        ui.avatarSelected = !ui.avatarSelected;
    ctx.Probe("media.avatar.active");
    ImGui::SameLine();
    facet::demo::NoteBeside(ui.avatarSelected ? "Selected" : "Not selected");

    SectionHeader("Avatar group");
    Paragraph("Overlapping avatars with a chip that counts the rest. Hover or focus the group for every name.");
    LabeledRow("Shown at most");
    facet::Slider("##maxVisible", &ui.maxVisible, 0, 7, {.width = DemoWidth(160.0f)});
    ctx.Probe("media.group.max");
    ImGui::SameLine();
    facet::demo::NoteBeside("of 7 members; 0 fits the width");
    LabeledRow("Group");
    if (Is(ctx, kPeople))
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    if (facet::AvatarGroup("Placeholder members", kMembers, {.maxVisible = ui.maxVisible}))
        ++ui.groupActivations;
    ctx.Probe("media.group");
    ImGui::SameLine();
    facet::demo::NoteBeside("Activated %d times", ui.groupActivations);
    LabeledRow("Large, no members", large);
    {
        facet::demo::MixedHeightRow row(large);
        row.Item(large);
        facet::AvatarGroup("Placeholder team", std::span(kMembers, 4), {.size = facet::ControlSize::Large});
        row.Item(facet::ControlHeight(facet::ControlSize::Regular));
        facet::AvatarGroup("Empty group", std::span<const facet::AvatarItem>());
    }
    if (Is(ctx, kPeople))
        ctx.RevealInCapture(0.5f);
}

// Thumbnails from the getter overload: the labels are written into a buffer per entry.
char thumbnailLabel[32];
bool ThumbnailEntry(void*, int index, facet::ImageItem* out)
{
    std::snprintf(thumbnailLabel, sizeof(thumbnailLabel), "Placeholder %d", index + 1);
    out->label = thumbnailLabel;
    out->image = assets.thumbs[index % kThumbnails];
    return true;
}

void CollectionsSection(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    SectionHeader("Thumbnail strip");
    Paragraph("A scrolling row with the chosen thumbnail ringed. Left and Right choose, Home and End jump; the wheel "
              "and a drag scroll, and a released drag glides on.");
    LabeledRow("Data");
    facet::SegmentedControl("##stripState", &ui.stripState, kDataStates);
    ctx.Probe("media.strip.state");
    LabeledRow("Options");
    facet::Toggle("Captions##captions", &ui.captions);
    ImGui::SameLine();
    facet::Toggle("Read-only##stripReadOnly", &ui.stripReadOnly);
    LabeledRow("Strip");
    const int count = ui.stripState == 3 ? 0 : kThumbnails;
    const facet::DataState state = ui.stripState == 1 ? facet::DataState::Loading
                                   : ui.stripState == 2 ? facet::DataState::Error
                                                        : facet::DataState::Ready;
    if (Is(ctx, kCollections))
        ctx.ForceNextItemState(ForcedItemState::Hovered, 2);
    if (Is(ctx, kStates))
        facet::ThumbnailStrip("Placeholder thumbnails, loading", &ui.thumbnail, kThumbnails, &ThumbnailEntry, nullptr,
                              {.thumbnailHeight = t.spacing.xxxl * 2.0f, .state = facet::DataState::Loading});
    else
        facet::ThumbnailStrip("Placeholder thumbnails", &ui.thumbnail, count, &ThumbnailEntry, nullptr,
                              {.thumbnailHeight = t.spacing.xxxl * 2.0f, .captions = ui.captions, .state = state,
                               .readOnly = ui.stripReadOnly});
    ctx.Probe("media.strip");
    if (Is(ctx, kStates))
        ctx.RevealInCapture(0.2f);  // the loading strip and, under it, the failed carousel
    {
        // Thumbnail 2 (index 1) for the selftest, inside the strip at every size the selftest runs.
        const ImVec2 min = ImGui::GetItemRectMin();
        const float inset = std::ceil(facet::internal::FocusRingOutset());
        const float thumb = facet::PxSnapped(t.spacing.xxxl * 2.0f);
        const float stride = thumb + facet::PxSnapped(t.spacing.sm);
        const ImVec2 at(min.x + inset + stride, min.y + inset);
        facet::demo::ProbeRect(ctx, "media.strip.second", 0, at, ImVec2(at.x + thumb, at.y + thumb));
    }
    Note("Chosen: %d", ui.thumbnail + 1);
    LabeledRow("Error and empty");
    facet::ThumbnailStrip("Placeholder thumbnails, failed", &ui.thumbnail, 0, &ThumbnailEntry, nullptr,
                          {.thumbnailHeight = t.spacing.xxxl, .state = facet::DataState::Error});
    LabeledRow("");
    facet::ThumbnailStrip("Placeholder thumbnails, none", &ui.thumbnail, 0, &ThumbnailEntry, nullptr,
                          {.thumbnailHeight = t.spacing.xxxl, .emptyText = "No placeholder images"});

    SectionHeader("Carousel");
    Paragraph("One slide at a time. The buttons and the arrow keys page, the dots jump, a drag follows the pointer "
              "and pages by distance or by a flick. Autoplay pauses while the pointer or the keyboard focus is on it, "
              "has a pause button, and is off while motion is reduced.");
    LabeledRow("Data");
    facet::SegmentedControl("##carouselState", &ui.carouselState, kDataStates);
    LabeledRow("Fit");
    facet::SegmentedControl("##carouselFit", &ui.carouselFit, kFits);
    LabeledRow("Options");
    facet::Toggle("Loop##loop", &ui.loop);
    ctx.Probe("media.carousel.loop");
    ImGui::SameLine();
    facet::Toggle("Autoplay##autoplay", &ui.autoplay);
    LabeledRow("Carousel");
    facet::ImageItem slides[kSlides];
    static const char* const kSlideLabels[kSlides] = {"Placeholder slide 1", "Placeholder slide 2", "Placeholder slide 3",
                                                       "Placeholder slide 4"};
    const facet::ImageHandle slideImages[kSlides] = {assets.wide, assets.tall, assets.thumbs[3], assets.muted};
    for (int i = 0; i < kSlides; ++i)
        slides[i] = {kSlideLabels[i], slideImages[i]};
    const int slideCount = ui.carouselState == 3 ? 0 : kSlides;
    const facet::DataState carouselState = ui.carouselState == 1 ? facet::DataState::Loading
                                           : ui.carouselState == 2 ? facet::DataState::Error
                                                                   : facet::DataState::Ready;
    if (Is(ctx, kCollections))
        ctx.ForceNextItemState(ForcedItemState::Focused | ForcedItemState::Dragging, 0);
    const float carouselWidth = DemoWidth(t.spacing.xxxl * kCarouselSpans);
    facet::Carousel("Placeholder slides", &ui.slide, std::span(slides, static_cast<std::size_t>(slideCount)),
                    {.loop = ui.loop, .captions = true, .autoplay = ui.autoplay ? kAutoplaySeconds : 0.0f,
                     .fit = static_cast<facet::ImageFit>(ui.carouselFit),
                     .state = Is(ctx, kStates) ? facet::DataState::Error : carouselState, .width = carouselWidth});
    ctx.Probe("media.carousel");
    ui.carouselId = ImGui::GetItemID();
    {
        // The slide area (the item less the row of dots) for the selftest.
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float row = facet::ControlHeight(facet::ControlSize::Small) + facet::PxSnapped(t.spacing.xs);
        facet::demo::ProbeRect(ctx, "media.carousel.slide", ui.carouselId, min, ImVec2(max.x, max.y - row));
    }
    if (Is(ctx, kCollections))
        ctx.RevealInCapture(0.6f);
    Note("Slide %d of %d", ui.slide + 1, kSlides);
}

void ViewerSection(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    SectionHeader("Image viewer");
    Paragraph("The wheel zooms around the pointer, a drag pans, a double click toggles between fit and 100 percent. "
              "With the focus on it, + and - zoom, the arrows pan, 0 fits and 1 shows 100 percent. The pattern has a "
              "see-through background, so the checkerboard shows.");
    LabeledRow("Options");
    facet::Toggle("Minimap##minimap", &ui.minimap);
    ImGui::SameLine();
    if (facet::Button("Fit##fit", {.size = facet::ControlSize::Small}))
        ui.view = {};
    ctx.Probe("media.viewer.fit");
    ImGui::SameLine();
    if (facet::Button("100 percent##actual", {.size = facet::ControlSize::Small}))
        ui.view = {.zoom = 1.0f, .center = ImVec2(kViewerWidth * 0.5f, kViewerHeight * 0.5f)};
    ctx.Probe("media.viewer.actual");
    LabeledRow("Viewer");
    if (Is(ctx, kViewer))
    {
        ui.view = {.zoom = 2.0f, .center = ImVec2(kViewerWidth * 0.3f, kViewerHeight * 0.4f)};
        ctx.ForceNextItemState(ForcedItemState::Focused);
    }
    facet::ImageViewer("Placeholder pattern with a see-through background", assets.viewer, &ui.view,
                       {.minimap = ui.minimap, .height = t.spacing.xxxl * kViewerSpans});
    ctx.Probe("media.viewer");
    if (ui.view.zoom > 0.0f)
        Note("Zoom: %.0f percent", ui.view.zoom * 100.0f);
    else
        Note("Zoom: fit");

    SectionHeader("Image compare");
    Paragraph("Two images over each other with a divider: drag it or press anywhere to move it there; with the focus "
              "on it the arrows move it (Shift finer, Ctrl coarser), Home and End go to the ends.");
    LabeledRow("Orientation");
    facet::SegmentedControl("##orientation", &ui.orientation, kOrientations);
    LabeledRow("Options");
    facet::Toggle("Read-only##compareReadOnly", &ui.compareReadOnly);
    LabeledRow("Compare");
    if (Is(ctx, kViewer))
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Dragging, 0);
    facet::ImageCompare("Placeholder pattern in color and in grey", &ui.split, assets.wide, assets.muted,
                        {.orientation = ui.orientation == 0 ? facet::Orientation::Horizontal : facet::Orientation::Vertical,
                         .beforeLabel = "Color", .afterLabel = "Grey", .width = DemoWidth(t.spacing.xxxl * kCarouselSpans),
                         .readOnly = ui.compareReadOnly});
    ctx.Probe("media.compare");
    if (Is(ctx, kViewer))
        ctx.RevealInCapture(0.5f);
    Note("Divider at %.0f percent", ui.split * 100.0f);
}

void TimelineSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Timeline scrubber");
    Paragraph("A placeholder clip of 90 seconds. Press or drag the ruler to move the playhead; the playhead and the "
              "loop markers are Tab stops that the arrows step and Home and End send to their ends. The wheel zooms "
              "around the pointer, Shift+wheel pans.");
    LabeledRow("Loaded");
    facet::Slider("##buffered", &ui.buffered, 0.0f, static_cast<float>(kTimelineSeconds), {.format = "%.0f s", .width = DemoWidth(200.0f)});
    LabeledRow("Options");
    facet::Toggle("Loop markers##loopMarkers", &ui.loopMarkers);
    ImGui::SameLine();
    if (facet::Button(ui.playing ? "Pause##play" : "Play##play", {.size = facet::ControlSize::Small}))
        ui.playing = !ui.playing;
    ctx.Probe("media.timeline.play");
    if (ui.playing && !ctx.capture)
    {
        ui.playhead += ImGui::GetIO().DeltaTime * kPlaybackRate;
        if (ui.loopMarkers && ui.playhead >= ui.loopOut)
            ui.playhead = ui.loopIn;
        else if (ui.playhead >= kTimelineSeconds)
            ui.playhead = 0.0;
    }
    LabeledRow("Timeline");
    if (Is(ctx, kTimeline))
        ctx.ForceNextItemState(ForcedItemState::Focused, 0);
    const facet::TimelineScrubberOptions options{.buffered = ui.buffered};
    if (ui.loopMarkers)
        facet::TimelineScrubber("Placeholder clip", &ui.playhead, &ui.loopIn, &ui.loopOut, kTimelineSeconds, options);
    else
        facet::TimelineScrubber("Placeholder clip", &ui.playhead, kTimelineSeconds, options);
    ctx.Probe("media.timeline");
    Note("Playhead %.1f s, loop %.1f s to %.1f s", ui.playhead, ui.loopIn, ui.loopOut);
    LabeledRow("Read-only");
    double fixed = 30.0;
    facet::TimelineScrubber("Placeholder clip, read-only", &fixed, kTimelineSeconds, {.readOnly = true});
}

void DropSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Drop zone");
    Paragraph("Drag the placeholder file onto the zones. The first takes it; the second accepts another type, so it "
              "stays unmarked and the pointer shows that it refuses. A click, Enter or Space activates a zone, as an "
              "app would open its file dialog. Files from the operating system arrive only when the host passes them "
              "in as a payload.");
    LabeledRow("Source");
    facet::Button("Placeholder file##source", {.icon = facet::Icon("file")});
    ctx.Probe("media.drop.source");
    {
        const int payload = 1;
        if (facet::internal::DragDropBeginSource(kAccepted[0], &payload, sizeof(payload),
                                                 {.previewText = "Placeholder file", .icon = facet::Icon("file")}))
            facet::internal::DragDropEndSource();
    }
    LabeledRow("Accepts it");
    if (Is(ctx, kTimeline))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    const facet::DropZoneAction action = facet::DropZone("Placeholder files", kAccepted,
                                                         {.hint = "Drop the placeholder file here", .icon = facet::Icon("upload")});
    ctx.Probe("media.drop");
    if (action == facet::DropZoneAction::Dropped && facet::DropZonePayload() != nullptr)
        ++ui.drops;
    if (action == facet::DropZoneAction::Activated)
        ++ui.zoneActivations;
    Note("Dropped %d times, activated %d times", ui.drops, ui.zoneActivations);
    LabeledRow("Refuses it");
    facet::DropZone("Accepts another type", kRefusing, {.height = facet::T().spacing.xxxl * 2.0f});
    if (Is(ctx, kTimeline))
        ctx.RevealInCapture(0.45f);
}

} // namespace

FACET_DEMO_PAGE("media", "Media", "Widgets", 130, "default", "people", "collections", "viewer", "timeline", "states",
                "disabled")
{
    EnsureAssets(ctx.dark);
    facet::demo::PageHeader("Media",
                            "Images in design units, avatars, thumbnail strips, carousels, image viewers, before "
                            "and after comparisons, timeline scrubbers and drop zones. The pixel-sized core items "
                            "Image and IconGlyph are shown on the Images and Icons pages.");
    facet::demo::ReadyFamilyNote("media");

    const bool disabled = Is(ctx, kDisabled);
    if (disabled)
        ImGui::BeginDisabled();
    ImagesSection(ctx);
    PeopleSection(ctx);
    CollectionsSection(ctx);
    ViewerSection(ctx);
    TimelineSection(ctx);
    DropSection(ctx);
    if (disabled)
        ImGui::EndDisabled();
}

FACET_DEMO_SELFTEST("media")
{
    using T = facet::demo::Target;
    test.Do([] {
        ui = {};
    });

    // Image view: an activatable image by click and keys; the retry button of the failed one.
    test.Reveal("media.image.active");
    test.WaitSeconds(0.4f);
    test.Click("media.image.active");
    test.Expect("a click activates the image", [] { return ui.imageActivations == 1; });
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus
    test.ExpectFocused("media.image.active");
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Enter and Space activate the focused image", [] { return ui.imageActivations == 3; });
    test.Click("media.image.retry");
    test.Expect("the retry button of the failed image is reported", [] { return ui.retries == 1; });
    test.MouseMove(T::Item("media.fit", 0.1f, 0.5f));  // the first segment
    test.MouseDown();
    test.MouseUp();
    test.Expect("the fit control switches the boxes to Contain", [] { return ui.fit == 0; });

    // Avatar: a selectable avatar by click and keys.
    test.Reveal("media.avatar.active");
    test.WaitSeconds(0.4f);
    test.Click("media.avatar.active");
    test.Expect("a click selects the avatar", [] { return ui.avatarSelected; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space clears the selection again", [] { return !ui.avatarSelected; });

    // Avatar group: activation by click and keys; the slider shows more avatars.
    test.Reveal("media.group");
    test.WaitSeconds(0.4f);
    test.Click("media.group");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the group activates by click and Enter", [] { return ui.groupActivations == 2; });
    test.Click("media.group.max");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_End);
    test.Expect("End shows every member", [] { return ui.maxVisible == 7; });

    // Thumbnail strip: click, arrows, Home and End; a drag scrolls without choosing.
    test.Reveal("media.strip");
    test.WaitSeconds(0.4f);
    test.Click("media.strip.second");
    test.Expect("a click chooses the second thumbnail", [] { return ui.thumbnail == 1; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right chooses the next one", [] { return ui.thumbnail == 2; });
    test.KeyPress(ImGuiKey_End);
    test.Expect("End chooses the last one", [] { return ui.thumbnail == kThumbnails - 1; });
    test.KeyPress(ImGuiKey_Home);
    test.Expect("Home chooses the first one", [] { return ui.thumbnail == 0; });
    test.Drag({T::Item("media.strip", 0.6f, 0.4f), T::Item("media.strip", 0.2f, 0.4f)}, 0.3f);
    test.WaitSeconds(0.5f);
    test.Expect("a drag scrolls without choosing", [] { return ui.thumbnail == 0; });
    test.Wheel(T::Item("media.strip", 0.5f, 0.4f), -1.0f);
    test.Wheel(T::Item("media.strip", 0.5f, 0.4f), -1.0f);
    test.Expect("the wheel scrolls without choosing", [] { return ui.thumbnail == 0; });

    // Carousel: the next button, the arrows and a flick.
    test.Reveal("media.carousel");
    test.WaitSeconds(0.4f);
    test.Click("media.carousel.loop");
    test.Expect("loop is on", [] { return ui.loop; });
    test.MouseMove(T::Item("media.carousel.slide", 1.0f, 0.5f).Offset(-22.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("the next button pages", [] { return ui.slide == 1; });
    test.Click("media.carousel.slide");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right pages", [] { return ui.slide == 2; });
    test.KeyPress(ImGuiKey_End);
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right after the last slide wraps when looping", [] { return ui.slide == 0; });
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("Left before the first slide wraps back", [] { return ui.slide == kSlides - 1; });
    test.Drag({T::Item("media.carousel.slide", 0.5f, 0.3f), T::Item("media.carousel.slide", 0.2f, 0.3f)}, 0.05f);
    test.WaitSeconds(0.5f);
    test.Expect("a flick to the left pages forward", [] { return ui.slide == 0; });
    test.Wheel(T::Item("media.carousel.slide", 0.5f, 0.5f), 0.0f, -1.0f);
    test.Wheel(T::Item("media.carousel.slide", 0.5f, 0.5f), 0.0f, -1.0f);
    test.Expect("a sideways wheel pages", [] { return ui.slide != 0; });

    // Image viewer: the wheel zooms, the keys fit and show 100 percent, the page buttons set the view.
    test.Reveal("media.viewer");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("media.viewer", 0.5f, 0.5f));
    test.Wheel(T::Item("media.viewer", 0.6f, 0.5f), 1.0f);
    test.Wheel(T::Item("media.viewer", 0.6f, 0.5f), 1.0f);
    test.Expect("the wheel zooms in", [] { return ui.view.zoom > 0.0f; });
    test.Click("media.viewer");
    test.KeyPress(ImGuiKey_0);
    test.Expect("0 fits the image", [] { return ui.view.zoom == 0.0f; });
    test.KeyPress(ImGuiKey_1);
    test.ExpectNear("1 shows 100 percent", [] { return ui.view.zoom; }, 1.0, 0.001);
    test.KeyPress(ImGuiKey_Equal);
    test.ExpectNear("+ zooms in", [] { return ui.view.zoom; }, 1.25, 0.001);
    test.Click("media.viewer.fit");
    test.Expect("the Fit button resets the view", [] { return ui.view.zoom == 0.0f; });

    // Image compare: a press moves the divider, the keys step it.
    test.Reveal("media.compare");
    test.WaitSeconds(0.4f);
    test.Click("media.compare");
    test.MouseMove(T::Item("media.compare", 0.25f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a press moves the divider there", [] { return ui.split; }, 0.25, 0.03);
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home moves it to the start", [] { return ui.split; }, 0.0, 0.0001);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps it", [] { return ui.split; }, 0.01, 0.002);

    // Timeline: a press on the ruler, the keys on the playhead and on a loop marker.
    test.Reveal("media.timeline");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("media.timeline", 0.5f, 0.15f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a press on the ruler moves the playhead", [] { return ui.playhead; }, 45.0, 3.0);
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus on the playhead
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home sends the playhead to the start", [] { return ui.playhead; }, 0.0, 0.0001);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps a hundredth", [] { return ui.playhead; }, 0.9, 0.0001);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End stops the loop start at the loop end", [] { return ui.loopIn; }, 50.0, 0.0001);
    test.Click("media.timeline.play");
    test.WaitSeconds(0.5f);
    test.Click("media.timeline.play");
    test.Expect("playing moves the playhead", [] { return ui.playhead > 0.9; });

    // Drop zone: a drag of the placeholder file delivers it; click and keys activate.
    test.Reveal("media.drop");
    test.WaitSeconds(0.4f);
    test.DragAndHold({T::Item("media.drop.source"), T::Item("media.drop")}, 0.3f, 0.2f);
    test.Expect("the zone takes the dropped file", [] { return ui.drops == 1; });
    test.Click("media.drop");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("a click and Space activate the zone", [] { return ui.zoneActivations == 2; });
}
