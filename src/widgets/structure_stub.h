#pragma once

// Placeholders of the planned widgets of the structure families (lists, navigation,
// containers, layout, overlays). Every stub in src/widgets/<family>/ draws through these
// functions, so all of them look the same and none of them looks operable. Phase 2 replaces
// a stub's body with the widget and stops calling them; nothing else depends on this file.
//
// What a stub does, by kind of widget:
//   - A widget in the page flow (field, row, bar, button) calls StubItem once: the placeholder
//     item every planned widget shares (internal::PlannedWidget in
//     src/internal/planned_widget.h) at the widget's default size. The stub returns the neutral
//     result (false, None, -1, an invalid id) and never writes through a value pointer.
//   - A container whose content sits in the page flow calls BeginStubFrame and EndStubFrame:
//     the same header line, then the caller's content, inside one dashed outline. A bool
//     Begin of this kind returns true, so the content shows, unless the caller's own arguments
//     already say the content is hidden: a sidebar in Hidden mode, an overlay whose visible flag
//     is false, a dependent group in Hide mode while its parent is off, the pane a Compact
//     list-detail does not show, an inspector pane whose visible flag is false, a collapsible
//     group box or sidebar section that starts closed, a virtual list, item grid or tree view
//     that is not Ready (or has no rows). Those draw one StubItem line instead, where the
//     finished widget draws its state. A stub never returns true where the declaration promises
//     false. Collapsible sections return false (closed). The frame pushes the Begin's scope (internal::PushScope) and, where the widget
//     scopes ids, the id.
//   - A layer (a window, dialog, popup or drawer) returns false and opens nothing. It calls
//     StubLayer, which draws one placeholder line in the page flow while the caller asks for
//     the layer to be visible (its `open` flag is true), so a page shows the request. Layers
//     opened by input (context menus, tooltips) draw nothing, like the closed widget.
// Outside a frame, without a current window or in Dear ImGui's fallback window, nothing is
// drawn and bool Begins return false (rule 14).
//
// Lengths: option widths are design units (rule 6); the functions here take and return pixels.

namespace facet::internal
{

// Rows of an inline list placeholder whose options leave visibleRows at 0: the list.inlineRows
// token.
int StubListRows();

// Whether a stub may draw: inside a frame, with a current window that is not the fallback
// (internal::PlannedCanDraw).
bool StubCanDraw();

// The widget width in pixels by the precedence of rule 6 (internal::PlannedWidth): `widthUnits`
// from the options (> 0 units, kFill the available width, < 0 the available width minus that
// many units), then ImGui::SetNextItemWidth, then ImGui::PushItemWidth, then the family default:
// the available width when `fill`, otherwise `contentWidth` (pixels). At least 1.
float StubWidth(float widthUnits, bool fill, float contentWidth = 0.0f);

// Width in pixels of a content-sized placeholder for `widget` and `label`: its text plus the
// inner padding on both sides.
float StubContentWidth(const char* widget, const char* label);

// One placeholder item `width` x `height` pixels with the id of `label` (null: no id, as for
// a separator).
void StubItem(const char* widget, const char* label, float width, float height);

// A layer's placeholder line (available width, regular control height) when `visible`.
void StubLayer(const char* widget, const char* label, bool visible);

// Opens a framed in-flow container. `scope` is the Begin's name as a string literal
// ("BeginCard"), pushed with PushScope; `pushId` pushes `label` (null: `widget`) on the id
// stack until the End. The header item has the id of `label` (null: no id).
// Draws nothing outside a frame, but the scope is pushed all the same, so the End balances.
void BeginStubFrame(const char* scope, const char* widget, const char* label, bool pushId);

// Closes the frame opened with the same `scope`. Returns false, and does nothing else, when no
// such scope is open (the usage error is reported). Frames left open inside it are closed too.
bool EndStubFrame(const char* scope);

// facet::EndFrame: forgets frames left open (the scope stack reports them).
void ClearStubFrames();

} // namespace facet::internal
