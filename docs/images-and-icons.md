# Images and icons

Headers: `facet/core/image.h` and `facet/core/icons.h`. Everything here runs on the thread
that builds the Dear ImGui frame, between `facet::Init` and `facet::Shutdown`, except the icon
registry, which works at any time.

## Image handles

Every load returns an `ImageHandle`: a slot index plus a generation. A default handle is
invalid. `ReleaseImage` frees the slot; the handle and every copy of it then report
`ImageStatus::Invalid`, even after the slot is reused for another image, because the new image
gets a new generation. Handles from before `Shutdown` stay invalid after the next `Init`.

| Function | What it does |
|---|---|
| `LoadImageFile(path)` | Reads and decodes a PNG, JPEG, BMP, TGA or GIF file (UTF-8 path) and uploads it. |
| `LoadImageMemory(data, size)` | The same for an encoded file in memory. |
| `LoadImageFileAsync(path)` | Returns a handle at once; a worker thread reads and decodes. |
| `LoadImageMemoryAsync(data, size)` | The same for memory; the bytes are copied first. |
| `CreateImageRGBA(pixels, w, h, stride)` | Uploads 8-bit RGBA with straight alpha. |
| `LoadSvgFile(path, size)`, `LoadSvgMemory(svg, length, size)` | Rasterizes an SVG at `size` design units times `Scale()`. |
| `GetImageStatus`, `IsImageReady`, `ImageFailed` | `Invalid`, `Loading`, `Ready`, `Failed` or `Lost`. |
| `ImageSize`, `ImageTexture` | Pixel size and the `ImTextureRef` of a ready image. |
| `WaitForImage(handle, timeout)` | Blocks until a pending decode finished and uploads it. |
| `ReleaseImage(handle)` | Frees the slot; the texture is released in the next `NewFrame`. |

Synchronous loads return an invalid handle when the source cannot be read or decoded.
Asynchronous loads always return a valid handle; a failure shows as `ImageStatus::Failed`.

### Asynchronous decoding

Two worker threads (one on single-core machines) start with the first asynchronous request.
They read and decode; uploads happen on the frame thread in `facet::NewFrame` (or in
`WaitForImage`), because the Direct3D upload uses the immediate context. Releasing a handle
whose request is still queued removes the request, so it is never decoded; a decode already
running finishes and its result is dropped. `Shutdown` drops queued requests, waits for
the decodes in progress and discards them.

### Deferred release

A texture can still be referenced by the draw data of the frame in which it was released.
`ReleaseImage`, and the replacement of an SVG raster after a scale change, therefore keep the
old texture until the next `facet::NewFrame`, after that draw data was rendered.

### Device loss

`InvalidateDeviceObjects` releases every texture. `CreateDeviceObjects` then

- rasterizes SVG images again from the kept source,
- reloads images that came from a file, asynchronously (they show the loading state briefly),
- marks images created from memory or from pixels as `ImageStatus::Lost`. Their pixels are
  not kept on the CPU; the app creates them again.

### SVG and scale

The SVG source is parsed once and kept. The image is rasterized at its design size times
`facet::Scale()`, rounded to whole pixels, and again at the start of the frame after the scale
changes. Colors stay as written in the document (`currentColor` is black). The rasterizer
produces premultiplied pixels; they are converted to straight alpha before upload, since the
Dear ImGui renderer blends straight alpha.

## Drawing images

`Image(handle, size, options)` is a Dear ImGui item: it takes layout space, is clipped and
scrolls like any other item. `DrawImage(drawList, handle, pMin, pMax, options)` draws into a
draw list without an item. Both have overloads for an app-owned `ID3D11ShaderResourceView*`;
Facet reads the texture size from the view and keeps no reference to it.

`ImageOptions`:

- `fit`: `Contain` (whole image, letterboxed inside the box), `Cover` (box filled, the texture
  cropped around its center) or `Fill` (stretched).
- `radius`: corner radius in pixels, usually `Px(T().radii.card)` or another radius token.
  Corners follow `FillRect`, so they match cards of the same radius.
- `tint`: multiplies the image colors; the style alpha is applied on top.
- `showStates`: while an image is loading, a pulsing placeholder (still with reduced motion);
  when it failed, was released or lost, the image-off icon and a short label.

A size component of 0 takes the image's own size and keeps its aspect ratio.

## Icons

An `Icon` holds the FNV-1a hash of its name, so `Icon("chevron-down")` is a compile-time
constant. Built-in icons are the files in `assets/icons/`, embedded at build time and named
after the file. `RegisterIcon(name, svg)` adds or replaces one at runtime; `FindIcon(name)`
looks one up; `IconCount`, `IconNameAt` and `IconName` list them.

- `IconGlyph(icon, size, color)` is an item centered on the current text line. Size 0 uses
  the font size, color 0 the label color token.
- `DrawIcon(drawList, icon, pos, size, color)` draws inside other widgets.

Icons are rasterized as a white mask the first time a pixel size is needed and stored in an
atlas of 1024 x 1024 pages; drawing multiplies the mask by the color. Position and size are
rounded to whole pixels. After a scale change the atlas is emptied at the next frame start
and refilled at the new sizes. An unknown name draws a crossed box.

### Drawing a new icon

- 16 x 16 view box, `fill="none"`, `stroke="currentColor"`, `stroke-width="1.5"`, round
  caps and joins. Fill only where a shape needs it (dots, the filled star), with
  `fill="currentColor" stroke="none"`.
- Keep the stroke inside the 1 unit margin: path coordinates between 1.75 and 14.25.
- Check the icon on the Icons page of `facet_demo` at scale 1 and 2.

## Internal helpers

`src/image/image_internal.h` (library code only):

- `FitImage(boxMin, boxMax, imageSize, fit)` returns the destination rectangle and UVs.
- `PremultipliedBgraToStraightRgba(...)` converts rasterizer output.
- `RasterizeSvgDocument(...)`, `RasterizeIcon(id, pixels, out)` rasterize without a device.
- `DrawImageLoading(...)`, `DrawImageFailed(...)` draw the two placeholders for widgets that
  manage their own image slots.
- `ReadFileBytes(path, out)` reads a whole file from a UTF-8 path.
