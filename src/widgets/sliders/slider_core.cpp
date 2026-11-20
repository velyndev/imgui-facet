// The shared parts of the sliders family (slider_core.h).

#include "widgets/sliders/slider_core.h"

#include <algorithm>
#include <cmath>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"

namespace facet::internal
{
namespace
{

// How much a held knob grows, as a share of its radius: enough to show the grab under a finger
// or pointer without covering the neighbouring ticks.
constexpr float kSliderKnobPressGrowth = 0.15f;
// The hover darkens the knob outline halfway from the boundary color toward label.
constexpr float kSliderKnobHoverShare = 0.5f;
// The knob outline is one and a half hairlines, so the round edge reads as firmly as a hairline
// on a straight edge.
constexpr float kSliderKnobOutlineHairlines = 1.5f;
// Animation sub key of the value bubble under the widget or handle id.
constexpr int kSliderBubbleSub = 0x0B00;

struct SliderValueColumn
{
    float width = 0.0f;
    float fontSize = 0.0f;
};

struct SliderTyping
{
    ImGuiID widget = 0;
    int part = -1;
    int frames = 0;       // frames since typing began, until the field took the keyboard
    bool started = false; // the field had the keyboard
};

SliderTyping& CurrentTyping()
{
    static SliderTyping typing;
    return typing;
}

// Frames a typing request waits for the field to take the keyboard before it is dropped.
constexpr int kSliderTypingWaitFrames = 3;

} // namespace

SliderMetrics GetSliderMetrics(ControlSize size)
{
    const Theme& t = T();
    SliderMetrics m;
    m.control = GetControlMetrics(size);
    // The knob follows the control height, so the three sizes differ in their knob as well;
    // an even diameter keeps its centre on a whole pixel.
    const float regular = ControlHeight(ControlSize::Regular);
    const float share = regular > 0.0f ? m.control.height / regular : 1.0f;
    m.knob = std::max(2.0f, std::round(Px(t.metrics.sliderKnobSize) * share * 0.5f) * 2.0f);
    m.track = std::max(1.0f, PxSnapped(t.metrics.sliderTrackHeight));
    m.tick = std::max(1.0f, PxSnapped(t.metrics.sliderTickSize));
    m.valueGap = PxSnapped(t.spacing.sm);
    return m;
}

SliderLook PickSliderLook(bool disabled, bool readOnly)
{
    if (disabled)
        return SliderLook::Disabled;
    return readOnly ? SliderLook::ReadOnly : SliderLook::Normal;
}

SliderFrame LayoutSliderFrame(ImVec2 origin, const char* label, float width, float valueWidth, const SliderMetrics& m,
                              Orientation orientation, float length)
{
    SliderFrame f;
    origin = ImVec2(std::round(origin.x), std::round(origin.y));
    const float height = m.control.height;
    if (orientation == Orientation::Vertical)
    {
        length = std::max(1.0f, std::round(length));
        f.widget = ImRect(origin, ImVec2(origin.x + height, origin.y + length));
        f.control = f.widget;
        f.knob = std::min(m.knob, std::max(2.0f, std::floor(length * 0.5f) * 2.0f));
        const float x = std::round(f.control.GetCenter().x);
        const float half = f.knob * 0.5f;
        f.track = ImRect(x, f.control.Min.y + half, x, std::max(f.control.Min.y + half, f.control.Max.y - half));
        f.value = ImRect(f.widget.Max, f.widget.Max);
        return f;
    }
    width = std::max(1.0f, std::round(width));
    f.widget = ImRect(origin, ImVec2(origin.x + width, origin.y + height));
    f.label = MeasureFrontLabel(label, m.control.size, width);
    f.label.width = std::round(f.label.width);
    f.labelPos = ImVec2(origin.x, CenteredTextY(origin.y, height));
    float controlMin = origin.x + f.label.width;
    float controlMax = origin.x + width;
    float column = valueWidth > 0.0f ? std::ceil(valueWidth) + m.valueGap : 0.0f;
    // The value column gives way first, so the track keeps room for two knobs.
    if (controlMax - controlMin - column < m.knob * 2.0f)
        column = 0.0f;
    f.value = ImRect(controlMax - column + (column > 0.0f ? m.valueGap : 0.0f), origin.y, controlMax, origin.y + height);
    controlMax -= column;
    controlMax = std::max(controlMin + 1.0f, controlMax);
    if (controlMax > origin.x + width)
    {
        controlMin = std::max(origin.x, origin.x + width - 1.0f);
        controlMax = origin.x + width;
    }
    f.control = ImRect(controlMin, origin.y, controlMax, origin.y + height);
    // A control shorter than its knob gets a smaller knob, so nothing leaves the row.
    f.knob = std::min(m.knob, std::max(2.0f, std::floor(f.control.GetWidth() * 0.5f) * 2.0f));
    const float y = std::round(f.control.GetCenter().y);
    const float half = f.knob * 0.5f;
    f.track = ImRect(f.control.Min.x + half, y, std::max(f.control.Min.x + half, f.control.Max.x - half), y);
    return f;
}

float SliderTrackPos(const ImRect& track, Orientation orientation, double fraction)
{
    return std::round(FractionToTrack(track, orientation, fraction));
}

ImRect SliderKnobRect(const ImRect& track, Orientation orientation, float knob, double fraction)
{
    const float along = SliderTrackPos(track, orientation, fraction);
    const float half = std::round(knob * 0.5f);
    if (orientation == Orientation::Vertical)
    {
        const float x = std::round(track.Min.x);
        return ImRect(x - half, along - half, x - half + knob, along - half + knob);
    }
    const float y = std::round(track.Min.y);
    return ImRect(along - half, y - half, along - half + knob, y - half + knob);
}

namespace
{

// The rectangle of the track line between two positions along it, whole pixels.
ImRect TrackLine(const ImRect& track, Orientation orientation, float thickness, float a, float b)
{
    const float lo = std::min(a, b);
    const float hi = std::max(a, b);
    if (orientation == Orientation::Vertical)
    {
        const float x = std::round(track.Min.x - thickness * 0.5f);
        return ImRect(x, lo, x + thickness, hi);
    }
    const float y = std::round(track.Min.y - thickness * 0.5f);
    return ImRect(lo, y, hi, y + thickness);
}

} // namespace

void DrawSliderTrack(ImDrawList* drawList, const ImRect& track, Orientation orientation, const SliderMetrics& m,
                     double fillFrom, double fillTo, SliderLook look, bool error, ImU32 fillColor)
{
    const Colors& c = T().colors;
    const bool vertical = orientation == Orientation::Vertical;
    const float half = m.track * 0.5f;
    // The line reaches half a thickness past the knob travel, so its round ends sit under the
    // knob at either end.
    const float start = vertical ? track.Max.y + half : track.Min.x - half;
    const float end = vertical ? track.Min.y - half : track.Max.x + half;
    const ImRect whole = TrackLine(track, orientation, m.track, std::round(start), std::round(end));
    // The unfilled line in the 3:1 boundary color, so the ends of the range stay visible.
    const ImVec4 rest = look == SliderLook::Disabled ? c.fill[3] : (error ? c.destructive : ControlBoundaryColor(c));
    FillRect(drawList, whole.Min, whole.Max, ToU32(rest), half);
    const float a = SliderTrackPos(track, orientation, fillFrom);
    const float b = SliderTrackPos(track, orientation, fillTo);
    if (a == b)
        return;
    ImU32 color = fillColor;
    if (look == SliderLook::Disabled)
        color = ToU32(c.labelTertiary);
    else if (look == SliderLook::ReadOnly)
        color = ToU32(c.labelSecondary);
    else if (color == 0)
        color = ToU32(c.accent);
    const ImRect fill = TrackLine(track, orientation, m.track, a, b);
    FillRect(drawList, fill.Min, fill.Max, color, half);
}

void DrawSliderTrackStretch(ImDrawList* drawList, const ImRect& track, Orientation orientation, const SliderMetrics& m,
                            double from, double to, ImU32 color)
{
    const float a = SliderTrackPos(track, orientation, from);
    const float b = SliderTrackPos(track, orientation, to);
    if (a == b)
        return;
    const ImRect fill = TrackLine(track, orientation, m.track, a, b);
    FillRect(drawList, fill.Min, fill.Max, color, m.track * 0.5f);
}

void DrawSliderTicks(ImDrawList* drawList, const ImRect& track, Orientation orientation, const SliderMetrics& m,
                     std::span<const double> fractions, SliderLook look)
{
    const Colors& c = T().colors;
    const ImU32 color = ToU32(look == SliderLook::Disabled ? c.labelTertiary : c.labelSecondary);
    // Below the track (beside a vertical one), clear of the knob's round edge.
    const float offset = std::round(m.knob * 0.5f) + m.tick * 0.5f;
    const float radius = m.tick * 0.5f;
    for (const double fraction : fractions)
    {
        const float at = SliderTrackPos(track, orientation, fraction);
        const ImVec2 center = orientation == Orientation::Vertical ? ImVec2(std::round(track.Min.x) + offset, at)
                                                                   : ImVec2(at, std::round(track.Min.y) + offset);
        drawList->AddCircleFilled(center, radius, color);
    }
}

int SliderStepTicks(const ValueMapping& mapping, std::span<double> out, int maxTicks)
{
    const ValueMapping m = NormalizeMapping(mapping);
    const double step = m.step > 0.0 ? m.step : (m.integer ? 1.0 : 0.0);
    if (step <= 0.0 || !(m.max > m.min) || !std::isfinite(m.max - m.min))
        return 0;
    const double count = std::floor((m.max - m.min) / step + 1e-9);
    if (count + 1.0 > static_cast<double>(std::min<int>(maxTicks, static_cast<int>(out.size()))) || count < 1.0)
        return 0;
    int written = 0;
    for (int i = 0; i <= static_cast<int>(count); ++i)
        out[static_cast<size_t>(written++)] = ValueToFraction(m, m.min + i * step);
    // A max off the grid is a stop too (End and drags reach it).
    if (m.min + count * step < m.max - step * 1e-9 && written < static_cast<int>(out.size()))
        out[static_cast<size_t>(written++)] = 1.0;
    return written;
}

void DrawSliderKnob(ImDrawList* drawList, const ImRect& rect, const ItemState& item, SliderLook look)
{
    const Colors& c = T().colors;
    const ImVec2 center = rect.GetCenter();
    const float base = rect.GetWidth() * 0.5f;
    const float grow = look == SliderLook::Normal ? kSliderKnobPressGrowth * item.pressAmount : 0.0f;
    // Whole pixels: the knob grows in pixel steps instead of blurring its edge.
    const float radius = std::max(1.0f, std::round(base * (1.0f + grow)));
    const ImVec4 boundary = ControlBoundaryColor(c);
    ImVec4 outline = Mix(boundary, c.label, item.hoverAmount * kSliderKnobHoverShare);
    if (look == SliderLook::Disabled)
        outline = c.labelTertiary;
    drawList->AddCircleFilled(center, radius, ToU32(c.bg));
    drawList->AddCircle(center, radius, ToU32(outline), 0, HairlineThickness() * kSliderKnobOutlineHairlines);
    // Round, like the knob, so the ring keeps an even gap all around it.
    DrawFocusRing(drawList, ImRect(center.x - radius, center.y - radius, center.x + radius, center.y + radius), radius,
                  item.focusAmount, 0, RingShape::Round);
}

void DrawSliderBubble(ImDrawList* drawList, const ImRect& knob, const char* text, Orientation orientation, float amount)
{
    if (amount <= 0.0f || text == nullptr || text[0] == '\0')
        return;
    const Theme& t = T();
    const Colors& c = t.colors;
    ControlTextScope scope(TextStyle::Footnote);
    const ImVec2 size = ImGui::CalcTextSize(text);
    const float padX = PxSnapped(t.spacing.sm);
    const float padY = PxSnapped(t.spacing.xs);
    const float gap = PxSnapped(t.spacing.xs);
    const ImVec2 box(std::ceil(size.x) + padX * 2.0f, std::ceil(size.y) + padY * 2.0f);
    ImVec2 min;
    if (orientation == Orientation::Vertical)
        min = ImVec2(knob.Max.x + gap, std::round(knob.GetCenter().y - box.y * 0.5f));
    else
        min = ImVec2(std::round(knob.GetCenter().x - box.x * 0.5f), knob.Min.y - gap - box.y);
    min = ImVec2(std::round(min.x), std::round(min.y));
    const ImVec2 max(min.x + box.x, min.y + box.y);
    // Inverted surface: label behind bg-colored text reads over any surface in both themes.
    FillRect(drawList, min, max, ToU32(WithAlpha(c.label, c.label.w * amount)), Px(t.radii.element));
    drawList->AddText(ImVec2(min.x + padX, min.y + padY), ToU32(WithAlpha(c.bg, amount)), text);
}

void DrawSliderValueText(ImDrawList* drawList, const ImRect& column, const char* text, bool disabled)
{
    if (column.GetWidth() <= 0.0f || text == nullptr)
        return;
    const float width = ImGui::CalcTextSize(text).x;
    const float x = std::round(std::max(column.Min.x, column.Max.x - width));
    drawList->PushClipRect(column.Min, column.Max, true);
    drawList->AddText(ImVec2(x, CenteredTextY(column.Min.y, column.GetHeight())), ToU32(ControlSecondaryColor(disabled)),
                      text);
    drawList->PopClipRect();
}

float SliderValueWidth(ImGuiID id, double min, double max, double value, const char* format, FormatFn formatter,
                       void* user, bool integer)
{
    char buffer[64];
    float widest = 0.0f;
    for (const double v : {min, max, value})
    {
        if (!std::isfinite(v))
            continue;
        FormatNumber(buffer, sizeof(buffer), v, format, formatter, user, integer, false);
        widest = std::max(widest, ImGui::CalcTextSize(buffer).x);
    }
    SliderValueColumn& column = GetState<SliderValueColumn>(id);
    // A new font size (a scale change) starts the measure over.
    const float fontSize = ImGui::GetFontSize();
    if (column.fontSize != fontSize)
        column = {0.0f, fontSize};
    column.width = std::max(column.width, std::ceil(widest));
    return column.width;
}

float SliderBubbleAmount(ImGuiID id, const ItemState& item)
{
    const bool show = item.active || item.dragging || item.focusVisible || (item.forced & ForcedItemState::Pressed) != ForcedItemState::None;
    return Tween(id, kSliderBubbleSub, show ? 1.0f : 0.0f, T().motion.durationFast);
}

// ---- Typed values --------------------------------------------------------------------------

bool SliderWantsTyping(const ItemState& item)
{
    if (!item.pressed || item.disabled || item.readOnly || item.forced != ForcedItemState::None)
        return false;
    const ImGuiIO& io = ImGui::GetIO();
    if (item.keyboardPressed)
        return ImGui::IsKeyDown(ImGuiKey_Enter) || ImGui::IsKeyDown(ImGuiKey_KeypadEnter);
    return io.KeyCtrl;
}

void SliderBeginTyping(ImGuiID widgetId, int part)
{
    SliderTyping& typing = CurrentTyping();
    typing = {};
    typing.widget = widgetId;
    typing.part = part;
}

int SliderTypingPart(ImGuiID widgetId)
{
    const SliderTyping& typing = CurrentTyping();
    return typing.widget == widgetId ? typing.part : -1;
}

SliderTypeResult SliderTypeField(ImGuiID widgetId, int part, const ImRect& rect, double value,
                                 const NumberFieldParams& params, ImGuiID returnFocus)
{
    SliderTypeResult result;
    SliderTyping& typing = CurrentTyping();
    if (typing.widget != widgetId || typing.part != part)
        return result;
    ImGuiContext& g = *GImGui;
    ImGui::PushOverrideID(widgetId);
    result.fieldId = ImGui::GetID("##type");
    if (typing.frames == 0)
        TextFieldRequestFocus(result.fieldId, TextFieldFocus::SelectAll);
    ImGui::SetCursorScreenPos(rect.Min);
    ImGui::SetNextItemWidth(rect.GetWidth());
    NumberFieldParams field = params;
    field.width = 0.0f;
    field.error = nullptr;
    field.helpText = nullptr;
    field.steppers = false;
    double edited = value;
    const NumberFieldResult edit = NumberFieldBehavior("##type", &edited, field);
    ImGui::PopID();
    result.typing = true;
    if (edit.changed && !edit.field.canceled)
    {
        result.changed = true;
        result.value = edited;
    }
    if (edit.field.active)
        typing.started = true;
    ++typing.frames;
    const bool timedOut = !typing.started && typing.frames > kSliderTypingWaitFrames;
    if ((typing.started && !edit.field.active) || timedOut)
    {
        result.ended = true;
        typing = {};
        // Enter and Escape leave the focus on the field: it goes back to the knob. Tab moved it on.
        if (returnFocus != 0 && (g.NavId == result.fieldId || g.NavId == 0))
        {
            ImGui::SetFocusID(returnFocus, g.CurrentWindow);
            // The keyboard ended the edit: the knob shows its focus ring and takes the next key.
            ImGui::SetNavCursorVisible(true);
        }
    }
    return result;
}

void ResetSliderTyping()
{
    CurrentTyping() = {};
}

} // namespace facet::internal
