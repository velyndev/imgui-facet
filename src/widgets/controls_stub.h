#pragma once

// The placeholder shared by the declared but not yet implemented widgets of the controls group
// (buttons, selection, sliders, inputs, pickers). Each stub in src/widgets/<family>/ works out
// the default size its finished widget will have from the tokens, then calls ControlStub,
// returns the neutral result and writes nothing. Phase 2 replaces a stub file with the widget
// and stops including this header there.
//
// The placeholder is one Dear ImGui item under the widget's id, not interactive and not a Tab
// stop, so a page of stubs never looks or acts like working controls. It draws the faintest
// fill wash with a hairline outline in the separator color, and the caption every planned
// widget shares (internal::DrawPlannedCaption in src/internal/planned_widget.h): "<Widget>:
// planned (<visible label>)", shortened to what fits as described there; hovering the box
// shows the whole text in a tooltip.
//
// Usage in a stub:
//
//     bool Toggle(const char* label, bool*, const ToggleOptions& options)
//     {
//         namespace stub = internal::controls;
//         const float content = stub::StubLabelWidth(label, options.size) + Px(T().metrics.toggleWidth);
//         stub::ControlStub("Toggle", label, stub::StubWidth(options.width, content),
//                           stub::StubHeight(options.size, options.error, options.helpText));
//         return false;
//     }
//
// The helpers live in facet::internal::controls so they never collide with the placeholder
// helpers of the other widget groups, which are linked into the same library.

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
#include "internal/planned_widget.h"

namespace facet::internal::controls
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

// StubWidth for a control that narrows itself when the row is short (labels collapse to icons,
// buttons move into a menu, cells and pads shrink): the family default is `content` (pixels),
// but at most the available width. A width the caller asked for is kept as it is.
inline float StubFittedWidth(float optionWidth, float content)
{
    if (!StubCanDraw())
        return 0.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    return StubWidth(optionWidth, content < available ? content : (available > 1.0f ? available : 1.0f));
}

// `length` (pixels), but at most the available width and at least one pixel: the default size
// of a control without a width option that shrinks in a short row (JoystickPad, Rating).
inline float StubFitAvailable(float length)
{
    if (!StubCanDraw())
        return 0.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    const float fitted = length < available ? length : available;
    return fitted < 1.0f ? 1.0f : fitted;
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

// Width of caller text (not a label: "##" is drawn as it is) in the label style of `size`,
// reading at most `size` bytes up to the first zero, so an unterminated buffer is never read
// past its end.
inline float StubTextWidth(const char* text, std::size_t byteCount, ControlSize size)
{
    if (text == nullptr || byteCount == 0 || !StubCanDraw())
        return 0.0f;
    const void* zero = std::memchr(text, 0, byteCount);
    const char* end = zero != nullptr ? static_cast<const char*>(zero) : text + byteCount;
    return MeasureText(ControlTextStyle(size), text, end).x;
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

// Size of a two-dimensional pad: `widthUnits` by rule 6 with the default pad side (at most the
// available width), and `heightUnits` or square.
inline ImVec2 StubPadSize(float widthUnits, float heightUnits)
{
    const float width = StubFittedWidth(widthUnits, Px(T().spacing.xxxl) * kStubPadInXxxl);
    return ImVec2(width, heightUnits > 0.0f ? Px(heightUnits) : width);
}

inline constexpr int kStubCaptionSize = kPlannedCaptionSize;

// The placeholder text for a box `room` pixels wide (internal::PlannedCaption).
inline bool StubCaption(const char* widget, const char* label, float room, char* out, std::size_t outSize)
{
    return PlannedCaption(widget, label, room, out, outSize);
}

// Draws the placeholder look into `box` (pixels): the fill wash, the hairline outline and the
// caption, and the whole text in a tooltip when `hovered` and the caption had to be shortened.
// Submits no item; ControlStub and MessageStub decide what the box is.
inline void DrawStub(const ImRect& box, const char* widget, const char* label, bool hovered)
{
    const Theme& theme = T();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = Px(theme.radii.control);
    FillRect(drawList, box.Min, box.Max, ToU32(theme.colors.fill[3]), radius);
    StrokeRect(drawList, box.Min, box.Max, ToU32(theme.colors.separator), radius, HairlineThickness());
    DrawPlannedCaption(drawList, box, widget, label, hovered);
}

// Submits the placeholder item of `widget` (the public function name) with the id of `label`
// (Dear ImGui rules; null counts as "") and the size in pixels. Does nothing outside a frame.
inline void ControlStub(const char* widget, const char* label, float width, float height)
{
    if (!StubCanDraw())
        return;
    if (label == nullptr)
        label = "";
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect box(pos, ImVec2(pos.x + (width < 1.0f ? 1.0f : width), pos.y + (height < 1.0f ? 1.0f : height)));
    const ImGuiID id = ImGui::GetID(label);
    ImGui::ItemSize(box);
    if (!ImGui::ItemAdd(box, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoTabStop))
        return;
    DrawStub(box, widget, label, ImGui::IsItemHovered());
}

// True when the last item is an item in the content of the current window. Its id does not
// tell: groups (and with them stock composite widgets such as ImGui::InputInt) and text items
// have id 0 while idle, and Begin leaves the title bar as the last item with the move id. The
// rect tells: an item has height and lies at or below the content start, the title bar lies
// above it and a child window's empty title bar has no height.
inline bool StubLastItemInContent()
{
    ImGuiContext& g = *GImGui;
    const ImRect& rect = g.LastItemData.Rect;
    return rect.Max.y > rect.Min.y && rect.Min.y >= g.CurrentWindow->DC.CursorStartPos.y;
}

// The placeholder of a message line under the last item (FieldValidation): one Footnote line
// starting at the item's left edge, spacing.xs below it, reaching the right edge of the content
// region; at the cursor when the window has no item yet. The box is spacing.xs taller than the
// line so its outline never crosses the text. It is layout only, not an item: the last item
// stays the one the message belongs to, so item queries after the call still describe it. Does
// nothing outside a frame.
inline void MessageStub(const char* widget, const char* text)
{
    if (!StubCanDraw())
        return;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    const ImGuiLastItemData last = g.LastItemData;
    const bool underItem = StubLastItemInContent();
    const float right = window->DC.CursorPos.x + ImGui::GetContentRegionAvail().x;
    const float x = underItem ? last.Rect.Min.x : window->DC.CursorPos.x;
    const float y = underItem ? last.Rect.Max.y + Px(T().spacing.xs) : window->DC.CursorPos.y;
    const float height = MeasureText(TextStyle::Footnote, "Ag").y + Px(T().spacing.xs);
    const ImRect box(ImVec2(x, y), ImVec2(right > x + 1.0f ? right : x + 1.0f, y + height));
    ImGui::SetCursorScreenPos(box.Min);
    ImGui::ItemSize(box);
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(box.Min, box.Max);
    if (ImGui::IsRectVisible(box.Min, box.Max))
        DrawStub(box, widget, text, hovered);
    g.LastItemData = last;
}

} // namespace facet::internal::controls
