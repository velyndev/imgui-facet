// Behaviour of the search, preset and app control composites (SearchFilterBar, SettingsSearch,
// KeybindList, PresetList, ThemeModeControl, AccentPicker, UiScaleControl, Onboarding,
// ConsoleView).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_composites_settings.cpp. A test that changes the theme mode, the accent or the UI
// scale restores them before it ends.

#include <cmath>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/composites/keybind_list.h"

namespace
{

// Puts the theme, the accent and the UI scale back the way a session starts.
struct ThemeRestore
{
    ~ThemeRestore()
    {
        facet::ClearAccent();
        facet::SetThemeMode(facet::ThemeMode::Light, false);
        facet::SetUiScale(1.0f);
    }
};

// The center of segment `index` of `count` equal segments filling `rect`.
ImVec2 SegmentCenter(const ImRect& rect, int index, int count)
{
    const float w = rect.GetWidth() / static_cast<float>(count);
    return ImVec2(rect.Min.x + w * (static_cast<float>(index) + 0.5f), rect.GetCenter().y);
}

} // namespace

// ---- ThemeModeControl ----------------------------------------------------------------------

TEST_CASE("ThemeModeControl writes the clicked mode once and applies it with SetThemeMode")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    facet::ThemeMode mode = facet::ThemeMode::Light;
    int writes = 0;
    session.SetUi([&] {
        if (facet::ThemeModeControl("##theme", &mode, {.width = 300.0f}))
            ++writes;
        session.Track("theme");
    });
    session.Frame();
    session.Click(SegmentCenter(session.ItemRect("theme"), 1, 3));
    CHECK(mode == facet::ThemeMode::Dark);
    CHECK(writes == 1);
    CHECK(facet::GetThemeMode() == facet::ThemeMode::Dark);
    // A click on the chosen segment writes nothing more.
    session.Click(SegmentCenter(session.ItemRect("theme"), 1, 3));
    CHECK(writes == 1);
}

TEST_CASE("ThemeModeControl moves with the arrow keys and is one Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    facet::ThemeMode mode = facet::ThemeMode::Light;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ThemeModeControl("##theme", &mode, {.apply = false});
        session.Track("theme");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(mode == facet::ThemeMode::System);
    // apply off: the core mode is left alone.
    CHECK(facet::GetThemeMode() == facet::ThemeMode::Light);
}

TEST_CASE("ThemeModeControl read-only, disabled or without a value writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    facet::ThemeMode readOnly = facet::ThemeMode::Light;
    facet::ThemeMode disabled = facet::ThemeMode::Light;
    bool anyWrite = false;
    session.SetUi([&] {
        anyWrite |= facet::ThemeModeControl("##ro", &readOnly, {.width = 300.0f, .readOnly = true});
        session.Track("ro");
        ImGui::BeginDisabled();
        anyWrite |= facet::ThemeModeControl("##off", &disabled, {.width = 300.0f});
        session.Track("off");
        ImGui::EndDisabled();
        anyWrite |= facet::ThemeModeControl("##null", nullptr, {.width = 300.0f});
        session.Track("null");
    });
    session.Frame();
    session.Click(SegmentCenter(session.ItemRect("ro"), 2, 3));
    session.Click(SegmentCenter(session.ItemRect("off"), 2, 3));
    session.Click(SegmentCenter(session.ItemRect("null"), 2, 3));
    CHECK_FALSE(anyWrite);
    CHECK(readOnly == facet::ThemeMode::Light);
    CHECK(disabled == facet::ThemeMode::Light);
    CHECK(facet::GetThemeMode() == facet::ThemeMode::Light);
}

// ---- AccentPicker --------------------------------------------------------------------------

TEST_CASE("Every offered accent keeps AA contrast for text on it and for the focus ring in both themes")
{
    for (const bool dark : {false, true})
    {
        for (const facet::AccentPreset& preset : facet::AccentPresets())
        {
            INFO((dark ? "dark " : "light ") << preset.name);
            facet::Theme theme = dark ? facet::Theme::Dark() : facet::Theme::Light();
            theme.ApplyAccent(preset.color);
            const facet::Colors& c = theme.colors;
            CHECK(facet::ContrastRatio(c.onAccent, c.accent) >= 4.5f);
            CHECK(facet::ContrastRatio(c.onAccent, c.accentHover) >= 4.5f);
            CHECK(facet::ContrastRatio(c.onAccent, c.accentPressed) >= 4.5f);
            CHECK(facet::ContrastRatio(c.focusRing, c.bg) >= 3.0f);
            CHECK(facet::ContrastRatio(c.accentText, c.bg) >= 4.5f);
        }
    }
}

