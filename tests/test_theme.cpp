// Theme presets, switching, blending and the Dear ImGui style mapping.

#include <cstddef>
#include <vector>

#include <doctest/doctest.h>

#include <facet/facet.h>

namespace
{

bool SameColor(const ImVec4& a, const ImVec4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

bool SameColors(const facet::Colors& a, const facet::Colors& b)
{
    bool same = SameColor(a.bg, b.bg) && SameColor(a.bgSecondary, b.bgSecondary) &&
                SameColor(a.bgTertiary, b.bgTertiary) && SameColor(a.bgElevated, b.bgElevated) &&
                SameColor(a.label, b.label) && SameColor(a.labelSecondary, b.labelSecondary) &&
                SameColor(a.accent, b.accent) && SameColor(a.accentText, b.accentText) &&
                SameColor(a.onAccent, b.onAccent) && SameColor(a.scrim, b.scrim) &&
                SameColor(a.separator, b.separator) && SameColor(a.focusRing, b.focusRing);
    for (int i = 0; i < 4; ++i)
        same = same && SameColor(a.fill[i], b.fill[i]);
    for (int i = 0; i < 3; ++i)
        same = same && SameColor(a.bgElevatedLevels[i], b.bgElevatedLevels[i]);
    for (int i = 0; i < facet::kChartPaletteSize; ++i)
        same = same && SameColor(a.chart[i], b.chart[i]);
    same = same && SameColor(a.destructiveHover, b.destructiveHover) &&
           SameColor(a.destructivePressed, b.destructivePressed) && SameColor(a.onDestructive, b.onDestructive) &&
           SameColor(a.onSuccess, b.onSuccess) && SameColor(a.onWarning, b.onWarning) &&
           SameColor(a.destructiveSubtle, b.destructiveSubtle) && SameColor(a.successSubtle, b.successSubtle) &&
           SameColor(a.warningSubtle, b.warningSubtle) && SameColor(a.accentTextVisited, b.accentTextVisited) &&
           SameColor(a.codeKeyword, b.codeKeyword) && SameColor(a.codeString, b.codeString) &&
           SameColor(a.codeNumber, b.codeNumber) && SameColor(a.checker[0], b.checker[0]) &&
           SameColor(a.checker[1], b.checker[1]);
    return same && SameColor(a.scrollThumb, b.scrollThumb) && SameColor(a.scrollThumbHover, b.scrollThumbHover);
}

// The colors added for the first widgets, as pointers into a theme.
std::vector<ImVec4*> WidgetColorTokens(facet::Theme& t)
{
    facet::Colors& c = t.colors;
    return {&c.destructiveHover, &c.destructivePressed, &c.onDestructive, &c.onSuccess, &c.onWarning,
            &c.destructiveSubtle, &c.successSubtle, &c.warningSubtle, &c.accentTextVisited,
            &c.codeKeyword, &c.codeString, &c.codeNumber, &c.checker[0], &c.checker[1]};
}

// Every length and time token of the groups for popups, containers, scrolling, lists, drag and
// drop, text fields, tabs, tables and pickers, the control sizes in Metrics and the popup motion,
// as pointers into a theme, so one loop checks them all.
std::vector<float*> AddedFloatTokens(facet::Theme& t)
{
    facet::OverlayTokens& o = t.overlay;
    facet::ContainerTokens& c = t.containers;
    facet::ScrollTokens& s = t.scroll;
    facet::ListTokens& l = t.list;
    facet::DragTokens& d = t.drag;
    facet::Metrics& m = t.metrics;
    return {&o.anchorGap, &o.viewportMargin, &o.menuMinWidth, &o.submenuDelay, &o.tooltipDelay, &o.tooltipGrace,
            &o.tooltipMaxWidth, &o.toastDuration, &o.toastWidth, &o.toastStackGap, &o.toastMargin, &o.modalWidth,
            &o.drawerWidth, &o.drawerHeight, &o.commandPaletteWidth, &o.menuShortcutGap,
            &t.tabs.minWidth, &t.tabs.maxWidth, &t.table.headerHeight, &t.table.minColumnWidth, &t.table.resizeBand,
            &t.picker.dayCell,
            &c.shellSize.x, &c.shellSize.y, &c.shellMinSize.x, &c.shellMinSize.y, &c.titleBarHeight,
            &c.resizeBorder, &c.sidebarWidth, &c.railWidth, &c.inspectorWidth, &c.inspectorMinWidth,
            &s.barThickness, &s.barThicknessHover, &s.barInset, &s.minThumbLength, &s.barHideDelay,
            &s.wheelDistance, &s.edgeFade,
            &l.rowHeightSmall, &l.rowHeight, &l.rowHeightLarge, &l.indent, &l.typeaheadTimeout,
            &d.startThreshold, &d.ghostOpacity, &d.autoScrollZone, &d.autoScrollSpeed, &d.hoverOpenDelay,
            &t.field.padding, &t.field.caretWidth,
            &m.faderLength, &m.dialSweep, &m.padSize, &m.pickerWidth, &m.numberFieldWidth, &m.histogramHeight,
            &m.chartHeight, &m.panelHeight, &m.cardWidth, &m.thumbnailSize, &m.slotSize,
            &t.motion.popupOpen, &t.motion.popupClose, &t.motion.popupScaleFrom,
            &m.statusDotSize, &m.pageDotSize, &m.sliderTickSize, &m.segmentInset, &m.progressHeightSmall,
            &m.progressHeight, &m.progressHeightLarge, &m.emptyIconSize, &c.statusBarHeight,
            &t.motion.feedbackHold, &t.motion.pulsePeriod,
            // The tokens of the second widget wave.
            &m.stepMarkerSize, &m.stepConnectorWidth, &m.checkerCell, &t.layout.dividerBand,
            &t.layout.panelHeaderHeight, &o.arrowSize, &o.commandPaletteTop, &d.flickSpeed, &d.inertiaTime,
            &t.tabs.underlineHeight, &t.picker.colorBarHeight, &t.picker.hueRingWidth, &t.picker.timeColumnWidth,
            &t.charts.lineWidth, &t.charts.markerSize, &t.charts.barGap, &t.charts.legendSwatch,
            &t.charts.donutHole, &t.charts.gaugeWidth, &t.charts.meterSegmentGap, &t.charts.peakHold,
            &t.media.avatarOverlap, &t.media.playheadWidth, &t.media.dashLength, &t.media.dashGap,
            // The tokens of the composites.
            &t.composites.editorHandleSize, &t.composites.gradientStopSize, &t.composites.stopRemoveDistance,
            &t.composites.easingPreviewTime, &t.composites.propertyLabelMinWidth,
            &t.composites.searchFieldMinWidth, &t.composites.keybindColumnWidth,
            &t.composites.inventoryTintHeight, &t.composites.mediaCoverAspect,
            &t.composites.mediaCardHorizontalBelow, &t.composites.canvasSnapDistance,
            &t.composites.canvasHandleSize};
}

// Restores the global theme state after a test changes it.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    facet::ThemeMode mode = facet::GetThemeMode();
    ~ThemeGuard()
    {
        facet::ClearAccent();
        facet::SetThemeMode(mode, false);
        facet::SetTheme(saved, false);
    }
};

} // namespace

