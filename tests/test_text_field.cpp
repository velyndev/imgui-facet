// Text field core (src/internal/text_field.h) and number editing (text_field_number.h): the
// editing that stays Dear ImGui's, the buffers, the edit sessions, the adornments, the geometry
// for other blocks, the requests, number parsing and formatting, and the number field.

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/item.h"
#include "internal/scroll.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "support/session.h"

using facet::ValueScale;
using facet::internal::NumberFieldBehavior;
using facet::internal::NumberFieldParams;
using facet::internal::NumberFieldResult;
using facet::internal::NumberParse;
using facet::internal::ParseNumber;
using facet::internal::TextFieldBehavior;
using facet::internal::TextFieldParams;
using facet::internal::TextFieldResult;

// ---- Allocation counting ---------------------------------------------------------------------
// Global operator new is replaced for the test binary so a case can count the allocations made
// inside a window of code; it counts only while gTextFieldCountAllocations is set.

namespace
{
std::atomic<bool> gTextFieldCountAllocations{false};
std::atomic<int> gTextFieldAllocations{0};
} // namespace

void* operator new(std::size_t size)
{
    if (gTextFieldCountAllocations.load(std::memory_order_relaxed))
        gTextFieldAllocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size != 0 ? size : 1))
        return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void* memory) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{

void* CountingImGuiAlloc(std::size_t size, void*)
{
    if (gTextFieldCountAllocations.load(std::memory_order_relaxed))
        gTextFieldAllocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size);
}

void CountingImGuiFree(void* memory, void*)
{
    std::free(memory);
}

// Motion as the presets have it, the light theme and scale 1 at the end of the case.
struct TextFieldThemeGuard
{
    TextFieldThemeGuard() { facet::SetFollowSystemMotion(false); }
    ~TextFieldThemeGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

// A field under test with everything its call reported, counted over the frames.
struct TestField
{
    std::string text;
    TextFieldParams params;
    TextFieldResult result;
    int edits = 0;
    int commits = 0;
    int cancels = 0;
    int activations = 0;
    int afterEdit = 0;
    int submits = 0;

    void Run(const char* label)
    {
        result = TextFieldBehavior(label, &text, params);
        Count();
    }
    void Count()
    {
        edits += result.edited ? 1 : 0;
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
        activations += ImGui::IsItemActivated() ? 1 : 0;
        afterEdit += ImGui::IsItemDeactivatedAfterEdit() ? 1 : 0;
        submits += result.submitted ? 1 : 0;
    }
    ImVec2 TextPoint(float dx = 2.0f) const { return ImVec2(result.text.Min.x + dx, result.text.GetCenter().y); }
};

// The selection of the active item `id`: start, end, caret, or all -1.
struct Selection
{
    int start = -1;
    int end = -1;
    int caret = -1;
    friend bool operator==(const Selection&, const Selection&) = default;
};

Selection ActiveSelection(ImGuiID id)
{
    Selection selection;
    ImGuiContext& g = *GImGui;
    if (g.ActiveId != id)
        return selection;
    ImGuiInputTextState* state = ImGui::GetInputTextState(id);
    if (state == nullptr)
        return selection;
    selection.start = std::min(state->GetSelectionStart(), state->GetSelectionEnd());
    selection.end = std::max(state->GetSelectionStart(), state->GetSelectionEnd());
    selection.caret = state->GetCursorPos();
    return selection;
}

// The style values and stacks a field call must leave as it found them.
struct StyleSnapshot
{
    ImVec4 colors[ImGuiCol_COUNT];
    ImVec2 framePadding;
    float frameRounding = 0.0f;
    float frameBorderSize = 0.0f;
    ImVec2 itemInnerSpacing;
    float caretSize = 0.0f;
    float scrollbarSize = 0.0f;
    float scrollbarRounding = 0.0f;
    float scrollbarPadding = 0.0f;
    float alpha = 0.0f;
    int colorStack = 0;
    int varStack = 0;
    int fontStack = 0;
    int itemFlagsStack = 0;
    int idStack = 0;
    int groupStack = 0;
    ImGuiItemFlags itemFlags = 0;
    ImGuiID reactivate = 0;
    const char* mixedLabel = nullptr;
    ImFont* font = nullptr;
    float fontSize = 0.0f;
    ImRect clip;
    int drawClipDepth = 0;

    static StyleSnapshot Take()
    {
        ImGuiContext& g = *GImGui;
        StyleSnapshot s;
        std::memcpy(s.colors, g.Style.Colors, sizeof(s.colors));
        s.framePadding = g.Style.FramePadding;
        s.frameRounding = g.Style.FrameRounding;
        s.frameBorderSize = g.Style.FrameBorderSize;
        s.itemInnerSpacing = g.Style.ItemInnerSpacing;
        s.caretSize = g.Style.InputTextCursorSize;
        s.scrollbarSize = g.Style.ScrollbarSize;
        s.scrollbarRounding = g.Style.ScrollbarRounding;
        s.scrollbarPadding = g.Style.ScrollbarPadding;
        s.alpha = g.Style.Alpha;
        s.colorStack = g.ColorStack.Size;
        s.varStack = g.StyleVarStack.Size;
        s.fontStack = g.FontStack.Size;
        s.itemFlagsStack = g.ItemFlagsStack.Size;
        s.idStack = g.CurrentWindow->IDStack.Size;
        s.groupStack = g.GroupStack.Size;
        s.itemFlags = g.CurrentItemFlags;
        s.reactivate = g.InputTextReactivateId;
        s.mixedLabel = g.MixedValueLabel;
        s.font = g.Font;
        s.fontSize = g.FontSize;
        s.clip = g.CurrentWindow->ClipRect;
        s.drawClipDepth = g.CurrentWindow->DrawList->_ClipRectStack.Size;
        return s;
    }
    bool Equals(const StyleSnapshot& o) const
    {
        return std::memcmp(colors, o.colors, sizeof(colors)) == 0 && framePadding.x == o.framePadding.x &&
               framePadding.y == o.framePadding.y && frameRounding == o.frameRounding &&
               frameBorderSize == o.frameBorderSize && itemInnerSpacing.x == o.itemInnerSpacing.x &&
               itemInnerSpacing.y == o.itemInnerSpacing.y && caretSize == o.caretSize && scrollbarSize == o.scrollbarSize &&
               scrollbarRounding == o.scrollbarRounding && scrollbarPadding == o.scrollbarPadding && alpha == o.alpha &&
               colorStack == o.colorStack && varStack == o.varStack && fontStack == o.fontStack &&
               itemFlagsStack == o.itemFlagsStack && idStack == o.idStack && groupStack == o.groupStack &&
               itemFlags == o.itemFlags && reactivate == o.reactivate && mixedLabel == o.mixedLabel && font == o.font &&
               fontSize == o.fontSize && clip.Min.x == o.clip.Min.x && clip.Max.x == o.clip.Max.x &&
               clip.Min.y == o.clip.Min.y && clip.Max.y == o.clip.Max.y && drawClipDepth == o.drawClipDepth;
    }
};

} // namespace

// ---- Editing stays Dear ImGui's ----------------------------------------------------------------

TEST_CASE("Text field: typing writes per keystroke and the item queries see one session")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    session.SetUi([&] { field.Run("##name"); });
    session.Frame();
    session.Click(field.TextPoint());
    CHECK(field.result.active);
    CHECK(field.activations == 1);
    session.Type("abc");
    CHECK(field.text == "abc");
    CHECK(field.edits >= 1);
    CHECK(field.commits == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK_FALSE(field.result.active);
    CHECK(field.submits == 1);
    CHECK(field.commits == 1);
    CHECK(field.afterEdit == 1);
    CHECK(field.cancels == 0);
    // Activating and leaving without an edit commits nothing.
    session.Click(field.TextPoint());
    session.KeyPress(ImGuiKey_Enter);
    CHECK(field.commits == 1);
}

