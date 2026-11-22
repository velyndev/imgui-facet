// Images page: fit modes, corner radii, tint, asynchronous loading with its loading and
// failed states, and SVG rasterized per scale. Every picture is a test pattern generated
// here from the theme colors, or an SVG document written by this page; none is a photo.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "pages/pages.h"

// The encoder is compiled with internal linkage in this file as well, like in capture.cpp.
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

using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Test pattern sizes in texels. They are pixel data, not layout lengths.
constexpr int kCardWidth = 320;
constexpr int kCardHeight = 180;
constexpr int kAsyncWidth = 1280;  // large enough that decoding takes a visible moment
constexpr int kAsyncHeight = 720;
constexpr int kNeutralSide = 128;
constexpr int kCardGridCell = 20;
// Opacity of the background color laid over the test card's grid lines.
constexpr float kGridLineOpacity = 0.35f;
// Grey levels of the neutral tint pattern, as fractions of white: the two check shades and
// how much the soft spot brightens them. The pattern is fixed pixel data and deliberately
// does not follow the theme, because a tint is only judged fairly against the same grey
// in both themes.
constexpr float kNeutralDark = 0.78f;
constexpr float kNeutralLight = 0.9f;
constexpr float kNeutralSpot = 0.25f;
// Bytes of the encoded PNG kept for the damaged-data example: the signature and part of
// the header, but no image data.
constexpr std::size_t kTruncatedBytes = 64;
// Example boxes are this many xxxl spacing units wide.
constexpr float kTileSpans = 4.0f;
constexpr float kSmallTileSpans = 2.0f;
// Wide boxes have the aspect ratio of the test card.
constexpr float kWideAspect = static_cast<float>(kCardWidth) / kCardHeight;
// SVG design sizes for the three-scale row, as multiples of the document's own size.
constexpr float kSvgScales[] = {1.0f, 2.0f, 3.0f};
constexpr float kSvgDesignSide = 48.0f;  // the document's own width and height
constexpr const char* kMissingPath = "missing/facet-demo-image-that-does-not-exist.png";

struct ImagesUi
{
    facet::ImageFit fit = facet::ImageFit::Cover;
    int radius = 2;  // index into the radius choices below
    bool tint = false;
};

ImagesUi ui;

struct Assets
{
    bool built = false;
    bool dark = false;
    facet::ImageHandle card;
    facet::ImageHandle neutral;
    facet::ImageHandle async;
    facet::ImageHandle missing;
    facet::ImageHandle damaged;
    facet::ImageHandle svgScaled[std::size(kSvgScales)];
    std::vector<unsigned char> png;  // the async pattern, encoded
};

Assets assets;

// What the last frame drew, for the selftest.
struct Drawn
{
    bool offGrid = false;  // an example box was not a whole number of pixels
    int failedBoxes = 0;   // failed examples, which draw their own outline
};

Drawn drawn;

// --- Pattern generation ---------------------------------------------------------------

void Put(std::vector<unsigned char>& pixels, int width, int x, int y, const ImVec4& color)
{
    const ImU32 packed = facet::ToU32(color);
    unsigned char* p = &pixels[(static_cast<std::size_t>(y) * width + x) * 4u];
    p[0] = static_cast<unsigned char>(packed >> IM_COL32_R_SHIFT);
    p[1] = static_cast<unsigned char>(packed >> IM_COL32_G_SHIFT);
    p[2] = static_cast<unsigned char>(packed >> IM_COL32_B_SHIFT);
    p[3] = static_cast<unsigned char>(packed >> IM_COL32_A_SHIFT);
}

// A test card: a horizontal gradient between two theme colors, a grid, a ring in the
// middle and one colored square per corner, so cropping and stretching are easy to see.
std::vector<unsigned char> MakeTestCard(const facet::Colors& c, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4u);
    std::vector<ImVec4> column(static_cast<std::size_t>(width));
    for (int x = 0; x < width; ++x)
        column[static_cast<std::size_t>(x)] = facet::Mix(c.accent, c.success, static_cast<float>(x) / (width - 1));

    const int cell = std::max(4, kCardGridCell * height / kCardHeight);
    const int corner = std::min(width, height) / 6;
    const float cx = 0.5f * width;
    const float cy = 0.5f * height;
    const float ring = 0.3f * std::min(width, height);
    const float ringHalfWidth = std::max(1.0f, 0.012f * std::min(width, height));
    const ImVec4 cornerColors[] = {c.destructive, c.warning, c.bg, c.label};

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            ImVec4 color = column[static_cast<std::size_t>(x)];
            if (x % cell == 0 || y % cell == 0)
                color = facet::Over(facet::WithAlpha(c.bg, kGridLineOpacity), color);
            const float distance = std::hypot(x + 0.5f - cx, y + 0.5f - cy);
            if (std::fabs(distance - ring) <= ringHalfWidth)
                color = c.bg;
            const bool left = x < corner;
            const bool right = x >= width - corner;
            const bool top = y < corner;
            const bool bottom = y >= height - corner;
            if ((left || right) && (top || bottom))
                color = cornerColors[(top ? 0 : 2) + (left ? 0 : 1)];
            Put(pixels, width, x, y, color);
        }
    }
    return pixels;
}

