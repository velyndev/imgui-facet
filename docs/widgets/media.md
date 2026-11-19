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

An image as one item in design units, with loading and failed states, optional activation and retry. Status: planned.

### Avatar

```cpp
bool Avatar(const char* label, ImageHandle image, const AvatarOptions& options = {});
```

Round picture with an initials fallback, status dot and selection ring. Status: planned.

### AvatarGroup

```cpp
bool AvatarGroup(const char* label, std::span<const AvatarItem> items, const AvatarGroupOptions& options = {});
```

Overlapping avatars with a "+N" chip; the names show on hover. Status: planned.

### ThumbnailStrip

```cpp
bool ThumbnailStrip(const char* label, int* selected, std::span<const ImageItem> items, const ThumbnailStripOptions& options = {});
bool ThumbnailStrip(const char* label, int* selected, int itemCount, ImageItemGetter getter, void* user, const ThumbnailStripOptions& options = {});
```

Scrolling row of thumbnails with one selected. Status: planned.

### Carousel

```cpp
bool Carousel(const char* label, int* index, std::span<const ImageItem> slides, const CarouselOptions& options = {});
```

One slide at a time with buttons, page dots, swipe and an optional pausable autoplay. Status: planned.

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
