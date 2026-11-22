// Tokens page: every color token with its live contrast, the accent presets, spacing, radii,
// control heights and elevation levels, read from the current theme.

#include <algorithm>
#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Tile widths in multiples of the largest spacing token.
constexpr float kColorTileInXxxl = 5.5f;
constexpr float kSwatchInXxxl = 1.0f;
constexpr float kFullTurn = 6.28318530718f;

// What a color tile reports as its contrast.
enum class Measure
{
    Surface,   // label and labelSecondary drawn on the color (over bg)
    Text,      // the color as text on bg
    OnAccent,  // onAccent drawn on the color
    Edge,      // an outline or divider on bg: quiet, 3:1 in the increased contrast variant
    Knob,      // the face of knobs
    None,      // decoration only
};

struct ColorToken
{
    const char* name;
    const ImVec4 facet::Colors::*member;
    Measure measure;
};

constexpr ColorToken kSurfaces[] = {
    {"bg", &facet::Colors::bg, Measure::Surface},
    {"bgSecondary", &facet::Colors::bgSecondary, Measure::Surface},
    {"bgTertiary", &facet::Colors::bgTertiary, Measure::Surface},
    {"bgElevated", &facet::Colors::bgElevated, Measure::Surface},
};

constexpr ColorToken kLabels[] = {
    {"label", &facet::Colors::label, Measure::Text},
    {"labelSecondary", &facet::Colors::labelSecondary, Measure::Text},
    {"labelTertiary", &facet::Colors::labelTertiary, Measure::Text},
    {"labelQuaternary", &facet::Colors::labelQuaternary, Measure::Text},
};

// The border tokens of the border rule and the knob face.
constexpr ColorToken kBorders[] = {
    {"separator", &facet::Colors::separator, Measure::Edge},
    {"separatorOpaque", &facet::Colors::separatorOpaque, Measure::Edge},
    {"controlBorder", &facet::Colors::controlBorder, Measure::Edge},
    {"controlBorderHover", &facet::Colors::controlBorderHover, Measure::Edge},
    {"knob", &facet::Colors::knob, Measure::Knob},
};

constexpr ColorToken kAccents[] = {
    {"accent", &facet::Colors::accent, Measure::OnAccent},
    {"accentHover", &facet::Colors::accentHover, Measure::OnAccent},
    {"accentPressed", &facet::Colors::accentPressed, Measure::OnAccent},
    {"accentSubtle", &facet::Colors::accentSubtle, Measure::Surface},
    {"accentText", &facet::Colors::accentText, Measure::Text},
    {"focusRing", &facet::Colors::focusRing, Measure::Text},
};

constexpr ColorToken kStatus[] = {
    {"destructive", &facet::Colors::destructive, Measure::Text},
    {"success", &facet::Colors::success, Measure::Text},
    {"warning", &facet::Colors::warning, Measure::Text},
    {"shadow", &facet::Colors::shadow, Measure::None},
    {"glassTint", &facet::Colors::glassTint, Measure::None},
    {"scrim", &facet::Colors::scrim, Measure::None},
    {"scrollThumb", &facet::Colors::scrollThumb, Measure::Text},
    {"scrollThumbHover", &facet::Colors::scrollThumbHover, Measure::Text},
};

struct TokensState
{
    int lastPreset = -1;  // index of the accent preset applied last, -1 for the theme default
    float scrollY = 0.0f; // for the selftest: how far the page is scrolled
};

TokensState tokens;

ImU32 U32(const ImVec4& color)
{
    return facet::ToU32(color);
}

// A color chip; translucent colors are drawn over a checkerboard so their alpha shows.
void DrawChip(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, const ImVec4& color)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const float radius = facet::Px(theme.radii.element);
    if (color.w < 1.0f)
        facet::FillCheckerboard(drawList, pMin, pMax, U32(c.bg), U32(facet::Over(c.fill[1], c.bg)),
                                facet::Px(theme.spacing.xs * 2.0f), radius);
    facet::FillRect(drawList, pMin, pMax, U32(color), radius);
    facet::StrokeRect(drawList, pMin, pMax, U32(c.separator), radius, facet::HairlineThickness());
}

