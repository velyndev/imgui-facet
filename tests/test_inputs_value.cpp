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
#include "internal/keys.h"
#include "internal/popup_host.h"
#include "internal/text_field_number.h"
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

// ---- KeybindCapture ----------------------------------------------------------------------------

namespace
{

// The slot rects of a KeybindCapture without a label: the primary slot starts at the widget's
// left edge; with the alternate slot each takes half the width less the gap.
ImVec2 PrimarySlot(const ImRect& widget)
{
    return ImVec2(widget.Min.x + widget.GetHeight() * 0.5f, widget.Min.y + widget.GetHeight() * 0.5f);
}

} // namespace

TEST_CASE("KeybindCapture listens after a click, owns the keys and writes the chord on release")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind bind;
    Signals signals;
    bool appSawK = false;
    const facet::ShortcutId shortcut = facet::RegisterShortcut("test.capture.leak", ImGuiKey_K | ImGuiMod_Ctrl);
    bool shortcutFired = false;
    session.SetUi([&] {
        signals.Take(facet::KeybindCapture("##bind", &bind));
        session.Track("bind");
        appSawK |= ImGui::IsKeyPressed(ImGuiKey_K);
        shortcutFired |= facet::ShortcutPressed(shortcut);
    });
    session.Frames(2);
    session.Click(PrimarySlot(session.ItemRect("bind")));
    CHECK(ImGui::GetIO().WantCaptureKeyboard);
    CHECK(facet::internal::ActiveChordCaptureOwner() != 0);
    session.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    session.Frame();
    CHECK(bind.primary == (ImGuiKey_K | ImGuiMod_Ctrl));
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
    CHECK_FALSE(appSawK);
    CHECK_FALSE(shortcutFired);
    CHECK_FALSE(ImGui::IsAnyItemActive());
    facet::UnregisterShortcut(shortcut);
}

TEST_CASE("KeybindCapture: Escape cancels, a click elsewhere cancels, the clear button clears")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind bind{.primary = ImGuiKey_F5};
    session.SetUi([&] {
        facet::KeybindCapture("##bind", &bind);
        session.Track("bind");
        ImGui::Button("elsewhere");
        session.Track("elsewhere");
    });
    session.Frames(2);
    const ImRect rect = session.ItemRect("bind");
    session.Click(PrimarySlot(rect));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK(bind.primary == ImGuiKey_F5);
    CHECK_FALSE(ImGui::IsAnyItemActive());
    session.Click(PrimarySlot(rect));
    REQUIRE(ImGui::IsAnyItemActive());
    session.Click(session.ItemCenter("elsewhere"));
    CHECK(bind.primary == ImGuiKey_F5);
    CHECK(ImGui::GetActiveID() != session.ItemId("bind"));
    // The clear button sits at the end of the slot.
    session.Click(ImVec2(rect.Max.x - rect.GetHeight() * 0.5f, rect.GetCenter().y));
    CHECK(bind.primary == ImGuiKey_None);
}

TEST_CASE("KeybindCapture: Enter on the focused slot listens; Backspace and Delete clear it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind bind{.primary = ImGuiKey_F5, .alternate = ImGuiKey_F6};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::KeybindCapture("##bind", &bind, {.showAlternate = true});
        session.Track("bind");
    });
    session.Frames(2);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);  // the primary slot
    session.KeyPress(ImGuiKey_Delete);
    CHECK(bind.primary == ImGuiKey_None);
    CHECK(bind.alternate == ImGuiKey_F6);
    session.KeyPress(ImGuiKey_Tab);  // the alternate slot
    session.KeyPress(ImGuiKey_Backspace);
    CHECK(bind.alternate == ImGuiKey_None);
    session.KeyPress(ImGuiKey_Enter);
    session.KeyPress(ImGuiKey_G);
    session.Frame();
    CHECK(bind.alternate == ImGuiKey_G);
    CHECK(bind.primary == ImGuiKey_None);
}

TEST_CASE("KeybindCapture takes modifiers alone only when allowed, and mouse buttons when allowed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind keys;
    facet::Keybind modifiers;
    facet::Keybind mouse;
    session.SetUi([&] {
        facet::KeybindCapture("##keys", &keys, {.allowMouse = false});
        session.Track("keys");
        facet::KeybindCapture("##modifiers", &modifiers, {.allowModifierOnly = true});
        session.Track("modifiers");
        facet::KeybindCapture("##mouse", &mouse);
        session.Track("mouse");
    });
    session.Frames(2);
    session.Click(PrimarySlot(session.ItemRect("keys")));
    session.KeyPress(ImGuiKey_LeftShift);
    CHECK(keys.primary == ImGuiKey_None);  // still listening
    session.Click(session.ItemCenter("keys"), ImGuiMouseButton_Right);
    CHECK(keys.primary == ImGuiKey_None);  // mouse buttons are not taken here
    session.KeyPress(ImGuiKey_Escape);

    session.Click(PrimarySlot(session.ItemRect("modifiers")));
    session.KeyPress(ImGuiKey_LeftCtrl | ImGuiMod_Ctrl);  // a backend sends the key and its modifier
    session.Frame();
    CHECK(facet::ChordMods(modifiers.primary) == ImGuiMod_Ctrl);

    session.Click(PrimarySlot(session.ItemRect("mouse")));
    REQUIRE(facet::internal::ActiveChordCaptureOwner() == ImGui::GetActiveID());
    session.Click(session.ItemCenter("mouse"), ImGuiMouseButton_Middle);
    session.Frame();
    CHECK(mouse.primary == ImGuiKey_MouseMiddle);
}

