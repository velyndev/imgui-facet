// Behaviour of the buttons family (facet/widgets/buttons.h). Button: activation by pointer and
// keyboard, the disabled and loading states, sizes and the width rule, a cut label with its
// tooltip, the variant colors and the item queries after the call.

#include <cfloat>
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

// --- IconButton -------------------------------------------------------------------------------

TEST_CASE("An icon button is one control height square, activates and shows its label as a tooltip")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    bool tip = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::IconButton("Pin window", facet::Icon("pin")))
            ++presses;
        session.Track("pin");
        tip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        facet::IconButton("Large##large", facet::Icon("pin"), {.size = facet::ControlSize::Large});
        session.Track("large");
    });
    session.Frame();
    const ImRect r = session.ItemRect("pin");
    CHECK(r.GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(r.GetHeight() == doctest::Approx(r.GetWidth()));
    CHECK(session.ItemRect("large").GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
    session.MouseMove(session.ItemCenter("pin"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tip);
    session.Click(session.ItemCenter("pin"));
    CHECK(presses == 1);
    // Keyboard: a Tab stop that Space and Enter activate; the tooltip shows on keyboard focus too.
    session.MouseMove(ImVec2(1.0f, 500.0f));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("pin"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tip);
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(presses == 3);
}

TEST_CASE("A latched icon button whose label changes keeps its id and shows its tooltip")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool pinned = false;
    bool tip = false;
    session.SetUi([&] {
        if (facet::IconButton(pinned ? "Unpin###pin" : "Pin###pin", facet::Icon(pinned ? "unpin" : "pin"), {.selected = pinned}))
            pinned = !pinned;
        session.Track("pin");
        tip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        ImGui::Button("elsewhere");
        session.Track("elsewhere");
    });
    session.Frame();
    session.Click(session.ItemCenter("pin"));
    CHECK(pinned);
    session.MouseMove(session.ItemCenter("elsewhere"));
    session.Frames(3);
    session.MouseMove(session.ItemCenter("pin"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.3f);
    CHECK(tip);
}

TEST_CASE("A selected icon button shows the latched tint; loading and disabled ones do not activate")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 tint = facet::ToU32(facet::T().colors.accentSubtle);
    int selectedTint = 0;
    int plainTint = -1;
    int presses = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::IconButton("Lock", facet::Icon("lock"), {.selected = true});
        selectedTint = CountColor(from, tint);
        from = drawList->VtxBuffer.Size;
        facet::IconButton("Unlock", facet::Icon("unlock"));
        plainTint = CountColor(from, tint);
        presses += facet::IconButton("Refresh", facet::Icon("refresh"), {.loading = true}) ? 1 : 0;
        session.Track("loading");
        ImGui::BeginDisabled();
        presses += facet::IconButton("Delete", facet::Icon("trash"), {.variant = facet::ButtonVariant::Destructive}) ? 1 : 0;
        session.Track("disabled");
        ImGui::EndDisabled();
    });
    session.Frame();
    CHECK(selectedTint > 0);
    CHECK(plainTint == 0);
    session.Click(session.ItemCenter("loading"));
    session.Click(session.ItemCenter("disabled"));
    CHECK(presses == 0);
}

// --- Link -------------------------------------------------------------------------------------

namespace
{

std::string& OpenedUrl()
{
    static std::string url;
    return url;
}

// Stands in for the platform's shell, so a test never starts a browser.
bool RecordOpen(ImGuiContext*, const char* path)
{
    OpenedUrl() = path != nullptr ? path : "";
    return true;
}

} // namespace

TEST_CASE("A link activates by click and keys and asks the platform hook to open its address")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImGui::GetPlatformIO().Platform_OpenInShellFn = RecordOpen;
    OpenedUrl().clear();
    int presses = 0;
    int plainPresses = 0;
    float textWidth = 0.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Link("Release notes", {.url = "https://example.invalid/notes"}))
            ++presses;
        session.Track("link");
        if (facet::Link("Show details"))
            ++plainPresses;
        session.Track("plain");
        textWidth = facet::MeasureText(facet::TextStyle::Body, "Show details").x;
    });
    session.Frame();
    CHECK(session.ItemRect("plain").GetWidth() == doctest::Approx(std::ceil(textWidth)));
    CHECK(session.ItemRect("link").GetWidth() > session.ItemRect("plain").GetWidth() * 0.5f);
    session.Click(session.ItemCenter("link"));
    CHECK(presses == 1);
    CHECK(OpenedUrl() == "https://example.invalid/notes");
    OpenedUrl().clear();
    session.Click(session.ItemCenter("plain"));
    CHECK(plainPresses == 1);
    CHECK(OpenedUrl().empty());  // no address, nothing opened
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("link"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(presses == 2);
}

