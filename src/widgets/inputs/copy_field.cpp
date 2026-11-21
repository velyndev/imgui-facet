// CopyField: a read-only text field on the text field core with a copy button in its trailing
// slot. The core shows the text, lets it be selected and copied, and draws the frame, the label
// in front and the message line; this file adds the copy button with its moment of feedback, the
// reveal button and dots while masked, Ctrl+C with nothing selected, the height of a wrapped
// multi-line value and the skeleton while loading.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/copy_feedback.h"
#include "internal/item.h"
#include "internal/loading.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"

namespace facet
{
namespace
{

// The height of the skeleton bar while loading, as a share of the text line.
constexpr float kCopyFieldSkeletonShare = 0.6f;

// Kept under the field id.
struct CopyFieldState
{
    double copiedAt = -1.0;  // ImGui::GetTime() of the last copy; < 0 before the first
    bool revealed = false;   // masked: the text shows
    bool focused = false;    // the field or one of its buttons had the focus in the last frame
    int lines = 1;           // multiline: the visual lines of the last frame
};

// What the trailing slot reports back to the widget.
struct CopyFieldSlot
{
    bool masked = false;
    bool revealed = false;
    bool copiedLook = false;  // the check and Strings::copied instead of the copy icon
    bool off = false;         // loading or no text: the buttons are disabled
    ImGuiID copyId = 0;
    ImGuiID revealId = 0;
    bool copyPressed = false;
    bool copyKeyboard = false;
    bool revealPressed = false;
    bool revealKeyboard = false;
};

void CopyFieldButtons(const internal::TextFieldSlot& slot, void* user)
{
    CopyFieldSlot& s = *static_cast<CopyFieldSlot*>(user);
    const Strings& strings = GetStrings();
    const internal::ItemState copy =
        internal::TextFieldSlotButton(slot, 0, s.copyId, Icon(s.copiedLook ? "check" : "copy"),
                                      {.tabStop = true, .disabled = s.off,
                                       .name = s.copiedLook ? strings.copied : strings.copy});
    s.copyPressed = copy.pressed;
    s.copyKeyboard = copy.keyboardPressed;
    if (!s.masked)
        return;
    const internal::ItemState reveal =
        internal::TextFieldSlotButton(slot, 1, s.revealId, Icon(s.revealed ? "eye-off" : "eye"),
                                      {.tabStop = true, .selected = s.revealed, .disabled = s.off,
                                       .name = s.revealed ? strings.hide : strings.show});
    s.revealPressed = reveal.pressed;
    s.revealKeyboard = reveal.keyboardPressed;
}

// Ctrl+C or Ctrl+Insert pressed this frame while field `id` has the keyboard with nothing selected.
bool CopyFieldCopyKeyWithoutSelection(ImGuiID id)
{
    const ImGuiContext& g = *GImGui;
    if (g.ActiveId != id)
        return false;
    const bool copyKey = ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_C) ||
                         ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Insert);
    if (!copyKey)
        return false;
    int start = 0;
    int end = 0;
    int caret = 0;
    return !internal::TextFieldGetSelection(id, &start, &end, &caret) || start == end;
}

} // namespace

bool CopyField(const char* label, const char* text, const CopyFieldOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    CopyFieldState& state = internal::GetState<CopyFieldState>(id);
    const bool hasText = text != nullptr && text[0] != '\0';
    const bool disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool off = options.loading || !hasText;

    CopyFieldSlot slot;
    slot.masked = options.masked;
    slot.off = off;
    slot.copyId = ImGui::GetIDWithSeed("copy", nullptr, id);
    slot.revealId = ImGui::GetIDWithSeed("reveal", nullptr, id);

    // The focus left the field and its buttons: back to dots.
    const bool focusedNow = g.NavId == id || g.NavId == slot.copyId || g.NavId == slot.revealId ||
                            g.ActiveId == id || g.ActiveId == slot.copyId || g.ActiveId == slot.revealId;
    if (state.focused && !focusedNow)
        state.revealed = false;
    state.focused = focusedNow;
    slot.revealed = options.masked && state.revealed;
    const double now = ImGui::GetTime();
    slot.copiedLook = internal::CopyFeedbackActive(state.copiedAt);

    bool copied = false;
    if (hasText && !options.loading && !disabled && CopyFieldCopyKeyWithoutSelection(id))
    {
        // Dear ImGui copies the whole text itself when nothing is selected, but not while masked.
        ImGui::SetClipboardText(text);
        copied = true;
    }

    const internal::ControlMetrics metrics = internal::GetControlMetrics(options.size);
    internal::TextFieldParams params;
    params.password = options.masked;
    params.revealed = slot.revealed;
    params.multiline = options.multiline && !options.masked;
    params.wordWrap = true;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = true;
    params.hideText = options.loading;
    params.error = options.error;
    params.helpText = options.helpText;
    params.trailingWidth = internal::TextFieldSlotButtonsWidth(options.masked ? 2 : 1, options.size);
    params.trailingSlot = CopyFieldButtons;
    params.trailingUser = &slot;
    float fontSize = 0.0f;
    {
        const internal::ControlTextScope scope(metrics);
        fontSize = std::round(scope.FontSize());
    }
    if (params.multiline)
    {
        // As tall as the wrapped text of the last frame, at least one line.
        const float padY = std::round((metrics.height - fontSize) * 0.5f);
        params.height = (padY * 2.0f + fontSize * static_cast<float>(std::max(1, state.lines))) / Px(1.0f);
    }

    // The core never writes a read-only field: the caller's text is shown in place, nothing is copied
    // per frame.
    char empty[1] = {0};
    char* shown = hasText ? const_cast<char*>(text) : empty;
    const std::size_t shownSize = hasText ? std::strlen(text) + 1 : 1;

    ImGui::BeginGroup();
    const internal::TextFieldResult field = internal::TextFieldBehavior(label, shown, shownSize, params);
    if (params.multiline)
        state.lines = field.lineCount;

    if (options.loading && ImGui::IsRectVisible(field.text.Min, field.text.Max))
    {
        const float lineHeight = std::round(fontSize * kCopyFieldSkeletonShare);
        const float top = std::round(field.text.GetCenter().y - lineHeight * 0.5f);
        internal::DrawSkeleton(window->DrawList, ImVec2(field.text.Min.x, top), ImVec2(field.text.Max.x, top + lineHeight),
                               Px(T().radii.element));
    }

    if (slot.copyPressed && !off)
    {
        ImGui::SetClipboardText(text);
        copied = true;
    }
    if (slot.revealPressed && !off)
        state.revealed = !state.revealed;
    if (copied)
        state.copiedAt = now;
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return copied;
}

} // namespace facet