TEST_CASE("Elevated surfaces get lighter with the level in dark and stay white in light")
{
    const facet::Colors dark = facet::Theme::Dark().colors;
    CHECK(SameColor(dark.ElevatedSurface(2), dark.bgElevated));
    CHECK(facet::RelativeLuminance(dark.ElevatedSurface(1)) > facet::RelativeLuminance(dark.bgSecondary));
    CHECK(facet::RelativeLuminance(dark.ElevatedSurface(2)) > facet::RelativeLuminance(dark.ElevatedSurface(1)));
    CHECK(facet::RelativeLuminance(dark.ElevatedSurface(3)) > facet::RelativeLuminance(dark.ElevatedSurface(2)));
    // Out-of-range levels clamp.
    CHECK(SameColor(dark.ElevatedSurface(0), dark.ElevatedSurface(1)));
    CHECK(SameColor(dark.ElevatedSurface(7), dark.ElevatedSurface(3)));

    const facet::Colors light = facet::Theme::Light().colors;
    for (int level = 1; level <= 3; ++level)
        CHECK(SameColor(light.ElevatedSurface(level), light.bgElevated));
}

TEST_CASE("The token groups hold the documented starting values")
{
    const facet::Theme theme = facet::Theme::Light();
    CHECK(theme.spacing.xs == 4.0f);
    CHECK(theme.spacing.xxxl == 32.0f);
    CHECK(theme.radii.control == 8.0f);
    CHECK(theme.radii.window == 16.0f);
    CHECK(theme.radii.continuousFrom == 10.0f);
    CHECK(theme.metrics.controlHeight == 28.0f);
    CHECK(theme.type[facet::TextStyle::Body].size == 13.0f);
    CHECK(theme.type[facet::TextStyle::Headline].weight == facet::FontWeight::Semibold);
    CHECK(theme.type[facet::TextStyle::LargeTitle].size == 26.0f);
    CHECK(theme.type[facet::TextStyle::Caption].size == 11.0f);
    CHECK(theme.motion.durationFast == doctest::Approx(0.12f));
    CHECK(theme.motion.durationStandard == doctest::Approx(0.2f));
    CHECK(theme.motion.durationSlow == doctest::Approx(0.35f));
    CHECK(theme.elevation.Level(0).blur == theme.elevation.Level(1).blur);
    CHECK(theme.elevation.Level(9).blur == theme.elevation.Level(3).blur);
    CHECK(theme.elevation.Level(3).blur > theme.elevation.Level(1).blur);
    CHECK_FALSE(theme.dark);
    CHECK(facet::Theme::Dark().dark);
}

