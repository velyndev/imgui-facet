#pragma once

// CPU side of the soft shadows: Gaussian kernels, the baked corner tile, 9-slice geometry
// and the shelf packer of the shadow atlas. Pure math without Direct3D, so it is unit tested.
//
// A shadow is the Gaussian blur of a rounded rectangle. Its four corners are mirror images
// of each other and its edges are constant along their length, so one baked corner tile
// (the top-left quadrant) is enough: the 9-slice mirrors it into the other corners and
// stretches its last row and column along the edges.

#include <cstdint>
#include <vector>

#include <imgui.h>

namespace facet::internal
{

// Limits of the baked tiles, in pixels. Larger values are clamped, which keeps a tile
// (radius + 6 sigma, see ShadowTileLayout) small enough for one atlas page.
inline constexpr float kMaxShadowSigma = 64.0f;
inline constexpr int kMaxShadowRadius = 128;

// Sigma is quantized to this step so nearby values share one tile.
inline constexpr float kShadowSigmaStep = 0.5f;

// Number of pixels on each side of the kernel center: ceil(3 sigma), 0 for sigma <= 0.
// Three sigma hold 99.7 % of the Gaussian, the rest is below one 8-bit step.
int GaussianExtent(float sigma);

// Discrete Gaussian kernel sampled at integer offsets -extent..extent and normalized to a
// sum of 1. A sigma of 0 or less gives the single weight {1}.
void GaussianKernel(float sigma, std::vector<float>& weights);

// Quantized tile parameters. Two shadows with equal keys use the same atlas tile.
struct ShadowKey
{
    int radius = 0;          // corner radius in whole pixels
    int sigmaSteps = 0;      // sigma in kShadowSigmaStep units
    bool outsideOnly = false;  // the shape's interior is cut out of the tile (glow)

    float Sigma() const { return static_cast<float>(sigmaSteps) * kShadowSigmaStep; }
    std::uint64_t Packed() const;
    bool operator==(const ShadowKey&) const = default;
};

// Rounds the radius to whole pixels and sigma to kShadowSigmaStep, both clamped to the limits.
ShadowKey MakeShadowKey(float radius, float sigma, bool outsideOnly);

// Size of the corner tile of a key.
struct ShadowTileLayout
{
    int margin = 1;  // pixels between the tile edge and the shape edge: max(1, extent)
    int size = 2;    // tile width and height: radius + 2 * margin
};
ShadowTileLayout TileLayout(const ShadowKey& key);

// Bakes the top-left quadrant of the blurred shape into size * size alpha values (0..1,
// row-major). The shape's straight edges start at `margin` and its corner arc is centered at
// margin + radius. The last row and column hold the plain edge profile, the bottom-right
// texel the interior value. With outsideOnly, every value is multiplied by the uncovered
// part of its pixel, so the tile is zero inside the shape.
void BakeShadowTile(const ShadowKey& key, std::vector<float>& tile);

// Anti-aliased coverage (0..1) of the pixel (x, y) of the tile by the unblurred shape.
float ShadowShapeCoverage(const ShadowKey& key, int x, int y);

// The 4 x 4 vertex grid of a 9-slice shadow. Column i has screen x[i] and texture u[i]; the
// right corner mirrors the left one, so u[3] == u[0] and u[2] == u[1]. Rows likewise.
struct NineSlice
{
    float x[4] = {};
    float y[4] = {};
    float u[4] = {};
    float v[4] = {};
};

// Grid for the shape rect shapeMin..shapeMax (after offset and spread). The shadow covers
// the shape plus `margin` pixels on every side, mapped 1:1 onto tile texels. When the
// shadow is narrower or lower than two tiles, the corners shrink to half its size so they
// do not overlap. tileUv is the atlas UV of the tile's top-left corner, texelUv the UV size
// of one texel.
NineSlice ShadowNineSlice(ImVec2 shapeMin, ImVec2 shapeMax, const ShadowTileLayout& layout, ImVec2 tileUv,
                          ImVec2 texelUv);

// Shelf packer for the atlas pages: rectangles go left to right into rows (shelves) whose
// height is set by their first rectangle.
class ShelfPacker
{
public:
    void Reset(int width, int height);
    // Finds space for a w x h rectangle. Returns false when the page is full.
    bool Allocate(int w, int h, int& x, int& y);
    int Width() const { return width; }
    int Height() const { return height; }
    // Rows used from the top, for tests and statistics.
    int UsedHeight() const { return nextShelfY; }

private:
    struct Shelf
    {
        int y = 0;
        int height = 0;
        int cursorX = 0;
    };
    std::vector<Shelf> shelves;
    int width = 0;
    int height = 0;
    int nextShelfY = 0;
};

} // namespace facet::internal