TEST_CASE("Text field: editing matches a stock Dear ImGui field key for key")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char stock[128] = "";
    char facetText[128] = "";
    TextFieldResult facetResult;
    ImGuiID stockId = 0;
    ImRect stockRect;
    session.SetUi([&] {
        ImGui::InputText("##stock", stock, sizeof(stock));
        stockId = ImGui::GetItemID();
        stockRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        facetResult = TextFieldBehavior("##facet", facetText, sizeof(facetText));
    });
    session.Frame();

    // One script, played on each field in turn; the text, selection and clipboard are recorded
    // at every checkpoint.
    struct Checkpoint
    {
        std::string text;
        Selection selection;
        std::string clipboard;
    };
    const auto play = [&](ImVec2 first, ImGuiID id, const char* buffer) {
        std::vector<Checkpoint> out;
        const auto check = [&] { out.push_back({buffer, ActiveSelection(id), session.Clipboard()}); };
        session.SetClipboard("");
        session.Click(first);
        session.Type("hello brave new world");
        check();
        session.KeyPress(ImGuiKey_Home);
        session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);  // word move
        check();
        session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl | ImGuiMod_Shift);  // word select
        check();
        session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
        check();
        session.KeyPress(ImGuiKey_End);
        session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
        check();
        session.KeyPress(ImGuiKey_Z | ImGuiMod_Ctrl);  // undo the paste
        check();
        session.KeyPress(ImGuiKey_Y | ImGuiMod_Ctrl);  // redo it
        check();
        for (int i = 0; i < 3; ++i)
            session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
        session.KeyPress(ImGuiKey_Backspace);
        check();
        session.HoldKey(ImGuiKey_LeftArrow, 0.8f);  // key repeat
        check();
        session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
        check();
        session.KeyPress(ImGuiKey_X | ImGuiMod_Ctrl);
        check();
        session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
        session.DoubleClick(first);  // double click selects the word under the pointer
        check();
        session.KeyPress(ImGuiKey_Enter);
        return out;
    };
    const float inset = ImGui::GetStyle().FramePadding.x + 2.0f;
    const std::vector<Checkpoint> stockRun = play(ImVec2(stockRect.Min.x + inset, stockRect.GetCenter().y), stockId, stock);
    const std::vector<Checkpoint> facetRun = play(ImVec2(facetResult.text.Min.x + 2.0f, facetResult.text.GetCenter().y),
                                                  facetResult.id, facetText);
    REQUIRE(stockRun.size() == facetRun.size());
    for (std::size_t i = 0; i < stockRun.size(); ++i)
    {
        CAPTURE(i);
        CHECK(facetRun[i].text == stockRun[i].text);
        CHECK(facetRun[i].selection == stockRun[i].selection);
        CHECK(facetRun[i].clipboard == stockRun[i].clipboard);
    }
    // The script did something on the way: a word was copied and the double click selected one.
    CHECK(stockRun[3].clipboard.rfind("brave", 0) == 0);
    CHECK(stockRun.back().selection.end > stockRun.back().selection.start);
}

TEST_CASE("Text field: the IME position is Dear ImGui's and matches the caret rect")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.params.width = 120.0f;
    session.SetUi([&] { field.Run("##ime"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("a long line that scrolls sideways inside the field");
    session.Frame();
    const ImGuiPlatformImeData& ime = GImGui->PlatformImeData;
    CHECK(ime.WantVisible);
    ImRect caret;
    REQUIRE(facet::internal::TextFieldCaretRect(field.result.id, &caret));
    CHECK(caret.Min.x - 1.0f == doctest::Approx(ime.InputPos.x));
    CHECK(caret.Min.y == doctest::Approx(ime.InputPos.y));
    // The caret stays inside the text area while the text scrolls.
    CHECK(caret.Min.x >= field.result.text.Min.x - 0.5f);
    CHECK(caret.Min.x <= field.result.text.Max.x + 0.5f);
    CHECK(field.result.caret.Min.x == caret.Min.x);
}

// ---- Style left as it was --------------------------------------------------------------------

TEST_CASE("Text field: the host's style and stacks are unchanged after every kind of call")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "text";
    char buffer[32] = "buffer";
    int mismatches = 0;
    int calls = 0;
    const auto checked = [&](auto&& call) {
        const StyleSnapshot before = StyleSnapshot::Take();
        call();
        mismatches += before.Equals(StyleSnapshot::Take()) ? 0 : 1;
        ++calls;
    };
    bool activateMultiline = false;
    session.SetUi([&] {
        checked([&] { TextFieldBehavior("plain", &text); });
        checked([&] { TextFieldBehavior("##icon", &text, {.icon = facet::Icon("search"), .clearButton = true, .loading = true}); });
        checked([&] { TextFieldBehavior("##secret", &text, {.password = true}); });
        checked([&] { TextFieldBehavior("##area", &text, {.multiline = true}); });
        checked([&] { TextFieldBehavior("##ro", &text, {.readOnly = true, .error = "Message"}); });
        checked([&] { TextFieldBehavior("Floating", &text, {.floatingLabel = true}); });
        checked([&] { TextFieldBehavior("##mixed", buffer, sizeof(buffer), {.mixed = true}); });
        ImGui::BeginDisabled();
        checked([&] { TextFieldBehavior("##disabled", &text); });
        ImGui::EndDisabled();
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        checked([&] { TextFieldBehavior("##pushedmixed", &text); });
        ImGui::PopItemFlag();
        checked([&] {
            double value = 1.5;
            NumberFieldBehavior("##number", &value, {.steppers = true});
            NumberFieldBehavior("##drag", &value, {.drag = true});
        });
        // Far below the window: InputTextEx returns early because its item is clipped.
        ImGui::SetCursorPosY(4000.0f);
        checked([&] { TextFieldBehavior("##clipped", &text, {.clearButton = true}); });
        checked([&] { TextFieldBehavior("##clippedarea", &text, {.multiline = true}); });
        if (activateMultiline)
            ImGui::SetKeyboardFocusHere();
        checked([&] { TextFieldBehavior("##clippedactive", &text); });
    });
    session.Frames(3);
    activateMultiline = true;
    session.Frames(3);
    CHECK(calls > 0);
    CHECK(mismatches == 0);
}

// ---- Buffers ---------------------------------------------------------------------------------

TEST_CASE("Text field: a std::string grows through the resize callback, also for long text")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text.shrink_to_fit();
    const std::size_t capacity = field.text.capacity();
    session.SetUi([&] { field.Run("##grow"); });
    session.Frame();
    session.Click(field.TextPoint());
    std::string longText(5000, 'x');
    longText += "end";
    session.SetClipboard(longText);
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(field.text == longText);
    CHECK(field.text.capacity() > capacity);
    CHECK(field.result.length == static_cast<int>(longText.size()));
    session.Type("!");
    CHECK(field.text.size() == longText.size() + 1);
}

TEST_CASE("Text field: a char buffer cuts the text to fit and stays zero terminated")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char buffer[6] = "";
    TextFieldResult result;
    session.SetUi([&] { result = TextFieldBehavior("##small", buffer, sizeof(buffer)); });
    session.Frame();
    session.Click(ImVec2(result.text.Min.x + 2.0f, result.text.GetCenter().y));
    session.Type("abcdefghij");
    CHECK(std::string(buffer) == "abcde");
    CHECK(buffer[5] == 0);
}

