// Shared widget types and functions (widgets/common.h): the flags macro, the control size
// stack, control metrics and the built-in strings.

#include <cstdint>
#include <cstring>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "support/session.h"

namespace
{

enum class Parts : std::uint32_t
{
    None = 0,
    Head = 1u << 0,
    Body = 1u << 1,
    Tail = 1u << 2,
};
FACET_FLAGS(Parts)

} // namespace

TEST_CASE("FACET_FLAGS gives a flags enum its operators and Has")
{
    constexpr Parts both = Parts::Head | Parts::Tail;
    static_assert(Has(both, Parts::Head));
    static_assert(!Has(both, Parts::Body));
    static_assert(!Has(both, Parts::None));
    static_assert((both & Parts::Tail) == Parts::Tail);
    static_assert((both ^ Parts::Head) == Parts::Tail);
    Parts set = Parts::None;
    set |= Parts::Body;
    CHECK(Has(set, Parts::Body));
    set &= ~Parts::Body;
    CHECK(set == Parts::None);
    CHECK_FALSE(Has(set, Parts::None));
}

TEST_CASE("Shared constants and the defaults of the shared types")
{
    CHECK(facet::kFill == -FLT_MIN);
    CHECK(facet::kIndeterminate < 0.0f);
    const facet::ChoiceItem item{"Low"};
    CHECK(std::strcmp(item.label, "Low") == 0);
    CHECK_FALSE(item.icon.IsValid());
    CHECK(item.id == 0);
    CHECK_FALSE(item.disabled);
    const facet::RowRange rows;
    CHECK(rows.first == rows.last);
    // Value-initialized enums give the default behavior.
    CHECK(facet::ControlSize{} == facet::ControlSize::Inherit);
    CHECK(facet::Tone{} == facet::Tone::Neutral);
    CHECK(facet::DataState{} == facet::DataState::Ready);
    CHECK(facet::CheckState{} == facet::CheckState::Off);
    CHECK(facet::ValueScale{} == facet::ValueScale::Linear);
    CHECK(facet::Radius{} == facet::Radius::None);
}

TEST_CASE("The control size stack resolves Inherit to the innermost push")
{
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Regular);
    CHECK(facet::ResolveControlSize(facet::ControlSize::Large) == facet::ControlSize::Large);
    facet::PushControlSize(facet::ControlSize::Small);
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Small);
    // An explicit size still wins inside a push.
    CHECK(facet::ResolveControlSize(facet::ControlSize::Large) == facet::ControlSize::Large);
    facet::PushControlSize(facet::ControlSize::Large);
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Large);
    // Pushing Inherit keeps the current size.
    facet::PushControlSize(facet::ControlSize::Inherit);
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Large);
    facet::PopControlSize();
    facet::PopControlSize();
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Small);
    facet::PopControlSize();
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Regular);
}

TEST_CASE("Control heights and text styles follow the tokens and the scale")
{
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    const facet::Metrics& metrics = facet::T().metrics;
    CHECK(facet::ControlHeight(facet::ControlSize::Small) == metrics.controlHeightSmall);
    CHECK(facet::ControlHeight(facet::ControlSize::Regular) == metrics.controlHeight);
    CHECK(facet::ControlHeight(facet::ControlSize::Large) == metrics.controlHeightLarge);
    CHECK(facet::ControlHeight(facet::ControlSize::Inherit) == metrics.controlHeight);
    facet::PushControlSize(facet::ControlSize::Large);
    CHECK(facet::ControlHeight(facet::ControlSize::Inherit) == metrics.controlHeightLarge);
    facet::PopControlSize();
    facet::SetUiScale(2.0f);
    CHECK(facet::ControlHeight(facet::ControlSize::Regular) == doctest::Approx(metrics.controlHeight * facet::Scale()));
    facet::SetUiScale(1.0f);

    CHECK(facet::ControlTextStyle(facet::ControlSize::Small) == facet::TextStyle::Footnote);
    CHECK(facet::ControlTextStyle(facet::ControlSize::Regular) == facet::TextStyle::Body);
    CHECK(facet::ControlTextStyle(facet::ControlSize::Large) == facet::TextStyle::Body);
}

