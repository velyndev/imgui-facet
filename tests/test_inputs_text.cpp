// Behaviour of the text-like inputs of facet/widgets/inputs.h.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_inputs_text.cpp.

#include <array>
#include <cstring>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"

namespace
{

// What one widget call reported, counted over the frames.
struct Signals
{
    int writes = 0;
    int commits = 0;
    int cancels = 0;

    void Count(bool written)
    {
        writes += written ? 1 : 0;
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
    }
};

} // namespace

// ---- TextField ---------------------------------------------------------------------------------

TEST_CASE("TextField writes per keystroke and Enter commits the session once")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    Signals signals;
    session.SetUi([&] {
        signals.Count(facet::TextField("Name", &text));
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("a");
    CHECK(text == "a");
    session.Type("b");
    CHECK(text == "ab");
    CHECK(signals.writes == 2);
    CHECK(signals.commits == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(signals.commits == 1);
    CHECK(signals.cancels == 0);
    CHECK(text == "ab");
    CHECK(signals.writes == 2);
}

TEST_CASE("TextField restores the text from activation on Escape and reports a cancel")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "start";
    Signals signals;
    session.SetUi([&] {
        signals.Count(facet::TextField("Name", &text));
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("x");
    REQUIRE(text == "startx");
    const int writesBefore = signals.writes;
    session.KeyPress(ImGuiKey_Escape);
    CHECK(text == "start");
    CHECK(signals.writes == writesBefore + 1);
    CHECK(signals.cancels == 1);
    CHECK(signals.commits == 0);
    CHECK_FALSE(ImGui::IsAnyItemActive());
}

TEST_CASE("TextField commits once when Tab moves the focus away")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string first;
    std::string second;
    Signals signals;
    session.SetUi([&] {
        signals.Count(facet::TextField("First", &first));
        session.Track("first");
        facet::TextField("Second", &second);
        session.Track("second");
    });
    session.Frame();
    session.Click(session.ItemCenter("first"));
    session.Type("abc");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(signals.commits == 1);
    CHECK(ImGui::GetFocusID() == session.ItemId("second"));
    session.Type("z");
    CHECK(second == "z");
    CHECK(first == "abc");
    CHECK(signals.commits == 1);
}

TEST_CASE("A read-only TextField can be selected and copied but never changes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "fixed";
    Signals signals;
    session.SetUi([&] {
        signals.Count(facet::TextField("Name", &text, {.readOnly = true}));
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("abc");
    session.KeyPress(ImGuiKey_Backspace);
    CHECK(text == "fixed");
    CHECK(signals.writes == 0);
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "fixed");
    session.KeyPress(ImGuiKey_X | ImGuiMod_Ctrl);
    CHECK(text == "fixed");
}

TEST_CASE("A pushed read-only item flag keeps TextField from writing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "fixed";
    int writes = 0;
    session.SetUi([&] {
        ImGui::PushItemFlag(ImGuiItemFlags_ReadOnly, true);
        writes += facet::TextField("Name", &text) ? 1 : 0;
        session.Track("field");
        ImGui::PopItemFlag();
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("abc");
    CHECK(text == "fixed");
    CHECK(writes == 0);
}

TEST_CASE("TextField with a null text or an empty buffer is shown read-only and returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char buffer[4] = "abc";
    int writes = 0;
    session.SetUi([&] {
        writes += facet::TextField("Null", static_cast<std::string*>(nullptr)) ? 1 : 0;
        session.Track("null");
        writes += facet::TextField("Empty", buffer, 0) ? 1 : 0;
        session.Track("empty");
    });
    session.Frame();
    session.Click(session.ItemCenter("null"));
    session.Type("x");
    session.Click(session.ItemCenter("empty"));
    session.Type("y");
    CHECK(writes == 0);
    CHECK(std::strcmp(buffer, "abc") == 0);
}

TEST_CASE("The buffer form of TextField cuts typed text at the buffer size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<char, 8> buffer = {};  // seven characters and the terminator
    int writes = 0;
    session.SetUi([&] {
        writes += facet::TextField("Code", buffer.data(), buffer.size()) ? 1 : 0;
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("abcdefghij");
    CHECK(std::string(buffer.data()) == "abcdefg");
    CHECK(buffer.back() == '\0');
    CHECK(writes >= 1);
}

TEST_CASE("TextField stops at maxLength code points, also for a paste")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    session.SetUi([&] {
        facet::TextField("Name", &text, {.maxLength = 4});
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("\xC3\xA4" "bcdef");  // a two-byte character first
    CHECK(text == "\xC3\xA4" "bcd");
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.SetClipboard("123456");
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(text == "1234");
}

TEST_CASE("The TextField clear button empties the text with one write and one committed edit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "abc";
    Signals signals;
    ImGuiID fieldId = 0;
    session.SetUi([&] {
        signals.Count(facet::TextField("Name", &text, {.clearButton = true}));
        session.Track("field");
        fieldId = ImGui::GetID("Name");
    });
    session.Frame();
    const ImRect field = session.ItemRect("field");
    // The clear button sits at the end of the frame, centered on it.
    const float side = facet::ControlHeight(facet::ControlSize::Regular) - 2.0f * facet::Px(facet::T().spacing.xs);
    session.Click(ImVec2(field.Max.x - facet::Px(facet::T().spacing.xs) - side * 0.5f, field.GetCenter().y));
    CHECK(text.empty());
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
    session.Frames(2);
    // The keyboard went back to the text.
    session.Type("z");
    CHECK(text == "z");
    CHECK(ImGui::GetActiveID() == fieldId);
}

TEST_CASE("A read-only TextField hides the clear button")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "abc";
    session.SetUi([&] {
        facet::TextField("Name", &text, {.clearButton = true, .readOnly = true});
        session.Track("field");
    });
    session.Frame();
    const ImRect field = session.ItemRect("field");
    const float side = facet::ControlHeight(facet::ControlSize::Regular) - 2.0f * facet::Px(facet::T().spacing.xs);
    session.Click(ImVec2(field.Max.x - facet::Px(facet::T().spacing.xs) - side * 0.5f, field.GetCenter().y));
    CHECK(text == "abc");
}

namespace
{

struct CallbackLog
{
    int completion = 0;
    int edit = 0;
    int always = 0;
};

int LogTextCallback(ImGuiInputTextCallbackData* data)
{
    CallbackLog& log = *static_cast<CallbackLog*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
        ++log.completion;
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
        ++log.edit;
    else if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
        ++log.always;
    return 0;
}

} // namespace

TEST_CASE("TextField forwards only the callback events it was asked for")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    CallbackLog log;
    session.SetUi([&] {
        // Password is not a callback event: it is dropped, so the text stays visible and copyable.
        facet::TextField("Name", &text,
                         {.callback = LogTextCallback,
                          .callbackUser = &log,
                          .callbackEvents = ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_Password});
        session.Track("field");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("ab");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(log.completion == 1);
    CHECK(log.edit == 0);
    CHECK(log.always == 0);
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "ab");
}

TEST_CASE("A disabled TextField is no Tab stop, takes no text and returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string before;
    std::string text = "abc";
    std::string after;
    int writes = 0;
    session.SetUi([&] {
        facet::TextField("Before", &before);
        session.Track("before");
        ImGui::BeginDisabled();
        writes += facet::TextField("Disabled", &text) ? 1 : 0;
        session.Track("disabled");
        ImGui::EndDisabled();
        facet::TextField("After", &after);
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("disabled"));
    session.Type("x");
    CHECK(text == "abc");
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(writes == 0);
}

TEST_CASE("TextField writes on Enter only when the caller turns live edits off")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    Signals signals;
    session.SetUi([&] {
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputText, false);
        signals.Count(facet::TextField("Name", &text));
        session.Track("field");
        ImGui::PopItemFlag();
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("abc");
    CHECK(text.empty());
    CHECK(signals.writes == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(text == "abc");
    CHECK(signals.writes == 1);
    CHECK(signals.commits == 1);
}

TEST_CASE("TextField takes its width in the order of the conventions")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    float units = 0.0f;
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        units = facet::Px(1.0f);
        facet::TextField("Default", &text);
        session.Track("default");
        facet::TextField("Units", &text, {.width = 200.0f});
        session.Track("units");
        facet::TextField("Fill", &text, {.width = facet::kFill});
        session.Track("fill");
        facet::TextField("Minus", &text, {.width = -50.0f});
        session.Track("minus");
        ImGui::SetNextItemWidth(150.0f);
        facet::TextField("Next", &text);
        session.Track("next");
        // The option wins over SetNextItemWidth, and the request does not leak to the next item.
        ImGui::SetNextItemWidth(150.0f);
        facet::TextField("Both", &text, {.width = 100.0f});
        session.Track("both");
        facet::TextField("After", &text);
        session.Track("after");
        ImGui::PushItemWidth(120.0f);
        facet::TextField("Pushed", &text);
        session.Track("pushed");
        ImGui::PopItemWidth();
    });
    session.Frame();

    CHECK(session.ItemRect("default").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(200.0f * units));
    CHECK(session.ItemRect("fill").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("minus").GetWidth() == doctest::Approx(available - 50.0f * units));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(150.0f));
    CHECK(session.ItemRect("both").GetWidth() == doctest::Approx(100.0f * units));
    CHECK(session.ItemRect("after").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("pushed").GetWidth() == doctest::Approx(120.0f));
}

TEST_CASE("TextField called outside a frame returns false and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "abc";
    char buffer[8] = "abc";
    CHECK_FALSE(facet::TextField("Name", &text));
    CHECK_FALSE(facet::TextField("Name", buffer, sizeof(buffer)));
    CHECK(text == "abc");
    CHECK(std::strcmp(buffer, "abc") == 0);
}

TEST_CASE("TextField leaves the Dear ImGui stacks and the item width as it found them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "abc";
    bool same = false;
    session.SetUi([&] {
        ImGuiContext& g = *GImGui;
        const int ids = g.CurrentWindow->IDStack.Size;
        const int colors = g.ColorStack.Size;
        const int vars = g.StyleVarStack.Size;
        const int fonts = g.FontStack.Size;
        const int flags = g.ItemFlagsStack.Size;
        const float width = ImGui::CalcItemWidth();
        facet::TextField("Name", &text, {.icon = facet::Icon("search"), .clearButton = true, .loading = true,
                                         .error = "Message"});
        same = ids == g.CurrentWindow->IDStack.Size && colors == g.ColorStack.Size && vars == g.StyleVarStack.Size &&
               fonts == g.FontStack.Size && flags == g.ItemFlagsStack.Size && width == ImGui::CalcItemWidth();
        session.Track("field");
    });
    session.Frame();
    CHECK(same);
    session.Click(session.ItemCenter("field"));
    session.Frame();
    CHECK(same);
}
