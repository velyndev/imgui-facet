// Demo page: Multi-handle. Prototype range sliders built only from the item behaviour, the focus
// ring, the value drag core and the multi-handle model: two handles with a minimum gap that
// stop at each other or push each other, logarithmic, stepped, mirrored and vertical ones, and
// one with three handles that can be added and removed.

#include <cmath>
#include <cstdio>
#include <span>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/multi_handle.h"
#include "internal/row_layout.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::ProtoRangeOptions;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kDragging,
    kFocused,
    kSpan,
    kDisabled,
};

enum Range
{
    kClamp,
    kPush,
    kLog,
    kStepped,
    kMirror,
    kVertical,
    kRangeCount,
};

struct RangeSpec
{
    const char* label;
    const char* probe;
    const char* format;
    double low;
    double high;
    ProtoRangeOptions options;
};

const RangeSpec kSpecs[kRangeCount] = {
    {"Stop at the gap", "range.clamp", "%.0f", 20.0, 60.0, {.slider = {.min = 0.0, .max = 100.0}, .minGap = 10.0}},
    {"Push", "range.push", "%.0f", 30.0, 50.0, {.slider = {.min = 0.0, .max = 100.0}, .minGap = 10.0, .push = true}},
    {"Logarithmic", "range.log", "%.0f", 100.0, 2000.0,
     {.slider = {.min = 20.0, .max = 20000.0, .scale = facet::ValueScale::Logarithmic}}},
    {"Steps of 10", "range.stepped", "%.0f", 20.0, 70.0,
     {.slider = {.min = 0.0, .max = 100.0, .step = 10.0}, .minGap = 10.0}},
    {"Mirrored", "range.mirror", "%.0f", 30.0, 70.0,
     {.slider = {.min = 0.0, .max = 100.0}, .minGap = 10.0, .mirror = true}},
    {"Vertical", "range.vertical", "%.0f", 25.0, 75.0,
     {.slider = {.min = 0.0, .max = 100.0, .orientation = facet::Orientation::Vertical}, .minGap = 10.0}},
};

constexpr int kMaxMulti = 6;
constexpr int kMinMulti = 2;
constexpr double kMultiGap = 5.0;

struct MultiPageState
{
    double values[kRangeCount][2] = {};
    int commits[kRangeCount] = {};
    int cancels[kRangeCount] = {};
    double multi[kMaxMulti] = {};
    ImGuiID keys[kMaxMulti] = {};  // one per handle, so focus stays on a handle when others come and go
    ImGuiID nextKey = 1;
    int multiCount = 0;
    bool full = false;  // the last add found no room
    bool initialized = false;
    double noted = 0.0;  // the selftest's note of a value before a key press
};

MultiPageState page;

void Reset()
{
    page = {};
    for (int i = 0; i < kRangeCount; ++i)
    {
        page.values[i][0] = kSpecs[i].low;
        page.values[i][1] = kSpecs[i].high;
    }
    page.multi[0] = 20.0;
    page.multi[1] = 50.0;
    page.multi[2] = 80.0;
    page.multiCount = 3;
    for (int i = 0; i < page.multiCount; ++i)
        page.keys[i] = page.nextKey++;
    page.initialized = true;
}

void ForceForCapture(facet::demo::PageCtx& ctx)
{
    if (!ctx.capture)
        return;
    switch (ctx.captureState)
    {
    case kDragging:
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Dragging, 1);
        break;
    case kFocused:
        ctx.ForceNextItemState(ForcedItemState::Focused, 0);
        break;
    case kSpan:
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Dragging, 2);
        break;
    default:
        break;
    }
}

void RangeRow(facet::demo::PageCtx& ctx, int index)
{
    const RangeSpec& spec = kSpecs[index];
    char readout[160];
    char low[32];
    char high[32];
    std::snprintf(low, sizeof(low), spec.format, page.values[index][0]);
    std::snprintf(high, sizeof(high), spec.format, page.values[index][1]);
    std::snprintf(readout, sizeof(readout), "%s to %s. Committed %d, canceled %d.", low, high, page.commits[index],
                  page.cancels[index]);
    // Disabled around the whole row, so its label goes quiet with the control.
    const bool disabled = ctx.capture && ctx.captureState == kDisabled && index == kClamp;
    if (disabled)
        ImGui::BeginDisabled();
    facet::internal::BeginRow(spec.label, {.helpText = readout});
    if (index == kClamp)
        ForceForCapture(ctx);
    facet::demo::ProtoRange(ctx, "##value", page.values[index], 2, spec.options, spec.probe);
    page.commits[index] += facet::IsItemEditCommitted() ? 1 : 0;
    page.cancels[index] += facet::IsItemEditCanceled() ? 1 : 0;
    facet::internal::EndRow();
    if (disabled)
        ImGui::EndDisabled();
    // The capture states show on the first slider: keep it in the image at any size.
    if (index == kClamp)
        ctx.RevealInCapture(0.4f);
}

