// Behaviour of the number-like inputs of facet/widgets/inputs.h.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks are in tests/test_api_inputs_value.cpp. The number core underneath
// (typing, parsing, steps, drag mode) has its own tests in tests/test_text_field.cpp; the cases
// here check what each widget adds: its type, its options and its own parts.

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/control.h"
#include "internal/focus.h"
#include "support/session.h"

namespace
{

// Counts what the item queries report after a widget call.
struct Signals
{
    int writes = 0;
    int commits = 0;
    int cancels = 0;

    void Take(bool written)
    {
        writes += written ? 1 : 0;
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
    }
};

// A point inside the text of a field that spans `rect`, clear of the steppers at its end.
ImVec2 TextPoint(const ImRect& rect)
{
    return ImVec2(rect.Min.x + rect.GetHeight() * 0.5f, rect.GetCenter().y);
}

bool ErrorShown(const char* text)
{
    for (const facet::internal::FieldErrorRecord& record : facet::internal::PreviousFrameFieldErrors())
        if (std::strcmp(record.text, text) == 0)
            return true;
    return false;
}

} // namespace

// ---- NumberInput -------------------------------------------------------------------------------

TEST_CASE("NumberInput writes a typed int on Enter, once, clamped to its range")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 5;
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::NumberInput("##count", &value, {.min = 0, .max = 10}));
        session.Track("field");
        ImGui::Button("next");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("8");
    CHECK(value == 5);  // typed numbers wait for the commit
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 8);
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("250");
    session.KeyPress(ImGuiKey_Tab);
    session.Frame();
    CHECK(value == 10);
    CHECK(signals.writes == 2);
    CHECK(signals.commits == 2);
}

TEST_CASE("NumberInput keeps an unbounded int inside the int range when typing and stepping")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 0;
    session.SetUi([&] {
        facet::NumberInput("##wide", &value);
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("99999999999");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == INT_MAX);
    session.Click(TextPoint(session.ItemRect("field")));
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(value == INT_MAX);  // no wrap to INT_MIN
    session.Type("-99999999999");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == INT_MIN);
    session.Click(TextPoint(session.ItemRect("field")));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Ctrl);
    CHECK(value == INT_MIN);
}

TEST_CASE("NumberInput reads a comma decimal, an exponent and keeps the value for empty text")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double value = 1.0;
    session.SetUi([&] {
        facet::NumberInput("##real", &value, {.format = "%.4f"});
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("2,5");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 2.5);
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("1e-3");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == doctest::Approx(0.001));
    session.Click(TextPoint(session.ItemRect("field")));
    session.KeyPress(ImGuiKey_Delete);  // everything was selected: the text is empty now
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == doctest::Approx(0.001));
}

TEST_CASE("NumberInput shows Strings::invalidValue after a text it cannot read until edited again")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 3.0f;
    const char* const invalid = facet::GetStrings().invalidValue;
    session.SetUi([&] {
        facet::NumberInput("##rejected", &value);
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("-");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(value == 3.0f);
    CHECK(ErrorShown(invalid));
    // Activating the field again takes the message away; a valid number keeps it away.
    session.Click(TextPoint(session.ItemRect("field")));
    session.Frame();
    CHECK_FALSE(ErrorShown(invalid));
    session.Type("4");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(value == 4.0f);
    CHECK_FALSE(ErrorShown(invalid));
}

TEST_CASE("NumberInput steppers and keys step at once with Shift finer and Ctrl coarser")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 1.0f;
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::NumberInput("##step", &value, {.step = 1.0, .stepFast = 10.0}));
        session.Track("field");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("field");
    session.Click(ImVec2(rect.Max.x - rect.GetHeight() * 0.5f, rect.GetCenter().y));  // +
    CHECK(value == 2.0f);
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
    session.Click(TextPoint(rect));
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Shift);
    CHECK(value == doctest::Approx(2.1f));
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Ctrl);
    CHECK(value == doctest::Approx(12.1f));
    session.KeyPress(ImGuiKey_Escape);  // back to the value at activation, written and reported
    CHECK(value == 2.0f);
    CHECK(signals.cancels == 1);
}

