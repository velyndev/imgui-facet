#pragma once

// The edits of the theme editor and their export as C++. Header only and free of Dear ImGui
// frame calls, so facet_tests can include it (tests/test_demo_theme_export.cpp).
//
// An edit is a factor per group over the starting values of the preset (Theme::Light() or
// Theme::Dark()), and for some groups a value per token that replaces the factor for that token.
// ApplyThemeEdits builds the edited theme from a preset; BuildThemeExport writes the C++ that
// sets exactly the edited tokens, comparing them with the same preset.

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <facet/core/theme.h>

namespace facet::demo
{

// The groups of tokens the editor changes together.
enum class ThemeEditGroup
{
    Radii,
    Spacing,
    Type,
    Heights,
    Motion,
    Blur,
    Shadow,
    Lines,
};
inline constexpr int kThemeEditGroupCount = 8;

struct ThemeEditGroupInfo
{
    const char* name;    // shown in the editor
    float minFactor;     // range of the group factor
    float maxFactor;
    // Step of the factor slider: 1 lies on its grid, and every group has at most 20 steps, so every
    // factor slider shows its stops as ticks (a slider draws them for at most 21 stops).
    double step;         // a double like SliderOptions::step, so 0.1 stays exact enough for the grid
    bool tokenRows;      // the editor shows a row per token
};

inline const ThemeEditGroupInfo& ThemeEditGroupOf(ThemeEditGroup group)
{
    static const ThemeEditGroupInfo kGroups[kThemeEditGroupCount] = {
        {"Corner radii", 0.0f, 2.0f, 0.1, true},
        {"Spacing", 0.5f, 2.0f, 0.1, true},
        {"Type sizes", 0.75f, 1.5f, 0.05, true},
        {"Control heights", 0.75f, 1.5f, 0.05, true},
        {"Motion scale", 0.25f, 4.0f, 0.25, false},
        {"Blur strength", 0.0f, 2.0f, 0.1, false},
        {"Shadow strength", 0.0f, 2.0f, 0.1, false},
        {"Lines", 0.5f, 2.0f, 0.1, true},
    };
    return kGroups[static_cast<int>(group)];
}

// One editable length or factor of a Theme. `path` is the member path below Theme, written the
// way the exported code names it; `ref` reaches the same member, so a path that does not exist
// would not compile.
struct ThemeEditToken
{
    const char* path;
    const char* label;   // shown in the editor
    ThemeEditGroup group;
    float& (*ref)(Theme& theme);
    float minValue;      // range of the per-token row
    float maxValue;
};

#define FACET_DEMO_THEME_TOKEN(group, member, label, lo, hi)                                      \
    ThemeEditToken                                                                                 \
    {                                                                                              \
        #member, label, ThemeEditGroup::group, [](Theme& t) -> float& { return t.member; }, lo, hi \
    }

inline constexpr int kThemeEditTokenCount = 31;

inline const std::array<ThemeEditToken, kThemeEditTokenCount>& ThemeEditTokens()
{
    static const std::array<ThemeEditToken, kThemeEditTokenCount> kTokens = {
        FACET_DEMO_THEME_TOKEN(Radii, radii.element, "Element", 0.0f, 24.0f),
        FACET_DEMO_THEME_TOKEN(Radii, radii.control, "Control", 0.0f, 24.0f),
        FACET_DEMO_THEME_TOKEN(Radii, radii.card, "Card", 0.0f, 32.0f),
        FACET_DEMO_THEME_TOKEN(Radii, radii.popover, "Popover", 0.0f, 32.0f),
        FACET_DEMO_THEME_TOKEN(Radii, radii.window, "Window", 0.0f, 32.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.xs, "xs", 0.0f, 16.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.sm, "sm", 0.0f, 24.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.md, "md", 0.0f, 32.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.lg, "lg", 0.0f, 40.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.xl, "xl", 0.0f, 48.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.xxl, "xxl", 0.0f, 56.0f),
        FACET_DEMO_THEME_TOKEN(Spacing, spacing.xxxl, "xxxl", 0.0f, 64.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[0].size, "Large title", 16.0f, 40.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[1].size, "Title 1", 14.0f, 34.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[2].size, "Title 2", 12.0f, 28.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[3].size, "Title 3", 11.0f, 24.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[4].size, "Headline", 10.0f, 20.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[5].size, "Body", 10.0f, 20.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[6].size, "Callout", 9.0f, 18.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[7].size, "Footnote", 9.0f, 18.0f),
        FACET_DEMO_THEME_TOKEN(Type, type.styles[8].size, "Caption", 8.0f, 16.0f),
        FACET_DEMO_THEME_TOKEN(Heights, metrics.controlHeightSmall, "Small", 16.0f, 40.0f),
        FACET_DEMO_THEME_TOKEN(Heights, metrics.controlHeight, "Regular", 20.0f, 48.0f),
        FACET_DEMO_THEME_TOKEN(Heights, metrics.controlHeightLarge, "Large", 24.0f, 56.0f),
        FACET_DEMO_THEME_TOKEN(Motion, motion.scale, "Scale", 0.25f, 4.0f),
        FACET_DEMO_THEME_TOKEN(Blur, glass.blurRadius, "Blur radius", 0.0f, 40.0f),
        FACET_DEMO_THEME_TOKEN(Shadow, elevation.levels[0].opacity, "Level 1", 0.0f, 1.0f),
        FACET_DEMO_THEME_TOKEN(Shadow, elevation.levels[1].opacity, "Level 2", 0.0f, 1.0f),
        FACET_DEMO_THEME_TOKEN(Shadow, elevation.levels[2].opacity, "Level 3", 0.0f, 1.0f),
        FACET_DEMO_THEME_TOKEN(Lines, metrics.hairline, "Border width", 0.5f, 3.0f),
        FACET_DEMO_THEME_TOKEN(Lines, metrics.focusRingWidth, "Focus ring width", 1.0f, 4.0f),
    };
    return kTokens;
}

#undef FACET_DEMO_THEME_TOKEN

// Everything the editor changed. The default value changes nothing.
struct ThemeEdits
{
    std::array<float, kThemeEditGroupCount> factor = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    std::array<bool, kThemeEditTokenCount> overridden = {};
    std::array<float, kThemeEditTokenCount> value = {};  // used where overridden
    bool reduceMotion = false;
    bool themeDefaultAccent = true;  // each preset's own accent
    ImVec4 accent = AccentPresets()[0].color;  // used while themeDefaultAccent is false; the custom well shows it
    // The increased contrast variant (facet::SetIncreasedContrast): a library switch, not a theme token.
    bool increasedContrast = false;

