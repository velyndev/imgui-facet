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

Every widget below is implemented; each entry ends with its status. Images are drawn on the pixel
grid: a box lands on whole pixels, and an image shown at the size of its texture is sampled one
texel per pixel, so it stays sharp at scale 1. Rounded corners are geometry with the texture mapped
over them, so nothing of a picture shows past a corner.

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
color, and `selected` the accent outline at the one hairline width on the edge, with a hairline of
gap to the picture. With `activatable` it is a Tab stop with
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
`spacing.xxxl` by default) in Cover fit, the chosen one with the selected tile look (`accentSubtle`
and the accent outline at the one hairline width, the picture a hairline inside it) and its caption
in label. One item and one Tab stop: a click that does not move chooses, Left and Right move the
selection, Home and End jump; after keyboard input the focus ring sits inside the chosen thumbnail,
and a click never shows it. A drag
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
sides on the elevated surface, page dots under it (the current one in the accent mark `focusRing`,
the others in `controlBorder`; the page text from `Strings::pageOf` when the
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

A viewport of the available width and height. Zoom 0 fits the image; otherwise `zoom` is design
units per image pixel within the limits: never below fit or 100 percent, whichever is smaller,
and never above `maxZoom`. The wheel zooms around the pointer, a sideways or Shift+wheel pans, a
drag pans and a released pan glides on and slows over `drag.inertiaTime` (not with reduced motion),
and a double click toggles between fit and 100 percent around the pointer. With keyboard focus, +
and - zoom around the middle, the arrows pan, 0 fits and 1 shows 100 percent. The pan is clamped so
the image always covers the viewport where it is larger, and stays centered where it is smaller.
`checkerboard` draws `colors.checker` behind the image; `minimap` shows an overview with the
visible region while the image is larger than the viewport. Loading and failed images show the
skeleton and the failed marker. Zoomed in, the texture is sampled by the host's sampler (usually
linear), so single pixels are not drawn as hard squares. Status: ready.

### ImageCompare

```cpp
bool ImageCompare(const char* label, float* split, ImageHandle before, ImageHandle after, const ImageCompareOptions& options = {});
```

Two images in one box (from the first image's aspect ratio), the first before the divider and the
second after it, each with its own loading and failed look. The divider is a line of the surface
color between `controlBorder` hairlines, so it shows over any picture, with a slider knob in its middle,
and is one handle of the multi-handle block: drag it, press anywhere to move it there, or focus it
and use the arrows (Shift fine, Ctrl coarse), Page Up and Down, Home and End; Escape during a drag
restores it. Vertical moves the divider up and down with the first image on top. `beforeLabel` and
`afterLabel` are captions on elevated chips in the corners. Status: ready.

### TimelineScrubber

```cpp
bool TimelineScrubber(const char* label, double* playhead, double duration, const TimelineScrubberOptions& options = {});
bool TimelineScrubber(const char* label, double* playhead, double* loopIn, double* loopOut, double duration, const TimelineScrubberOptions& options = {});
```

A ruler with tick labels (minutes and seconds by default, hours from an hour on, tenths or
hundredths when zoomed in that far; `format` or `formatter` replace them; the ticks in the quiet
`controlBorder`) over a track that shows the whole duration in `fill[0]`, the `buffered` part in a
lighter fill and the part before the playhead in the accent mark (`focusRing`, 3:1 against the
track). The loop markers are narrow solid knobs in the `knob` token with the level 1 shadow and the
`controlBorder` hairline (`controlBorderHover` under the pointer); the playhead knob is the slider
knob. A press anywhere moves the playhead there and drags it, and gives it the keyboard focus. The
playhead and, in the second form, the loop markers are Tab stops: the arrows step by `step` (a
hundredth of the duration by default; Shift a tenth, Ctrl ten steps), Home and End go to the ends a
handle may reach; the loop markers never cross and the region between them is tinted. The wheel
zooms around the pointer and Shift+wheel pans; the view is kept under the id and resets when the
duration changes. Read-only and a duration of 0 or less show everything and write nothing. A
focused or hovered handle names itself and its time in the tooltip. Status: ready.

### DropZone

```cpp
DropZoneAction DropZone(const char* label, std::span<const char* const> acceptTypes, const DropZoneOptions& options = {});
const ImGuiPayload* DropZonePayload();
```

A dashed area (`media.dashLength` and `media.dashGap` at the hairline width in the quiet
`controlBorder`) with the icon over the
hint. While a payload of an accepted type is over it, it takes the drop target look of the drag and
drop core; a payload of another type gets no highlight and the not-allowed cursor. A drop returns
`Dropped` and `DropZonePayload` gives the payload until the end of the frame. With `activatable` it
is a Tab stop activated by a click, Enter or Space. A disabled zone refuses every payload. Without a
hint the visible label part is the text in the zone, shown in full on hover and focus when it is cut;
with a hint it is the tooltip.
Status: ready.

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
