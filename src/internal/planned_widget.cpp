// The placeholder of planned widgets (planned_widget.h).

#include "internal/planned_widget.h"

#include <cstdio>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

bool CanDraw()
{
    return PlannedCanDraw();
}

} // namespace

bool PlannedCanDraw()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    return g != nullptr && g->WithinFrameScope && g->CurrentWindow != nullptr && !g->CurrentWindow->IsFallbackWindow;
}

float PlannedWidth(float optionWidth, float fallback, bool itemWidth)
{
    if (!PlannedCanDraw())
        return 1.0f;
    ImGuiContext& g = *GImGui;
    const float available = ImGui::GetContentRegionAvail().x;
    float width = 0.0f;
    if (optionWidth > 0.0f)
        width = Px(optionWidth);
    else if (optionWidth == kFill)
        width = available;
    else if (optionWidth < 0.0f)
        width = available - Px(-optionWidth);
    else if (itemWidth && ((g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) != 0 ||
                           g.CurrentWindow->DC.ItemWidthStack.Size > 0))
        width = ImGui::CalcItemWidth();
    else
        width = fallback > 0.0f ? fallback : available;
    return width < 1.0f ? 1.0f : width;
}

void DrawPlannedBox(ImDrawList* drawList, const ImRect& box)
{
    if (drawList == nullptr)
        return;
    const Theme& theme = T();
    const ImU32 color = ToU32(theme.colors.separator);
    const float thickness = HairlineThickness();
    const float dash = ImMax(Px(theme.spacing.xs), 1.0f);
    const ImVec2 min = box.Min;
    const ImVec2 max = box.Max;
    for (float x = min.x; x < max.x; x += 2.0f * dash)
    {
        const float end = ImMin(x + dash, max.x);
        HairlineH(drawList, x, end, min.y, color);
        HairlineH(drawList, x, end, max.y - thickness, color);
    }
    for (float y = min.y; y < max.y; y += 2.0f * dash)
    {
        const float end = ImMin(y + dash, max.y);
        HairlineV(drawList, min.x, y, end, color);
        HairlineV(drawList, max.x - thickness, y, end, color);
    }
}

bool PlannedCaption(const char* widget, const char* label, float room, char* out, std::size_t outSize)
{
    if (out == nullptr || outSize == 0)
        return false;
    out[0] = '\0';
    if (room <= 0.0f || !CanDraw())
        return true;
    if (widget == nullptr)
        widget = "";
    if (label == nullptr)
        label = "";
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const int labelLength = static_cast<int>(labelEnd - label);
    const bool hasLabel = labelLength > 0;
    const auto fits = [&] { return ImGui::CalcTextSize(out).x <= room; };
    if (hasLabel)
    {
        std::snprintf(out, outSize, "%s: planned (%.*s)", widget, labelLength, label);
        if (fits())
            return false;
        // Once the whole text does not fit, only the label is kept, so every box of a row that
        // has a label shows it, whatever its width.
        TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), label, labelEnd, room, TruncateMode::End, out, outSize);
        return true;
    }
    std::snprintf(out, outSize, "%s: planned", widget);
    if (fits())
        return false;
    TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), widget, nullptr, room, TruncateMode::End, out, outSize);
    return true;
}

void DrawPlannedCaption(ImDrawList* drawList, const ImRect& box, const char* widget, const char* label, bool hovered)
{
    if (drawList == nullptr || !CanDraw())
        return;
    if (widget == nullptr)
        widget = "";
    if (label == nullptr)
        label = "";
    const Theme& theme = T();
    const bool disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    PushTextStyle(TextStyle::Footnote);
    const float inset = Px(theme.spacing.sm);
    char text[kPlannedCaptionSize];
    const bool shortened = PlannedCaption(widget, label, box.GetWidth() - 2.0f * inset, text, sizeof(text));
    if (text[0] != '\0')
    {
        const ImVec4& color = disabled ? theme.colors.labelTertiary : theme.colors.labelSecondary;
        const float textHeight = ImGui::GetTextLineHeight();
        // One line of text centered in a single-line box, or on the first line of a taller one,
        // where a row label beside the box sits too.
        const float height = box.GetHeight();
        const float lineBox = height < 2.0f * textHeight + 2.0f * inset ? height : textHeight + 2.0f * inset;
        drawList->PushClipRect(box.Min, box.Max, true);
        drawList->AddText(ImVec2(box.Min.x + inset, CenteredTextY(box.Min.y, lineBox)), ToU32(color), text);
        drawList->PopClipRect();
    }
    PopTextStyle();

    if (shortened && hovered)
    {
        const char* labelEnd = ImGui::FindRenderedTextEnd(label);
        if (labelEnd > label)
            ImGui::SetTooltip("%s: planned (%.*s)", widget, static_cast<int>(labelEnd - label), label);
        else
            ImGui::SetTooltip("%s: planned", widget);
    }
}

void PlannedWidget(const char* widget, const char* label, float width, float height)
{
    if (!PlannedCanDraw())
        return;
    const Theme& theme = T();

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = width > 0.0f ? width : ImMax(ImGui::GetContentRegionAvail().x, 1.0f);
    const float h = height > 0.0f ? height : Px(theme.metrics.controlHeight);
    const ImRect box(pos, ImVec2(pos.x + w, pos.y + h));
    const ImGuiID id = label != nullptr ? ImGui::GetID(label) : 0;
    ImGui::ItemSize(box);
    if (!ImGui::ItemAdd(box, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoTabStop))
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawPlannedBox(drawList, box);
    DrawPlannedCaption(drawList, box, widget, label, ImGui::IsItemHovered());
}

} // namespace facet::internal