namespace
{

// The highest vertex alpha drawn strictly inside `rect` in the session window's draw list, and how
// many such vertices there are: the glyphs of a field's text.
int TextVertexAlpha(const ImRect& rect, int* count)
{
    const ImGuiWindow* window = ImGui::FindWindowByName("test");
    int best = 0;
    *count = 0;
    if (window == nullptr)
        return 0;
    for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
    {
        if (vertex.pos.x <= rect.Min.x || vertex.pos.x >= rect.Max.x || vertex.pos.y <= rect.Min.y ||
            vertex.pos.y >= rect.Max.y)
            continue;
        const int alpha = static_cast<int>((vertex.col >> IM_COL32_A_SHIFT) & 0xFF);
        if (alpha > 0)
            ++*count;
        best = std::max(best, alpha);
    }
    return best;
}

} // namespace

TEST_CASE("Text field: a disabled field draws its text whether the flag comes from BeginDisabled or not")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "Sample";
    TextFieldResult result;
    int mode = 0;  // 0: a pushed item flag, 1: BeginDisabled, 2: BeginDisabled and a host alpha of half
    session.SetUi([&] {
        if (mode == 0)
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        else
            ImGui::BeginDisabled();
        if (mode == 2)
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        result = TextFieldBehavior("##disabled", &text);
        if (mode == 2)
            ImGui::PopStyleVar();
        if (mode == 0)
            ImGui::PopItemFlag();
        else
            ImGui::EndDisabled();
    });
    int count = 0;
    session.Frames(2);
    const int pushedFlag = TextVertexAlpha(result.text, &count);
    CHECK(count > 0);
    CHECK(pushedFlag > 0);
    mode = 1;
    session.Frames(2);
    const int disabled = TextVertexAlpha(result.text, &count);
    CHECK(count > 0);
    CHECK(disabled == pushedFlag);
    mode = 2;
    session.Frames(2);
    const int halved = TextVertexAlpha(result.text, &count);
    CHECK(count > 0);
    CHECK(halved == doctest::Approx(disabled * 0.5).epsilon(0.05));
}

TEST_CASE("Text field: a zero-size or null buffer shows an empty read-only field")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char unused[4] = {'x', 'y', 'z', 'w'};
    TextFieldResult zero;
    TextFieldResult null;
    session.SetUi([&] {
        zero = TextFieldBehavior("##zero", unused, 0);
        null = TextFieldBehavior("##null", static_cast<std::string*>(nullptr));
    });
    session.Frame();
    session.Click(ImVec2(zero.text.Min.x + 2.0f, zero.text.GetCenter().y));
    session.Type("abc");
    session.Click(ImVec2(null.text.Min.x + 2.0f, null.text.GetCenter().y));
    session.Type("abc");
    CHECK(unused[0] == 'x');
    CHECK(unused[3] == 'w');
    CHECK_FALSE(zero.edited);
    CHECK_FALSE(null.edited);
}

TEST_CASE("Text field: text the caller changes while the field has the keyboard replaces the edit text")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "start";
    session.SetUi([&] { field.Run("##external"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.KeyPress(ImGuiKey_End);
    field.text = "replaced by the app";
    session.Frames(2);
    CHECK(field.text == "replaced by the app");
    CHECK(field.result.active);
    session.KeyPress(ImGuiKey_End);
    session.Type("!");
    CHECK(field.text.back() == '!');
    CHECK(field.text.rfind("replaced by the app", 0) == 0);
}

TEST_CASE("Text field: an idle field allocates nothing per frame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "some text";
    std::string secret = "hidden";
    char buffer[64] = "buffer text";
    std::string area = "line one\nline two";
    double number = 2.5;
    bool count = false;
    session.SetUi([&] {
        gTextFieldCountAllocations = count;
        TextFieldBehavior("Name", &text, {.icon = facet::Icon("search"), .clearButton = true});
        TextFieldBehavior("##secret", &secret, {.password = true});
        TextFieldBehavior("##buffer", buffer, sizeof(buffer), {.helpText = "Help"});
        TextFieldBehavior("##area", &area, {.multiline = true});
        NumberFieldBehavior("##number", &number, {.format = "%.2f ms", .steppers = true});
        NumberFieldBehavior("##drag", &number, {.drag = true});
        gTextFieldCountAllocations = false;
    });
    ImGuiMemAllocFunc allocFunc = nullptr;
    ImGuiMemFreeFunc freeFunc = nullptr;
    void* allocUser = nullptr;
    ImGui::GetAllocatorFunctions(&allocFunc, &freeFunc, &allocUser);
    ImGui::SetAllocatorFunctions(CountingImGuiAlloc, CountingImGuiFree, nullptr);
    session.Frames(20);
    gTextFieldAllocations = 0;
    count = true;
    session.Frames(20);
    count = false;
    const int idle = gTextFieldAllocations.load();
    // A field that has the keyboard but sees no input is idle too.
    session.Click(ImVec2(400.0f, 20.0f));
    session.Frames(20);
    gTextFieldAllocations = 0;
    count = true;
    session.Frames(20);
    count = false;
    const int focused = gTextFieldAllocations.load();
    ImGui::SetAllocatorFunctions(allocFunc, freeFunc, allocUser);
    CHECK(idle == 0);
    CHECK(focused == 0);
}

// ---- Commit and cancel -----------------------------------------------------------------------

TEST_CASE("Text field: Escape restores the text from activation and reports a cancel")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "keep";
    session.SetUi([&] { field.Run("##escape"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.KeyPress(ImGuiKey_End);
    session.Type(" changed");
    CHECK(field.text == "keep changed");
    const int editsBefore = field.edits;
    session.KeyPress(ImGuiKey_Escape);
    CHECK(field.text == "keep");
    CHECK(field.edits == editsBefore + 1);  // the restore is a write, as stock fields do
    CHECK(field.cancels == 1);
    CHECK(field.commits == 0);
    CHECK_FALSE(field.result.active);
}

TEST_CASE("Text field: focus loss commits once, by Tab and by a click elsewhere")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField first;
    TestField second;
    session.SetUi([&] {
        first.Run("##first");
        second.Run("##second");
    });
    session.Frame();
    session.Click(first.TextPoint());
    session.Type("one");
    session.KeyPress(ImGuiKey_Tab);
    session.Frame();
    CHECK(first.commits == 1);
    CHECK(first.afterEdit == 1);
    CHECK(second.result.active);
    session.Type("two");
    session.Click(ImVec2(700.0f, 500.0f));
    CHECK(second.commits == 1);
    CHECK(second.text == "two");
    session.Frames(3);
    CHECK(first.commits == 1);
    CHECK(second.commits == 1);
}

TEST_CASE("Text field: with live edit off the text is written only when the session ends")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    session.SetUi([&] {
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputText, false);
        field.Run("##commitonly");
        ImGui::PopItemFlag();
    });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("draft");
    CHECK(field.text.empty());
    CHECK(field.edits == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(field.text == "draft");
    CHECK(field.edits == 1);
    CHECK(field.commits == 1);
}

TEST_CASE("Text field: escapeClears empties first, then ends the session without a cancel")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.params.escapeClears = true;
    session.SetUi([&] { field.Run("##search"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("query");
    session.KeyPress(ImGuiKey_Escape);
    CHECK(field.text.empty());
    CHECK(field.result.active);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(field.result.active);
    CHECK(field.cancels == 0);
    CHECK(field.commits == 1);
}

// ---- Adornments ------------------------------------------------------------------------------

TEST_CASE("Text field: the clear button empties the text, commits once and gives the keyboard back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "to be cleared";
    field.params.clearButton = true;
    int clears = 0;
    session.SetUi([&] {
        field.Run("##clearable");
        clears += field.result.clearPressed ? 1 : 0;
    });
    session.Frame();
    REQUIRE(field.result.clear.GetWidth() > 0.0f);
    // The text area ends before the button.
    CHECK(field.result.text.Max.x <= field.result.clear.Min.x);
    session.Click(field.result.clear.GetCenter());
    CHECK(field.text.empty());
    CHECK(clears == 1);
    CHECK(field.commits == 1);
    session.Frame();
    CHECK(field.result.active);
    session.Type("new");
    CHECK(field.text == "new");
    // Without text the button is gone.
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_Delete);
    session.Frame();
    CHECK(field.result.clear.GetWidth() == 0.0f);
}

TEST_CASE("Text field: Tab skips the clear button and reaches slot buttons that ask for it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "abc";
    bool revealed = false;
    ImGuiID revealId = 0;
    ImGuiID fieldId = 0;
    ImGuiID afterId = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        TextFieldParams params;
        params.clearButton = true;
        params.password = true;
        params.revealed = revealed;
        params.trailingWidth = facet::internal::TextFieldSlotButtonsWidth(1, facet::ControlSize::Regular);
        params.trailingSlot = [](const facet::internal::TextFieldSlot& slot, void* user) {
            ImGuiID& id = *static_cast<ImGuiID*>(user);
            id = ImGui::GetIDWithSeed("reveal", nullptr, slot.fieldId);
            facet::internal::TextFieldSlotButton(slot, 0, id, facet::Icon("eye"), {.tabStop = true, .name = "Show"});
        };
        params.trailingUser = &revealId;
        fieldId = TextFieldBehavior("##secret", &text, params).id;
        ImGui::Button("after");
        afterId = ImGui::GetItemID();
    });
    session.Frame();
    session.Click(ImVec2(10.0f, 10.0f));  // the "before" button
    session.KeyPress(ImGuiKey_Tab);       // after a click the first Tab shows the focus where it is
    session.KeyPress(ImGuiKey_Tab);
    CHECK(GImGui->NavId == fieldId);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(GImGui->NavId == revealId);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(GImGui->NavId == afterId);
}

TEST_CASE("Text field: a slot button draws its glyph text, underlined when asked, instead of the icon")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    struct Slot
    {
        const char* glyph = nullptr;
        bool underline = false;
        ImRect rect;
        int vertices = 0;
    };
    Slot plain{.glyph = "Aa"};
    Slot underlined{.glyph = "Aa", .underline = true};
    const auto run = [&](const char* label, Slot& slot) {
        TextFieldParams params;
        params.trailingWidth = facet::internal::TextFieldSlotButtonsWidth(1, facet::ControlSize::Regular);
        params.trailingSlot = [](const facet::internal::TextFieldSlot& s, void* user) {
            Slot& data = *static_cast<Slot*>(user);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const int first = drawList->VtxBuffer.Size;
            const facet::internal::ItemState item = facet::internal::TextFieldSlotButton(
                s, 0, ImGui::GetIDWithSeed("toggle", nullptr, s.fieldId), facet::Icon(),
                {.tabStop = true, .name = "Toggle", .glyph = data.glyph, .glyphUnderline = data.underline});
            data.rect = item.rect;
            data.vertices = drawList->VtxBuffer.Size - first;
        };
        params.trailingUser = &slot;
        TextFieldBehavior(label, &text, params);
    };
    session.SetUi([&] {
        run("##plain", plain);
        run("##underlined", underlined);
    });
    session.Frame();
    // Two letters at least, and the underline adds a line on top of them.
    CHECK(plain.vertices >= 8);
    CHECK(underlined.vertices > plain.vertices);
    CHECK(plain.rect.GetWidth() > 0.0f);
}