TEST_CASE("A link draws accentText, accentTextVisited when visited, and nothing activates it while disabled")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    ImGui::GetPlatformIO().Platform_OpenInShellFn = RecordOpen;
    const facet::Colors& c = facet::T().colors;
    int fresh = 0;
    int visited = 0;
    int presses = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::Link("Fresh");
        fresh = CountColor(from, facet::ToU32(c.accentText));
        from = drawList->VtxBuffer.Size;
        facet::Link("Visited", {.visited = true});
        visited = CountColor(from, facet::ToU32(c.accentTextVisited));
        ImGui::BeginDisabled();
        presses += facet::Link("Disabled", {.url = "https://example.invalid"}) ? 1 : 0;
        session.Track("disabled");
        ImGui::EndDisabled();
    });
    session.Frame();
    CHECK(fresh > 0);
    CHECK(visited > 0);
    OpenedUrl().clear();
    session.Click(session.ItemCenter("disabled"));
    CHECK(presses == 0);
    CHECK(OpenedUrl().empty());
}

// --- CopyButton -------------------------------------------------------------------------------

TEST_CASE("A copy button writes the clipboard and confirms without moving anything")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 success = facet::ToU32(facet::T().colors.success);
    int copies = 0;
    int successVertices = 0;
    float afterX = 0.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        if (facet::CopyButton("Copy the address", "placeholder text", {.iconOnly = false}))
            ++copies;
        successVertices = CountColor(from, success);
        session.Track("copy");
        ImGui::SameLine();
        afterX = ImGui::GetCursorScreenPos().x;
        facet::CopyButton("##icon", "second text");
        session.Track("icon");
    });
    session.Frame();
    const ImRect rest = session.ItemRect("copy");
    const float restAfter = afterX;
    CHECK(successVertices == 0);
    session.Click(session.ItemCenter("copy"));
    CHECK(copies == 1);
    CHECK(session.Clipboard() == "placeholder text");
    // Confirming: the check in the success color, the same rectangle, the next item where it was.
    session.Frame();
    CHECK(successVertices > 0);
    CHECK(session.ItemRect("copy").GetWidth() == doctest::Approx(rest.GetWidth()));
    CHECK(afterX == doctest::Approx(restAfter));
    // After motion.feedbackHold the button shows its name again.
    session.Seconds(facet::T().motion.feedbackHold + 0.2f);
    CHECK(successVertices == 0);
    // Keyboard, and the icon-only form: one control height square.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("icon"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(session.Clipboard() == "second text");
    const ImRect icon = session.ItemRect("icon");
    CHECK(icon.GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
}

TEST_CASE("A copy button holds its confirmation for feedbackHold times motion.scale")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::Theme theme = facet::Theme::Light();
    theme.motion.scale = 2.0f;
    facet::SetTheme(theme, false);
    const ImU32 success = facet::ToU32(facet::T().colors.success);
    int successVertices = 0;
    session.SetUi([&] {
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::CopyButton("Copy the address", "placeholder text");
        successVertices = CountColor(from, success);
        session.Track("copy");
    });
    session.Frame();
    session.Click(session.ItemCenter("copy"));
    // Past the unscaled hold, still inside the scaled one: confirming.
    session.Seconds(facet::T().motion.feedbackHold + 0.2f);
    CHECK(successVertices > 0);
    // Past the scaled hold: the name again.
    session.Seconds(facet::T().motion.feedbackHold + 0.2f);
    CHECK(successVertices == 0);
}

TEST_CASE("A copy button without text is disabled and copies nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetClipboard("unchanged");
    int copies = 0;
    session.SetUi([&] {
        copies += facet::CopyButton("Copy", nullptr) ? 1 : 0;
        session.Track("copy");
    });
    session.Frame();
    session.Click(session.ItemCenter("copy"));
    CHECK(copies == 0);
    CHECK(session.Clipboard() == "unchanged");
}

// --- ToggleButtonGroup ------------------------------------------------------------------------

namespace
{

ImGuiID ToggleEntryId(const char* label, int index)
{
    return ImHashData(&index, sizeof(index), ImGui::GetID(label));
}

// The bounding box of the vertices of `color` the window draw list received since `from`.
ImRect ColorBounds(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImRect bounds(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        if (drawList->VtxBuffer[i].col == color)
            bounds.Add(drawList->VtxBuffer[i].pos);
    return bounds;
}

const facet::ChoiceItem kTools[] = {
    {"Grid", facet::Icon("grid")},
    {"List", facet::Icon("list")},
    {"Layers", facet::Icon("layers"), nullptr, nullptr, true},
    {"Image", facet::Icon("image")},
};

} // namespace

TEST_CASE("Independent toggle buttons flip their flags; each enabled button is a Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool flags[3] = {false, true, false};  // the fourth item has no flag: drawn disabled
    int writes = 0;
    ImGuiID ids[4] = {};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        for (int i = 0; i < 4; ++i)
            ids[i] = ToggleEntryId("Tools", i);
        if (facet::ToggleButtonGroup("Tools", flags, kTools))
            ++writes;
        session.Track("group");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const ImRect r = session.ItemRect("group");
    const float side = facet::ControlHeight(facet::ControlSize::Regular);
    // Icon-only buttons are one control height wide, at the end of the label in front.
    session.Click(ImVec2(r.Max.x - side * 3.5f, r.GetCenter().y));
    CHECK(flags[0]);
    session.Click(ImVec2(r.Max.x - side * 2.5f, r.GetCenter().y));
    CHECK_FALSE(flags[1]);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == ids[0]);
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(flags[0]);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == ids[1]);
    session.KeyPress(ImGuiKey_Tab);  // the disabled third and the flagless fourth are passed over
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(writes == 3);
}