TEST_CASE("AccentPicker applies a preset, the theme default and a custom color through the core")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    bool themeDefault = true;
    ImVec4 accent(0.0f, 0.0f, 0.0f, 1.0f);
    int writes = 0;
    session.SetUi([&] {
        if (facet::AccentPicker("##accent", &themeDefault, &accent, {.customColor = false}))
            ++writes;
        session.Track("accent");
    });
    session.Frame();
    const float side = std::round(facet::ControlHeight(facet::ControlSize::Inherit));
    const float gap = std::round(facet::Px(facet::T().spacing.xs));
    const ImRect rect = session.ItemRect("accent");
    const auto swatch = [&](int i) {
        return ImVec2(rect.Min.x + static_cast<float>(i) * (side + gap) + side * 0.5f, rect.Min.y + side * 0.5f);
    };
    // The third preset.
    session.Click(swatch(2));
    const ImVec4 third = facet::AccentPresets()[2].color;
    CHECK(writes == 1);
    CHECK_FALSE(themeDefault);
    CHECK(accent.x == doctest::Approx(third.x));
    CHECK(accent.y == doctest::Approx(third.y));
    CHECK(accent.z == doctest::Approx(third.z));
    CHECK(facet::HasCustomAccent());
    CHECK(facet::ToU32(facet::T().colors.accent) == facet::ToU32(third));
    // Back to the theme default: the core forgets the accent, *accent stays as it was.
    session.Click(swatch(0));
    CHECK(writes == 2);
    CHECK(themeDefault);
    CHECK(accent.x == doctest::Approx(third.x));
    CHECK_FALSE(facet::HasCustomAccent());
    // A custom color goes through the same derivation as a preset.
    facet::Theme expected = facet::T();
    const ImVec4 custom(0.9f, 0.5f, 0.1f, 1.0f);
    expected.ApplyAccent(custom);
    facet::SetAccent(custom);
    CHECK(facet::ToU32(facet::T().colors.onAccent) == facet::ToU32(expected.colors.onAccent));
    CHECK(facet::ToU32(facet::T().colors.focusRing) == facet::ToU32(expected.colors.focusRing));
}

TEST_CASE("AccentPicker moves with the arrow keys and chooses with Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    bool themeDefault = true;
    ImVec4 accent(0.0f, 0.0f, 0.0f, 1.0f);
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::AccentPicker("##accent", &themeDefault, &accent, {.apply = false});
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK_FALSE(themeDefault);
    CHECK(facet::ToU32(accent) == facet::ToU32(facet::AccentPresets()[1].color));
    // apply off leaves the core accent alone.
    CHECK_FALSE(facet::HasCustomAccent());
    session.KeyPress(ImGuiKey_Home);
    session.KeyPress(ImGuiKey_Space);
    CHECK(themeDefault);
}

TEST_CASE("AccentPicker without a theme default treats the first preset as a color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    ImVec4 accent = facet::AccentPresets()[3].color;
    bool wrote = false;
    session.SetUi([&] {
        wrote |= facet::AccentPicker("##accent", nullptr, &accent, {.customColor = false, .apply = false});
        session.Track("accent");
    });
    session.Frame();
    const float side = std::round(facet::ControlHeight(facet::ControlSize::Inherit));
    const ImRect rect = session.ItemRect("accent");
    session.Click(ImVec2(rect.Min.x + side * 0.5f, rect.Min.y + side * 0.5f));
    CHECK(wrote);
    CHECK(facet::ToU32(accent) == facet::ToU32(facet::AccentPresets()[0].color));
}

TEST_CASE("AccentPicker read-only or without an accent writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    bool themeDefault = true;
    ImVec4 accent(0.0f, 0.0f, 0.0f, 1.0f);
    bool wrote = false;
    session.SetUi([&] {
        wrote |= facet::AccentPicker("##ro", &themeDefault, &accent, {.readOnly = true});
        session.Track("ro");
        wrote |= facet::AccentPicker("##null", &themeDefault, nullptr);
        session.Track("null");
    });
    session.Frame();
    const float side = std::round(facet::ControlHeight(facet::ControlSize::Inherit));
    for (const char* name : {"ro", "null"})
    {
        const ImRect rect = session.ItemRect(name);
        session.Click(ImVec2(rect.Min.x + side * 2.5f, rect.Min.y + side * 0.5f));
    }
    CHECK_FALSE(wrote);
    CHECK(themeDefault);
    CHECK_FALSE(facet::HasCustomAccent());
}

// ---- UiScaleControl ------------------------------------------------------------------------

TEST_CASE("UiScaleControl steps with its buttons and applies each step at once")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    float scale = 1.0f;
    int writes = 0;
    session.SetUi([&] {
        if (facet::UiScaleControl("##scale", &scale, 0.75f, 2.0f, {.width = 400.0f}))
            ++writes;
        session.Track("scale");
    });
    session.Frame();
    const auto zoomIn = [&] {
        const ImRect rect = session.ItemRect("scale");
        const float button = std::round(facet::ControlHeight(facet::ControlSize::Inherit));
        return ImVec2(rect.Max.x - button * 0.5f, rect.GetCenter().y);
    };
    session.Click(zoomIn());
    CHECK(scale == doctest::Approx(1.25f));
    CHECK(writes == 1);
    CHECK(facet::UiScale() == doctest::Approx(1.25f));
    session.Click(zoomIn());
    session.Click(zoomIn());
    session.Click(zoomIn());
    session.Click(zoomIn());
    // The maximum holds.
    CHECK(scale == doctest::Approx(2.0f));
    CHECK(facet::UiScale() == doctest::Approx(2.0f));
}

