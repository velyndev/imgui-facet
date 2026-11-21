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
#include <span>
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

// ---- VectorInput -------------------------------------------------------------------------------

namespace
{

// The rect of field `index` of a side-by-side VectorInput without a label spanning `widget`.
ImRect VectorField(const ImRect& widget, int index, int count)
{
    const float gap = std::round(facet::Px(facet::T().spacing.sm));
    const float width = std::floor((widget.GetWidth() - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
    const float x = widget.Min.x + static_cast<float>(index) * (width + gap);
    return ImRect(x, widget.Min.y, x + width, widget.Min.y + facet::ControlHeight(facet::ControlSize::Regular));
}

} // namespace

TEST_CASE("VectorInput: each component is a Tab stop in order and an edit writes one component once")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float v[3] = {1.0f, 2.0f, 3.0f};
    Signals signals;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        signals.Take(facet::VectorInput("##v", std::span<float>(v), {.linkToggle = true}));
        session.Track("vector");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    ImGuiID focus[5] = {};
    for (ImGuiID& f : focus)
    {
        session.KeyPress(ImGuiKey_Tab);
        f = ImGui::GetFocusID();
    }
    // X, Y, Z, the link button, then the next item.
    CHECK(focus[0] != focus[1]);
    CHECK(focus[1] != focus[2]);
    CHECK(focus[2] != focus[3]);
    CHECK(focus[4] == session.ItemId("after"));
    // Typing into Y writes Y alone, with one write and one commit.
    const ImRect rect = session.ItemRect("vector");
    session.Click(VectorField(rect, 1, 3).GetCenter());
    session.Frame();
    session.Type("7");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(v[0] == 1.0f);
    CHECK(v[1] == 7.0f);
    CHECK(v[2] == 3.0f);
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
}

TEST_CASE("VectorInput: the axis label drags its component; the link scales every component alike")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int v[3] = {2, 4, 6};
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::VectorInput("##v", std::span<int>(v), {.speed = 1.0, .linkToggle = true}));
        session.Track("vector");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("vector");
    const ImRect x = VectorField(rect, 0, 3);
    const ImVec2 axis(x.Min.x + facet::Px(4.0f), x.GetCenter().y);
    session.Drag({axis, ImVec2(axis.x + 10.0f * facet::Px(1.0f), axis.y)}, 6);
    CHECK(v[0] == 12);
    CHECK(v[1] == 4);
    // The link button sits at the end of the last field.
    const ImRect z = VectorField(rect, 2, 3);
    session.Click(ImVec2(z.Max.x - z.GetHeight() * 0.5f, z.GetCenter().y));
    const int writesBefore = signals.writes;
    session.Click(x.GetCenter());
    session.Frame();
    session.Type("24");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(v[0] == 24);
    CHECK(v[1] == 8);
    CHECK(v[2] == 12);
    CHECK(signals.writes == writesBefore + 1);
}

TEST_CASE("VectorInput stacks its fields when narrow, shows more than four read-only, and takes ImVec2")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float wide[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    ImVec2 point(0.5f, 0.25f);
    float narrowHeight = 0.0f;
    session.SetUi([&] {
        facet::VectorInput("##narrow", &point, {.width = 80.0f});
        narrowHeight = ImGui::GetItemRectSize().y;
        session.Track("narrow");
        facet::VectorInput("##wide", std::span<float>(wide));
        session.Track("wide");
    });
    session.Frame();
    CHECK(narrowHeight > facet::ControlHeight(facet::ControlSize::Regular) * 1.5f);
    // The second field sits under the first: type into it.
    const ImRect rect = session.ItemRect("narrow");
    session.Click(ImVec2(rect.GetCenter().x, rect.Max.y - facet::ControlHeight(facet::ControlSize::Regular) * 0.5f));
    session.Frame();
    session.Type("2");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(point.x == 0.5f);
    CHECK(point.y == 2.0f);
    const ImRect wideRect = session.ItemRect("wide");
    session.Click(VectorField(wideRect, 0, 4).GetCenter());
    session.Type("9");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(wide[0] == 1.0f);
}

TEST_CASE("VectorInput normalize keeps a float vector at length 1 after an edit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 v(1.0f, 0.0f, 0.0f, 0.0f);
    session.SetUi([&] {
        facet::VectorInput("##n", &v, {.normalize = true});
        session.Track("vector");
    });
    session.Frame();
    session.Click(VectorField(session.ItemRect("vector"), 1, 4).GetCenter());
    session.Frame();
    session.Type("1");
    session.KeyPress(ImGuiKey_Enter);
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    CHECK(length == doctest::Approx(1.0f));
    CHECK(v.x == doctest::Approx(v.y));
}

// ---- Rating ------------------------------------------------------------------------------------

namespace
{

// The center of icon `index` (0 based) of a Rating without a label at the left of `rect`, or a
// point `fraction` across it.
ImVec2 RatingIcon(const ImRect& rect, int index, float fraction = 0.5f)
{
    const float box = rect.GetHeight();
    return ImVec2(rect.Min.x + box * (static_cast<float>(index) + fraction), rect.GetCenter().y);
}

} // namespace