void FormatColor(const ImVec4& color, char* out, size_t size)
{
    const ImU32 packed = facet::ToU32(color);
    const unsigned r = (packed >> IM_COL32_R_SHIFT) & 0xFFu;
    const unsigned g = (packed >> IM_COL32_G_SHIFT) & 0xFFu;
    const unsigned b = (packed >> IM_COL32_B_SHIFT) & 0xFFu;
    if (color.w < 1.0f)
        std::snprintf(out, size, "#%02X%02X%02X at %.0f%%", r, g, b, static_cast<double>(color.w * 100.0f));
    else
        std::snprintf(out, size, "#%02X%02X%02X", r, g, b);
}

void FormatMeasure(const ColorToken& token, const ImVec4& color, char* out, size_t size)
{
    const facet::Colors& c = facet::T().colors;
    switch (token.measure)
    {
    case Measure::Surface:
    {
        const ImVec4 surface = facet::Over(color, c.bg);
        std::snprintf(out, size, "label %.1f:1, secondary %.1f:1",
                      static_cast<double>(facet::ContrastRatio(c.label, surface)),
                      static_cast<double>(facet::ContrastRatio(c.labelSecondary, surface)));
        return;
    }
    case Measure::Text:
        std::snprintf(out, size, "on bg %.1f:1", static_cast<double>(facet::ContrastRatio(color, c.bg)));
        return;
    case Measure::OnAccent:
        std::snprintf(out, size, "onAccent %.1f:1", static_cast<double>(facet::ContrastRatio(c.onAccent, color)));
        return;
    case Measure::Edge:
        std::snprintf(out, size, facet::IncreasedContrast() ? "on bg %.1f:1, increased contrast" : "on bg %.1f:1, quiet",
                      static_cast<double>(facet::ContrastRatio(facet::Over(color, c.bg), c.bg)));
        return;
    case Measure::Knob:
        std::snprintf(out, size, "knob face, with the controlBorder hairline");
        return;
    case Measure::None:
        std::snprintf(out, size, "decoration, not text");
        return;
    }
}

// One tile per token: chip, name, hex value and the live contrast figure.
void DrawColorGroup(const char* title, const ColorToken* list, size_t count)
{
    const facet::Theme& theme = facet::T();
    const ImGuiStyle& style = ImGui::GetStyle();
    Note("%s", title);
    const float tileWidth = facet::Px(theme.spacing.xxxl * kColorTileInXxxl);
    const float chip = facet::Px(theme.spacing.xxxl * kSwatchInXxxl);
    const float gap = facet::Px(theme.spacing.md);
    const float rightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    char hex[48];
    char measure[64];
    for (size_t i = 0; i < count; ++i)
    {
        const ColorToken& token = list[i];
        const ImVec4& color = theme.colors.*token.member;
        ImGui::BeginGroup();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        DrawChip(drawList, pos, ImVec2(pos.x + chip, pos.y + chip), color);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + chip + gap, pos.y));
        ImGui::BeginGroup();
        facet::Text(facet::TextStyle::Headline, "%s", token.name);
        FormatColor(color, hex, sizeof(hex));
        Note("%s", hex);
        FormatMeasure(token, color, measure, sizeof(measure));
        Note("%s", measure);
        ImGui::EndGroup();
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(tileWidth, std::max(chip, ImGui::GetItemRectSize().y)));
        ImGui::EndGroup();
        const float nextRight = ImGui::GetItemRectMax().x + style.ItemSpacing.x + tileWidth;
        if (i + 1 < count && nextRight <= rightEdge)
            ImGui::SameLine(0.0f, style.ItemSpacing.x);
    }
    ImGui::Dummy(ImVec2(0.0f, gap));
}