TEST_CASE("SetTheme and SetAccent change the current theme")
{
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Dark(), false);
    CHECK(facet::T().dark);
    facet::SetAccent(IM_COL32(0xFF, 0xD6, 0x0A, 0xFF));
    CHECK(facet::T().colors.accent.x == doctest::Approx(1.0f));
    CHECK(facet::ContrastRatio(facet::T().colors.onAccent, facet::T().colors.accent) >= 4.5f);
    facet::SetTheme(facet::Theme::Light(), false);
    CHECK_FALSE(facet::T().dark);
    CHECK(facet::T().colors.accent.z == doctest::Approx(facet::Theme::Light().colors.accent.z));
}

TEST_CASE("An animated SetTheme switches at once while Facet is not running")
{
    ThemeGuard guard;
    REQUIRE_FALSE(facet::IsInitialized());
    facet::SetTheme(facet::Theme::Dark(), true);
    CHECK_FALSE(facet::ThemeTransitionActive());
    CHECK(facet::T().dark);
    CHECK(SameColors(facet::T().colors, facet::Theme::Dark().colors));
}

TEST_CASE("MixThemes returns exact copies at the endpoints")
{
    const facet::Theme light = facet::Theme::Light();
    facet::Theme dark = facet::Theme::Dark();
    dark.spacing.md = 20.0f;
    dark.radii.card = 4.0f;

    const facet::Theme start = facet::MixThemes(light, dark, 0.0f);
    CHECK(SameColors(start.colors, light.colors));
    CHECK(start.spacing.md == light.spacing.md);
    CHECK(start.radii.card == light.radii.card);
    CHECK_FALSE(start.dark);

    const facet::Theme end = facet::MixThemes(light, dark, 1.0f);
    CHECK(SameColors(end.colors, dark.colors));
    CHECK(end.spacing.md == 20.0f);
    CHECK(end.radii.card == 4.0f);
    CHECK(end.dark);

    // Values outside 0 to 1 are clamped.
    CHECK(SameColors(facet::MixThemes(light, dark, -2.0f).colors, light.colors));
    CHECK(SameColors(facet::MixThemes(light, dark, 3.0f).colors, dark.colors));
}

TEST_CASE("MixThemes blends colors in OKLab and lengths linearly")
{
    const facet::Theme light = facet::Theme::Light();
    facet::Theme dark = facet::Theme::Dark();
    dark.spacing.md = 20.0f;

    const facet::Theme mid = facet::MixThemes(light, dark, 0.5f);
    CHECK(mid.spacing.md == doctest::Approx((light.spacing.md + 20.0f) * 0.5f));
    // White to black at the midpoint is the OKLab midpoint: lightness 0.5.
    CHECK(facet::ToOklab(mid.colors.bg).L == doctest::Approx(0.5f).epsilon(0.01));
    const ImVec4 expected = facet::Mix(light.colors.accent, dark.colors.accent, 0.5f);
    CHECK(mid.colors.accent.x == doctest::Approx(expected.x));
    CHECK(mid.colors.accent.z == doctest::Approx(expected.z));
    // Switches flip at the halfway point.
    CHECK(mid.dark);
    CHECK_FALSE(facet::MixThemes(light, dark, 0.49f).dark);

    // Lightness of the background falls monotonically along the blend.
    float previous = 2.0f;
    for (int i = 0; i <= 10; ++i)
    {
        const float L = facet::ToOklab(facet::MixThemes(light, dark, static_cast<float>(i) / 10.0f).colors.bg).L;
        CHECK(L <= previous + 1e-5f);
        previous = L;
    }
}

