// Demo page: Value drag. Prototype sliders built only from the item behaviour, the focus ring
// and the value drag core: linear, integer, stepped with an end off the grid, logarithmic, with
// a snap stop, vertical, read-only and disabled, each with its value and its edit sessions.

#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/row_layout.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::ProtoSliderOptions;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kHovered,
    kDragging,
    kFocused,
    kStates,  // the read-only and disabled sliders
};

enum Slider
{
    kLinear,
    kInteger,
    kStepped,
    kLog,
    kStop,
    kVertical,
    kReadOnly,
    kDisabled,
    kSliderCount,
};

struct SliderSpec
{
    const char* label;
    const char* probe;
    const char* format;
    double initial;
    ProtoSliderOptions options;
};

constexpr double kStops[] = {0.0};

const SliderSpec kSpecs[kSliderCount] = {
    {"Linear, 0 to 1", "drag.linear", "%.3f", 0.5, {}},
    {"Integer, 0 to 100", "drag.integer", "%.0f", 40.0, {.min = 0.0, .max = 100.0, .integer = true}},
    {"Steps of 5, 0 to 42", "drag.stepped", "%.0f", 20.0, {.min = 0.0, .max = 42.0, .step = 5.0}},
    {"Logarithmic, 20 to 20000", "drag.log", "%.1f", 1000.0,
     {.min = 20.0, .max = 20000.0, .scale = facet::ValueScale::Logarithmic}},
    {"Snap stop at 0, -1 to 1", "drag.stop", "%.3f", 0.4, {.min = -1.0, .max = 1.0, .stops = kStops}},
    {"Vertical, 0 to 1", "drag.vertical", "%.3f", 0.25, {.orientation = facet::Orientation::Vertical}},
    {"Read-only", "drag.readonly", "%.3f", 0.7, {.readOnly = true}},
    {"Disabled", "drag.disabled", "%.3f", 0.3, {}},
};

struct DragPageState
{
    double values[kSliderCount] = {};
    int commits[kSliderCount] = {};
    int cancels[kSliderCount] = {};
    bool initialized = false;
};

DragPageState page;

void Reset()
{
    page = {};
    for (int i = 0; i < kSliderCount; ++i)
        page.values[i] = kSpecs[i].initial;
    page.initialized = true;
}

void SliderRow(facet::demo::PageCtx& ctx, int index)
{
    const SliderSpec& spec = kSpecs[index];
    char readout[160];
    char value[48];
    std::snprintf(value, sizeof(value), spec.format, page.values[index]);
    std::snprintf(readout, sizeof(readout), "%s. Committed %d, canceled %d.", value, page.commits[index],
                  page.cancels[index]);
    // Disabled around the whole row, so its label goes quiet with the control.
    if (index == kDisabled)
        ImGui::BeginDisabled();
    facet::internal::BeginRow(spec.label, {.helpText = readout});
    if (index == kLinear && ctx.capture)
    {
        if (ctx.captureState == kHovered)
            ctx.ForceNextItemState(ForcedItemState::Hovered);
        else if (ctx.captureState == kDragging)
            ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Dragging);
        else if (ctx.captureState == kFocused)
            ctx.ForceNextItemState(ForcedItemState::Focused);
    }
    facet::demo::ProtoSlider(ctx, "##value", &page.values[index], spec.options, spec.probe);
    page.commits[index] += facet::IsItemEditCommitted() ? 1 : 0;
    page.cancels[index] += facet::IsItemEditCanceled() ? 1 : 0;
    facet::internal::EndRow();
    if (index == kDisabled)
        ImGui::EndDisabled();
    // The capture states show on the linear slider, and "states" on the read-only and disabled
    // ones further down: keep them in the image at any size.
    if (index == kLinear && ctx.captureState != kStates)
        ctx.RevealInCapture(0.5f);
    if (index == kDisabled && ctx.captureState == kStates)
        ctx.RevealInCapture(0.7f);
}

} // namespace

FACET_DEMO_PAGE("value-drag", "Value drag", "Blocks", 50, "default", "hovered", "dragging", "focused", "states")
{
    if (!page.initialized)
        Reset();
    facet::demo::PageHeader("Value drag",
                            "Mapping a pointer position and key presses onto a value: linear, logarithmic, "
                            "stepped and whole-number ranges, fine and coarse movement, and one edit session per "
                            "drag or key press.");
    Note("Prototype sliders built only from the item behaviour, focus ring and value drag blocks. Drag the knob "
         "(Shift finer, Ctrl to tenths), click the track to jump, or focus a slider and use the arrows (Shift "
         "finer, Ctrl coarser), Page Up and Down, Home and End. The wheel turns a focused slider, or any slider "
         "with Alt held. Escape during a drag or a held key restores the value.");

    SectionHeader("Ranges");
    facet::internal::BeginRowGroup("ranges");
    for (int i = kLinear; i <= kStop; ++i)
        SliderRow(ctx, i);
    facet::internal::EndRowGroup();

    SectionHeader("Vertical");
    SliderRow(ctx, kVertical);

    SectionHeader("States");
    facet::internal::BeginRowGroup("states");
    SliderRow(ctx, kReadOnly);
    SliderRow(ctx, kDisabled);
    facet::internal::EndRowGroup();
}

