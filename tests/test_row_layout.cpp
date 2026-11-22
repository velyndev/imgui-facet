// Labelled rows (src/internal/row_layout.h).

#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/row_layout.h"
#include "support/session.h"

using facet::internal::BeginRow;
using facet::internal::EndRow;
using facet::internal::RowLayout;
using facet::internal::RowOptions;

namespace
{

struct LayoutGuard
{
    LayoutGuard()
    {
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
    ~LayoutGuard()
    {
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

// What one row reports, plus the control the caller submitted in it.
struct RowProbe
{
    RowLayout layout;
    float itemWidth = 0.0f;
    ImRect control;
    ImRect row;
    ImGuiID controlId = 0;
    float controlBaseline = 0.0f;
    bool reset = false;
};

// A row with a control that draws its text like a Facet control of the regular size.
void Row(const char* label, const RowOptions& options, RowProbe& probe)
{
    probe.layout = BeginRow(label, options);
    probe.itemWidth = ImGui::CalcItemWidth();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = facet::ControlHeight(options.size);
    probe.controlId = ImGui::GetID("##value");
    ImGui::Dummy(ImVec2(probe.itemWidth, height));
    probe.control = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    facet::PushTextStyle(facet::ControlTextStyle(options.size));
    ImFontBaked* baked = ImGui::GetFontBaked();
    probe.controlBaseline = facet::CenteredTextY(pos.y, height) + baked->Ascent * ImGui::GetFontSize() / baked->Size;
    facet::PopTextStyle();
    probe.reset = EndRow();
    probe.row = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

} // namespace

TEST_CASE("Rows: label column beside the control, baselines aligned")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe probe;
    float origin = 0.0f;
    session.SetUi([&] {
        origin = ImGui::GetCursorScreenPos().x;
        Row("Volume", {}, probe);
    });
    session.Frames(2);
    const facet::Theme& t = facet::T();
    CHECK_FALSE(probe.layout.stacked);
    CHECK(probe.control.Min.x == doctest::Approx(origin + t.layout.labelColumn));
    CHECK(probe.itemWidth == doctest::Approx(probe.layout.control.GetWidth()));
    CHECK(probe.layout.baseline == doctest::Approx(probe.controlBaseline));
    CHECK_FALSE(probe.layout.truncated);
    // The row is one item as tall as the control.
    CHECK(probe.row.GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));

    // Large controls move the label baseline with them.
    RowProbe large;
    session.SetUi([&] { Row("Volume", {.size = facet::ControlSize::Large}, large); });
    session.Frame();
    CHECK(large.layout.baseline == doctest::Approx(large.controlBaseline));
    CHECK(large.layout.baseline > probe.layout.baseline);
}

TEST_CASE("Rows: narrow regions stack the label above a full-width control")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe probe;
    float width = 600.0f;
    session.SetUi([&] {
        ImGui::BeginChild("narrow", ImVec2(width, 300.0f));
        Row("Volume", {}, probe);
        ImGui::EndChild();
    });
    const facet::Theme& t = facet::T();
    width = t.layout.labelColumn + t.layout.minRowControl - 20.0f;
    session.Frames(2);
    CHECK(probe.layout.stacked);
    CHECK(probe.control.Min.x == doctest::Approx(probe.layout.label.Min.x));
    CHECK(probe.control.Min.y >= probe.layout.label.Max.y);
    CHECK(probe.itemWidth == doctest::Approx(probe.layout.control.GetWidth()));
    CHECK(probe.itemWidth == doctest::Approx(width));
}

TEST_CASE("Rows: a long label ends in an ellipsis and shows the full text on hover")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe probe;
    session.SetUi([&] { Row("A label much longer than the label column can hold", {}, probe); });
    session.Frames(2);
    CHECK(probe.layout.truncated);
    CHECK(probe.layout.label.GetWidth() <= facet::T().layout.labelColumn - facet::T().spacing.sm + 0.5f);
    session.MouseMove(probe.layout.label.GetCenter());
    session.Frame();
    ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
    REQUIRE(tooltip != nullptr);
    CHECK(tooltip->Active);
    // The control still gets the rest of the row.
    CHECK(probe.itemWidth > 0.0f);
}

