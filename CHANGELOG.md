# Changelog

What changed in Facet, newest first. Versions follow `FACET_VERSION` in
`include/facet/core/version.h`. Until 1.0 the public API may change between minor versions.

## Unreleased (0.1.0)

The first public state of the library. There is no tagged release yet.

- **Core:** theme tokens with a light and a dark preset, animated theme switches, an accent color
  set at run time, an increased contrast variant, design units with three scale modes, motion
  that follows the Windows animation setting, text styles on Segoe UI loaded from Windows.
- **Effects, images and icons:** soft shadows, backdrop blur for Direct3D 11, image and SVG
  loading with generational handles, an embedded icon set and an icon registry for SVG files.
- **Widgets:** fourteen families, all ready: buttons, selection, lists, sliders, inputs, pickers,
  navigation, containers, layout, overlays, display, charts, media and composites. Each widget is
  documented under `docs/widgets/`.
- **Test app:** `facet_demo` with a page per family, a theme editor that exports its edits as
  C++, an overlay mode, and capture and selftest modes.
- **Build:** CMake presets for Visual Studio 2026 (x64 and x86) and Ninja, a consumer example
  that builds against its own Dear ImGui target, doctest unit tests and a GitHub Actions workflow.

Known limitation: a popup level taller than its room and the multiline text of `TextArea` still
show Dear ImGui's own scrollbar instead of Facet's overlay scrollbar.
