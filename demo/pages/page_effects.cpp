// Effects page: elevation shadows on cards, a glass panel next to the opaque fallback over a
// busy backdrop, and the two glow uses. The sliders change the shadow strength and the glass
// blur radius live; every color and length comes from the theme tokens.

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "pages/pages.h"

namespace
{

// Layout in multiples of token values, so the page scales with the theme.
constexpr float kCardHeightControls = 3.0f;       // card height in controlHeightLarge units
constexpr float kBackdropHeightControls = 8.0f;   // glass backdrop height in controlHeightLarge
constexpr float kStackedHeightControls = 7.0f;    // the same when the panels are stacked; kept
                                                  // low so the whole section fits a short view
constexpr float kMinPanelWidthControls = 6.0f;    // narrower panels stack instead
constexpr float kPanelTopFraction = 0.3f;         // backdrop shown above the panels, so shapes
                                                  // and text cross the glass edge
constexpr float kStackedPanelTopFraction = 0.2f;  // the same for stacked panels
constexpr float kSliderWidthEm = 16.0f;           // slider width in multiples of the font size
constexpr float kGlowSampleWidthControls = 4.0f;  // glow samples: width in controlHeightLarge
constexpr int kBackdropShapes = 16;
constexpr int kBackdropLines = 12;

// Slider ranges. Strength multiplies the opacity of the elevation tokens; the blur radius is
// in design units like Glass::blurRadius.
constexpr float kStrengthMin = 0.0f;
constexpr float kStrengthMax = 3.0f;
constexpr float kBlurMin = 0.0f;
constexpr float kBlurMax = 60.0f;

struct EffectsState
{
    float shadowStrength = 1.0f;
    float blurRadius = -1.0f;  // design units; negative until the theme value is read
    bool accentGlow = true;

    // What the last frame drew, for the selftest.
    ImU32 drawnShadowColor = 0;
    float drawnBlurPixels = 0.0f;
};

EffectsState effects;
EffectsState before;  // selftest: values before an input step

float BlurRadius()
{
    if (effects.blurRadius < 0.0f)
        effects.blurRadius = facet::T().glass.blurRadius;
    return effects.blurRadius;
}

void DrawControls(facet::demo::PageCtx& ctx)
{
    ImGui::Text("Glass available: %s", facet::GlassAvailable() ? "yes, panels blur the backdrop"
                                                               : "no, panels use the opaque fallback");
    const float sliderWidth = ImGui::GetFontSize() * kSliderWidthEm;
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderFloat("Shadow strength", &effects.shadowStrength, kStrengthMin, kStrengthMax, "%.2fx");
    ctx.Probe("effects.strength");
    ImGui::SetNextItemWidth(sliderWidth);
    BlurRadius();
    ImGui::SliderFloat("Blur radius", &effects.blurRadius, kBlurMin, kBlurMax, "%.0f");
    ctx.Probe("effects.blur");
    ImGui::Checkbox("Accent glow", &effects.accentGlow);
    ctx.Probe("effects.glow");
}

facet::ShadowSpec ScaledShadow(int level)
{
    facet::ShadowSpec spec = facet::ElevationShadow(level);
    spec.color = facet::ScaleAlpha(spec.color, effects.shadowStrength);
    return spec;
}

// Title and detail inside a panel, wrapped to its width and clipped to its rect.
void DrawPanelText(ImDrawList* drawList, ImVec2 min, ImVec2 max, const char* title, const char* detail)
{
    const facet::Theme& theme = facet::T();
    const float padding = facet::Px(theme.spacing.lg);
    const float wrap = max.x - min.x - padding * 2.0f;
    if (wrap <= 0.0f)
        return;
    drawList->PushClipRect(min, max, true);
    facet::PushTextStyle(facet::TextStyle::Headline);
    ImFont* font = ImGui::GetFont();
    const float titleSize = ImGui::GetFontSize();
    const ImVec2 titlePos(min.x + padding, min.y + padding);
    drawList->AddText(font, titleSize, titlePos, facet::ToU32(theme.colors.label), title, nullptr, wrap);
    const float titleHeight = font->CalcTextSizeA(titleSize, FLT_MAX, wrap, title).y;
    facet::PopTextStyle();
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      ImVec2(titlePos.x, titlePos.y + titleHeight + facet::Px(theme.spacing.xs)),
                      facet::ToU32(theme.colors.labelSecondary), detail, nullptr, wrap);
    drawList->PopClipRect();
}

