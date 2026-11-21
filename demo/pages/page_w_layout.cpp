// Demo page: Layout. One section per widget of facet/widgets/layout.h, with its variants and
// states, and the selftest at the end of the file. The controls inside the layouts are Facet widgets
// of the other families; their labels are placeholders, not real settings. Widgets of this family
// that are still planned draw their placeholder.

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"
#include "widgets/layout/layout_internal.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Capture states of the page, in the order of the FACET_DEMO_PAGE registration.
enum CaptureState
{
    kCaptureDefault = 0,
    kCaptureFocused = 1,
};

constexpr int kDefaultLevel = 40;
// The narrow card of the settings rows: too narrow for the label column and a control.
constexpr float kNarrowCard = 220.0f;

const char* const kSegments[] = {"First", "Second", "Third"};
const char* const kAligns[] = {"Start", "Center", "End"};
const char* const kGaps[] = {"None", "Tight", "Item", "Loose", "Section"};
// Grid column limits: index 0 is no limit, the others the column count.
const char* const kColumns[] = {"Auto", "One", "Two"};
constexpr int kGridCells = 7;
constexpr float kGridCell = 80.0f;
// The width of the stack column.
constexpr float kColumnWidth = 280.0f;

struct LayoutState
{
    // Settings rows
    bool toggle = false;
    int segment = 0;
    int level = kDefaultLevel;
    int smallPresses = 0;
    int largePresses = 0;
    float rowControlX[3] = {};      // where the controls of the first three rows start
    float largeLabelTop = 0.0f;     // the label of the Large row
    float largeTextTop = 0.0f;      // where the Large button draws its text
    bool largeStacked = false;      // the Large row is too narrow for label and control on one line
    bool narrowStacked = false;     // the rows in the narrow card stack
    // Stack
    int stackPresses[3] = {};
    float applyRight = 0.0f;
    float actionsRight = 0.0f;
    int stackAlign = 1;
    float sizeCenters[3] = {};
    int stackGap = 2;
    float columnTops[2] = {};       // bottom of the first and top of the second item of the column
    int otherPresses = 0;           // the buttons of the size row and the column
    // Grid
    int gridColumns = 0;
    float gridCellX[kGridCells] = {};
    // Placeholders of the planned widgets
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

bool Focused(const facet::demo::PageCtx& ctx)
{
    return ctx.capture && ctx.captureState == kCaptureFocused;
}

// Records the reset button of the settings row `label` that just ended as a probe.
void ProbeReset(facet::demo::PageCtx& ctx, const char* name, const char* label)
{
    const facet::internal::RowLayout& row = facet::internal::LastSettingsRowLayout();
    const float gap = facet::Px(facet::T().spacing.sm);
    const float side = row.control.GetHeight();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID id = ImHashStr("##reset", 0, window->GetID(label));
    const ImVec2 min(row.control.Max.x + gap, row.control.Min.y);
    facet::demo::ProbeRect(ctx, name, id, min, ImVec2(min.x + side, min.y + side));
}

void SettingsRows(facet::demo::PageCtx& ctx, LayoutState& s)
{
    SectionHeader("Settings row");
    Paragraph("A label column and a control column. The controls of a column of rows start at one x "
              "whatever the label lengths; a label too long for its column ends in an ellipsis and "
              "shows in full on hover. The labels are placeholders.");
    facet::BeginSettingsRow("Switch", {.description = "A description line under the label."});
    if (Focused(ctx))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    facet::Toggle("##value", &s.toggle);
    ctx.Probe("layout.row.toggle");
    s.rowControlX[0] = ImGui::GetItemRectMin().x;
    facet::EndSettingsRow();

    facet::BeginSettingsRow("A placeholder label long enough to end in an ellipsis");
    facet::SegmentedControl("##value", &s.segment, kSegments);
    ctx.Probe("layout.row.segment");
    s.rowControlX[1] = ImGui::GetItemRectMin().x;
    facet::EndSettingsRow();

    facet::BeginSettingsRow("Level", {.icon = facet::Icon("sliders"), .modified = s.level != kDefaultLevel,
                                      .resetButton = true});
    facet::Slider("##value", &s.level, 0, 100);
    ctx.Probe("layout.row.slider");
    s.rowControlX[2] = ImGui::GetItemRectMin().x;
    facet::EndSettingsRow();
    ProbeReset(ctx, "layout.row.reset", "Level");
    if (facet::SettingsRowResetPressed())
        s.level = kDefaultLevel;
    Note("Switch %s, segment %d, level %d (the reset button shows while the level differs from %d).",
         s.toggle ? "on" : "off", s.segment + 1, s.level, kDefaultLevel);

    // Control sizes: the label sits on the line of the control's text.
    facet::PushControlSize(facet::ControlSize::Small);
    facet::BeginSettingsRow("Small control");
    if (facet::Button("Count a press"))
        ++s.smallPresses;
    ctx.Probe("layout.row.small");
    facet::EndSettingsRow();
    facet::PopControlSize();
    facet::BeginSettingsRow("Large control");
    if (facet::Button("Count a press", {.size = facet::ControlSize::Large}))
        ++s.largePresses;
    ctx.Probe("layout.row.large");
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        facet::PushTextStyle(facet::ControlTextStyle(facet::ControlSize::Large));
        s.largeTextTop = facet::CenteredTextY(min.y, max.y - min.y);
        facet::PopTextStyle();
    }
    facet::EndSettingsRow();
    s.largeLabelTop = facet::internal::LastSettingsRowLayout().label.Min.y;
    s.largeStacked = facet::internal::LastSettingsRowLayout().stacked;
    Note("Presses: small %d, large %d.", s.smallPresses, s.largePresses);