TEST_CASE("Unbalanced control sizes are reported and EndFrame clears them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int popErrors = 0;
    int depthErrors = 0;
    facet::ControlSize atCapacity = facet::ControlSize::Inherit;
    session.SetUi([&] {
        int start = facet::internal::UsageErrorCount();
        facet::PopControlSize();
        popErrors = facet::internal::UsageErrorCount() - start;
        start = facet::internal::UsageErrorCount();
        for (int i = 0; i < facet::kControlSizeStackDepth + 2; ++i)
            facet::PushControlSize(i < facet::kControlSizeStackDepth - 1 ? facet::ControlSize::Small
                                                                           : facet::ControlSize::Large);
        depthErrors = facet::internal::UsageErrorCount() - start;
        atCapacity = facet::ResolveControlSize(facet::ControlSize::Inherit);
        // Two pops balance the ignored pushes; the rest stays open until EndFrame.
        facet::PopControlSize();
        facet::PopControlSize();
    });
    const int before = facet::internal::UsageErrorCount();
    session.Frame();
    CHECK(popErrors == 1);
    CHECK(depthErrors == 1);
    CHECK(atCapacity == facet::ControlSize::Large);
    // The sizes left pushed are one more report, and the next frame starts at Regular.
    CHECK(facet::internal::UsageErrorCount() == before + 3);
    CHECK(facet::ResolveControlSize(facet::ControlSize::Inherit) == facet::ControlSize::Regular);
}

TEST_CASE("SetStrings copies the table and keeps English for null members")
{
    CHECK(std::strcmp(facet::GetStrings().cancel, "Cancel") == 0);
    char buffer[16] = "Abbrechen";
    facet::Strings german;
    german.cancel = buffer;
    german.close = nullptr;
    facet::SetStrings(german);
    buffer[0] = 'X';  // the table holds a copy
    CHECK(std::strcmp(facet::GetStrings().cancel, "Abbrechen") == 0);
    CHECK(std::strcmp(facet::GetStrings().close, "Close") == 0);
    CHECK(std::strcmp(facet::GetStrings().bindToggle, "Toggle") == 0);
    CHECK(std::strcmp(facet::GetStrings().framesPerSecond, "fps") == 0);
    CHECK(std::strcmp(facet::GetStrings().seconds, "s") == 0);
    facet::SetStrings(facet::Strings{});
    CHECK(std::strcmp(facet::GetStrings().cancel, "Cancel") == 0);
}

TEST_CASE("NextVisibleRows answers neutrally until the list core exists")
{
    facet::RowRange rows{3, 9};
    CHECK_FALSE(facet::NextVisibleRows(&rows));
    CHECK(rows.first == 0);
    CHECK(rows.last == 0);
    CHECK_FALSE(facet::NextVisibleRows(nullptr));
}

TEST_CASE("The edit session queries answer false without a context or an edit")
{
    // No Dear ImGui context at all.
    CHECK_FALSE(facet::IsItemEditCommitted());
    CHECK_FALSE(facet::IsItemEditCanceled());
    // A stock item that was clicked but not edited.
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool committed = false;
    bool canceled = false;
    session.SetUi([&] {
        ImGui::Button("button");
        session.Track("button");
        committed = committed || facet::IsItemEditCommitted();
        canceled = canceled || facet::IsItemEditCanceled();
    });
    session.Frame();
    session.Click(session.ItemCenter("button"));
    CHECK_FALSE(committed);
    CHECK_FALSE(canceled);
}

TEST_CASE("IsItemEditCommitted follows a stock widget's edit session")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    int commits = 0;
    int canceled = 0;
    session.SetUi([&] {
        ImGui::SetNextItemWidth(200.0f);
        ImGui::SliderFloat("value", &value, 0.0f, 1.0f);
        session.Track("value");
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        canceled += facet::IsItemEditCanceled() ? 1 : 0;
    });
    session.Frame();
    const ImRect r = session.ItemRect("value");
    session.Drag({ImVec2(r.Min.x + 50.0f, r.GetCenter().y), ImVec2(r.Min.x + 150.0f, r.GetCenter().y)});
    CHECK(commits == 1);
    CHECK(canceled == 0);
}