    bool operator==(const ThemeEdits& other) const
    {
        return factor == other.factor && overridden == other.overridden && value == other.value &&
               reduceMotion == other.reduceMotion && themeDefaultAccent == other.themeDefaultAccent &&
               accent.x == other.accent.x && accent.y == other.accent.y && accent.z == other.accent.z &&
               accent.w == other.accent.w && increasedContrast == other.increasedContrast;
    }
};

inline bool ThemeEditValuesDiffer(float a, float b)
{
    return std::fabs(a - b) > 1e-4f;
}

// The value of token `index` in `theme`. The accessor only reads here.
inline float ThemeTokenValue(const Theme& theme, int index)
{
    return ThemeEditTokens()[static_cast<std::size_t>(index)].ref(const_cast<Theme&>(theme));
}

// The value of token `index` after the edits, from the preset `base`, within the range of the
// token's row: a group factor never pushes a token past it (a shadow opacity past 1), so the live
// theme, the row and the export show the same value.
inline float EditedTokenValue(const Theme& base, const ThemeEdits& edits, int index)
{
    const ThemeEditToken& token = ThemeEditTokens()[static_cast<std::size_t>(index)];
    const float value = edits.overridden[static_cast<std::size_t>(index)]
                            ? edits.value[static_cast<std::size_t>(index)]
                            : ThemeTokenValue(base, index) * edits.factor[static_cast<std::size_t>(token.group)];
    return std::fmin(std::fmax(value, token.minValue), token.maxValue);
}

// The preset `base` with the edits applied.
inline Theme ApplyThemeEdits(const Theme& base, const ThemeEdits& edits)
{
    Theme theme = base;
    for (int i = 0; i < kThemeEditTokenCount; ++i)
        ThemeEditTokens()[static_cast<std::size_t>(i)].ref(theme) = EditedTokenValue(base, edits, i);
    theme.motion.reduceMotion = edits.reduceMotion || base.motion.reduceMotion;
    if (!edits.themeDefaultAccent)
        theme.ApplyAccent(edits.accent);
    return theme;
}

// True when token `index` differs from the preset after the edits.
inline bool ThemeTokenEdited(const Theme& base, const ThemeEdits& edits, int index)
{
    return ThemeEditValuesDiffer(EditedTokenValue(base, edits, index), ThemeTokenValue(base, index));
}

// A float as a C++ literal: "16.0f", "9.6f", "0.108f".
inline std::string ThemeExportFloat(float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.4g", static_cast<double>(value));
    std::string text = buffer;
    if (text.find_first_of(".e") == std::string::npos)
        text += ".0";
    return text + "f";
}

// The C++ that applies the edits to a theme, for the clipboard. `presetName` names the preset
// the edits were made on ("Light" or "Dark"); `uiScale` is the user UI scale (not a theme
// token: it is mentioned in a comment when it is not 1). Only tokens that differ from the
// preset get a line.
inline std::string BuildThemeExport(const Theme& base, const ThemeEdits& edits, const char* presetName, float uiScale)
{
    std::string out;
    out += "// Theme edits made in the facet_demo theme editor on facet::Theme::";
    out += presetName;
    out += "().\n// Use: facet::Theme theme = facet::Theme::";
    out += presetName;
    out += "(); ApplyThemeEdits(theme); facet::SetTheme(theme);\n";
    out += "#include <facet/facet.h>\n\ninline void ApplyThemeEdits(facet::Theme& theme)\n{\n";
    int lines = 0;
    for (int i = 0; i < kThemeEditTokenCount; ++i)
    {
        if (!ThemeTokenEdited(base, edits, i))
            continue;
        out += "    theme.";
        out += ThemeEditTokens()[static_cast<std::size_t>(i)].path;
        out += " = " + ThemeExportFloat(EditedTokenValue(base, edits, i)) + ";\n";
        ++lines;
    }
    if (edits.reduceMotion && !base.motion.reduceMotion)
    {
        out += "    theme.motion.reduceMotion = true;\n";
        ++lines;
    }
    if (!edits.themeDefaultAccent)
    {
        out += "    theme.ApplyAccent(ImVec4(" + ThemeExportFloat(edits.accent.x) + ", " + ThemeExportFloat(edits.accent.y) +
               ", " + ThemeExportFloat(edits.accent.z) + ", 1.0f));\n";
        ++lines;
    }
    if (lines == 0)
        out += "    (void)theme;  // nothing was edited\n";
    out += "}\n";
    if (ThemeEditValuesDiffer(uiScale, 1.0f))
        out += "// UI scale (not a theme token): facet::SetUiScale(" + ThemeExportFloat(uiScale) + ");\n";
    if (edits.increasedContrast)
        out += "// Increased contrast (not a theme token): facet::SetIncreasedContrast(true);\n";
    return out;
}

} // namespace facet::demo