TEST_CASE("Rows: keyboard focus on the control shows the full text of a cut label")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe before;
    RowLayout layout;
    ImGuiID control = 0;
    session.SetUi([&] {
        Row("Before", {}, before);
        layout = BeginRow("A label much longer than the label column can hold");
        control = ImGui::GetID("##value");
        ImGui::Button("##value");
        EndRow();
        ImGui::Button("after");
    });
    session.Frames(2);
    REQUIRE(layout.truncated);
    // The pointer stays away from the label: only the focus can show the tooltip.
    session.MouseMove(ImVec2(590.0f, 390.0f));
    const auto tooltipShown = [] {
        ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
        return tooltip != nullptr && tooltip->Active && !tooltip->Hidden;
    };
    CHECK_FALSE(tooltipShown());
    // The row before has no Tab stop, so Tab focuses the control of the cut row.
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == control);
    CHECK(tooltipShown());
    // Placed under the label.
    ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
    REQUIRE(tooltip != nullptr);
    CHECK(tooltip->Pos.y >= layout.label.Max.y);
    CHECK(tooltip->Pos.x == doctest::Approx(layout.label.Min.x));
    // Focus on an item after the row hides it again.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() != control);
    CHECK_FALSE(tooltipShown());
}

TEST_CASE("Rows: a group shares one label column sized by its widest label")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(800.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe a;
    RowProbe b;
    RowProbe outside;
    session.SetUi([&] {
        facet::internal::BeginRowGroup("group");
        Row("Short", {}, a);
        Row("A clearly longer label here", {}, b);
        facet::internal::EndRowGroup();
        Row("Short", {}, outside);
    });
    session.Frames(3);
    CHECK(a.control.Min.x == b.control.Min.x);
    CHECK(a.control.Min.x > outside.control.Min.x);  // wider than the token column
    CHECK_FALSE(b.layout.truncated);
    // At most the configured share of the width.
    CHECK(a.control.Min.x - a.layout.label.Min.x <= 800.0f * facet::internal::kRowGroupMaxShare + 1.0f);
}

TEST_CASE("Rows: description, help text and error lines add to the row")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe plain;
    RowProbe help;
    RowProbe error;
    RowProbe described;
    session.SetUi([&] {
        Row("Plain", {}, plain);
        Row("Help", {.helpText = "Applies after a restart."}, help);
        Row("Error", {.error = "Enter a value from 1 to 10.", .helpText = "Replaced by the error."}, error);
        Row("Described", {.description = "A second line that explains the setting in more words."}, described);
    });
    session.Frames(2);
    CHECK(help.row.GetHeight() > plain.row.GetHeight());
    CHECK(error.row.GetHeight() > plain.row.GetHeight());
    CHECK(described.row.GetHeight() > plain.row.GetHeight());
    // The control keeps its place: messages go under it.
    CHECK(error.control.Min.y == doctest::Approx(error.row.Min.y));
}

TEST_CASE("Rows: a description beside a free control column wraps across the row")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(800.0f, 400.0f)});
    REQUIRE(session.Ready());
    // About 330 pixels of footnote text: two lines or more in the label column, one across the row.
    const char* text = "The pill then moves at once, and the list keeps its place while you scroll.";
    RowProbe free;
    RowProbe withHelp;
    session.SetUi([&] {
        facet::internal::BeginRowGroup("group");
        Row("A", {.description = text}, free);
        Row("B", {.description = text, .helpText = "Applies after a restart."}, withHelp);
        facet::internal::EndRowGroup();
    });
    session.Frames(3);
    const facet::Theme& t = facet::T();
    const float footnote = t.type[facet::TextStyle::Footnote].size;
    const float control = facet::ControlHeight(facet::ControlSize::Regular);
    // One footnote line under a control of the regular height.
    CHECK(free.row.GetHeight() < control + 1.5f * footnote);
    // With help text under the control, the description keeps to the label column and wraps more.
    CHECK(withHelp.row.GetHeight() > control + 2.0f * footnote);
}