// Adds a handle in the middle of the widest gap.
void AddHandle()
{
    double* values = page.multi;
    int widest = 0;
    double gap = 0.0;
    double previous = 0.0;
    for (int i = 0; i <= page.multiCount; ++i)
    {
        const double next = i < page.multiCount ? values[i] : 100.0;
        if (next - previous > gap)
        {
            gap = next - previous;
            widest = i;
        }
        previous = next;
    }
    const double start = widest > 0 ? values[widest - 1] : 0.0;
    facet::internal::HandleConstraints constraints;
    constraints.min = 0.0;
    constraints.max = 100.0;
    constraints.minGap = kMultiGap;
    const int index = facet::internal::InsertHandle(std::span<double>(values, kMaxMulti), page.multiCount,
                                                    start + gap * 0.5, constraints);
    page.full = index < 0;
    if (index >= 0)
    {
        for (int i = page.multiCount; i > index; --i)
            page.keys[i] = page.keys[i - 1];
        page.keys[index] = page.nextKey++;
        ++page.multiCount;
    }
}

void RemoveHandleAt(int index)
{
    const int count =
        facet::internal::RemoveHandle(std::span<double>(page.multi, kMaxMulti), page.multiCount, index, kMinMulti);
    if (count == page.multiCount)
        return;
    for (int i = index; i + 1 < page.multiCount; ++i)
        page.keys[i] = page.keys[i + 1];
    page.multiCount = count;
}

} // namespace

FACET_DEMO_PAGE("multi-handle", "Multi-handle", "Blocks", 60, "default", "dragging", "focused", "span", "disabled")
{
    if (!page.initialized)
        Reset();
    facet::demo::PageHeader("Multi-handle",
                            "Several handles on one track. Each is its own item with its own focus and Tab stop; "
                            "they never cross, keep a minimum gap or push each other, and the span between them "
                            "drags both.");
    Note("Prototype range sliders built only from the item behaviour, focus ring, value drag and multi-handle "
         "blocks. A click on the track moves the nearest handle; a drag between the handles moves the span. Tab "
         "moves between handles, the arrows move the focused one, Escape restores the values from before the drag "
         "or key press.");

    SectionHeader("Two handles");
    facet::internal::BeginRowGroup("ranges");
    for (int i = kClamp; i <= kMirror; ++i)
        RangeRow(ctx, i);
    facet::internal::EndRowGroup();

    SectionHeader("Vertical");
    RangeRow(ctx, kVertical);

    SectionHeader("Three handles and more");
    char readout[160];
    int written = std::snprintf(readout, sizeof(readout), "Values:");
    for (int i = 0; i < page.multiCount && written > 0 && written < static_cast<int>(sizeof(readout)); ++i)
        written += std::snprintf(readout + written, sizeof(readout) - static_cast<size_t>(written), " %.0f",
                                 page.multi[i]);
    facet::internal::BeginRow("Handles", {.description = "Delete or Backspace removes the focused handle; at least "
                                                         "two stay.",
                                          .helpText = readout});
    int remove = -1;
    ProtoRangeOptions options;
    options.slider = {.min = 0.0, .max = 100.0};
    options.minGap = kMultiGap;
    options.spanDrag = false;
    options.removeKeys = true;
    options.keys = std::span<const ImGuiID>(page.keys, static_cast<size_t>(page.multiCount));
    facet::demo::ProtoRange(ctx, "##value", page.multi, page.multiCount, options, "range.multi", &remove);
    if (remove >= 0)
        RemoveHandleAt(remove);
    facet::internal::EndRow();
    ImGui::BeginDisabled(page.multiCount >= kMaxMulti);
    if (facet::demo::ProtoButton(ctx, "Add a handle", "range.add"))
        AddHandle();
    ImGui::EndDisabled();
    if (page.full)
        Note("No room for another handle at the minimum gap.");
}