void DrawFills()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    Note("Fills, drawn over bg; label contrast on each");
    const float width = facet::Px(theme.spacing.xxxl * 3.0f);
    const float height = facet::Px(theme.metrics.controlHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float right = facet::demo::ContentRight();
    char text[48];
    for (int i = 0; i < 4; ++i)
    {
        ImGui::Dummy(ImVec2(width, height));
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        facet::FillRect(drawList, a, b, U32(c.fill[i]), facet::Px(theme.radii.control));
        std::snprintf(text, sizeof(text), "fill[%d] %.1f:1", i,
                      static_cast<double>(facet::ContrastRatio(c.label, facet::Over(c.fill[i], c.bg))));
        facet::PushTextStyle(facet::TextStyle::Footnote);
        const ImVec2 size = ImGui::CalcTextSize(text);
        drawList->AddText(ImVec2(a.x + (b.x - a.x - size.x) * 0.5f, a.y + (b.y - a.y - size.y) * 0.5f), U32(c.label),
                          text);
        facet::PopTextStyle();
        if (i < 3)
            facet::demo::SameLineIfFits(right, width);
    }
    ImGui::Dummy(ImVec2(0.0f, facet::Px(theme.spacing.md)));
}

// The accent presets as round swatches; each is a button (Tab, Space) that applies it. The
// first swatch stands for the theme's own accent, which differs between light and dark:
// it clears the custom accent instead of applying the preset color, and it is drawn in the
// accent of the current theme while no custom accent is set.
void DrawAccentPicker(facet::demo::PageCtx& ctx)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const auto presets = facet::AccentPresets();
    const float size = facet::Px(theme.metrics.controlHeight);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float right = facet::demo::ContentRight();
    const bool custom = facet::HasCustomAccent();
    char probe[32];
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        const facet::AccentPreset& preset = presets[static_cast<size_t>(i)];
        const bool themeDefault = i == 0;
        ImGui::PushID(i);
        if (ImGui::InvisibleButton(preset.name, ImVec2(size, size), ImGuiButtonFlags_EnableNav))
        {
            if (themeDefault)
            {
                facet::ClearAccent();
                facet::SetThemeMode(facet::GetThemeMode(), true);
            }
            else
            {
                facet::SetAccent(preset.color);
            }
            tokens.lastPreset = themeDefault ? -1 : i;
        }
        facet::ItemCursor();
        std::snprintf(probe, sizeof(probe), "tokens.accent.%d", i);
        ctx.Probe(probe);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(themeDefault ? "%s (theme default)" : "%s", preset.name);
        const ImVec2 center((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                            (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
        const bool current = themeDefault ? !custom : custom && facet::ToU32(preset.color) == facet::ToU32(c.accent);
        const ImVec4 swatch = themeDefault && !custom ? c.accent : preset.color;
        const float ring = facet::Px(theme.metrics.focusRingWidth);
        drawList->AddCircleFilled(center, size * 0.5f - ring, U32(swatch));
        if (current)
            facet::StrokeArc(drawList, center, size * 0.5f - ring * 0.5f, 0.0f, kFullTurn, U32(c.label),
                             facet::HairlineThickness() * 2.0f, false);
        ImGui::PopID();
        if (i + 1 < static_cast<int>(presets.size()))
            facet::demo::SameLineIfFits(right, size);
    }
    const bool named = custom && tokens.lastPreset > 0;
    Note("Accent: %s%s. Hover, pressed, subtle, focus ring, on-accent text and accent text are derived from it.",
         presets[named ? static_cast<size_t>(tokens.lastPreset) : 0].name, named ? "" : ", the theme default");
}

void DrawSpacing()
{
    const facet::Theme& theme = facet::T();
    const facet::Spacing& s = theme.spacing;
    const struct
    {
        const char* name;
        float value;
    } steps[] = {{"xs", s.xs}, {"sm", s.sm}, {"md", s.md}, {"lg", s.lg}, {"xl", s.xl}, {"xxl", s.xxl}, {"xxxl", s.xxxl}};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float labelWidth = facet::Px(s.xxxl * 2.0f);
    for (const auto& step : steps)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        facet::Text(facet::TextStyle::Body, "%s  %.0f", step.name, static_cast<double>(step.value));
        const float lineHeight = ImGui::GetItemRectSize().y;
        const float barHeight = facet::Px(s.sm);
        const ImVec2 a(pos.x + labelWidth, pos.y + (lineHeight - barHeight) * 0.5f);
        facet::FillRect(drawList, a, ImVec2(a.x + facet::Px(step.value), a.y + barHeight), U32(theme.colors.accent));
    }
}

