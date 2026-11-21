# Media

Images in design units, avatars and avatar groups, thumbnail strips, carousels, image viewers,
before and after comparisons, timeline scrubbers and drop zones.

Header: `facet/widgets/media.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference. The demo gallery shows every widget on the page
Widgets > Media.

Images are `ImageHandle` values and icons `Icon` values from the core headers
([images and icons](../images-and-icons.md)). The core functions `Image` and `IconGlyph` stay the
low-level items sized in pixels; the widgets here take design units and `Radius` tokens. A handle
that is still loading shows a skeleton and a failed one a marker with its alt text, so loading
needs no separate widget.

Every widget here is **planned** except the core items: each one below is declared, compiles and
can be called; until Phase 2 implements it, a call draws a placeholder of the widget's default
size and returns the neutral result. Each entry ends with its status.

## Widgets

### IconGlyph (core)

```cpp
void IconGlyph(Icon icon, float size = 0.0f, ImU32 color = 0);
```

A tinted icon on the text line, sized in pixels; declared in `core/icons.h`. Status: ready.

### ImageView

```cpp
bool ImageView(const char* label, ImageHandle image, const ImageViewOptions& options = {});
bool ImageView(const char* label, ID3D11ShaderResourceView* texture, const ImageViewOptions& options = {});
bool ImageViewRetryPressed();
```

An image as one item in design units. `fit` places it in its box (Contain letterboxes, Cover
crops around the center, Fill stretches), `corners` rounds it with a radius token, clamped to half
the shorter side, and `tint` multiplies its colors. A width or height left at 0 follows the
image's aspect ratio, and both at 0 take the texture size in pixels (an SVG handle is rasterized for
the current scale, so it keeps its design size); `kFill` and widths relative to the available
width resolve as for every widget. While the handle loads the box shows the skeleton, keeping the
size the image had last or the large control height square while no size was ever known; a
failed, released or invalid handle shows the failed marker with the alt text, also when the handle
becomes invalid while it is shown. A ready image fades in over `motion.durationFast`. With
`activatable` it is a Tab stop with hover and press washes and the focus ring, activated by a click,
Enter or Space; otherwise it is not a Tab stop. `retryLabel` adds a small button at the bottom of a
failed box (in its middle when the box has no room for the marker as well), reported by
`ImageViewRetryPressed`. The alt text is the tooltip on hover and keyboard focus. `showStates` off
draws nothing while the image is not ready. The texture overload reads the size of the view and
shows the failed marker for a null view. Status: ready.

### Avatar

```cpp
bool Avatar(const char* label, ImageHandle image, const AvatarOptions& options = {});
```

A round picture of the control height of `size` (or `diameter` units, made even). The image fills
the circle (Cover); while it loads, a round skeleton; without a ready image, the initials of the
visible label part (the first letter of the first and of the last word) in label on `fill[1]`,
or the user icon when the label has no word. `squircle` gives a continuous-corner square,
`showStatus` a status dot in the tone's glyph color on the lower right with a ring of the surface
color, and `selected` an accent ring inside the edge. With `activatable` it is a Tab stop with
hover and press washes and the focus ring. The name is the tooltip on hover and keyboard focus.
Status: ready.

### AvatarGroup

```cpp
bool AvatarGroup(const char* label, std::span<const AvatarItem> items, const AvatarGroupOptions& options = {});
```

Avatars overlapping by `media.avatarOverlap` of their diameter, each ringed in the surface color
so the edge between them shows, and a pill with `Strings::moreCount` for the members that are not
shown. `maxVisible` caps the avatars; 0 shows as many as fit the available width, giving way to
the chip. The group is one item and one Tab stop, activated by a click, Enter or Space; hovering
or focusing it shows the visible label part as the heading of a tooltip with every name. A group
without members holds its place with an empty outline and lists `Strings::empty`. Descriptions of
the items are not shown yet. Status: ready.

### ThumbnailStrip

```cpp
bool ThumbnailStrip(const char* label, int* selected, std::span<const ImageItem> items, const ThumbnailStripOptions& options = {});
bool ThumbnailStrip(const char* label, int* selected, int itemCount, ImageItemGetter getter, void* user, const ThumbnailStripOptions& options = {});
```

A horizontally scrolling row of square thumbnails (`thumbnailHeight` units, four times
`spacing.xxxl` by default) in Cover fit, the chosen one with an accent ring inside its edge and its
caption in label. One item and one Tab stop: a click that does not move chooses, Left and Right
move the selection, Home and End jump, and the focus ring sits on the chosen thumbnail. A drag
scrolls the row and a released drag glides on for `drag.inertiaTime` before it settles; the wheel
(also a sideways one) scrolls while there is more row than fits. A new selection is scrolled into
view. The row settles on the standard spring, and jumps while motion is reduced. Read-only shows the
selection and the arrows scroll the row instead. `Loading` draws skeleton thumbnails, `Error` the
error message, and no items the empty message; then the strip is not a Tab stop. Without captions,
the tooltip also names the thumbnail under the pointer, or the chosen one on keyboard focus. The
getter overload is asked for the visible entries only. Reordering by drag is planned. Status: ready.

### Carousel

```cpp
bool Carousel(const char* label, int* index, std::span<const ImageItem> slides, const CarouselOptions& options = {});
```

One slide at a time (9/16 of the width by default) with round previous and next buttons over its
sides on the elevated surface, page dots under it (the page text from `Strings::pageOf` when the
dots do not fit) and, with `autoplay`, a pause button at the end of that row. The slides are one
Tab stop: Left and Right page, Home and End jump; the buttons are Tab stops of their own and a dot
takes a click. A drag moves the slide with the pointer (with resistance past the ends) and the
release pages by more than half a slide or by a flick faster than `drag.flickSpeed`; the slide then
settles on the standard spring, at once with reduced motion. A sideways wheel or Shift+wheel
pages one slide per notch; the plain wheel keeps scrolling the window. `loop` wraps at both ends. Autoplay
pages every `autoplay` seconds, wrapping at the end, and waits while the pointer or visible
keyboard focus is on a part of it, while it is paused, and always while motion is reduced (then
the pause button is gone too). `captions` puts the item label on an elevated chip at the bottom.
`Loading` draws a skeleton slide, `Error` and an empty list their messages. Status: ready.

### ImageViewer

```cpp
bool ImageViewer(const char* label, ImageHandle image, ImageViewerState* view, const ImageViewerOptions& options = {});
```

Zoom and pan viewport with fit and 100 percent, optional minimap. Status: planned.

### ImageCompare

```cpp
bool ImageCompare(const char* label, float* split, ImageHandle before, ImageHandle after, const ImageCompareOptions& options = {});
```

Before and after images with a draggable divider. Status: planned.

### TimelineScrubber

```cpp
bool TimelineScrubber(const char* label, double* playhead, double duration, const TimelineScrubberOptions& options = {});
bool TimelineScrubber(const char* label, double* playhead, double* loopIn, double* loopOut, double duration, const TimelineScrubberOptions& options = {});
```

Time ruler with a playhead and optional loop markers. Status: planned.

### DropZone

```cpp
DropZoneAction DropZone(const char* label, std::span<const char* const> acceptTypes, const DropZoneOptions& options = {});
const ImGuiPayload* DropZonePayload();
```

Area that accepts drag and drop payloads of given types and can be activated. Status: planned.

## Usage

### Image with a retry for failed loads

The result reports activation; the retry button has its own query, the same pattern as
`DataStateViewRetryPressed`, so an activatable image in the failed state stays unambiguous.

```cpp
static facet::ImageHandle cover = facet::LoadImageFileAsync(path);
if (facet::ImageView("Cover image", cover, {.corners = facet::Radius::Card, .activatable = true,
                                            .retryLabel = "Try again", .width = 160.0f, .height = 90.0f}))
    OpenLightbox(cover);                   // the application's own function
