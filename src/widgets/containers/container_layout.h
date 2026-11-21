#pragma once

// Layout helpers shared by the in-flow containers of the family (Card, GroupBox, Disclosure,
// Accordion, the toolbar and status bar strips). Names carry "Container" because every family
// links into one library.
//
// A surface drawn behind content that is submitted after the Begin (a card's fill) needs the
// content's height, which is known only at the End. The backdrop splits the window draw list so
// the content goes into a channel above the one the End fills:
//
//     const int backdrop = BeginContainerBackdrop(window->DrawList);
//     ... content ...
//     ContainerBackdropBehind(backdrop);            // draw the surface now, under the content
//     FillRect(window->DrawList, min, max, fill, radius);
//     EndContainerBackdrop(backdrop);
//
// The inset puts the content inside a padded frame of a given width and narrows the window's
// work rect, so items that fill the available width stop at the inner edge:
//
//     ContainerInset inset;
//     BeginContainerInset(&inset, width, padding);    // a group at the cursor
//     ImGui::Dummy(...);                              // the header line, full width
//     OpenContainerInset(inset);                       // the indent and the narrowed work rect
//     ... content ...
//     const ImRect frame = EndContainerInset(inset, bottomPadding, minHeight);
//
// The collapse region lays out content at an animated height, for the sections that open and
// close (Disclosure, Accordion, a collapsible GroupBox):
//
//     ContainerCollapse collapse;
//     if (BeginContainerCollapse(id, open, &collapse))   // false: closed, nothing to submit
//     {
//         ... content ...
//         EndContainerCollapse(collapse);
//     }
//
// While the height animates, the content is clipped at the animated height and items of a closing
// region take no keyboard focus. Once open and settled nothing is clipped, so focus rings show
// whole. With reduced motion the region opens and closes at once (the animation store's rule).
// Lengths are pixels.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/theme.h"
#include "internal/item.h"