TEST_CASE("MixThemes blends the popup, container, scroll, list, drag and field tokens")
{
    facet::Theme a = facet::Theme::Light();
    facet::Theme b = facet::Theme::Dark();
    std::vector<float*> fromA = AddedFloatTokens(a);
    std::vector<float*> fromB = AddedFloatTokens(b);
    REQUIRE(fromA.size() == fromB.size());
    // b gets every token doubled plus one, so each one differs and has a known midpoint.
    for (std::size_t i = 0; i < fromB.size(); ++i)
        *fromB[i] = *fromA[i] * 2.0f + 1.0f;
    b.overlay.toastMaxCount = a.overlay.toastMaxCount + 2;
    b.list.popupRows = a.list.popupRows + 4;
    b.list.inlineRows = a.list.inlineRows + 4;

    facet::Theme mid = facet::MixThemes(a, b, 0.5f);
    facet::Theme start = facet::MixThemes(a, b, 0.0f);
    facet::Theme end = facet::MixThemes(a, b, 1.0f);
    const std::vector<float*> fromMid = AddedFloatTokens(mid);
    const std::vector<float*> fromStart = AddedFloatTokens(start);
    const std::vector<float*> fromEnd = AddedFloatTokens(end);
    for (std::size_t i = 0; i < fromMid.size(); ++i)
    {
        CAPTURE(i);
        CHECK(*fromMid[i] == doctest::Approx((*fromA[i] + *fromB[i]) * 0.5f));
        CHECK(*fromStart[i] == *fromA[i]);
        CHECK(*fromEnd[i] == *fromB[i]);
    }
    // Counts switch at the halfway point, like enums.
    CHECK(facet::MixThemes(a, b, 0.49f).overlay.toastMaxCount == a.overlay.toastMaxCount);
    CHECK(mid.overlay.toastMaxCount == b.overlay.toastMaxCount);
    CHECK(mid.list.popupRows == b.list.popupRows);
    CHECK(mid.list.inlineRows == b.list.inlineRows);
    // The new colors blend in OKLab like the others.
    const ImVec4 expected = facet::Mix(a.colors.scrollThumb, b.colors.scrollThumb, 0.5f);
    CHECK(mid.colors.scrollThumb.x == doctest::Approx(expected.x));
    CHECK(mid.colors.scrollThumb.w == doctest::Approx(expected.w));
    const ImVec4 chart = facet::Mix(a.colors.chart[3], b.colors.chart[3], 0.5f);
    CHECK(mid.colors.chart[3].y == doctest::Approx(chart.y));
    // So do the colors of the first widgets, every one of them.
    const std::vector<ImVec4*> colorsA = WidgetColorTokens(a);
    const std::vector<ImVec4*> colorsB = WidgetColorTokens(b);
    const std::vector<ImVec4*> colorsMid = WidgetColorTokens(mid);
    const std::vector<ImVec4*> colorsStart = WidgetColorTokens(start);
    const std::vector<ImVec4*> colorsEnd = WidgetColorTokens(end);
    for (std::size_t i = 0; i < colorsMid.size(); ++i)
    {
        CAPTURE(i);
        const ImVec4 blend = facet::Mix(*colorsA[i], *colorsB[i], 0.5f);
        CHECK(colorsMid[i]->x == doctest::Approx(blend.x));
        CHECK(colorsMid[i]->y == doctest::Approx(blend.y));
        CHECK(colorsMid[i]->w == doctest::Approx(blend.w));
        CHECK(SameColor(*colorsStart[i], *colorsA[i]));
        CHECK(SameColor(*colorsEnd[i], *colorsB[i]));
    }
}

