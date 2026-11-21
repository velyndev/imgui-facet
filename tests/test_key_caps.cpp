// Key caps block (src/internal/key_caps.h): measuring and drawing a chord as caps.

#include <string>

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

TEST_CASE("Key caps: field caps and the KeyCap mark share their names and colors")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::internal::KeyCapColors rest;
    facet::internal::KeyCapColors held;
    facet::internal::KeyCapColors off;
    const char* names[facet::internal::kKeyCapMax] = {};
    int chordCount = 0;
    int foldedCount = 0;
    int markFillVertices = 0;
    session.SetUi([&] {
        rest = facet::internal::KeyCapLook(false, false);
        held = facet::internal::KeyCapLook(true, false);
        off = facet::internal::KeyCapLook(false, true);
        const char* folded[facet::internal::kKeyCapMax] = {};
        foldedCount = facet::internal::KeyCapNames(ImGuiKey_LeftShift, folded);
        chordCount = facet::internal::KeyCapNames(ImGuiKey_K | ImGuiMod_Ctrl | ImGuiMod_Shift, names);
        // The display mark draws its caps in the same rest fill.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int from = drawList->VtxBuffer.Size;
        facet::KeyCap(ImGuiKey_K);
        markFillVertices = 0;
        for (int i = from; i < drawList->VtxBuffer.Size; ++i)
            if (drawList->VtxBuffer[i].col == rest.fill)
                ++markFillVertices;
    });
    session.Frame();
    const facet::Colors& c = facet::T().colors;
    CHECK(rest.fill == facet::ToU32(c.fill[2]));
    CHECK(held.fill == facet::ToU32(c.fill[0]));
    CHECK(held.outline == facet::ToU32(c.focusRing));
    CHECK(off.fill == facet::ToU32(c.fill[3]));
    CHECK(off.outline == facet::ToU32(c.separator));
    REQUIRE(chordCount == 3);
    CHECK(std::string(names[0]) == facet::KeyNameText(static_cast<ImGuiKey>(ImGuiMod_Ctrl)));
    CHECK(std::string(names[1]) == facet::KeyNameText(static_cast<ImGuiKey>(ImGuiMod_Shift)));
    CHECK(std::string(names[2]) == facet::KeyNameText(ImGuiKey_K));
    CHECK(foldedCount == 1);
    CHECK(markFillVertices > 0);
}
