// Demo page: Layout. Calls every v1 widget of facet/widgets/layout.h once, grouped by purpose.
// The widgets are planned, so each call draws its placeholder; Phase 2 replaces them one by one
// on this page. The width classes the layouts use already work (core/layout.h) and are shown at
// the top.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct LayoutState
{
    float splitRatio = 0.4f;
    float panelSizes[3] = {0.25f, 0.5f, 0.25f};
    bool detailOpen = false;
    bool parentEnabled = false;
    ImGuiSelectionBasicStorage selection;
};

// Height of the sample regions: a few large control heights in design units, so it follows the
// tokens.
constexpr float kRegionControls = 5.0f;

float RegionHeight()
{
    return facet::T().metrics.controlHeightLarge * kRegionControls;
}

LayoutState& State()
{
    static LayoutState state;
    return state;
}

const char* WidthClassName(facet::WidthClass widthClass)
{
    switch (widthClass)
    {
    case facet::WidthClass::Compact:
        return "Compact";
    case facet::WidthClass::Regular:
        return "Regular";
    case facet::WidthClass::Expanded:
        return "Expanded";
    }
    return "";
}

void Arrange()
{
    SectionHeader("Arrange items");
    Note("Available width here: %.0f units, width class %s.", facet::AvailableWidthUnits(),
         WidthClassName(facet::AvailableWidthClass()));
    facet::BeginStack("stack", {.orientation = facet::Orientation::Horizontal, .align = facet::LayoutAlign::Center});
    Note("Item 1");
    facet::StackSpacer();
    Note("Item 2");
    facet::EndStack();
    facet::BeginGrid("grid", {.maxColumns = 3});
    for (int cell = 0; cell < 3; ++cell)
    {
        facet::GridNext();
        Note("Cell %d", cell + 1);
    }
    facet::EndGrid();
    facet::BeginFlow("flow", {.maxLines = 2});
    for (int item = 0; item < 3; ++item)
        if (facet::FlowNext())
            Note("Item %d", item + 1);
    facet::EndFlow();
}

void Divide(LayoutState& s)
{
    SectionHeader("Divide a region");
    facet::BeginSplitView("split", &s.splitRatio, {.collapsible = true, .height = RegionHeight()});
    Note("First pane.");
    facet::SplitViewNext();
    Note("Second pane.");
    facet::EndSplitView();
    facet::BeginPanels("panels", s.panelSizes, {.height = RegionHeight()});
    const char* const titles[] = {"Panel 1", "Panel 2", "Panel 3"};
    for (const char* title : titles)
    {
        if (facet::BeginPanel(title))
        {
            Note("%s content.", title);
            facet::EndPanel();
        }
    }
    facet::EndPanels();
    facet::BeginListDetail("listdetail", &s.detailOpen);
    if (facet::BeginListPane())
    {
        Note("List pane.");
        facet::EndListPane();
    }
    if (facet::BeginDetailPane())
    {
        Note("Detail pane.");
        facet::EndDetailPane();
    }
    facet::EndListDetail();
}

void Settings(LayoutState& s)
{
    SectionHeader("Settings");
    facet::BeginSettingsRow("Setting 1", {.description = "Description of the setting"});
    ImGui::Checkbox("##value", &s.parentEnabled);
    facet::EndSettingsRow();
    if (facet::BeginDependentGroup("dependent", s.parentEnabled))
    {
        facet::BeginSettingsRow("Setting 2", {.modified = true, .resetButton = true});
        Note("Control of setting 2.");
        facet::EndSettingsRow();
        facet::EndDependentGroup();
    }
    facet::Separator();
    facet::SeparatorText("Section");
}

void Tiles(LayoutState& s)
{
    SectionHeader("Tiles");
    Paragraph("A virtualized grid of equal tiles for libraries and inventories.");
    if (facet::BeginItemGrid("items", 500, &s.selection, {.height = RegionHeight()}))
    {
        facet::RowRange tiles;
        while (facet::NextVisibleRows(&tiles))
            for (int tile = tiles.first; tile < tiles.last; ++tile)
                facet::ItemGridTile(tile);
        facet::EndItemGrid();
    }
}

} // namespace

FACET_DEMO_PAGE("layout", "Layout", "Widgets", 90)
{
    facet::demo::PageHeader("Layout",
                            "Stacks, grids, flow layout, split views, panels, list and detail, settings rows, "
                            "dependent groups, separators and item grids.");
    facet::demo::PlannedFamilyNote("layout");
    LayoutState& s = State();
    Arrange();
    Divide(s);
    Settings(s);
    Tiles(s);
}
