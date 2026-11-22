// Draw page: continuous against circular corners, hairlines at fractional scales, gradients,
// checkerboards, arcs and loading indicators, and DrawGroup fading, scaling and sliding a
// group of shapes with its clipping.

#include <algorithm>
#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/loading.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

constexpr float kTurn = 6.28318530718f;
constexpr float kQuarterTurn = kTurn * 0.25f;

// Sub-pixel offsets of the hairline rows.
constexpr float kHairlineOffsets[] = {0.0f, 0.25f, 0.5f, 0.75f};
// Ring fill levels of the arc samples.
constexpr float kRingLevels[] = {0.25f, 0.5f, 0.75f, 1.0f};
// Opacity steps of the alpha preview.
constexpr float kAlphaSteps[] = {1.0f, 0.75f, 0.5f, 0.25f};

struct DrawPageState
{
    float alpha = 1.0f;
    float scale = 1.0f;
    float offset = 0.0f;  // in multiples of spacing.xxxl
    bool shown = true;
    float appear = 1.0f;  // animated 0 (hidden) to 1 (shown)
    // For the selftest: the toggle button has been hovered for the normal hover delay.
    bool toggleHoverDelay = false;
};

DrawPageState draw;
float alphaBefore = 0.0f;  // selftest: the alpha before a held key

ImU32 U32(const ImVec4& color)
{
    return facet::ToU32(color);
}

void DrawCorners(facet::demo::PageCtx& ctx)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    bool continuous = theme.radii.continuousCorners;
    if (ImGui::Checkbox("Continuous corners in FillRect", &continuous))
    {
        facet::Theme changed = theme;
        changed.radii.continuousCorners = continuous;
        facet::SetTheme(changed, false);
    }
    facet::ItemCursor();
    ctx.Probe("draw.continuous");

    const struct
    {
        const char* name;
        float radius;
    } samples[] = {
        {"element", theme.radii.element}, {"control", theme.radii.control}, {"card", theme.radii.card},
        {"window", theme.radii.window},   {"window x 2", theme.radii.window * 2.0f},
    };
    // Four tiles per row; they shrink in narrow windows so the comparison stays side by side.
    const float gap = facet::Px(theme.spacing.lg);
    const float size = std::min(facet::Px(theme.spacing.xxxl * 2.5f), (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Note("Columns: circular arc, continuous, both with the arc outlined, FillRect (follows the switch).");
    for (const auto& sample : samples)
    {
        const float radius = facet::Px(sample.radius);
        Note("%s %.0f", sample.name, static_cast<double>(sample.radius));
        for (int column = 0; column < 4; ++column)
        {
            ImGui::Dummy(ImVec2(size, size));
            const ImVec2 a = ImGui::GetItemRectMin();
            const ImVec2 b = ImGui::GetItemRectMax();
            switch (column)
            {
            case 0:
                facet::FillRoundRect(drawList, a, b, U32(c.fill[0]), radius);
                break;
            case 1:
                facet::FillContinuousRect(drawList, a, b, U32(c.fill[0]), radius);
                break;
            case 2:
                facet::FillContinuousRect(drawList, a, b, U32(c.fill[0]), radius);
                facet::StrokeRoundRect(drawList, a, b, U32(c.accent), radius, facet::HairlineThickness());
                break;
            default:
                facet::FillRect(drawList, a, b, U32(c.fill[0]), radius);
                break;
            }
            if (column < 3)
                ImGui::SameLine(0.0f, gap);
        }
    }
    Paragraph("The continuous corner starts curving further along the edge and bends more gently, so its "
              "outline leaves the circular arc's outline near the edges.");
}

void DrawHairlines()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = std::min(facet::Px(theme.spacing.xxxl * 5.0f), ImGui::GetContentRegionAvail().x);
    const float rowGap = facet::Px(theme.spacing.md);
    const float right = facet::demo::ContentRight();
    const float height = rowGap * static_cast<float>(std::size(kHairlineOffsets) + 1);
    const char* titles[] = {"Unsnapped: Px(1) wide at the exact position", "Snapped: HairlineH"};
    for (int column = 0; column < 2; ++column)
    {
        ImGui::BeginGroup();
        Note("%s", titles[column]);
        ImGui::Dummy(ImVec2(width, height));
        const ImVec2 a = ImGui::GetItemRectMin();
        facet::FillRect(drawList, a, ImGui::GetItemRectMax(), U32(c.bgSecondary), facet::Px(theme.radii.element));
        for (size_t i = 0; i < std::size(kHairlineOffsets); ++i)
        {
            const float y = a.y + rowGap * static_cast<float>(i + 1) + kHairlineOffsets[i];
            const float x0 = a.x + rowGap;
            const float x1 = a.x + width - rowGap;
            if (column == 0)
                drawList->AddLine(ImVec2(x0, y), ImVec2(x1, y), U32(c.label), facet::Px(theme.metrics.hairline));
            else
                facet::HairlineH(drawList, x0, x1, y, U32(c.label));
        }
        ImGui::EndGroup();
        if (column == 0)
            facet::demo::SameLineIfFits(right, width, facet::Px(theme.spacing.xl));
    }
    Note("Rows sit 0, 0.25, 0.5 and 0.75 px off the pixel grid. Scale now %.2f: change it in the title strip.",
         static_cast<double>(facet::Scale()));
}

