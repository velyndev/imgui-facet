// The ordered subsystem table. See src/internal/subsystem.h for the hook rules.
//
// Init, NewFrame and EndFrame run in table order, Shutdown in reverse, so every module still
// finds what it depends on. The order and its reasons:
//
// Core, in place since Phase 1b:
//   theme, scale       plain token state that every later module reads; theme's EndFrame
//                      also closes accent scopes left open
//   state storage      per-widget state other modules may keep entries in
//   anim               motion store; reads theme motion tokens
//   font               font atlas setup; needs the scale
//   texture            image and texture cache; the icon atlas uploads through it
//   icon               icon atlas; rasterized per scale
//   shadow             shadow and glow atlas; rasterized per scale
//   blur               backdrop blur pass; optional, it may fall back
//
// Interaction and surface blocks (Phase 1c), before every widget:
//   input              key chord state and the shortcut triggers of the frame: plain input
//                      bookkeeping that the later blocks may read
//   focus              keyboard modality (whether focus rings show), decided before any
//                      widget draws; EndFrame drops forced item states no widget took
//   popup host         the open popups and the closing ones it replays, and the Escape and
//                      context menu key routing between layers (src/internal/popup_host.h);
//                      before scroll, so a wheel turn over a popup belongs to the popup
//   scroll             smooth wheel and overlay scrollbars; before drag and drop, which
//                      scrolls a list while a drag rests near its edge
//   drag and drop      the drag in progress; at EndFrame its preview is drawn after the
//                      popups, on the foreground layer, so it stays on top
//   text field         per-frame state of the text field core
//
// Widgets (Phase 1c and 2), after the blocks they build on:
//   widgets common     the control size stack; EndFrame closes sizes left pushed
//   one entry per family or part of a family that is built on its own (buttons, selection,
//                      sliders, text inputs, value inputs, lists, containers, display, color
//                      pickers, date and other pickers, navigation, layout, overlays), in that
//                      order; each fills its own accessor in a file of its own. The overlays'
//                      EndFrame runs after every page has drawn (the toast stack).
//   widgets structure  forgets the placeholder frames of structure_stub.h left open
//   DataTable, charts, media, composites, in that order, each with an entry of its own
//
// Last:
//   scopes             EndFrame reports Begin / End pairs still open, after every module
//                      above has drawn, so a scope a module opens in its own EndFrame counts

#include "internal/subsystem.h"

#include <array>

namespace facet::internal
{

std::span<const Subsystem> Subsystems()
{
    static const std::array<Subsystem, 35> table = {
        ThemeSubsystem(),
        ScaleSubsystem(),
        StateStorageSubsystem(),
        AnimSubsystem(),
        FontSubsystem(),
        TextureSubsystem(),
        IconSubsystem(),
        ShadowSubsystem(),
        BlurSubsystem(),
        InputSubsystem(),
        FocusSubsystem(),
        PopupHostSubsystem(),
        ScrollSubsystem(),
        DragDropSubsystem(),
        TextFieldSubsystem(),
        WidgetsCommonSubsystem(),
        WidgetsButtonsSubsystem(),
        WidgetsSelectionSubsystem(),
        WidgetsSlidersSubsystem(),
        WidgetsTextInputsSubsystem(),
        WidgetsValueInputsSubsystem(),
        WidgetsListsSubsystem(),
        WidgetsContainersSubsystem(),
        WidgetsDisplaySubsystem(),
        WidgetsColorPickersSubsystem(),
        WidgetsDatePickersSubsystem(),
        WidgetsNavigationSubsystem(),
        WidgetsLayoutSubsystem(),
        WidgetsOverlaysSubsystem(),
        WidgetsStructureSubsystem(),
        WidgetsDataTableSubsystem(),
        WidgetsChartsSubsystem(),
        WidgetsMediaSubsystem(),
        WidgetsCompositesSubsystem(),
        ScopesSubsystem(),
    };
    return table;
}

} // namespace facet::internal
