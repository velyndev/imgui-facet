#pragma once

// Looks shared by the display widgets (facet/widgets/display.h): the colors of a tone, the
// placement of marks that sit on a line of text (badges, pills, key caps, inline code), key caps,
// placed on a line of text, the "copied" feedback, the progress tone and the dismiss key of alerts. Every name carries the
// family, because every family links into the same library.
//
// Tone colors (DisplayToneLook) follow what the theme guarantees (theme.h, test_contrast.cpp):
//   - text: readable as text (4.5:1) on the content surfaces: label for Neutral, accentText for
//     Accent, the status color for the others;
//   - glyph: an icon or dot that must be seen (3:1) on the content surfaces and on the tone's wash;
//   - wash: the translucent tone background of badges, pills, banners and alerts, on which label
//     and labelSecondary stay readable; text on a wash is therefore always label;
//   - solid and onSolid: a filled mark and the text on it (4.5:1).
// A tone never carries meaning alone: every widget pairs it with an icon or a word.
//
// Inline marks: a badge in a line of text sits on the line's text baseline, so it reads as part of
// the sentence at every scale:
//
//     const DisplayInlineBox box = DisplayPlaceInline(window, id, ImVec2(width, height), baselineInBox, style);
//     if (box.visible) { ... draw in box.rect, text top at DisplayTextTop(box.baseline) ... }

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"

namespace facet::internal
{

// ---- Tones ------------------------------------------------------------------------------------

struct DisplayToneColors
{
    ImVec4 text;     // readable text in the tone on the content surfaces
    ImVec4 glyph;    // icons and dots, 3:1 on the content surfaces and on `wash`
    ImVec4 wash;     // translucent tone background; text on it is label
    ImVec4 solid;    // a filled mark
    ImVec4 onSolid;  // text and glyphs on `solid`
};

// The colors of `tone`. Disabled: every part takes the disabled look (labelTertiary, fill[3]).
DisplayToneColors DisplayToneLook(Tone tone, bool disabled);

// The icon a tone shows when the caller gives none: info for Neutral and Accent, success,
// warning and error for the status tones.
Icon DisplayToneIcon(Tone tone);

// The fill of a progress mark (ProgressBar, ProgressRing, CharacterCounter ring): 3:1 on the
// content surfaces. The accent through focusRing, labelSecondary for Neutral, the status colors.
ImVec4 DisplayProgressColor(Tone tone);

// ---- Text lines -------------------------------------------------------------------------------

// The ascent of the current font at the current size, in pixels (from the line top to the
// baseline).
float DisplayFontAscent();

// The y at which text in the current font sits on `baseline` (rounded to whole pixels).
float DisplayTextTop(float baseline);

// The y of the middle of the capital letters of a line of text in the current font whose top is
// at `textTop`; icons and dots beside text are centered on it.
float DisplayCapsCenter(float textTop);

// A mark placed on the current line of text.
struct DisplayInlineBox
{
    ImRect rect;            // the mark, screen pixels
    float baseline = 0.0f;  // the text baseline inside it, screen pixels
    bool visible = false;   // ItemAdd accepted it (not clipped away)
};

// Lays out an item of `size` pixels whose text baseline lies `baselineInBox` pixels below its top,
// so that the baseline meets the baseline of `lineStyle` text on the same line: text drawn before
// it (ImGui::SameLine) and after it line up with the mark at every scale. On a line of its own the
// mark starts at the cursor. Registers the item under `id` with `itemFlags` (ImGuiItemFlags_NoNav
// for a mark that is not a Tab stop). Call with no font pushed that differs from the window's.
DisplayInlineBox DisplayPlaceInline(ImGuiWindow* window, ImGuiID id, ImVec2 size, float baselineInBox,
                                    TextStyle lineStyle, ImGuiItemFlags itemFlags = ImGuiItemFlags_NoNav);

// The layout part of DisplayPlaceInline alone (ItemSize, no ItemAdd), for a mark that registers
// itself as an interactive part (PartBehavior) at box.rect; box.visible is left false.
DisplayInlineBox DisplayLayoutInline(ImGuiWindow* window, ImVec2 size, float baselineInBox, TextStyle lineStyle);

// The id of a widget that takes no label (Skeleton, KeyCap, CountBadge): `name` ("##Skeleton")
// hashed in the current window, never the window's own id.
ImGuiID DisplayAnonymousId(ImGuiWindow* window, const char* name);

// ---- Status dots --------------------------------------------------------------------------------

// A status dot of `diameter` pixels around `center` in `color`. With `pulse`, a ring grows out of
// it and fades once per motion.pulsePeriod (frame-rate independent, on Dear ImGui's clock); the
// ring is left out while motion is reduced, so the dot stands still.
void DrawDisplayDot(ImDrawList* drawList, ImVec2 center, float diameter, ImU32 color, bool pulse);

// ---- Key caps ---------------------------------------------------------------------------------

// The cap names and colors are the shared ones of internal/key_caps.h (KeyCapNames, KeyCapLook);
// the metrics below place them on a line of text.

// The metrics of a mark of a size (Small, Regular, Large): badges, pills, key caps, inline code.
struct DisplayMarkMetrics
{
    TextStyle textStyle = TextStyle::Footnote;  // the key names
    TextStyle lineStyle = TextStyle::Body;      // the text line the caps sit on
    float height = 0.0f;   // one cap, pixels
    float paddingX = 0.0f; // inside a cap
    float gap = 0.0f;      // between caps
    float radius = 0.0f;
};
DisplayMarkMetrics DisplayMarkLook(ControlSize size);
// Pixels from the top of a mark of `m` to the baseline of its text, centered in the mark.
float DisplayMarkBaseline(const DisplayMarkMetrics& m);

// The width in pixels of the caps of `chord` (Strings::unbound for ImGuiKey_None).
float DisplayCapsWidth(ImGuiKeyChord chord, const DisplayMarkMetrics& m);

// Draws the caps of `chord` with their top-left at `pos`, the key names on `baseline`. `pressed`
// shows the held look; `disabled` the disabled colors.
void DrawDisplayCaps(ImDrawList* drawList, ImVec2 pos, float baseline, ImGuiKeyChord chord, const DisplayMarkMetrics& m,
                     bool pressed, bool disabled);

// ---- Copy feedback ----------------------------------------------------------------------------

// Starts the "copied" feedback of item `id`: Strings::copied in a tooltip level above `anchor`
// for CopyFeedbackHold() (internal/copy_feedback.h).
void StartDisplayCopiedFeedback(ImGuiID id, const ImRect& anchor);
// Call every frame after the item: shows the feedback while it lasts. Returns true while shown.
bool DisplayCopiedFeedback(ImGuiID id, const ImRect& anchor);

// ---- For tests ----------------------------------------------------------------------------------

// Whether the LogView under `viewId` (its id hashed in its window) follows new entries; false for a
// view never drawn.
bool DisplayLogViewFollowing(ImGuiID viewId);

// ---- Dismissing alerts --------------------------------------------------------------------------

// For an item of a dismissible alert (its close button or an action), right after the item:
// while it has keyboard focus it takes ImGuiKey_Escape from Dear ImGui's navigation for the next
// frame, and returns true on the frame Escape is pressed while it owns the key.
bool DisplayDismissKey(ImGuiID itemId);

} // namespace facet::internal
