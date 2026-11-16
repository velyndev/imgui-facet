#include "effects/shadow_math.h"

#include <algorithm>
#include <cmath>

namespace facet::internal
{

int GaussianExtent(float sigma)
{
    if (!(sigma > 0.0f))
        return 0;
    return static_cast<int>(std::ceil(3.0f * sigma));
}

void GaussianKernel(float sigma, std::vector<float>& weights)
{
    const int extent = GaussianExtent(sigma);
    weights.assign(static_cast<size_t>(extent) * 2 + 1, 0.0f);
    if (extent == 0)
    {
        weights[0] = 1.0f;
        return;
    }
    const float denominator = 2.0f * sigma * sigma;
    float sum = 0.0f;
    for (int i = -extent; i <= extent; ++i)
    {
        const float d = static_cast<float>(i);
        const float w = std::exp(-(d * d) / denominator);
        weights[static_cast<size_t>(i + extent)] = w;
        sum += w;
    }
    for (float& w : weights)
        w /= sum;
}

std::uint64_t ShadowKey::Packed() const
{
    return static_cast<std::uint64_t>(static_cast<std::uint32_t>(radius)) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sigmaSteps)) << 24) |
           (static_cast<std::uint64_t>(outsideOnly ? 1u : 0u) << 48);
}

ShadowKey MakeShadowKey(float radius, float sigma, bool outsideOnly)
{
    ShadowKey key;
    const float r = std::clamp(std::isfinite(radius) ? radius : 0.0f, 0.0f, static_cast<float>(kMaxShadowRadius));
    const float s = std::clamp(std::isfinite(sigma) ? sigma : 0.0f, 0.0f, kMaxShadowSigma);
    key.radius = static_cast<int>(std::lround(r));
    key.sigmaSteps = static_cast<int>(std::lround(s / kShadowSigmaStep));
    key.outsideOnly = outsideOnly;
    return key;
}

ShadowTileLayout TileLayout(const ShadowKey& key)
{
    ShadowTileLayout layout;
    layout.margin = std::max(1, GaussianExtent(key.Sigma()));
    layout.size = key.radius + 2 * layout.margin;
    return layout;
}

float ShadowShapeCoverage(const ShadowKey& key, int x, int y)
{
    const ShadowTileLayout layout = TileLayout(key);
    const float m = static_cast<float>(layout.margin);
    const float r = static_cast<float>(key.radius);
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;
    if (key.radius > 0 && px < m + r && py < m + r)
    {
        // Inside the corner square: distance to the arc centered at (m + r, m + r).
        const float dx = px - (m + r);
        const float dy = py - (m + r);
        const float distance = r - std::sqrt(dx * dx + dy * dy);
        return std::clamp(distance + 0.5f, 0.0f, 1.0f);
    }
    // Straight edges lie on pixel boundaries, so these pixels are fully in or out.
    const float distance = std::min(px - m, py - m);
    return std::clamp(distance + 0.5f, 0.0f, 1.0f);
}

void BakeShadowTile(const ShadowKey& key, std::vector<float>& tile)
{
    const ShadowTileLayout layout = TileLayout(key);
    const int size = layout.size;
    const int extent = GaussianExtent(key.Sigma());
    // The blur at tile pixel x reads x - extent .. x + extent. Left of the tile the shape is
    // absent; right of it the shape continues, which the buffer covers up to size + margin.
    const int buffer = size + layout.margin;

    std::vector<float> kernel;
    GaussianKernel(key.Sigma(), kernel);

    std::vector<float> coverage(static_cast<size_t>(buffer) * static_cast<size_t>(buffer));
    for (int y = 0; y < buffer; ++y)
    {
        for (int x = 0; x < buffer; ++x)
            coverage[static_cast<size_t>(y) * static_cast<size_t>(buffer) + static_cast<size_t>(x)] =
                ShadowShapeCoverage(key, x, y);
    }

    // Horizontal pass: every buffer row, tile columns only.
    std::vector<float> horizontal(static_cast<size_t>(buffer) * static_cast<size_t>(size));
    for (int y = 0; y < buffer; ++y)
    {
        const float* row = coverage.data() + static_cast<size_t>(y) * static_cast<size_t>(buffer);
        for (int x = 0; x < size; ++x)
        {
            float sum = 0.0f;
            for (int k = -extent; k <= extent; ++k)
            {
                const int sx = x + k;
                if (sx >= 0)
                    sum += kernel[static_cast<size_t>(k + extent)] * row[sx];
            }
            horizontal[static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)] = sum;
        }
    }

    // Vertical pass into the tile.
    tile.assign(static_cast<size_t>(size) * static_cast<size_t>(size), 0.0f);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            float sum = 0.0f;
            for (int k = -extent; k <= extent; ++k)
            {
                const int sy = y + k;
                if (sy >= 0)
                    sum += kernel[static_cast<size_t>(k + extent)] *
                           horizontal[static_cast<size_t>(sy) * static_cast<size_t>(size) + static_cast<size_t>(x)];
            }
            float value = std::clamp(sum, 0.0f, 1.0f);
            if (key.outsideOnly)
                value *= 1.0f - coverage[static_cast<size_t>(y) * static_cast<size_t>(buffer) + static_cast<size_t>(x)];
            tile[static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)] = value;
        }
    }
}

NineSlice ShadowNineSlice(ImVec2 shapeMin, ImVec2 shapeMax, const ShadowTileLayout& layout, ImVec2 tileUv,
                          ImVec2 texelUv)
{
    NineSlice grid;
    const float margin = static_cast<float>(layout.margin);
    const float size = static_cast<float>(layout.size);
    const float x0 = shapeMin.x - margin;
    const float y0 = shapeMin.y - margin;
    const float x3 = shapeMax.x + margin;
    const float y3 = shapeMax.y + margin;
    const float cornerW = std::min(size, (x3 - x0) * 0.5f);
    const float cornerH = std::min(size, (y3 - y0) * 0.5f);

    grid.x[0] = x0;
    grid.x[1] = x0 + cornerW;
    grid.x[2] = x3 - cornerW;
    grid.x[3] = x3;
    grid.y[0] = y0;
    grid.y[1] = y0 + cornerH;
    grid.y[2] = y3 - cornerH;
    grid.y[3] = y3;

    grid.u[0] = grid.u[3] = tileUv.x;
    grid.u[1] = grid.u[2] = tileUv.x + cornerW * texelUv.x;
    grid.v[0] = grid.v[3] = tileUv.y;
    grid.v[1] = grid.v[2] = tileUv.y + cornerH * texelUv.y;
    return grid;
}

void ShelfPacker::Reset(int w, int h)
{
    shelves.clear();
    width = w;
    height = h;
    nextShelfY = 0;
}

bool ShelfPacker::Allocate(int w, int h, int& x, int& y)
{
    if (w <= 0 || h <= 0 || w > width || h > height)
        return false;
    // Best fit: the lowest shelf that is tall enough and has room left.
    Shelf* best = nullptr;
    for (Shelf& shelf : shelves)
    {
        if (shelf.height >= h && shelf.cursorX + w <= width && (best == nullptr || shelf.height < best->height))
            best = &shelf;
    }
    // A much taller shelf would waste most of its height; open a new one if there is room.
    if (best != nullptr && best->height > h * 2 && nextShelfY + h <= height)
        best = nullptr;
    if (best == nullptr)
    {
        if (nextShelfY + h > height)
            return false;
        shelves.push_back({nextShelfY, h, 0});
        nextShelfY += h;
        best = &shelves.back();
    }
    x = best->cursorX;
    y = best->y;
    best->cursorX += w;
    return true;
}

} // namespace facet::internal
