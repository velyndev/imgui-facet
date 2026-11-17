#pragma once

#include <cstdint>
#include <span>

#include <facet/widgets/common.h>

#include "pages/pages.h"

namespace facet::demo
{

class AppShell;

struct GalleryState
{
    int selectedPage = 0;      // index into Pages()
    bool dark = true;          // theme shown while followSystem is off
    bool followSystem = false; // show the Windows app mode instead of `dark`
    float uiScale = 1.0f;      // user scale; the title strip control changes it
    float baseScale = 1.0f;    // set by the caller every frame: monitor DPI / 96 in the window,
                               // --scale offscreen

    // Theme mode and total scale last applied to Facet. DrawGallery applies them again
    // whenever they differ from the values above. -1: nothing applied yet; the first frame
    // also forgets an accent left over from an earlier session.
    int appliedMode = -1;
    float appliedScale = 0.0f;

    // The page whose navigation row was last scrolled into view; while it differs from
    // selectedPage, the navigation scrolls the selected row into view.
    int revealedPage = -1;
};

// io settings shared by all modes. Deterministic runs (capture, selftest) also disable
// the text cursor blink and mouse cursor changes so frames depend only on the input.
void ConfigureImGuiIo(bool deterministic);

// Sets the DPI part of the Facet scale and writes the current theme and scale into the
// ImGui style (facet::ApplyImGuiStyle). Called once before Facet starts.
void ApplyStyle(float scale);

// Opaque clear color behind the UI: the theme's background.
void ClearColor(float out[4]);

// Draws the whole demo UI: title strip, page navigation and the selected page.
// First applies the theme mode and scale of `state` when they changed (facet::SetThemeMode,
// facet::SetDpiScale, facet::SetUiScale, facet::ApplyImGuiStyle; the style then follows
// every theme transition through facet::SyncImGuiStyle), and sets ctx.dark and ctx.scale.
// Theme switches animate in the window; capture and selftest runs (ctx.capture or
// ctx.probes set) switch at once and ignore the Windows animation setting, so their frames
// do not depend on timing or on the machine. Must run before any ImGui window is begun.
// With a shell, the title strip moves the window and its buttons control it; without one
// (offscreen runs) the strip is drawn the same way and the window buttons do nothing.
void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell);

// Building blocks for the demo pages, drawn with the text style and color tokens.

// The page title (Title1) and a one-paragraph summary under it.
void PageHeader(const char* title, const char* summary);
// A section heading (Title3) with the section spacing above it.
void SectionHeader(const char* title);
// Wrapped body text in labelSecondary.
void Paragraph(const char* text);
// Footnote text in labelSecondary, printf formatted, wrapped at the content width.
void Note(const char* fmt, ...) IM_FMTARGS(1);
// A Note on the line of the last item (call after ImGui::SameLine): its first line of text is
// centered on the item's height, wherever the item's text sits.
void NoteBeside(const char* fmt, ...) IM_FMTARGS(1);
// Starts a settings row: `label` in Footnote labelSecondary in the label column of
// facet::LayoutLabeledRow (the layout tokens), centered on the control height, with the cursor
// placed after the column for the control (submit the control with a "##" label). When the
// content is too narrow for the column and a control, the label goes on its own line above the
// control. The Foundations and Media pages label their settings this way; the Blocks pages use
// the row layout block (src/internal/row_layout.h) instead.
void LabeledRow(const char* label);

// Prototype controls for the Blocks pages, built only from the interaction blocks in
// src/internal (item behaviour, focus ring, value drag core, multi-handle model, selection
// pill). They are not the Phase 2 widgets: each draws the plainest look that shows its block
// working, and the pages label them as prototypes. Every one records probes for selftests:
// `probe` for the whole control, and for sliders "<probe>.track" (the line the knob centres
// travel along, so a target fraction is a value fraction) and "<probe>.h<i>" per handle.
// Lengths are in design units; 0 takes the available width.

// A push button with `label` as its text. Returns true when pressed. `repeat` presses again
// while held.
bool ProtoButton(PageCtx& ctx, const char* label, const char* probe, bool repeat = false);
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

// Row wrapping for narrow windows and large scales. ContentRight() is the right edge of the
// content area at the cursor; take it at the start of a row. SameLineIfFits continues the
// row after the last item when an item `nextWidth` wide still ends before `right`, and
// otherwise lets the next item start a new row. Returns whether the row continues.
float ContentRight();
bool SameLineIfFits(float right, float nextWidth, float spacing = -1.0f);
// Widths of stock controls for SameLineIfFits.
float ButtonWidth(const char* label);
float CheckboxWidth(const char* label);

} // namespace facet::demo