TEST_CASE("NumberInput takes the wheel only while it has the keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 4;
    session.SetUi([&] {
        facet::NumberInput("##wheel", &value);
        session.Track("field");
    });
    session.Frame();
    const ImVec2 at = TextPoint(session.ItemRect("field"));
    session.Wheel(at, 1.0f);
    CHECK(value == 4);
    session.Click(at);
    session.Wheel(at, 1.0f);
    CHECK(value == 5);
}

TEST_CASE("NumberInput read-only, null and disabled fields never write")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 4;
    int disabledValue = 4;
    bool anyWrite = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        anyWrite |= facet::NumberInput("##readonly", &value, {.readOnly = true});
        session.Track("readonly");
        anyWrite |= facet::NumberInput("##null", static_cast<int*>(nullptr));
        ImGui::BeginDisabled();
        anyWrite |= facet::NumberInput("##disabled", &disabledValue);
        session.Track("disabled");
        ImGui::EndDisabled();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("readonly")));
    session.Type("9");
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Enter);
    const ImRect rect = session.ItemRect("readonly");
    session.Click(ImVec2(rect.Max.x - rect.GetHeight() * 0.5f, rect.GetCenter().y));
    session.Click(TextPoint(session.ItemRect("disabled")));
    session.Type("9");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 4);
    CHECK(disabledValue == 4);
    CHECK_FALSE(anyWrite);
    // Tab passes the disabled field: readonly, null, then after.
    session.Click(session.ItemCenter("before"));
    for (int i = 0; i < 4; ++i)
        session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("NumberInput mixed shows an empty field until the first edit writes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double value = 7.0;
    bool mixed = true;
    session.SetUi([&] {
        if (facet::NumberInput("##mixed", &value, {.mixed = mixed}))
            mixed = false;
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    ImGuiInputTextState* text = ImGui::GetInputTextState(session.ItemId("field"));
    REQUIRE(text != nullptr);
    CHECK(text->TextLen == 0);
    session.Type("12");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 12.0);
    CHECK_FALSE(mixed);
}

// ---- DragNumber --------------------------------------------------------------------------------

TEST_CASE("DragNumber drags by pointer travel without drift and writes the caller's type")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 10;
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::DragNumber("##drag", &value, {.speed = 0.5}));
        session.Track("field");
    });
    session.Frame();
    const ImVec2 start = session.ItemCenter("field");
    const float unit = facet::Px(1.0f);
    session.Drag({start, ImVec2(start.x + 40.0f * unit, start.y), ImVec2(start.x - 10.0f * unit, start.y), start}, 6);
    CHECK(value == 10);
    session.Drag({start, ImVec2(start.x + 40.0f * unit, start.y)}, 6);
    CHECK(value == 30);
    CHECK(signals.commits == 2);
}

TEST_CASE("DragNumber steps a finite range by a hundredth with Left and Right, and types on Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::DragNumber("##amount", &value, {.min = 0.0, .max = 1.0});
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("field"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(value == doctest::Approx(0.51f));
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Ctrl);
    CHECK(value == doctest::Approx(0.41f));
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    session.Type("2");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 1.0f);  // clamped to the range
}

TEST_CASE("DragNumber wraps a finite range when asked and keeps an open float inside float")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int angle = 350;
    float open = 0.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::DragNumber("##angle", &angle, {.min = 0.0, .max = 360.0, .wrap = true});
        session.Track("angle");
        facet::DragNumber("##open", &open);
        session.Track("open");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);  // ten steps of 3.6, rounded: 4 each
    CHECK(angle >= 0);
    CHECK(angle < 360);
    CHECK(angle != 350);
    session.Click(session.ItemCenter("open"));
    session.Frame();
    session.Type("1e300");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(std::isfinite(open));
    CHECK(open == FLT_MAX);
}