TEST_CASE("Text field: text never reaches under an adornment, at any width and scale")
{
    TextFieldThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text = "some text that is longer than the narrowest field";
    TextFieldResult result;
    float width = 0.0f;
    session.SetUi([&] {
        result = TextFieldBehavior("Label", &text,
                                   {.icon = facet::Icon("search"),
                                    .suffix = "ms",
                                    .clearButton = true,
                                    .width = width,
                                    .loading = true,
                                    .trailingWidth = facet::internal::TextFieldSlotButtonsWidth(2, facet::ControlSize::Regular)});
    });
    for (const float scale : {0.75f, 1.0f, 2.0f})
    {
        facet::SetUiScale(scale);
        for (const float w : {20.0f, 90.0f, 160.0f, 400.0f})
        {
            CAPTURE(scale);
            CAPTURE(w);
            width = w;
            session.Frames(2);
            CHECK(result.text.GetWidth() > 0.0f);
            CHECK(result.text.Min.x >= result.frame.Min.x);
            CHECK(result.text.Max.x <= result.frame.Max.x);
            if (result.clear.GetWidth() > 0.0f)
                CHECK(result.text.Max.x <= result.clear.Min.x);
            CHECK(result.text.Max.x <= result.trailing.Min.x);
            CHECK(result.trailing.Max.x <= result.frame.Max.x);
        }
    }
}

// ---- Behaviour options -----------------------------------------------------------------------

TEST_CASE("Text field: maxLength cuts typing and pasting at code points")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.params.maxLength = 5;
    session.SetUi([&] { field.Run("##limited"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("ab\xC3\xA9");  // a, b, e acute: 3 code points
    session.SetClipboard("cdefg");
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(field.text == "ab\xC3\xA9" "cd");
    CHECK(field.result.length == 5);
    // A selection that is replaced frees its room.
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    session.Type("z");
    CHECK(field.text == "ab\xC3\xA9" "cz");
}

TEST_CASE("Text field: a password copies nothing while masked and everything when revealed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "secret";
    field.params.password = true;
    session.SetUi([&] { field.Run("##password"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.SetClipboard("before");
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "before");
    field.params.revealed = true;
    session.Frame();
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "secret");
}

TEST_CASE("Text field: read-only text can be selected and copied but is never written")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "read only";
    field.params.readOnly = true;
    session.SetUi([&] { field.Run("##readonly"); });
    session.Frame();
    session.Click(field.TextPoint());
    CHECK(field.result.active);
    session.Type("xyz");
    session.KeyPress(ImGuiKey_Backspace);
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(field.text == "read only");
    CHECK(session.Clipboard() == "read only");
    CHECK(field.edits == 0);
}

TEST_CASE("Text field: a disabled field takes no keyboard, no click and no Tab")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    ImGuiID afterId = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        ImGui::BeginDisabled();
        field.Run("##disabled");
        ImGui::EndDisabled();
        ImGui::Button("after");
        afterId = ImGui::GetItemID();
    });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("abc");
    CHECK(field.text.empty());
    CHECK_FALSE(field.result.active);
    session.Click(ImVec2(10.0f, 10.0f));
    session.KeyPress(ImGuiKey_Tab);  // shows the focus on "before"
    session.KeyPress(ImGuiKey_Tab);
    CHECK(GImGui->NavId == afterId);
}