FACET_DEMO_SELFTEST("multi-handle")
{
    using T = facet::demo::Target;
    test.Do([] { Reset(); });

    // Stop at the gap: a drag into the other handle stops 10 before it.
    test.Drag({T::Item("range.clamp.track", 0.2f), T::Item("range.clamp.track", 0.4f)});
    test.ExpectNear("the low handle follows the drag", [] { return page.values[kClamp][0]; }, 40.0, 1.0);
    test.ExpectNear("one committed edit", [] { return page.commits[kClamp]; }, 1.0, 0.0);
    test.Drag({T::Item("range.clamp.track", 0.4f), T::Item("range.clamp.track", 0.95f)});
    test.ExpectNear("it stops one gap before the high handle", [] { return page.values[kClamp][0]; }, 50.0, 0.0);
    test.ExpectNear("the high handle stays", [] { return page.values[kClamp][1]; }, 60.0, 0.0);

    // A click on the track moves the nearer handle; a drag between the handles moves both.
    test.MouseMove(T::Item("range.clamp.track", 0.9f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click near the high handle moves it there", [] { return page.values[kClamp][1]; }, 90.0, 1.0);
    test.Drag({T::Item("range.clamp.track", 0.7f), T::Item("range.clamp.track", 0.5f)});
    test.ExpectNear("the span drag moves the low handle", [] { return page.values[kClamp][0]; }, 30.0, 1.0);
    test.ExpectNear("and the high one by the same amount", [] { return page.values[kClamp][1]; }, 70.0, 1.0);

    // Keyboard: each handle a Tab stop, arrows on the focused one, Escape.
    test.Click("range.clamp.h0");
    test.ExpectFocused("range.clamp.h0");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End stops the low handle at the gap", [] { return page.values[kClamp][0]; }, 60.0, 1.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.clamp.h1");
    test.Do([] { page.noted = page.values[kClamp][1]; });
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    test.Expect("Ctrl+Right moves the high handle up to the next tenth", [] {
        const double v = page.values[kClamp][1];
        return v > page.noted && v - page.noted <= 10.0 + 1e-9 && std::fmod(v, 10.0) == 0.0;
    });
    test.Do([] { page.noted = page.values[kClamp][1]; });
    test.KeyDown(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Escape);
    test.KeyUp(ImGuiKey_RightArrow);
    test.Expect("Escape during the held key restores it", [] { return page.values[kClamp][1] == page.noted; });
    test.Expect("and counts as canceled", [] { return page.cancels[kClamp] >= 1; });

    // Push: the low handle pushes the high one to the end.
    test.Drag({T::Item("range.push.track", 0.3f), T::Item("range.push.track", 1.2f)});
    test.ExpectNear("pushed to the end", [] { return page.values[kPush][1]; }, 100.0, 0.0);
    test.ExpectNear("the pusher stops one gap before it", [] { return page.values[kPush][0]; }, 90.0, 0.0);
    test.MouseMove(T::Item("range.push.track", 0.9f));
    test.MouseDown();
    test.MouseMove(T::Item("range.push.track", 0.4f));
    test.KeyPress(ImGuiKey_Escape);
    test.MouseUp();
    test.ExpectNear("Escape restores the low handle", [] { return page.values[kPush][0]; }, 90.0, 0.0);

    // Logarithmic, stepped, mirrored, vertical.
    test.Click("range.log.h1");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("the logarithmic end is reachable", [] { return page.values[kLog][1]; }, 20000.0, 0.0);
    test.Drag({T::Item("range.stepped.track", 0.2f), T::Item("range.stepped.track", 0.43f)});
    test.ExpectNear("the stepped handle lands on a step", [] { return page.values[kStepped][0]; }, 40.0, 0.0);
    test.Drag({T::Item("range.mirror.track", 0.3f), T::Item("range.mirror.track", 0.1f)});
    test.ExpectNear("the mirrored low handle", [] { return page.values[kMirror][0]; }, 10.0, 1.0);
    test.ExpectNear("moves the high one the other way", [] { return page.values[kMirror][1]; }, 90.0, 1.0);
    test.Drag({T::Item("range.vertical.track", 0.5f, 0.25f), T::Item("range.vertical.track", 0.5f, 0.05f)});
    test.ExpectNear("dragging the vertical high handle up", [] { return page.values[kVertical][1]; }, 95.0, 1.5);

    // Three handles: add, remove by key, never below two.
    test.Click("range.add");
    test.ExpectNear("Add puts a fourth handle in the widest gap", [] { return page.multiCount; }, 4.0, 0.0);
    test.Click("range.multi.h0");
    test.KeyPress(ImGuiKey_Delete);
    test.ExpectNear("Delete removes the focused handle", [] { return page.multiCount; }, 3.0, 0.0);
    test.Click("range.multi.h0");
    test.KeyPress(ImGuiKey_Backspace);
    test.Click("range.multi.h0");
    test.KeyPress(ImGuiKey_Delete);
    test.ExpectNear("two handles always stay", [] { return page.multiCount; }, 2.0, 0.0);
}