TEST_CASE("An exclusive toggle group writes the index, and with allowEmpty a second press writes -1")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"Left", "Center", "Right"};
    int strict = 0;
    int loose = 1;
    session.SetUi([&] {
        facet::ToggleButtonGroup("##strict", &strict, kItems, {.width = 300.0f});
        session.Track("strict");
        facet::ToggleButtonGroup("##loose", &loose, kItems, {.allowEmpty = true, .width = 300.0f});
        session.Track("loose");
    });
    session.Frame();
    const ImRect s = session.ItemRect("strict");
    session.Click(ImVec2(s.Min.x + s.GetWidth() * 0.83f, s.GetCenter().y));
    CHECK(strict == 2);
    session.Click(ImVec2(s.Min.x + s.GetWidth() * 0.83f, s.GetCenter().y));
    CHECK(strict == 2);  // pressing the selected button again keeps it
    const ImRect l = session.ItemRect("loose");
    session.Click(ImVec2(l.Min.x + l.GetWidth() * 0.5f, l.GetCenter().y));
    CHECK(loose == -1);
    session.Click(ImVec2(l.Min.x + l.GetWidth() * 0.17f, l.GetCenter().y));
    CHECK(loose == 0);
}

TEST_CASE("The selection pill of a toggle group moves frame by frame exactly as a segmented control's")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    static const char* const kItems[] = {"One", "Two", "Three", "Four"};
    int a = 0;
    int b = 0;
    ImRect segments;
    ImRect toggles;
    session.SetUi([&] {
        const ImU32 tint = facet::ToU32(facet::T().colors.accentSubtle);
        int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::SegmentedControl("##segments", &a, kItems, {.width = 320.0f});
        segments = ColorBounds(from, tint);
        from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ToggleButtonGroup("##toggles", &b, kItems, {.width = 320.0f});
        toggles = ColorBounds(from, tint);
    });
    session.Frame();
    a = 3;
    b = 3;
    for (int frame = 0; frame < 30; ++frame)
    {
        session.Frame();
        CHECK(toggles.Min.x == doctest::Approx(segments.Min.x));
        CHECK(toggles.Max.x == doctest::Approx(segments.Max.x));
        CHECK(toggles.GetHeight() == doctest::Approx(segments.GetHeight()));
    }
}

TEST_CASE("Toggle buttons that do not fit move into a menu and can be chosen there")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kItems[] = {"First option", "Second option", "Third option", "Fourth option"};
    int index = 0;
    bool moreHovered = false;
    bool menuOpen = false;
    float available = 0.0f;
    session.SetUi([&] {
        ImGui::BeginChild("narrow", ImVec2(facet::Px(200.0f), facet::Px(300.0f)));
        available = ImGui::GetContentRegionAvail().x;
        const ImGuiID id = ImGui::GetID("##view");
        facet::ToggleButtonGroup("##view", &index, kItems);
        session.Track("group");
        moreHovered = GImGui->HoveredId == ImGui::GetIDWithSeed("more", nullptr, id);
        menuOpen = facet::internal::IsPopupLevelOpen(ImGui::GetIDWithSeed("more menu", nullptr, id));
        ImGui::EndChild();
    });
    session.Frame();
    session.Frame();
    const ImRect r = session.ItemRect("group");
    CHECK(r.GetWidth() == doctest::Approx(available));
    // The "more" button is at the end of the row.
    session.MouseMove(ImVec2(r.Max.x - 4.0f, r.GetCenter().y));
    session.Frame();
    CHECK(moreHovered);
    session.Click(ImVec2(r.Max.x - 4.0f, r.GetCenter().y));
    session.Frames(2);
    REQUIRE(menuOpen);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(index > 0);
    CHECK_FALSE(menuOpen);
}

TEST_CASE("A button outside a frame returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CHECK_FALSE(facet::Button("Save"));
    CHECK_FALSE(facet::Button("Save", {.variant = facet::ButtonVariant::Primary, .loading = true}));
}
