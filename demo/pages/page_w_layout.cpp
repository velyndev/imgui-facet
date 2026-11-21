// Demo page: Layout. One section per widget of facet/widgets/layout.h, with its variants and
// states, and the selftest at the end of the file. The controls inside the layouts are Facet widgets
// of the other families; their labels are placeholders, not real settings. Widgets of this family
// that are still planned draw their placeholder.

#include <algorithm>
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
    kCaptureArrange = 2,
    kCaptureSplit = 3,
    kCaptureDependent = 4,
};

// Split views: the width slider's lowest share, the first pane's minimum and the vertical minimums
// (units), and the placeholder lines of the first pane.
constexpr int kMinSplitShare = 20;
constexpr float kSplitMin = 160.0f;
constexpr float kSplitMinVertical = 48.0f;
constexpr int kSplitLines = 8;

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
    // Split view
    int splitWidth = 100;
    float splitRatio = 0.4f;
    float splitVertical = 0.5f;
    int splitPresses = 0;
    float endRatio = 0.0f;          // the ratio End gave, for the selftest
    // Dependent group
    bool parentEnabled = false;
    bool childToggle = false;
    int childLevel = 50;
    bool hideParent = false;
    bool hiddenToggle = false;
    // Placeholders of the planned widgets
    float panelSizes[3] = {0.25f, 0.5f, 0.25f};
    bool detailOpen = false;
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

// Records the divider of the split view that just ended as a probe.
void ProbeDivider(facet::demo::PageCtx& ctx, const char* name)
{
    const facet::internal::LayoutDividerRecord& divider = facet::internal::LastSplitViewDivider();
    facet::demo::ProbeRect(ctx, name, divider.id, divider.band.Min, divider.band.Max);
}

void SplitViews(facet::demo::PageCtx& ctx, LayoutState& s)
{
    SectionHeader("Split view");
    Paragraph("Two panes with a divider. Drag the divider or focus it with Tab and use the arrow keys, "
              "Home and End; a double click goes back to the first ratio. The first view is "
              "collapsible: dragging past half a minimum, or Enter, collapses its first pane. Narrow the "
              "width to see the minimums hold and, in a Compact width, the panes stack. The pane texts "
              "are placeholders.");
    facet::BeginSettingsRow("Width");
    facet::Slider("##value", &s.splitWidth, kMinSplitShare, 100, {.format = "%d %%"});
    ctx.Probe("layout.split.width");
    facet::EndSettingsRow();
    const float share = static_cast<float>(s.splitWidth) / 100.0f;
    const float width = std::max(1.0f, facet::AvailableWidthUnits() * share);
    if (ctx.capture && ctx.captureState == kCaptureSplit)
        ctx.ForceNextItemState(ForcedItemState::Focused, 0);
    // Stacked in a Compact width, the panes need the height of both minimums.
    const float height = RegionHeight() * (facet::WidthClassOf(width) == facet::WidthClass::Compact ? 2.0f : 1.0f);
    facet::BeginSplitView("split", &s.splitRatio,
                          {.minFirst = kSplitMin, .collapsible = true, .width = width, .height = height});
    if (facet::Button("Count a press", {.width = facet::kFill}))
        ++s.splitPresses;
    ctx.Probe("layout.split.first");
    for (int line = 0; line < kSplitLines; ++line)
        Note("Placeholder line %d of the first pane.", line + 1);
    facet::SplitViewNext();
    Note("Second pane. Ratio %.2f, button presses %d.", s.splitRatio, s.splitPresses);
    if (facet::Button("Count a press##second"))
        ++s.splitPresses;
    ctx.Probe("layout.split.second");
    facet::EndSplitView();
    ProbeDivider(ctx, "layout.split.divider");
    if (ctx.capture && ctx.captureState == kCaptureSplit)
        ctx.RevealInCapture();

    ImGui::Spacing();
    facet::BeginSplitView("vertical", &s.splitVertical,
                          {.orientation = facet::Orientation::Vertical, .minFirst = kSplitMinVertical,
                           .minSecond = kSplitMinVertical, .width = width, .height = RegionHeight()});
    Note("Top pane of a vertical split.");
    facet::SplitViewNext();
    Note("Bottom pane. Ratio %.2f.", s.splitVertical);
    facet::EndSplitView();
    ProbeDivider(ctx, "layout.split.vertical");
}

