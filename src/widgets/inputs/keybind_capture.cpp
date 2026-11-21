// KeybindCapture: records a key bind into one or two slots, each a field-like item that shows
// its chord as key caps (src/internal/key_caps.h), with an optional bind mode selector after
// them. Listening runs on the chord capture session of the keys block (src/internal/keys.h),
// which owns every key while it listens, so no shortcut, key bind or navigation sees the keys.
//
// Layout, left to right inside the widget width: the visible label (rule 6), the primary slot,
// the alternate slot (showAlternate), the KeybindModeSelector (showMode, selection.h) when it
// fits beside the slots, else on the line below them; the message line under the slots.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/selection.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/key_caps.h"
#include "internal/keys.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/value_drag.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

constexpr int kKeybindSlots = 2;

struct KeybindCaptureState
{
    internal::ChordCapture capture;
    int listeningSlot = -1;   // the slot the capture writes, -1 while not listening
    float modeWidth = 0.0f;   // pixels the mode selector took in the last frame
};

ImGuiKeyChord* KeybindSlotChord(Keybind* bind, int slot)
{
    if (bind == nullptr)
        return nullptr;
    return slot == 0 ? &bind->primary : &bind->alternate;
}

// The text of a chord for the cut form and its tooltip ("Ctrl+Shift+K").
struct KeybindChordText
{
    char text[64] = {};
};

KeybindChordText KeybindText(ImGuiKeyChord chord)
{
    KeybindChordText out;
    ChordToText(chord, out.text, static_cast<int>(sizeof(out.text)));
    return out;
}

// What one slot did this frame.
struct KeybindSlotResult
{
    bool written = false;
};

struct KeybindSlotDesc
{
    ImGuiID id = 0;
    int index = 0;
    ImRect rect;
    ImGuiKeyChord chord = ImGuiKey_None;
    bool readOnly = false;
    bool error = false;
    const char* placeholder = nullptr;
    const internal::ControlMetrics* metrics = nullptr;
    const internal::ForcedItemRequest* forced = nullptr;
};

// Draws the content of a slot: caps, or the cut chord text, or a secondary text. Returns whether
// the content was cut, and writes the text shown for the tooltip.
bool DrawKeybindSlotContent(ImDrawList* drawList, const ImRect& area, ImGuiKeyChord chord, bool listening,
                            ImGuiKeyChord held, const char* placeholder, const internal::ControlMetrics& m,
                            bool disabled, KeybindChordText* shownText)
{
    const ImGuiKeyChord shown = listening ? held : chord;
    const float centerY = area.GetCenter().y;
    if (shown != ImGuiKey_None)
    {
        const float capsWidth = internal::MeasureKeyCaps(shown, m.size);
        if (capsWidth <= area.GetWidth())
        {
            internal::DrawKeyCaps(drawList, shown, ImVec2(area.Min.x, centerY), m.size, disabled, listening);
            return false;
        }
        // Too narrow for the caps: the chord as text, cut with an ellipsis.
        *shownText = KeybindText(shown);
        internal::ControlTextScope text(m);
        const internal::FittedText fitted = internal::FitText(shownText->text, nullptr, area.GetWidth());
        const ImU32 color = ToU32(listening ? internal::ControlSecondaryColor(disabled) : internal::ControlLabelColor(disabled));
        drawList->AddText(ImVec2(area.Min.x, CenteredTextY(area.Min.y, area.GetHeight())), color, fitted.text);
        return fitted.truncated;
    }
    const char* text = listening ? GetStrings().listening : placeholder;
    internal::ControlTextScope scope(m);
    const internal::FittedText fitted = internal::FitText(text, nullptr, area.GetWidth());
    drawList->AddText(ImVec2(area.Min.x, CenteredTextY(area.Min.y, area.GetHeight())),
                      ToU32(internal::ControlSecondaryColor(disabled)), fitted.text);
    if (fitted.truncated)
        ImStrncpy(shownText->text, text, sizeof(shownText->text));
    return fitted.truncated;
}

} // namespace