TEST_CASE("Text field: multiline takes Enter as a line break and wraps")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.params.multiline = true;
    field.params.width = 160.0f;
    session.SetUi([&] { field.Run("##area"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("first");
    session.KeyPress(ImGuiKey_Enter);
    session.Type("second line with enough words to wrap at least once");
    CHECK(field.text.find('\n') == 5);
    CHECK(field.result.active);
    CHECK(field.submits == 0);
    // The caret sits on a lower visual line than the first line: the second line wrapped.
    ImRect caret;
    REQUIRE(facet::internal::TextFieldCaretRect(field.result.id, &caret));
    CHECK(caret.Min.y >= field.result.frame.Min.y + caret.GetHeight() * 2.0f - 1.0f);
    CHECK(caret.Min.y == doctest::Approx(GImGui->PlatformImeData.InputPos.y));
    CHECK(caret.Min.x - 1.0f == doctest::Approx(GImGui->PlatformImeData.InputPos.x));
    session.KeyPress(ImGuiKey_Enter | ImGuiMod_Ctrl);
    CHECK_FALSE(field.result.active);
    CHECK(field.commits == 1);
}

TEST_CASE("Text field: a floating label field is taller and the label floats while it has the keyboard")
{
    TextFieldThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField plain;
    TestField floating;
    floating.params.floatingLabel = true;
    floating.params.placeholder = "Placeholder";
    session.SetUi([&] {
        plain.Run("##plain");
        floating.Run("Email");
    });
    session.Frame();
    CHECK(floating.result.frame.GetHeight() > plain.result.frame.GetHeight());
    // The label is inside the frame, not in front of it.
    CHECK(floating.result.frame.Min.x == plain.result.frame.Min.x);
    session.Click(floating.TextPoint());
    session.Type("me");
    CHECK(floating.text == "me");
}

// ---- Geometry --------------------------------------------------------------------------------

namespace
{

// The rectangles drawn in `color` into `drawList` (Dear ImGui's selection fills).
std::vector<ImRect> FilledRects(const ImDrawList* drawList, ImU32 color)
{
    std::vector<ImRect> rects;
    for (int i = 0; i + 3 < drawList->VtxBuffer.Size; ++i)
    {
        const ImDrawVert& v = drawList->VtxBuffer[i];
        if (v.col != color)
            continue;
        ImRect r(v.pos, v.pos);
        for (int k = 1; k < 4; ++k)
            r.Add(drawList->VtxBuffer[i + k].pos);
        rects.push_back(r);
        i += 3;
    }
    return rects;
}

} // namespace

TEST_CASE("Text field: selection rects line up with the selection Dear ImGui draws, scrolled or not")
{
    TextFieldThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    for (const float scale : {0.75f, 1.0f, 2.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        TestField field;
        field.params.width = 140.0f;
        session.SetUi([&] { field.Run("##selection"); });
        session.Frame();
        session.Click(field.TextPoint());
        session.Type("a long line of text that scrolls well past the end of the field");
        for (int i = 0; i < 6; ++i)
            session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
        session.Frame();
        ImRect rects[4];
        const int count = facet::internal::TextFieldSelectionRects(field.result.id, rects);
        REQUIRE(count == 1);
        const ImU32 selection = ImGui::GetColorU32(facet::T().colors.accentSubtle);
        const std::vector<ImRect> drawn = FilledRects(ImGui::FindWindowByName("test")->DrawList, selection);
        REQUIRE(drawn.size() == 1);
        CHECK(rects[0].Min.x == doctest::Approx(drawn[0].Min.x).epsilon(0.001));
        CHECK(rects[0].Max.x == doctest::Approx(drawn[0].Max.x).epsilon(0.001));
        CHECK(rects[0].Min.y <= drawn[0].Max.y);
        CHECK(rects[0].Max.y >= drawn[0].Min.y);
        int start = 0;
        int end = 0;
        int caret = 0;
        REQUIRE(facet::internal::TextFieldGetSelection(field.result.id, &start, &end, &caret));
        CHECK(end - start == 6);
        session.KeyPress(ImGuiKey_Escape);
    }
}

TEST_CASE("Text field: the popup anchor follows the frame or the caret")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    session.SetUi([&] { field.Run("##anchor"); });
    session.Frame();
    session.Click(field.TextPoint());
    session.Type("abc");
    const facet::internal::PopupAnchor below =
        facet::internal::TextFieldPopupAnchor(field.result, facet::internal::TextFieldAnchor::Field);
    CHECK(below.matchWidth);
    CHECK(below.rect.Min.x == field.result.frame.Min.x);
    CHECK(below.rect.Max.x == field.result.frame.Max.x);
    const facet::internal::PopupAnchor caret =
        facet::internal::TextFieldPopupAnchor(field.result, facet::internal::TextFieldAnchor::Caret);
    CHECK_FALSE(caret.matchWidth);
    CHECK(caret.rect.Min.x > field.result.text.Min.x);
    CHECK(caret.rect.Min.x == field.result.caret.Min.x);
}

// ---- Requests --------------------------------------------------------------------------------

TEST_CASE("Text field: focus, selection and insert requests act in the next call")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "hello world";
    session.SetUi([&] { field.Run("##requests"); });
    session.Frame();
    const ImGuiID id = field.result.id;
    facet::internal::TextFieldRequestFocus(id, facet::internal::TextFieldFocus::End);
    session.Frame();
    CHECK(field.result.active);
    CHECK(ActiveSelection(id) == Selection{11, 11, 11});
    facet::internal::TextFieldSetSelection(id, 6, 11);
    session.Frame();
    CHECK(ActiveSelection(id) == Selection{6, 11, 11});
    facet::internal::TextFieldInsert(id, "there\nfriend");
    session.Frame();
    CHECK(field.text == "hello there friend");
    // One undo step takes the insert back.
    session.KeyPress(ImGuiKey_Z | ImGuiMod_Ctrl);
    CHECK(field.text == "hello world");
    // A request for a field that never takes the keyboard is dropped after two frames.
    session.KeyPress(ImGuiKey_Escape);
    facet::internal::TextFieldSetSelection(id, 0, 1);
    session.Frames(4);
    session.Click(field.TextPoint(200.0f));
    CHECK(ActiveSelection(id).start == ActiveSelection(id).end);
}

// ---- Numbers: text -----------------------------------------------------------------------------

TEST_CASE("Number text: plain numbers parse, everything else is rejected")
{
    double value = -1.0;
    CHECK(ParseNumber("42", &value) == NumberParse::Ok);
    CHECK(value == 42.0);
    CHECK(ParseNumber("  -3.5 ", &value) == NumberParse::Ok);
    CHECK(value == -3.5);
    CHECK(ParseNumber("+7", &value) == NumberParse::Ok);
    CHECK(value == 7.0);
    CHECK(ParseNumber(".5", &value) == NumberParse::Ok);
    CHECK(value == 0.5);
    CHECK(ParseNumber("5.", &value) == NumberParse::Ok);
    CHECK(value == 5.0);
    CHECK(ParseNumber("1e3", &value) == NumberParse::Ok);
    CHECK(value == 1000.0);
    CHECK(ParseNumber("2.5E-2", &value) == NumberParse::Ok);
    CHECK(value == doctest::Approx(0.025));
    CHECK(ParseNumber("2,5", &value) == NumberParse::Ok);  // a lone comma is the decimal separator
    CHECK(value == 2.5);
    CHECK(ParseNumber("-0", &value) == NumberParse::Ok);
    CHECK_FALSE(std::signbit(value));
    value = 99.0;
    CHECK(ParseNumber("", &value) == NumberParse::Empty);
    CHECK(ParseNumber("   ", &value) == NumberParse::Empty);
    CHECK(ParseNumber(nullptr, &value) == NumberParse::Empty);
    CHECK(ParseNumber("-", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("+", &value) == NumberParse::Invalid);
    CHECK(ParseNumber(".", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("1e", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("1e+", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("1,234.5", &value) == NumberParse::Invalid);  // not guessed
    CHECK(ParseNumber("1.2.3", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("2*3", &value) == NumberParse::Invalid);       // no expressions
    CHECK(ParseNumber("12px", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("inf", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("nan", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("1e999", &value) == NumberParse::Invalid);
    CHECK(ParseNumber("1 2", &value) == NumberParse::Invalid);
    CHECK(value == 99.0);  // never written by a failed parse
}

TEST_CASE("Number text: formats with the format, a FormatFn, and in the editable form")
{
    char out[64];
    using facet::internal::FormatNumber;
    CHECK(FormatNumber(out, 64, 3.14159, nullptr, nullptr, nullptr, false, false) == 4);
    CHECK(std::string(out) == "3.14");
    CHECK(FormatNumber(out, 64, 41.6, nullptr, nullptr, nullptr, true, false) > 0);
    CHECK(std::string(out) == "42");
    FormatNumber(out, 64, 12.5, "%.1f ms", nullptr, nullptr, false, false);
    CHECK(std::string(out) == "12.5 ms");
    FormatNumber(out, 64, 12.5, "%.1f ms", nullptr, nullptr, false, true);
    CHECK(std::string(out) == "12.5");
    FormatNumber(out, 64, 50.0, "%d %%", nullptr, nullptr, true, false);
    CHECK(std::string(out) == "50 %");
    FormatNumber(out, 64, 50.0, "Value: %5.1f", nullptr, nullptr, false, true);
    CHECK(std::string(out) == "50.0");
    FormatNumber(out, 64, -0.0, "%.2f", nullptr, nullptr, false, false);
    CHECK(std::string(out) == "0.00");
    const facet::FormatFn hex = [](char* buffer, int size, double value, void*) {
        return std::snprintf(buffer, static_cast<std::size_t>(size), "#%X", static_cast<unsigned>(value));
    };
    FormatNumber(out, 64, 255.0, nullptr, hex, nullptr, true, false);
    CHECK(std::string(out) == "#FF");
    FormatNumber(out, 64, 255.0, nullptr, hex, nullptr, true, true);
    CHECK(std::string(out) == "255");
    FormatNumber(out, 64, 0.1, nullptr, hex, nullptr, false, true);
    CHECK(std::string(out) == "0.1");  // the shortest exact form
    // Locale independent: the separator stays a dot under a comma locale.
    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    if (std::setlocale(LC_NUMERIC, "de-DE") != nullptr)
    {
        FormatNumber(out, 64, 1.5, "%.1f", nullptr, nullptr, false, false);
        CHECK(std::string(out) == "1.5");
        double value = 0.0;
        CHECK(ParseNumber("1.5", &value) == NumberParse::Ok);
        CHECK(value == 1.5);
    }
    std::setlocale(LC_NUMERIC, saved.c_str());
}

// ---- Numbers: the field ------------------------------------------------------------------------

namespace
{

struct TestNumber
{
    double value = 0.0;
    NumberFieldParams params;
    NumberFieldResult result;
    int changes = 0;
    int commits = 0;
    int cancels = 0;
    int parseFailures = 0;

    void Run(const char* label)
    {
        result = NumberFieldBehavior(label, &value, params);
        changes += result.changed ? 1 : 0;
        parseFailures += result.parseFailed ? 1 : 0;
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
    }
    ImVec2 TextPoint() const { return ImVec2(result.field.text.Min.x + 2.0f, result.field.text.GetCenter().y); }
};

} // namespace

TEST_CASE("Number field: typed text is written on Enter, clamped, and only when it parses")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 5.0;
    number.params.min = 0.0;
    number.params.max = 100.0;
    session.SetUi([&] {
        number.Run("##number");
        ImGui::Button("next");  // where Tab goes
    });
    session.Frame();
    session.Click(number.TextPoint());
    CHECK(number.result.typing);
    session.Type("250");  // everything was selected on activation
    CHECK(number.value == 5.0);  // commit only
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 100.0);
    CHECK(number.commits == 1);
    session.Click(number.TextPoint());
    session.Type("-");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 100.0);
    CHECK(number.parseFailures == 1);
    session.Click(number.TextPoint());
    session.Type("2,5");
    session.KeyPress(ImGuiKey_Tab);
    session.Frame();
    CHECK(number.value == 2.5);
    // Letters are not even typed.
    session.Click(number.TextPoint());
    session.Type("x7");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 7.0);
}

TEST_CASE("Number field: per keystroke when live scalar editing is on")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    session.SetUi([&] {
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        number.Run("##live");
        ImGui::PopItemFlag();
    });
    session.Frame();
    session.Click(number.TextPoint());
    session.Type("1");
    CHECK(number.value == 1.0);
    session.Type("2");
    CHECK(number.value == 12.0);
    session.Type("-");  // "12-" does not parse: nothing written
    CHECK(number.value == 12.0);
}

TEST_CASE("Number field: Up, Down and Page keys step with the modifiers; Escape restores")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 10.0;
    number.params.step = 1.0;
    number.params.stepFast = 10.0;
    session.SetUi([&] { number.Run("##steps"); });
    session.Frame();
    session.Click(number.TextPoint());
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(number.value == 11.0);
    session.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Shift);
    CHECK(number.value == doctest::Approx(11.1));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Ctrl);
    CHECK(number.value == doctest::Approx(1.1));
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(number.value == doctest::Approx(11.1));
    CHECK(number.result.typing);  // the keys did not move the focus away
    session.KeyPress(ImGuiKey_Escape);
    CHECK(number.value == 10.0);
    CHECK(number.cancels == 1);
    CHECK(number.commits == 0);
    // A typed number is the base of the next step.
    session.Click(number.TextPoint());
    session.Type("40");
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(number.value == 41.0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 41.0);
    CHECK(number.commits == 1);
}

TEST_CASE("Number field: a typing field that slides under the pointer mid-scroll leaves the wheel to the area")
{
    facet::SetFollowSystemMotion(false);
    facet::SetUiScale(1.0f);
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        TestNumber number;
        number.value = 10.0;
        number.params.step = 1.0;
        ImGuiID areaId = 0;
        session.SetUi([&] {
            if (facet::internal::BeginScrollAreaCore("area", {.size = ImVec2(300.0f, 200.0f)}))
            {
                areaId = ImGui::GetCurrentWindow()->ID;
                ImGui::Dummy(ImVec2(10.0f, 120.0f));
                number.Run("##wheel");
                ImGui::Dummy(ImVec2(10.0f, 800.0f));
                facet::internal::EndScrollAreaCore();
            }
        });
        session.Frames(2);
        session.Click(number.TextPoint());
        REQUIRE(number.result.typing);
        ImGuiWindow* area = ImGui::FindWindowByID(areaId);
        REQUIRE(area != nullptr);
        // The pointer rests one wheel notch above the field, off it.
        const float notch = facet::Px(facet::T().scroll.wheelDistance);
        const ImVec2 at = number.result.field.frame.GetCenter();
        const ImVec2 rest(at.x, at.y - notch);
        session.MouseMove(rest);
        session.Frames(2);
        session.Wheel(rest, -1.0f);
        const float firstTarget = facet::internal::ScrollAreaTarget(area).y;
        CHECK(firstTarget == doctest::Approx(notch));
        // Most of the way there the field is under the pointer while the area still moves.
        session.Seconds(0.1f);
        REQUIRE(facet::internal::IsScrollAreaScrolling(area));
        REQUIRE(number.result.field.frame.Contains(rest));
        REQUIRE(number.result.typing);
        session.Wheel(rest, -1.0f);
        CHECK(number.value == 10.0);
        CHECK(facet::internal::ScrollAreaTarget(area).y == doctest::Approx(firstTarget + notch));
        // Once the area has come to rest, the typing field under the pointer steps with the wheel.
        session.Seconds(1.5f);
        REQUIRE_FALSE(facet::internal::IsScrollAreaScrolling(area));
        REQUIRE(number.result.typing);
        session.Wheel(number.result.field.frame.GetCenter(), 1.0f);
        CHECK(number.value == 11.0);
    }
    facet::SetFollowSystemMotion(true);
}