TEST_CASE("The tokens of the first widgets hold their documented starting values in both presets")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Metrics& m = theme.metrics;
        CHECK(m.statusDotSize == 8.0f);
        CHECK(m.pageDotSize < m.statusDotSize);
        CHECK(m.sliderTickSize == m.sliderTrackHeight);
        CHECK(m.segmentInset == m.toggleKnobInset);
        CHECK(m.progressHeightSmall == m.sliderTrackHeight);
        CHECK(m.progressHeightSmall < m.progressHeight);
        CHECK(m.progressHeight < m.progressHeightLarge);
        CHECK(m.emptyIconSize == theme.spacing.xxxl);
        CHECK(theme.containers.statusBarHeight == theme.list.rowHeightSmall);
        CHECK(theme.motion.feedbackHold > theme.motion.durationSlow);
        CHECK(theme.motion.pulsePeriod > theme.motion.spinnerPeriod);
        // The washes are the status colors at 14 %, never stronger than accentSubtle.
        const facet::Colors& c = theme.colors;
        CHECK(c.destructiveSubtle.w == doctest::Approx(0.14f));
        CHECK(c.successSubtle.w == doctest::Approx(0.14f));
        CHECK(c.warningSubtle.w == doctest::Approx(0.14f));
        CHECK(c.destructiveSubtle.w <= c.accentSubtle.w);
        CHECK(c.destructiveSubtle.x == doctest::Approx(c.destructive.x));
    }
}

TEST_CASE("The tokens of the second widget wave hold their documented starting values in both presets")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Metrics& m = theme.metrics;
        CHECK(m.stepMarkerSize == 24.0f);
        CHECK(m.stepMarkerSize >= m.minHitTarget);  // a done step can be activated
        CHECK(m.stepConnectorWidth == 2.0f);
        CHECK(m.checkerCell == 6.0f);
        CHECK(theme.layout.dividerBand == theme.table.resizeBand);
        CHECK(theme.layout.panelHeaderHeight == theme.list.rowHeight);
        CHECK(theme.overlay.arrowSize == theme.spacing.sm);
        CHECK(theme.overlay.commandPaletteTop == theme.spacing.xxxl * 3.0f);
        CHECK(theme.drag.flickSpeed == 600.0f);
        CHECK(theme.drag.inertiaTime == doctest::Approx(0.3f));
        CHECK(theme.tabs.underlineHeight == m.stepConnectorWidth);
        CHECK(theme.picker.colorBarHeight == 12.0f);
        CHECK(theme.picker.colorBarHeight < m.sliderKnobSize);  // the knob still overlaps the bar
        CHECK(theme.picker.hueRingWidth == 20.0f);
        CHECK(theme.picker.timeColumnWidth == 48.0f);
        const facet::ChartTokens& c = theme.charts;
        CHECK(c.lineWidth == 2.0f);
        CHECK(c.markerSize == m.pageDotSize);
        CHECK(c.barGap == 2.0f);
        CHECK(c.legendSwatch == 10.0f);
        CHECK(c.donutHole == doctest::Approx(0.6f));
        CHECK(c.gaugeWidth == theme.picker.colorBarHeight);
        CHECK(c.meterSegmentGap == 2.0f);
        CHECK(c.peakHold == doctest::Approx(1.0f));
        CHECK(theme.media.avatarOverlap == doctest::Approx(0.25f));
        CHECK(theme.media.playheadWidth == c.lineWidth);
        CHECK(theme.media.dashLength == 6.0f);
        CHECK(theme.media.dashGap == theme.spacing.xs);
    }
}

TEST_CASE("The tokens of the composites hold their documented starting values in both presets")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Metrics& m = theme.metrics;
        const facet::CompositeTokens& c = theme.composites;
        // An editor handle reads as grabbable next to a chart marker and stays below the slider
        // knob, so points close together stay apart; its hit target grows to minHitTarget.
        CHECK(c.editorHandleSize == 10.0f);
        CHECK(c.editorHandleSize > theme.charts.markerSize);
        CHECK(c.editorHandleSize < m.sliderKnobSize);
        // A stop marker shows its color inside and fits the default bar height (twice a control).
        CHECK(c.gradientStopSize == 14.0f);
        CHECK(c.gradientStopSize > c.editorHandleSize);
        CHECK(c.gradientStopSize < m.controlHeight);
        CHECK(c.stopRemoveDistance == theme.spacing.xxxl);
        CHECK(c.easingPreviewTime == doctest::Approx(1.0f));
        CHECK(c.easingPreviewTime > theme.motion.durationSlow);  // slow enough to follow the curve
        CHECK(c.propertyLabelMinWidth == theme.spacing.xxxl * 2.0f);
        CHECK(c.propertyLabelMinWidth < theme.layout.labelColumn);
        CHECK(c.searchFieldMinWidth == 160.0f);
        CHECK(c.searchFieldMinWidth >= theme.layout.minRowControl);
        CHECK(c.keybindColumnWidth == c.searchFieldMinWidth);
        CHECK(c.inventoryTintHeight == m.sliderTrackHeight);
        CHECK(c.mediaCoverAspect == doctest::Approx(9.0f / 16.0f));
        CHECK(c.mediaCardHorizontalBelow == m.padSize);
        CHECK(c.mediaCardHorizontalBelow < m.cardWidth);  // the default card width lays out vertically
        CHECK(c.canvasSnapDistance == 6.0f);
        CHECK(c.canvasSnapDistance > theme.drag.startThreshold);  // the snap band is wider than a slip of the press
        CHECK(c.canvasHandleSize == m.sliderTrackHeight * 2.0f);
        CHECK(c.canvasHandleSize < m.minHitTarget);
    }
}

