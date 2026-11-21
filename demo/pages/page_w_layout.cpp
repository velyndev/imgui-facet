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

void Arrange()
{
    SectionHeader("Arrange items");
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

void Dependent(LayoutState& s)
{
    SectionHeader("Dependent group and separators");
    if (facet::BeginDependentGroup("dependent", s.parentEnabled))
    {
        Note("Content of the group.");
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

FACET_DEMO_PAGE("layout", "Layout", "Widgets", 90, "default", "focused")
{
    facet::demo::PageHeader("Layout",
                            "Stacks, grids, flow layout, split views, panels, list and detail, settings rows, "
                            "dependent groups, separators and item grids.");
    facet::demo::PlannedFamilyNote("layout");
    LayoutState& s = State();
    SettingsRows(ctx, s);
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
}
