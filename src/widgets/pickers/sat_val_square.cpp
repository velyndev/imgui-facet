// SatValSquare: the saturation and value square of ColorPicker on its own, for the hue of the color.
// One item and one Tab stop on the square part (color_parts.h): a press puts the handle under the
// pointer, a drag follows it, the arrows move it on both axes (Shift fine, Ctrl coarse), Escape
// restores the color from the start of the edit. Hue and alpha are kept; the hue memory keeps the
// hue and the saturation while the color passes through grey and black.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/pickers/color_model.h"
#include "widgets/pickers/color_parts.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{

bool SatValSquare(const char* label, ImVec4* color, const SatValSquareOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float side = std::round(internal::ResolveItemWidth(options.width, next, std::min(available, Px(T().metrics.pickerWidth))));
    const bool readOnly = options.readOnly || color == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    // A missing value shows the accent as a neutral sample; nothing is written.
    const ImVec4 value = color != nullptr ? *color : T().colors.accent;
    internal::ColorHsv hsv = internal::ColorTrackHsv(id, value);
    const internal::SliderMetrics m = internal::GetSliderMetrics(ControlSize::Inherit);

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect rect(origin, ImVec2(origin.x + side, origin.y + side));
    ImGui::BeginGroup();
    ImGui::ItemSize(rect.GetSize());
    internal::ApplyNextItemData(next);
    internal::ColorPartParams params;
    params.readOnly = readOnly;
    params.mixed = options.mixed;
    const float knob = std::min(m.knob, std::max(2.0f, std::floor(side * 0.25f) * 2.0f));
    const internal::ColorPartResult square = internal::ColorSquarePart(id, rect, knob, value, &hsv, params);
    bool written = false;
    if (square.changed && !readOnly)
    {
        const ImVec4 out = internal::ColorForWrite(square.color, value, false, true);
        if (!internal::ColorSame(out, *color, true))
        {
            *color = out;
            written = true;
        }
        internal::ColorRememberHsv(id, out, hsv);
    }
    // A panel shows its label as its tooltip (rule 6).
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(square.item, label, labelEnd, labelEnd != label);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