// A light grey pattern (checks and a soft spot) that shows a tint color faithfully.
std::vector<unsigned char> MakeNeutral(int side)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(side) * side * 4u);
    const int check = side / 8;
    for (int y = 0; y < side; ++y)
    {
        for (int x = 0; x < side; ++x)
        {
            const float dx = (x + 0.5f) / side - 0.5f;
            const float dy = (y + 0.5f) / side - 0.5f;
            const float spot = std::max(0.0f, 1.0f - 2.0f * std::sqrt(dx * dx + dy * dy));
            const float base = ((x / check + y / check) % 2 == 0) ? kNeutralDark : kNeutralLight;
            const float value = std::min(1.0f, base + kNeutralSpot * spot);
            Put(pixels, side, x, y, ImVec4(value, value, value, 1.0f));
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

std::string Hex(const ImVec4& color)
{
    const ImU32 packed = facet::ToU32(color);
    char text[8];
    std::snprintf(text, sizeof(text), "#%02X%02X%02X", static_cast<unsigned>((packed >> IM_COL32_R_SHIFT) & 0xFF),
                  static_cast<unsigned>((packed >> IM_COL32_G_SHIFT) & 0xFF),
                  static_cast<unsigned>((packed >> IM_COL32_B_SHIFT) & 0xFF));
    return text;
}

// A full-color SVG built from theme colors: a gradient tile with a circle, a square and a
// stroked check, so gradients, fills and round strokes can be compared across scales.
std::string MakeSvg(const facet::Colors& c)
{
    std::string svg;
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"48\" height=\"48\" viewBox=\"0 0 48 48\">";
    svg += "<defs><linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">";
    svg += "<stop offset=\"0\" stop-color=\"" + Hex(c.accent) + "\"/>";
    svg += "<stop offset=\"1\" stop-color=\"" + Hex(c.success) + "\"/>";
    svg += "</linearGradient></defs>";
    svg += "<rect x=\"2\" y=\"2\" width=\"44\" height=\"44\" rx=\"10\" fill=\"url(#g)\"/>";
    svg += "<circle cx=\"16\" cy=\"16\" r=\"7\" fill=\"" + Hex(c.warning) + "\"/>";
    svg += "<rect x=\"26\" y=\"26\" width=\"14\" height=\"14\" rx=\"3\" fill=\"" + Hex(c.destructive) + "\"/>";
    svg += "<path d=\"M9 33l5 5 9-11\" fill=\"none\" stroke=\"" + Hex(c.onAccent) +
           "\" stroke-width=\"3\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
    svg += "</svg>";
    return svg;
}

// --- Asset lifetime ---------------------------------------------------------------------

void ReleaseAll()
{
    for (facet::ImageHandle* handle : {&assets.card, &assets.neutral, &assets.async, &assets.missing,
                                       &assets.damaged})
    {
        facet::ReleaseImage(*handle);
        *handle = {};
    }
    for (facet::ImageHandle& handle : assets.svgScaled)
    {
        facet::ReleaseImage(handle);
        handle = {};
    }
}

void StartAsyncLoads()
{
    facet::ReleaseImage(assets.async);
    facet::ReleaseImage(assets.missing);
    facet::ReleaseImage(assets.damaged);
    assets.async = facet::LoadImageMemoryAsync(assets.png.data(), assets.png.size());
    assets.missing = facet::LoadImageFileAsync(kMissingPath);
    assets.damaged = facet::LoadImageMemoryAsync(assets.png.data(), std::min(assets.png.size(), kTruncatedBytes));
}

// Builds every image for the current theme. Runs again after a theme switch and after
// Facet was restarted (capture and selftest runs start a fresh session per page state).
void EnsureAssets(bool dark)
{
    if (assets.built && assets.dark == dark && facet::GetImageStatus(assets.card) != facet::ImageStatus::Invalid)
        return;
    ReleaseAll();
    const facet::Colors& c = facet::T().colors;

    const std::vector<unsigned char> card = MakeTestCard(c, kCardWidth, kCardHeight);
    assets.card = facet::CreateImageRGBA(card.data(), kCardWidth, kCardHeight);
    const std::vector<unsigned char> neutral = MakeNeutral(kNeutralSide);
    assets.neutral = facet::CreateImageRGBA(neutral.data(), kNeutralSide, kNeutralSide);

    const std::vector<unsigned char> large = MakeTestCard(c, kAsyncWidth, kAsyncHeight);
    assets.png.clear();
    stbi_write_png_to_func(&AppendBytes, &assets.png, kAsyncWidth, kAsyncHeight, 4, large.data(), kAsyncWidth * 4);
    StartAsyncLoads();

    const std::string svg = MakeSvg(c);
    for (std::size_t i = 0; i < std::size(kSvgScales); ++i)
    {
        const float side = kSvgDesignSide * kSvgScales[i];
        assets.svgScaled[i] = facet::LoadSvgMemory(svg.c_str(), svg.size(), ImVec2(side, side));
    }
    assets.built = true;
    assets.dark = dark;
}

// --- Drawing helpers ------------------------------------------------------------------

const char* StatusName(facet::ImageStatus status)
{
    switch (status)
    {
    case facet::ImageStatus::Invalid:
        return "invalid";
    case facet::ImageStatus::Loading:
        return "loading";
    case facet::ImageStatus::Ready:
        return "ready";
    case facet::ImageStatus::Failed:
        return "failed";
    case facet::ImageStatus::Lost:
        return "lost";
    }
    return "unknown";
}

void Caption(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    facet::PushTextStyle(facet::TextStyle::Footnote);
    ImGui::TextUnformatted(text);
    facet::PopTextStyle();
    ImGui::PopStyleColor();
}

// One example: the image in a box with a hairline outline that shows the box bounds, and
// a caption under it, laid out as a group so examples flow side by side. A failed image
// draws its own outline, so the box gets no second one.
void Example(facet::ImageHandle image, ImVec2 size, const facet::ImageOptions& options, const char* caption)
{
    ImGui::BeginGroup();
    facet::Image(image, size, options);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec2 boxSize(max.x - min.x, max.y - min.y);
    if (boxSize.x != std::floor(boxSize.x) || boxSize.y != std::floor(boxSize.y))
        drawn.offGrid = true;
    if (!facet::ImageFailed(image))
        facet::StrokeRect(ImGui::GetWindowDrawList(), min, max, facet::ToU32(facet::T().colors.separator),
                          options.radius, facet::HairlineThickness());
    else
        ++drawn.failedBoxes;
    Caption(caption);
    ImGui::EndGroup();
}

// Right edge of the page content in screen pixels, recorded at the top of the page.
float contentRight = 0.0f;

// Starts the next example on the same line when it fits, otherwise on a new line.
void Flow(float nextWidth)
{
    // Whole pixels, so every box and its outline start on the pixel grid.
    const float spacing = std::round(facet::Px(facet::T().spacing.lg));
    if (ImGui::GetItemRectMax().x + spacing + nextWidth <= contentRight)
        ImGui::SameLine(0.0f, spacing);
}

struct RadiusChoice
{
    const char* name;
    float facet::Radii::*token;
};

constexpr RadiusChoice kRadii[] = {
    {"element", &facet::Radii::element},
    {"control", &facet::Radii::control},
    {"card", &facet::Radii::card},
    {"window", &facet::Radii::window},
};

// One setting as a labeled row with a segmented control, probed as `probe`.
void ChoiceRow(facet::demo::PageCtx& ctx, const char* label, const char* probe, std::span<const char* const> names,
               int& selected)
{
    facet::demo::LabeledRow(label);
    ImGui::PushID(probe);
    facet::SegmentedControl("##choice", &selected, names);
    ImGui::PopID();
    ctx.Probe(probe);
}

} // namespace

// Capture states, in the order passed to FACET_DEMO_PAGE below. "default" shows the top of
// the page, "loading" scrolls to the asynchronous examples while they are pending, and
// "results" scrolls to the end with the finished loads (ready and failed) and the SVGs.
constexpr int kStateLoading = 1;
constexpr int kStateResults = 2;

FACET_DEMO_PAGE("images", "Images", "Media", 20, "default", "loading", "results")
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    EnsureAssets(ctx.dark);
    drawn = Drawn{};
    if (ctx.capture)
    {
        ui = ImagesUi{};
        if (ctx.captureState == kStateLoading)
            StartAsyncLoads();  // requested again every frame, so the capture shows them pending
        else
            for (facet::ImageHandle handle : {assets.async, assets.missing, assets.damaged})
                facet::WaitForImage(handle, 10.0f);
    }

    const float tile = std::floor(facet::Px(theme.spacing.xxxl) * kTileSpans);
    contentRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

    facet::demo::PageHeader("Images", "Every picture on this page is a generated test pattern (made with "
                                      "CreateImageRGBA from the theme colors) or an SVG document written for this "
                                      "page. None is a photo.");

    SectionHeader("Fit modes");
    Paragraph("A 16:9 test card in square boxes. The outline marks the box.");
    const char* fitNames[] = {"Contain", "Cover", "Fill"};
    for (int i = 0; i < 3; ++i)
    {
        facet::ImageOptions options;
        options.fit = static_cast<facet::ImageFit>(i);
        if (i > 0)
            Flow(tile);
        Example(assets.card, ImVec2(tile, tile), options, fitNames[i]);
    }

    SectionHeader("Options");
    {
        const char* const names[] = {"Contain", "Cover", "Fill"};
        int fit = static_cast<int>(ui.fit);
        ChoiceRow(ctx, "Fit", "images.fit", names, fit);
        ui.fit = static_cast<facet::ImageFit>(fit);
    }
    {
        const char* names[std::size(kRadii)] = {};
        for (std::size_t i = 0; i < std::size(kRadii); ++i)
            names[i] = kRadii[i].name;
        ChoiceRow(ctx, "Radius", "images.radius", names, ui.radius);
    }
    facet::demo::LabeledRow("Tint");
    facet::Checkbox("Accent tint", &ui.tint);
    ctx.Probe("images.tint");
    {
        facet::ImageOptions options;
        options.fit = ui.fit;
        options.radius = facet::Px(theme.radii.*kRadii[ui.radius].token);
        options.tint = ui.tint ? facet::ToU32(c.accent) : IM_COL32_WHITE;
        const float width = std::floor(std::min(ImGui::GetContentRegionAvail().x, tile * kWideAspect));
        Example(assets.card, ImVec2(width, tile), options, "Preview with the options above");
    }

    SectionHeader("Corner radii");
    for (int i = 0; i < static_cast<int>(std::size(kRadii)); ++i)
    {
        facet::ImageOptions options;
        options.fit = facet::ImageFit::Cover;
        options.radius = facet::Px(theme.radii.*kRadii[i].token);
        char caption[48];
        std::snprintf(caption, sizeof(caption), "%s %.0f", kRadii[i].name,
                      static_cast<double>(theme.radii.*kRadii[i].token));
        if (i > 0)
            Flow(tile);
        Example(assets.card, ImVec2(tile, tile), options, caption);
    }

    SectionHeader("Tint");
    Paragraph("A grey pattern multiplied by theme colors.");
    struct TintSample
    {
        const char* name;
        ImVec4 color;
        bool none;
    };
    const TintSample tints[] = {
        {"none", c.label, true},     {"accent", c.accent, false},           {"success", c.success, false},
        {"warning", c.warning, false}, {"destructive", c.destructive, false},
    };
    const float small = std::floor(facet::Px(theme.spacing.xxxl) * kSmallTileSpans);
    for (std::size_t i = 0; i < std::size(tints); ++i)
    {
        facet::ImageOptions options;
        options.radius = facet::Px(theme.radii.control);
        options.tint = tints[i].none ? IM_COL32_WHITE : facet::ToU32(tints[i].color);
        if (i > 0)
            Flow(small);
        Example(assets.neutral, ImVec2(small, small), options, tints[i].name);
    }

    SectionHeader("Asynchronous loading");
    Paragraph("Decoded on a worker thread and uploaded at the start of a later frame. A skeleton with a "
              "picture glyph shows while decoding runs; a failed load shows an icon and a label.");
    {
        facet::ImageOptions options;
        options.fit = facet::ImageFit::Cover;
        options.radius = facet::Px(theme.radii.card);
        char caption[96];
        std::snprintf(caption, sizeof(caption), "Generated PNG, %d x %d: %s", kAsyncWidth, kAsyncHeight,
                      StatusName(facet::GetImageStatus(assets.async)));
        const float wide = std::floor(std::min(ImGui::GetContentRegionAvail().x, tile * kWideAspect));
        Example(assets.async, ImVec2(wide, tile), options, caption);
        Flow(tile);
        std::snprintf(caption, sizeof(caption), "Missing file: %s",
                      StatusName(facet::GetImageStatus(assets.missing)));
        Example(assets.missing, ImVec2(tile, tile), options, caption);
        Flow(tile);
        std::snprintf(caption, sizeof(caption), "Truncated PNG: %s",
                      StatusName(facet::GetImageStatus(assets.damaged)));
        Example(assets.damaged, ImVec2(tile, tile), options, caption);
    }
    if (facet::Button("Load again"))
        StartAsyncLoads();
    ctx.Probe("images.reload");
    if (ctx.captureState == kStateLoading)
        ctx.RevealInCapture(0.7f);

    SectionHeader("SVG");
    Paragraph("A full-color SVG keeps its colors. Each copy is rasterized at its design size times the UI "
              "scale, and again when the scale changes.");
    // The copies sit on one bottom line, so their captions share a baseline.
    float tallest = 0.0f;
    for (const facet::ImageHandle handle : assets.svgScaled)
        tallest = std::max(tallest, facet::ImageSize(handle).y);
    for (std::size_t i = 0; i < std::size(kSvgScales); ++i)
    {
        const ImVec2 pixels = facet::ImageSize(assets.svgScaled[i]);
        char caption[64];
        std::snprintf(caption, sizeof(caption), "%.0f units, %.0f x %.0f px",
                      static_cast<double>(kSvgDesignSide * kSvgScales[i]), static_cast<double>(pixels.x),
                      static_cast<double>(pixels.y));
        if (i > 0)
            Flow(pixels.x);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(pixels.x, tallest - pixels.y));
        facet::Image(assets.svgScaled[i], ImVec2(0.0f, 0.0f));
        Caption(caption);
        ImGui::EndGroup();
    }
    if (ctx.captureState == kStateResults)
        ctx.RevealInCapture(1.0f);
}

