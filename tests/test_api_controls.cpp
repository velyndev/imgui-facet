// The placeholder helper the controls stubs share (src/widgets/controls_stub.h): the caption
// rule, the caption position, and the tooltip of a shortened caption. The API of each controls
// family is tested in its own file (tests/test_api_buttons.cpp, test_api_selection.cpp,
// test_api_sliders.cpp, test_api_inputs_text.cpp, test_api_inputs_value.cpp,
// test_api_pickers_color.cpp, test_api_pickers_date.cpp), so these tests call the helpers
// directly and do not depend on any
// widget still being a placeholder.

#include <algorithm>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"
#include "internal/planned_widget.h"
#include "widgets/controls_stub.h"

TEST_CASE("The placeholder text keeps the label once the whole caption does not fit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string wide;
    std::string medium;
    std::string label;
    std::string tiny;
    std::string none;
    std::string name;
    std::string nameTiny;
    std::string low;
    std::string unlabelled;
    float lowNameWidth = 0.0f;
    float lowCaptionWidth = 0.0f;
    bool shortened[5] = {};
    session.SetUi([&] {
        namespace stub = facet::internal::controls;
        facet::PushTextStyle(facet::TextStyle::Footnote);
        char text[stub::kStubCaptionSize];
        const float fullWidth = ImGui::CalcTextSize("RangeSlider: planned (Destructive)").x;
        const float shortWidth = ImGui::CalcTextSize("RangeSlider: planned").x;
        const float labelWidth = ImGui::CalcTextSize("Destructive").x;
        const float nameWidth = ImGui::CalcTextSize("RangeSlider").x;
        shortened[0] = stub::StubCaption("RangeSlider", "Destructive##range", fullWidth + 1.0f, text, sizeof(text));
        wide = text;
        // Room for "RangeSlider: planned" but not the whole text: still the label, so boxes side
        // by side in one row all show their labels.
        shortened[1] = stub::StubCaption("RangeSlider", "Destructive##range", shortWidth + 1.0f, text, sizeof(text));
        medium = text;
        shortened[2] = stub::StubCaption("RangeSlider", "Destructive##range", labelWidth + 1.0f, text, sizeof(text));
        label = text;
        // A label narrower than the widget name, with room for the name: the label wins.
        lowNameWidth = nameWidth;
        lowCaptionWidth = ImGui::CalcTextSize("RangeSlider: planned (Low)").x;
        stub::StubCaption("RangeSlider", "Low##range", nameWidth + 1.0f, text, sizeof(text));
        low = text;
        stub::StubCaption("RangeSlider", "##range", shortWidth + 1.0f, text, sizeof(text));
        unlabelled = text;
        shortened[3] = stub::StubCaption("RangeSlider", "Destructive##range", labelWidth * 0.6f, text, sizeof(text));
        tiny = text;
        shortened[4] = stub::StubCaption("RangeSlider", "Destructive##range", 0.0f, text, sizeof(text));
        none = text;
        // Without a visible label: the widget name, then the name shortened.
        stub::StubCaption("RangeSlider", "##range", nameWidth + 1.0f, text, sizeof(text));
        name = text;
        stub::StubCaption("RangeSlider", "##range", nameWidth * 0.6f, text, sizeof(text));
        nameTiny = text;
        facet::PopTextStyle();
    });
    session.Frame();

    CHECK(wide == "RangeSlider: planned (Destructive)");
    CHECK(medium == "Destructive");
    CHECK(label == "Destructive");
    REQUIRE(lowCaptionWidth > lowNameWidth + 1.0f);
    CHECK(low == "Low");
    CHECK(unlabelled == "RangeSlider: planned");
    CHECK(tiny.size() > 0);
    CHECK(tiny.compare(0, 3, "Des") == 0);
    CHECK(tiny.size() < std::string("Destructive").size());
    CHECK(none.empty());
    CHECK(name == "RangeSlider");
    CHECK(nameTiny.compare(0, 3, "Ran") == 0);
    CHECK(nameTiny.size() < std::string("RangeSlider").size());
    CHECK_FALSE(shortened[0]);
    CHECK(shortened[1]);
    CHECK(shortened[2]);
    CHECK(shortened[3]);
    CHECK(shortened[4]);
}

TEST_CASE("Every placeholder group shortens its caption with the shared rule and keeps it on the first line")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    float textRight = 0.0f;
    float textTop = 0.0f;
    float boxTop = 0.0f;
    float boxRight = 0.0f;
    session.SetUi([&] {
        // A narrow, tall content placeholder: its caption may not run past the box and sits on
        // the first line.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const int from = drawList->VtxBuffer.Size;
        facet::internal::PlannedWidget("ImageView", "Placeholder image##view", 100.0f, 120.0f);
        boxTop = pos.y;
        boxRight = pos.x + 100.0f;
        float top = 1e9f;
        float right = -1e9f;
        // The box is drawn first (fill and hairline); the caption glyphs come after them.
        const ImU32 caption = facet::ToU32(facet::T().colors.labelSecondary);
        for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        {
            const ImDrawVert& v = drawList->VtxBuffer[i];
            if (v.col != caption)
                continue;
            top = std::min(top, v.pos.y);
            right = std::max(right, v.pos.x);
        }
        textTop = top;
        textRight = right;
    });
    session.Frame();
    const float line = facet::ControlHeight(facet::ControlSize::Small);
    CHECK(textTop < boxTop + line);
    CHECK(textRight <= boxRight - facet::Px(facet::T().spacing.sm) + 0.5f);
}

TEST_CASE("A shortened placeholder shows its whole text in a tooltip on hover")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        namespace stub = facet::internal::controls;
        // A box one control height wide is far too narrow for its text.
        const float height = facet::ControlHeight(facet::ControlSize::Regular);
        stub::ControlStub("IconButton", "Pin##narrow", height, height);
        session.Track("narrow");
        stub::ControlStub("TextField", "Name##wide", ImGui::GetContentRegionAvail().x, height);
        session.Track("wide");
    });
    session.Frame();
    const auto tooltipShown = [] {
        ImGuiWindow* tooltip = ImGui::FindWindowByName("##Tooltip_00");
        return tooltip != nullptr && tooltip->Active;
    };
    session.MouseMove(session.ItemCenter("wide"));
    session.Frame();
    CHECK_FALSE(tooltipShown());
    session.MouseMove(session.ItemCenter("narrow"));
    session.Frame();
    CHECK(tooltipShown());
}