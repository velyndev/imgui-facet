// Behaviour of the text-like inputs of facet/widgets/inputs.h.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_inputs_text.cpp.

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/text_field.h"
#include "support/session.h"
#include "widgets/inputs/text_inputs_shared.h"

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

// ---- SecureField -------------------------------------------------------------------------------

namespace
{

// The center of the trailing button `index` (0 the rightmost) of a field frame at control size
// Regular, as the text field core places slot and clear buttons.
ImVec2 TrailingButtonCenter(const ImRect& frame, int index = 0)
{
    const float gap = facet::Px(facet::T().spacing.xs);
    const float side = facet::ControlHeight(facet::ControlSize::Regular) - 2.0f * gap;
    return ImVec2(frame.Max.x - gap - side * 0.5f - static_cast<float>(index) * (side + gap), frame.GetCenter().y);
}

// The caret x of the active field `id`, relative to the start of its text: masked text advances by
// the password advance, shown text by the glyphs.
float CaretOffset(ImGuiID id, float textLeft)
{
    ImRect caret;
    if (!facet::internal::TextFieldCaretRect(id, &caret))
        return -1.0f;
    return caret.Min.x - textLeft;
}

} // namespace

TEST_CASE("SecureField writes per keystroke and never puts its text on the clipboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string secret;
    Signals signals;
    ImRect frame;
    session.SetUi([&] {
        signals.Count(facet::SecureField("##secret", &secret));
        session.Track("field");
    });
    session.Frame();
    frame = session.ItemRect("field");
    session.Click(ImVec2(frame.Min.x + 20.0f, frame.GetCenter().y));
    session.Type("hunter2");
    CHECK(secret == "hunter2");
    CHECK(signals.writes == 1);  // the session types the whole text in one frame
    session.SetClipboard("unchanged");
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "unchanged");
    // Shown: still no copy, and no cut.
    session.Click(TrailingButtonCenter(frame));
    session.Frames(2);
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_X | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_Insert | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "unchanged");
    CHECK(secret == "hunter2");
    // Plain Delete still deletes the selection.
    session.KeyPress(ImGuiKey_Delete);
    CHECK(secret.empty());
}

TEST_CASE("The SecureField reveal button shows the text until the focus leaves the field")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string secret;
    std::string other;
    ImGuiID id = 0;
    ImRect frame;
    session.SetUi([&] {
        facet::SecureField("##secret", &secret);
        session.Track("field");
        id = ImGui::GetID("##secret");
        facet::TextField("##other", &other);
        session.Track("other");
    });
    session.Frame();
    frame = session.ItemRect("field");
    const float textLeft = frame.Min.x + facet::Px(facet::T().field.padding);
    session.Click(ImVec2(frame.Min.x + 20.0f, frame.GetCenter().y));
    session.Type("iiii");
    const float masked = CaretOffset(id, textLeft);
    REQUIRE(masked > 0.0f);
    // A click on the eye shows the text and gives the keyboard back to the field.
    session.Click(TrailingButtonCenter(frame));
    session.Frames(2);
    const float shown = CaretOffset(id, textLeft);
    REQUIRE(shown > 0.0f);
    CHECK(shown != doctest::Approx(masked));
    session.Type("i");
    CHECK(secret == "iiiii");
    // The keyboard reaches the button with Tab; Space hides the text again.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == ImGui::GetIDWithSeed("reveal", nullptr, id));
    session.KeyPress(ImGuiKey_Space);
    session.Click(ImVec2(frame.Min.x + 20.0f, frame.GetCenter().y));
    session.KeyPress(ImGuiKey_End);
    CHECK(CaretOffset(id, textLeft) == doctest::Approx(masked * 5.0f / 4.0f));
    // Shown, then the focus goes to another field: back to dots.
    session.Click(TrailingButtonCenter(frame));
    session.Frames(2);
    session.Click(session.ItemCenter("other"));
    session.Click(ImVec2(frame.Min.x + 20.0f, frame.GetCenter().y));
    session.KeyPress(ImGuiKey_End);
    CHECK(CaretOffset(id, textLeft) == doctest::Approx(masked * 5.0f / 4.0f));
}