TEST_CASE("Rows: a leading icon moves the label, and a group lines up every label")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(800.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe icon;
    RowProbe plain;
    RowProbe outside;
    float origin = 0.0f;
    session.SetUi([&] {
        origin = ImGui::GetCursorScreenPos().x;
        facet::internal::BeginRowGroup("group");
        Row("Volume", {.icon = facet::Icon("search")}, icon);
        Row("Pitch", {}, plain);
        facet::internal::EndRowGroup();
        Row("Pitch", {}, outside);
    });
    session.Frames(3);
    const facet::Theme& t = facet::T();
    const float iconRoom = std::round(t.type[facet::TextStyle::Body].size) + t.spacing.xs;
    CHECK(icon.layout.label.Min.x == doctest::Approx(origin + iconRoom));
    CHECK(plain.layout.label.Min.x == doctest::Approx(icon.layout.label.Min.x));
    CHECK(outside.layout.label.Min.x == doctest::Approx(origin));
    // The label column holds icon, label and gap, so the label is not cut.
    CHECK_FALSE(icon.layout.truncated);
    CHECK(icon.control.Min.x >= icon.layout.label.Max.x + t.spacing.sm - 0.5f);
}

TEST_CASE("Rows: a disabled row draws its texts in the disabled color")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    // The colors of the vertices a row draws before its control, enabled and disabled.
    const auto rowColors = [](bool disabled, bool* reported) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (disabled)
            ImGui::BeginDisabled();
        const int from = drawList->VtxBuffer.Size;
        *reported = BeginRow("Volume", {.description = "Second line.", .modified = true}).disabled;
        ImVector<ImU32> colors;
        for (int i = from; i < drawList->VtxBuffer.Size; ++i)
            colors.push_back(drawList->VtxBuffer[i].col);
        ImGui::Button("##value");
        EndRow();
        if (disabled)
            ImGui::EndDisabled();
        return colors;
    };
    ImVector<ImU32> enabled;
    ImVector<ImU32> disabled;
    bool enabledReported = true;
    bool disabledReported = false;
    session.SetUi([&] {
        enabled = rowColors(false, &enabledReported);
        disabled = rowColors(true, &disabledReported);
    });
    session.Frames(2);
    const facet::Colors& c = facet::T().colors;
    const ImU32 label = facet::ToU32(c.label);
    const ImU32 secondary = facet::ToU32(c.labelSecondary);
    const ImU32 tertiary = facet::ToU32(c.labelTertiary);
    CHECK_FALSE(enabledReported);
    CHECK(disabledReported);
    CHECK(enabled.contains(label));
    CHECK(enabled.contains(secondary));
    CHECK_FALSE(disabled.contains(label));
    CHECK_FALSE(disabled.contains(secondary));
    CHECK(disabled.contains(tertiary));
}

TEST_CASE("Rows: the reset button keeps its slot and reports activation")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe probe;
    bool modified = false;
    int resets = 0;
    session.SetUi([&] {
        Row("Scale", {.modified = modified, .resettable = true}, probe);
        resets += probe.reset ? 1 : 0;
    });
    session.Frames(2);
    const float widthUnmodified = probe.itemWidth;
    modified = true;
    session.Frame();
    CHECK(probe.itemWidth == widthUnmodified);  // the slot was reserved
    const float h = facet::ControlHeight(facet::ControlSize::Regular);
    const ImVec2 button(probe.layout.control.Max.x + facet::Px(facet::T().spacing.sm) + h * 0.5f,
                        probe.layout.control.Min.y + h * 0.5f);
    session.Click(button);
    CHECK(resets == 1);
    modified = false;
    session.Click(button);
    CHECK(resets == 1);  // not shown while unmodified
}

