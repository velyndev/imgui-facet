// Behaviour of the buttons family (facet/widgets/buttons.h). Button: activation by pointer and
// keyboard, the disabled and loading states, sizes and the width rule, a cut label with its
// tooltip, the variant colors and the item queries after the call.

#include <cmath>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"

namespace
{

// Vertices of `color` the window draw list received since `from`.
int CountColor(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        count += drawList->VtxBuffer[i].col == color ? 1 : 0;
    return count;
}

// Restores the theme a test changes.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    ~ThemeGuard() { facet::SetTheme(saved, false); }
};

} // namespace

TEST_CASE("A button activates on a click released inside, once")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    bool hovered = false;
    session.SetUi([&] {
        if (facet::Button("Save"))
            ++presses;
        session.Track("save");
        hovered = ImGui::IsItemHovered();
        ImGui::Button("elsewhere");
        session.Track("elsewhere");
    });
    session.Frame();
    session.Click(session.ItemCenter("save"));
    CHECK(presses == 1);
    // Pressed on the button, released elsewhere: no activation.
    session.MouseMove(session.ItemCenter("save"));
    session.MouseDown();
    session.MouseMove(session.ItemCenter("elsewhere"));
    session.MouseUp();
    session.Frame();
    CHECK(presses == 1);
    session.MouseMove(session.ItemCenter("save"));
    session.Frame();
    CHECK(hovered);
}

TEST_CASE("A focused button activates with Space and Enter and is a Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Button("Apply", {.variant = facet::ButtonVariant::Primary}))
            ++presses;
        session.Track("apply");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("apply"));
    session.KeyPress(ImGuiKey_Space);
    CHECK(presses == 1);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(presses == 2);
}

TEST_CASE("A disabled button never activates and Tab passes over it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        ImGui::BeginDisabled();
        if (facet::Button("Delete", {.variant = facet::ButtonVariant::Destructive}))
            ++presses;
        session.Track("delete");
        ImGui::EndDisabled();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("delete"));
    CHECK(presses == 0);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("A loading button keeps its width and its Tab stop and ignores activation")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool loading = false;
    int presses = 0;
    float idleWidth = 0.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Button("Upload", {.icon = facet::Icon("upload"), .loading = loading}))
            ++presses;
        session.Track("upload");
        if (facet::Button("Send", {.loading = loading}))
            ++presses;
        session.Track("send");
    });
    session.Frame();
    idleWidth = session.ItemRect("upload").GetWidth();
    const float sendWidth = session.ItemRect("send").GetWidth();
    loading = true;
    session.Frame();
    CHECK(session.ItemRect("upload").GetWidth() == doctest::Approx(idleWidth));
    CHECK(session.ItemRect("send").GetWidth() == doctest::Approx(sendWidth));
    session.Click(session.ItemCenter("upload"));
    session.Click(session.ItemCenter("send"));
    CHECK(presses == 0);
    // Still focusable, so the focus does not jump away while it waits.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("upload"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(presses == 0);
}

TEST_CASE("A button takes the control height of its size and its width by rule 6")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    float labelWidth = 0.0f;
    float padding = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        labelWidth = facet::MeasureText(facet::TextStyle::Body, "Save").x;
        padding = facet::Px(facet::T().spacing.md);
        facet::Button("Save");
        session.Track("regular");
        facet::Button("Save##small", {.size = facet::ControlSize::Small});
        session.Track("small");
        facet::Button("Save##large", {.size = facet::ControlSize::Large});
        session.Track("large");
        facet::PushControlSize(facet::ControlSize::Small);
        facet::Button("Save##inherited");
        session.Track("inherited");
        facet::PopControlSize();
        facet::Button("Save##units", {.width = 150.0f});
        session.Track("units");
        facet::Button("Save##fill", {.width = facet::kFill});
        session.Track("fill");
        ImGui::SetNextItemWidth(130.0f);
        facet::Button("Save##next");
        session.Track("next");
        facet::Button("##icon only", {.icon = facet::Icon("settings")});
        session.Track("icon");
    });
    session.Frame();
    CHECK(session.ItemRect("regular").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("small").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    CHECK(session.ItemRect("large").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
    CHECK(session.ItemRect("inherited").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    // Its content by default: the label and the padding on both sides.
    CHECK(session.ItemRect("regular").GetWidth() == doctest::Approx(std::ceil(labelWidth + padding * 2.0f)));
    CHECK(session.ItemRect("small").GetWidth() < session.ItemRect("regular").GetWidth());
    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(150.0f)));
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(130.0f));
    // An icon without a label: one control height square.
    const ImRect icon = session.ItemRect("icon");
    CHECK(icon.GetWidth() == doctest::Approx(icon.GetHeight()));
}

