// MultiSlider: N ordered handles on one track (the multi-handle block, src/internal/multi_handle.h)
// dividing it into segments. The fixed overloads keep the handle count; the growable ones add a
// handle on a double click on the track and remove one with Delete or Backspace, or when it is
// dragged off the track and released there.
//
// Every handle has a key kept under the widget id, so its item id, focus and animations stay with
// the same handle when another one is inserted or removed before it.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/multi_handle.h"
#include "internal/state_storage.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

using internal::kMaxHandles;

// Stretches of more ticks than this are left unmarked.
constexpr int kMultiSliderMaxTicks = 21;

// The handle keys of one multi slider.
struct MultiSliderKeys
{
    ImGuiID keys[kMaxHandles] = {};
    int count = -1;        // handles the keys were made for; another count from the caller starts over
    ImGuiID next = 1;      // the next fresh key
    ImGuiID offTrack = 0;  // key of the handle being dragged off the track, 0 none
    // The values at the last press on the track, and whether it landed on the empty track: the
    // first click of a double click moves the nearest handle there, and the double click puts
    // it back before it adds a handle.
    double pressValues[kMaxHandles] = {};
    int pressCount = -1;
    bool pressOnEmpty = false;
};

// The last insert and removal, read by MultiSliderAddedHandle and MultiSliderRemovedHandle.
struct MultiSliderEvents
{
    int frame = -1;
    int added = -1;
    int removed = -1;
};

MultiSliderEvents& Events()
{
    static MultiSliderEvents events;
    return events;
}

// The item id the multi-handle block gives the handle with `key`.
ImGuiID MultiSliderHandleId(ImGuiID widget, ImGuiID key)
{
    return ImHashData(&key, sizeof(key), widget);
}

