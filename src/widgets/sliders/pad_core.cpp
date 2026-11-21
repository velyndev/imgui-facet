// The shared parts of the two-dimensional sliders (pad_core.h).

#include "widgets/sliders/pad_core.h"

#include <algorithm>
#include <cmath>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet::internal
{
namespace
{

// More grid lines than this on one axis would crowd the pad into a texture: quarters instead.
constexpr int kPadMaxGridLines = 32;

// The fractions of the grid lines of one axis inside the pad (the ends are the frame itself).
int PadGridLines(const ValueMapping& mapping, double* out, int capacity)
{
    const ValueMapping m = NormalizeMapping(mapping);
    int count = 0;
    if (m.step > 0.0 && m.max > m.min && std::isfinite(m.max - m.min))
    {
        const double steps = std::floor((m.max - m.min) / m.step + 1e-9);
        if (steps >= 2.0 && steps <= static_cast<double>(kPadMaxGridLines))
        {
            for (int i = 1; i < static_cast<int>(steps) && count < capacity; ++i)
                out[count++] = ValueToFraction(m, m.min + i * m.step);
            return count;
        }
    }
    for (int i = 1; i < 4 && count < capacity; ++i)
        out[count++] = i * 0.25;
    return count;
}

} // namespace

float ResolvePadWidth(float optionsWidth, const NextItemData& next)
{
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float side = std::min(std::round(Px(T().metrics.padSize)), std::floor(available));
    return std::max(1.0f, std::round(ResolveItemWidth(optionsWidth, next, std::max(1.0f, side))));
}

PadFrame LayoutPadFrame(ImVec2 origin, float width, float height, const SliderMetrics& m)
{
    PadFrame p;
    origin = ImVec2(std::round(origin.x), std::round(origin.y));
    width = std::max(1.0f, std::round(width));
    height = std::max(1.0f, std::round(height));
    p.frame = ImRect(origin, ImVec2(origin.x + width, origin.y + height));
    // A pad smaller than two knobs gets a smaller knob, so the knob never leaves the frame.
    p.knob = std::min(m.knob, std::max(2.0f, std::floor(std::min(width, height) * 0.25f) * 2.0f));
    const float half = p.knob * 0.5f;
    p.area = ImRect(p.frame.Min.x + half, p.frame.Min.y + half, std::max(p.frame.Min.x + half, p.frame.Max.x - half),
                    std::max(p.frame.Min.y + half, p.frame.Max.y - half));
    return p;
}

ImRect PadKnobRect(ImVec2 point, float knob)
{
    const float half = std::round(knob * 0.5f);
    const float x = std::round(point.x);
    const float y = std::round(point.y);
    return ImRect(x - half, y - half, x - half + knob, y - half + knob);
}

void DrawPadGrid(ImDrawList* drawList, const ImRect& area, const ValueMapping& mappingX, const ValueMapping& mappingY,
                 bool yDown)
{
    const ImU32 color = ToU32(T().colors.separator);
    double lines[kPadMaxGridLines];
    const int columns = PadGridLines(mappingX, lines, kPadMaxGridLines);
    for (int i = 0; i < columns; ++i)
        HairlineV(drawList, std::round(area.Min.x + static_cast<float>(lines[i]) * area.GetWidth()), area.Min.y, area.Max.y,
                  color);
    const int rows = PadGridLines(mappingY, lines, kPadMaxGridLines);
    for (int i = 0; i < rows; ++i)
    {
        const float f = static_cast<float>(lines[i]);
        HairlineH(drawList, area.Min.x, area.Max.x,
                  std::round(yDown ? area.Min.y + f * area.GetHeight() : area.Max.y - f * area.GetHeight()), color);
    }
}

} // namespace facet::internal