namespace facet::internal
{

// ---- Header row ---------------------------------------------------------------------------------

// How the toggle row of a section that opens and closes looks.
enum class ContainerHeaderStyle
{
    Row,   // a full-width row: chevron, icon, label, trailing text at the end; fill[2] on hover
    Link,  // inline accent text followed by the chevron, as wide as its content; underline on hover
};

struct ContainerHeaderDesc
{
    const char* label = nullptr;     // the visible part [label, labelEnd)
    const char* labelEnd = nullptr;
    Icon icon;                       // after the chevron (Row only)
    const char* trailing = nullptr;  // labelSecondary text: at the row end, or after the label with `trailingAfterLabel`
    bool trailingAfterLabel = false;
    TextStyle style = TextStyle::Body;
    ContainerHeaderStyle look = ContainerHeaderStyle::Row;
    bool open = false;               // chevron-down instead of chevron-right (chevron-up for Link)
    float width = 0.0f;              // pixels; Row only (Link takes its content width)
    float height = 0.0f;             // pixels
};

// The x offset from the header's left edge to its label (Row): where the content of the section
// starts, so it lines up with the title.
float ContainerHeaderLabelOffset(TextStyle style);

// Lays out and runs the header as one item at the cursor (ItemBehavior with `options`), draws it,
// its focus ring and the tooltip of a cut label. Enter, Space and a click press it.
ItemState ContainerHeader(ImGuiID id, const ContainerHeaderDesc& desc, const ItemOptions& options = {});

// ---- Backdrop ---------------------------------------------------------------------------------

// Nesting depth of backdrops (cards in cards); deeper ones draw their surface over the content.
inline constexpr int kContainerBackdropDepth = 16;

// Splits `drawList` so what is drawn next goes above the backdrop. Returns the slot to pass to
// the other calls, or -1 when the draw list is null or the depth is used up.
int BeginContainerBackdrop(ImDrawList* drawList);
// Draws into the channel under the content from here until EndContainerBackdrop.
void ContainerBackdropBehind(int slot);
// Merges the channels back. Slots opened after `slot` and not ended are merged first.
void EndContainerBackdrop(int slot);
// facet::EndFrame: merges every backdrop a missing End left open.
void ClearContainerBackdrops();

// ---- Inset ------------------------------------------------------------------------------------

struct ContainerInset
{
    ImGuiWindow* window = nullptr;
    ImVec2 start = {0.0f, 0.0f};  // top-left of the frame
    float right = 0.0f;           // right edge of the frame
    float padding = 0.0f;         // left and right padding of the content
    float savedWorkMaxX = 0.0f;
    float savedContentMaxX = 0.0f;
    bool opened = false;          // OpenContainerInset ran
};

// Starts a group `width` pixels wide at the cursor of the current window.
void BeginContainerInset(ContainerInset* inset, float width, float padding);
// Indents the content by the padding and narrows the work rect to the inner right edge.
void OpenContainerInset(ContainerInset& inset);
// Undoes the indent and the work rect, adds `bottomPadding` below the content (the item spacing
// after the last item counts toward it), makes the frame at least `minHeight` tall and ends the
// group. Returns the frame rectangle, which is also the last item.
ImRect EndContainerInset(const ContainerInset& inset, float bottomPadding, float minHeight = 0.0f);
// facet::EndFrame and usage errors: puts the work rect back without touching the layout.
void RestoreContainerInset(const ContainerInset& inset);

// ---- Collapse region --------------------------------------------------------------------------

struct ContainerCollapse
{
    ImGuiWindow* window = nullptr;
    ImGuiID id = 0;
    float top = 0.0f;          // y where the content starts
    float lineX = 0.0f;        // x of a new line where the region starts
    float amount = 1.0f;       // 0 closed to 1 open
    ImVec2 savedMaxPos = {0.0f, 0.0f};
    bool clipped = false;      // a clip rect was pushed
    bool noFocus = false;      // the item flags were pushed (closing)
};

// Starts the content of the region `id` (hashed by the caller) that is `open` or closed. Returns
// false when it is closed and its height has reached 0: submit nothing and do not call the End.
bool BeginContainerCollapse(ImGuiID id, bool open, ContainerCollapse* collapse);
// Lays the region out at its animated height and measures the content for the next frame.
void EndContainerCollapse(const ContainerCollapse& collapse);
// The open amount of the region `id` this frame (0 to 1), without starting it.
float ContainerCollapseAmount(ImGuiID id, bool open);

// ---- Open state ---------------------------------------------------------------------------------

// The open state of a section kept under its id (GroupBox, Disclosure without a caller flag,
// accordion sections), saved in the Dear ImGui ini file. `defaultOpen` applies the first time;
// ImGui::SetNextItemOpen captured in `next` applies as for a stock tree node (ImGuiCond_Always
// every time, the other conditions only while nothing is known yet). Writes mark the ini dirty.
bool ContainerOpenState(ImGuiID id, bool defaultOpen, const NextItemData& next);
void SetContainerOpenState(ImGuiID id, bool open);

// ---- Title bar regions --------------------------------------------------------------------------

// Where a title bar's parts were in a frame, for the app frame's hit test: its free area is the
// caption, the caller's items and the window buttons are not.
struct TitleBarRegions
{
    ImGuiID rootWindow = 0;     // the root window it was submitted to
    ImRect bar;                 // the whole bar, pixels
    ImRect items;               // the caller's items; empty when none
    ImRect buttons[3];          // minimize, maximize or restore, close; empty without window buttons
};

inline constexpr int kTitleBarRegionCount = 8;

// Records the regions of a title bar submitted this frame (the first kTitleBarRegionCount count).
void RecordTitleBarRegions(const TitleBarRegions& regions);
// The regions recorded in the last finished frame.
int PreviousTitleBarRegions(const TitleBarRegions** regions);
// facet::EndFrame: this frame's regions become the previous frame's.
void RotateTitleBarRegions();
// facet::EndFrame: forgets the window button activated in the frame that ends.
void ClearTitleBarPress();

// ---- Frame end ----------------------------------------------------------------------------------

// facet::EndFrame (containers_frame.cpp): every widget of the family forgets the Begins whose End
// was not called (the scope stack reports them) and puts back what they changed in a window.
void ClearCardStack();
void ClearGroupBoxStack();
void ClearDisclosureStack();
void ClearAccordionStack();
void ClearShellStack();
void ClearTitleBarStack();

} // namespace facet::internal