// Runs the widget on `values` (the first *count of `capacity` entries, ascending). `growable`
// allows adding and removing. Returns true when a value was written or a handle added or removed;
// *count then holds the new count and the handles an edit moved (all of them after an insert or a
// removal) are written.
bool MultiSliderCore(const char* label, double* values, int* count, int capacity, double min, double max,
                     bool integer, bool growable, const MultiSliderOptions& o)
{
    MultiSliderEvents& events = Events();
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    events = {ImGui::GetFrameCount(), -1, -1};
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    internal::ControlTextScope text(m.control);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const bool vertical = o.orientation == Orientation::Vertical;
    int n = count != nullptr ? std::clamp(*count, 0, std::min(capacity, kMaxHandles)) : 0;

    internal::ValueMapping mapping;
    mapping.min = min;
    mapping.max = max;
    mapping.scale = o.scale;
    mapping.step = o.step;
    mapping.integer = integer;
    mapping = internal::NormalizeMapping(mapping);
    const bool readOnly = o.readOnly || count == nullptr || !(mapping.max > mapping.min) ||
                          (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    internal::HandleConstraints constraints;
    constraints.min = mapping.min;
    constraints.max = mapping.max;
    constraints.minGap = std::isnan(o.minGap) ? 0.0 : std::max(0.0, o.minGap);
    constraints.collision =
        o.collision == HandleCollision::Push ? internal::CollisionMode::Push : internal::CollisionMode::Clamp;
    const bool canAdd = growable && o.allowAdd && !readOnly && !disabled;
    const int minHandles = std::max(0, o.minHandles);
    const bool canRemove = growable && o.allowRemove && !readOnly && !disabled && n > minHandles;
    int limit = std::min(capacity, kMaxHandles);
    if (o.maxHandles > 0)
        limit = std::min(limit, o.maxHandles);

    MultiSliderKeys& keys = internal::GetState<MultiSliderKeys>(id);
    if (keys.count != n)
    {
        for (int i = 0; i < n; ++i)
            keys.keys[i] = keys.next++;
        keys.count = n;
        keys.offTrack = 0;
    }

    const float width = vertical ? m.control.height : internal::ResolveItemWidth(o.width, next, kFill);
    const float length = vertical ? Px(o.height > 0.0f ? o.height : T().metrics.faderLength) : 0.0f;
    const internal::SliderFrame frame = internal::LayoutSliderFrame(window->DC.CursorPos, label, width, 0.0f, m,
                                                                    o.orientation, length);

    // The values as drawn: repaired, written only when an edit moves them.
    double drawn[kMaxHandles];
    for (int i = 0; i < n; ++i)
        drawn[i] = std::isnan(values[i]) ? mapping.min : values[i];
    internal::ConstrainHandles(std::span<double>(drawn, static_cast<size_t>(n)), constraints);
    double edited[kMaxHandles];
    std::copy(drawn, drawn + n, edited);

    ImGui::BeginGroup();
    ImGui::ItemSize(frame.widget.GetSize());
    internal::ItemState handles[kMaxHandles];
    internal::MultiHandleResult r;
    ImGuiContext& g = *GImGui;
    if (n > 0)
    {
        internal::MultiHandleOptions options;
        options.constraints = constraints;
        options.drag.mapping = mapping;
        options.drag.orientation = o.orientation;
        options.drag.readOnly = readOnly;
        options.handleSize = frame.knob;
        options.spanDrag = o.dragSpan;
        options.removeKeys = canRemove;
        options.keys = std::span<const ImGuiID>(keys.keys, static_cast<size_t>(n));
        const ImVec2 cursor = window->DC.CursorPos;
        ImGui::SetCursorScreenPos(frame.control.Min);
        r = internal::MultiHandleBehavior(id, frame.control, frame.track, std::span<double>(edited, static_cast<size_t>(n)),
                                          std::span<internal::ItemState>(handles, static_cast<size_t>(n)), options);
        window->DC.CursorPos = cursor;
    }
    else
    {
        // No handles: the track alone, which takes the double click that adds one.
        internal::ItemOptions trackOptions;
        trackOptions.press = internal::PressMode::Click;
        trackOptions.navigable = false;
        r.track = internal::PartBehavior(window->GetID("##track"), frame.control, trackOptions);
    }
    bool changed = false;
    for (int i = 0; i < n; ++i)
        changed = changed || edited[i] != drawn[i];
    int added = -1;
    int removed = -1;

    // Removal: Delete or Backspace on the focused handle, or a handle released off the track.
    int removeIndex = canRemove ? r.removeRequested : -1;
    if (canRemove && r.activeHandle >= 0 && g.ActiveIdSource == ImGuiInputSource_Mouse)
    {
        // Two control heights from the track line, well past the row, so a sloppy drag along the
        // track never removes a handle.
        const ImVec2 mouse = g.IO.MousePos;
        const float off = vertical ? std::fabs(mouse.x - frame.track.Min.x) : std::fabs(mouse.y - frame.track.Min.y);
        keys.offTrack = ImGui::IsMousePosValid(&mouse) && off > m.control.height * 2.0f ? keys.keys[r.activeHandle] : 0;
    }
    else if (keys.offTrack != 0)
    {
        for (int i = 0; i < n && removeIndex < 0; ++i)
        {
            if (keys.keys[i] == keys.offTrack && g.ActiveId == 0 && canRemove)
                removeIndex = i;
        }
        keys.offTrack = 0;
    }
    if (removeIndex >= 0)
    {
        const int before = n;
        n = internal::RemoveHandle(std::span<double>(edited, static_cast<size_t>(before)), before, removeIndex, minHandles);
        if (n < before)
        {
            for (int i = removeIndex; i + 1 < before; ++i)
                keys.keys[i] = keys.keys[i + 1];
            keys.count = n;
            removed = removeIndex;
            changed = true;
            if (g.ActiveId != 0)
                ImGui::ClearActiveID();
            // The focus goes to the neighbour that took the removed handle's place.
            if (n > 0)
                ImGui::SetFocusID(MultiSliderHandleId(id, keys.keys[std::min(removeIndex, n - 1)]), window);
        }
    }
    // Adding: a double click on the empty track (not on a handle, not on the span).
    const bool doubleClick = r.track.pressed && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const bool addHere = canAdd && removed < 0 && n < limit && doubleClick && keys.pressOnEmpty && keys.pressCount == n;
    if (r.track.pressed && !doubleClick && r.track.forced == internal::ForcedItemState::None)
    {
        // Where a first click lands, measured on the values before it moved anything.
        const float along = static_cast<float>(internal::TrackToFraction(frame.track, o.orientation, g.IO.MousePos));
        const float trackLength = vertical ? frame.track.GetHeight() : frame.track.GetWidth();
        bool empty = true;
        for (int i = 0; i < n; ++i)
        {
            const float at = static_cast<float>(internal::ValueToFraction(mapping, drawn[i]));
            empty = empty && std::fabs(at - along) * trackLength > frame.knob * 0.5f;
        }
        if (o.dragSpan && n >= 2)
        {
            const double a = internal::ValueToFraction(mapping, drawn[0]);
            const double b = internal::ValueToFraction(mapping, drawn[n - 1]);
            empty = empty && !(along > a && along < b);
        }
        keys.pressOnEmpty = empty;
        keys.pressCount = n;
        std::copy(drawn, drawn + n, keys.pressValues);
    }
    if (addHere)
    {
        const double at =
            internal::FractionToValue(mapping, internal::TrackToFraction(frame.track, o.orientation, g.IO.MousePos));
        // The first click of the double click moved the nearest handle there: it goes back.
        std::copy(keys.pressValues, keys.pressValues + n, edited);
        keys.pressCount = -1;
        if (g.ActiveId != 0)
            ImGui::ClearActiveID();
        const int index = internal::InsertHandle(std::span<double>(edited, static_cast<size_t>(kMaxHandles)), n, at,
                                                 constraints);
        if (index >= 0)
        {
            for (int i = n; i > index; --i)
                keys.keys[i] = keys.keys[i - 1];
            keys.keys[index] = keys.next++;
            ++n;
            keys.count = n;
            added = index;
            changed = true;
        }
        else
        {
            changed = false;
        }
    }

    // ---- Draw ----
    const internal::SliderLook look = internal::PickSliderLook(disabled || r.track.disabled, readOnly);
    const bool mixedShown = o.mixed && !changed;
    ImDrawList* drawList = window->DrawList;
    if (ImGui::IsRectVisible(frame.widget.Min, frame.widget.Max))
    {
        internal::DrawSliderTrack(drawList, frame.track, o.orientation, m, 0.0, 0.0, look, o.error != nullptr);
        if (!mixedShown && n > 0)
        {
            if (o.segmentColors.empty())
            {
                // One accent fill: from the first handle to the last (from the start with one handle).
                const double a = n > 1 ? internal::ValueToFraction(mapping, edited[0]) : 0.0;
                internal::DrawSliderTrack(drawList, frame.track, o.orientation, m, a,
                                          internal::ValueToFraction(mapping, edited[n - 1]), look, o.error != nullptr);
            }
            else
            {
                // n handles make n + 1 segments, each in its data color (0: the chart palette color).
                for (int k = 0; k <= n; ++k)
                {
                    const double a = k == 0 ? 0.0 : internal::ValueToFraction(mapping, edited[k - 1]);
                    const double b = k == n ? 1.0 : internal::ValueToFraction(mapping, edited[k]);
                    ImU32 color = o.segmentColors[static_cast<size_t>(k) % o.segmentColors.size()];
                    if (color == 0)
                        color = ToU32(internal::SeriesColor(T().colors, k));
                    if (look == internal::SliderLook::Disabled)
                        color = ToU32(T().colors.fill[3]);
                    internal::DrawSliderTrackStretch(drawList, frame.track, o.orientation, m, a, b, color);
                }
            }
        }
        if (o.step > 0.0)
        {
            double ticks[kMultiSliderMaxTicks + 1];
            const int tickCount = internal::SliderStepTicks(mapping, ticks, kMultiSliderMaxTicks);
            internal::DrawSliderTicks(drawList, frame.track, o.orientation, m,
                                      std::span<const double>(ticks, static_cast<size_t>(tickCount)), look);
        }
        // After an insert or a removal the item states no longer match the handles: they are
        // drawn from the next frame on.
        if (added < 0 && removed < 0)
        {
            int order[kMaxHandles];
            const int top = r.activeHandle >= 0 ? r.activeHandle : r.focusedHandle;
            internal::HandleDrawOrder(std::span<const double>(edited, static_cast<size_t>(n)), constraints, top,
                                      std::span<int>(order, static_cast<size_t>(n)));
            char textBuffer[64];
            for (int k = 0; k < n; ++k)
            {
                const int i = order[k];
                const ImRect knob = internal::SliderKnobRect(frame.track, o.orientation, frame.knob,
                                                             internal::ValueToFraction(mapping, edited[i]));
                if (mixedShown)
                {
                    internal::DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, handles[i].focusAmount, 0,
                                            internal::RingShape::Round);
                    continue;
                }
                // A handle dragged off the track takes the disabled look: the release removes it.
                const bool leaving = keys.offTrack != 0 && keys.keys[i] == keys.offTrack;
                internal::DrawSliderKnob(drawList, knob, handles[i], leaving ? internal::SliderLook::Disabled : look);
                internal::FormatNumber(textBuffer, sizeof(textBuffer), edited[i], o.format, o.formatter,
                                       o.formatterUser, integer, false);
                internal::DrawSliderBubble(drawList, knob, textBuffer, o.orientation,
                                           internal::SliderBubbleAmount(MultiSliderHandleId(id, keys.keys[i]), handles[i]));
            }
        }
    }
    const bool lookDisabled = look == internal::SliderLook::Disabled;
    if (!vertical && frame.label.text != nullptr)
        internal::DrawFrontLabel(drawList, frame.label, frame.labelPos, m.control.size, lookDisabled);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    bool focusVisible = false;
    if (added < 0 && removed < 0)
    {
        for (int i = 0; i < n; ++i)
            focusVisible = focusVisible || handles[i].focusVisible;
    }
    internal::CutTextTooltip(id, frame.widget, r.track.hovered, focusVisible, label, labelEnd,
                             vertical ? labelEnd != label : frame.label.shown.truncated);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
    {
        const float x = vertical ? frame.widget.Min.x : frame.control.Min.x;
        internal::DrawControlMessage(message, ImVec2(x, frame.widget.Max.y), frame.widget.Max.x - x, lookDisabled, id,
                                     frame.widget);
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    if (!changed || count == nullptr)
        return false;
    for (int i = 0; i < n; ++i)
    {
        if (added >= 0 || removed >= 0 || edited[i] != drawn[i])
            values[i] = integer ? std::round(edited[i]) : edited[i];
    }
    *count = n;
    events.added = added;
    events.removed = removed;
    return true;
}

// A fixed span: the count stays, allowAdd and allowRemove are a usage error.
template <class Value>
bool RunFixed(const char* label, std::span<Value> values, double min, double max, const MultiSliderOptions& o)
{
    if (o.allowAdd || o.allowRemove)
        internal::ReportUsageError("MultiSlider: allowAdd and allowRemove need the storage and count or the vector overload");
    double buffer[kMaxHandles];
    int count = std::min(static_cast<int>(values.size()), kMaxHandles);
    for (int i = 0; i < count; ++i)
        buffer[i] = internal::SliderValueIn(values[static_cast<size_t>(i)]);
    double before[kMaxHandles];
    std::copy(buffer, buffer + count, before);
    if (!MultiSliderCore(label, buffer, &count, count, min, max, std::is_integral_v<Value>, false, o))
        return false;
    for (int i = 0; i < count; ++i)
    {
        if (buffer[i] == before[i])
            continue;
        if constexpr (std::is_integral_v<Value>)
            values[static_cast<size_t>(i)] = static_cast<Value>(std::llround(buffer[i]));
        else
            values[static_cast<size_t>(i)] = static_cast<Value>(buffer[i]);
    }
    return true;
}

} // namespace

