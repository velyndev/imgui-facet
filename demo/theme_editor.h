#pragma once

// The theme editor of the test app: a pane beside the page that edits the live theme (mode,
// accent, radii, spacing, type sizes, control heights, motion, blur and shadow strength, the UI
// scale and the app or overlay mode) with Facet widgets only, resets a group or everything, and
// copies the edits as C++ (theme_export.h).

#include "gallery.h"

namespace facet::demo
{

// The preset the edits apply to: Theme::Dark() or Theme::Light().
Theme ThemeEditorPreset(bool dark);

// The title of the editor pane; its id is fixed ("###theme-editor").
inline constexpr const char* kThemeEditorTitle = "Theme editor###theme-editor";

// Draws the editor's controls into the current window (the content of the inspector pane).
// Writes state.edits, state.dark, state.followSystem, state.uiScale, state.overlayMode and
// state.backdropPaused; DrawGallery applies them. Probes: "editor.<control>".
void DrawThemeEditor(GalleryState& state, PageCtx& ctx);

// A UI scale control (75 to 200 percent) for `state`: a drag applies on release, the buttons and
// keys at once. Shared by the title bar and the editor.
void GalleryUiScaleControl(GalleryState& state, const char* label, float widthUnits);

// The C++ text of the current edits, as "Copy as C++" puts it on the clipboard.
std::string ThemeEditorExportText(const GalleryState& state);

} // namespace facet::demo
