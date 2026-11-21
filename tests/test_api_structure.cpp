// The placeholder helpers of the structure families (src/widgets/structure_stub.h): the width
// precedence, the shortened caption inside the outline, the disabled look, framed containers
// and their scopes. The API of each structure family is tested in its own file
// (tests/test_api_lists.cpp, test_api_containers.cpp, test_api_navigation.cpp,
// test_api_layout.cpp, test_api_overlays.cpp), so these tests call the helpers directly and do
// not depend on any widget still being a placeholder.

#include <cfloat>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"
#include "support/structure_stub_check.h"
#include "widgets/structure_stub.h"

namespace
{

using namespace facet::test::structure;
namespace stub = facet::internal;

// The scope names of the frames these tests open, as a widget passes its Begin's name.
constexpr const char* kOuterScope = "BeginTestOuter";
constexpr const char* kInnerScope = "BeginTestInner";

float RegularHeight()
{
    return facet::ControlHeight(facet::ControlSize::Regular);
}

} // namespace

TEST_CASE("A structure placeholder takes its width by the precedence of rule 6")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    float content = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        stub::StubItem("TestBar", "units", stub::StubWidth(100.0f, true), RegularHeight());
        session.Track("units");
        stub::StubItem("TestBar", "fill", stub::StubWidth(facet::kFill, false, 50.0f), RegularHeight());
        session.Track("fill");
        stub::StubItem("TestBar", "minus", stub::StubWidth(-20.0f, true), RegularHeight());
        session.Track("minus");
        ImGui::SetNextItemWidth(150.0f);
        stub::StubItem("TestBar", "next", stub::StubWidth(0.0f, true), RegularHeight());
        session.Track("next");
        ImGui::PushItemWidth(170.0f);
        stub::StubItem("TestBar", "pushed", stub::StubWidth(0.0f, true), RegularHeight());
        session.Track("pushed");
        ImGui::PopItemWidth();
        stub::StubItem("TestBar", "default", stub::StubWidth(0.0f, true), RegularHeight());
        session.Track("default");
        content = stub::StubContentWidth("TestButton", "content");
        stub::StubItem("TestButton", "content", stub::StubWidth(0.0f, false, content), RegularHeight());
        session.Track("content");
    });
    session.Frame();

    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(100.0f)));
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("minus").GetWidth() == doctest::Approx(available - facet::Px(20.0f)));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(150.0f));
    CHECK(session.ItemRect("pushed").GetWidth() == doctest::Approx(170.0f));
    CHECK(session.ItemRect("default").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("content").GetWidth() == doctest::Approx(content));
    CHECK(content < available);
    CHECK(session.ItemRect("default").GetHeight() == doctest::Approx(RegularHeight()));
}

TEST_CASE("A narrow structure placeholder truncates its text inside its outline")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int from = drawList->VtxBuffer.Size;
        stub::StubItem("TestBar", "A label that is far too long for the box", facet::Px(60.0f), RegularHeight());
        session.Track("narrow");
        CHECK(VerticesInside(drawList, from, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())));
        CHECK(drawList->VtxBuffer.Size > from);
    });
    session.Frame();
    CHECK(session.ItemRect("narrow").GetWidth() == doctest::Approx(facet::Px(60.0f)));
}

TEST_CASE("A structure placeholder dims its text inside BeginDisabled")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme saved = facet::T();
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 secondary = facet::ToU32(facet::T().colors.labelSecondary);
    const ImU32 tertiary = facet::ToU32(facet::T().colors.labelTertiary);
    REQUIRE(secondary != tertiary);
    int enabledSecondary = 0;
    int disabledSecondary = 0;
    int disabledTertiary = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        stub::StubItem("TestBar", "enabled", stub::StubWidth(0.0f, true), RegularHeight());
        enabledSecondary = VerticesWithColor(drawList, from, secondary);
        ImGui::BeginDisabled();
        from = drawList->VtxBuffer.Size;
        stub::StubItem("TestBar", "disabled", stub::StubWidth(0.0f, true), RegularHeight());
        disabledSecondary = VerticesWithColor(drawList, from, secondary);
        disabledTertiary = VerticesWithColor(drawList, from, tertiary);
        ImGui::EndDisabled();
    });
    session.Frame();
    facet::SetTheme(saved, false);
    CHECK(enabledSecondary > 0);
    CHECK(disabledSecondary == 0);
    CHECK(disabledTertiary > 0);
}