TEST_CASE("Number field: steppers step at once, stop at the ends and commit once per press")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 1.0;
    number.params.integer = true;
    number.params.min = 0.0;
    number.params.max = 3.0;
    number.params.steppers = true;
    session.SetUi([&] { number.Run("##steppers"); });
    session.Frame();
    REQUIRE(number.result.field.trailing.GetWidth() > 0.0f);
    const ImRect slot = number.result.field.trailing;
    const ImVec2 plus(slot.Max.x - slot.GetHeight() * 0.5f, slot.GetCenter().y);
    const ImVec2 minus(slot.Min.x + slot.GetHeight() * 0.3f, slot.GetCenter().y);
    session.Click(plus);
    CHECK(number.value == 2.0);
    CHECK(number.commits == 1);
    session.Click(plus);
    session.Click(plus);
    CHECK(number.value == 3.0);
    session.Click(minus);
    CHECK(number.value == 2.0);
    CHECK(number.commits == 3);  // the third + was disabled at the maximum: no edit
}

TEST_CASE("Number field: a logarithmic drag multiplies the value by equal factors for equal travel")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 1.0;
    number.params.drag = true;
    number.params.min = 1.0;
    number.params.max = 10000.0;
    number.params.scale = ValueScale::Logarithmic;
    session.SetUi([&] { number.Run("##log"); });
    session.Frame();
    const ImRect frame = number.result.field.frame;
    const float quarter = frame.GetWidth() * 0.25f;
    const ImVec2 start(frame.Min.x + 4.0f, frame.GetCenter().y);
    // A quarter of the frame width is a quarter of the four decades: one decade.
    session.Drag({start, ImVec2(start.x + quarter, start.y)}, 6);
    CHECK(number.value == doctest::Approx(10.0).epsilon(0.02));
    session.Drag({start, ImVec2(start.x + quarter, start.y)}, 6);
    CHECK(number.value == doctest::Approx(100.0).epsilon(0.02));
    // A linear field over the same range moves by a quarter of the range instead.
    number.params.scale = ValueScale::Linear;
    number.value = 1.0;
    session.Frame();
    session.Drag({start, ImVec2(start.x + quarter, start.y)}, 6);
    CHECK(number.value > 1000.0);
}

