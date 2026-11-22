#include "overlay_backdrop.h"

#include <cmath>

#include <facet/facet.h>

namespace facet::demo
{
namespace
{

// Sizes and speeds in multiples of spacing.xxxl, so the scene follows the scale.
constexpr float kBandWidthInXxxl = 1.5f;     // one slanted band
constexpr float kBandSpeedInXxxl = 0.75f;    // band drift per second
constexpr int kCircleCount = 9;
constexpr float kCircleRadiusInXxxl = 2.5f;  // the largest circle
constexpr float kTextRowSpeedInXxxl = 1.0f;  // text drift per second
constexpr float kBandOpacity = 0.35f;       // the bands sit under the circles and the text
constexpr float kTau = 6.28318530718f;

// Text of the rows: it says what the scene is, so a capture never shows invented content.
constexpr const char* kBackdropText = "Generated backdrop, drawn in code    ";

} // namespace

void DrawOverlayBackdrop(ImDrawList* drawList, ImVec2 min, ImVec2 max, double time)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const float unit = facet::Px(t.spacing.xxxl);
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    const float seconds = static_cast<float>(std::fmod(time, 3600.0));
    drawList->AddRectFilled(min, max, facet::ToU32(c.bg));
    drawList->PushClipRect(min, max, true);

    // Slanted bands in the chart colors, drifting to the right.
    const float band = unit * kBandWidthInXxxl;
    const float slant = height * 0.5f;
    const float period = band * static_cast<float>(facet::kChartPaletteSize);
    const float shift = std::fmod(seconds * unit * kBandSpeedInXxxl, period);
    int index = 0;
    for (float x = min.x - slant - period + shift; x < max.x + period; x += band, ++index)
    {
        const ImVec4 color = c.chart[index % facet::kChartPaletteSize];
        const ImU32 fill = facet::ToU32(facet::WithAlpha(color, kBandOpacity));
        drawList->AddQuadFilled(ImVec2(x, max.y), ImVec2(x + band * 0.5f, max.y), ImVec2(x + band * 0.5f + slant, min.y),
                                ImVec2(x + slant, min.y), fill);
    }

    // Circles on slow orbits around points spread over the area.
    for (int i = 0; i < kCircleCount; ++i)
    {
        const float fi = static_cast<float>(i);
        const float cx = min.x + width * (0.1f + 0.8f * std::fmod(fi * 0.37f, 1.0f));
        const float cy = min.y + height * (0.15f + 0.7f * std::fmod(fi * 0.61f, 1.0f));
        const float speed = 0.05f + 0.03f * fi;
        const float angle = seconds * speed * kTau + fi;
        const float orbit = unit * (0.5f + 0.25f * fi);
        const ImVec2 center(cx + std::cos(angle) * orbit, cy + std::sin(angle * 0.8f) * orbit);
        const float radius = unit * kCircleRadiusInXxxl * (0.35f + 0.08f * fi);
        drawList->AddCircleFilled(center, radius, facet::ToU32(c.chart[(i + 3) % facet::kChartPaletteSize]));
        drawList->AddCircle(center, radius * 0.7f, facet::ToU32(c.label), 0, facet::Px(t.metrics.hairline) * 2.0f);
    }

    // Rows of text drifting sideways, every other row the other way.
    facet::PushTextStyle(facet::TextStyle::Title2);
    const float lineHeight = ImGui::GetFontSize() * 2.0f;
    const float textWidth = ImGui::CalcTextSize(kBackdropText).x;
    int row = 0;
    for (float y = min.y + lineHeight * 0.5f; y < max.y; y += lineHeight, ++row)
    {
        const float direction = row % 2 == 0 ? 1.0f : -1.0f;
        const float offset = std::fmod(seconds * unit * kTextRowSpeedInXxxl * direction, textWidth);
        for (float x = min.x - textWidth + offset; x < max.x; x += textWidth)
            drawList->AddText(ImVec2(x, y), facet::ToU32(c.label), kBackdropText);
    }
    facet::PopTextStyle();
    drawList->PopClipRect();
}

} // namespace facet::demo
