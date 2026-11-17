#pragma once

// The placeholder shared by the declared but not yet implemented widgets of the controls group
// (buttons, selection, sliders, inputs, pickers). Each stub in src/widgets/<family>/ works out
// the default size its finished widget will have from the tokens, then calls ControlStub,
// returns the neutral result and writes nothing. Phase 2 replaces a stub file with the widget
// and stops including this header there.
//
// The placeholder is one Dear ImGui item under the widget's id, not interactive and not a Tab
// stop, so a page of stubs never looks or acts like working controls. It draws the faintest
// fill wash with a hairline outline in the separator color, and the text "<Widget>: planned
// (<visible label>)" in Footnote and labelSecondary (labelTertiary inside BeginDisabled). When
// the box is too narrow the label part is dropped first, then the rest is shortened with an
// ellipsis, and hovering the box shows the whole text in a tooltip.
//
// Usage in a stub:
//
//     bool Toggle(const char* label, bool*, const ToggleOptions& options)
//     {
//         const float content = internal::StubLabelWidth(label, options.size) + Px(T().metrics.toggleWidth);
//         internal::ControlStub("Toggle", label, internal::StubWidth(options.width, content),
//                               internal::StubHeight(options.size, options.error, options.helpText));
//         return false;
//     }

#include <cstddef>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "internal/text_util.h"

namespace facet::internal
{

// True inside a frame with a current window: the only time a stub draws.
inline bool StubCanDraw()
{
    return ImGui::GetCurrentContext() != nullptr && GImGui->CurrentWindow != nullptr && GImGui->WithinFrameScope;
}

// The widget width in pixels by docs/api-conventions.md rule 6: `optionWidth` (units; > 0 that
// width, kFill the available width, other values < 0 the available width minus that many
// units), else ImGui::SetNextItemWidth, else a pushed ImGui::PushItemWidth, else
// `defaultWidth` (pixels; 0 or less fills the available width). Never below one pixel.
inline float StubWidth(float optionWidth, float defaultWidth)
{
    if (!StubCanDraw())
        return 0.0f;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    const float available = ImGui::GetContentRegionAvail().x;
    float width = 0.0f;
    if (optionWidth > 0.0f)
        width = Px(optionWidth);
    else if (optionWidth == kFill)
        width = available;
    else if (optionWidth < 0.0f)
        width = available - Px(-optionWidth);
    else if ((g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) != 0)
        width = g.NextItemData.Width < 0.0f ? available + g.NextItemData.Width : g.NextItemData.Width;
    else if (window->DC.ItemWidthStack.Size > 0)
        width = window->DC.ItemWidth < 0.0f ? available + window->DC.ItemWidth : window->DC.ItemWidth;
    else
        width = defaultWidth > 0.0f ? defaultWidth : available;
    return width < 1.0f ? 1.0f : width;
}

// Height of the message line under a control (spacing.xs and one Footnote line) when the widget
// shows an error or a help text; 0 otherwise.
inline float StubMessageHeight(const char* error, const char* helpText)
{
    if ((error == nullptr && helpText == nullptr) || !StubCanDraw())
        return 0.0f;
    return Px(T().spacing.xs) + MeasureText(TextStyle::Footnote, "Ag").y;
}

// Height of a one-line control of `size`, plus its message line.
inline float StubHeight(ControlSize size, const char* error = nullptr, const char* helpText = nullptr)
{
    return ControlHeight(size) + StubMessageHeight(error, helpText);
}

// Width of `text` up to "##" in the label style of `size` (ControlTextStyle), plus spacing.sm
// as the gap to what follows it; 0 for an empty visible part.
inline float StubLabelWidth(const char* text, ControlSize size)
{
    if (text == nullptr || !StubCanDraw())
        return 0.0f;
    const char* end = ImGui::FindRenderedTextEnd(text);
    if (end == text)
        return 0.0f;
    return MeasureText(ControlTextStyle(size), text, end).x + Px(T().spacing.sm);
}

// Height of one line of the label text above a group (RadioGroup, CheckboxGroup), plus
// spacing.xs; 0 for an empty visible part.
inline float StubLabelLine(const char* text)
{
    if (text == nullptr || !StubCanDraw() || ImGui::FindRenderedTextEnd(text) == text)
        return 0.0f;
    return MeasureText(TextStyle::Body, "Ag").y + Px(T().spacing.xs);
}

// Default lengths the theme has no token for yet, as multiples of existing tokens. The widgets
// of Phase 2 take them from the tokens requested for them.
inline constexpr float kStubFaderInControls = 5.0f;  // length of a vertical slider, in control heights
inline constexpr float kStubDialInControls = 2.0f;   // knob and dial diameter, in control heights of its size
inline constexpr float kStubPadInXxxl = 6.0f;        // side of XYPad and BoxRangePad, in spacing.xxxl
inline constexpr float kStubPickerInXxxl = 8.0f;     // width of ColorPicker and SatValSquare, in spacing.xxxl

// Size of a slider-like control: horizontal fills (rule 6) at the control height; vertical is
// one control height wide and `heightUnits` (0: kStubFaderInControls) high. Plus the message line.
inline ImVec2 StubSliderSize(Orientation orientation, float widthUnits, float heightUnits, ControlSize size,
                             const char* error, const char* helpText)
{
    const float message = StubMessageHeight(error, helpText);
    if (orientation == Orientation::Vertical)
    {
        const float length = heightUnits > 0.0f ? Px(heightUnits) : ControlHeight(size) * kStubFaderInControls;
        return ImVec2(ControlHeight(size), length + message);
    }
    return ImVec2(StubWidth(widthUnits, 0.0f), ControlHeight(size) + message);
}

// Diameter of a round control: `diameterUnits`, or kStubDialInControls control heights of `size`.
inline float StubDial(float diameterUnits, ControlSize size)
{
    return diameterUnits > 0.0f ? Px(diameterUnits) : ControlHeight(size) * kStubDialInControls;
}

// Size of a two-dimensional pad: `widthUnits` by rule 6 with the default pad side, and
// `heightUnits` or square.
inline ImVec2 StubPadSize(float widthUnits, float heightUnits)
{
    const float width = StubWidth(widthUnits, Px(T().spacing.xxxl) * kStubPadInXxxl);
    return ImVec2(width, heightUnits > 0.0f ? Px(heightUnits) : width);
}

inline constexpr int kStubCaptionSize = 192;

// Writes the placeholder text for a box `room` pixels wide, measured in the current font: the
// longest of "<widget>: planned (<visible label>)" and "<widget>: planned" that fits, or the
// latter shortened with an ellipsis. Empty when `room` is 0 or less or not even the ellipsis
// fits. `out` is always zero terminated (outSize > 0). Returns true when the label or part of the
// name had to be left out.
inline bool StubCaption(const char* widget, const char* label, float room, char* out, std::size_t outSize)
{
    if (outSize == 0)
        return false;
    out[0] = '\0';
    if (room <= 0.0f || !StubCanDraw())
        return true;
    if (widget == nullptr)
        widget = "";
    if (label == nullptr)
        label = "";
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    if (labelEnd > label)
    {
        std::snprintf(out, outSize, "%s: planned (%.*s)", widget, static_cast<int>(labelEnd - label), label);
        if (ImGui::CalcTextSize(out).x <= room)
            return false;
    }
    char shortText[kStubCaptionSize];
    std::snprintf(shortText, sizeof(shortText), "%s: planned", widget);
    TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), shortText, nullptr, room, TruncateMode::End, out, outSize);
    return labelEnd > label || std::strcmp(out, shortText) != 0;
}