void DrawGradients()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 size(std::min(facet::Px(theme.spacing.xxxl * 5.0f), ImGui::GetContentRegionAvail().x),
                      facet::Px(theme.spacing.xxxl * 2.0f));
    const float right = facet::demo::ContentRight();
    const float radius = facet::Px(theme.radii.card);
    const float cell = facet::Px(theme.spacing.sm);
    const ImU32 checkA = U32(c.bg);
    const ImU32 checkB = U32(facet::Over(c.fill[1], c.bg));

    ImGui::BeginGroup();
    Note("Horizontal: accent to transparent, over a checkerboard");
    ImGui::Dummy(size);
    facet::FillCheckerboard(drawList, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), checkA, checkB, cell, radius);
    facet::FillGradientRect(drawList, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), U32(c.accent),
                            U32(facet::WithAlpha(c.accent, 0.0f)), facet::GradientDir::Horizontal, radius);
    ImGui::EndGroup();
    facet::demo::SameLineIfFits(right, size.x, facet::Px(theme.spacing.xl));
    ImGui::BeginGroup();
    Note("Vertical: bgTertiary to bgSecondary");
    ImGui::Dummy(size);
    facet::FillGradientRect(drawList, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), U32(c.bgTertiary),
                            U32(c.bgSecondary), facet::GradientDir::Vertical, radius);
    facet::StrokeRect(drawList, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), U32(c.separator), radius,
                      facet::HairlineThickness());
    ImGui::EndGroup();

    Note("Alpha preview: the accent at 100, 75, 50 and 25 %%");
    const float chip = facet::Px(theme.metrics.controlHeightLarge);
    for (size_t i = 0; i < std::size(kAlphaSteps); ++i)
    {
        const float alpha = kAlphaSteps[i];
        ImGui::Dummy(ImVec2(chip, chip));
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        const float r = facet::Px(theme.radii.element);
        facet::FillCheckerboard(drawList, a, b, checkA, checkB, facet::Px(theme.spacing.xs * 1.5f), r);
        facet::FillRect(drawList, a, b, U32(facet::WithAlpha(c.accent, alpha)), r);
        if (i + 1 < std::size(kAlphaSteps))
            facet::demo::SameLineIfFits(right, chip);
    }
}