bool MultiSlider(const char* label, std::span<float> values, float min, float max, const MultiSliderOptions& options)
{
    return RunFixed(label, values, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max), options);
}

bool MultiSlider(const char* label, std::span<int> values, int min, int max, const MultiSliderOptions& options)
{
    return RunFixed(label, values, static_cast<double>(min), static_cast<double>(max), options);
}

bool MultiSlider(const char* label, std::span<float> storage, int* count, float min, float max,
                 const MultiSliderOptions& options)
{
    double buffer[kMaxHandles];
    const int capacity = std::min(static_cast<int>(storage.size()), kMaxHandles);
    int n = count != nullptr ? std::clamp(*count, 0, capacity) : 0;
    for (int i = 0; i < n; ++i)
        buffer[i] = internal::SliderFloatToDouble(storage[static_cast<size_t>(i)]);
    double before[kMaxHandles];
    std::copy(buffer, buffer + n, before);
    const int countBefore = n;
    if (!MultiSliderCore(label, buffer, count != nullptr ? &n : nullptr, capacity, internal::SliderFloatToDouble(min),
                         internal::SliderFloatToDouble(max), false, true, options))
        return false;
    for (int i = 0; i < n; ++i)
    {
        if (n != countBefore || buffer[i] != before[i])
            storage[static_cast<size_t>(i)] = static_cast<float>(buffer[i]);
    }
    *count = n;
    return true;
}

