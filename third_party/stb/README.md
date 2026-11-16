# stb (vendored)

Single-file libraries by Sean Barrett, vendored without modification.

| File | Version | Use |
|---|---|---|
| `stb_image.h` | 2.32 | Image decoding |
| `stb_image_write.h` | 1.18 | PNG output of the demo capture mode |

Source: https://github.com/nothings/stb

<!-- Pinned commit: 9600522de53973336fa033a289858a4354cbb04c -->
Pinned commit: `9600522de53973336fa033a289858a4354cbb04c`. Each file was downloaded from
`https://raw.githubusercontent.com/nothings/stb/<commit>/<file>`.

The implementation is compiled exactly once, in `src/third_party/stb_impl.cpp`. Do not edit the
headers in this folder; to update, download both headers and `LICENSE` from a newer commit and
change the hash above.

License: dual MIT or public domain, see `LICENSE`.