TEST_CASE("The checkerboard cells differ in both presets and stay in the range of the theme's surfaces")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Colors& c = theme.colors;
        CHECK(c.checker[0].w == 1.0f);
        CHECK(c.checker[1].w == 1.0f);
        // The cells are told apart, so alpha shows, but they never compete with the color on them.
        const float between = facet::ContrastRatio(c.checker[0], c.checker[1]);
        CHECK(between >= 1.3f);
        CHECK(between <= 2.0f);
        // A light grey pair in light, a dark one in dark: both cells on the theme's side of mid grey.
        for (const ImVec4& cell : c.checker)
            CHECK((facet::RelativeLuminance(cell) > 0.18f) == !theme.dark);
    }
}

TEST_CASE("ApplyAccent derives the status states from changed status colors")
{
    facet::Theme theme = facet::Theme::Light();
    theme.colors.destructive = facet::Rgb(0x0A7A3D);
    theme.colors.warning = facet::Rgb(0xFFE066);
    theme.ApplyAccent(theme.colors.accent);
    const facet::Colors& c = theme.colors;
    CHECK(c.destructiveSubtle.x == doctest::Approx(c.destructive.x));
    CHECK(c.destructiveSubtle.y == doctest::Approx(c.destructive.y));
    CHECK(facet::ContrastRatio(c.onDestructive, c.destructive) >= 4.5f);
    CHECK(facet::ContrastRatio(c.onDestructive, c.destructivePressed) >= 4.5f);
    // A light warning takes black text.
    CHECK(c.onWarning.x == doctest::Approx(0.0f));
}

TEST_CASE("The tokens of part B hold their documented starting values in both presets")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        CHECK(theme.overlay.anchorGap == theme.spacing.xs);
        CHECK(theme.overlay.viewportMargin == theme.spacing.sm);
        CHECK(theme.overlay.toastMaxCount == 3);
        CHECK(theme.list.popupRows == 8);
        CHECK(theme.list.rowHeight > theme.metrics.controlHeight);
        CHECK(theme.scroll.barThicknessHover > theme.scroll.barThickness);
        CHECK(theme.scroll.minThumbLength >= theme.metrics.minHitTarget);
        CHECK(theme.containers.shellMinSize.x < theme.containers.shellSize.x);
        CHECK(theme.containers.shellMinSize.y < theme.containers.shellSize.y);
        CHECK(theme.containers.railWidth < theme.containers.sidebarWidth);
        CHECK(theme.motion.popupClose < theme.motion.popupOpen);
        CHECK(theme.motion.popupScaleFrom < 1.0f);
        CHECK(theme.drag.ghostOpacity > 0.0f);
        CHECK(theme.drag.ghostOpacity < 1.0f);
    }
}

TEST_CASE("SetThemeMode selects the presets and keeps a custom accent")
{
    ThemeGuard guard;
    facet::SetThemeMode(facet::ThemeMode::Dark, false);
    CHECK(facet::GetThemeMode() == facet::ThemeMode::Dark);
    CHECK(facet::T().dark);

    const ImVec4 purple = facet::AccentPresets()[1].color;
    facet::SetAccent(purple);
    facet::SetThemeMode(facet::ThemeMode::Light, false);
    CHECK_FALSE(facet::T().dark);
    CHECK(facet::ToU32(facet::T().colors.accent) == facet::ToU32(purple));

    facet::SetThemeMode(facet::ThemeMode::System, false);
    CHECK(facet::T().dark == facet::SystemPrefersDarkTheme());

    // SetTheme replaces the accent with the one of the theme passed.
    facet::SetTheme(facet::Theme::Light(), false);
    CHECK(facet::ToU32(facet::T().colors.accent) == facet::ToU32(facet::Theme::Light().colors.accent));
}