bool MultiSlider(const char* label, std::vector<float>* values, float min, float max, const MultiSliderOptions& options)
{
    double buffer[kMaxHandles];
    int n = values != nullptr ? std::min(static_cast<int>(values->size()), kMaxHandles) : 0;
    for (int i = 0; i < n; ++i)
        buffer[i] = internal::SliderFloatToDouble((*values)[static_cast<size_t>(i)]);
    double before[kMaxHandles];
    std::copy(buffer, buffer + n, before);
    const int countBefore = n;
    if (!MultiSliderCore(label, buffer, values != nullptr ? &n : nullptr, kMaxHandles,
                         internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max), false, true, options))
        return false;
    // Allocates only when a handle was added; an allocation failure keeps the old values.
    if (n > countBefore)
    {
        try
        {
            values->resize(static_cast<size_t>(n));
        }
        catch (...)
        {
            return false;
        }
    }
    for (int i = 0; i < n; ++i)
    {
        if (n != countBefore || buffer[i] != before[i])
            (*values)[static_cast<size_t>(i)] = static_cast<float>(buffer[i]);
    }
    if (n < countBefore)
        values->resize(static_cast<size_t>(n));
    return true;
}

void internal::ResetMultiSliderEvents()
{
    Events() = {};
}

int MultiSliderAddedHandle()
{
    const MultiSliderEvents& events = Events();
    if (ImGui::GetCurrentContext() == nullptr || events.frame != ImGui::GetFrameCount())
        return -1;
    return events.added;
}

int MultiSliderRemovedHandle()
{
    const MultiSliderEvents& events = Events();
    if (ImGui::GetCurrentContext() == nullptr || events.frame != ImGui::GetFrameCount())
        return -1;
    return events.removed;
}

} // namespace facet