TEST_CASE("UiScaleControl applies a slider drag once, on release")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    float scale = 1.0f;
    session.SetUi([&] {
        facet::UiScaleControl("##scale", &scale, 0.75f, 2.0f, {.width = 400.0f});
        session.Track("scale");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("scale");
    session.MouseMove(ImVec2(rect.Min.x + 2.0f, rect.GetCenter().y));
    session.MouseDown();
    session.MouseMove(ImVec2(rect.Min.x + rect.GetWidth() * 0.6f, rect.GetCenter().y));
    session.MouseMove(ImVec2(rect.Min.x + rect.GetWidth() * 0.7f, rect.GetCenter().y));
    CHECK(scale > 1.0f);
    // Still held: the scale is not applied yet, so the track stays under the pointer.
    CHECK(facet::UiScale() == doctest::Approx(1.0f));
    session.MouseUp();
    CHECK(facet::UiScale() == doctest::Approx(scale));
}

TEST_CASE("UiScaleControl takes the zoom shortcuts and Ctrl+wheel, and read-only turns them off")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    const facet::ShortcutId in = facet::RegisterShortcut("test.ui scale.in", ImGuiMod_Ctrl | ImGuiKey_Equal);
    const facet::ShortcutId out = facet::RegisterShortcut("test.ui scale.out", ImGuiMod_Ctrl | ImGuiKey_Minus);
    const facet::ShortcutId reset = facet::RegisterShortcut("test.ui scale.reset", ImGuiMod_Ctrl | ImGuiKey_0);
    float scale = 1.0f;
    bool readOnly = false;
    session.SetUi([&] {
        facet::UiScaleControl("##scale", &scale, 0.5f, 2.0f,
                              {.zoomInShortcut = in, .zoomOutShortcut = out, .zoomResetShortcut = reset,
                               .wheelZoom = true, .apply = false, .readOnly = readOnly});
    });
    session.Frames(2);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Equal);
    CHECK(scale == doctest::Approx(1.25f));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Minus);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Minus);
    CHECK(scale == doctest::Approx(0.75f));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_0);
    CHECK(scale == doctest::Approx(1.0f));
    session.KeyDown(ImGuiMod_Ctrl);
    session.Wheel(ImVec2(400.0f, 300.0f), 1.0f);
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(scale == doctest::Approx(1.25f));
    // apply off: the core scale stays.
    CHECK(facet::UiScale() == doctest::Approx(1.0f));
    readOnly = true;
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_Equal);
    CHECK(scale == doctest::Approx(1.25f));
    facet::UnregisterShortcut(in);
    facet::UnregisterShortcut(out);
    facet::UnregisterShortcut(reset);
}

TEST_CASE("UiScaleControl swaps min and max and moves to the ends with Home and End")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeRestore restore;
    float scale = 1.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::UiScaleControl("##scale", &scale, 2.0f, 0.5f, {.apply = false});
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_End);
    CHECK(scale == doctest::Approx(2.0f));
    session.KeyPress(ImGuiKey_Home);
    CHECK(scale == doctest::Approx(0.5f));
}

// ---- SearchFilterBar -----------------------------------------------------------------------

TEST_CASE("SearchFilterBar writes the query per keystroke and Escape restores it from activation")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query = "a";
    facet::FilterChip chips[] = {{"Filter 1"}, {"Filter 2"}};
    int writes = 0;
    session.SetUi([&] {
        if (facet::SearchFilterBar("##bar", &query, chips, {.resultCount = 2}))
            ++writes;
        session.Track("bar");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("bar");
    session.Click(ImVec2(rect.Min.x + facet::Px(40.0f), rect.Min.y + facet::ControlHeight(facet::ControlSize::Inherit) * 0.5f));
    session.KeyPress(ImGuiKey_End);
    session.Type("b");
    CHECK(query == "ab");
    session.Type("c");
    CHECK(query == "abc");
    CHECK(writes == 2);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(query == "a");
    CHECK(writes == 3);
    CHECK_FALSE(chips[0].active);
    CHECK_FALSE(chips[1].active);
}

TEST_CASE("SearchFilterBar chips toggle their active flag by mouse and keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    facet::FilterChip chips[] = {{"Filter 1"}, {"Filter 2"}};
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::SearchFilterBar("##bar", &query, chips, {.width = 600.0f}))
            ++writes;
        session.Track("bar");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("bar");
    // Wide enough: one line, the chips at the end.
    CHECK(rect.GetHeight() < facet::ControlHeight(facet::ControlSize::Inherit) * 1.5f);
    session.Click(ImVec2(rect.Max.x - facet::Px(20.0f), rect.GetCenter().y));
    CHECK(chips[1].active);
    CHECK_FALSE(chips[0].active);
    CHECK(writes == 1);
    // Tab goes from the field to the first chip, and Space toggles it.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK(chips[0].active);
    CHECK(writes == 2);
    CHECK(query.empty());
}

TEST_CASE("A narrow SearchFilterBar puts the chips under the field")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    facet::FilterChip chips[] = {{"Filter 1"}, {"Filter 2"}, {"Filter 3"}};
    session.SetUi([&] {
        facet::SearchFilterBar("##bar", &query, chips, {.width = 220.0f});
        session.Track("bar");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("bar");
    const float row = facet::ControlHeight(facet::ControlSize::Inherit);
    CHECK(rect.GetHeight() > row * 1.5f);
    CHECK(rect.GetWidth() == doctest::Approx(facet::Px(220.0f)).epsilon(0.01));
    // The first chip starts the second line.
    session.Click(ImVec2(rect.Min.x + facet::Px(16.0f), rect.Max.y - row * 0.5f));
    CHECK(chips[0].active);
}

TEST_CASE("SearchFilterBar read-only or disabled writes neither the query nor the chips")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char buffer[32] = "abc";
    facet::FilterChip chips[] = {{"Filter 1"}};
    bool readOnly = true;
    bool wrote = false;
    session.SetUi([&] {
        ImGui::BeginDisabled(!readOnly);
        wrote |= facet::SearchFilterBar("##bar", buffer, sizeof(buffer), chips, {.width = 500.0f, .readOnly = readOnly});
        ImGui::EndDisabled();
        session.Track("bar");
    });
    session.Frame();
    for (int pass = 0; pass < 2; ++pass)
    {
        const ImRect rect = session.ItemRect("bar");
        session.Click(ImVec2(rect.Max.x - facet::Px(20.0f), rect.GetCenter().y));
        session.Click(ImVec2(rect.Min.x + facet::Px(40.0f), rect.GetCenter().y));
        session.Type("x");
        readOnly = false;  // the second pass: disabled
        session.Frame();
    }
    CHECK_FALSE(wrote);
    CHECK(std::string(buffer) == "abc");
    CHECK_FALSE(chips[0].active);
}