TEST_CASE("Tab skips structure placeholders and a click on one activates nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        stub::StubItem("TestBar", "planned", stub::StubWidth(0.0f, true), RegularHeight());
        session.Track("planned");
        stub::BeginStubFrame(kOuterScope, "TestStack", "frame", false);
        ImGui::Button("inside");
        session.Track("inside");
        stub::EndStubFrame(kOuterScope);
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("inside"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    session.Click(session.ItemCenter("planned"));
    CHECK_FALSE(ImGui::IsAnyItemActive());
}

TEST_CASE("A structure container placeholder frames its content inside the available width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    float contentRight = 0.0f;
    float left = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        left = ImGui::GetCursorScreenPos().x;
        stub::BeginStubFrame(kOuterScope, "TestStack", "frame", false);
        ImGui::Button("inside", ImVec2(-FLT_MIN, 0.0f));
        session.Track("inside");
        contentRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        stub::EndStubFrame(kOuterScope);
        session.Track("frame");
    });
    session.Frame();
    const ImRect frame = session.ItemRect("frame");
    const ImRect button = session.ItemRect("inside");
    // The frame spans the available width and its content sits inside it with the inset.
    CHECK(frame.GetWidth() == doctest::Approx(available));
    CHECK(button.Min.x > frame.Min.x);
    CHECK(button.Max.x < frame.Max.x);
    CHECK(button.Max.y < frame.Max.y);
    CHECK(contentRight < left + available);
}

TEST_CASE("An End without its Begin is reported and changes no stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    bool ended = true;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        ended = stub::EndStubFrame(kOuterScope) || stub::EndStubFrame(kInnerScope) || stub::EndStubFrame(kOuterScope);
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK_FALSE(ended);
    CHECK(reported == 3);
}

TEST_CASE("An outer End closes the placeholder frames left open inside it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        stub::BeginStubFrame(kOuterScope, "TestStack", "outer", false);
        stub::BeginStubFrame(kInnerScope, "TestRow", "row", true);  // pushes an id and is never ended
        stub::BeginStubFrame(kInnerScope, "TestGrid", "grid", false);  // never ended
        stub::EndStubFrame(kOuterScope);
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 2);
}

TEST_CASE("A placeholder frame left open at the end of a frame is reported and forgotten")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool leaveOpen = true;
    Values v;
    float windowLeft = 0.0f;
    session.SetUi([&] {
        if (leaveOpen)
        {
            stub::BeginStubFrame(kOuterScope, "TestStack", "open", false);
            return;
        }
        // The next frame starts clean: a balanced pair reports nothing and draws its frame.
        windowLeft = ImGui::GetCursorScreenPos().x;
        CHECK_BALANCED({
            stub::BeginStubFrame(kOuterScope, "TestStack", "next", false);
            ImGui::Button("inside");
            session.Track("inside");
            stub::EndStubFrame(kOuterScope);
        });
    });
    // More frames left open than the placeholder stack holds: if they were kept, the stack
    // would be full and the next frame placeholder would draw no frame.
    const int before = facet::internal::UsageErrorCount();
    constexpr int kLeftOpen = facet::internal::kScopeStackDepth + 4;
    session.Frames(kLeftOpen);
    CHECK(facet::internal::UsageErrorCount() == before + kLeftOpen);
    CHECK(facet::internal::OpenScopeCount() == 0);
    leaveOpen = false;
    session.Frame();
    CHECK(facet::internal::UsageErrorCount() == before + kLeftOpen);
    CHECK(session.ItemRect("inside").Min.x > windowLeft);
}

TEST_CASE("Structure placeholder helpers outside a frame draw nothing and keep the scopes balanced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    // Between frames: no frame scope.
    const int before = facet::internal::UsageErrorCount();
    CHECK_FALSE(stub::StubCanDraw());
    stub::StubItem("TestBar", "outside", 100.0f, RegularHeight());
    stub::StubLayer("TestLayer", "outside", true);
    stub::BeginStubFrame(kOuterScope, "TestStack", "outside", true);
    CHECK(stub::EndStubFrame(kOuterScope));
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}