    // A narrow region: the control goes under its label.
    ImGui::Spacing();
    facet::BeginCard("Narrow region", {.width = facet::demo::DemoWidth(kNarrowCard)});
    facet::BeginSettingsRow("Switch##narrow", {.description = "The control sits under the label here."});
    facet::Toggle("##value", &s.toggle);
    ctx.Probe("layout.row.narrow");
    facet::EndSettingsRow();
    s.narrowStacked = facet::internal::LastSettingsRowLayout().stacked;
    facet::EndCard();

    ImGui::Spacing();
    ImGui::BeginDisabled();
    facet::BeginSettingsRow("Disabled row", {.description = "Label and description follow the disabled control."});
    bool off = false;
    facet::Toggle("##value", &off);
    facet::EndSettingsRow();
    ImGui::EndDisabled();
}

void Stacks(facet::demo::PageCtx& ctx, LayoutState& s)
{
    SectionHeader("Stack");
    Paragraph("Items in a row or a column with a gap token. In the first row a spacer takes the free "
              "space, so the last button sits at the end; the second row centers controls of three "
              "sizes on one line.");
    facet::BeginStack("actions", {.orientation = facet::Orientation::Horizontal, .width = facet::kFill});
    if (facet::Button("Back"))
        ++s.stackPresses[0];
    ctx.Probe("layout.stack.back");
    if (facet::Button("Next"))
        ++s.stackPresses[1];
    ctx.Probe("layout.stack.next");
    facet::StackSpacer();
    if (facet::Button("Apply", {.variant = facet::ButtonVariant::Primary}))
        ++s.stackPresses[2];
    ctx.Probe("layout.stack.apply");
    s.applyRight = ImGui::GetItemRectMax().x;
    facet::EndStack();
    ctx.Probe("layout.stack.actions");
    s.actionsRight = ImGui::GetItemRectMax().x;
    Note("Presses: back %d, next %d, apply %d.", s.stackPresses[0], s.stackPresses[1], s.stackPresses[2]);

    const facet::LayoutAlign aligns[] = {facet::LayoutAlign::Start, facet::LayoutAlign::Center, facet::LayoutAlign::End};
    facet::BeginSettingsRow("Alignment");
    facet::SegmentedControl("##value", &s.stackAlign, kAligns);
    ctx.Probe("layout.stack.align");
    facet::EndSettingsRow();
    facet::BeginStack("sizes", {.orientation = facet::Orientation::Horizontal, .align = aligns[s.stackAlign]});
    if (facet::Button("Small", {.size = facet::ControlSize::Small}))
        ++s.otherPresses;
    s.sizeCenters[0] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    facet::StackSpacer();
    if (facet::Button("Regular"))
        ++s.otherPresses;
    s.sizeCenters[1] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    facet::StackSpacer();
    if (facet::Button("Large", {.size = facet::ControlSize::Large}))
        ++s.otherPresses;
    s.sizeCenters[2] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    facet::EndStack();

    facet::BeginSettingsRow("Gap");
    facet::SegmentedControl("##value", &s.stackGap, kGaps);
    ctx.Probe("layout.stack.gap");
    facet::EndSettingsRow();
    const facet::Gap gaps[] = {facet::Gap::None, facet::Gap::Tight, facet::Gap::Item, facet::Gap::Loose, facet::Gap::Section};
    facet::BeginStack("column", {.gap = gaps[s.stackGap], .align = facet::LayoutAlign::Stretch,
                                 .width = facet::demo::DemoWidth(kColumnWidth)});
    if (facet::Button("First in the column"))
        ++s.otherPresses;
    s.columnTops[0] = ImGui::GetItemRectMax().y;
    if (facet::Button("Second in the column"))
        ++s.otherPresses;
    s.columnTops[1] = ImGui::GetItemRectMin().y;
    facet::EndStack();
    Note("Presses of the buttons in the size row and the column: %d.", s.otherPresses);
}

