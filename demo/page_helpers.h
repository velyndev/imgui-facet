#pragma once

// Building blocks the demo pages call: page text (headers, paragraphs, notes), labelled settings
// rows, row wrapping and width helpers, probes for parts of widgets and floating layers, and the
// prototype controls of the Blocks pages. The gallery shell (window, navigation, title strip and
// page host) is in gallery.h.

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/widgets/common.h>

#include "pages/pages.h"

namespace facet::demo
{

// Building blocks for the demo pages, drawn with the text style and color tokens.

// The page title (Title1) and a one-paragraph summary under it.
void PageHeader(const char* title, const char* summary);
// A section heading (Title3) with the section spacing above it.
void SectionHeader(const char* title);
// Wrapped body text in labelSecondary.
void Paragraph(const char* text);
// Footnote text in labelSecondary, printf formatted, wrapped at the content width.
void Note(const char* fmt, ...) IM_FMTARGS(1);
// The Note every widget family page shows under its header: that its widgets work as documented,
// and where `family` ("lists") is declared and documented.
void ReadyFamilyNote(const char* family);
// A Note on the line of the last item (call after ImGui::SameLine): its first line of text is
// centered on the item's height, wherever the item's text sits.
void NoteBeside(const char* fmt, ...) IM_FMTARGS(1);
// Starts a settings row: `label` in Footnote labelSecondary in the label column of
// facet::LayoutLabeledRow (the layout tokens), centered on a line `lineHeight` pixels tall (0: the
// regular control height), with the cursor placed after the column for the control (submit the
// control with a "##" label). A row of controls taller than Regular passes the height of its line,
// the same value its MixedHeightRow takes, so the label lines up with the controls' text. When the
// content is too narrow for the column and a control, the label goes on its own line above the
// control. The Foundations and Media pages label their settings this way; the Blocks pages use
// the row layout block (src/internal/row_layout.h) instead.
void LabeledRow(const char* label, float lineHeight = 0.0f);
// The box the last LabeledRow label is centered in (the label column by the height of its line),
// in screen pixels, so a page can check in its selftest that the label and the controls of its
// row share one center line.
ImRect LastLabeledRowBox();

// Prototype controls for the Blocks pages, built only from the interaction blocks in
// src/internal (item behaviour, focus ring, value drag core, multi-handle model, selection
// pill). They are not the finished widgets: each draws the plainest look that shows its block
// working, and the pages label them as prototypes. Every one records probes for selftests:
// `probe` for the whole control, and for sliders "<probe>.track" (the line the knob centres
// travel along, so a target fraction is a value fraction) and "<probe>.h<i>" per handle.
// Lengths are in design units; 0 takes the available width.

// A push button with `label` as its text. Returns true when pressed. `repeat` presses again
// while held. `selected` marks the current entry of a group: the button leaves out its fill, so
// the group's selection pill (src/internal/selection_pill.h) drawn behind it shows, and draws
// its label in semibold (PushPillLabelStyle), the cue that does not depend on color. Measure a
// selected button with ProtoButtonWidth.
bool ProtoButton(PageCtx& ctx, const char* label, const char* probe, bool repeat = false, bool selected = false);
// The width of a ProtoButton with this label, in pixels.
float ProtoButtonWidth(const char* label, bool selected = false);
// An on/off switch with the label in front; Space, Enter or a click flips *value. Returns
// true when *value was written.
bool ProtoToggle(PageCtx& ctx, const char* label, bool* value, const char* probe);

struct ProtoSliderOptions
{
    double min = 0.0;
    double max = 1.0;
    double step = 0.0;
    bool integer = false;
    facet::ValueScale scale = facet::ValueScale::Linear;
    facet::Orientation orientation = facet::Orientation::Horizontal;
    std::span<const double> stops = {};
    // Units along the track. Horizontal: > 0 that many units, otherwise the width precedence of
    // the API conventions (SetNextItemWidth, PushItemWidth, then the available width).
    // Vertical: 0 takes five times spacing.xxxl.
    float length = 0.0f;
    bool readOnly = false;
};
// A single value slider on the value drag core. `id` is never shown. Returns true when *value
// was written.
bool ProtoSlider(PageCtx& ctx, const char* id, double* value, const ProtoSliderOptions& options, const char* probe);

struct ProtoRangeOptions
{
    ProtoSliderOptions slider;
    double minGap = 0.0;
    bool push = false;        // push the neighbour instead of stopping at it
    bool mirror = false;      // two handles move symmetrically around the middle of the range
    bool spanDrag = true;
    bool removeKeys = false;  // Delete or Backspace on a focused handle asks for its removal
    std::span<const ImGuiID> keys = {};  // one identity per handle when handles come and go
};
// A slider with `count` handles on the multi-handle model. Returns true when a value was
// written; *removeRequested (when not null) receives the handle whose removal was asked for, or -1.
bool ProtoRange(PageCtx& ctx, const char* id, double* values, int count, const ProtoRangeOptions& options,
                const char* probe, int* removeRequested = nullptr);

// A segmented control: one Tab stop, the arrows move the selection, the selection pill slides
// between the segments. Returns true when *index was written. Segment i records "<probe>.<i>".
bool ProtoSegmented(PageCtx& ctx, const char* id, int* index, std::span<const char* const> items, const char* probe,
                    float width = 0.0f);

// Records a probe for a rectangle that is not Dear ImGui's last item (a handle, a track).
void ProbeRect(PageCtx& ctx, const char* name, ImGuiID id, ImVec2 min, ImVec2 max);
// A probe of a part of a floating layer (a popup level, a modal, a toast, the watermark), which can
// lie outside the page's visible area: recorded against the display, not the page's clip rect, so
// the selftest reaches it without scrolling the page. FloatingProbe takes the id of `name`.
void FloatingProbeRect(PageCtx& ctx, const char* name, ImGuiID id, ImVec2 min, ImVec2 max);
void FloatingProbe(PageCtx& ctx, const char* name, const ImRect& rect);

// Row wrapping for narrow windows and large scales. ContentRight() is the right edge of the
// content area at the cursor; take it at the start of a row. SameLineIfFits continues the
// row after the last item when an item `nextWidth` wide still ends before `right`, and
// otherwise lets the next item start a new row. Returns whether the row continues.
float ContentRight();
bool SameLineIfFits(float right, float nextWidth, float spacing = -1.0f);
// Widths of stock controls for SameLineIfFits.
float ButtonWidth(const char* label);
float CheckboxWidth(const char* label);

// A width option of `units` design units, never more than the room left in the row (also in
// units), so a demo control with a set width stays inside a narrow window or at a large scale.
float DemoWidth(float units);

// Places controls of different heights side by side, each centered on a line `lineHeight` pixels
// tall (the tallest control of the line, such as ControlHeight(Large)), so their labels line up
// instead of hanging from the top. In a compact window each control goes on a line of its own at
// the column the row started in. Call Item with the control's height before each control.
class MixedHeightRow
{
public:
    explicit MixedHeightRow(float lineHeight, float spacing = -1.0f);
    void Item(float height);

private:
    float top_ = 0.0f;
    float column_ = 0.0f;
    float lineHeight_ = 0.0f;
    float spacing_ = -1.0f;
    int count_ = 0;
    bool stack_ = false;
};

} // namespace facet::demo