TEST_CASE("SecureField with Hold shows the text only while the button is held, and None has no button")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string held = "iiii";
    std::string none = "iiii";
    ImGuiID heldId = 0;
    ImGuiID noneId = 0;
    float glyphRight = 0.0f;  // right edge of the glyphs the held field drew this frame
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int first = drawList->VtxBuffer.Size;
        facet::SecureField("##held", &held, {.reveal = facet::SecureReveal::Hold});
        session.Track("held");
        heldId = ImGui::GetID("##held");
        // Text glyphs are textured quads in the label color; the frame and icons are not both.
        const ImVec2 white = drawList->_Data->TexUvWhitePixel;
        const ImU32 textRgb = facet::ToU32(facet::T().colors.label) & ~IM_COL32_A_MASK;
        const float iconLeft = session.ItemRect("held").Max.x - facet::ControlHeight(facet::ControlSize::Regular);
        glyphRight = 0.0f;
        for (int i = first; i < drawList->VtxBuffer.Size; ++i)
        {
            const ImDrawVert& v = drawList->VtxBuffer[i];
            if ((v.uv.x == white.x && v.uv.y == white.y) || (v.col & ~IM_COL32_A_MASK) != textRgb || v.pos.x > iconLeft)
                continue;
            glyphRight = std::max(glyphRight, v.pos.x);
        }
        facet::SecureField("##none", &none, {.reveal = facet::SecureReveal::None});
        session.Track("none");
        noneId = ImGui::GetID("##none");
    });
    session.Frame();
    const ImRect heldFrame = session.ItemRect("held");
    const ImRect noneFrame = session.ItemRect("none");
    const float pad = facet::Px(facet::T().field.padding);
    // Hold: the button belongs to the slot; while it is down the field is not active, so the
    // shown state is read from the draw list: the dots use the bullet glyph's advance.
    session.Click(ImVec2(heldFrame.Min.x + 20.0f, heldFrame.GetCenter().y));
    session.KeyPress(ImGuiKey_End);
    const float masked = CaretOffset(heldId, heldFrame.Min.x + pad);
    REQUIRE(masked > 0.0f);
    const float dotsRight = glyphRight;
    REQUIRE(dotsRight > heldFrame.Min.x);
    // While the button is down the field is not active; the drawn glyphs show the text instead
    // of the wider dots ("i" is narrower than the bullet advance).
    session.MouseMove(TrailingButtonCenter(heldFrame));
    session.MouseDown();
    session.Frames(2);
    CHECK(glyphRight < dotsRight - 1.0f);
    session.MouseUp();
    session.Frames(2);
    CHECK(glyphRight == doctest::Approx(dotsRight));
    // Released: the keyboard is back in the field and the text is dots again.
    session.KeyPress(ImGuiKey_End);
    CHECK(CaretOffset(heldId, heldFrame.Min.x + pad) == doctest::Approx(masked));
    // None: a click at the end of the frame lands in the text, no button takes it.
    session.Click(TrailingButtonCenter(noneFrame));
    CHECK(ImGui::GetActiveID() == noneId);
    CHECK(CaretOffset(noneId, noneFrame.Min.x + pad) == doctest::Approx(masked));
}

TEST_CASE("SecureField shows the Caps Lock hint while it is focused and letters arrive in upper case")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string secret;
    std::string plain;
    session.SetUi([&] {
        facet::SecureField("##secret", &secret);
        session.Track("field");
        facet::SecureField("##plain", &plain, {.capsLockHint = false});
        session.Track("plain");
    });
    session.Frame();
    const float rest = session.ItemRect("field").GetHeight();
    session.Click(session.ItemCenter("field"));
    session.Type("ab");
    session.Frame();
    CHECK(session.ItemRect("field").GetHeight() == doctest::Approx(rest));
    session.Type("CD");  // upper case without Shift: Caps Lock is on
    session.Frame();
    CHECK(secret == "abCD");
    CHECK(facet::internal::text_inputs::CapsLockLikelyOn());
    const float hinted = session.ItemRect("field").GetHeight();
    CHECK(hinted > rest);
    session.KeyPress(ImGuiKey_CapsLock);  // flips what is known
    session.Frame();
    CHECK(session.ItemRect("field").GetHeight() == doctest::Approx(rest));
    session.KeyPress(ImGuiKey_CapsLock);
    session.Frame();
    CHECK(session.ItemRect("field").GetHeight() == doctest::Approx(hinted));
    // Without the option, or without focus, nothing shows.
    session.Click(session.ItemCenter("plain"));
    session.Type("EF");
    session.Frame();
    CHECK(session.ItemRect("plain").GetHeight() == doctest::Approx(rest));
    CHECK(session.ItemRect("field").GetHeight() == doctest::Approx(rest));
    session.KeyPress(ImGuiKey_Escape);
}