// ---- SettingsSearch ------------------------------------------------------------------------

namespace
{

// Rows of a settings search test: label, keywords and section.
struct SearchRow
{
    const char* label;
    const char* keywords;
    const char* section;
};

const SearchRow kSearchRows[] = {
    {"Vertical sync", "vsync tearing", "Display"},
    {"Resolution", "size pixels", "Display"},
    {"Master volume", "sound level", "Audio"},
    {"Mute when inactive", "sound focus", "Audio"},
    {"Language", "locale", "General"},
};

// Submits the rows through the search and returns how many were let through.
int SubmitSearchRows(const char* search, const char* filter, const facet::FilterSettings* settings = nullptr)
{
    int shown = 0;
    facet::BeginSettingsSearch(search, {.filter = filter, .filterSettings = settings});
    for (const SearchRow& row : kSearchRows)
    {
        if (facet::SettingsSearchMatch(row.label, {.keywords = row.keywords, .section = row.section}))
        {
            ImGui::TextUnformatted(row.label);
            ++shown;
        }
    }
    facet::EndSettingsSearch();
    return shown;
}

} // namespace

TEST_CASE("SettingsSearch lets matching rows through and counts the hits per section of the last frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const char* filter = "";
    int shown = 0;
    session.SetUi([&] { shown = SubmitSearchRows("test settings", filter); });
    session.Frame();
    // An empty filter lets every row through, and every row counts.
    CHECK(shown == 5);
    CHECK(facet::SettingsSearchHitCount("test settings", nullptr) == 5);
    CHECK(facet::SettingsSearchHitCount("test settings", "Display") == 2);
    filter = "sound";
    session.Frame();
    CHECK(shown == 2);
    CHECK(facet::SettingsSearchHitCount("test settings", nullptr) == 2);
    CHECK(facet::SettingsSearchHitCount("test settings", "Audio") == 2);
    CHECK(facet::SettingsSearchHitCount("test settings", "Display") == 0);
    // Fuzzy: the characters in order, in the label, the description or the keywords.
    filter = "vsnc";
    session.Frame();
    CHECK(shown == 1);
    filter = "no such setting";
    session.Frame();
    CHECK(shown == 0);
    CHECK(facet::SettingsSearchHitCount("test settings", nullptr) == 0);
    // Unknown names and sections count 0.
    CHECK(facet::SettingsSearchHitCount("another search", nullptr) == 0);
    CHECK(facet::SettingsSearchHitCount("test settings", "Network") == 0);
}

TEST_CASE("SettingsSearch outside a search lets every row through and counts nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shown = false;
    session.SetUi([&] { shown = facet::SettingsSearchMatch("Row", {.section = "Section"}); });
    session.Frame();
    CHECK(shown);
    CHECK(facet::SettingsSearchHitCount("", "Section") == 0);
}

TEST_CASE("SettingsSearch with filter settings matches whole query words")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::FilterSettings whole{.wholeWord = true};
    const char* filter = "sync";
    int shown = 0;
    session.SetUi([&] { shown = SubmitSearchRows("test words", filter, &whole); });
    session.Frame();
    CHECK(shown == 1);
    filter = "syn";
    session.Frame();
    CHECK(shown == 0);
}

TEST_CASE("SettingsSearch scrolls the first hit into view and highlights it until the flash ends")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string filter;
    int highlight = 0;
    session.SetUi([&] {
        ImGui::BeginChild("rows", ImVec2(300.0f, 120.0f));
        facet::BeginSettingsSearch("test reveal", {.filter = filter.c_str()});
        // Content the search does not filter (headings, notes) keeps the hit far down.
        for (int i = 0; i < 40; ++i)
            ImGui::TextUnformatted("Always shown");
        char label[32];
        for (int i = 0; i < 60; ++i)
        {
            std::snprintf(label, sizeof(label), "Setting %d", i);
            if (facet::SettingsSearchMatch(label))
            {
                ImGui::TextUnformatted(label);
                if (i == 50)
                    session.Track("row 50");
            }
        }
        facet::EndSettingsSearch();
        // The highlight is an accentSubtle fill in the window's draw list.
        const ImU32 fill = facet::ToU32(facet::T().colors.accentSubtle);
        highlight = 0;
        for (const ImDrawVert& v : ImGui::GetWindowDrawList()->VtxBuffer)
            highlight += (v.col & IM_COL32_A_MASK) != 0 && (v.col | IM_COL32_A_MASK) == (fill | IM_COL32_A_MASK) ? 1 : 0;
        ImGui::EndChild();
    });
    session.Frames(2);
    CHECK(highlight == 0);
    CHECK_FALSE(session.ItemVisible("row 50"));  // far below the visible part
    filter = "Setting 50";
    session.Frames(3);
    CHECK(session.ItemVisible("row 50"));
    CHECK(highlight > 0);
    // The flash holds for motion.feedbackHold, then fades.
    const facet::Motion& motion = facet::T().motion;
    session.Seconds((motion.feedbackHold + motion.durationSlow) * motion.scale + 0.2f);
    CHECK(highlight == 0);
}