void DrawRadii()
{
    const facet::Theme& theme = facet::T();
    const facet::Radii& r = theme.radii;
    const struct
    {
        const char* name;
        float value;
    } steps[] = {{"element", r.element}, {"control", r.control}, {"card", r.card}, {"popover", r.popover}, {"window", r.window}};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float size = facet::Px(theme.spacing.xxxl * 2.0f);
    const float right = facet::demo::ContentRight();
    for (size_t i = 0; i < std::size(steps); ++i)
    {
        const auto& step = steps[i];
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(size, size));
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        facet::FillRect(drawList, a, b, U32(theme.colors.bgTertiary), facet::Px(step.value));
        facet::StrokeRect(drawList, a, b, U32(theme.colors.separator), facet::Px(step.value),
                          facet::HairlineThickness());
        facet::Text(facet::TextStyle::Footnote, "%s %.0f", step.name, static_cast<double>(step.value));
        ImGui::EndGroup();
        if (i + 1 < std::size(steps))
            facet::demo::SameLineIfFits(right, size, facet::Px(theme.spacing.lg));
    }
    Note("From %.0f up, corners use the continuous path (%s).", static_cast<double>(r.continuousFrom),
         r.continuousCorners ? "on" : "off");
}

void DrawControlHeights()
{
    const facet::Theme& theme = facet::T();
    const facet::Metrics& m = theme.metrics;
    const struct
    {
        const char* name;
        float value;
    } steps[] = {{"small", m.controlHeightSmall}, {"regular", m.controlHeight}, {"large", m.controlHeightLarge}};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = facet::Px(theme.spacing.xxxl * 3.0f);
    const float right = facet::demo::ContentRight();
    // Swatches sit on one bottom line, so their captions share a baseline.
    float tallest = 0.0f;
    for (const auto& step : steps)
        tallest = std::max(tallest, facet::Px(step.value));
    for (size_t i = 0; i < std::size(steps); ++i)
    {
        const auto& step = steps[i];
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(width, tallest - facet::Px(step.value)));
        ImGui::Dummy(ImVec2(width, facet::Px(step.value)));
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        facet::FillRect(drawList, a, b, U32(theme.colors.fill[1]), facet::Px(theme.radii.control));
        facet::Text(facet::TextStyle::Footnote, "%s %.0f", step.name, static_cast<double>(step.value));
        ImGui::EndGroup();
        if (i + 1 < std::size(steps))
            facet::demo::SameLineIfFits(right, width, facet::Px(theme.spacing.lg));
    }
}

void DrawElevationLevels()
{
    const facet::Theme& theme = facet::T();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = facet::Px(theme.spacing.xxxl * 4.0f);
    const float height = facet::Px(theme.spacing.xxxl * 2.0f);
    const float right = facet::demo::ContentRight();
    for (int level = 1; level <= 3; ++level)
    {
        const facet::ShadowLevel& shadow = theme.elevation.Level(level);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(width, height));
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        const float radius = facet::Px(theme.radii.card);
        facet::DrawElevation(drawList, a, b, radius, level);
        facet::FillRect(drawList, a, b, U32(theme.colors.ElevatedSurface(level)), radius);
        facet::StrokeRect(drawList, a, b, U32(theme.colors.separator), radius, facet::HairlineThickness());
        // The caption wraps at the card width, so every group is exactly one card wide and
        // the cards keep their places when the figures change width between themes.
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        char surface[48];
        FormatColor(theme.colors.ElevatedSurface(level), surface, sizeof(surface));
        Note("level %d: offset %.0f, blur %.0f, opacity %.2f, surface %s", level, static_cast<double>(shadow.offset.y),
             static_cast<double>(shadow.blur), static_cast<double>(shadow.opacity), surface);
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        if (level < 3)
            facet::demo::SameLineIfFits(right, width, facet::Px(theme.spacing.xxl));
    }
}

} // namespace