bool KeybindCapture(const char* label, Keybind* bind, const KeybindCaptureOptions& options)
{
    // 1. Entry.
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    KeybindCaptureState& state = internal::GetState<KeybindCaptureState>(id);
    const bool disabled = ((next.itemFlags | g.CurrentItemFlags) & ImGuiItemFlags_Disabled) != 0;
    const bool readOnly = options.readOnly || bind == nullptr || ((next.itemFlags | g.CurrentItemFlags) & ImGuiItemFlags_ReadOnly) != 0;
    const char* placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().unbound;
    const int slotCount = options.showAlternate ? 2 : 1;

    // 2. Width and layout.
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width);
    const float gap = std::round(Px(t.spacing.sm));
    const float slotsLeft = front.width;
    float slotsWidth = std::max(1.0f, width - front.width);
    bool modeBeside = false;
    if (options.showMode && state.modeWidth > 0.0f)
    {
        const float slotMinimum = std::round(Px(t.metrics.numberFieldWidth)) * static_cast<float>(slotCount);
        if (slotsWidth - state.modeWidth - gap >= slotMinimum)
        {
            modeBeside = true;
            slotsWidth -= state.modeWidth + gap;
        }
    }
    const float slotWidth =
        std::max(1.0f, std::floor((slotsWidth - gap * static_cast<float>(slotCount - 1)) / static_cast<float>(slotCount)));

    // The session ends when a frame passes without this widget: a stale slot index is dropped.
    if (!state.capture.Listening())
        state.listeningSlot = -1;

    ImGui::BeginGroup();
    const ImVec2 origin = window->DC.CursorPos;
    bool written = false;
    bool anyFocusVisible = false;
    ImRect slotsRect(origin.x + slotsLeft, origin.y, origin.x + slotsLeft, origin.y + m.height);
    ImGuiID firstSlotId = 0;
    ImDrawList* drawList = window->DrawList;

    // 3. The slots.
    for (int slot = 0; slot < slotCount; ++slot)
    {
        const ImGuiID slotId = ImGui::GetIDWithSeed(slot == 0 ? "primary" : "alternate", nullptr, id);
        if (slot == 0)
            firstSlotId = slotId;
        const float x = origin.x + slotsLeft + static_cast<float>(slot) * (slotWidth + gap);
        const ImRect rect(x, origin.y, x + slotWidth, origin.y + m.height);
        slotsRect.Max.x = rect.Max.x;
        ImGuiKeyChord* chord = KeybindSlotChord(bind, slot);
        const ImGuiKeyChord current = chord != nullptr ? *chord : ImGuiKey_None;
        const bool listeningHere = state.capture.Listening() && state.listeningSlot == slot;

        internal::ItemState item;
        ImGui::SetCursorScreenPos(rect.Min);
        if (listeningHere)
        {
            // While listening the slot is the active item and stays so; its button behaviour does
            // not run (it would drop the activity when the mouse button is up).
            ImGui::ItemSize(rect.GetSize());
            item.id = slotId;
            item.rect = rect;
            item.hitRect = rect;
            item.visible = ImGui::ItemAdd(rect, slotId);
            item.hovered = ImGui::IsItemHovered();
            item.active = true;
            item.focused = g.NavId == slotId;
            const internal::ChordCaptureEvent event = state.capture.Update();
            if (event == internal::ChordCaptureEvent::Captured)
            {
                if (chord != nullptr && !readOnly && *chord != state.capture.Result())
                {
                    *chord = state.capture.Result();
                    written = true;
                    ImGui::MarkItemEdited(slotId);
                }
            }
            else if (event == internal::ChordCaptureEvent::Canceled)
            {
                internal::MarkEditCanceled(slotId);
            }
            if (event != internal::ChordCaptureEvent::None)
            {
                state.listeningSlot = -1;
                if (g.ActiveId == slotId)
                    ImGui::ClearActiveID();
            }
        }
        else
        {
            internal::ItemOptions itemOptions;
            itemOptions.part = slot;
            itemOptions.forced = &forced;
            itemOptions.readOnly = readOnly;
            itemOptions.allowOverlap = true;  // the clear button sits inside the slot
            itemOptions.baseline = CenteredTextY(rect.Min.y, m.height) - rect.Min.y;
            if (slot == 0)
                internal::ApplyNextItemData(next);
            item = internal::ItemBehavior(slotId, rect.GetSize(), itemOptions);
            if (item.pressed && !readOnly && !item.disabled && item.forced == internal::ForcedItemState::None)
            {
                // A click, Enter or Space starts listening; the keys that started it are ignored
                // until they are released.
                state.capture.Start(slotId, {.allowMouse = options.allowMouse,
                                             .allowGamepad = options.allowGamepad,
                                             .allowModifierOnly = options.allowModifierOnly});
                state.listeningSlot = slot;
                ImGui::SetActiveID(slotId, window);
                ImGui::SetFocusID(slotId, window);
                g.ActiveIdSource = ImGuiInputSource_None;
            }
            else if (item.focused && !readOnly && !item.disabled && current != ImGuiKey_None)
            {
                // Backspace or Delete clears the focused slot.
                ImGui::SetKeyOwner(ImGuiKey_Backspace, slotId);
                ImGui::SetKeyOwner(ImGuiKey_Delete, slotId);
                if (ImGui::IsKeyPressed(ImGuiKey_Backspace, ImGuiInputFlags_None, slotId) ||
                    ImGui::IsKeyPressed(ImGuiKey_Delete, ImGuiInputFlags_None, slotId))
                {
                    *chord = ImGuiKey_None;
                    written = true;
                    ImGui::MarkItemEdited(slotId);
                }
            }
        }
        anyFocusVisible = anyFocusVisible || item.focusVisible;
        const bool forcedListening = Has(item.forced, internal::ForcedItemState::Pressed) ||
                                     Has(item.forced, internal::ForcedItemState::Open);
        const bool listening = (state.capture.Listening() && state.listeningSlot == slot) || forcedListening;
        const ImGuiKeyChord shownChord = chord != nullptr ? *chord : ImGuiKey_None;

        // The clear button, while there is something to clear. A left press on it also ends
        // listening (the capture lets the press through), then the release clears.
        const bool clearShown = !readOnly && !item.disabled && (shownChord != ImGuiKey_None || listening);
        const float buttonsWidth = clearShown ? std::round(Px(internal::TextFieldSlotButtonsWidth(1, m.size))) : 0.0f;
        const float gapXs = std::round(Px(t.spacing.xs));
        if (clearShown)
        {
            internal::TextFieldSlot clearSlot;
            clearSlot.rect = ImRect(rect.Max.x - gapXs - buttonsWidth, rect.Min.y, rect.Max.x - gapXs, rect.Max.y);
            clearSlot.fieldId = slotId;
            clearSlot.disabled = item.disabled;
            clearSlot.size = m.size;
            const internal::ItemState clear = internal::TextFieldSlotButton(
                clearSlot, 0, ImGui::GetIDWithSeed("clear", nullptr, slotId), Icon("clear"), {.name = GetStrings().clear});
            if (clear.pressed && chord != nullptr)
            {
                if (state.capture.Listening() && state.listeningSlot == slot)
                {
                    state.capture.Clear();
                    state.listeningSlot = -1;
                }
                if (*chord != ImGuiKey_None)
                {
                    *chord = ImGuiKey_None;
                    written = true;
                    ImGui::MarkItemEdited(clear.id);
                }
            }
        }

        // 4. Draw the slot.
        if (item.visible)
        {
            internal::DrawFieldFrame(drawList, rect,
                                     {.hoverAmount = listening ? 0.0f : item.hoverAmount,
                                      .active = listening,
                                      .disabled = item.disabled,
                                      .readOnly = readOnly,
                                      .error = options.error != nullptr});
            const float pad = m.fieldPadding;
            const ImRect area(rect.Min.x + pad, rect.Min.y, rect.Max.x - (clearShown ? buttonsWidth + gapXs * 2.0f : pad),
                              rect.Max.y);
            const ImGuiKeyChord held = forcedListening ? ImGuiKey_None : state.capture.Held();
            KeybindChordText cut;
            ImGui::PushClipRect(rect.Min, rect.Max, true);
            const bool truncated = DrawKeybindSlotContent(drawList, area, shownChord, listening, held, placeholder, m,
                                                          item.disabled, &cut);
            ImGui::PopClipRect();
            if (!listening)
                internal::DrawItemFocusRing(item, m.radius);
            internal::CutTextTooltip(slotId, rect, item.hovered, item.focusVisible, cut.text, nullptr, truncated);
        }
    }

    // The visible label in front, centered on the slot line.
    if (front.text != nullptr)
    {
        internal::ControlTextScope text(m);
        internal::DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size, disabled);
        const ImRect labelRect(origin, ImVec2(origin.x + front.width, origin.y + m.height));
        internal::CutTextTooltip(id, labelRect, ImGui::IsMouseHoveringRect(labelRect.Min, labelRect.Max), anyFocusVisible,
                                 front.text, front.end, front.shown.truncated);
    }

    // The bind mode selector.
    float lineBottom = origin.y + m.height;
    if (options.showMode)
    {
        BindMode mode = bind != nullptr ? bind->mode : BindMode::Hold;
        const ImVec2 at = modeBeside ? ImVec2(slotsRect.Max.x + gap, origin.y)
                                     : ImVec2(origin.x + slotsLeft, lineBottom + std::round(Px(t.spacing.xs)));
        ImGui::SetCursorScreenPos(at);
        if (!modeBeside)
            ImGui::SetNextItemWidth(std::max(1.0f, origin.x + width - at.x));
        // Under the widget id, so two captures in one window keep their selectors apart.
        ImGui::PushOverrideID(id);
        const bool modeWritten = KeybindModeSelector("##mode", &mode, {.size = options.size, .readOnly = readOnly});
        ImGui::PopID();
        if (modeWritten && bind != nullptr && !readOnly)
        {
            bind->mode = mode;
            written = true;
        }
        state.modeWidth = ImGui::GetItemRectSize().x;
        if (!modeBeside)
            lineBottom = ImGui::GetItemRectMax().y;
    }

    // 5. The message line under the slots, then the result.
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    internal::DrawControlMessage(message, ImVec2(slotsRect.Min.x, lineBottom), slotsRect.GetWidth(), disabled,
                                 firstSlotId, slotsRect);
    // The group spans the widget width and every line, so the next item starts below them.
    window->DC.CursorMaxPos.x = std::max(window->DC.CursorMaxPos.x, origin.x + width);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, lineBottom);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