void DrawShadows()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const float gap = facet::Px(theme.spacing.xxxl);
    const float padding = facet::Px(theme.spacing.lg);
    const float radius = facet::Px(theme.radii.card);
    const float height = facet::Px(theme.metrics.controlHeightLarge * kCardHeightControls);
    // Room around the cards for the largest shadow.
    const float shadowRoom = facet::Px(theme.spacing.xxxl);
    const float available = ImGui::GetContentRegionAvail().x;
    // Three cards in a row when each fits its longest line, otherwise one per row.
    const float minWidth = ImGui::CalcTextSize("y 00, blur 00, 000% opacity").x + padding * 2.0f;
    const bool row = (available - gap * 2.0f - shadowRoom * 2.0f) / 3.0f >= minWidth;
    const float width = row ? (available - gap * 2.0f - shadowRoom * 2.0f) / 3.0f : available - shadowRoom * 2.0f;
    if (width <= 0.0f)
        return;
    const float areaHeight = row ? height + shadowRoom * 2.0f : height * 3.0f + gap * 2.0f + shadowRoom * 2.0f;

    ImGui::Dummy(ImVec2(available, areaHeight));
    const ImVec2 area = ImGui::GetItemRectMin();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int level = 1; level <= 3; ++level)
    {
        const float step = static_cast<float>(level - 1);
        const ImVec2 min = row ? ImVec2(area.x + shadowRoom + step * (width + gap), area.y + shadowRoom)
                               : ImVec2(area.x + shadowRoom, area.y + shadowRoom + step * (height + gap));
        const ImVec2 max(min.x + width, min.y + height);
        const facet::ShadowSpec spec = ScaledShadow(level);
        if (level == 2)
            effects.drawnShadowColor = spec.color;
        facet::DrawShadow(drawList, min, max, radius, spec);
        facet::FillRect(drawList, min, max, facet::ToU32(c.bgElevated), radius);

        char title[32];
        std::snprintf(title, sizeof(title), "Elevation %d", level);
        const facet::ShadowLevel& token = theme.elevation.Level(level);
        char detail[96];
        std::snprintf(detail, sizeof(detail), "y %.0f, blur %.0f, %.0f%% opacity", static_cast<double>(token.offset.y),
                      static_cast<double>(token.blur),
                      static_cast<double>(token.opacity * effects.shadowStrength * 100.0f));
        DrawPanelText(drawList, min, max, title, detail);
    }
}

// A busy backdrop of token colors: diagonal bands, circles and lines of text, all drawn with
// the draw list, so the blur behind the glass panel is easy to judge.
void DrawBackdrop(ImDrawList* drawList, ImVec2 min, ImVec2 max)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const float radius = facet::Px(theme.radii.card);
    facet::FillRect(drawList, min, max, facet::ToU32(c.bgTertiary), radius);

    drawList->PushClipRect(min, max, true);
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    const float band = facet::Px(theme.spacing.xl);
    for (float x = -height; x < width; x += band * 2.0f)
    {
        const ImVec2 a(min.x + x, max.y);
        const ImVec2 b(min.x + x + band, max.y);
        const ImVec2 d(min.x + x + height + band, min.y);
        const ImVec2 e(min.x + x + height, min.y);
        drawList->AddQuadFilled(a, b, d, e, facet::ToU32(c.fill[0]));
    }

    const ImVec4 colors[] = {c.accent, c.success, c.warning, c.destructive, c.accentText};
    const float unit = facet::Px(theme.metrics.controlHeightLarge);
    for (int i = 0; i < kBackdropShapes; ++i)
    {
        // Spread the shapes with fixed irrational steps: deterministic, never on a grid.
        const float fx = std::fmod(0.13f + static_cast<float>(i) * 0.618034f, 1.0f);
        const float fy = std::fmod(0.31f + static_cast<float>(i) * 0.414214f, 1.0f);
        const float size = unit * (0.8f + std::fmod(static_cast<float>(i) * 0.37f, 1.2f));
        const ImVec2 center(min.x + fx * width, min.y + fy * height);
        const ImU32 color = facet::ToU32(colors[i % static_cast<int>(std::size(colors))]);
        if (i % 3 == 0)
            facet::FillRect(drawList, ImVec2(center.x - size, center.y - size * 0.5f),
                            ImVec2(center.x + size, center.y + size * 0.5f), color, facet::Px(theme.radii.element));
        else
            drawList->AddCircleFilled(center, size * 0.5f, color);
    }

    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    for (int i = 0; i < kBackdropLines; ++i)
    {
        char line[96];
        std::snprintf(line, sizeof(line), "Backdrop line %d: sharp letters behind the glass turn soft.", i + 1);
        const float y = min.y + facet::Px(theme.spacing.md) + static_cast<float>(i) * lineHeight * 1.5f;
        drawList->AddText(ImVec2(min.x + facet::Px(theme.spacing.md), y),
                          facet::ToU32(i % 2 == 0 ? c.label : c.labelSecondary), line);
    }
    drawList->PopClipRect();
}

