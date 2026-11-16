#pragma once

// Icons: single-color glyphs from SVG sources, tinted at draw time.
//
// Identifier decision: an Icon is a lightweight handle holding the 32-bit FNV-1a hash of
// the icon's name, not a generated enum. Reasons:
//   - Built-in icons (assets/icons/<name>.svg, named by the file name without extension)
//     and icons an app registers at runtime use the same type and the same lookup.
//   - Adding an icon file regenerates only the embedded asset table; no header changes,
//     so no other module or app recompiles because an icon was added.
//   - The hash is constexpr, so Icon("chevron") costs nothing at run time and can be a
//     compile-time constant in widget code.
// The cost is that a misspelled name is found at run time, not at compile time: drawing
// an unknown icon shows a visible placeholder, and RegisterIcon refuses a name whose hash
// collides with a different registered name.

#include <cstdint>
#include <string_view>

#include <imgui.h>

namespace facet
{

struct Icon
{
    std::uint32_t id = 0;  // 0 is the invalid icon

    constexpr Icon() = default;
    constexpr explicit Icon(std::string_view name) : id(Hash(name)) {}

    constexpr bool IsValid() const { return id != 0; }
    friend constexpr bool operator==(Icon, Icon) = default;

    // FNV-1a over the bytes of the name. Never 0 for a non-empty name.
    static constexpr std::uint32_t Hash(std::string_view name)
    {
        if (name.empty())
            return 0;
        std::uint32_t hash = 2166136261u;
        for (const char c : name)
        {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 16777619u;
        }
        return hash != 0 ? hash : 1u;
    }
};

// The icon as an ImGui item, aligned like text. `size` in pixels, 0 for the current font
// size; `color` 0 for the current text color.
void IconGlyph(Icon icon, float size = 0.0f, ImU32 color = 0);

// Draws the icon into a draw list with its top-left corner at `pos`.
void DrawIcon(ImDrawList* drawList, Icon icon, ImVec2 pos, float size, ImU32 color);

// Adds or replaces an icon from an SVG document (copied). Its shapes are used as a mask:
// the icon is drawn in the tint color. Returns the icon, or an invalid icon when the source
// cannot be parsed, the name is empty, or the name's hash equals that of another name.
Icon RegisterIcon(const char* name, const char* svgSource);

// The icon of a registered name (built-in or registered at runtime), or an invalid icon.
Icon FindIcon(const char* name);

} // namespace facet