TEST_CASE("The SecureField strength meter sits under the frame and the message line under it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string secret = "abc";
    float meter = 0.0f;
    float message = 0.0f;
    session.SetUi([&] {
        meter = facet::PxSnapped(facet::T().metrics.progressHeightSmall) + facet::Px(facet::T().spacing.xs);
        facet::SecureField("##plain", &secret);
        session.Track("plain");
        facet::SecureField("##meter", &secret, {.strength = 0.5f});
        session.Track("meter");
        facet::SecureField("##both", &secret, {.strength = 0.9f, .helpText = "Message"});
        session.Track("both");
        facet::SecureField("##help", &secret, {.helpText = "Message"});
        session.Track("help");
    });
    session.Frame();
    const float plain = session.ItemRect("plain").GetHeight();
    message = session.ItemRect("help").GetHeight() - plain;
    CHECK(message > 0.0f);
    CHECK(session.ItemRect("meter").GetHeight() == doctest::Approx(plain + meter).epsilon(0.02));
    CHECK(session.ItemRect("both").GetHeight() == doctest::Approx(plain + meter + message).epsilon(0.02));
}

TEST_CASE("A read-only SecureField takes no text and a null text returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string secret = "abc";
    int writes = 0;
    session.SetUi([&] {
        writes += facet::SecureField("##ro", &secret, {.readOnly = true}) ? 1 : 0;
        session.Track("ro");
        writes += facet::SecureField("##null", static_cast<std::string*>(nullptr)) ? 1 : 0;
        session.Track("null");
    });
    session.Frame();
    session.Click(session.ItemCenter("ro"));
    session.Type("x");
    session.KeyPress(ImGuiKey_Backspace);
    session.Click(session.ItemCenter("null"));
    session.Type("y");
    CHECK(secret == "abc");
    CHECK(writes == 0);
    CHECK_FALSE(facet::SecureField("##outside", &secret));
}

// ---- SearchField -------------------------------------------------------------------------------

namespace
{

const char* const kRecentQueries[] = {"Option 1", "Option 2", "Option 3"};

// The completion list of the last tracked field, as seen inside the latest frame (the popup host
// answers only inside a frame). Each test tracks it in every frame, so nothing carries over.
struct CompletionSeen
{
    bool open = false;
    ImRect rect;
};
CompletionSeen gCompletionSeen;

// Call inside the ui code, after the field.
void TrackCompletion(ImGuiID fieldId)
{
    const ImGuiID level = ImGui::GetIDWithSeed("##completion", nullptr, fieldId);
    gCompletionSeen.open = facet::internal::IsPopupLevelOpen(level);
    gCompletionSeen.rect = facet::internal::PopupLevelRect(level);
}

bool CompletionOpen()
{
    return gCompletionSeen.open;
}

// The center of row `row` of the open list (rows of the Regular size inside the level's padding).
ImVec2 CompletionRowCenter(int row)
{
    const ImRect& rect = gCompletionSeen.rect;
    const float pad = facet::Px(facet::T().spacing.xs);
    const float height = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    return ImVec2(rect.GetCenter().x, rect.Min.y + pad + height * (static_cast<float>(row) + 0.5f));
}

} // namespace

