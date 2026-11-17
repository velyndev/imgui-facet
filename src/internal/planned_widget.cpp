#include "internal/planned_widget.h"

#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"

namespace facet::internal
{

void PlannedWidget(const char* widget, const char* label, float width, float height)
{
    if (ImGui::GetCurrentContext() == nullptr || GImGui->CurrentWindow == nullptr)
        return;
    const Theme& theme = T();
    if (label == nullptr)
        label = "";
    if (widget == nullptr)
        widget = "";

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    char text[160];
    if (labelEnd > label)
        std::snprintf(text, sizeof(text), "%s: planned (%.*s)", widget, static_cast<int>(labelEnd - label), label);
    else
        std::snprintf(text, sizeof(text), "%s: planned", widget);

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

    PushTextStyle(TextStyle::Footnote);
    const float inset = Px(theme.spacing.sm);
    const float y = CenteredTextY(box.Min.y, h);
    drawList->PushClipRect(box.Min, ImVec2(box.Max.x - inset, box.Max.y), true);
    drawList->AddText(ImVec2(box.Min.x + inset, y), ToU32(theme.colors.labelSecondary), text);
    drawList->PopClipRect();
    PopTextStyle();
}

} // namespace facet::internal
