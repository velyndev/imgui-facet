# Third-party notices

Facet uses the following third-party software. The fetched libraries are downloaded by CMake at
configure time (see `cmake/FacetDeps.cmake`); stb is vendored in `third_party/stb`.

## Dear ImGui

- Version: v1.93.0, https://github.com/ocornut/imgui
- Use: immediate mode UI library that Facet builds on; the demo also uses its Win32 and DX11 backends.
- License: MIT. Copyright (c) 2014-2026 Omar Cornut.

## FreeType

- Version: 2.15.0 (tag VER-2-15-0), https://github.com/freetype/freetype
- Use: font rasterization through Dear ImGui's FreeType loader (when `FACET_USE_FREETYPE` is on).
- License: used under the FreeType License (FTL). FreeType also includes code under the zlib
  license (gzip module) and other licenses compatible with the FTL, listed in its `LICENSE.TXT`.

Portions of this software are copyright © 2025 The FreeType Project (https://freetype.org).
All rights reserved.

## lunasvg and plutovg

- Version: lunasvg v3.5.0 with the plutovg copy it bundles, https://github.com/sammycage/lunasvg
- Use: SVG parsing and rasterization.
- License: MIT. Copyright (c) 2020-2025 Samuel Ugochukwu.

## stb_image and stb_image_write

- Version: stb_image 2.32 and stb_image_write 1.18 from commit
  9600522de53973336fa033a289858a4354cbb04c, https://github.com/nothings/stb
- Use: image decoding in the library; PNG encoding in the demo (capture output and the generated sample image of the Images page).
- License: MIT or public domain, at your choice. See `third_party/stb/LICENSE`.
  Copyright (c) 2017 Sean Barrett.

## doctest (tests only)

- Version: v2.5.3, https://github.com/doctest/doctest
- Use: unit test framework for `facet_tests`. Not linked into the library or the demo.
- License: MIT. Copyright (c) 2016-2023 Viktor Kirilov.