TEST_CASE("Escape in the search field clears the query first, so every settings row shows again")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    int shown = 0;
    session.SetUi([&] {
        facet::SearchField("##search", &query);
        session.Track("search");
        shown = SubmitSearchRows("test escape", query.c_str());
    });
    session.Frame();
    session.Click(session.ItemCenter("search"));
    session.Type("mute");
    CHECK(shown == 1);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(query.empty());
    CHECK(shown == 5);
    CHECK(ImGui::GetActiveID() == session.ItemId("search"));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(ImGui::GetActiveID() == 0);
}

// ---- PresetList ----------------------------------------------------------------------------

namespace
{

// A preset list over three presets (the first built in) that records the last action.
struct PresetHarness
{
    facet::PresetItem presets[3] = {{"Preset 1", false, true}, {"Preset 2", true}, {"Preset 3"}};
    int active = 0;
    facet::PresetListResult last;
    std::string lastName;
    int actions = 0;
    facet::PresetListOptions options;

    void Run(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        const facet::PresetListResult result = facet::PresetList("presets", &active, presets, options);
        session.Track("presets");
        if (result.action != facet::PresetAction::None)
        {
            last = result;
            lastName = result.name != nullptr ? result.name : "";
            ++actions;
        }
    }

    // The center of row `index` of the list in the latest frame.
    static ImVec2 Row(facet::test::Session& session, int index)
    {
        const ImRect rect = session.ItemRect("presets");
        const float row = facet::internal::ListRowHeight(facet::ControlSize::Inherit);
        return ImVec2(rect.Min.x + facet::Px(40.0f), rect.Min.y + row * (static_cast<float>(index) + 0.5f));
    }
};

} // namespace

TEST_CASE("PresetList chooses a preset by click and by Enter, and the arrows only move the focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PresetHarness h;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(PresetHarness::Row(session, 2));
    CHECK(h.active == 2);
    CHECK(h.last.action == facet::PresetAction::Selected);
    CHECK(h.last.index == 2);
    CHECK(h.actions == 1);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(h.active == 2);
    CHECK(h.actions == 1);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(h.active == 1);
    CHECK(h.actions == 2);
}

TEST_CASE("PresetList renames with F2: Enter commits the name, Escape cancels, a taken name gets a number")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PresetHarness h;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(PresetHarness::Row(session, 1));
    session.KeyPress(ImGuiKey_F2);
    session.Frames(2);
    session.Type("Renamed");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(h.last.action == facet::PresetAction::Rename);
    CHECK(h.last.index == 1);
    CHECK(h.lastName == "Renamed");
    const int actions = h.actions;
    // Escape drops the edit.
    session.KeyPress(ImGuiKey_F2);
    session.Frames(2);
    session.Type("Dropped");
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(h.actions == actions);
    // A name another preset has gets the first free number.
    session.KeyPress(ImGuiKey_F2);
    session.Frames(2);
    session.Type("preset 3");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(h.last.action == facet::PresetAction::Rename);
    CHECK(h.lastName == "preset 3 2");
    // A built-in preset is never renamed.
    session.Click(PresetHarness::Row(session, 0));
    const int before = h.actions;
    session.KeyPress(ImGuiKey_F2);
    session.Frames(2);
    session.Type("x");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(h.actions == before);
    CHECK(h.active == 0);
}

TEST_CASE("PresetList asks before a delete: confirming reports it, declining does nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PresetHarness h;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(PresetHarness::Row(session, 2));
    const int afterClick = h.actions;
    session.KeyPress(ImGuiKey_Delete);
    session.Frames(10);
    // The dialog is destructive: the focus starts on Cancel and Escape declines.
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(10);
    CHECK(h.actions == afterClick);
    session.KeyPress(ImGuiKey_Delete);
    session.Frames(10);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(10);
    CHECK(h.last.action == facet::PresetAction::Delete);
    CHECK(h.last.index == 2);
    // A built-in preset is never offered for deletion: no dialog opens.
    session.Click(PresetHarness::Row(session, 0));
    session.KeyPress(ImGuiKey_Delete);
    session.Frames(10);
    CHECK(facet::internal::TopPopupLevel() == 0);
}

TEST_CASE("PresetList creates a preset from its menu with a free default name")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PresetHarness h;
    h.presets[2].label = facet::GetStrings().newPreset;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    // Tab leaves the rows for the menu button; Enter opens the menu with its first row focused, and
    // that row creates.
    session.Click(PresetHarness::Row(session, 0));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(10);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(h.last.action == facet::PresetAction::Create);
    CHECK(h.last.index == -1);
    CHECK(h.lastName == std::string(facet::GetStrings().newPreset) + " 2");
}

TEST_CASE("PresetList read-only or disabled writes nothing and offers no menu")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PresetHarness h;
    h.options.readOnly = true;
    bool disabled = false;
    session.SetUi([&] {
        ImGui::BeginDisabled(disabled);
        h.Run(session);
        ImGui::EndDisabled();
    });
    session.Frame();
    const float rows = facet::internal::ListRowHeight(facet::ControlSize::Inherit) * 3.0f;
    CHECK(session.ItemRect("presets").GetHeight() == doctest::Approx(rows).epsilon(0.02));
    session.Click(PresetHarness::Row(session, 2));
    session.KeyPress(ImGuiKey_Delete);
    session.KeyPress(ImGuiKey_F2);
    session.Frames(3);
    CHECK(h.active == 0);
    CHECK(h.actions == 0);
    h.options.readOnly = false;
    disabled = true;
    session.Frame();
    session.Click(PresetHarness::Row(session, 1));
    CHECK(h.active == 0);
    CHECK(h.actions == 0);
}