void DrawGlassSection()
{
    const facet::Theme& theme = facet::T();
    const float unit = facet::Px(theme.metrics.controlHeightLarge);
    const float inset = facet::Px(theme.spacing.xxl);
    const float gap = facet::Px(theme.spacing.xxl);
    const float width = ImGui::GetContentRegionAvail().x;
    // Side by side when both panels keep a readable width, stacked otherwise.
    const bool stacked = (width - inset * 2.0f - gap) * 0.5f < unit * kMinPanelWidthControls;
    const float height = unit * (stacked ? kStackedHeightControls : kBackdropHeightControls);
    if (width - inset * 2.0f <= 0.0f)
        return;
    ImGui::Dummy(ImVec2(width, height));
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawBackdrop(drawList, min, max);

    const float radius = facet::Px(theme.radii.popover);
    const float top = min.y + height * (stacked ? kStackedPanelTopFraction : kPanelTopFraction);
    ImVec2 glassMin;
    ImVec2 glassMax;
    ImVec2 fallbackMin;
    ImVec2 fallbackMax;
    if (stacked)
    {
        const float panelHeight = (max.y - inset - top - gap) * 0.5f;
        glassMin = ImVec2(min.x + inset, top);
        glassMax = ImVec2(max.x - inset, top + panelHeight);
        fallbackMin = ImVec2(glassMin.x, glassMax.y + gap);
        fallbackMax = ImVec2(glassMax.x, max.y - inset);
    }
    else
    {
        const float panelWidth = (width - inset * 2.0f - gap) * 0.5f;
        glassMin = ImVec2(min.x + inset, top);
        glassMax = ImVec2(glassMin.x + panelWidth, max.y - inset);
        fallbackMin = ImVec2(glassMax.x + gap, top);
        fallbackMax = ImVec2(max.x - inset, glassMax.y);
    }

    facet::GlassSpec spec;
    spec.blurRadius = facet::Px(BlurRadius());
    effects.drawnBlurPixels = spec.blurRadius;
    // Glass panels are floating layers: they get the popover elevation as well. It goes
    // through the spec so the shadow is not blurred into the panel's own backdrop.
    spec.shadow = ScaledShadow(2);

    facet::DrawGlass(drawList, glassMin, glassMax, radius, spec);
    char detail[96];
    std::snprintf(detail, sizeof(detail), "Blur %.0f, tint %.0f%%", static_cast<double>(BlurRadius()),
                  static_cast<double>(theme.glass.tintOpacity * 100.0f));
    DrawPanelText(drawList, glassMin, glassMax, facet::GlassAvailable() ? "Glass" : "Glass (fallback active)", detail);

    facet::DrawGlassFallback(drawList, fallbackMin, fallbackMax, radius, spec);
    DrawPanelText(drawList, fallbackMin, fallbackMax, "Opaque fallback", "Used when the target cannot be read");
}

void DrawGlowSection()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const float width = facet::Px(theme.metrics.controlHeightLarge * kGlowSampleWidthControls);
    const float height = facet::Px(theme.metrics.controlHeightLarge);
    const float room = facet::Px(theme.spacing.xxxl);
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float available = ImGui::GetContentRegionAvail().x;
    // Side by side when both samples fit with room for their glow, stacked otherwise.
    const bool row = width * 2.0f + room * 4.0f <= available;
    const float sampleHeight = height + room * 2.0f + lineHeight;
    ImGui::Dummy(ImVec2(available, row ? sampleHeight : sampleHeight * 2.0f));
    const ImVec2 area = ImGui::GetItemRectMin();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Focus ring halo around a control-shaped sample.
    const ImVec2 focusMin(area.x + room, area.y + room);
    const ImVec2 focusMax(focusMin.x + width, focusMin.y + height);
    const float controlRadius = facet::Px(theme.radii.control);
    facet::FillRect(drawList, focusMin, focusMax, facet::ToU32(c.fill[1]), controlRadius);
    facet::DrawGlow(drawList, focusMin, focusMax, controlRadius, facet::ToU32(c.focusRing),
                    facet::Px(theme.spacing.sm));
    facet::StrokeRect(drawList, focusMin, focusMax, facet::ToU32(c.focusRing), controlRadius,
                      facet::Px(theme.metrics.focusRingWidth) * 0.5f);
    drawList->AddText(ImVec2(focusMin.x, focusMax.y + room * 0.5f), facet::ToU32(c.labelSecondary),
                      "Focus ring halo");

    // The opt-in accent glow around an accent pill.
    const ImVec2 accentMin = row ? ImVec2(focusMax.x + room * 2.0f, focusMin.y) : ImVec2(focusMin.x, focusMin.y + sampleHeight);
    const ImVec2 accentMax(accentMin.x + width, accentMin.y + height);
    const float pill = height * 0.5f;
    if (effects.accentGlow)
        facet::DrawGlow(drawList, accentMin, accentMax, pill, facet::ToU32(c.accent), facet::Px(theme.spacing.xl));
    facet::FillRect(drawList, accentMin, accentMax, facet::ToU32(c.accent), pill);
    const char* label = "Accent sample";
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImVec2((accentMin.x + accentMax.x - labelSize.x) * 0.5f, (accentMin.y + accentMax.y - labelSize.y) * 0.5f),
                      facet::ToU32(c.onAccent), label);
    drawList->AddText(ImVec2(accentMin.x, accentMax.y + room * 0.5f), facet::ToU32(c.labelSecondary),
                      effects.accentGlow ? "Accent glow on" : "Accent glow off");
}

} // namespace