if (facet::ImageViewRetryPressed())
{
    facet::ReleaseImage(cover);
    cover = facet::LoadImageFileAsync(path);
}
```

### Image viewer with a saved view

`ImageViewerState` is plain data: keep it, save it, or reset it to `{}` to fit the image again.

```cpp
static facet::ImageViewerState view;
facet::ImageViewer("Preview", image, &view, {.minimap = true});
if (facet::Button("Fit"))
    view = {};
```

### Timeline with a loop

```cpp
static double playhead = 0.0, loopIn = 0.0, loopOut = 0.0;
if (facet::TimelineScrubber("Timeline", &playhead, &loopIn, &loopOut, clipSeconds, {.buffered = loadedSeconds}))
    Seek(playhead);                        // the application's player
```

### Drop zone

The zone accepts payloads from any drag source, including `BeginDragSource` in
`composites.h`. Files dragged in from the operating system arrive only when the host turns its
window messages into a payload.

```cpp
static const char* const types[] = {"app.asset"};
switch (facet::DropZone("Assets", types, {.hint = "Drop an asset here", .icon = facet::Icon("upload")}))
{
case facet::DropZoneAction::Dropped:
    AddAsset(*static_cast<const AssetRef*>(facet::DropZonePayload()->Data));
    break;
case facet::DropZoneAction::Activated:
    OpenAssetBrowser();
    break;
default:
    break;
}
```

## Planned, not declared yet

- **AnimatedImage**: sprite sheet animation for previews and animated icons, paused while
  clipped and under reduced motion.
- **ImageCropper**: a crop rectangle with eight handles, a thirds grid and an optional locked
  aspect ratio.
- **ThumbnailStrip reordering**: drag to reorder thumbnails.
- **Operating system file drops**: a host hook that turns dropped files into a payload.
