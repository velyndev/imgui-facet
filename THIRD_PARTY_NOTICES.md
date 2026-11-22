# Third-party notices

Facet uses the software below. CMake downloads the fetched libraries at configure time, pinned
by release tag (`cmake/FacetDeps.cmake`); stb is vendored in `third_party/stb`. Nothing else is
bundled.

| Dependency | Version | License | Where it ends up |
|---|---|---|---|
| Dear ImGui | v1.93.0 | MIT | The library builds on it; fetched unless the project sets `FACET_IMGUI_TARGET` |
| FreeType | 2.15.0 (tag `VER-2-15-0`) | FreeType License (FTL) | Font rasterization inside the fetched Dear ImGui, when `FACET_USE_FREETYPE` is on |
| lunasvg, with its bundled plutovg | v3.5.0 | MIT | SVG parsing and rasterization in the library |
| stb_image | 2.32 | MIT or public domain | Image decoding in the library |
| stb_image_write | 1.18 | MIT or public domain | PNG encoding in `facet_demo` only |
| doctest | v2.5.3 | MIT | `facet_tests` only; fetched only when `FACET_BUILD_TESTS` is on |

## What a program built with Facet ships

A program that links `facet::facet` contains code of Dear ImGui (unless it brings its own),
lunasvg, plutovg and stb_image, and of FreeType when it uses the fetched Dear ImGui with
`FACET_USE_FREETYPE` on. Its documentation or an about screen carries:

- the copyright and permission notices of Dear ImGui and lunasvg (which cover plutovg too),
  reproduced below;
- the stb notice from `third_party/stb/LICENSE` (the MIT alternative needs the notice; the
  public domain alternative needs none);
- with FreeType, the credit line of the FreeType License:

  Portions of this software are copyright © 2026 The FreeType Project (https://freetype.org).
  All rights reserved.

doctest and stb_image_write are not part of the library.

## Fonts and system components

Facet ships no font. It loads Segoe UI Variable (`SegUIVar.ttf`), or the static Segoe UI files
(`segoeui.ttf`, `seguisb.ttf`, `segoeuib.ttf`) when the variable font is missing or Dear ImGui
does not rasterize with FreeType, from the Windows font folder at run time. These fonts belong
to Windows and are never copied into the build output. An application that wants other fonts
passes its own with `facet::SetFontSet` and is responsible for their licenses.

Direct3D 11, DXGI, the shader compiler `d3dcompiler_47.dll` and the Desktop Window Manager API
are parts of Windows that Facet links against; they are not distributed either.

## Dear ImGui

https://github.com/ocornut/imgui, tag v1.93.0.

```
The MIT License (MIT)

Copyright (c) 2014-2026 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## FreeType

https://github.com/freetype/freetype, tag VER-2-15-0. Used under the FreeType License (FTL),
one of the two licenses FreeType offers. The full text is `docs/FTL.TXT` in the FreeType
sources (after a configure: `build/<preset>/_deps/facet_freetype-src/docs/FTL.TXT`); the credit
line it asks for is quoted above. FreeType also contains code under the zlib license (the gzip
module) and other licenses compatible with the FTL, listed in its `LICENSE.TXT`. Facet's build
turns off FreeType's optional zlib, bzip2, PNG, HarfBuzz and Brotli dependencies, so none of
those libraries is linked.

## lunasvg and plutovg

https://github.com/sammycage/lunasvg, tag v3.5.0, with the plutovg copy in its `plutovg`
folder. Both carry the same license text.

```
MIT License

Copyright (c) 2020-2025 Samuel Ugochukwu <sammycageagle@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## stb_image and stb_image_write

https://github.com/nothings/stb, commit 9600522de53973336fa033a289858a4354cbb04c, vendored
unmodified in `third_party/stb` (see `third_party/stb/README.md`). License: MIT or public
domain, at your choice; the full text, copyright (c) 2017 Sean Barrett, is
`third_party/stb/LICENSE`.

## doctest (tests only)

https://github.com/doctest/doctest, tag v2.5.3. MIT, copyright (c) 2016-2023 Viktor Kirilov.
It is compiled into `facet_tests` only, never into the library or the demo, so programs built
with Facet do not carry it.