namespace
{

// "1m30" style text: minutes before an "m", seconds after it; a plain number is seconds.
NumberParse ParseMinutes(void*, const char* text, double* out)
{
    const char* m = std::strchr(text, 'm');
    if (m == nullptr)
        return ParseNumber(text, out);
    const std::string minutes(text, m);
    double whole = 0.0;
    double rest = 0.0;
    if (ParseNumber(minutes.c_str(), &whole) != NumberParse::Ok)
        return NumberParse::Invalid;
    if (m[1] != '\0' && ParseNumber(m + 1, &rest) != NumberParse::Ok)
        return NumberParse::Invalid;
    *out = whole * 60.0 + rest;
    return NumberParse::Ok;
}

} // namespace

TEST_CASE("Number field: a parser reads the field's own grammar and letters can be typed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 5.0;
    number.params.parser = &ParseMinutes;
    session.SetUi([&] { number.Run("##duration"); });
    session.Frame();
    session.Click(number.TextPoint());
    session.Type("1m30");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 90.0);
    // The plain editable form still reads back.
    session.Click(number.TextPoint());
    session.Type("45");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 45.0);
    // Text the parser rejects writes nothing.
    session.Click(number.TextPoint());
    session.Type("xm");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 45.0);
    CHECK(number.parseFailures == 1);
}

TEST_CASE("Number field: dragging goes by the pointer travel without drift; a click types")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 10.0;
    number.params.drag = true;
    number.params.speed = 0.5;
    session.SetUi([&] { number.Run("Drag"); });
    session.Frame();
    const ImVec2 start = number.result.field.frame.GetCenter();
    session.Drag({start, ImVec2(start.x + 40.0f, start.y), ImVec2(start.x - 10.0f, start.y), start}, 6);
    CHECK(number.value == 10.0);  // back where it started: exactly the start value
    CHECK(number.commits == 1);   // the value moved during the session
    session.Drag({start, ImVec2(start.x + 40.0f, start.y)}, 6);
    CHECK(number.value == doctest::Approx(30.0));
    CHECK(number.commits == 2);
    // Shift moves a tenth as far.
    session.KeyDown(ImGuiMod_Shift);
    session.Drag({start, ImVec2(start.x + 40.0f, start.y)}, 6);
    session.KeyUp(ImGuiMod_Shift);
    CHECK(number.value == doctest::Approx(32.0));
    // A click without travel types a value instead.
    session.Click(start);
    session.Frame();
    CHECK(number.result.typing);
    session.Type("7");
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(number.value == 7.0);
    CHECK_FALSE(number.result.typing);
    // The label drags too.
    const ImVec2 label(number.result.field.frame.Min.x - 16.0f, start.y);
    session.Drag({label, ImVec2(label.x + 20.0f, label.y)}, 6);
    CHECK(number.value == doctest::Approx(17.0));
}

TEST_CASE("Number field: a click on a drag field types at every scale")
{
    TextFieldThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    for (const float scale : {0.75f, 1.0f, 2.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        TestNumber number;
        number.value = 12.0;
        number.params.format = "%.1f";
        number.params.min = -100.0;
        number.params.max = 100.0;
        number.params.step = 0.5;
        number.params.steppers = true;
        number.params.drag = true;
        number.params.unit = "ms";
        number.params.width = 220.0f;
        session.SetUi([&] { number.Run("##scaled"); });
        session.Frames(2);
        session.Click(number.result.field.frame.GetCenter());
        session.Frame();
        CHECK(number.result.typing);
        session.Type("250");
        session.KeyPress(ImGuiKey_Enter);
        CHECK(number.value == 100.0);
    }
}

TEST_CASE("Number field: drag mode steps with Left and Right while focused and types on Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestNumber number;
    number.value = 3.0;
    number.params.drag = true;
    number.params.integer = true;
    session.SetUi([&] {
        ImGui::Button("before");
        number.Run("##dragkeys");
    });
    session.Frame();
    session.Click(ImVec2(10.0f, 10.0f));
    session.KeyPress(ImGuiKey_Tab);  // shows the focus on "before"
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(number.value == 4.0);
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Ctrl);
    CHECK(number.value == -6.0);
    session.KeyPress(ImGuiKey_Enter);
    session.Frame();
    CHECK(number.result.typing);
    session.Type("12");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(number.value == 12.0);
}