TEST_CASE("PresetList shows its loading, error and empty states without taking a choice")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    PresetHarness h;
    h.options.state = facet::DataState::Loading;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(PresetHarness::Row(session, 1));
    CHECK(h.actions == 0);
    h.options.state = facet::DataState::Error;
    session.Frame();
    CHECK(session.ItemRect("presets").GetHeight() > 0.0f);
    // An empty list keeps its height for the empty text.
    h.options.state = facet::DataState::Ready;
    session.SetUi([&] {
        const facet::PresetListResult r = facet::PresetList("empty", &h.active, std::span<const facet::PresetItem>());
        session.Track("empty");
        CHECK(r.action == facet::PresetAction::None);
    });
    session.Frame();
    CHECK(session.ItemRect("empty").GetHeight() >= facet::internal::ListRowHeight(facet::ControlSize::Inherit));
}

// ---- KeybindList ---------------------------------------------------------------------------

namespace
{

// Where the parts of a KeybindList without group headings are, from its item rect, by the list's
// own layout (widgets/composites/keybind_list.h); the lists here are wide enough to stand side by
// side.
struct KeybindLayout
{
    ImRect rect;
    int columns = 2;  // 1 without the alternate column

    // Column 0: primary, 1: alternate, 2: the reset button.
    ImVec2 At(int row, int column) const
    {
        const facet::internal::KeybindListLayout layout = facet::internal::KeybindListMeasure(rect.GetWidth(), columns == 2);
        const float top = rect.Min.y + layout.headingHeight + static_cast<float>(row) * (layout.rowHeight + layout.spacing);
        float x = rect.Min.x + layout.resetX + layout.button * 0.5f;
        if (column < 2)
            x = rect.Min.x + (column == 0 ? layout.primaryX : layout.alternateX) + layout.column * 0.5f;
        return ImVec2(x, top + layout.rowHeight * 0.5f);
    }
};

// Vertices drawn in the warning token: the conflict mark.
int WarningVertices()
{
    const ImU32 warning = facet::ToU32(facet::T().colors.warning);
    int count = 0;
    for (const ImDrawVert& v : ImGui::GetWindowDrawList()->VtxBuffer)
        count += v.col == warning ? 1 : 0;
    return count;
}

} // namespace

TEST_CASE("KeybindList captures a chord, and Escape while it listens cancels")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::KeybindListItem items[] = {
        {"Action 1", nullptr, {ImGuiMod_Ctrl | ImGuiKey_1}, {ImGuiMod_Ctrl | ImGuiKey_1}},
        {"Action 2", nullptr, {ImGuiMod_Ctrl | ImGuiKey_2}, {ImGuiMod_Ctrl | ImGuiKey_2}},
    };
    int writes = 0;
    session.SetUi([&] {
        if (facet::KeybindList("binds", items, {.groups = false, .width = 600.0f}))
            ++writes;
        session.Track("binds");
    });
    session.Frame();
    const KeybindLayout layout{session.ItemRect("binds")};
    session.Click(layout.At(1, 0));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_K);
    session.Frames(2);
    CHECK(items[1].bind.primary == (ImGuiMod_Ctrl | ImGuiKey_K));
    CHECK(writes == 1);
    // Escape while listening writes nothing.
    session.Click(layout.At(0, 0));
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(items[0].bind.primary == (ImGuiMod_Ctrl | ImGuiKey_1));
    CHECK(writes == 1);
}

TEST_CASE("KeybindList marks a conflict between two actions, clears a chord and resets a row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::KeybindListItem items[] = {
        {"Action 1", nullptr, {ImGuiMod_Ctrl | ImGuiKey_1}, {ImGuiMod_Ctrl | ImGuiKey_1}},
        {"Action 2", nullptr, {ImGuiMod_Ctrl | ImGuiKey_1}, {ImGuiMod_Ctrl | ImGuiKey_2}},
    };
    int warnings = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::KeybindList("binds", items, {.alternate = false, .groups = false, .width = 600.0f});
        session.Track("binds");
        warnings = WarningVertices();
    });
    session.Frame();
    CHECK(warnings > 0);
    // Backspace on the focused field clears the chord; the conflict goes away.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Backspace);
    session.Frames(2);
    CHECK(items[0].bind.primary == ImGuiKey_None);
    CHECK(warnings == 0);
    // Both rows now differ from their default; the reset button of the second restores it.
    const KeybindLayout layout{session.ItemRect("binds"), 1};
    session.Click(layout.At(1, 2));
    session.Frames(2);
    CHECK(items[1].bind.primary == (ImGuiMod_Ctrl | ImGuiKey_2));
    CHECK(items[0].bind.primary == ImGuiKey_None);
}