void Grids(facet::demo::PageCtx& ctx, LayoutState& s)
{
    SectionHeader("Grid");
    Paragraph("Equal columns that reflow with the width: as many columns of the minimum cell width "
              "as fit. The last cell spans two columns. The cells are placeholders.");
    facet::BeginSettingsRow("Columns");
    facet::SegmentedControl("##value", &s.gridColumns, kColumns);
    ctx.Probe("layout.grid.columns");
    facet::EndSettingsRow();
    facet::BeginGrid("cells", {.minCellWidth = kGridCell, .maxColumns = s.gridColumns});
    for (int i = 0; i < kGridCells; ++i)
    {
        facet::GridNext({.columnSpan = i == kGridCells - 1 ? 2 : 1});
        ImGui::PushID(i);
        facet::BeginCard("##cell");
        Note("Cell %d", i + 1);
        facet::EndCard();
        ImGui::PopID();
        s.gridCellX[i] = ImGui::GetItemRectMin().x;
    }
    facet::EndGrid();
}

void Separators()
{
    SectionHeader("Separator");
    Paragraph("Hairlines snapped to whole pixels: across the width, between items on one line, and "
              "with a section text at the start or in the middle.");
    facet::SeparatorText("Section text");
    Note("Content of a section.");
    facet::Separator();
    facet::Badge("Item 1");
    ImGui::SameLine();
    facet::Separator({.orientation = facet::Orientation::Vertical});
    ImGui::SameLine();
    facet::Badge("Item 2");
    ImGui::SameLine();
    facet::Separator({.orientation = facet::Orientation::Vertical, .opaque = true});
    ImGui::SameLine();
    facet::Badge("Item 3");
    facet::SeparatorText("Centered section text", {.centered = true});
    facet::Separator({.opaque = true});
}

