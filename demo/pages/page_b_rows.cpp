// Demo page: Rows and width classes. Labelled rows from the row layout block
// (internal/row_layout.h) in a region whose width a prototype slider sets, so the shared label
// column, the stacked fallback, label truncation, the description, help and error lines and the
// reset affordance can be seen at every width. The width class of the region comes from
// core/layout.h.

#include <algorithm>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/row_layout.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kNarrow,
    kError,
    kFocused,
};

constexpr double kDefaultScale = 100.0;
// The narrowest region the width slider offers, and the width of the narrow capture, in units.
constexpr double kMinRegion = 160.0;
constexpr double kNarrowRegion = 220.0;
// The region the selftest checks truncation in: the label column is capped at 40 % of it, far
// narrower than the long label, whatever the scale and the window size.
constexpr double kTruncationRegion = 480.0;
constexpr const char* kLongLabel = "A long label that ends in an ellipsis whenever the label column is narrower than "
                                   "its text";

struct RowsPageState
{
    double region = 0.0;  // units; 0 until the first frame sets the full width
    bool vsync = true;
    double volume = 0.8;
    double scale = kDefaultScale;
    bool longToggle = false;
    double lower = 20.0;
    double upper = 60.0;
    int resets = 0;
    double clicked = 0.0;  // the selftest's note of a value before a key press
    // What the page saw in the last frame, for the selftest.
    bool stacked = false;
    bool truncated = false;
    bool error = false;
    float availableUnits = 0.0f;
    facet::WidthClass widthClass = facet::WidthClass::Compact;
};

RowsPageState page;

const char* ClassName(facet::WidthClass widthClass)
{
    switch (widthClass)
    {
    case facet::WidthClass::Compact:
        return "Compact";
    case facet::WidthClass::Regular:
        return "Regular";
    default:
        return "Expanded";
    }
}

void Rows(facet::demo::PageCtx& ctx)
{
    using facet::internal::BeginRow;
    using facet::internal::EndRow;
    facet::internal::BeginRowGroup("settings");

    const facet::internal::RowLayout& first = BeginRow("Vertical sync");
    page.stacked = first.stacked;
    facet::demo::ProtoToggle(ctx, "##value", &page.vsync, "rows.vsync");
    EndRow();

    BeginRow("Volume", {.description = "A row with a description line under its label."});
    facet::demo::ProtoSlider(ctx, "##value", &page.volume, {}, "rows.volume");
    EndRow();

    // Modified while it differs from the default; the reset button returns it.
    const bool modified = page.scale != kDefaultScale;
    const facet::internal::RowLayout scaleRow =
        BeginRow("Interface scale", {.helpText = "Percent. A changed value shows a marker and a reset button.",
                                     .modified = modified,
                                     .resettable = true});
    const ImGuiID resetId = ImGui::GetID("##reset");  // the row's reset button, under the row's id
    if (ctx.capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    facet::demo::ProtoSlider(ctx, "##value", &page.scale, {.min = 50.0, .max = 200.0, .step = 5.0}, "rows.scale");
    if (EndRow())
    {
        page.scale = kDefaultScale;
        ++page.resets;
    }
    if (modified)
    {
        const float size = facet::ControlHeight(facet::ControlSize::Regular);
        const ImVec2 min(scaleRow.control.Max.x + facet::Px(facet::T().spacing.sm), scaleRow.control.Min.y);
        facet::demo::ProbeRect(ctx, "rows.reset", resetId, min, ImVec2(min.x + size, min.y + size));
    }

    const facet::internal::RowLayout& longRow = BeginRow(kLongLabel);
    page.truncated = longRow.truncated;
    facet::demo::ProbeRect(ctx, "rows.longlabel", 0, longRow.label.Min, longRow.label.Max);
    facet::demo::ProtoToggle(ctx, "##value", &page.longToggle, "rows.long");
    EndRow();

    BeginRow("Lower bound");
    facet::demo::ProtoSlider(ctx, "##value", &page.lower, {.min = 0.0, .max = 100.0, .step = 1.0}, "rows.lower");
    EndRow();
    page.error = page.lower > page.upper;
    BeginRow("Upper bound", {.error = page.error ? "The upper bound is below the lower bound." : nullptr,
                             .helpText = "Must not be below the lower bound."});
    facet::demo::ProtoSlider(ctx, "##value", &page.upper, {.min = 0.0, .max = 100.0, .step = 1.0}, "rows.upper");
    EndRow();

    facet::internal::EndRowGroup();
}

} // namespace

