#pragma once

// Media: images in design units, avatars and avatar groups, thumbnail strips, carousels, image
// viewers, before and after comparisons, timeline scrubbers and drop zones.
//
// Images are ImageHandle values from core/image.h and icons are Icon values from core/icons.h.
// The core functions Image and IconGlyph stay the low-level items sized in pixels; the widgets
// here take design units and Radius tokens. A handle that is still loading shows a skeleton,
// a failed one a marker with the alt text, so there is no separate asynchronous image widget.
//
// Every widget that writes a value or a selection has `readOnly` (rule 7): the widget shows
// its value and can be focused, but writes nothing and returns false.
//
// The strips, viewers and rulers here are canvases and panels (rule 6): their visible label part
// is not drawn beside them, so they keep the whole width, but it is their tooltip on hover and
// on keyboard focus. A settings row gives them a visible label.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. Every widget here is declared and planned: its body draws a
// placeholder until Phase 2 implements it (docs/widgets/media.md lists the status of each).
// Every function is for the thread that runs the Dear ImGui frame.

#include <cstdint>
#include <limits>
#include <span>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/widgets/common.h"

struct ID3D11ShaderResourceView;

namespace facet
{

// --- Images ---------------------------------------------------------------------------------

struct ImageViewOptions
{
    ImageFit fit = ImageFit::Contain;
    Radius corners = Radius::None;
    ImU32 tint = IM_COL32_WHITE;           // image data color, not a theme color
    bool showStates = true;                // skeleton while loading, failed marker on error
    bool activatable = false;              // a Tab stop; click, Enter and Space activate it
    const char* retryLabel = nullptr;      // failed state: a button with this text; null for none
    float width = 0.0f;                    // units; 0 from the image, keeping its aspect ratio
    float height = 0.0f;                   // units; 0 from the image, keeping its aspect ratio
};

// An image as one item, sized in design units. The visible label part is the alt text, shown
// in the failed state and as the tooltip, and the accessible name. While the handle is
// loading, the box keeps its size (control height large square while the size is unknown).
// When the image becomes ready it fades in over motion.durationFast, keyed by the id. Returns
// true on the frame the image was activated (activatable only); the retry button is reported
// by ImageViewRetryPressed, never by the result.
bool ImageView(const char* label, ImageHandle image, const ImageViewOptions& options = {});
// An app-owned texture; Facet reads its size and keeps no reference. It never loads or fails.
bool ImageView(const char* label, ID3D11ShaderResourceView* texture, const ImageViewOptions& options = {});
// True on the frame the retry button (retryLabel) of the last ImageView call was activated.
bool ImageViewRetryPressed();

// --- Avatars --------------------------------------------------------------------------------

struct AvatarOptions
{
    bool squircle = false;                 // a continuous-corner square instead of a circle
    bool showStatus = false;               // a status dot on the lower right
    Tone status = Tone::Neutral;           // color of the status dot
    bool selected = false;                 // the accent ring of a chosen avatar
    bool activatable = false;              // a Tab stop; click, Enter and Space activate it
    float diameter = 0.0f;                 // units; 0 the control height of size
    ControlSize size = ControlSize::Inherit;
};

// A round picture of a person, team or device. Without a ready image it shows the initials of
// the visible label part on a neutral fill. The visible label part is the tooltip and the
// accessible name. Returns true on the frame it was activated (activatable only). Keeps no
// state.
bool Avatar(const char* label, ImageHandle image, const AvatarOptions& options = {});

// One avatar of a group, a profile list or a user chip.
struct AvatarItem
{
    const char* label = nullptr;           // name: initials, tooltip and accessible name
    ImageHandle image;                     // invalid: initials
    const char* detail = nullptr;          // second line where the widget has room; null for none
    ImGuiID id = 0;                        // stable id of the entry's state when entries change order; 0 uses the index
};

struct AvatarGroupOptions
{
    int maxVisible = 0;                    // avatars before the "+N" chip; 0 fits the width
    float diameter = 0.0f;                 // units; 0 the control height of size
    ControlSize size = ControlSize::Inherit;
};

// Overlapping avatars with a "+N" chip for the rest; hovering the group lists every name in a
// tooltip under the visible label part as its heading. The group is one Tab stop; keyboard focus
// shows the same tooltip. Returns
// true on the frame the group was activated (click, Enter, Space), for example to open a member
// list.
bool AvatarGroup(const char* label, std::span<const AvatarItem> items, const AvatarGroupOptions& options = {});

// --- Collections of images ------------------------------------------------------------------

// One image of a strip or carousel.
struct ImageItem
{
    const char* label = nullptr;           // alt text and caption
    ImageHandle image;
    ImGuiID id = 0;                        // stable id of the entry's state when entries change order; 0 uses the index
};

// Fills `out` for item `index` and returns true, or false to skip it. Called during the call
// only, for the visible items.
using ImageItemGetter = bool (*)(void* user, int index, ImageItem* out);

struct ThumbnailStripOptions
{
    float thumbnailHeight = 0.0f;          // units; 0 for four times the xxxl spacing
    bool captions = false;                 // the item label under each thumbnail
    DataState state = DataState::Ready;    // Loading shows skeleton thumbnails, Error the error state
    const char* emptyText = nullptr;       // shown for 0 items; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    bool readOnly = false;                 // the row still scrolls; the selection is shown, never written
};

// A horizontally scrolling row of thumbnails with the chosen one ringed. One Tab stop; Left
// and Right move the selection, Home and End jump; the wheel scrolls. *selected -1 means none.
// Returns true when *selected was written. Scroll position kept under the id. The visible label
// part is the strip's tooltip (see the top of this file).
bool ThumbnailStrip(const char* label, int* selected, std::span<const ImageItem> items,
                    const ThumbnailStripOptions& options = {});
bool ThumbnailStrip(const char* label, int* selected, int itemCount, ImageItemGetter getter, void* user,
                    const ThumbnailStripOptions& options = {});

struct CarouselOptions
{
    bool loop = false;                     // the last slide wraps to the first
    bool captions = false;                 // the item label over the bottom of the slide
    float autoplay = 0.0f;                 // seconds per slide; 0 off. Pauses while hovered or
                                           // focused, has a pause button, and is off with reduced motion
    ImageFit fit = ImageFit::Cover;
    DataState state = DataState::Ready;    // Loading shows a skeleton slide, Error the error state
    const char* emptyText = nullptr;       // shown for 0 slides; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for 9/16 of the width
};

// One slide at a time with previous and next buttons and page dots. The slide follows a
// horizontal drag and snaps by velocity with the standard spring. One Tab stop for the slides
// (Left and Right page) plus the buttons. Returns true when *index was written. The visible label
// part is the carousel's tooltip (see the top of this file); slide captions come from the items.
bool Carousel(const char* label, int* index, std::span<const ImageItem> slides, const CarouselOptions& options = {});

// The view of an ImageViewer, owned by the caller so it can be saved or reset.
struct ImageViewerState
{
    float zoom = 0.0f;                     // design units per image pixel (1 is 100 percent); 0 fits the image
    ImVec2 center = {0.0f, 0.0f};          // image pixel at the viewport center
};

struct ImageViewerOptions
{
    bool minimap = false;                  // overview with the visible region while zoomed in
    bool checkerboard = true;              // a checkerboard behind transparent pixels
    float maxZoom = 32.0f;                 // design units per image pixel at the closest zoom
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 fills the available height
};

// A zoom and pan viewport. The wheel zooms around the cursor, a drag pans with inertia, a
// double click toggles between fit and 100 percent; while focused, + and - zoom, arrows pan
// and 0 fits. 100 percent is one image pixel per design unit (zoom 1), so it follows the UI
// scale like every other length. The visible label part is the alt text: the tooltip of the
// viewport (see the top of this file) and the text of its failed state. Returns true when
// *view was written.
bool ImageViewer(const char* label, ImageHandle image, ImageViewerState* view, const ImageViewerOptions& options = {});

struct ImageCompareOptions
{
    Orientation orientation = Orientation::Horizontal;  // Horizontal: the divider moves left and right
    const char* beforeLabel = nullptr;     // corner caption of the first image; null for none
    const char* afterLabel = nullptr;
    ImageFit fit = ImageFit::Contain;
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 from the first image's aspect ratio
    bool readOnly = false;
};

// Two images of the same size on top of each other with a divider at *split (0 to 1). The
// divider is a handle: drag it, or focus it and use arrows (Shift fine, Ctrl coarse), Home and
// End. A click elsewhere moves it there. Returns true when *split was written. The visible
// label part is the tooltip (see the top of this file); the corner captions come from the options.
bool ImageCompare(const char* label, float* split, ImageHandle before, ImageHandle after,
                  const ImageCompareOptions& options = {});

// --- Timeline -------------------------------------------------------------------------------

struct TimelineScrubberOptions
{
    const char* format = nullptr;          // printf for times in seconds; null: minutes and seconds
    FormatFn formatter = nullptr;          // used instead of format when set
    void* formatterUser = nullptr;
    double buffered = std::numeric_limits<double>::quiet_NaN();  // seconds already loaded, drawn as a lighter band; NaN none
    double step = 0.0;                     // seconds per arrow key step; 0 for a hundredth of the duration
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for twice the control height
    bool readOnly = false;                 // the ruler still zooms and pans; the handles never write
};

// A time ruler from 0 to `duration` seconds with a playhead. Dragging the ruler or the playhead
// writes *playhead while dragging; the wheel zooms around the cursor and Shift+wheel pans
// (zoom kept under the id). The playhead is a Tab stop; arrows step, Home and End jump. A
// duration <= 0 draws the ruler read-only. Returns true when a value was written. The visible
// label part is the tooltip (see the top of this file), so the ruler keeps the whole width.
bool TimelineScrubber(const char* label, double* playhead, double duration, const TimelineScrubberOptions& options = {});
// With loop markers: *loopIn and *loopOut are two more handles, each a Tab stop, that never
// cross (loopIn <= loopOut).
bool TimelineScrubber(const char* label, double* playhead, double* loopIn, double* loopOut, double duration,
                      const TimelineScrubberOptions& options = {});

// --- Drop zone ------------------------------------------------------------------------------

// What happened in a DropZone this frame.
enum class DropZoneAction
{
    None,
    Dropped,    // a payload of an accepted type was dropped; read it with DropZonePayload
    Activated,  // the zone was activated (click, Enter, Space), for example to open a file dialog
};

struct DropZoneOptions
{
    const char* hint = nullptr;            // text in the zone; null shows the visible label part
    Icon icon;                             // drawn above the hint; none for no icon
    bool activatable = true;               // the zone is a Tab stop and can be activated
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for four times the control height
};

// A dashed area that accepts Dear ImGui drag and drop payloads whose type is in `acceptTypes`
// (each at most 32 bytes, read during the call). While a payload is dragged over it, the zone
// is tinted in the accent for an accepted type and in the destructive token otherwise. The
// visible label part is the text in the zone unless `hint` replaces it; then it is the zone's
// tooltip. Files dragged in from the operating system reach
// it only when the host submits them as a payload (the host owns the window messages).
DropZoneAction DropZone(const char* label, std::span<const char* const> acceptTypes,
                        const DropZoneOptions& options = {});
// The payload of the last DropZone call that returned Dropped, valid until the end of the
// frame; null otherwise.
const ImGuiPayload* DropZonePayload();

} // namespace facet