TEST_CASE("KeybindCapture shows a conflict the caller found with FindChordConflict")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId save = facet::RegisterShortcut("test.capture.save", ImGuiKey_S | ImGuiMod_Ctrl,
                                                           {.label = "Save"});
    facet::Keybind bind;
    session.SetUi([&] {
        const facet::ShortcutId conflict = facet::FindChordConflict(bind.primary);
        facet::KeybindCapture("##bind", &bind, {.error = conflict.IsValid() ? "Used by Save" : nullptr});
        session.Track("bind");
    });
    session.Frames(2);
    CHECK_FALSE(ErrorShown("Used by Save"));
    session.Click(PrimarySlot(session.ItemRect("bind")));
    session.KeyPress(ImGuiKey_S | ImGuiMod_Ctrl);
    session.Frames(2);
    CHECK(bind.primary == (ImGuiKey_S | ImGuiMod_Ctrl));
    CHECK(ErrorShown("Used by Save"));
    facet::UnregisterShortcut(save);
}

TEST_CASE("KeybindCapture read-only and disabled never listen or write")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Keybind readOnly{.primary = ImGuiKey_F1};
    facet::Keybind disabled{.primary = ImGuiKey_F2};
    session.SetUi([&] {
        facet::KeybindCapture("##readonly", &readOnly, {.readOnly = true});
        session.Track("readonly");
        ImGui::BeginDisabled();
        facet::KeybindCapture("##disabled", &disabled);
        session.Track("disabled");
        ImGui::EndDisabled();
    });
    session.Frames(2);
    session.Click(PrimarySlot(session.ItemRect("readonly")));
    session.KeyPress(ImGuiKey_A);
    session.KeyPress(ImGuiKey_Delete);
    session.Click(PrimarySlot(session.ItemRect("disabled")));
    session.KeyPress(ImGuiKey_B);
    CHECK(readOnly.primary == ImGuiKey_F1);
    CHECK(disabled.primary == ImGuiKey_F2);
    CHECK(facet::internal::ActiveChordCaptureOwner() == 0);
}

// ---- UnitInput ---------------------------------------------------------------------------------

namespace
{

const facet::UnitItem kTimeUnits[] = {{"ms", 0.001}, {"s", 1.0}, {"min", 60.0}};

} // namespace

TEST_CASE("UnitInput shows the base value in the chosen unit and reads a typed suffix")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double seconds = 0.25;
    int unit = 0;  // ms
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::UnitInput("##time", &seconds, &unit, kTimeUnits));
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    ImGuiInputTextState* text = ImGui::GetInputTextState(session.ItemId("field"));
    REQUIRE(text != nullptr);
    CHECK(std::string(text->TextA.Data) == "250");
    session.Type("1.5 s");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == doctest::Approx(1.5));
    CHECK(unit == 1);
    CHECK(signals.commits == 1);
    // Case does not matter when no unit matches exactly; a plain number is in the shown unit.
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("2MIN");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == doctest::Approx(120.0));
    CHECK(unit == 2);
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("3");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == doctest::Approx(180.0));
}

TEST_CASE("UnitInput round-trips its own formatted text and rejects unknown suffixes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double meters = 1234.5;
    int unit = 1;
    const facet::UnitItem lengths[] = {{"m", 1.0}, {"km", 1000.0}};
    session.SetUi([&] {
        facet::UnitInput("##length", &meters, &unit, lengths, {.format = "%.4f km"});
        session.Track("field");
    });
    session.Frame();
    // The text the format shows, typed back as it is, gives the same value.
    char shown[64];
    facet::internal::FormatNumber(shown, 64, meters / 1000.0, "%.4f km", nullptr, nullptr, false, false);
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type(shown);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(meters == doctest::Approx(1234.5));
    CHECK(unit == 1);
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("12 parsecs");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(meters == doctest::Approx(1234.5));
    CHECK(ErrorShown(facet::GetStrings().invalidValue));
}

TEST_CASE("UnitInput menu switches the unit by mouse and keyboard, converting or keeping the number")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double converted = 2.0;
    int convertedUnit = 1;
    double kept = 2.0;
    int keptUnit = 1;
    session.SetUi([&] {
        facet::UnitInput("##converted", &converted, &convertedUnit, kTimeUnits);
        session.Track("converted");
        facet::UnitInput("##kept", &kept, &keptUnit, kTimeUnits, {.convert = false});
        session.Track("kept");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("converted");
    const ImVec2 button(rect.Max.x - rect.GetHeight(), rect.GetCenter().y);
    session.Click(button);
    REQUIRE(GImGui->OpenPopupStack.Size == 1);  // the menu (the level queries need a frame)
    // The rows follow the units: Down from the first row moves to "s", then "min".
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(convertedUnit == 2);
    CHECK(converted == 2.0);  // the quantity stays
    const ImRect keptRect = session.ItemRect("kept");
    session.Click(ImVec2(keptRect.Max.x - keptRect.GetHeight(), keptRect.GetCenter().y));
    REQUIRE(GImGui->OpenPopupStack.Size == 1);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Enter);  // the first row: ms
    session.Frame();
    CHECK(keptUnit == 0);
    CHECK(kept == doctest::Approx(0.002));  // the number 2 stays, now in ms
}

TEST_CASE("UnitInput without a unit index shows the first unit and converts typed suffixes into it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double seconds = 1.0;
    const facet::UnitItem units[] = {{"s", 1.0}, {"ms", 0.001}};
    session.SetUi([&] {
        facet::UnitInput("##fixed", &seconds, nullptr, units);
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    session.Type("500ms");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == doctest::Approx(0.5));
}