TEST_CASE("Text field: a click in the frame where no part is gives the text the keyboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "12";
    field.params.suffix = "units";
    TestNumber number;
    number.params.drag = true;
    number.params.unit = "units";
    session.SetUi([&] {
        field.Run("##suffix");
        number.Run("##dragsuffix");
    });
    session.Frame();
    // Between the end of the text area and the end of the frame: the suffix strip.
    session.Click(ImVec2(field.result.text.Max.x + 4.0f, field.result.frame.GetCenter().y));
    session.Frame();
    CHECK(field.result.active);
    CHECK(ActiveSelection(field.result.id) == Selection{2, 2, 2});
    session.KeyPress(ImGuiKey_Escape);
    session.Click(ImVec2(number.result.field.text.Max.x + 4.0f, number.result.field.frame.GetCenter().y));
    session.Frame();
    CHECK(number.result.typing);
}
TEST_CASE("Text field: options for composite widgets: frameless, hidden text, leading spinner, line count")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField frameless;
    frameless.params.frameless = true;
    TestField hidden;
    hidden.params.hideText = true;
    TestField plain;
    plain.params.icon = facet::Icon("search");
    TestField spinner;
    spinner.params.icon = facet::Icon("search");
    spinner.params.loading = true;
    spinner.params.spinnerReplacesIcon = true;
    TestField area;
    area.params.multiline = true;
    area.params.width = 200.0f;
    area.text = "one\ntwo\nthree";
    int hiddenGlyphs = -1;
    session.SetUi([&] {
        frameless.Run("##frameless");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int before = drawList->VtxBuffer.Size;
        hidden.Run("##hidden");
        // Glyph vertices inside the hidden field's text area: none may be drawn. (The frame's
        // rounded corners are textured quads too; their inner corner lies one pixel past the
        // padding, so the test stays two pixels inside the area.)
        hiddenGlyphs = 0;
        const ImVec2 white = drawList->_Data->TexUvWhitePixel;
        ImRect inside = hidden.result.text;
        inside.Expand(-2.0f);
        for (int i = before; i < drawList->VtxBuffer.Size; ++i)
        {
            const ImDrawVert& v = drawList->VtxBuffer[i];
            if ((v.uv.x != white.x || v.uv.y != white.y) && inside.Contains(v.pos))
                ++hiddenGlyphs;
        }
        plain.Run("##plainicon");
        spinner.Run("##spinner");
        area.Run("##lines");
    });
    session.Frame();
    // No frame, no padding: the text starts at the frame edge.
    CHECK(frameless.result.text.Min.x == frameless.result.frame.Min.x);
    // The spinner takes the icon's place: the text area is the same as without it.
    CHECK(spinner.result.text.Min.x == plain.result.text.Min.x);
    CHECK(spinner.result.text.Max.x == plain.result.text.Max.x);
    CHECK(area.result.lineCount == 3);
    area.text = "one long line of words that has to wrap at least once in a field this narrow";
    session.Frame();
    CHECK(area.result.lineCount >= 2);
    // Hidden text still edits, and the widget gets the caret to draw its own.
    session.Click(hidden.TextPoint());
    session.Type("1234");
    CHECK(hidden.text == "1234");
    CHECK(hiddenGlyphs == 0);
    CHECK(hidden.result.caret.Min.x > hidden.result.text.Min.x);
}

TEST_CASE("Text field: a read-only field reports caret and selection in the frame of its call")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "copy me";
    field.params.readOnly = true;
    int rects = -1;
    session.SetUi([&] {
        field.Run("##copyfield");
        ImRect out[2];
        rects = facet::internal::TextFieldSelectionRects(field.result.id, out);
    });
    session.Frame();
    session.Click(field.TextPoint());
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    CHECK(rects == 1);
    CHECK(field.result.caret.Min.x > field.result.text.Min.x);
}
TEST_CASE("Text field: text that continues past an edge fades out there, and only glyphs fade")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestField field;
    field.text = "a sample line that is much longer than the narrow field it is shown in";
    field.params.width = 120.0f;
    float rightAlpha = -1.0f;
    float leftAlpha = -1.0f;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int before = drawList->VtxBuffer.Size;
        field.Run("##fade");
        // The lowest glyph alpha in the last and the first few pixels of the text area.
        // Text glyphs only: textured and in the label color (the frame's rounded corners are
        // textured too, in a fill color).
        const ImVec2 white = drawList->_Data->TexUvWhitePixel;
        const ImU32 textRgb = facet::ToU32(facet::T().colors.label) & ~IM_COL32_A_MASK;
        rightAlpha = leftAlpha = 1.0f;
        for (int i = before; i < drawList->VtxBuffer.Size; ++i)
        {
            const ImDrawVert& v = drawList->VtxBuffer[i];
            if ((v.uv.x == white.x && v.uv.y == white.y) || (v.col & ~IM_COL32_A_MASK) != textRgb)
                continue;
            const float alpha = static_cast<float>((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            if (v.pos.x > field.result.text.Max.x - 3.0f && v.pos.x <= field.result.text.Max.x + 1.0f)
                rightAlpha = std::min(rightAlpha, alpha);
            // Scrolled glyphs reach left of the area (clipped there), so the window starts before it.
            if (v.pos.x >= field.result.text.Min.x - 20.0f && v.pos.x < field.result.text.Min.x + 3.0f)
                leftAlpha = std::min(leftAlpha, alpha);
        }
    });
    session.Frame();
    CHECK(rightAlpha < 0.3f);  // continues on the right: faded
    CHECK(leftAlpha > 0.9f);   // starts at the left edge: not faded
    // With the caret at the end the text scrolls: now it continues on the left.
    session.Click(field.TextPoint());
    session.KeyPress(ImGuiKey_End);
    session.Frame();
    CHECK(leftAlpha < 0.3f);
}

TEST_CASE("Text field: a field with a visible label ends at its frame, also in the window's content size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    TextFieldResult plain;
    TextFieldResult multi;
    ImRect plainItem;
    ImRect multiItem;
    float idealRight = 0.0f;
    session.SetUi([&] {
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));
        ImGui::Begin("auto sized", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextItemWidth(200.0f);
        plain = TextFieldBehavior("Visible label", &text);
        plainItem = GImGui->LastItemData.Rect;
        ImGui::SetNextItemWidth(200.0f);
        multi = TextFieldBehavior("Another label", &text, {.multiline = true});
        multiItem = GImGui->LastItemData.Rect;
        idealRight = GImGui->CurrentWindow->DC.IdealMaxPos.x;
        ImGui::End();
    });
    session.Frames(3);
    // Dear ImGui's own label (drawn after its frame, out of sight) adds nothing to the widget.
    CHECK(plainItem.Max.x == doctest::Approx(plain.frame.Max.x));
    CHECK(multiItem.Max.x == doctest::Approx(multi.frame.Max.x));
    CHECK(plainItem.GetWidth() == doctest::Approx(200.0f));
    CHECK(idealRight <= plain.frame.Max.x + 0.5f);
}

TEST_CASE("Text field: a hidden label leaves the whole width to the frame and keeps the id")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    TextFieldResult shown;
    TextFieldResult hidden;
    ImRect hiddenItem;
    ImGuiID expected = 0;
    session.SetUi([&] {
        shown = TextFieldBehavior("Visible label##shown", &text);
        hidden = TextFieldBehavior("Visible label##hidden", &text, {.labelHidden = true});
        hiddenItem = GImGui->LastItemData.Rect;
        expected = ImGui::GetID("Visible label##hidden");
    });
    session.Frame();
    CHECK(shown.frame.GetWidth() < hidden.frame.GetWidth());
    CHECK(hidden.frame.Min.x == doctest::Approx(hiddenItem.Min.x));
    CHECK(hidden.id == expected);
}