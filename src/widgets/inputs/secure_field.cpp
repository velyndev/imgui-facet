// SecureField: a password field on the text field core. The core draws the dots and turns copying
// off while the text is masked; this file adds the reveal button in the trailing slot, the reset to
// masked when the focus leaves, the strength meter and the Caps Lock hint under the frame, and
// keeps copy and cut off while the text shows as well, so the secret never reaches the clipboard.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// Kept under the field id.
struct SecureFieldState
{
    bool revealed = false;  // Toggle: the text shows
    bool held = false;      // Hold: the reveal button was held in the last frame
    bool focused = false;   // the field or its button had the focus in the last frame
};

// What the trailing slot reports back to the widget.
struct SecureRevealSlot
{
    SecureReveal mode = SecureReveal::Toggle;
    bool shown = false;
    ImGuiID buttonId = 0;
    bool pressed = false;
    bool keyboardPressed = false;
    bool held = false;
};

void SecureRevealButton(const internal::TextFieldSlot& slot, void* user)
{
    SecureRevealSlot& reveal = *static_cast<SecureRevealSlot*>(user);
    const Strings& strings = GetStrings();
    reveal.buttonId = ImGui::GetIDWithSeed("reveal", nullptr, slot.fieldId);
    const bool toggle = reveal.mode == SecureReveal::Toggle;
    const internal::ItemState item = internal::TextFieldSlotButton(
        slot, 0, reveal.buttonId, Icon(reveal.shown ? "eye-off" : "eye"),
        {.tabStop = true, .selected = toggle && reveal.shown, .name = (toggle && reveal.shown) ? strings.hide : strings.show});
    reveal.pressed = item.pressed;
    reveal.keyboardPressed = item.keyboardPressed;
    reveal.held = item.held;
}

// The strength meter: a progressHeightSmall track the width of the frame, filled to `strength`
// in the destructive, warning or success color by thirds, so a weak value never reads as good.
float DrawStrengthMeter(ImDrawList* drawList, ImVec2 at, float width, float strength, bool disabled)
{
    const Theme& t = T();
    const Colors& c = t.colors;
    const float thickness = PxSnapped(t.metrics.progressHeightSmall);
    const float top = std::round(at.y + Px(t.spacing.xs));
    const ImRect track(ImVec2(at.x, top), ImVec2(at.x + width, top + thickness));
    const float radius = std::min(Px(t.radii.element), thickness * 0.5f);
    FillRect(drawList, track.Min, track.Max, ToU32(disabled ? c.fill[3] : c.fill[0]), radius);
    const float value = std::clamp(strength, 0.0f, 1.0f);
    if (value > 0.0f)
    {
        constexpr float kWeakBelow = 1.0f / 3.0f;
        constexpr float kStrongFrom = 2.0f / 3.0f;
        const ImVec4 tone = disabled             ? c.labelTertiary
                            : value < kWeakBelow  ? c.destructive
                            : value < kStrongFrom ? c.warning
                                                  : c.success;
        const float x1 = std::max(track.Min.x + thickness, std::round(track.Min.x + track.GetWidth() * value));
        FillRect(drawList, track.Min, ImVec2(std::min(x1, track.Max.x), track.Max.y), ToU32(tone), radius);
    }
    return track.Max.y;
}

// Takes Ctrl+C, Ctrl+X, Ctrl+Insert and Shift+Delete away from the field for this frame.
void BlockSecureCopy(ImGuiID owner)
{
    // Only while the modifier is down, so plain Delete still deletes (letters arrive as characters).
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl)
    {
        ImGui::SetKeyOwner(ImGuiKey_C, owner, ImGuiInputFlags_LockThisFrame);
        ImGui::SetKeyOwner(ImGuiKey_X, owner, ImGuiInputFlags_LockThisFrame);
        ImGui::SetKeyOwner(ImGuiKey_Insert, owner, ImGuiInputFlags_LockThisFrame);
    }
    if (io.KeyShift)
        ImGui::SetKeyOwner(ImGuiKey_Delete, owner, ImGuiInputFlags_LockThisFrame);
}

template <class Call>
bool SecureFieldCall(const char* label, bool writable, const SecureFieldOptions& options, Call call)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    SecureFieldState& state = internal::GetState<SecureFieldState>(id);
    const ImGuiID buttonId = ImGui::GetIDWithSeed("reveal", nullptr, id);

    // The focus left the field and its button: back to dots.
    const bool focusedNow = g.NavId == id || g.NavId == buttonId || g.ActiveId == id || g.ActiveId == buttonId;
    if (state.focused && !focusedNow)
        state.revealed = false;
    state.focused = focusedNow;

    bool shown = false;
    if (options.reveal == SecureReveal::Toggle)
        shown = state.revealed;
    else if (options.reveal == SecureReveal::Hold)
        shown = state.held;

    // Shown text is still not copied: the copy keys belong to the button id for this frame.
    if (g.ActiveId == id)
        BlockSecureCopy(buttonId);

    SecureRevealSlot reveal;
    reveal.mode = options.reveal;
    reveal.shown = shown;
    internal::TextFieldParams params;
    params.placeholder = options.placeholder;
    params.password = true;
    params.revealed = shown;
    params.maxLength = options.maxLength;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    if (options.reveal != SecureReveal::None)
    {
        params.trailingWidth = internal::TextFieldSlotButtonsWidth(1, options.size);
        params.trailingSlot = SecureRevealButton;
        params.trailingUser = &reveal;
    }

    const bool disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImGui::BeginGroup();
    const internal::TextFieldResult field = call(label, params);

    if (options.reveal == SecureReveal::Toggle && reveal.pressed)
        state.revealed = !state.revealed;
    state.held = options.reveal == SecureReveal::Hold && reveal.held;
    // A click on the button gives the keyboard back to the text, so typing goes on.
    if (reveal.pressed && !reveal.keyboardPressed && !options.readOnly)
        internal::TextFieldRequestFocus(id, internal::TextFieldFocus::Keep);

    // Under the frame: the strength meter, then one message line.
    ImDrawList* drawList = window->DrawList;
    float bottom = field.frame.Max.y;
    if (options.strength >= 0.0f)
        bottom = DrawStrengthMeter(drawList, ImVec2(field.frame.Min.x, bottom), field.frame.GetWidth(),
                                   options.strength, disabled);
    const bool capsHint = options.capsLockHint && field.active && internal::text_inputs::CapsLockLikelyOn();
    internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    if (capsHint && options.error == nullptr)
        message = {GetStrings().capsLock, false};
    bottom = internal::DrawControlMessage(message, ImVec2(field.frame.Min.x, bottom), field.frame.GetWidth(), disabled,
                                          id, field.frame);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, bottom);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return field.edited && writable && !options.readOnly;
}

} // namespace

bool SecureField(const char* label, std::string* text, const SecureFieldOptions& options)
{
    return SecureFieldCall(label, internal::text_inputs::TextWritable(text), options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, text, p);
                           });
}

bool SecureField(const char* label, char* buffer, std::size_t bufferSize, const SecureFieldOptions& options)
{
    return SecureFieldCall(label, internal::text_inputs::TextWritable(buffer, bufferSize), options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, buffer, bufferSize, p);
                           });
}

} // namespace facet
