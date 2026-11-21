// The public API of the text-like inputs of facet/widgets/inputs.h (TextField, SecureField,
// SearchField, TextArea, Autocomplete, CopyField, FilterField, TagInput, InlineRename, MaskedInput,
// PinCodeInput, PathField, FieldValidation and ValidationSummary): every options struct follows
// the conventions, and every widget that is still a placeholder is safe to call, submits one item
// that cannot be operated, leaves the Dear ImGui stacks as it found them, returns the neutral
// result and writes nothing. Behaviour tests of implemented widgets go into
// tests/test_inputs_text.cpp; the number-like inputs have tests/test_api_inputs_value.cpp.

#include <cfloat>
#include <cmath>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/controls_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::controls;

static_assert(OptionsFollowConventions<facet::TextFieldOptions>());
static_assert(OptionsFollowConventions<facet::SecureFieldOptions>());
static_assert(OptionsFollowConventions<facet::SearchFieldOptions>());
static_assert(OptionsFollowConventions<facet::FilterFieldOptions>());
static_assert(OptionsFollowConventions<facet::AutocompleteOptions>());
static_assert(OptionsFollowConventions<facet::TagInputOptions>());
static_assert(OptionsFollowConventions<facet::TextAreaOptions>());
static_assert(OptionsFollowConventions<facet::InlineRenameOptions>());
static_assert(OptionsFollowConventions<facet::MaskedInputOptions>());
static_assert(OptionsFollowConventions<facet::PinCodeInputOptions>());
static_assert(OptionsFollowConventions<facet::PathFieldOptions>());
static_assert(OptionsFollowConventions<facet::CopyFieldOptions>());
static_assert(OptionsFollowConventions<facet::FieldValidationOptions>());
static_assert(OptionsFollowConventions<facet::ValidationSummaryOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::SecureReveal{} == facet::SecureReveal::Toggle);

// Every placeholder of these widgets once, with the overloads, as an app would call it.
void CallTextInputStubs(Values& v)
{
    using namespace facet;
    char* const buffer = v.buffer.data();
    const std::size_t bufferSize = v.buffer.size();
    FACET_CHECK_STUB(false, FilterField("Filter 1", &v.text, &v.filter));
    FACET_CHECK_STUB(false, FilterField("Filter 2", buffer, bufferSize, nullptr, {.placeholder = "Placeholder"}));
    CHECK(PassesFilter(v.text, v.filter, "Sample text"));
    FACET_CHECK_STUB(false, InlineRename("Rename 1", &v.text, &v.editing));
    FACET_CHECK_STUB(false, InlineRename("Rename 2", buffer, bufferSize, nullptr, {.selectStem = true}));
    FACET_CHECK_STUB(false, MaskedInput("Masked 1", &v.text, "9999-9999"));
    FACET_CHECK_STUB(false, MaskedInput("Masked 2", buffer, bufferSize, nullptr));
    FACET_CHECK_STUB(false, PinCodeInput("Pin 1", &v.text, {.masked = true}));
    FACET_CHECK_STUB(false, PinCodeInput("Pin 2", buffer, bufferSize, {.length = 4}));
    FACET_CHECK_STUB(false, PathField("Path 1", &v.text));
    FACET_CHECK_STUB(false, PathField("Path 2", buffer, bufferSize, {.browseButton = false}));
    CHECK_FALSE(PathFieldBrowsePressed());
}

// Number of FACET_CHECK_STUB and FACET_CHECK_LAYOUT_STUB calls in CallTextInputStubs.
constexpr int kTextInputStubCalls = 10;

} // namespace

TEST_CASE("Every text input placeholder returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallTextInputStubs, kTextInputStubCalls);
}

TEST_CASE("Text input placeholders called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::PathField("Path 1", &values.text));
    facet::FieldValidation("Message");
    CHECK(Describe(values) == pristine);
}

TEST_CASE("Text input placeholders take their width in the order of the conventions")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float units = 0.0f;
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        units = facet::Px(1.0f);
        facet::PathField("Default", static_cast<std::string*>(nullptr));
        session.Track("default");
        facet::PathField("Units", static_cast<std::string*>(nullptr), {.width = 200.0f});
        session.Track("units");
        facet::PathField("Fill", static_cast<std::string*>(nullptr), {.width = facet::kFill});
        session.Track("fill");
        facet::PathField("Minus", static_cast<std::string*>(nullptr), {.width = -50.0f});
        session.Track("minus");
        ImGui::SetNextItemWidth(150.0f);
        facet::PathField("Next", static_cast<std::string*>(nullptr));
        session.Track("next");
        // The option wins over SetNextItemWidth, and the request does not leak to the next item.
        ImGui::SetNextItemWidth(150.0f);
        facet::PathField("Both", static_cast<std::string*>(nullptr), {.width = 100.0f});
        session.Track("both");
        facet::PathField("After", static_cast<std::string*>(nullptr));
        session.Track("after");
        ImGui::PushItemWidth(120.0f);
        facet::PathField("Pushed", static_cast<std::string*>(nullptr));
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