void DrawArcsAndLoading()
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float size = facet::Px(theme.spacing.xxxl * 1.5f);
    const float thickness = facet::Px(theme.metrics.sliderTrackHeight);
    const float right = facet::demo::ContentRight();
    char label[16];
    for (const float level : kRingLevels)
    {
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(size, size));
        const ImVec2 center((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                            (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
        const float radius = size * 0.5f - thickness;
        facet::StrokeArc(drawList, center, radius, 0.0f, kTurn, U32(c.fill[1]), thickness, false);
        // From twelve o'clock, clockwise.
        facet::StrokeArc(drawList, center, radius, -kQuarterTurn, -kQuarterTurn + kTurn * level, U32(c.accent),
                         thickness, true);
        std::snprintf(label, sizeof(label), "%.0f%%", static_cast<double>(level * 100.0f));
        Note("%s", label);
        ImGui::EndGroup();
        facet::demo::SameLineIfFits(right, size, facet::Px(theme.spacing.lg));
    }
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(size, size));
    const ImVec2 spinnerCenter((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
    facet::internal::DrawSpinner(drawList, spinnerCenter, size * 0.5f - thickness, U32(c.accent), thickness);
    Note("Spinner");
    ImGui::EndGroup();
    const float skeletonWidth = std::min(facet::Px(theme.spacing.xxxl * 6.0f), right - ImGui::GetCursorScreenPos().x);
    facet::demo::SameLineIfFits(right, skeletonWidth, facet::Px(theme.spacing.xl));

    // Skeleton lines of a loading list row (placeholder content).
    ImGui::BeginGroup();
    const float lineHeight = facet::Px(theme.spacing.md);
    const float widths[] = {theme.spacing.xxxl * 6.0f, theme.spacing.xxxl * 4.5f, theme.spacing.xxxl * 5.0f};
    for (const float width : widths)
    {
        ImGui::Dummy(ImVec2(std::min(facet::Px(width), skeletonWidth), lineHeight));
        facet::internal::DrawSkeleton(drawList, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                      facet::Px(theme.radii.element));
    }
    Note("Skeleton, shimmer every %.1f s", static_cast<double>(theme.motion.shimmerPeriod * theme.motion.scale));
    ImGui::EndGroup();
}

void DrawGroupDemo(facet::demo::PageCtx& ctx)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    const float sliderWidth = facet::Px(theme.spacing.xxxl * 5.0f);

    facet::demo::LabeledRow("Alpha");
    ImGui::SetNextItemWidth(std::min(sliderWidth, ImGui::GetContentRegionAvail().x));
    ImGui::SliderFloat("##alpha", &draw.alpha, 0.0f, 1.0f, "%.2f");
    facet::ItemCursor();
    ctx.Probe("draw.alpha");
    facet::demo::LabeledRow("Scale");
    ImGui::SetNextItemWidth(std::min(sliderWidth, ImGui::GetContentRegionAvail().x));
    ImGui::SliderFloat("##scale", &draw.scale, 0.8f, 1.1f, "%.2f");
    facet::ItemCursor();
    ctx.Probe("draw.scale");
    facet::demo::LabeledRow("Slide");
    ImGui::SetNextItemWidth(std::min(sliderWidth, ImGui::GetContentRegionAvail().x));
    ImGui::SliderFloat("##offset", &draw.offset, -1.0f, 1.0f, "%.2f");
    facet::ItemCursor();
    ctx.Probe("draw.offset");
    // A fixed id: the label changes, the item (and its keyboard focus) stays.
    if (ImGui::Button(draw.shown ? "Hide card###toggle" : "Show card###toggle"))
        draw.shown = !draw.shown;
    facet::ItemCursor();
    ctx.Probe("draw.toggle");
    draw.toggleHoverDelay = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);

    draw.appear = facet::Tween(ImGui::GetID("##appear"), 0, draw.shown ? 1.0f : 0.0f, theme.motion.durationStandard,
                               facet::Ease::OutCubic);

    // The card: a surface, a title, and a list clipped to an inner frame.
    const float margin = facet::Px(theme.spacing.xxxl);
    const ImVec2 cardSize(std::min(facet::Px(theme.spacing.xxxl * 8.0f), ImGui::GetContentRegionAvail().x - margin * 2.0f),
                          facet::Px(theme.spacing.xxxl * 4.0f));
    ImGui::Dummy(ImVec2(cardSize.x + facet::Px(theme.spacing.xxxl) * 2.0f, cardSize.y + facet::Px(theme.spacing.lg)));
    const ImVec2 area = ImGui::GetItemRectMin();
    const ImVec2 a(area.x + facet::Px(theme.spacing.xxxl), area.y + facet::Px(theme.spacing.sm));
    const ImVec2 b(a.x + cardSize.x, a.y + cardSize.y);
    const ImVec2 center((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // The spot the card occupies at rest.
    facet::StrokeRect(drawList, a, b, U32(c.separator), facet::Px(theme.radii.card), facet::HairlineThickness());

    const float alpha = draw.alpha * draw.appear;
    const float scale = draw.scale * (0.96f + 0.04f * draw.appear);
    const ImVec2 slide(draw.offset * facet::Px(theme.spacing.xxxl), 0.0f);
    {
        facet::DrawGroup group(drawList, alpha, scale, center, slide);
        const float radius = facet::Px(theme.radii.card);
        facet::FillRect(drawList, a, b, U32(c.bgElevated), radius);
        facet::StrokeRect(drawList, a, b, U32(c.separatorOpaque), radius, facet::HairlineThickness());
        const float pad = facet::Px(theme.spacing.lg);
        facet::PushTextStyle(facet::TextStyle::Headline);
        drawList->AddText(ImVec2(a.x + pad, a.y + pad), U32(c.label), "Grouped card");
        const float titleBottom = a.y + pad + ImGui::GetFontSize() + facet::Px(theme.spacing.sm);
        facet::PopTextStyle();

        // An inner list clipped to its frame: the clipping moves with the group.
        const ImVec2 listMin(a.x + pad, titleBottom);
        const ImVec2 listMax(b.x - pad, b.y - pad);
        facet::FillRect(drawList, listMin, listMax, U32(c.fill[3]), facet::Px(theme.radii.element));
        drawList->PushClipRect(listMin, listMax, true);
        facet::PushTextStyle(facet::TextStyle::Body);
        const float line = ImGui::GetTextLineHeightWithSpacing();
        char text[32];
        for (int i = 0; i < 4; ++i)
        {
            std::snprintf(text, sizeof(text), "Placeholder row %d", i + 1);
            drawList->AddText(ImVec2(listMin.x + facet::Px(theme.spacing.sm),
                                     listMin.y + facet::Px(theme.spacing.xs) + line * static_cast<float>(i)),
                              U32(c.label), text);
        }
        facet::PopTextStyle();
        drawList->PopClipRect();
    }
    Note("Alpha %.2f, scale %.3f, slide %.0f px. The inner list stays clipped to its own frame.",
         static_cast<double>(alpha), static_cast<double>(scale), static_cast<double>(slide.x));
}

} // namespace

FACET_DEMO_PAGE("draw", "Draw", "Foundations", 3, "default", "group")
{
    if (ctx.capture)
    {
        const bool group = ctx.captureState == 1;
        draw.alpha = group ? 0.6f : 1.0f;
        draw.scale = group ? 0.9f : 1.0f;
        draw.offset = group ? 0.5f : 0.0f;
    }

    facet::demo::PageHeader("Draw", "The drawing helpers every widget builds on. All lengths are pixels after "
                                    "Px(); colors come from the theme.");

    SectionHeader("Corners");
    DrawCorners(ctx);

    SectionHeader("Hairlines");
    DrawHairlines();

    SectionHeader("Gradients and alpha");
    DrawGradients();

    SectionHeader("Arcs and loading");
    DrawArcsAndLoading();

    SectionHeader("DrawGroup");
    DrawGroupDemo(ctx);
    if (ctx.captureState == 1)
        ctx.RevealInCapture(0.8f);
}

FACET_DEMO_SELFTEST("draw")
{
    test.Do([] {
        draw = DrawPageState{};
        facet::Theme theme = facet::T();
        theme.radii.continuousCorners = true;
        facet::SetTheme(theme, false);
    });
    test.Click("draw.continuous");
    test.Expect("the checkbox turns continuous corners off", [] { return !facet::T().radii.continuousCorners; });
    // After a click the first Tab only shows the keyboard cursor on the clicked item.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("draw.continuous");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space turns them on again", [] { return facet::T().radii.continuousCorners; });

    test.Click("draw.toggle");
    test.Expect("Hide starts fading the card out", [] { return draw.appear > 0.0f && draw.appear < 1.0f; });
    test.Wait(30);
    test.Expect("the card is hidden after the transition", [] { return draw.appear == 0.0f; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("draw.toggle");
    test.KeyPress(ImGuiKey_Space);
    test.Wait(30);
    test.Expect("Space on the button shows it again", [] { return draw.shown && draw.appear == 1.0f; });

    test.Click("draw.alpha");
    test.Expect("clicking the middle of the alpha slider sets about 0.5",
                [] { return draw.alpha > 0.4f && draw.alpha < 0.6f; });
    test.Click("draw.offset");
    test.Expect("clicking the middle of the slide slider centers the card",
                [] { return draw.offset > -0.15f && draw.offset < 0.15f; });

    using facet::demo::Target;
    // A drag through intermediate points: the value follows the pointer to the last point.
    test.Drag({Target::Item("draw.alpha", 0.1f), Target::Item("draw.alpha", 0.9f), Target::Item("draw.alpha", 0.3f)},
              0.5f);
    test.ExpectNear("dragging the alpha slider out to 0.9 and back to 0.3 leaves about 0.3",
                    [] { return static_cast<double>(draw.alpha); }, 0.3, 0.06);
    // A drag that leaves the window: the slider follows to the end of its range.
    test.Drag({Target::Item("draw.offset"), Target::Display(1.3f, 0.5f)}, 0.25f);
    test.ExpectNear("dragging the slide slider out of the window ends at the maximum",
                    [] { return static_cast<double>(draw.offset); }, 1.0, 1e-6);

    // A held key repeats: one second of the right arrow moves the activated slider many steps.
    // The first Tab after a click opens the slider's text input on the clicked slider; Enter
    // leaves it unchanged, and Space then lets the arrow keys move the value.
    test.Click("draw.alpha");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("draw.alpha");
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Space);
    test.Do([] { alphaBefore = draw.alpha; });
    test.HoldKey(ImGuiKey_RightArrow, 1.0f);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("holding the right arrow for 1 s raises the alpha by many steps",
                [] { return draw.alpha > alphaBefore + 0.1f; });

    // Hovering for a while: the hover delay of tooltips counts seconds of frame time.
    test.Hover(Target::Item("draw.toggle"), 0.1f);
    test.Expect("after 0.1 s the button is not yet hovered for a tooltip", [] { return !draw.toggleHoverDelay; });
    test.Hover(Target::Item("draw.toggle"), 0.5f);
    test.Expect("after 0.6 s in all it is", [] { return draw.toggleHoverDelay; });
}