FACET_DEMO_SELFTEST("images")
{
    static facet::ImageHandle before;
    test.Do([] { ui = ImagesUi{}; });

    using T = facet::demo::Target;
    test.MouseMove(T::Item("images.fit", 0.05f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("clicking Contain selects it", [] { return ui.fit == facet::ImageFit::Contain; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right on the focused control selects Cover", [] { return ui.fit == facet::ImageFit::Cover; });

    test.MouseMove(T::Item("images.radius", 0.05f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("clicking element selects the smallest radius", [] { return ui.radius == 0; });
    test.MouseMove(T::Item("images.radius", 0.95f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("clicking window selects the largest radius", [] { return ui.radius == 3; });
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("Left on the focused control selects card", [] { return ui.radius == 2; });

    test.Click("images.tint");
    test.Expect("clicking Accent tint turns it on", [] { return ui.tint; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("images.tint");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused checkbox turns the tint off", [] { return !ui.tint; });

    // The next Tab stop is Load again; Space starts the loads anew.
    test.Do([] { before = assets.async; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("images.reload");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on Load again starts a new load",
                [] { return assets.async.IsValid() && !(assets.async == before); });

    test.Do([] { before = assets.async; });
    test.Click("images.reload");
    test.Expect("Load again starts a new load", [] { return assets.async.IsValid() && !(assets.async == before); });
    test.Expect("the previous handle is released", [] { return !facet::IsImageReady(before); });
    test.Do([] {
        facet::WaitForImage(assets.async, 10.0f);
        facet::WaitForImage(assets.missing, 10.0f);
        facet::WaitForImage(assets.damaged, 10.0f);
    });
    test.Expect("the generated PNG decodes", [] { return facet::IsImageReady(assets.async); });
    test.Expect("the missing file ends in the failed state",
                [] { return facet::GetImageStatus(assets.missing) == facet::ImageStatus::Failed; });
    test.Expect("the truncated PNG ends in the failed state",
                [] { return facet::GetImageStatus(assets.damaged) == facet::ImageStatus::Failed; });
    test.Wait(1);
    test.Expect("the two failed examples draw only their own outline", [] { return drawn.failedBoxes == 2; });
    test.Expect("every example box is a whole number of pixels", [] { return !drawn.offGrid; });
    test.Expect("every SVG copy is rasterized at its design size times the scale", [] {
        for (std::size_t i = 0; i < std::size(kSvgScales); ++i)
        {
            const float expected = std::round(kSvgDesignSide * kSvgScales[i] * facet::Scale());
            if (facet::ImageSize(assets.svgScaled[i]).x != expected)
                return false;
        }
        return true;
    });
}