TEST_CASE("KeybindList over the shortcut registry writes and resets the registered chords")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId first = facet::RegisterShortcut("test.keybind list.first", ImGuiMod_Ctrl | ImGuiKey_F7,
                                                             {.label = "First test action"});
    const facet::ShortcutId second = facet::RegisterShortcut("test.keybind list.second", ImGuiMod_Ctrl | ImGuiKey_F8,
                                                              {.label = "Second test action"});
    session.SetUi([&] {
        facet::KeybindList("registry", {.filter = "test action", .filterSettings = nullptr, .alternate = false,
                                        .groups = false, .width = 600.0f});
        session.Track("registry");
    });
    session.Frame();
    const KeybindLayout layout{session.ItemRect("registry"), 1};
    session.Click(layout.At(1, 0));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_F9);
    session.Frames(2);
    facet::ShortcutInfo info;
    REQUIRE(facet::GetShortcut(second, &info));
    CHECK(info.primary == (ImGuiMod_Ctrl | ImGuiKey_F9));
    session.Click(layout.At(1, 2));
    session.Frames(2);
    REQUIRE(facet::GetShortcut(second, &info));
    CHECK(info.primary == (ImGuiMod_Ctrl | ImGuiKey_F8));
    facet::UnregisterShortcut(first);
    facet::UnregisterShortcut(second);
}

TEST_CASE("KeybindList filters rows, shows no results, and read-only never listens")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::KeybindListItem items[] = {
        {"Action 1", "Group 1", {ImGuiKey_A}, {ImGuiKey_A}},
        {"Action 2", "Group 1", {ImGuiKey_B}, {ImGuiKey_B}},
    };
    const char* filter = "Action 2";
    bool readOnly = false;
    bool wrote = false;
    session.SetUi([&] {
        wrote |= facet::KeybindList("binds", items, {.filter = filter, .groups = false, .width = 600.0f, .readOnly = readOnly});
        session.Track("binds");
    });
    session.Frame();
    const float row = facet::ControlHeight(facet::ControlSize::Inherit);
    const float one = session.ItemRect("binds").GetHeight();
    filter = "";
    session.Frame();
    CHECK(session.ItemRect("binds").GetHeight() > one + row * 0.5f);
    filter = "no such action";
    session.Frame();
    CHECK(session.ItemRect("binds").GetHeight() >= row * 2.0f);
    filter = "";
    readOnly = true;
    session.Frame();
    const KeybindLayout layout{session.ItemRect("binds")};
    session.Click(layout.At(0, 0));
    session.KeyPress(ImGuiKey_K);
    session.KeyPress(ImGuiKey_Backspace);
    session.Frames(2);
    CHECK_FALSE(wrote);
    CHECK(items[0].bind.primary == ImGuiKey_A);
}

// ---- ConsoleView ---------------------------------------------------------------------------

namespace
{

const facet::LogEntry kConsoleLog[] = {{"Line 1"}, {"Line 2", facet::LogLevel::Warning}};

// A console of a fixed height with a stock button before it; records submits.
struct ConsoleHarness
{
    std::string command;
    std::span<const char* const> history;
    std::span<const char* const> commands;
    int submits = 0;
    std::string submitted;
    bool readOnly = false;

    void Run(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        if (facet::ConsoleView("console", &command, kConsoleLog,
                               {.history = history, .commands = commands, .height = 200.0f, .readOnly = readOnly}))
        {
            ++submits;
            submitted = command;
        }
        session.Track("console");
        ImGui::Button("after");
        session.Track("after");
    }

    static ImVec2 Field(facet::test::Session& session)
    {
        const ImRect rect = session.ItemRect("console");
        return ImVec2(rect.Min.x + facet::Px(40.0f), rect.Max.y - facet::ControlHeight(facet::ControlSize::Inherit) * 0.5f);
    }
};

} // namespace

TEST_CASE("ConsoleView writes the command per keystroke, and Enter submits it and keeps the keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ConsoleHarness h;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(ConsoleHarness::Field(session));
    session.Type("he");
    CHECK(h.command == "he");
    session.Type("lp");
    CHECK(h.command == "help");
    CHECK(h.submits == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(h.submits == 1);
    CHECK(h.submitted == "help");
    CHECK(h.command == "help");  // the caller clears it
    h.command.clear();
    session.Frames(3);
    // The field has the keyboard again: typing goes on.
    session.Type("next");
    CHECK(h.command == "next");
    // Enter on an empty field submits nothing.
    h.command.clear();
    session.Frames(2);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(h.submits == 1);
}

TEST_CASE("ConsoleView walks the history with Up and Down and stops at both ends")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kHistory[] = {"first", "second", "third"};
    ConsoleHarness h;
    h.history = kHistory;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(ConsoleHarness::Field(session));
    session.Type("draft");
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(h.command == "third");
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(h.command == "first");
    session.KeyPress(ImGuiKey_UpArrow);  // the oldest end
    CHECK(h.command == "first");
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(h.command == "second");
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);  // past the newest: the typed text again
    CHECK(h.command == "draft");
    session.KeyPress(ImGuiKey_DownArrow);  // the newest end
    CHECK(h.command == "draft");
}