FACET_DEMO_PAGE("effects", "Effects", "Core", 20, "shadows", "glass", "glow")
{
    facet::Text(facet::TextStyle::Title1, "Effects");
    ImGui::TextWrapped("Shadows mark floating layers, glass is for the menu shell and popovers, and glow "
                       "marks the keyboard focus plus one optional accent.");
    DrawControls(ctx);

    ImGui::SeparatorText("Shadows");
    DrawShadows();
    if (ctx.captureState == 0)
        ctx.RevealInCapture(0.5f);

    ImGui::SeparatorText("Glass");
    DrawGlassSection();
    // The bottom of the glass section at the bottom of the view, so on short targets the
    // whole backdrop with both panels is in the image.
    if (ctx.captureState == 1)
        ctx.RevealInCapture(1.0f);

    ImGui::SeparatorText("Glow");
    DrawGlowSection();
    if (ctx.captureState == 2)
        ctx.RevealInCapture(1.0f);
}

FACET_DEMO_SELFTEST("effects")
{
    test.Do([] {
        effects = EffectsState{};
        before = effects;
    });
    test.Wait(1);
    test.Do([] { before = effects; });

    test.Click("effects.strength");
    test.Expect("clicking the strength slider changes the strength",
                [] { return effects.shadowStrength != before.shadowStrength; });
    test.Expect("the drawn shadow color follows the strength",
                [] { return effects.drawnShadowColor != before.drawnShadowColor; });

    test.Do([] { before = effects; });
    test.Click("effects.blur");
    test.Expect("clicking the blur slider changes the radius", [] { return effects.blurRadius != before.blurRadius; });
    test.Expect("the glass panel uses the new radius",
                [] { return effects.drawnBlurPixels == facet::Px(effects.blurRadius); });

    // Keyboard: Tab to the checkbox, Space toggles it.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("effects.blur");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("effects.glow");
    test.Do([] { before = effects; });
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the checkbox toggles the accent glow", [] { return effects.accentGlow != before.accentGlow; });

    // Tabbing onto a slider opens its value for typing with the text selected: typing a
    // number and Enter sets it.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("effects.blur");
    test.Do([] { ImGui::GetIO().AddInputCharactersUTF8("12"); });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("typing 12 and Enter sets the blur radius", [] { return effects.blurRadius == 12.0f; });
    test.Expect("the glass panel follows the typed radius",
                [] { return effects.drawnBlurPixels == facet::Px(effects.blurRadius); });

    // Space activates the focused slider for the arrow keys; Space again ends it (Escape
    // would revert to the value before activation).
    test.Do([] { before = effects; });
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("the left arrow lowers the blur radius", [] { return effects.blurRadius < before.blurRadius; });

    // The strength slider by keyboard as well.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("effects.strength");
    test.Do([] { ImGui::GetIO().AddInputCharactersUTF8("2"); });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("typing 2 and Enter sets the shadow strength", [] { return effects.shadowStrength == 2.0f; });
    test.Do([] { before = effects; });
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("the right arrow raises the shadow strength",
                [] { return effects.shadowStrength > before.shadowStrength; });
    test.Expect("the drawn shadow follows the keyboard change",
                [] { return effects.drawnShadowColor != before.drawnShadowColor; });

    test.Expect("glass is available on the offscreen target", [] { return facet::GlassAvailable(); });
}
