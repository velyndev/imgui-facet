#pragma once

// The gallery shell: the demo window with its title bar, page navigation, theme editor pane and
// the host that draws the selected page, in app mode (an app frame that fills the window) or in
// overlay mode (a floating shell over a generated backdrop). The building blocks pages call are
// in page_helpers.h. A page includes this file only for NavTitleCut, the one shell query a page
// selftest reads.

#include <array>
#include <cstdint>
#include <span>
#include <string>

#include <facet/widgets/common.h>
#include <facet/widgets/containers.h>

#include "pages/pages.h"
#include "theme_export.h"

namespace facet::demo
{

class AppShell;

struct GalleryState
{
    int selectedPage = 0;      // index into Pages()
    bool dark = true;          // theme shown while followSystem is off
    bool followSystem = false; // show the Windows app mode instead of `dark`
    float uiScale = 1.0f;      // user scale; the title bar and the theme editor change it
    float baseScale = 1.0f;    // set by the caller every frame: monitor DPI / 96 in the window,
                               // --scale offscreen

    // Theme mode, edits and total scale last applied to Facet. DrawGallery applies them again
    // whenever they differ from the values above. -1: nothing applied yet; the first frame
    // also forgets an accent left over from an earlier session.
    int appliedMode = -1;
    bool appliedDark = false;
    float appliedScale = 0.0f;
    ThemeEdits appliedEdits;

    // The page whose navigation row was last scrolled into view; while it differs from
    // selectedPage, the navigation scrolls the selected row into view.
    int revealedPage = -1;

    // Navigation: the filter text, and whether the labels show although the width asks for the
    // icon rail (the expand button of the rail sets it, the collapse button clears it).
    std::string navFilter;
    bool navExpanded = false;
    bool navRail = false;  // the navigation was drawn as the icon rail in the last frame
    int navFocusedPage = -1;  // the row last scrolled into view for the keyboard focus

    // The theme editor pane, and the edits it holds: `edits` is what its controls show, applied
    // at once except while a slider that moves the layout (spacing, type, control heights) is
    // dragged, which applies on release.
    bool editorOpen = false;
    ThemeEdits edits;
    bool editsHeld = false;     // a layout slider was being dragged in the last frame
    std::array<bool, kThemeEditGroupCount> editorTokensOpen = {};  // the "Each token" disclosures
    float uiScaleDrag = -1.0f;  // the UI scale shown while its slider is dragged; -1 while not

    // Overlay mode: the gallery floats as a shell over the generated backdrop. The overlay
    // controller shows and hides it; the backdrop moves unless motion is reduced or paused.
    bool overlayMode = false;
    bool overlayVisible = true;
    bool backdropPaused = false;
    bool overlayInfoPinned = true;  // the information panel stays while the menu is hidden
    ImVec2 shellMin = {0.0f, 0.0f};  // the overlay shell's window in the last frame it showed, pixels
    ImVec2 shellMax = {0.0f, 0.0f};
    double backdropTime = 0.0;  // seconds of backdrop motion so far

    // The window button activated last (offscreen runs have no window to act on; the selftests
    // read it and write None before a step).
    WindowButton lastWindowButton = WindowButton::None;
};

// io settings shared by all modes. Deterministic runs (capture, selftest) also disable
// the text cursor blink and mouse cursor changes so frames depend only on the input.
void ConfigureImGuiIo(bool deterministic);

// Sets the DPI part of the Facet scale and writes the current theme and scale into the
// ImGui style (facet::ApplyImGuiStyle). Called once before Facet starts.
void ApplyStyle(float scale);

// Opaque clear color behind the UI: the theme's background.
void ClearColor(float out[4]);

// Draws the whole demo UI: in app mode the title bar, page navigation, the selected page and the
// theme editor pane; in overlay mode the backdrop and the same UI in a floating shell.
// First applies the theme mode, the theme edits and the scale of `state` when they changed
// (facet::SetThemeMode, facet::SetTheme, facet::SetDpiScale, facet::SetUiScale,
// facet::ApplyImGuiStyle; the style then follows every theme transition through
// facet::SyncImGuiStyle), and sets ctx.dark and ctx.scale. Theme switches animate in the window;
// capture and selftest runs (ctx.capture or ctx.probes set) switch at once and ignore the
// Windows animation setting, so their frames do not depend on timing or on the machine. Must run
// before any ImGui window is begun. With a shell, the window buttons control the window and the
// hit test of the window follows the app frame; without one (offscreen runs) the UI is drawn the
// same way and the window buttons only record their press in state.lastWindowButton.
void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell);

// The gallery drawn in the current frame, or null outside DrawGallery's frame. For selftest
// checks of the shell's own state.
GalleryState* CurrentGallery();

// True when the navigation title of page `pageId` ended in an ellipsis in the last frame (for
// selftests: a cut title shows in full on hover and on keyboard focus). Titles in the icon rail
// are not drawn, so they are never cut.
bool NavTitleCut(const char* pageId);

} // namespace facet::demo
