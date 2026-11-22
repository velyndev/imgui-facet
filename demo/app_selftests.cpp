// Selftests of the app around the pages: the title bar and its window buttons, the navigation
// with its filter and icon rail, the theme editor (every group, the resets and the export) and
// overlay mode (show, hide, move, resize and the size limits). They start on the showcase page.

#include <cmath>
#include <cstring>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "selftest.h"
#include "theme_editor.h"

namespace
{

using facet::demo::CurrentGallery;
using facet::demo::GalleryState;
using facet::demo::Target;
using facet::demo::ThemeEdits;

GalleryState& Gallery()
{
    return *CurrentGallery();
}

bool ClipboardHas(const char* text)
{
    const char* clipboard = ImGui::GetClipboardText();
    return clipboard != nullptr && std::strstr(clipboard, text) != nullptr;
}

float ShellWidth()
{
    return Gallery().shellMax.x - Gallery().shellMin.x;
}

float ShellHeight()
{
    return Gallery().shellMax.y - Gallery().shellMin.y;
}

} // namespace

FACET_DEMO_APP_SELFTEST("title-bar", "showcase")
{
    test.Expect("the app opens on the showcase, the first page of the navigation",
                [] { return facet::demo::FindPage("showcase") == 0; });
    test.ExpectPage("showcase");

    // The window buttons by mouse: offscreen they only record their press.
    test.Do([] { Gallery().lastWindowButton = facet::WindowButton::None; });
    test.Click("gallery.minimize");
    test.Expect("the minimize button reports its press",
                [] { return Gallery().lastWindowButton == facet::WindowButton::Minimize; });
    test.Click("gallery.maximize");
    test.Expect("the maximize button reports its press",
                [] { return Gallery().lastWindowButton == facet::WindowButton::Maximize; });

    // And by keyboard: after the click the first Tab shows the focus where it is, the next moves on.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("gallery.maximize");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("gallery.close");
    test.Do([] { Gallery().lastWindowButton = facet::WindowButton::None; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the close button reports it", [] { return Gallery().lastWindowButton == facet::WindowButton::Close; });
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("gallery.minimize");
    test.Do([] { Gallery().lastWindowButton = facet::WindowButton::None; });
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the minimize button reports it",
                [] { return Gallery().lastWindowButton == facet::WindowButton::Minimize; });

    // The theme mode beside the title, by mouse and keyboard (shown from the Regular width up).
    test.Do([] {
        Gallery().dark = true;
        Gallery().followSystem = false;
    });
    test.Wait(2);
    test.MouseMove(Target::Item("gallery.theme", 0.15f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.Expect("a click on the first segment shows the light theme", [] { return !facet::T().dark; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.Expect("Right in the theme control chooses Dark", [] { return facet::T().dark && !Gallery().followSystem; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.Expect("Right again follows the system", [] { return Gallery().followSystem; });
    test.Do([] {
        Gallery().dark = true;
        Gallery().followSystem = false;
    });

    // The theme editor button opens and closes the pane.
    test.Click("gallery.editor");
    test.Expect("the editor button opens the theme editor", [] { return Gallery().editorOpen; });
    test.WaitSeconds(0.5f);
    test.ExpectVisible("editor.theme");
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus where it is
    test.ExpectFocused("gallery.editor");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the button closes it again", [] { return !Gallery().editorOpen; });
}

FACET_DEMO_APP_SELFTEST("navigation", "showcase")
{
    // The rail: the navigation shows only icons where the width is not Expanded, and its button
    // shows the labels and the filter.
    test.Expect("the navigation is a rail exactly when the window is narrower than the Expanded width", [] {
        const float units = ImGui::GetIO().DisplaySize.x / facet::Scale();
        return Gallery().navRail == (units - facet::T().spacing.sm < facet::T().layout.expandedFrom);
    });
    // In either form a click opens a page and the arrows and Space open the next one.
    test.Click("nav.type");
    test.ExpectPage("type");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectFocused("nav.tokens");
    test.KeyPress(ImGuiKey_Space);
    test.ExpectPage("tokens");
    test.Do([] { Gallery().navExpanded = true; });
    test.Wait(10);
    test.Expect("the labels show on request", [] { return !Gallery().navRail; });

    // A click opens a page; the arrows move between the rows and Space opens one.
    test.Click("nav.tokens");
    test.ExpectPage("tokens");
    test.ExpectFocused("nav.tokens");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectFocused("nav.type");
    test.KeyPress(ImGuiKey_Space);
    test.ExpectPage("type");

    // The filter keeps the pages whose title or group contains the text.
    test.Click("nav.filter");
    test.Type("chart");
    test.Wait(2);
    test.ExpectVisible("nav.charts");
    test.ExpectNotVisible("nav.tokens");
    test.Type("zzzz");
    test.Wait(2);
    test.ExpectVisible("nav.empty");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape in the filter clears it", [] { return Gallery().navFilter.empty(); });
    test.Wait(2);
    test.ExpectVisible("nav.showcase");

    // A page selected from elsewhere is scrolled into view.
    test.SelectPage("composites-editors");
    test.ExpectVisible("nav.composites-editors");
    test.ExpectNotVisible("nav.tokens");
    test.Click("nav.tokens");
    test.HoldKey(ImGuiKey_DownArrow, 3.0f);
    test.ExpectFocused("nav.composites-editors");
    test.ExpectVisible("nav.composites-editors");
    test.HoldKey(ImGuiKey_UpArrow, 3.0f);
    test.ExpectFocused("nav.showcase");
    test.ExpectVisible("nav.showcase");
    test.Do([] { Gallery().navExpanded = false; });
}

FACET_DEMO_APP_SELFTEST("theme-editor", "showcase")
{
    using facet::demo::ThemeEditGroup;
    static facet::Theme base;
    test.Do([] {
        Gallery().edits = ThemeEdits{};
        Gallery().uiScale = 1.0f;
        Gallery().editorOpen = true;
        Gallery().editorTokensOpen = {};
    });
    test.WaitSeconds(0.5f);
    test.Do([] { base = facet::T(); });

    // Every group: dragging its factor changes the live theme the page draws with.
    test.Drag({Target::Item("editor.radii.factor", 0.6f), Target::Item("editor.radii.factor", 0.95f)}, 0.3f);
    test.Wait(2);
    test.Expect("the radius factor rounds the corners", [] { return facet::T().radii.card > base.radii.card; });
    test.Drag({Target::Item("editor.spacing.factor", 0.6f), Target::Item("editor.spacing.factor", 0.95f)}, 0.3f);
    test.Wait(2);
    test.Expect("the spacing factor applies on release", [] { return facet::T().spacing.md > base.spacing.md; });
    test.Drag({Target::Item("editor.type.factor", 0.6f), Target::Item("editor.type.factor", 0.95f)}, 0.3f);
    test.Wait(2);
    test.Expect("the type factor grows the text", [] { return facet::T().type.styles[5].size > base.type.styles[5].size; });
    test.Drag({Target::Item("editor.heights.factor", 0.6f), Target::Item("editor.heights.factor", 0.95f)}, 0.3f);
    test.Wait(2);
    test.Expect("the control height factor grows the controls",
                [] { return facet::T().metrics.controlHeight > base.metrics.controlHeight; });
    test.Drag({Target::Item("editor.motion.factor", 0.6f), Target::Item("editor.motion.factor", 0.95f)}, 0.3f);
    test.Wait(2);
    test.Expect("the motion factor slows the motion", [] { return facet::T().motion.scale > 1.0f; });
    test.Click("editor.reduce-motion");
    test.Expect("the switch reduces motion", [] { return facet::T().motion.reduceMotion; });
    test.Drag({Target::Item("editor.blur.factor", 0.6f), Target::Item("editor.blur.factor", 0.05f)}, 0.3f);
    test.Wait(2);
    test.Expect("the blur factor weakens the blur", [] { return facet::T().glass.blurRadius < base.glass.blurRadius; });
    test.Drag({Target::Item("editor.shadow.factor", 0.6f), Target::Item("editor.shadow.factor", 0.95f)}, 0.3f);
    test.Wait(2);
    test.Expect("the shadow factor strengthens the shadows",
                [] { return facet::T().elevation.levels[1].opacity > base.elevation.levels[1].opacity; });

    // The keyboard: a focused factor slider steps with the arrows.
    test.Click("editor.radii.reset");
    test.Expect("the radius reset returns to the preset", [] {
        return std::fabs(facet::T().radii.card - base.radii.card) < 0.01f &&
               Gallery().edits.factor[static_cast<int>(ThemeEditGroup::Radii)] == 1.0f;
    });
    test.Click("editor.radii.factor");
    test.Do([] { Gallery().edits.factor[static_cast<int>(ThemeEditGroup::Radii)] = 1.0f; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(2);
    test.Expect("Right steps the focused factor and the corners follow",
                [] { return facet::T().radii.card > base.radii.card; });

    // One token on its own, inside the group's disclosure.
    test.Click("editor.radii.tokens");
    test.WaitSeconds(0.5f);
    test.Expect("the disclosure opens the token rows", [] { return Gallery().editorTokensOpen[0]; });
    test.Drag({Target::Item("editor.token.radii.element", 0.6f), Target::Item("editor.token.radii.element", 0.05f)}, 0.3f);
    test.Wait(2);
    test.Expect("a token row sets that token alone",
                [] { return Gallery().edits.overridden[0] && facet::T().radii.element < base.radii.element; });

    // The export names what was changed.
    test.Click("editor.copy");
    test.Expect("Copy as C++ puts the edited tokens on the clipboard", [] {
        return ClipboardHas("inline void ApplyThemeEdits(facet::Theme& theme)") && ClipboardHas("theme.radii.element = ") &&
               ClipboardHas("theme.spacing.md = ") && ClipboardHas("theme.metrics.controlHeight = ") &&
               ClipboardHas("theme.motion.reduceMotion = true;") && ClipboardHas("theme.glass.blurRadius = ");
    });
    test.Expect("the export text equals what the editor builds",
                [] { return std::string(ImGui::GetClipboardText()) == facet::demo::ThemeEditorExportText(Gallery()); });

    // The accent: a swatch other than the theme default sets a custom accent.
    test.MouseMove(Target::Item("editor.accent", 0.3f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.Expect("a swatch sets the accent", [] { return !Gallery().edits.themeDefaultAccent && facet::HasCustomAccent(); });

    // Reset all returns everything to the preset.
    test.Click("editor.reset-all");
    test.Wait(2);
    test.Expect("Reset all returns every edit", [] { return Gallery().edits == ThemeEdits{}; });
    test.Expect("and the live theme to the preset", [] {
        return std::fabs(facet::T().spacing.md - base.spacing.md) < 0.01f &&
               std::fabs(facet::T().metrics.controlHeight - base.metrics.controlHeight) < 0.01f &&
               !facet::T().motion.reduceMotion && !facet::HasCustomAccent();
    });
    test.Click("editor.copy");
    test.Expect("an unedited theme exports no token", [] { return !ClipboardHas("    theme."); });
}

FACET_DEMO_APP_SELFTEST("overlay", "showcase")
{
    test.Do([] {
        Gallery().editorOpen = true;
        Gallery().overlayMode = false;
    });
    test.WaitSeconds(0.5f);  // the pane slides in
    test.Click("editor.overlay");
    test.Wait(20);
    test.Expect("the editor's switch turns overlay mode on", [] { return Gallery().overlayMode; });
    test.Do([] { Gallery().editorOpen = false; });
    test.Wait(10);
    test.ExpectVisible("overlay.shell");
    test.Expect("the overlay reports the shell as an input region",
                [] { return facet::OverlayHitTest(ImVec2((Gallery().shellMin.x + Gallery().shellMax.x) * 0.5f,
                                                         (Gallery().shellMin.y + Gallery().shellMax.y) * 0.5f)); });

    // Hide and show with the key.
    test.KeyPress(ImGuiKey_F9);
    test.WaitSeconds(0.5f);
    test.Expect("F9 hides the menu", [] { return !Gallery().overlayVisible; });
    test.Expect("a hidden menu takes no input", [] {
        facet::ScreenRegion regions[4];
        return facet::GetOverlayInputRegions(regions) == 0 && !facet::OverlayWantsMouse();
    });
    test.KeyPress(ImGuiKey_F9);
    test.WaitSeconds(0.5f);
    test.Expect("F9 shows it again", [] { return Gallery().overlayVisible; });

    // Resize at the edges and a corner; the opposite edges stay.
    static ImVec2 before;
    static ImVec2 size;
    const float band = facet::Theme::Light().containers.resizeBorder * 0.5f;
    test.Do([] { size = ImVec2(ShellWidth(), ShellHeight()); });
    test.Drag({Target::Item("overlay.shell", 1.0f, 0.5f).Offset(-band, 0.0f),
               Target::Item("overlay.shell", 1.0f, 0.5f).Offset(-band - 40.0f, 0.0f)},
              0.3f);
    test.Wait(2);
    test.Expect("the right edge narrows the shell", [] { return ShellWidth() < size.x - 1.0f; });
    test.Do([] { size = ImVec2(ShellWidth(), ShellHeight()); before = Gallery().shellMin; });
    test.Drag({Target::Item("overlay.shell", 0.0f, 0.5f).Offset(band, 0.0f),
               Target::Item("overlay.shell", 0.0f, 0.5f).Offset(band + 30.0f, 0.0f)},
              0.3f);
    test.Wait(2);
    test.Expect("the left edge narrows it and moves only the left side",
                [] { return ShellWidth() < size.x - 1.0f && Gallery().shellMin.x > before.x; });
    test.Do([] { size = ImVec2(ShellWidth(), ShellHeight()); });
    test.Drag({Target::Item("overlay.shell", 0.5f, 1.0f).Offset(0.0f, -band),
               Target::Item("overlay.shell", 0.5f, 1.0f).Offset(0.0f, -band - 30.0f)},
              0.3f);
    test.Wait(2);
    test.Expect("the bottom edge lowers it", [] { return ShellHeight() < size.y - 1.0f; });
    test.Do([] { size = ImVec2(ShellWidth(), ShellHeight()); before = Gallery().shellMin; });
    test.Drag({Target::Item("overlay.shell", 0.5f, 0.0f).Offset(0.0f, band),
               Target::Item("overlay.shell", 0.5f, 0.0f).Offset(0.0f, band + 20.0f)},
              0.3f);
    test.Wait(2);
    test.Expect("the top edge lowers it from the top",
                [] { return ShellHeight() < size.y - 1.0f && Gallery().shellMin.y > before.y; });

    // The minimum size holds when a corner is dragged far inward.
    test.Drag({Target::Item("overlay.shell", 1.0f, 1.0f).Offset(-band, -band), Target::Display(0.0f, 0.0f)}, 0.4f);
    test.Wait(2);
    test.Expect("the shell stops at its minimum size", [] {
        const float minWidth = facet::Px(facet::T().spacing.xxxl * 11.0f);
        const float minHeight = facet::Px(facet::T().spacing.xxxl * 8.0f);
        return ShellWidth() >= minWidth - 1.0f && ShellHeight() >= minHeight - 1.0f && ShellWidth() <= minWidth + 1.0f;
    });
    // Move, now that the shell is smaller than the display: drag its title bar.

    test.Do([] {
        before = Gallery().shellMin;
        size = ImVec2(ShellWidth(), ShellHeight());
    });
    const float titleMiddle = facet::Theme::Light().containers.titleBarHeight * 0.5f;
    test.Drag({Target::Item("overlay.shell", 0.5f, 0.0f).Offset(0.0f, titleMiddle),
               Target::Item("overlay.shell", 0.5f, 0.0f).Offset(-40.0f, titleMiddle + 30.0f)},
              0.3f);
    test.Wait(2);
    test.Expect("dragging the title bar moves the shell", [] {
        return std::fabs(Gallery().shellMin.x - before.x) > 1.0f || std::fabs(Gallery().shellMin.y - before.y) > 1.0f;
    });

    // And the maximum size, or the display, when it is dragged far outward.
    test.Drag({Target::Item("overlay.shell", 1.0f, 1.0f).Offset(-band, -band), Target::Display(1.5f, 1.5f)}, 0.4f);
    test.Wait(2);
    test.Expect("the shell stops at its maximum size or the display", [] {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float maxWidth = facet::Px(facet::T().spacing.xxxl * 50.0f);
        return ShellWidth() <= std::fmin(maxWidth, display.x) + 1.0f && Gallery().shellMax.y <= display.y + 1.0f;
    });

    // Back to app mode with the toggle in the editor.
    test.Do([] { Gallery().editorOpen = true; });
    test.WaitSeconds(0.5f);
    test.Click("editor.overlay");
    test.Wait(10);
    test.Expect("the switch returns to app mode", [] { return !Gallery().overlayMode; });
}
