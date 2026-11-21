// ColorChannelSlider: a slider over the gradient of one channel of a color, with the other channels
// held. It lays out like a Slider of the sliders family (slider_core.h: the label in front, the
// knob travel inset by half a knob, the fader length when vertical) and runs the value drag core
// with the keys of a Slider; the track is the channel's gradient (color_parts.h), a checkerboard
// under the alpha gradient, and the knob is a color handle filled with the color at its place.

#include "facet/widgets/pickers.h"

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/pickers/color_model.h"
#include "widgets/pickers/color_parts.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{

bool ColorChannelSlider(const char* label, ImVec4* color, const ColorChannelSliderOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(options.size);
    internal::ControlTextScope text(m.control);
    const ImGuiID id = window->GetID(label);
    const bool vertical = options.orientation == Orientation::Vertical;
    const bool readOnly = options.readOnly || color == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const ImVec4 value = color != nullptr ? *color : T().colors.accent;

    const float width = vertical ? m.control.height : internal::ResolveItemWidth(options.width, next, kFill);
    const float length = vertical ? Px(options.height > 0.0f ? options.height : T().metrics.faderLength) : 0.0f;
    const internal::SliderFrame frame =
        internal::LayoutSliderFrame(window->DC.CursorPos, label, width, 0.0f, m, options.orientation, length);

    ImGui::BeginGroup();
    ImGui::ItemSize(frame.widget.GetSize());
    internal::ApplyNextItemData(next);
    internal::ColorHsv hsv = internal::ColorTrackHsv(id, value);
    internal::ColorPartParams params;
    params.readOnly = readOnly;
    params.mixed = options.mixed;
    const internal::ColorPartResult bar = internal::ColorBarPart(id, frame.control, frame.track, frame.knob,
                                                                 options.orientation, options.channel, value, &hsv, params);
    bool written = false;
    if (bar.changed && !readOnly)
    {
        const bool alpha = options.channel == ColorChannel::Alpha;
        const ImVec4 out = internal::ColorForWrite(bar.color, value, alpha, true);
        if (!internal::ColorSame(out, *color, true))
        {
            *color = out;
            written = true;
        }
        internal::ColorRememberHsv(id, out, hsv);
    }

    const bool disabled = bar.item.disabled;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    if (!vertical && frame.label.text != nullptr)
        internal::DrawFrontLabel(window->DrawList, frame.label, frame.labelPos, m.control.size, disabled);
    if (vertical)
        internal::CutTextTooltip(bar.item, label, labelEnd, labelEnd != label);
    else
        internal::CutTextTooltip(id, frame.widget, bar.item.hovered, bar.item.focusVisible, label, labelEnd,
                                 frame.label.shown.truncated);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