FACET_DEMO_SELFTEST("value-drag")
{
    using T = facet::demo::Target;
    test.Do([] { Reset(); });

    // Mouse: grab without a jump, jump on the track, drag, Escape.
    test.Drag({T::Item("drag.linear.track", 0.5f), T::Item("drag.linear.track", 0.8f)});
    test.ExpectNear("dragging the knob from the middle to 0.8", [] { return page.values[kLinear]; }, 0.8, 0.01);
    test.ExpectNear("one committed session", [] { return page.commits[kLinear]; }, 1.0, 0.0);
    test.Click("drag.linear.track");
    test.ExpectNear("a click on the track jumps to the pointer", [] { return page.values[kLinear]; }, 0.5, 0.01);
    test.MouseMove(T::Item("drag.linear.track", 0.5f));
    test.MouseDown();
    test.MouseMove(T::Item("drag.linear.track", 0.9f));
    test.KeyPress(ImGuiKey_Escape);
    test.MouseUp();
    test.ExpectNear("Escape during the drag restores the value", [] { return page.values[kLinear]; }, 0.5, 0.01);
    test.ExpectNear("and cancels the session", [] { return page.cancels[kLinear]; }, 1.0, 0.0);

    // Keyboard on the focused slider, from the minimum so every step lands on its grid.
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home goes to the minimum", [] { return page.values[kLinear]; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps a hundredth", [] { return page.values[kLinear]; }, 0.01, 1e-9);
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Shift);
    test.ExpectNear("Shift+Right steps a thousandth", [] { return page.values[kLinear]; }, 0.011, 1e-9);
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+Right steps to the next tenth", [] { return page.values[kLinear]; }, 0.1, 1e-9);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left steps back a hundredth", [] { return page.values[kLinear]; }, 0.09, 1e-9);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End goes to the maximum", [] { return page.values[kLinear]; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_PageDown);
    test.ExpectNear("Page Down moves a tenth down", [] { return page.values[kLinear]; }, 0.9, 0.0001);
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home goes to the minimum", [] { return page.values[kLinear]; }, 0.0, 0.0);
    test.Wheel(T::Item("drag.linear.track", 0.0f), 3.0f);
    test.ExpectNear("three wheel notches over the focused slider", [] { return page.values[kLinear]; }, 0.03, 0.0001);

    // The wheel over a slider that is not focused scrolls instead, unless Alt is held.
    test.Wheel(T::Item("drag.integer"), 1.0f);
    test.ExpectNear("the unfocused slider keeps its value", [] { return page.values[kInteger]; }, 40.0, 0.0);
    test.KeyDown(ImGuiMod_Alt);
    test.Wheel(T::Item("drag.integer"), 1.0f);
    test.KeyUp(ImGuiMod_Alt);
    test.ExpectNear("Alt and the wheel step it", [] { return page.values[kInteger]; }, 41.0, 0.0);

    // Integer and stepped ranges.
    test.Click("drag.integer.track");
    test.ExpectNear("integers land on whole numbers", [] { return page.values[kInteger]; }, 50.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+Right moves ten", [] { return page.values[kInteger]; }, 60.0, 0.0);
    test.Click("drag.stepped");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("the end off the grid is reachable", [] { return page.values[kStepped]; }, 42.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("one step down lands on the last grid point", [] { return page.values[kStepped]; }, 40.0, 0.0);
    test.Drag({T::Item("drag.stepped.track", 40.0f / 42.0f), T::Item("drag.stepped.track", 0.36f)});
    test.ExpectNear("a drag snaps to the steps", [] { return page.values[kStepped]; }, 15.0, 0.0);

    // Logarithmic: a fraction t of the track is 20 * 1000^t, so 0.15 of it is about 56.4.
    test.MouseMove(T::Item("drag.log.track", 0.15f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("0.15 of a logarithmic track", [] { return page.values[kLog]; }, 56.4, 3.0);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End reaches the maximum exactly", [] { return page.values[kLog]; }, 20000.0, 0.0);

    // A snap stop pulls a drag that ends near it.
    test.Drag({T::Item("drag.stop.track", 0.7f), T::Item("drag.stop.track", 0.505f)});
    test.ExpectNear("the drag snaps to the stop at 0", [] { return page.values[kStop]; }, 0.0, 0.0);

    // Vertical: up is more.
    test.Drag({T::Item("drag.vertical.track", 0.5f, 0.75f), T::Item("drag.vertical.track", 0.5f, 0.25f)});
    test.ExpectNear("dragging up raises the vertical slider", [] { return page.values[kVertical]; }, 0.75, 0.02);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up steps it", [] { return page.values[kVertical]; }, 0.76, 0.02);

    // Read-only and disabled never change.
    test.Drag({T::Item("drag.readonly.track", 0.7f), T::Item("drag.readonly.track", 0.1f)});
    test.Click("drag.readonly");
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("read-only keeps its value", [] { return page.values[kReadOnly]; }, 0.7, 0.0);
    test.Drag({T::Item("drag.disabled.track", 0.3f), T::Item("drag.disabled.track", 0.9f)});
    test.ExpectNear("disabled keeps its value", [] { return page.values[kDisabled]; }, 0.3, 0.0);

    // A drag that rests on its target for half a second before the release: the value stays
    // where the drag stopped, and the whole drag is one edit session.
    test.Do([] { Reset(); });
    test.DragAndHold({T::Item("drag.linear.track", 0.5f), T::Item("drag.linear.track", 0.3f)}, 0.25f, 0.5f);
    test.ExpectNear("a drag held on its target ends there", [] { return page.values[kLinear]; }, 0.3, 0.01);
    test.ExpectNear("as one committed session", [] { return page.commits[kLinear]; }, 1.0, 0.0);
}
