// Image layout math and the loading and failed placeholders.

#include <cfloat>
#include <cmath>

#include <imgui.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "image/image_internal.h"
#include "internal/loading.h"

namespace facet::internal
{
namespace
{

float StyleAlpha()
{
    return ImGui::GetCurrentContext() != nullptr ? ImGui::GetStyle().Alpha : 1.0f;
}

ImU32 Token(const ImVec4& color)
{
    return ToU32(WithAlpha(color, color.w * StyleAlpha()));
}

float ClampRadius(ImVec2 pMin, ImVec2 pMax, float radius)
{
    const float limit = 0.5f * std::fmin(pMax.x - pMin.x, pMax.y - pMin.y);
    return std::fmax(0.0f, std::fmin(radius, limit));
}

} // namespace

FitResult FitImage(ImVec2 boxMin, ImVec2 boxMax, ImVec2 imageSize, ImageFit fit)
{
    FitResult result{boxMin, boxMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)};
    const float boxWidth = boxMax.x - boxMin.x;
    const float boxHeight = boxMax.y - boxMin.y;
    if (boxWidth <= 0.0f || boxHeight <= 0.0f || imageSize.x <= 0.0f || imageSize.y <= 0.0f ||
        fit == ImageFit::Fill)
        return result;

    const float boxAspect = boxWidth / boxHeight;
    const float imageAspect = imageSize.x / imageSize.y;
    if (fit == ImageFit::Contain)
    {
        if (imageAspect > boxAspect)
        {
            const float height = boxWidth / imageAspect;
            result.min.y = boxMin.y + 0.5f * (boxHeight - height);
            result.max.y = result.min.y + height;
        }
        else
        {
            const float width = boxHeight * imageAspect;
            result.min.x = boxMin.x + 0.5f * (boxWidth - width);
            result.max.x = result.min.x + width;
        }
        return result;
    }

    // Cover: show the centered part of the texture that has the box aspect.
    if (imageAspect > boxAspect)
    {
        const float visible = boxAspect / imageAspect;
        result.uvMin.x = 0.5f * (1.0f - visible);
        result.uvMax.x = result.uvMin.x + visible;
    }
    else
    {
        const float visible = imageAspect / boxAspect;
        result.uvMin.y = 0.5f * (1.0f - visible);
        result.uvMax.y = result.uvMin.y + visible;
    }
    return result;
}

void DrawImageLoading(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius)
{
    if (drawList == nullptr || pMax.x <= pMin.x || pMax.y <= pMin.y)
        return;
    const Theme& theme = T();
    const Colors& c = theme.colors;
    radius = ClampRadius(pMin, pMax, radius);

    // The same skeleton as every other loading block, on the shared shimmer clock (flat with
    // reduced motion).
    DrawSkeleton(drawList, pMin, pMax, radius);

    // A faint picture glyph says what is coming, when the box has room for it.
    const float iconSize = std::floor(Px(theme.spacing.xl));
    if (pMax.x - pMin.x >= 2.0f * iconSize && pMax.y - pMin.y >= 2.0f * iconSize)
    {
        const ImVec2 pos(0.5f * (pMin.x + pMax.x - iconSize), 0.5f * (pMin.y + pMax.y - iconSize));
        DrawIcon(drawList, Icon("image"), pos, iconSize, Token(c.labelTertiary));
    }
}

void DrawImageFailed(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const char* label)
{
    if (drawList == nullptr || pMax.x <= pMin.x || pMax.y <= pMin.y)
        return;
    const Theme& theme = T();
    const Colors& c = theme.colors;
    radius = ClampRadius(pMin, pMax, radius);

    FillRect(drawList, pMin, pMax, Token(c.fill[3]), radius);
    StrokeRect(drawList, pMin, pMax, Token(c.separator), radius, HairlineThickness());

    const float width = pMax.x - pMin.x;
    const float height = pMax.y - pMin.y;
    const float iconSize = std::floor(Px(theme.spacing.xl));
    const float padding = Px(theme.spacing.xs);
    if (width < iconSize + 2.0f * padding || height < iconSize + 2.0f * padding)
        return;

    // Icon and label stacked and centered; the label is left out when it does not fit.
    const TextStyleSpec& spec = theme.type[TextStyle::Footnote];
    ImFont* font = WeightFont(spec.weight);
    if (font == nullptr && ImGui::GetCurrentContext() != nullptr)
        font = ImGui::GetFont();
    const float fontSize = Px(spec.size);
    const float gap = Px(theme.spacing.xs);
    ImVec2 textSize(0.0f, 0.0f);
    bool showLabel = false;
    if (font != nullptr && label != nullptr && label[0] != '\0')
    {
        textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label);
        showLabel = textSize.x + 2.0f * padding <= width &&
                    iconSize + gap + textSize.y + 2.0f * padding <= height;
    }

    const float blockHeight = showLabel ? iconSize + gap + textSize.y : iconSize;
    const float top = pMin.y + 0.5f * (height - blockHeight);
    const ImVec2 iconPos(0.5f * (pMin.x + pMax.x - iconSize), top);
    const ImU32 color = Token(c.labelSecondary);
    DrawIcon(drawList, Icon("image-off"), iconPos, iconSize, color);
    if (showLabel)
    {
        const ImVec2 textPos(std::floor(0.5f * (pMin.x + pMax.x - textSize.x)), std::floor(top + iconSize + gap));
        drawList->AddText(font, fontSize, textPos, color, label);
    }
}

} // namespace facet::internal
