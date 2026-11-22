#pragma once

// Control anatomy: the parts that controls of every widget family share, each with one helper,
// so buttons, toggles, sliders, fields and lists built by different people measure, place and
// color those parts the same way. docs/writing-a-widget.md lists which helper covers which part.
//
// A push-like control (facet::Button in src/widgets/buttons/button.cpp is the full example):
//
//     const NextItemData next = TakeNextItemData();                  // item.h, before any item
//     const ControlMetrics m = GetControlMetrics(options.size);      // height, padding, radius
//     ControlTextScope text(m);                                      // the control text style
//     const float content = m.paddingX * 2.0f + ImGui::CalcTextSize(label, end).x;
//     const float width = ResolveItemWidth(options.width, next, content);   // rule 6
//     ApplyNextItemData(next);
//     const ItemState item = ItemBehavior(id, ImVec2(width, m.height));     // item.h
//     const ImVec4 fill = StateFill(c.fill[2], c.fill[1], c.fill[0], item);   // hover and press
//     const FittedText shown = FitText(label, end, width - m.paddingX * 2.0f);  // ellipsis
//     ... draw fill and shown.text in ControlLabelColor(item.disabled) ...
//     DrawItemFocusRing(item, m.radius);                             // focus.h
//     CutTextTooltip(item, label, end, shown.truncated);             // rule 11
//
// A field-like control (the text field core in src/internal/text_field.cpp uses every one):
//
//     const FrontLabel front = MeasureFrontLabel(label, m.size, width);      // label in front
//     const ImRect frame(origin.x + front.width, origin.y, origin.x + width, origin.y + m.height);
//     DrawFieldFrame(drawList, frame, {.hoverAmount = item.hoverAmount, .active = active,
//                                      .disabled = item.disabled, .readOnly = item.readOnly,
//                                      .error = options.error != nullptr});
//     DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(frame.Min.y, m.height)), disabled);
//     const ControlMessage message = PickControlMessage(options.error, options.helpText);
//     DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), disabled, id, frame);
//
// A group that wraps into rows, or a tall control, draws its label above itself instead
// (MeasureAboveLabel, DrawAboveLabel), as rule 6 of docs/api-conventions.md sorts them.
//
// Colors come from the theme only: label text in label (labelTertiary when disabled), secondary
// text and glyphs in labelSecondary, fills from the fill tokens through StateFill, outlines from
// ControlBoundaryColor, focusRing and destructive. The disabled look is the same for every
// variant of a control: fill[3] and labelTertiary, no hover or press feedback. Lengths are pixels
// here (the helpers apply Px() to the tokens).

#include <cstddef>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "internal/item.h"
#include "internal/text_util.h"

namespace facet::internal
{

// ---- Entry -----------------------------------------------------------------------------------

// The window a widget submits its items to, or null when it must submit nothing and return the
// neutral result (docs/api-conventions.md rule 14): no Dear ImGui context, outside
// ImGui::NewFrame / Render, no current window, or a window whose items are skipped (collapsed or
// clipped away, as stock widgets check with SkipItems).
ImGuiWindow* ControlWindow();

// ---- Size ------------------------------------------------------------------------------------

// The lengths of a control of one size, in pixels at the current scale.
struct ControlMetrics
{
    ControlSize size = ControlSize::Regular;  // resolved: never Inherit
    TextStyle textStyle = TextStyle::Body;    // ControlTextStyle(size)
    float height = 0.0f;    // ControlHeight(size)
    // Between the edge of a push-like control (button, chip, segment, toggle button) and its
    // content: spacing.sm, spacing.md and spacing.lg for Small, Regular and Large.
    float paddingX = 0.0f;
    float fieldPadding = 0.0f;  // between a field frame and its text or icon: field.padding
    float iconGap = 0.0f;       // between an icon and the text next to it: spacing.sm
    float radius = 0.0f;        // corner radius of the control: radii.control
};

// The metrics of a control of `size` (Inherit resolved through the control size stack).
ControlMetrics GetControlMetrics(ControlSize size);

// Pushes the text style of a control for its lifetime (facet::PushTextStyle), so measuring and
// drawing inside it use the control's font and size. Not copyable.
class ControlTextScope
{
public:
    explicit ControlTextScope(const ControlMetrics& metrics);
    explicit ControlTextScope(TextStyle style);
    ~ControlTextScope();
    ControlTextScope(const ControlTextScope&) = delete;
    ControlTextScope& operator=(const ControlTextScope&) = delete;