FACET_DEMO_PAGE("tokens", "Tokens", "Foundations", 0, "default", "accent", "elevation")
{
    // Capture state "accent" shows the second preset; "default" and "elevation" (scrolled to
    // the elevation levels at the end of the page) the theme's own accent.
    if (ctx.capture && ctx.captureState == 1 && !facet::HasCustomAccent())
    {
        facet::SetAccent(facet::AccentPresets()[1].color);
        tokens.lastPreset = 1;
    }
    if (ctx.capture && ctx.captureState != 1)
        tokens.lastPreset = -1;

    facet::demo::PageHeader("Tokens", "Every color, length and level that widgets draw with, read live from the "
                                      "current theme. Contrast figures are WCAG 2 ratios computed now; body text "
                                      "needs 4.5:1.");

    SectionHeader("Accent");
    DrawAccentPicker(ctx);

    SectionHeader("Colors");
    DrawColorGroup("Surfaces", kSurfaces, std::size(kSurfaces));
    DrawFills();
    DrawColorGroup("Labels", kLabels, std::size(kLabels));
    DrawColorGroup("Borders and the knob face", kBorders, std::size(kBorders));
    Paragraph("Every outline is one hairline in a border token: controlBorder for a control or a tile, separator "
              "for a surface and a divider. They stay quiet on purpose; the increased contrast variant resolves "
              "them to colors that reach 3:1 on every surface.");
    bool increased = facet::IncreasedContrastEnabled();
    if (facet::Toggle("Increased contrast", &increased))
        facet::SetIncreasedContrast(increased);
    ctx.Probe("tokens.contrast");
    ImGui::Dummy(ImVec2(0.0f, facet::Px(facet::T().spacing.md)));
    DrawColorGroup("Accent, derived in OKLCH", kAccents, std::size(kAccents));
    DrawColorGroup("Status, effects and scrollbars", kStatus, std::size(kStatus));
    Paragraph("labelTertiary and labelQuaternary stay below 4.5:1 on purpose: they mark disabled text and "
              "decoration only.");

    SectionHeader("Spacing");
    DrawSpacing();

    SectionHeader("Radii");
    DrawRadii();

    SectionHeader("Control heights");
    DrawControlHeights();

    SectionHeader("Elevation");
    DrawElevationLevels();
    Paragraph("Shadows come from the effects module and appear only on floating layers. In the dark theme a "
              "black shadow cannot darken the black background, so each level also gets a lighter surface.");
    if (ctx.captureState == 2)
        ctx.RevealInCapture(1.0f);
    tokens.scrollY = ImGui::GetScrollY();
}