void Divide(LayoutState& s)
{
    SectionHeader("Divide a region");
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

void Dependent(facet::demo::PageCtx& ctx, LayoutState& s)
{
    SectionHeader("Dependent group");
    Paragraph("Settings that only apply while the setting above them is on. Off, the first group stays "
              "visible and disabled, and Tab skips it; the second group collapses. The labels are "
              "placeholders.");
    facet::BeginSettingsRow("Parent setting");
    facet::Toggle("##value", &s.parentEnabled);
    ctx.Probe("layout.dep.parent");
    facet::EndSettingsRow();
    if (facet::BeginDependentGroup("disable", s.parentEnabled))
    {
        facet::BeginSettingsRow("Child switch");
        facet::Toggle("##value", &s.childToggle);
        ctx.Probe("layout.dep.child");
        facet::EndSettingsRow();
        facet::BeginSettingsRow("Child level");
        facet::Slider("##value", &s.childLevel, 0, 100);
        facet::EndSettingsRow();
        facet::EndDependentGroup();
    }
    facet::BeginSettingsRow("Second parent");
    facet::Toggle("##value", &s.hideParent);
    ctx.Probe("layout.dep.hideparent");
    facet::EndSettingsRow();
    if (facet::BeginDependentGroup("hide", s.hideParent, {.mode = facet::DependentMode::Hide}))
    {
        facet::BeginSettingsRow("Shown while on");
        facet::Toggle("##value", &s.hiddenToggle);
        ctx.Probe("layout.dep.hidden");
        facet::EndSettingsRow();
        facet::EndDependentGroup();
    }
    if (ctx.capture && ctx.captureState == kCaptureDependent)
        ctx.RevealInCapture();
    Note("Child switch %s, level %d; the second group's switch %s.", s.childToggle ? "on" : "off", s.childLevel,
         s.hiddenToggle ? "on" : "off");
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

FACET_DEMO_PAGE("layout", "Layout", "Widgets", 90, "default", "focused", "arrange", "split", "dependent")
{
    facet::demo::PageHeader("Layout",
                            "Stacks, grids, flow layout, split views, panels, list and detail, settings rows, "
                            "dependent groups, separators and item grids.");
    facet::demo::PlannedFamilyNote("layout");
    LayoutState& s = State();
    if (ctx.capture)
        s.parentEnabled = s.hideParent = ctx.captureState == kCaptureDependent;
    SettingsRows(ctx, s);
    Dependent(ctx, s);
    Stacks(ctx, s);
    Grids(ctx, s);
    if (ctx.capture && ctx.captureState == kCaptureArrange)
        ctx.RevealInCapture();
    Separators();
    Arrange();
    SplitViews(ctx, s);
    Divide(s);
    Tiles(s);
}

FACET_DEMO_SELFTEST("layout")
{
    using T = facet::demo::Target;
    test.Do([] {
        LayoutState& s = State();
        s.toggle = false;
        s.segment = 0;
        s.level = kDefaultLevel;
        s.smallPresses = 0;
        s.largePresses = 0;
        for (int& presses : s.stackPresses)
            presses = 0;
        s.otherPresses = 0;
        s.stackAlign = 1;
        s.stackGap = 2;
        s.gridColumns = 0;
        s.hiddenToggle = false;
        s.childLevel = 50;
        s.splitVertical = 0.5f;
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

    // Dependent group: off, the content is disabled and Tab skips it; on, it works.
    test.Do([] {
        LayoutState& s = State();
        s.parentEnabled = false;
        s.childToggle = false;
        s.hideParent = false;
    });
    test.Wait(2);
    test.Click("layout.dep.child");
    test.Expect("a click on a disabled child does nothing", [] { return !State().childToggle; });
    test.Click("layout.dep.parent");
    test.Click("layout.dep.parent");
    test.Expect("the parent is off again", [] { return !State().parentEnabled; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.dep.hideparent");
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("layout.dep.parent");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space turns the parent on", [] { return State().parentEnabled; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.dep.child");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("the child switch works while the parent is on", [] { return State().childToggle; });
    test.Click("layout.dep.hideparent");
    test.WaitSeconds(0.5f);
    test.Reveal("layout.dep.hidden");
    test.ExpectVisible("layout.dep.hidden");
    test.Click("layout.dep.hidden");
    test.Expect("the shown child of the second group works", [] { return State().hiddenToggle; });
    test.Click("layout.dep.hideparent");
    test.WaitSeconds(0.5f);
    test.ExpectNotVisible("layout.dep.hidden");

    // Split view: drag, keys, collapse, reset and Escape.
    test.Do([] {
        LayoutState& s = State();
        s.splitWidth = 100;
        s.splitRatio = 0.4f;
        s.splitPresses = 0;
    });
    test.Reveal("layout.split.divider");
    test.Wait(2);
    // The offsets go along both axes: in a Compact width the panes stack and the divider moves on y.
    test.Drag({T::Item("layout.split.divider"), T::Item("layout.split.divider").Offset(60.0f, 60.0f)}, 0.3f);
    test.Expect("dragging the divider widens the first pane", [] { return State().splitRatio > 0.42f; });
    test.DoubleClick("layout.split.divider");
    test.ExpectNear("a double click goes back to the first ratio", [] { return State().splitRatio; }, 0.4, 0.001);
    test.Click("layout.split.first");
    test.Expect("the button of the first pane works", [] { return State().splitPresses == 1; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.split.divider");
    test.KeyPress(ImGuiKey_End);
    test.Expect("End moves the focused divider to the second pane's minimum", [] { return State().splitRatio > 0.5f; });
    test.Do([] { State().endRatio = State().splitRatio; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter collapses the first pane", [] { return State().splitRatio == 0.0f; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter brings it back", [] { return State().splitRatio == State().endRatio; });
    test.KeyPress(ImGuiKey_Home);
    test.Expect("Home moves the divider to the first pane's minimum",
                [] { return State().splitRatio > 0.0f && State().splitRatio < State().endRatio; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("layout.split.second");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Tab goes on to the second pane, whose button works", [] { return State().splitPresses == 2; });
    test.Do([] { State().splitRatio = 0.4f; });
    test.Wait(2);
    test.Reveal("layout.split.divider");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("layout.split.divider"));
    test.MouseDown();
    test.MouseMove(T::Item("layout.split.divider").Offset(80.0f, 80.0f));
    test.MouseMove(T::Item("layout.split.divider").Offset(40.0f, 40.0f));
    test.KeyPress(ImGuiKey_Escape);
    test.MouseUp();
    test.ExpectNear("Escape during a drag puts the ratio back", [] { return State().splitRatio; }, 0.4, 0.001);
    test.Reveal("layout.split.vertical");
    test.WaitSeconds(0.4f);
    test.Drag({T::Item("layout.split.vertical"), T::Item("layout.split.vertical").Offset(0.0f, 20.0f)}, 0.3f);
    test.Expect("the vertical divider drags along y", [] { return State().splitVertical > 0.5f; });
}