TEST_CASE("A button narrower than its label cuts it and shows the whole label on hover")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    // The tooltip levels are queried inside the frame, where the popup host answers.
    bool narrowTip = false;
    bool wideTip = false;
    session.SetUi([&] {
        facet::Button("Export the selected layers##narrow", {.width = 60.0f});
        session.Track("narrow");
        narrowTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        facet::Button("Export");
        session.Track("wide");
        wideTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("wide"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK_FALSE(wideTip);
    session.MouseMove(session.ItemCenter("narrow"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(narrowTip);
    CHECK(session.ItemRect("narrow").GetWidth() == doctest::Approx(facet::Px(60.0f)));
}

TEST_CASE("Each button variant draws its own fill and the disabled look is shared")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    const facet::Colors& c = facet::T().colors;
    int primary = 0;
    int destructive = 0;
    int secondary = 0;
    int plain = 0;
    int disabledAccent = 0;
    int disabledFill = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::Button("Primary", {.variant = facet::ButtonVariant::Primary});
        primary = CountColor(from, facet::ToU32(c.accent));
        from = drawList->VtxBuffer.Size;
        facet::Button("Destructive", {.variant = facet::ButtonVariant::Destructive});
        destructive = CountColor(from, facet::ToU32(c.destructive));
        from = drawList->VtxBuffer.Size;
        facet::Button("Secondary");
        secondary = CountColor(from, facet::ToU32(c.fill[2]));
        from = drawList->VtxBuffer.Size;
        facet::Button("Plain", {.variant = facet::ButtonVariant::Plain});
        plain = CountColor(from, facet::ToU32(c.fill[2]));
        ImGui::BeginDisabled();
        from = drawList->VtxBuffer.Size;
        facet::Button("Disabled", {.variant = facet::ButtonVariant::Primary});
        disabledAccent = CountColor(from, facet::ToU32(c.accent));
        disabledFill = CountColor(from, facet::ToU32(c.fill[3]));
        ImGui::EndDisabled();
    });
    session.Frame();
    CHECK(primary > 0);
    CHECK(destructive > 0);
    CHECK(secondary > 0);
    CHECK(plain == 0);  // text only at rest
    CHECK(disabledAccent == 0);
    CHECK(disabledFill > 0);
}

TEST_CASE("The button focus ring shows after keyboard input only")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    // The secondary variant has no focusRing-colored outline of its own, so these are the ring.
    const ImU32 ring = facet::ToU32(facet::T().colors.focusRing);
    int ringVertices = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::Button("Open");
        session.Track("open");
        ringVertices = CountColor(from, ring);
    });
    session.Frame();
    session.Click(session.ItemCenter("open"));
    session.Frames(10);
    CHECK(ringVertices == 0);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("open"));
    session.Frames(10);
    CHECK(ringVertices > 0);
}

TEST_CASE("After a button the item queries describe it and every stack is as before")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGuiID expected = 0;
    ImGuiID lastItem = 0;
    int fonts = -1;
    int fontsAfter = -2;
    int ids = -1;
    int idsAfter = -2;
    session.SetUi([&] {
        expected = ImGui::GetID("Rename###rename");
        fonts = GImGui->FontStack.Size;
        ids = GImGui->CurrentWindow->IDStack.Size;
        facet::Button("Rename###rename");
        lastItem = ImGui::GetItemID();
        fontsAfter = GImGui->FontStack.Size;
        idsAfter = GImGui->CurrentWindow->IDStack.Size;
        // A null label counts as "".
        ImGui::PushID("null");
        facet::Button(nullptr, {.icon = facet::Icon("plus")});
        ImGui::PopID();
    });
    session.Frame();
    CHECK(lastItem == expected);
    CHECK(fontsAfter == fonts);
    CHECK(idsAfter == ids);
}

TEST_CASE("A button outside a frame returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CHECK_FALSE(facet::Button("Save"));
    CHECK_FALSE(facet::Button("Save", {.variant = facet::ButtonVariant::Primary, .loading = true}));
}