TEST_CASE("Rows: every row of a group keeps the reset slot of a resettable one")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(600.0f, 400.0f)});
    REQUIRE(session.Ready());
    RowProbe plain;
    RowProbe resettable;
    RowProbe outside;
    session.SetUi([&] {
        facet::internal::BeginRowGroup("group");
        Row("Plain", {}, plain);
        Row("Scale", {.resettable = true}, resettable);
        facet::internal::EndRowGroup();
        Row("Outside", {}, outside);
    });
    session.Frames(3);
    // Nothing is modified, so no reset button shows, but the controls end in one line.
    CHECK(plain.control.Max.x == doctest::Approx(resettable.control.Max.x));
    CHECK(plain.layout.control.Max.x == doctest::Approx(resettable.layout.control.Max.x));
    // Rows outside the group keep the whole width.
    CHECK(outside.control.Max.x > resettable.control.Max.x);
}

TEST_CASE("Rows: each row scopes its control's id by the label")
{
    LayoutGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    RowProbe a;
    RowProbe b;
    session.SetUi([&] {
        Row("First", {}, a);
        Row("Second", {}, b);
    });
    session.Frame();
    CHECK(a.controlId != 0);
    CHECK(a.controlId != b.controlId);
}

TEST_CASE("Rows: rows and groups left open are reported and cleared at EndFrame")
{
    LayoutGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        facet::internal::BeginRowGroup("open");
        BeginRow("Open");
        // Closes the Dear ImGui groups the row opened, but not the row itself.
        ImGui::PopItemWidth();
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::EndGroup();
    });
    const int before = facet::internal::UsageErrorCount();
    session.Frame();
    CHECK(facet::internal::UsageErrorCount() == before + 2);
    session.SetUi([] {});
    const int after = facet::internal::UsageErrorCount();
    session.Frame();
    CHECK(facet::internal::UsageErrorCount() == after);
    // EndRow without BeginRow is reported too.
    session.SetUi([] { EndRow(); });
    session.Frame();
    CHECK(facet::internal::UsageErrorCount() == after + 1);
}

TEST_CASE("Controls of one column share their trailing column from the frame before")
{
    LayoutGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    float narrow = 0.0f;
    float wide = 0.0f;
    float otherEdge = 0.0f;
    float otherKind = 0.0f;
    session.SetUi([&] {
        const float right = ImGui::GetCursorScreenPos().x + 300.0f;
        narrow = facet::internal::RowTrailingColumn(right, 20.0f);
        wide = facet::internal::RowTrailingColumn(right, 50.0f);
        // A control that ends elsewhere, or a column of another kind, is a column of its own.
        otherEdge = facet::internal::RowTrailingColumn(right + 40.0f, 20.0f);
        otherKind = facet::internal::RowTrailingColumn(right, 30.0f, 1);
    });
    session.Frame();
    // In the first frame the narrow control has not seen the wide one yet.
    CHECK(narrow == doctest::Approx(20.0f));
    CHECK(wide == doctest::Approx(50.0f));
    CHECK(otherEdge == doctest::Approx(20.0f));
    session.Frame();
    // From the next frame on both take the widest.
    CHECK(narrow == doctest::Approx(50.0f));
    CHECK(wide == doctest::Approx(50.0f));
    CHECK(otherEdge == doctest::Approx(20.0f));
    CHECK(otherKind == doctest::Approx(30.0f));
    // A column nobody asks for any more is forgotten.
    session.SetUi([&] {
        const float right = ImGui::GetCursorScreenPos().x + 300.0f;
        narrow = facet::internal::RowTrailingColumn(right, 20.0f);
    });
    session.Frame();
    session.Frame();
    CHECK(narrow == doctest::Approx(20.0f));
}