void Arrange()
{
    SectionHeader("Arrange items");
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

void Dependent(LayoutState& s)
{
    SectionHeader("Dependent group and separators");
    if (facet::BeginDependentGroup("dependent", s.parentEnabled))
    {
        Note("Content of the group.");
        facet::EndDependentGroup();
    }
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

FACET_DEMO_PAGE("layout", "Layout", "Widgets", 90, "default", "focused")
{
    facet::demo::PageHeader("Layout",
                            "Stacks, grids, flow layout, split views, panels, list and detail, settings rows, "
                            "dependent groups, separators and item grids.");
    facet::demo::PlannedFamilyNote("layout");
    LayoutState& s = State();
    SettingsRows(ctx, s);
    Stacks(ctx, s);
    Grids(ctx, s);
    Separators();
    Arrange();
    Divide(s);
    Dependent(s);
    Tiles(s);
}

FACET_DEMO_SELFTEST("layout")
{
    test.Do([] {
        LayoutState& s = State();
        s.toggle = false;
        s.segment = 0;
        s.level = kDefaultLevel;
        s.smallPresses = 0;
        s.largePresses = 0;
    });

    // Settings rows: one column, the label on the control's text line, stacked when narrow.
    test.Expect("the controls of a column of settings rows start at one x", [] {
        const LayoutState& s = State();
        return std::fabs(s.rowControlX[0] - s.rowControlX[1]) < 0.5f && std::fabs(s.rowControlX[0] - s.rowControlX[2]) < 0.5f;
    });
    test.Expect("the label of the Large row sits on the button's text line (or above it when stacked)", [] {
        const LayoutState& s = State();
        return s.largeStacked ? s.largeLabelTop < s.largeTextTop : std::fabs(s.largeLabelTop - s.largeTextTop) < 0.5f;
    });
    test.Expect("the rows of the narrow card stack their control", [] { return State().narrowStacked; });
    // Mouse and keyboard reach the controls in the rows.
    test.Click("layout.row.toggle");
    test.Expect("a click on the switch in a row flips it", [] { return State().toggle; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.row.toggle");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space flips the focused switch back", [] { return !State().toggle; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("the arrows move the segmented control in a row", [] { return State().segment == 1; });
    // The reset button: shown while modified, by mouse and by keyboard.
    test.Click("layout.row.slider");
    test.Expect("a click on the slider changes the level", [] { return State().level != kDefaultLevel; });
    test.Click("layout.row.reset");
    test.Expect("the reset button puts the level back", [] { return State().level == kDefaultLevel; });
    test.Click("layout.row.slider");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.row.slider");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("the arrow key steps the level", [] { return State().level != kDefaultLevel; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.row.reset");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the reset button puts the level back", [] { return State().level == kDefaultLevel; });
    test.Click("layout.row.large");
    test.Click("layout.row.small");
    test.Expect("the buttons of both sizes work in rows",
                [] { return State().largePresses == 1 && State().smallPresses == 1; });

    // Stack: the spacer puts the last button at the end; Tab follows the row; alignment and gap.
    test.Expect("the spacer puts the last button at the end of the stack",
                [] { return std::fabs(State().applyRight - State().actionsRight) < 0.5f; });
    test.Click("layout.stack.back");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.stack.next");
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.stack.apply");
    test.KeyPress(ImGuiKey_Space);
    test.Click("layout.stack.apply");
    test.Expect("each button of the stack works by mouse and keyboard", [] {
        const LayoutState& s = State();
        return s.stackPresses[0] == 1 && s.stackPresses[1] == 1 && s.stackPresses[2] == 2;
    });
    test.Do([] { State().stackAlign = 1; });
    test.Wait(3);
    test.Expect("centered, the three sizes share one center line", [] {
        const LayoutState& s = State();
        return std::fabs(s.sizeCenters[0] - s.sizeCenters[2]) < 1.0f && std::fabs(s.sizeCenters[1] - s.sizeCenters[2]) < 1.0f;
    });
    test.Click("layout.stack.align");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(3);
    test.Expect("aligned to the end, the small button ends on the large one's bottom", [] {
        const LayoutState& s = State();
        return s.stackAlign == 2 && s.sizeCenters[0] > s.sizeCenters[2];
    });
    test.Do([] { State().stackGap = 3; });
    test.Wait(2);
    test.Expect("the column takes the Loose gap", [] {
        const LayoutState& s = State();
        return std::fabs(s.columnTops[1] - s.columnTops[0] - facet::Px(facet::T().spacing.lg)) < 0.5f;
    });

    // Grid: one column, then two; the spanning cell starts a row of its own.
    test.Do([] { State().gridColumns = 1; });
    test.Wait(2);
    test.Expect("one column: every cell starts at one x",
                [] { return std::fabs(State().gridCellX[0] - State().gridCellX[1]) < 0.5f; });
    test.Click("layout.grid.columns");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.Expect("two columns: the second cell sits beside the first", [] {
        const LayoutState& s = State();
        return s.gridColumns == 2 && s.gridCellX[1] > s.gridCellX[0] + 1.0f && std::fabs(s.gridCellX[2] - s.gridCellX[0]) < 0.5f;
    });
    test.Do([] { State().gridColumns = 0; });
}