TEST_CASE("SetThemeMode swaps the colors and keeps the other groups of the current theme")
{
    ThemeGuard guard;
    facet::Theme custom = facet::Theme::Light();
    custom.motion.scale = 4.0f;
    custom.motion.reduceMotion = true;
    custom.radii.continuousCorners = false;
    custom.spacing.md = 14.0f;
    facet::SetTheme(custom, false);

    facet::SetThemeMode(facet::ThemeMode::Dark, false);
    CHECK(facet::T().dark);
    CHECK(SameColors(facet::T().colors, facet::Theme::Dark().colors));
    CHECK(facet::T().elevation.Level(2).opacity == facet::Theme::Dark().elevation.Level(2).opacity);
    CHECK(facet::T().motion.scale == 4.0f);
    CHECK(facet::T().motion.reduceMotion);
    CHECK_FALSE(facet::T().radii.continuousCorners);
    CHECK(facet::T().spacing.md == 14.0f);

    facet::SetThemeMode(facet::ThemeMode::Light, false);
    CHECK_FALSE(facet::T().dark);
    CHECK(SameColors(facet::T().colors, facet::Theme::Light().colors));
    CHECK(facet::T().motion.scale == 4.0f);
}

TEST_CASE("ApplyImGuiStyle maps tokens and scale onto a Dear ImGui style")
{
    ThemeGuard guard;
    const float savedUi = facet::UiScale();
    const float savedDpi = facet::DpiScale();
    const facet::ScaleMode savedMode = facet::GetScaleMode();
    facet::SetScaleMode(facet::ScaleMode::FollowDpi);
    facet::SetDpiScale(1.5f);
    facet::SetUiScale(2.0f);
    facet::SetTheme(facet::Theme::Dark(), false);

    ImGuiStyle style;
    style.Alpha = 0.75f;
    style.ButtonTextAlign = ImVec2(0.0f, 1.0f);
    style.SelectableTextAlign = ImVec2(1.0f, 0.5f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ColorButtonPosition = ImGuiDir_Left;
    style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesFull;
    facet::ApplyImGuiStyle(&style);
    const facet::Theme& theme = facet::T();
    CHECK(style.Colors[ImGuiCol_WindowBg].x == theme.colors.bg.x);
    CHECK(style.Colors[ImGuiCol_Text].x == theme.colors.label.x);
    CHECK(style.Colors[ImGuiCol_TextLink].z == theme.colors.accentText.z);
    CHECK(style.FrameRounding == doctest::Approx(theme.radii.control * 3.0f));
    CHECK(style.ItemSpacing.x == doctest::Approx(theme.spacing.sm * 3.0f));
    CHECK(style.FrameBorderSize == doctest::Approx(facet::HairlineThickness()));
    CHECK(style.FontSizeBase == theme.type[facet::TextStyle::Body].size);
    CHECK(style.FontScaleMain * style.FontScaleDpi == doctest::Approx(facet::Scale()));
    CHECK(style.FontScaleDpi == doctest::Approx(1.5f));
    // Settings that are not sizes and have no token are kept.
    CHECK(style.Alpha == 0.75f);
    CHECK(style.ButtonTextAlign.x == 0.0f);
    CHECK(style.ButtonTextAlign.y == 1.0f);
    CHECK(style.SelectableTextAlign.x == 1.0f);
    CHECK(style.WindowTitleAlign.x == 0.5f);
    CHECK(style.WindowMenuButtonPosition == ImGuiDir_Right);
    CHECK(style.ColorButtonPosition == ImGuiDir_Left);
    CHECK(style.TreeLinesFlags == ImGuiTreeNodeFlags_DrawLinesFull);
    // The frame height of a stock control is the control height token.
    const float frameHeight = theme.type[facet::TextStyle::Body].size * 3.0f + style.FramePadding.y * 2.0f;
    CHECK(frameHeight == doctest::Approx(theme.metrics.controlHeight * 3.0f).epsilon(0.05));

    // Applying twice gives the same sizes (no compounding).
    const float rounding = style.FrameRounding;
    facet::ApplyImGuiStyle(&style);
    CHECK(style.FrameRounding == rounding);

    facet::SetScaleMode(savedMode);
    facet::SetDpiScale(savedDpi);
    facet::SetUiScale(savedUi);
}