TEST_CASE("ConsoleView completes with Tab: none, one, and several candidates")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kCommands[] = {"clear", "close", "color", "help"};
    ConsoleHarness h;
    h.commands = kCommands;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(ConsoleHarness::Field(session));
    const float plain = session.ItemRect("console").GetHeight();
    // No candidate: the text stays.
    session.Type("zz");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(h.command == "zz");
    CHECK(session.ItemRect("console").GetHeight() == doctest::Approx(plain));
    // One candidate: it goes in with a space.
    session.KeyPress(ImGuiKey_Backspace);
    session.KeyPress(ImGuiKey_Backspace);
    session.Type("he");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(h.command == "help ");
    // Several: the shared prefix, then one candidate per Tab.
    h.command.clear();
    session.Frames(2);
    session.Type("cl");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(h.command == "cl");
    session.KeyPress(ImGuiKey_Tab);
    const std::string first = h.command;
    CHECK((first == "clear" || first == "close"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(h.command != first);
    CHECK((h.command == "clear" || h.command == "close"));
    CHECK(h.submits == 0);
}

TEST_CASE("ConsoleView shares a prefix of several candidates and Tab on an empty field moves on")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kCommands[] = {"reload-config", "reload-assets"};
    ConsoleHarness h;
    h.commands = kCommands;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(ConsoleHarness::Field(session));
    session.Type("rel");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(h.command == "reload-");
    h.command.clear();
    session.Frames(2);
    session.KeyPress(ImGuiKey_Escape);
    session.Click(ConsoleHarness::Field(session));
    session.KeyPress(ImGuiKey_Tab);
    session.Frames(2);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("ConsoleView reads only the visible lines of ten thousand, as of a hundred")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    struct Log
    {
        int calls = 0;
    } log;
    const facet::LogGetter getter = [](void* user, int index, facet::LogEntry* out) {
        ++static_cast<Log*>(user)->calls;
        out->text = index % 2 == 0 ? std::string_view("Even line") : std::string_view("Odd line");
        return true;
    };
    std::string command;
    int count = 100;
    session.SetUi([&] { facet::ConsoleView("console", &command, count, getter, &log, {.height = 200.0f}); });
    session.Frames(3);
    log.calls = 0;
    session.Frame();
    const int hundred = log.calls;
    count = 10000;
    session.Frames(3);
    log.calls = 0;
    session.Frame();
    CHECK(hundred > 0);
    CHECK(log.calls <= hundred + 2);
}

TEST_CASE("ConsoleView with a caller buffer and a log getter submits like the string form")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char buffer[32] = "";
    int submits = 0;
    const facet::LogGetter getter = [](void*, int, facet::LogEntry* out) {
        out->text = "Line";
        return true;
    };
    session.SetUi([&] {
        if (facet::ConsoleView("console", buffer, sizeof(buffer), 3, getter, nullptr, {.height = 200.0f}))
            ++submits;
        session.Track("console");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("console");
    session.Click(ImVec2(rect.Min.x + facet::Px(40.0f), rect.Max.y - facet::ControlHeight(facet::ControlSize::Inherit) * 0.5f));
    session.Type("run");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(submits == 1);
    CHECK(std::string(buffer) == "run");
}

TEST_CASE("ConsoleView read-only takes no typing and submits nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ConsoleHarness h;
    h.command = "fixed";
    h.readOnly = true;
    session.SetUi([&] { h.Run(session); });
    session.Frame();
    session.Click(ConsoleHarness::Field(session));
    session.Type("x");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(h.command == "fixed");
    CHECK(h.submits == 0);
}

// ---- Onboarding ----------------------------------------------------------------------------

namespace
{

// An onboarding panel of `count` steps that records whether its content was submitted.
struct OnboardingHarness
{
    bool open = false;
    int step = 0;
    int count = 3;
    bool skippable = true;
    bool shown = false;
    int shownStep = -1;

    void Run()
    {
        shown = false;
        if (facet::BeginOnboarding("Welcome", &open, &step, count, {.skippable = skippable}))
        {
            shown = true;
            shownStep = step;
            ImGui::TextUnformatted("Step content");
            facet::EndOnboarding();
        }
    }
};

} // namespace

TEST_CASE("Onboarding stays closed until opened, then Enter steps through and Finish closes it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OnboardingHarness h;
    session.SetUi([&] { h.Run(); });
    session.Frames(3);
    CHECK_FALSE(h.shown);
    h.open = true;
    session.Seconds(0.5f);
    CHECK(h.shown);
    CHECK(h.shownStep == 0);
    // Next has the keyboard focus on each step.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(h.step == 1);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(h.step == 2);
    session.KeyPress(ImGuiKey_Enter);  // Finish on the last step
    session.Seconds(0.5f);
    CHECK_FALSE(h.open);
    CHECK_FALSE(h.shown);
    CHECK(h.step == 2);
}

TEST_CASE("Onboarding goes back with the Back button and Escape closes it on any step")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OnboardingHarness h;
    h.open = true;
    h.step = 2;
    session.SetUi([&] { h.Run(); });
    session.Seconds(0.5f);
    // On the last step: Back, then Finish. Shift+Tab reaches Back.
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    CHECK(h.step == 1);
    session.KeyPress(ImGuiKey_Escape);
    session.Seconds(0.5f);
    CHECK_FALSE(h.open);
    CHECK(h.step == 1);
}

TEST_CASE("Onboarding skip closes it, and a panel that cannot be skipped has no Skip button")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OnboardingHarness h;
    h.open = true;
    session.SetUi([&] { h.Run(); });
    session.Seconds(0.5f);
    // Step 0: Skip, then Next. Shift+Tab reaches Skip.
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_Space);
    session.Seconds(0.5f);
    CHECK_FALSE(h.open);
    CHECK(h.step == 0);
    // Without skippable, Shift+Tab from Next has nothing before it to reach on the first step.
    h.skippable = false;
    h.open = true;
    session.Seconds(0.5f);
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    CHECK(h.open);
    CHECK(h.step == 1);
}

TEST_CASE("Onboarding counts fewer than one step as one and clamps the step")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OnboardingHarness h;
    h.open = true;
    h.count = 0;
    h.step = 5;
    session.SetUi([&] { h.Run(); });
    session.Seconds(0.5f);
    CHECK(h.shownStep == 0);
    session.KeyPress(ImGuiKey_Enter);  // the only step: Finish
    session.Seconds(0.5f);
    CHECK_FALSE(h.open);
}
