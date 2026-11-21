// Menu rows (src/internal/popup_menu.h): a leaf closes its chain, a submenu opens after the rest
// delay or at once on a press, follows its row, and the row keeps room for its shortcut.

#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "support/session.h"

namespace fi = facet::internal;

namespace
{

struct MenuMotionGuard
{
    MenuMotionGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetUiScale(1.0f);
    }
    ~MenuMotionGuard() { facet::SetFollowSystemMotion(true); }
};

const ImGuiID kMenuLevel = ImHashStr("menu.level");
const ImGuiID kSubLevel = ImHashStr("menu.sub");

// A button that opens a menu of "Cut" (with a shortcut), "Paste" and "More" (a submenu with "Inner").
struct MenuUi
{
    bool menuOpen = false;
    bool subOpen = false;
    std::string pressed;
    int presses = 0;
    ImRect menuRect;
    float needed = 0.0f;  // the label, the shortcut gap and the shortcut of "Cut" (pixels)

    void Row(facet::test::Session& session, ImGuiID level, const char* label, const fi::MenuRowDesc& desc = {})
    {
        const fi::MenuRowResult row = fi::MenuRow(level, label, desc);
        session.Track(label);
        if (row.pressed)
        {
            pressed = label;
            ++presses;
        }
    }

    void Ui(facet::test::Session& session)
    {
        facet::PushTextStyle(facet::TextStyle::Body);
        needed = ImGui::CalcTextSize("Cut").x + facet::Px(facet::T().overlay.menuShortcutGap) +
                 ImGui::CalcTextSize("Ctrl+X").x;
        facet::PopTextStyle();
        if (ImGui::Button("open"))
            fi::OpenPopupLevel(kMenuLevel, {.rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())});
        session.Track("open");
        if (fi::BeginPopupLevel(kMenuLevel))
        {
            fi::MenuLevelBegin(kMenuLevel);
            Row(session, kMenuLevel, "Cut", {.shortcut = "Ctrl+X"});
            Row(session, kMenuLevel, "Paste");
            Row(session, kMenuLevel, "More", {.submenu = kSubLevel});
            if (fi::BeginPopupLevel(kSubLevel))
            {
                fi::MenuLevelBegin(kSubLevel);
                Row(session, kSubLevel, "Inner");
                fi::EndPopupLevel();
            }
            fi::EndPopupLevel();
        }
        menuOpen = fi::IsPopupLevelOpen(kMenuLevel);
        subOpen = fi::IsPopupLevelOpen(kSubLevel);
        menuRect = fi::PopupLevelRect(kMenuLevel);
    }
};

} // namespace

TEST_CASE("Menu rows: a leaf runs once and closes the whole chain")
{
    MenuMotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frame();
    session.Click(session.ItemCenter("open"));
    session.Frames(2);
    REQUIRE(ui.menuOpen);
    session.Click(session.ItemCenter("More"));
    session.Frames(2);
    CHECK(ui.subOpen);  // a press opens the submenu at once
    CHECK(ui.presses == 0);
    session.Click(session.ItemCenter("Inner"));
    session.Frames(2);
    CHECK(ui.pressed == "Inner");
    CHECK(ui.presses == 1);
    CHECK_FALSE(ui.subOpen);
    CHECK_FALSE(ui.menuOpen);
}

TEST_CASE("Menu rows: a submenu opens after the rest delay and a leaf closes it")
{
    MenuMotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frame();
    session.Click(session.ItemCenter("open"));
    session.Frames(2);
    REQUIRE(ui.menuOpen);
    const float delay = facet::T().overlay.submenuDelay * facet::T().motion.scale;
    session.MouseMove(session.ItemCenter("More"));
    session.Seconds(delay * 0.5f);
    CHECK_FALSE(ui.subOpen);
    session.Seconds(delay);
    CHECK(ui.subOpen);
    // Resting on a leaf of the same level, away from the submenu, closes it.
    session.MouseMove(session.ItemCenter("Cut"));
    session.Seconds(delay * 2.0f);
    CHECK_FALSE(ui.subOpen);
    CHECK(ui.menuOpen);
}

TEST_CASE("Menu rows: the level is wide enough for the label, the gap and the shortcut")
{
    MenuMotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuUi ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frame();
    session.Click(session.ItemCenter("open"));
    session.Frames(3);
    REQUIRE(ui.menuOpen);
    CHECK(session.ItemRect("Cut").GetWidth() >= ui.needed);
    // Rows fill the level, so the shortcut column lines up at the end of every row.
    CHECK(session.ItemRect("Cut").GetWidth() == doctest::Approx(session.ItemRect("Paste").GetWidth()));
}

TEST_CASE("Menu rows: the check of a chosen row takes the shared choice color")
{
    MenuMotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImU32 check = fi::ChoiceCheckColor(false);
    REQUIRE(check == facet::ToU32(facet::T().colors.accentText));
    CHECK(fi::ChoiceCheckColor(true) == facet::ToU32(facet::T().colors.labelTertiary));
    int checkedVertices = -1;
    int plainVertices = -1;
    bool open = false;
    session.SetUi([&] {
        if (ImGui::Button("open"))
            fi::OpenPopupLevel(kMenuLevel, {.rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())});
        session.Track("open");
        if (fi::BeginPopupLevel(kMenuLevel))
        {
            fi::MenuLevelBegin(kMenuLevel);
            const ImDrawList* drawList = ImGui::GetWindowDrawList();
            const auto count = [&](int from) {
                int n = 0;
                for (int i = from; i < drawList->VtxBuffer.Size; ++i)
                    n += drawList->VtxBuffer[i].col == check ? 1 : 0;
                return n;
            };
            int from = drawList->VtxBuffer.Size;
            (void)fi::MenuRow(kMenuLevel, "Chosen", {.checked = true});
            checkedVertices = count(from);
            from = drawList->VtxBuffer.Size;
            (void)fi::MenuRow(kMenuLevel, "Other");
            plainVertices = count(from);
            fi::EndPopupLevel();
        }
        open = fi::IsPopupLevelOpen(kMenuLevel);
    });
    session.Frame();
    session.Click(session.ItemCenter("open"));
    session.Frames(10);
    REQUIRE(open);
    CHECK(checkedVertices > 0);
    CHECK(plainVertices == 0);
}