TEST_CASE("SearchField: Escape empties a query as an edit, then ends the session")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    Signals signals;
    ImGuiID id = 0;
    session.SetUi([&] {
        signals.Count(facet::SearchField("##search", &query));
        session.Track("field");
        id = ImGui::GetID("##search");
        TrackCompletion(id);
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Type("abc");
    session.KeyPress(ImGuiKey_Escape);
    CHECK(query.empty());
    CHECK(ImGui::GetActiveID() == id);
    CHECK(signals.cancels == 0);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(ImGui::GetActiveID() != id);
    CHECK(query.empty());
    CHECK(signals.cancels == 0);
}

TEST_CASE("SearchField lists recent entries under the empty focused field; Down and Enter take one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    int writes = 0;
    ImGuiID id = 0;
    session.SetUi([&] {
        writes += facet::SearchField("##search", &query, {.recent = kRecentQueries}) ? 1 : 0;
        session.Track("field");
        id = ImGui::GetID("##search");
        TrackCompletion(id);
    });
    session.Frame();
    CHECK_FALSE(CompletionOpen());
    session.Click(session.ItemCenter("field"));
    session.Frames(2);
    CHECK(CompletionOpen());
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(query == "Option 2");
    CHECK(writes == 1);
    CHECK(ImGui::GetActiveID() == id);  // Enter went to the list, the field keeps the keyboard
    session.Frames(2);
    CHECK_FALSE(CompletionOpen());
    session.Type("x");
    CHECK(query == "Option 2x");  // the caret moved after the accepted text
}

TEST_CASE("SearchField: a click on a recent entry takes it and gives the keyboard back to the field")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    ImGuiID id = 0;
    session.SetUi([&] {
        facet::SearchField("##search", &query, {.recent = kRecentQueries});
        session.Track("field");
        id = ImGui::GetID("##search");
        TrackCompletion(id);
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Frames(3);
    REQUIRE(CompletionOpen());
    session.Click(CompletionRowCenter(2));
    CHECK(query == "Option 3");
    session.Frames(2);
    CHECK(ImGui::GetActiveID() == id);
    session.Type("!");
    CHECK(query == "Option 3!");
}

TEST_CASE("SearchField: Escape closes the recent list first, Enter and Tab with no active row reach the field")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    std::string next;
    ImGuiID id = 0;
    session.SetUi([&] {
        facet::SearchField("##search", &query, {.recent = kRecentQueries});
        session.Track("field");
        id = ImGui::GetID("##search");
        TrackCompletion(id);
        facet::TextField("##next", &next);
        session.Track("next");
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Frames(2);
    REQUIRE(CompletionOpen());
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(CompletionOpen());
    CHECK(ImGui::GetActiveID() == id);
    session.Frames(2);
    CHECK_FALSE(CompletionOpen());  // stays closed until the next edit
    session.Type("a");
    session.KeyPress(ImGuiKey_Backspace);
    session.Frames(2);
    CHECK(CompletionOpen());  // edited back to empty: open again
    session.KeyPress(ImGuiKey_Tab);
    CHECK(query.empty());
    CHECK(ImGui::GetFocusID() == session.ItemId("next"));
    // Enter with no active row ends the edit.
    session.Click(session.ItemCenter("field"));
    session.Frames(2);
    REQUIRE(CompletionOpen());
    session.KeyPress(ImGuiKey_Enter);
    CHECK(query.empty());
    CHECK(ImGui::GetActiveID() != id);
}

TEST_CASE("SearchField: a registered focus shortcut gives the field the keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ShortcutId shortcut = facet::RegisterShortcut("test.search.focus", ImGuiMod_Ctrl | ImGuiKey_F);
    REQUIRE(shortcut.IsValid());
    std::string query = "abc";
    ImGuiID id = 0;
    session.SetUi([&] {
        facet::SearchField("##search", &query, {.focusShortcut = shortcut});
        id = ImGui::GetID("##search");
        TrackCompletion(id);
    });
    session.Frames(2);
    session.KeyPress(ImGuiKey_F | ImGuiMod_Ctrl);
    session.Frames(2);
    CHECK(ImGui::GetActiveID() == id);
    session.Type("x");
    CHECK(query == "x");  // the query was selected, as after Tab
    facet::UnregisterShortcut(shortcut);
}

TEST_CASE("A read-only SearchField shows no recent list and takes no text")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string query;
    int writes = 0;
    ImGuiID id = 0;
    session.SetUi([&] {
        writes += facet::SearchField("##search", &query, {.recent = kRecentQueries, .readOnly = true}) ? 1 : 0;
        session.Track("field");
        id = ImGui::GetID("##search");
        TrackCompletion(id);
    });
    session.Frame();
    session.Click(session.ItemCenter("field"));
    session.Frames(2);
    CHECK_FALSE(CompletionOpen());
    session.Type("x");
    CHECK(query.empty());
    CHECK(writes == 0);
}

// ---- TextArea ----------------------------------------------------------------------------------

TEST_CASE("TextArea: Enter starts a new line and Ctrl+Enter commits the session")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    Signals signals;
    session.SetUi([&] {
        signals.Count(facet::TextArea("##notes", &text));
        session.Track("area");
    });
    session.Frame();
    session.Click(session.ItemCenter("area"));
    session.Type("one");
    session.KeyPress(ImGuiKey_Enter);
    session.Type("two");
    CHECK(text == "one\ntwo");
    CHECK(signals.commits == 0);
    session.KeyPress(ImGuiKey_Enter | ImGuiMod_Ctrl);
    CHECK(signals.commits == 1);
    CHECK(text == "one\ntwo");
}

TEST_CASE("TextArea draws its label above the frame and keeps the label out of the frame width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    session.SetUi([&] {
        facet::TextArea("##plain", &text);
        session.Track("plain");
        facet::TextArea("Notes", &text);
        session.Track("labelled");
    });
    session.Frame();
    const ImRect plain = session.ItemRect("plain");
    const ImRect labelled = session.ItemRect("labelled");
    CHECK(labelled.GetWidth() == doctest::Approx(plain.GetWidth()));
    CHECK(labelled.GetHeight() > plain.GetHeight());
}