FACET_DEMO_SELFTEST("tokens")
{
    static bool startDark = false;
    test.Do([] { tokens = TokensState{}; });

    // An animated theme switch blends over several frames and ends exactly on the new theme.
    test.Do([] {
        startDark = facet::T().dark;
        facet::SetTheme(startDark ? facet::Theme::Light() : facet::Theme::Dark(), true);
    });
    test.Expect("an animated SetTheme is still blending one frame later",
                [] { return facet::ThemeTransitionActive(); });
    test.Wait(20);
    test.Expect("after the standard duration the new theme is shown exactly", [] {
        const facet::Theme target = startDark ? facet::Theme::Light() : facet::Theme::Dark();
        return !facet::ThemeTransitionActive() && facet::T().dark != startDark &&
               facet::ToU32(facet::T().colors.bg) == facet::ToU32(target.colors.bg);
    });
    test.Do([] { facet::SetTheme(startDark ? facet::Theme::Dark() : facet::Theme::Light(), false); });

    // Accent presets: by mouse, then the next one by keyboard.
    test.Click("tokens.accent.1");
    test.Expect("clicking a preset applies its accent", [] {
        return facet::ToU32(facet::T().colors.accent) == facet::ToU32(facet::AccentPresets()[1].color);
    });
    test.ExpectFocused("tokens.accent.1");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("tokens.accent.1");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("tokens.accent.2");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the next preset applies its accent", [] {
        return facet::ToU32(facet::T().colors.accent) == facet::ToU32(facet::AccentPresets()[2].color);
    });
    test.Expect("the derived on-accent text stays readable",
                [] { return facet::ContrastRatio(facet::T().colors.onAccent, facet::T().colors.accent) >= 4.5f; });
    test.Click("tokens.accent.0");
    test.Wait(30);
    test.Expect("the first swatch returns to the theme default", [] {
        const facet::Theme preset = facet::T().dark ? facet::Theme::Dark() : facet::Theme::Light();
        return !facet::HasCustomAccent() && facet::ToU32(facet::T().colors.accent) == facet::ToU32(preset.colors.accent);
    });

    // The increased contrast switch: the border tokens reach 3:1 on bg while it is on and return
    // to the quiet standard colors when it is off again.
    test.Do([] { facet::SetIncreasedContrast(false); });
    test.Click("tokens.contrast");
    test.Expect("the switch turns the increased contrast variant on", [] { return facet::IncreasedContrast(); });
    test.Expect("in the variant controlBorder and separator reach 3:1 on bg", [] {
        const facet::Colors& c = facet::T().colors;
        return facet::ContrastRatio(facet::Over(c.controlBorder, c.bg), c.bg) >= 3.0f &&
               facet::ContrastRatio(facet::Over(c.separator, c.bg), c.bg) >= 3.0f;
    });
    test.Click("tokens.contrast");
    test.Expect("off again, controlBorder is the quiet preset color", [] {
        const facet::Theme preset = facet::T().dark ? facet::Theme::Dark() : facet::Theme::Light();
        return !facet::IncreasedContrast() &&
               facet::ToU32(facet::T().colors.controlBorder) == facet::ToU32(preset.colors.controlBorder);
    });

    // The window buttons and the theme control of the title bar are covered by the app script
    // app:title-bar (demo/app_selftests.cpp).
    // Navigation: a click opens a page (its controls can be clicked), the arrow keys and
    // Space open the one above.
    test.Click("nav.type");
    test.ExpectFocused("nav.type");
    test.Click("type.clear");
    test.Click("nav.type");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectFocused("nav.tokens");
    test.KeyPress(ImGuiKey_Space);
    test.Click("tokens.accent.1");
    test.Expect("the Tokens page is shown again", [] {
        return facet::ToU32(facet::T().colors.accent) == facet::ToU32(facet::AccentPresets()[1].color);
    });
    test.Click("tokens.accent.0");
    test.Wait(30);
    test.Expect("after the page is opened again the first swatch gives the theme's own accent", [] {
        const facet::Theme preset = facet::T().dark ? facet::Theme::Dark() : facet::Theme::Light();
        return !facet::HasCustomAccent() && facet::ToU32(facet::T().colors.accent) == facet::ToU32(preset.colors.accent);
    });

    // The mouse wheel scrolls the page under the mouse.
    using facet::demo::Target;
    test.Wheel(Target::Item("tokens.accent.1"), -3.0f);
    test.Expect("three notches down scroll the page", [] { return tokens.scrollY > 0.0f; });

    // The page list scrolls. A page far down the list is scrolled into view when it is
    // selected (as by --page), and the keyboard reaches every page: from the first row,
    // holding Down walks to the last one, and Enter opens it.
    test.SelectPage("composites-editors");
    test.ExpectVisible("nav.composites-editors");
    test.ExpectNotVisible("nav.tokens");  // the top of the list is scrolled out of view
    test.Click("nav.tokens");
    test.ExpectPage("tokens");
    test.HoldKey(ImGuiKey_DownArrow, 3.0f);
    test.ExpectFocused("nav.composites-editors");
    test.ExpectVisible("nav.composites-editors");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectPage("composites-editors");
    // Back up: Up walks to the first page, the showcase, which is scrolled into view under its
    // heading.
    test.HoldKey(ImGuiKey_UpArrow, 3.0f);
    test.ExpectFocused("nav.showcase");
    test.ExpectVisible("nav.showcase");
}