TEST_CASE("The buffer form of InlineRename reads no further than the buffer size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    // The first three bytes are the buffer; the bytes after them belong to someone else.
    char memory[16] = "abcdefghijklmno";
    float expected = 0.0f;
    session.SetUi([&] {
        expected = facet::MeasureText(facet::TextStyle::Body, "abc").x + 2.0f * facet::Px(facet::T().spacing.sm);
        facet::InlineRename("rename", memory, 3, nullptr);
        session.Track("rename");
    });
    session.Frame();
    CHECK(session.ItemRect("rename").GetWidth() == doctest::Approx(expected));
}

TEST_CASE("The pin code placeholder fits a short row and follows the width rules")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string code;
    float available = 0.0f;
    session.SetUi([&] {
        facet::PinCodeInput("Code##wide", &code);
        session.Track("pin wide");
        // A row far narrower than the cells.
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(200.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::PinCodeInput("Code", &code);
        session.Track("pin");
        ImGui::SetNextItemWidth(facet::Px(30.0f));
        facet::PinCodeInput("Code##next", &code);
        session.Track("pin next");
        facet::PinCodeInput("Code##asked", &code, {.width = 300.0f});
        session.Track("pin asked");
        ImGui::EndChild();
    });
    session.Frame();

    REQUIRE(available > 1.0f);
    CHECK(session.ItemRect("pin wide").GetWidth() > available);
    CHECK(session.ItemRect("pin").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("pin next").GetWidth() == doctest::Approx(facet::Px(30.0f)));
    CHECK(session.ItemRect("pin asked").GetWidth() == doctest::Approx(facet::Px(300.0f)));
}

TEST_CASE("FieldValidation sits under the validated item and keeps it as the last item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool checked = false;
    ImGuiID checkboxId = 0;
    ImGuiID lastAfter = 0;
    ImRect checkbox;
    ImVec2 drawnMin(FLT_MAX, FLT_MAX);
    float spacing = 0.0f;
    float cursorAfter = 0.0f;
    session.SetUi([&] {
        spacing = facet::Px(facet::T().spacing.xs);
        // The validated item does not start at the line start, so the message must follow it.
        ImGui::TextUnformatted("Label");
        ImGui::SameLine();
        ImGui::Checkbox("Option 1##validated", &checked);
        checkboxId = GImGui->LastItemData.ID;
        checkbox = GImGui->LastItemData.Rect;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int firstVertex = drawList->VtxBuffer.Size;
        facet::FieldValidation("Message", {.outline = false});
        for (int i = firstVertex; i < drawList->VtxBuffer.Size; ++i)
            drawnMin = ImMin(drawnMin, drawList->VtxBuffer[i].pos);
        lastAfter = GImGui->LastItemData.ID;
        cursorAfter = ImGui::GetCursorScreenPos().y;
    });
    session.Frame();

    REQUIRE(checkboxId != 0);
    CHECK(lastAfter == checkboxId);
    // Within a pixel: anti-aliased fringes lie half a pixel outside the shapes.
    CHECK(std::abs(drawnMin.x - checkbox.Min.x) <= 1.0f);
    CHECK(std::abs(drawnMin.y - (checkbox.Max.y + spacing)) <= 1.0f);
    CHECK(cursorAfter > drawnMin.y);
}

TEST_CASE("FieldValidation follows items without an id and ignores the title bar")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int port = 8;
    ImRect field;
    ImGuiID fieldId = 1;
    ImVec2 fieldDrawn(FLT_MAX, FLT_MAX);
    ImVec2 firstDrawn(FLT_MAX, FLT_MAX);
    ImVec2 contentStart;
    float spacing = 0.0f;
    const auto drawnMin = [](int firstVertex) {
        ImVec2 result(FLT_MAX, FLT_MAX);
        const ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (int i = firstVertex; i < drawList->VtxBuffer.Size; ++i)
            result = ImMin(result, drawList->VtxBuffer[i].pos);
        return result;
    };
    session.SetUi([&] {
        spacing = facet::Px(facet::T().spacing.xs);
        // A stock composite widget is a group: id 0 while idle, with a real rect.
        ImGui::TextUnformatted("Port");
        ImGui::SameLine();
        ImGui::InputInt("##port", &port);
        fieldId = GImGui->LastItemData.ID;
        field = GImGui->LastItemData.Rect;
        int firstVertex = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::FieldValidation("Message", {.outline = false});
        fieldDrawn = drawnMin(firstVertex);
        // First thing in a window with a title bar: the last item is the title bar, so the
        // message starts at the cursor, not under the title bar.
        ImGui::Begin("Titled window", nullptr, ImGuiWindowFlags_NoSavedSettings);
        contentStart = ImGui::GetCursorScreenPos();
        firstVertex = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::FieldValidation("Message", {.outline = false});
        firstDrawn = drawnMin(firstVertex);
        ImGui::End();
    });
    session.Frame();

    REQUIRE(fieldId == 0);
    // Within a pixel: anti-aliased fringes lie half a pixel outside the shapes.
    CHECK(std::abs(fieldDrawn.x - field.Min.x) <= 1.0f);
    CHECK(std::abs(fieldDrawn.y - (field.Max.y + spacing)) <= 1.0f);
    CHECK(std::abs(firstDrawn.x - contentStart.x) <= 1.0f);
    CHECK(std::abs(firstDrawn.y - contentStart.y) <= 1.0f);
}