TEST_CASE("Rating: a click writes the icon under the pointer, a second click on it clears")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int stars = 0;
    float halves = 0.0f;
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::Rating("##stars", &stars));
        session.Track("stars");
        facet::Rating("##halves", &halves, {.halfSteps = true});
        session.Track("halves");
    });
    session.Frame();
    session.MouseMove(RatingIcon(session.ItemRect("stars"), 3));
    session.Frames(2);
    CHECK(stars == 0);  // hovering only previews
    session.Click(RatingIcon(session.ItemRect("stars"), 3));
    CHECK(stars == 4);
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
    session.Click(RatingIcon(session.ItemRect("stars"), 3));
    CHECK(stars == 0);
    session.Click(RatingIcon(session.ItemRect("halves"), 2, 0.25f));
    CHECK(halves == 2.5f);
    session.Click(RatingIcon(session.ItemRect("halves"), 2, 0.75f));
    CHECK(halves == 3.0f);
}

TEST_CASE("Rating: one Tab stop; Left and Right step, Home and End go to 0 and count")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int stars = 2;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Rating("##stars", &stars, {.count = 7});
        session.Track("stars");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("stars"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(stars == 3);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(stars == 1);
    session.KeyPress(ImGuiKey_End);
    CHECK(stars == 7);
    session.KeyPress(ImGuiKey_Home);
    CHECK(stars == 0);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("Rating fits a short row by shrinking its icons; read-only and disabled never write")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float rating = 2.0f;
    int disabled = 1;
    float available = 0.0f;
    float wideRating = 0.0f;
    session.SetUi([&] {
        facet::Rating("Rating##wide", &rating, {.count = 10, .readOnly = true});
        wideRating = GImGui->LastItemData.Rect.GetWidth();
        session.Track("readonly");
        ImGui::BeginDisabled();
        facet::Rating("##disabled", &disabled);
        session.Track("disabled");
        ImGui::EndDisabled();
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(200.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::Rating("Rating", &rating, {.count = 10});
        session.Track("short");
        ImGui::EndChild();
    });
    session.Frame();
    REQUIRE(available > 1.0f);
    CHECK(wideRating > available);
    CHECK(session.ItemRect("short").GetWidth() == doctest::Approx(available));
    session.Click(session.ItemCenter("readonly"));
    session.Click(session.ItemCenter("disabled"));
    CHECK(rating == 2.0f);
    CHECK(disabled == 1);
}

// ---- SizeInput ---------------------------------------------------------------------------------

namespace
{

// The width (0) or height (1) field of a SizeInput without a label spanning `widget`.
ImRect SizeField(const ImRect& widget, int index)
{
    return VectorField(widget, index, 2);
}

// The center of slot button `index` (0 the rightmost) at the end of `field`.
ImVec2 SlotButton(const ImRect& field, int index)
{
    const float gap = std::round(facet::Px(facet::T().spacing.xs));
    const float side = field.GetHeight() - gap * 2.0f;
    return ImVec2(field.Max.x - gap - side * 0.5f - static_cast<float>(index) * (side + gap), field.GetCenter().y);
}

} // namespace

TEST_CASE("SizeInput: Tab goes width, lock, height, swap, presets; the lock keeps the ratio")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int w = 640;
    int h = 480;
    Signals signals;
    const ImVec2 presets[] = {{1920.0f, 1080.0f}, {800.0f, 600.0f}};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        signals.Take(facet::SizeInput("##size", &w, &h, {.swapButton = true, .presets = presets}));
        session.Track("size");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    ImGuiID focus[6] = {};
    for (ImGuiID& f : focus)
    {
        session.KeyPress(ImGuiKey_Tab);
        f = ImGui::GetFocusID();
    }
    for (int i = 0; i < 5; ++i)
        CHECK(focus[i] != focus[i + 1]);
    CHECK(focus[5] == session.ItemId("after"));
    // Lock, then a typed width sets the height in the same frame: one write.
    const ImRect rect = session.ItemRect("size");
    session.Click(SlotButton(SizeField(rect, 0), 0));
    const int writes = signals.writes;
    session.Click(TextPoint(SizeField(rect, 0)));
    session.Type("1280");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(w == 1280);
    CHECK(h == 960);
    CHECK(signals.writes == writes + 1);
    session.Click(TextPoint(SizeField(rect, 1)));
    session.Type("240");
    session.KeyPress(ImGuiKey_Tab);
    session.Frame();
    CHECK(w == 320);
    CHECK(h == 240);
}

