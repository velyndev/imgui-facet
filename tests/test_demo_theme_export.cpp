// The theme editor export of the test app (demo/theme_export.h): the C++ text names every
// token that differs from the preset and none that does not.

#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

#include "../demo/theme_export.h"

namespace
{

using facet::demo::BuildThemeExport;
using facet::demo::kThemeEditTokenCount;
using facet::demo::ThemeEditGroup;
using facet::demo::ThemeEdits;
using facet::demo::ThemeEditTokens;

// The assignment lines of an export, "theme.<path> = <value>;" without the leading "theme.".
std::vector<std::string> AssignedPaths(const std::string& text)
{
    std::vector<std::string> paths;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        const std::string prefix = "    theme.";
        if (line.rfind(prefix, 0) != 0)
            continue;
        const std::size_t equals = line.find(" = ");
        if (equals == std::string::npos)
            continue;
        paths.push_back(line.substr(prefix.size(), equals - prefix.size()));
    }
    return paths;
}

bool Contains(const std::vector<std::string>& paths, const std::string& path)
{
    for (const std::string& p : paths)
        if (p == path)
            return true;
    return false;
}

} // namespace

TEST_CASE("Theme export of an unedited theme sets no token")
{
    const facet::Theme base = facet::Theme::Dark();
    const std::string text = BuildThemeExport(base, ThemeEdits{}, "Dark", 1.0f);
    CHECK(AssignedPaths(text).empty());
    CHECK(text.find("ApplyAccent") == std::string::npos);
    CHECK(text.find("SetUiScale") == std::string::npos);
    CHECK(text.find("inline void ApplyThemeEdits(facet::Theme& theme)") != std::string::npos);
}

TEST_CASE("Theme export names every changed token and none that is unchanged")
{
    const facet::Theme base = facet::Theme::Light();
    ThemeEdits edits;
    edits.factor[static_cast<int>(ThemeEditGroup::Radii)] = 1.5f;     // every radius
    edits.factor[static_cast<int>(ThemeEditGroup::Shadow)] = 0.5f;    // every shadow level
    edits.factor[static_cast<int>(ThemeEditGroup::Motion)] = 2.0f;    // motion.scale
    // One spacing token on its own; the rest of the group stays.
    int spacingMd = -1;
    for (int i = 0; i < kThemeEditTokenCount; ++i)
        if (std::string(ThemeEditTokens()[static_cast<std::size_t>(i)].path) == "spacing.md")
            spacingMd = i;
    REQUIRE(spacingMd >= 0);
    edits.overridden[static_cast<std::size_t>(spacingMd)] = true;
    edits.value[static_cast<std::size_t>(spacingMd)] = 14.0f;
    // A token set by hand to its preset value is not a change.
    int body = -1;
    for (int i = 0; i < kThemeEditTokenCount; ++i)
        if (std::string(ThemeEditTokens()[static_cast<std::size_t>(i)].path) == "type.styles[5].size")
            body = i;
    REQUIRE(body >= 0);
    edits.overridden[static_cast<std::size_t>(body)] = true;
    edits.value[static_cast<std::size_t>(body)] = base.type.styles[5].size;
    edits.reduceMotion = true;
    edits.themeDefaultAccent = false;
    edits.accent = ImVec4(0.2f, 0.6f, 0.4f, 1.0f);

    const std::string text = BuildThemeExport(base, edits, "Light", 1.25f);
    const std::vector<std::string> paths = AssignedPaths(text);

    // Every token whose edited value differs from the preset is named, the others are not.
    const facet::Theme edited = facet::demo::ApplyThemeEdits(base, edits);
    for (const auto& token : ThemeEditTokens())
    {
        facet::Theme a = base;
        facet::Theme b = edited;
        const bool changed = facet::demo::ThemeEditValuesDiffer(token.ref(a), token.ref(b));
        CAPTURE(token.path);
        CHECK(Contains(paths, token.path) == changed);
    }
    CHECK(Contains(paths, "radii.card"));
    CHECK(Contains(paths, "elevation.levels[2].opacity"));
    CHECK(Contains(paths, "spacing.md"));
    CHECK_FALSE(Contains(paths, "spacing.sm"));
    CHECK_FALSE(Contains(paths, "type.styles[5].size"));
    CHECK_FALSE(Contains(paths, "metrics.controlHeight"));
    CHECK(Contains(paths, "motion.reduceMotion"));
    CHECK(text.find("theme.ApplyAccent(ImVec4(0.2f, 0.6f, 0.4f, 1.0f));") != std::string::npos);
    CHECK(text.find("facet::SetUiScale(1.25f)") != std::string::npos);
    CHECK(text.find("theme.spacing.md = 14.0f;") != std::string::npos);
    CHECK(text.find("facet::Theme::Light()") != std::string::npos);
}

TEST_CASE("A group factor never pushes a token past the range of its row, and an unedited preset stays")
{
    for (const bool dark : {false, true})
    {
        CAPTURE(dark);
        const facet::Theme base = dark ? facet::Theme::Dark() : facet::Theme::Light();
        // Unedited, no token is clamped away from its preset value.
        CHECK(AssignedPaths(BuildThemeExport(base, ThemeEdits{}, "Preset", 1.0f)).empty());
        ThemeEdits edits;
        for (int g = 0; g < facet::demo::kThemeEditGroupCount; ++g)
            edits.factor[static_cast<std::size_t>(g)] = facet::demo::ThemeEditGroupOf(static_cast<ThemeEditGroup>(g)).maxFactor;
        const facet::Theme edited = facet::demo::ApplyThemeEdits(base, edits);
        for (const auto& token : ThemeEditTokens())
        {
            CAPTURE(token.path);
            facet::Theme copy = edited;
            CHECK(token.ref(copy) <= token.maxValue);
            CHECK(token.ref(copy) >= token.minValue);
        }
        CHECK(edited.elevation.levels[2].opacity <= 1.0f);
        CHECK(BuildThemeExport(base, edits, "Preset", 1.0f).find("opacity = 1.1") == std::string::npos);
    }
}

TEST_CASE("Theme edits apply factors and per-token values to the preset")
{
    const facet::Theme base = facet::Theme::Dark();
    ThemeEdits edits;
    edits.factor[static_cast<int>(ThemeEditGroup::Heights)] = 1.25f;
    edits.overridden[0] = true;  // radii.element
    edits.value[0] = 3.0f;
    const facet::Theme edited = facet::demo::ApplyThemeEdits(base, edits);
    CHECK(edited.metrics.controlHeight == doctest::Approx(base.metrics.controlHeight * 1.25f));
    CHECK(edited.radii.element == doctest::Approx(3.0f));
    CHECK(edited.radii.card == doctest::Approx(base.radii.card));
    CHECK(edited.dark);
    CHECK(edited.colors.accent.x == doctest::Approx(base.colors.accent.x));
}