    // The font size of the pushed style in pixels (ImGui::GetFontSize while the scope lives).
    float FontSize() const { return fontSize_; }
    // The side of an icon beside the text: the font size rounded to whole pixels, so an icon
    // lines up with the letters next to it at every scale.
    float IconSize() const { return iconSize_; }

private:
    float fontSize_ = 0.0f;
    float iconSize_ = 0.0f;
};

// ---- Colors and feedback ----------------------------------------------------------------------

// Text of a control: label, or labelTertiary while disabled.
ImVec4 ControlLabelColor(bool disabled);
// Secondary text and glyphs (placeholders, units, leading icons of fields): labelSecondary, or
// labelTertiary while disabled.
ImVec4 ControlSecondaryColor(bool disabled);

// The hover and press feedback of a fill: `rest` blended toward `hover` by item.hoverAmount, then
// toward `pressed` by item.pressAmount (both animated over motion.durationFast by the item
// behaviour). A disabled item gets fill[3] whatever the colors.
ImVec4 StateFill(const ImVec4& rest, const ImVec4& hover, const ImVec4& pressed, const ItemState& item);

// The neutral control fill: fill[2] at rest, fill[1] hovered, fill[0] pressed, fill[3] disabled.
ImVec4 NeutralFill(const ItemState& item);

// ---- Field frame ------------------------------------------------------------------------------

// What the frame of a field-like control (text, number and drag fields, dropdown fields, key
// bind capture, cycle selectors) shows.
struct FieldFrameLook
{
    float hoverAmount = 0.0f;  // 0 to 1: fill[2] at rest toward fill[1] on hover
    bool active = false;       // has the keyboard or listens: a focusRing hairline instead of the boundary
    bool disabled = false;     // fill[3] and a separator hairline
    bool readOnly = false;     // no fill, the boundary hairline only
    bool error = false;        // a destructive hairline, also while disabled
};

// Draws the frame (fill and hairline, radii.control) of a field-like control into `drawList`.
// The keyboard focus ring is drawn separately (focus.h), after keyboard input only.
void DrawFieldFrame(ImDrawList* drawList, const ImRect& frame, const FieldFrameLook& look);

// ---- Text that fits ---------------------------------------------------------------------------

inline constexpr int kFittedTextSize = 256;

// A text as it fits a width in the current font: unchanged, or cut with the font's ellipsis.
struct FittedText
{
    char text[kFittedTextSize] = {};  // zero terminated; a longer text is cut at the buffer as well
    float width = 0.0f;               // pixels of `text`
    bool truncated = false;           // something was left out
};

// [text, end) (end null: zero terminated) fitted into `maxWidth` pixels in the current font.
// Nothing is allocated.
FittedText FitText(const char* text, const char* end, float maxWidth, TruncateMode mode = TruncateMode::End);

// The whole text of a label that a control cut with an ellipsis, as a tooltip after the hover
// delay and on visible keyboard focus (docs/api-conventions.md rule 11), through the popup host
// (BeginPopupTooltip). Does nothing unless `truncated`. Call it every frame after the item.
void CutTextTooltip(const ItemState& item, const char* text, const char* end, bool truncated);
// The same for a label that is not the item itself (a label in front of a field): `anchor` is
// the rectangle the tooltip belongs to, `hovered` and `focusVisible` say when it shows.
void CutTextTooltip(ImGuiID id, const ImRect& anchor, bool hovered, bool focusVisible, const char* text,
                    const char* end, bool truncated);

// ---- Labels -----------------------------------------------------------------------------------

// The largest share of a field widget's width its label in front may take; a longer label is
// cut with an ellipsis, so the control keeps at least the rest.
inline constexpr float kFrontLabelMaxShare = 0.5f;

// The visible label part drawn in front of a field-like or one-line compact control, inside the
// widget width (rule 6).
struct FrontLabel
{
    const char* text = nullptr;  // the visible part [text, end); null without one
    const char* end = nullptr;
    float width = 0.0f;          // pixels in front of the control: the (cut) text and spacing.sm; 0 without a label
    FittedText shown;            // what is drawn
};

// How much room a label in front takes.
enum class FrontLabelFit
{
    // The label column of a labelled row (RowFrontLabelColumn in row_layout.h): every control with
    // a label in front starts at one x, the x of a settings row's control, whatever the label says.
    // Field widgets and every control whose width is set take it. A widget narrower than the
    // column plus layout.minRowControl (where a settings row stacks) fits its text as Text does.
    Column,
    // The text width plus spacing.sm: a compact control that fits its content (a segmented
    // control, a color well) sits inline among other controls, where a column would only add a gap.
    Text,
};

// Measures the visible part of `label` (up to "##") in the text style of `size`. It takes the
// room `fit` names, at most kFrontLabelMaxShare of `totalWidth` (pixels, the whole widget); a text
// longer than its room is cut (show it with CutTextTooltip).
FrontLabel MeasureFrontLabel(const char* label, ControlSize size, float totalWidth,
                             FrontLabelFit fit = FrontLabelFit::Column);
// The fit of a compact control that fits its content unless a width is set (rule 6 of
// docs/api-conventions.md: `optionsWidth`, ImGui::SetNextItemWidth or ImGui::PushItemWidth): Column
// with a set width, so it lines up in a column of fields, Text otherwise.
FrontLabelFit CompactFrontLabelFit(float optionsWidth, const NextItemData& next);
// Draws it with its text top at `pos` (text style of the control, see CenteredTextY in text.h),
// in ControlLabelColor(disabled).
void DrawFrontLabel(ImDrawList* drawList, const FrontLabel& label, ImVec2 pos, ControlSize size, bool disabled);

// The visible label part drawn above a group that wraps into rows or a tall control (radio and
// checkbox groups, chip groups, list boxes, text areas, progress bars).
struct AboveLabel
{
    const char* text = nullptr;  // the visible part [text, end); null without one
    const char* end = nullptr;
    float height = 0.0f;         // pixels from the label top to the control: one text line and spacing.xs; 0 without one
};

AboveLabel MeasureAboveLabel(const char* label, ControlSize size);
// Draws the label line at `pos` (its top-left), `width` pixels wide, in the text style of `size`
// and ControlLabelColor(disabled), cut with an ellipsis. `trailing` (null for none) is drawn at
// the end of the line in ControlSecondaryColor(disabled), and the label gives way to it. Returns
// what was drawn of the label.
FittedText DrawAboveLabel(ImDrawList* drawList, const AboveLabel& label, ImVec2 pos, float width, ControlSize size,
                          bool disabled, const char* trailing = nullptr);

// ---- Icons and the loading spinner -------------------------------------------------------------

// Draws `icon` `size` pixels square with its left edge at `x`, centered on `centerY`, snapped to
// whole pixels.
void DrawControlIcon(ImDrawList* drawList, Icon icon, float x, float centerY, float size, ImU32 color);

// The spinner of a loading control (loading.h): `diameter` pixels around `center`, with a stroke
// of an eighth of the diameter (at least one hairline), in `color`.
void DrawControlSpinner(ImDrawList* drawList, ImVec2 center, float diameter, ImU32 color);

// ---- Help and error text under a control ------------------------------------------------------

// The message under a control: the error, which replaces the help text (rule 7), or the help text.
struct ControlMessage
{
    const char* text = nullptr;  // null or empty: no message line
    bool error = false;
    bool IsSet() const { return text != nullptr && text[0] != '\0'; }
};

ControlMessage PickControlMessage(const char* error, const char* helpText);

// Height of the message line under a control `width` pixels wide, the spacing.xs gap above it
// included; 0 when the message is not set. Footnote, wrapped; an error starts with the error icon.
float ControlMessageHeight(const ControlMessage& message, float width);

// Draws the message under the control whose bottom-left corner is `at` (pixels): spacing.xs
// lower, `width` wide, in Footnote; an error in destructive with the error icon (also while
// disabled, because it still applies), help text in ControlSecondaryColor(disabled). Extends the
// current window's content bounds over it, so the widget's group includes the line. Returns the y
// of its bottom (at.y when the message is not set). An error is also reported under `itemId`
// with `itemRect` for ValidationSummary (ReportFieldError); pass 0 to leave it out.
float DrawControlMessage(const ControlMessage& message, ImVec2 at, float width, bool disabled, ImGuiID itemId = 0,
                         const ImRect& itemRect = ImRect());

// ---- Reported errors (ValidationSummary) -------------------------------------------------------

inline constexpr int kFieldErrorTextSize = 160;

// One error message a control showed in a frame.
struct FieldErrorRecord
{
    ImGuiID windowId = 0;      // the window the control was drawn in
    ImGuiID rootWindowId = 0;  // its root window (a field inside a scroll area counts for the window around it)
    ImGuiID itemId = 0;        // the control to focus; 0 when it has no id (a stock group)
    ImRect rect;               // the control, screen pixels, to scroll to
    char text[kFieldErrorTextSize] = {};  // the message, cut at a whole character when longer
};

// Records an error message for the current window in this frame. Allocates only while more
// errors are reported in one frame than ever before.
void ReportFieldError(ImGuiID itemId, const ImRect& itemRect, const char* message);
// The errors every window reported in the previous frame, in the order they were reported.
std::span<const FieldErrorRecord> PreviousFrameFieldErrors();
// Forgets every record (facet::Shutdown and tests).
void ClearFieldErrors();

} // namespace facet::internal