TEST_CASE("SizeInput: swap exchanges the values; a preset writes both; values stay in range")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 size(640.0f, 480.0f);
    const ImVec2 presets[] = {{1920.0f, 1080.0f}, {800.0f, 600.0f}};
    session.SetUi([&] {
        facet::SizeInput("##size", &size, {.max = {4096.0f, 4096.0f}, .swapButton = true, .presets = presets});
        session.Track("size");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("size");
    session.Click(SlotButton(SizeField(rect, 1), 1));  // swap
    CHECK(size.x == 480.0f);
    CHECK(size.y == 640.0f);
    session.Click(SlotButton(SizeField(rect, 1), 0));  // presets
    REQUIRE(GImGui->OpenPopupStack.Size == 1);
    session.KeyPress(ImGuiKey_DownArrow);  // shows the keyboard focus in the menu
    session.KeyPress(ImGuiKey_End);        // the last preset
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(size.x == 800.0f);
    CHECK(size.y == 600.0f);
    session.Click(TextPoint(SizeField(rect, 0)));
    session.Type("-20");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(size.x == 0.0f);
    session.Click(TextPoint(SizeField(rect, 0)));
    session.Type("99999");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(size.x == 4096.0f);
}

// ---- DurationInput -----------------------------------------------------------------------------

TEST_CASE("DurationInput types units, colon text and plain seconds, and rejects garbage")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double seconds = 90.0;
    Signals signals;
    session.SetUi([&] {
        signals.Take(facet::DurationInput("##d", &seconds));
        session.Track("field");
    });
    session.Frame();
    const ImVec2 at = TextPoint(session.ItemRect("field"));
    session.Click(at);
    session.Frame();
    ImGuiInputTextState* text = ImGui::GetInputTextState(session.ItemId("field"));
    REQUIRE(text != nullptr);
    CHECK(std::string(text->TextA.Data) == "00:01:30");  // the shown text, all selected
    session.Type("1h2m3s");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == 3723.0);
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
    const struct
    {
        const char* typed;
        double expected;
    } cases[] = {{"1m30", 90.0}, {"01:02:03", 3723.0}, {"2:05", 125.0}, {"2,5 min", 150.0}, {"45", 45.0}, {"0:00:01.5", 1.5}};
    for (const auto& entry : cases)
    {
        CAPTURE(entry.typed);
        session.Click(at);
        session.Frame();
        session.Type(entry.typed);
        session.KeyPress(ImGuiKey_Enter);
        CHECK(seconds == doctest::Approx(entry.expected));
    }
    session.Click(at);
    session.Frame();
    session.Type("soon");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(seconds == doctest::Approx(1.5));
    CHECK(ErrorShown(facet::GetStrings().invalidValue));
}

TEST_CASE("DurationInput round-trips what it shows, with hours off and milliseconds on")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double seconds = 3725.25;
    std::string shown;
    session.SetUi([&] {
        facet::DurationInput("##d", &seconds, {.showHours = false, .showMilliseconds = true, .min = -10000.0});
        session.Track("field");
    });
    session.Frame();
    session.Click(TextPoint(session.ItemRect("field")));
    session.Frame();
    ImGuiInputTextState* text = ImGui::GetInputTextState(session.ItemId("field"));
    REQUIRE(text != nullptr);
    shown = text->TextA.Data;
    CHECK(shown == "62:05.250");
    session.KeyPress(ImGuiKey_Enter);  // unchanged text: nothing written
    session.Click(TextPoint(session.ItemRect("field")));
    session.Frame();
    session.Type(shown.c_str());
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == doctest::Approx(3725.25));
    session.Click(TextPoint(session.ItemRect("field")));
    session.Frame();
    session.Type("-1:30");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == doctest::Approx(-90.0));
}

TEST_CASE("DurationInput: Left and Right pick the segment, Up and Down step it with the carry")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double seconds = 59.0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::DurationInput("##d", &seconds);
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("field"));
    session.KeyPress(ImGuiKey_UpArrow);  // seconds, carried into the minutes
    CHECK(seconds == 60.0);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(seconds == 120.0);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_DownArrow);  // hours, clamped at the minimum of 0
    CHECK(seconds == 0.0);
    // Enter types; Escape leaves the text without writing.
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    session.Type("5");
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK(seconds == 0.0);
    // A typed digit on the focused field starts typing with it.
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_Tab);
    session.Type("7");
    session.Frames(2);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(seconds == 7.0);
}

TEST_CASE("DurationInput: a vertical drag on a segment moves it in whole units")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    double seconds = 90.0;
    float minutesX = 0.0f;
    session.SetUi([&] {
        facet::DurationInput("##d", &seconds);
        session.Track("field");
        facet::PushTextStyle(facet::ControlTextStyle(facet::ControlSize::Regular));
        minutesX = ImGui::GetItemRectMin().x + facet::Px(facet::T().field.padding) + ImGui::CalcTextSize("00:0").x;
        facet::PopTextStyle();
    });
    session.Frame();
    const float y = session.ItemRect("field").GetCenter().y;
    const float travel = facet::Px(facet::T().spacing.sm) * 3.0f;
    session.Drag({ImVec2(minutesX, y), ImVec2(minutesX, y - travel)}, 6);
    CHECK(seconds == doctest::Approx(90.0 + 3.0 * 60.0));
}
