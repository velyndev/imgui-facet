// PushAccent and PopAccent: a scoped accent with every derived token, no change to the
// current theme or the stock style, and repair of an unbalanced stack in EndFrame.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "support/session.h"

namespace
{

bool Same(const ImVec4& a, const ImVec4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// Every accent token, compared one by one.
bool SameAccentTokens(const facet::Colors& a, const facet::Colors& b)
{
    return Same(a.accent, b.accent) && Same(a.accentHover, b.accentHover) && Same(a.accentPressed, b.accentPressed) &&
           Same(a.accentSubtle, b.accentSubtle) && Same(a.onAccent, b.onAccent) &&
           Same(a.accentText, b.accentText) && Same(a.focusRing, b.focusRing);
}

const ImVec4 kGreen = facet::Rgb(0x248A3D);
const ImVec4 kOrange = facet::Rgb(0xC93400);

// Starts every case from the dark preset with its own accent.
struct DarkTheme
{
    DarkTheme()
    {
        facet::ClearAccent();
        facet::SetTheme(facet::Theme::Dark(), false);
    }
    ~DarkTheme() { facet::SetTheme(facet::Theme::Light(), false); }
};

} // namespace

TEST_CASE("PushAccent derives every accent token and PopAccent restores the theme")
{
    DarkTheme dark;
    const facet::Theme base = facet::T();
    facet::Theme expected = base;
    expected.ApplyAccent(kGreen);

    facet::PushAccent(kGreen);
    CHECK(SameAccentTokens(facet::T().colors, expected.colors));
    // Everything that is not an accent token stays.
    CHECK(Same(facet::T().colors.bg, base.colors.bg));
    CHECK(Same(facet::T().colors.label, base.colors.label));
    CHECK(facet::T().dark == base.dark);
    // Not a SetAccent: the custom accent and the unscoped theme are untouched.
    CHECK_FALSE(facet::HasCustomAccent());

    facet::PopAccent();
    CHECK(SameAccentTokens(facet::T().colors, base.colors));
}

TEST_CASE("Accent scopes nest and the innermost one wins")
{
    DarkTheme dark;
    facet::Theme green = facet::T();
    green.ApplyAccent(kGreen);
    facet::Theme orange = facet::T();
    orange.ApplyAccent(kOrange);

    facet::PushAccent(kGreen);
    facet::PushAccent(kOrange);
    CHECK(SameAccentTokens(facet::T().colors, orange.colors));
    facet::PopAccent();
    CHECK(SameAccentTokens(facet::T().colors, green.colors));
    facet::PopAccent();
}

TEST_CASE("A theme reference held in an outer accent scope keeps its accent across a nested scope")
{
    DarkTheme dark;
    facet::Theme green = facet::T();
    green.ApplyAccent(kGreen);

    facet::PushAccent(kGreen);
    const facet::Theme& outer = facet::T();
    facet::PushAccent(kOrange);
    (void)facet::T();
    facet::PopAccent();
    CHECK(SameAccentTokens(outer.colors, green.colors));
    // A second nested scope at the same depth does not reach it either.
    facet::PushAccent(kOrange);
    (void)facet::T();
    CHECK(SameAccentTokens(outer.colors, green.colors));
    facet::PopAccent();
    CHECK(&facet::T() == &outer);
    facet::PopAccent();
}

TEST_CASE("A theme change inside an accent scope keeps the scoped accent on the new surfaces")
{
    DarkTheme dark;
    facet::PushAccent(kGreen);
    facet::SetTheme(facet::Theme::Light(), false);
    facet::Theme expected = facet::Theme::Light();
    expected.ApplyAccent(kGreen);
    CHECK(SameAccentTokens(facet::T().colors, expected.colors));
    CHECK(Same(facet::T().colors.bg, expected.colors.bg));
    facet::PopAccent();
    CHECK(SameAccentTokens(facet::T().colors, facet::Theme::Light().colors));
}

TEST_CASE("An accent scope does not reach the Dear ImGui style")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DarkTheme dark;
    facet::ApplyImGuiStyle();
    const ImVec4 before = ImGui::GetStyle().Colors[ImGuiCol_CheckMark];
    facet::PushAccent(kGreen);
    facet::ApplyImGuiStyle();
    CHECK(Same(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], before));
    facet::PopAccent();
}

TEST_CASE("EndFrame reports accents left pushed and clears them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DarkTheme dark;
    const facet::Theme base = facet::T();
    bool scopedInFrame = false;
    session.SetUi([&] {
        facet::PushAccent(kGreen);
        facet::PushAccent(kOrange);
        scopedInFrame = !SameAccentTokens(facet::T().colors, base.colors);
    });
    const int errorsBefore = facet::internal::UsageErrorCount();
    session.Frame();
    CHECK(scopedInFrame);
    // One report for the frame, and the next frame starts unscoped.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore + 1);
    CHECK(SameAccentTokens(facet::T().colors, base.colors));
    session.SetUi([] {});
    session.Frame();
    CHECK(facet::internal::UsageErrorCount() == errorsBefore + 1);
}

TEST_CASE("Unbalanced and too deep accent pushes are reported and repaired")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DarkTheme dark;
    const facet::Theme base = facet::T();
    int popErrors = 0;
    int depthErrors = 0;
    bool innermostAtCapacity = false;
    session.SetUi([&] {
        // A pop without a push is reported and ignored.
        int start = facet::internal::UsageErrorCount();
        facet::PopAccent();
        popErrors = facet::internal::UsageErrorCount() - start;
        // Pushes beyond the capacity are reported once and ignored; their pops still balance,
        // so the accent of the last push that fitted stays in effect until its own pop.
        start = facet::internal::UsageErrorCount();
        for (int i = 0; i < facet::kAccentStackDepth + 3; ++i)
            facet::PushAccent(i < facet::kAccentStackDepth - 1 ? kOrange : kGreen);
        depthErrors = facet::internal::UsageErrorCount() - start;
        facet::Theme green = base;
        green.ApplyAccent(kGreen);
        innermostAtCapacity = SameAccentTokens(facet::T().colors, green.colors);
        for (int i = 0; i < facet::kAccentStackDepth + 3; ++i)
            facet::PopAccent();
    });
    const int errorsBefore = facet::internal::UsageErrorCount();
    session.Frame();
    CHECK(popErrors == 1);
    CHECK(depthErrors == 1);
    CHECK(innermostAtCapacity);
    // Balanced by the end of the frame: nothing more is reported.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore + 2);
    CHECK(SameAccentTokens(facet::T().colors, base.colors));
}
