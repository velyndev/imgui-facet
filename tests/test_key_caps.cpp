// Key caps block (src/internal/key_caps.h): measuring and drawing a chord as caps.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/key_caps.h"
#include "support/session.h"

using facet::internal::DrawKeyCaps;
using facet::internal::KeyCapHeight;
using facet::internal::MeasureKeyCaps;

TEST_CASE("Key caps: one cap per modifier and key, sided modifiers folded, drawn as wide as measured")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float none = -1.0f;
    float key = 0.0f;
    float chord = 0.0f;
    float shift = 0.0f;
    float leftShift = 0.0f;
    float drawn = 0.0f;
    float smallHeight = 0.0f;
    float regularHeight = 0.0f;
    session.SetUi([&] {
        none = MeasureKeyCaps(ImGuiKey_None, facet::ControlSize::Regular);
        key = MeasureKeyCaps(ImGuiKey_K, facet::ControlSize::Regular);
        chord = MeasureKeyCaps(ImGuiKey_K | ImGuiMod_Ctrl | ImGuiMod_Shift, facet::ControlSize::Regular);
        shift = MeasureKeyCaps(ImGuiMod_Shift, facet::ControlSize::Regular);
        leftShift = MeasureKeyCaps(ImGuiKey_LeftShift | ImGuiMod_Shift, facet::ControlSize::Regular);
        drawn = DrawKeyCaps(ImGui::GetWindowDrawList(), ImGuiKey_K | ImGuiMod_Ctrl | ImGuiMod_Shift, ImVec2(10.0f, 40.0f),
                            facet::ControlSize::Regular, false);
        smallHeight = KeyCapHeight(facet::ControlSize::Small);
        regularHeight = KeyCapHeight(facet::ControlSize::Regular);
    });
    session.Frame();
    CHECK(none == 0.0f);
    // A single letter is a square cap.
    CHECK(key == doctest::Approx(KeyCapHeight(facet::ControlSize::Regular)));
    CHECK(chord > key * 2.0f);
    CHECK(leftShift == doctest::Approx(shift));
    CHECK(drawn == doctest::Approx(chord));
    CHECK(smallHeight < regularHeight);
    CHECK(regularHeight < facet::ControlHeight(facet::ControlSize::Regular));
}
