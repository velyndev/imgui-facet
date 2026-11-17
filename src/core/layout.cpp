// Width classes and the labelled row layout (layout.h).
// Owner from Phase 1c part A on: the interaction blocks package (row layout and responsive core).

#include "facet/core/layout.h"

#include <algorithm>

#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet
{

WidthClass WidthClassOf(float widthUnits)
{
    const LayoutTokens& tokens = T().layout;
    if (!(widthUnits >= tokens.regularFrom))
        return WidthClass::Compact;
    if (widthUnits >= tokens.expandedFrom)
        return WidthClass::Expanded;
    return WidthClass::Regular;
}

float AvailableWidthUnits()
{
    if (ImGui::GetCurrentContext() == nullptr || GImGui->CurrentWindow == nullptr)
        return 0.0f;
    return std::max(0.0f, ImGui::GetContentRegionAvail().x) / Scale();
}

WidthClass AvailableWidthClass()
{
    return WidthClassOf(AvailableWidthUnits());
}

LabeledRowLayout LayoutLabeledRow(float availableWidth, float labelColumnUnits)
{
    const Theme& theme = T();
    const float width = std::max(0.0f, availableWidth);
    const float column = Px(labelColumnUnits > 0.0f ? labelColumnUnits : theme.layout.labelColumn);
    const float gap = Px(theme.spacing.sm);

    LabeledRowLayout row;
    row.stacked = width < column + Px(theme.layout.minRowControl);
    if (row.stacked)
    {
        row.labelWidth = width;
        row.controlOffset = 0.0f;
        row.controlWidth = width;
    }
    else
    {
        row.labelWidth = std::max(0.0f, column - gap);
        row.controlOffset = column;
        row.controlWidth = width - column;
    }
    return row;
}

} // namespace facet
