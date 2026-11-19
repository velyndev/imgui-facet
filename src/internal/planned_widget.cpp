// The placeholder of planned widgets (planned_widget.h).

#include "internal/planned_widget.h"

#include <cstdio>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

bool CanDraw()
{
    return ImGui::GetCurrentContext() != nullptr && GImGui->CurrentWindow != nullptr && GImGui->WithinFrameScope;
}

} // namespace

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
    if (ImGui::GetCurrentContext() == nullptr || GImGui->CurrentWindow == nullptr)
        return;
    const Theme& theme = T();
    if (label == nullptr)
        label = "";

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    const float h = height > 0.0f ? height : Px(theme.metrics.controlHeight);
    const ImRect box(pos, ImVec2(pos.x + w, pos.y + h));
    const ImGuiID id = ImGui::GetID(label);
    ImGui::ItemSize(box);
    if (!ImGui::ItemAdd(box, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoTabStop))
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = Px(theme.radii.control);
    FillRect(drawList, box.Min, box.Max, ToU32(theme.colors.fill[3]), radius);
    StrokeRect(drawList, box.Min, box.Max, ToU32(theme.colors.separator), radius, HairlineThickness());
    DrawPlannedCaption(drawList, box, widget, label, ImGui::IsItemHovered());
}

} // namespace facet::internal
