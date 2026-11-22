# stb (vendored)

Single-file libraries by Sean Barrett, vendored without modification.

| File | Version | Use |
|---|---|---|
| `stb_image.h` | 2.32 | Image decoding |
| `stb_image_write.h` | 1.18 | PNG output of the demo capture mode and the generated sample images of the demo's Images and Media pages |

Source: https://github.com/nothings/stb

<!-- Pinned commit: 9600522de53973336fa033a289858a4354cbb04c -->
Pinned commit: `9600522de53973336fa033a289858a4354cbb04c`. Each file was downloaded from
`https://raw.githubusercontent.com/nothings/stb/<commit>/<file>`.

stb_image is implemented in `src/image/image_io.cpp`; stb_image_write, used by the demo only, is
implemented in each demo file that writes PNG data (`demo/capture.cpp`,
`demo/pages/page_images.cpp` and `demo/pages/page_w_media.cpp`). Do not edit the headers in this
folder; to update, download both headers and `LICENSE` from a newer commit and
change the hash above.

License: dual MIT or public domain, see `LICENSE`.