// Submits the placeholder item of `widget` (the public function name) with the id of `label`
// (Dear ImGui rules; null counts as "") and the size in pixels. Does nothing outside a frame.
inline void ControlStub(const char* widget, const char* label, float width, float height)
{
    if (!StubCanDraw())
        return;
    if (label == nullptr)
        label = "";
    if (widget == nullptr)
        widget = "";
    const Theme& theme = T();
    if (width < 1.0f)
        width = 1.0f;
    if (height < 1.0f)
        height = 1.0f;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect box(pos, ImVec2(pos.x + width, pos.y + height));
    const ImGuiID id = ImGui::GetID(label);
    const bool disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImGui::ItemSize(box);
    if (!ImGui::ItemAdd(box, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoTabStop))
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = Px(theme.radii.control);
    FillRect(drawList, box.Min, box.Max, ToU32(theme.colors.fill[3]), radius);
    StrokeRect(drawList, box.Min, box.Max, ToU32(theme.colors.separator), radius, HairlineThickness());

    PushTextStyle(TextStyle::Footnote);
    const float inset = Px(theme.spacing.sm);
    const float room = width - 2.0f * inset;
    char text[kStubCaptionSize];
    const bool shortened = StubCaption(widget, label, room, text, sizeof(text));
    if (text[0] != '\0')
    {
        const ImVec4& color = disabled ? theme.colors.labelTertiary : theme.colors.labelSecondary;
        const float textHeight = ImGui::GetTextLineHeight();
        // One line of text centered in a single-line box, or at the top of a taller one.
        const float lineBox = height < 2.0f * textHeight + 2.0f * inset ? height : textHeight + 2.0f * inset;
        drawList->PushClipRect(box.Min, box.Max, true);
        drawList->AddText(ImVec2(box.Min.x + inset, CenteredTextY(box.Min.y, lineBox)), ToU32(color), text);
        drawList->PopClipRect();
    }
    PopTextStyle();

    // The whole text in a tooltip when the box had to shorten it.
    if (shortened && ImGui::IsItemHovered())
    {
        const char* labelEnd = ImGui::FindRenderedTextEnd(label);
        if (labelEnd > label)
            ImGui::SetTooltip("%s: planned (%.*s)", widget, static_cast<int>(labelEnd - label), label);
        else
            ImGui::SetTooltip("%s: planned", widget);
    }
}

} // namespace facet::internal