FACET_DEMO_PAGE("rows", "Rows and width classes", "Blocks", 30, "default", "narrow", "error", "focused")
{
    facet::demo::PageHeader("Rows and width classes",
                            "Labelled rows with a shared label column, and the width classes that let a layout "
                            "stack its columns in narrow regions.");
    Note("Prototype controls in rows from the row layout block. Labels share the column of the widest one, end in an "
         "ellipsis with the full text on hover or keyboard focus when they do not fit, and go above their control "
         "when the region "
         "gets narrow.");

    const float available = facet::AvailableWidthUnits();
    if (page.region <= 0.0)
        page.region = available;
    if (ctx.capture)
    {
        page.region = ctx.captureState == kNarrow ? kNarrowRegion : available;
        page.lower = ctx.captureState == kError ? 80.0 : 20.0;
        page.scale = ctx.captureState == kFocused ? 150.0 : kDefaultScale;
    }
    page.region = std::clamp(page.region, kMinRegion, std::max(kMinRegion, static_cast<double>(available)));

    SectionHeader("Region width");
    facet::internal::BeginRow("Width");
    facet::demo::ProtoSlider(ctx, "##width", &page.region,
                             {.min = kMinRegion, .max = std::max(kMinRegion + 1.0, static_cast<double>(available)),
                              .step = 1.0},
                             "rows.width");
    facet::internal::EndRow();

    // The region: a child as wide as the slider says, its height following its rows.
    ImGui::BeginChild("##region", ImVec2(facet::Px(static_cast<float>(page.region)), 0.0f),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    page.availableUnits = facet::AvailableWidthUnits();
    page.widthClass = facet::AvailableWidthClass();
    Note("This region: %.0f units of content width, width class %s.", page.availableUnits, ClassName(page.widthClass));
    Rows(ctx);
    ImGui::EndChild();
    ctx.RevealInCapture(0.5f);
    Note("Reset pressed %d times. Lower bound %.0f, upper bound %.0f.", page.resets, page.lower, page.upper);
}

FACET_DEMO_SELFTEST("rows")
{
    using T = facet::demo::Target;
    test.Do([] { page = {}; });

    // Narrow: the rows stack.
    test.Drag({T::Item("rows.width.track", 1.0f), T::Item("rows.width.track", -0.5f)});
    test.ExpectNear("the width slider reaches its minimum", [] { return page.region; }, kMinRegion, 0.0);
    test.Expect("a narrow region is Compact", [] { return page.widthClass == facet::WidthClass::Compact; });
    test.Expect("and its rows stack", [] { return page.stacked; });
    test.Click("rows.width");
    test.KeyPress(ImGuiKey_End);
    test.Expect("at full width the rows stack only when the label column and a control do not fit", [] {
        const facet::Theme& t = facet::T();
        return page.stacked == (page.availableUnits < t.layout.labelColumn + t.layout.minRowControl);
    });

    // Controls in rows work by mouse and keyboard.
    test.Click("rows.vsync");
    test.Expect("a click turns vertical sync off", [] { return !page.vsync; });
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus where it is
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space turns it on again", [] { return page.vsync; });
    test.Click("rows.volume.track");
    test.ExpectNear("a click sets the volume", [] { return page.volume; }, 0.5, 0.01);
    test.Do([] { page.clicked = page.volume; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right steps it up to the next hundredth",
                [] { return page.volume > page.clicked && page.volume - page.clicked <= 0.01 + 1e-9; });

    // Modified value and its reset button, by mouse and by keyboard.
    test.Click("rows.scale");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End sets the scale to 200", [] { return page.scale; }, 200.0, 0.0);
    test.Click("rows.reset");
    test.ExpectNear("the reset button restores the default", [] { return page.scale; }, kDefaultScale, 0.0);
    test.Click("rows.scale");
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("rows.reset");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Tab reaches the reset button and Enter presses it", [] { return page.scale; }, kDefaultScale, 0.0);
    test.ExpectNear("two resets", [] { return page.resets; }, 2.0, 0.0);

    // A long label is truncated and shows its full text on hover. The region is set to a width
    // where the label cannot fit, so the check holds at every scale and window size.
    test.Do([] { page.region = kTruncationRegion; });
    test.Wait(2);
    test.Expect("the long label is truncated", [] { return page.truncated; });
    test.Hover(T::Item("rows.longlabel"), 0.3f);
    test.Expect("hovering it shows the full text", [] {
        ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
        return tooltip != nullptr && tooltip->Active;
    });
    // The same text for keyboard users: the visible focus on the row's control shows it too,
    // with the pointer away from the label.
    test.Click("rows.long");
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus where it is
    test.ExpectFocused("rows.long");
    test.Expect("keyboard focus on its control shows the full text", [] {
        ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
        return tooltip != nullptr && tooltip->Active;
    });
    test.KeyPress(ImGuiKey_Space);
    test.Expect("the toggle in that row works by mouse and keyboard", [] { return !page.longToggle; });
    // The navigation titles follow the same rule. This page's title is cut in narrow windows at
    // large scales; there the keyboard focus on its row shows it in full.
    test.Click("nav.rows");
    test.Hover(T::Item("rows.width"), 0.1f);
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus where it is
    test.ExpectFocused("nav.rows");
    test.Expect("a cut navigation title shows in full on keyboard focus", [] {
        ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
        return !facet::demo::NavTitleCut("rows") || (tooltip != nullptr && tooltip->Active);
    });

    // Validation: the error line appears when the bounds cross.
    test.Expect("no error while the bounds are in order", [] { return !page.error; });
    test.Click("rows.lower");
    test.KeyPress(ImGuiKey_End);
    test.Expect("a lower bound above the upper one shows the error", [] { return page.error; });
    test.KeyPress(ImGuiKey_Home);
    test.Expect("and it goes away when fixed", [] { return !page.error; });
}