TEST_CASE("TextArea grows with its lines up to maxLines and stays fixed without it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string growing;
    std::string fixed;
    session.SetUi([&] {
        facet::TextArea("##growing", &growing, {.minLines = 2, .maxLines = 4});
        session.Track("growing");
        facet::TextArea("##fixed", &fixed, {.minLines = 2});
        session.Track("fixed");
    });
    session.Frame();
    const float growingRest = session.ItemRect("growing").GetHeight();
    const float fixedRest = session.ItemRect("fixed").GetHeight();
    CHECK(growingRest == doctest::Approx(fixedRest));
    growing = "1\n2\n3";
    fixed = "1\n2\n3";
    session.Frames(2);
    const float three = session.ItemRect("growing").GetHeight();
    CHECK(three > growingRest);
    CHECK(session.ItemRect("fixed").GetHeight() == doctest::Approx(fixedRest));
    growing = "1\n2\n3\n4\n5\n6\n7";
    session.Frames(2);
    const float capped = session.ItemRect("growing").GetHeight();
    CHECK(capped > three);
    growing += "\n8\n9";
    session.Frames(2);
    CHECK(session.ItemRect("growing").GetHeight() == doctest::Approx(capped));
}

TEST_CASE("TextArea: allowTab types a tab, maxLength stops typing, the counter adds its line")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string tabbed;
    std::string limited;
    std::string counted;
    session.SetUi([&] {
        facet::TextArea("##tabbed", &tabbed, {.allowTab = true});
        session.Track("tabbed");
        facet::TextArea("##limited", &limited, {.maxLength = 5});
        session.Track("limited");
        facet::TextArea("##counted", &counted, {.counter = true, .maxLength = 5});
        session.Track("counted");
    });
    session.Frame();
    session.Click(session.ItemCenter("tabbed"));
    session.Type("a");
    session.KeyPress(ImGuiKey_Tab);
    session.Type("b");
    CHECK(tabbed == "a\tb");
    session.Click(session.ItemCenter("limited"));
    session.Type("abcdefgh");
    CHECK(limited == "abcde");
    CHECK(session.ItemRect("counted").GetHeight() > session.ItemRect("limited").GetHeight());
}

TEST_CASE("TextArea: dragging the grip of a resizable area changes and keeps its height")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    session.SetUi([&] {
        facet::TextArea("##resizable", &text, {.resizable = true});
        session.Track("area");
    });
    session.Frame();
    const ImRect before = session.ItemRect("area");
    const ImVec2 grip(before.GetCenter().x, before.Max.y);
    session.Drag({grip, ImVec2(grip.x, grip.y + 60.0f)});
    session.Frames(2);
    const float after = session.ItemRect("area").GetHeight();
    CHECK(after == doctest::Approx(before.GetHeight() + 60.0f).epsilon(0.05));
    // Never below minLines.
    const ImVec2 moved(grip.x, session.ItemRect("area").Max.y);
    session.Drag({moved, ImVec2(moved.x, moved.y - 400.0f)});
    session.Frames(2);
    CHECK(session.ItemRect("area").GetHeight() == doctest::Approx(before.GetHeight()));
    CHECK(text.empty());
}

TEST_CASE("A read-only TextArea takes no text and the buffer form cuts at its size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string fixed = "fixed";
    char buffer[6] = {};
    int writes = 0;
    session.SetUi([&] {
        writes += facet::TextArea("##ro", &fixed, {.readOnly = true}) ? 1 : 0;
        session.Track("ro");
        facet::TextArea("##buffer", buffer, sizeof(buffer));
        session.Track("buffer");
    });
    session.Frame();
    session.Click(session.ItemCenter("ro"));
    session.Type("x");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(fixed == "fixed");
    CHECK(writes == 0);
    session.Click(session.ItemCenter("buffer"));
    session.Type("abcdefgh");
    CHECK(std::string(buffer) == "abcde");
}
