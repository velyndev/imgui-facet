// TextArea: multi-line text on the text field core, with the visible label above the frame, a
// height that grows with the text between minLines and maxLines, an optional grip that drags the
// height, and a line under the frame that holds the message and the character counter.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// Kept under the field id.
struct TextAreaState
{
    int lines = 0;              // visual lines of the text in the last frame
    float resizedUnits = 0.0f;  // the height the grip set, design units; 0 while not resized
    float dragStart = 0.0f;     // pixels, the height when the grip was grabbed
};

// The grip handle: a short bar centered on the bottom edge of the frame, as wide as two grid steps
// of spacing.xxl and spacing.xs thick.
ImRect TextAreaGripRect(const ImRect& frame)
{
    const Theme& t = T();
    const float width = std::round(Px(t.spacing.xxl));
    const float height = std::max(1.0f, std::round(Px(t.spacing.xs)));
    const float x = std::round(frame.GetCenter().x - width * 0.5f);
    const float y = std::round(frame.Max.y - height * 0.5f);
    return ImRect(x, y, x + width, y + height);
}

template <class Call>
bool TextAreaCall(const char* label, bool writable, const TextAreaOptions& options, Call call)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    TextAreaState& state = internal::GetState<TextAreaState>(id);
    const internal::NextItemData next = internal::TakeNextItemData();
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const float totalWidth = internal::ResolveItemWidth(options.width, next, kFill);
    const float unit = Px(1.0f);

    // Lines shown: minLines, growing with the text up to maxLines, or the height the grip set.
    const int minLines = std::max(1, options.minLines);
    int lines = minLines;
    if (options.maxLines > minLines)
        lines = std::clamp(state.lines, minLines, options.maxLines);
    float fontSize = 0.0f;
    {
        const internal::ControlTextScope body(TextStyle::Body);
        fontSize = body.FontSize();
    }
    const float controlHeight = ControlHeight(ControlSize::Regular);
    const float padY = std::round((controlHeight - fontSize) * 0.5f);
    const float minHeight = std::round(padY * 2.0f + std::round(fontSize) * static_cast<float>(minLines));
    float height = std::round(padY * 2.0f + std::round(fontSize) * static_cast<float>(lines));
    if (options.resizable && state.resizedUnits > 0.0f)
        height = std::max(minHeight, std::round(state.resizedUnits * unit));

    ImGui::BeginGroup();
    // The visible label above the frame (rule 6: a tall control).
    const internal::AboveLabel above = internal::MeasureAboveLabel(label, ControlSize::Regular);
    if (above.height > 0.0f)
    {
        internal::DrawAboveLabel(window->DrawList, above, window->DC.CursorPos, totalWidth, ControlSize::Regular,
                                 disabled);
        ImGui::ItemSize(ImVec2(totalWidth, above.height));
    }

    internal::TextFieldParams params;
    params.placeholder = options.placeholder;
    params.multiline = true;
    params.wordWrap = options.wordWrap;
    params.maxLength = options.maxLength;
    params.callback = options.callback;
    params.callbackUser = options.callbackUser;
    params.flags = options.callback != nullptr ? internal::text_inputs::TextCallbackEvents(options.callbackEvents)
                                               : ImGuiInputTextFlags_None;
    if (options.allowTab)
        params.flags |= ImGuiInputTextFlags_AllowTabInput;
    params.width = totalWidth / unit;
    params.height = height / unit;
    params.readOnly = options.readOnly;
    params.mixed = options.mixed;
    params.labelHidden = true;
    internal::ApplyNextItemData(next);
    const internal::TextFieldResult field = call(label, params);
    state.lines = field.lineCount;
    ImDrawList* drawList = window->DrawList;

    // The grip: drag it, or focus it with the arrows and press Up or Down, to change the height.
    if (options.resizable)
    {
        const ImRect grip = TextAreaGripRect(field.frame);
        internal::ItemOptions gripOptions;
        gripOptions.press = internal::PressMode::Click;
        gripOptions.tabStop = false;
        gripOptions.handCursor = false;
        const ImGuiID gripId = ImGui::GetIDWithSeed("grip", nullptr, id);
        const internal::ItemState item = internal::PartBehavior(gripId, grip, gripOptions);
        if ((item.hovered || item.held) && !item.disabled)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        const float current = field.frame.GetHeight();
        if (item.activated)
            state.dragStart = current;
        if (item.held && !item.keyboardPressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float dragged = std::max(minHeight, std::round(state.dragStart + ImGui::GetMouseDragDelta(0, 0.0f).y));
            state.resizedUnits = dragged / unit;
        }
        if (item.focused && !item.disabled)
        {
            ImGui::SetKeyOwner(ImGuiKey_UpArrow, gripId);
            ImGui::SetKeyOwner(ImGuiKey_DownArrow, gripId);
            const float step = std::round(fontSize);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, gripId))
                state.resizedUnits = (current + step) / unit;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, gripId))
                state.resizedUnits = std::max(minHeight, current - step) / unit;
        }
        if (item.visible)
        {
            const ImVec4 color = item.disabled              ? t.colors.labelTertiary
                                 : (item.hovered || item.held) ? t.colors.labelSecondary
                                                               : t.colors.labelTertiary;
            FillRect(drawList, grip.Min, grip.Max, ToU32(color), grip.GetHeight() * 0.5f);
            internal::DrawItemFocusRing(item, grip.GetHeight() * 0.5f);
        }
    }

    // The line under the frame: the message at the start, the counter at the end.
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    char counter[32] = {};
    float counterWidth = 0.0f;
    if (options.counter)
    {
        if (options.maxLength > 0)
            std::snprintf(counter, sizeof(counter), "%d / %d", field.length, options.maxLength);
        else
            std::snprintf(counter, sizeof(counter), "%d", field.length);
        counterWidth = MeasureText(TextStyle::Footnote, counter).x;
    }
    const float gap = Px(t.spacing.md);
    const float messageWidth = std::max(1.0f, field.frame.GetWidth() - (counterWidth > 0.0f ? counterWidth + gap : 0.0f));
    float bottom = internal::DrawControlMessage(message, ImVec2(field.frame.Min.x, field.frame.Max.y), messageWidth,
                                                disabled, id, field.frame);
    if (counterWidth > 0.0f)
    {
        const float top = field.frame.Max.y + Px(t.spacing.xs);
        PushTextStyle(TextStyle::Footnote);
        drawList->AddText(ImVec2(field.frame.Max.x - counterWidth, top), ToU32(internal::ControlSecondaryColor(disabled)),
                          counter);
        bottom = std::max(bottom, top + ImGui::GetFontSize());
        PopTextStyle();
    }
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, bottom);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return field.edited && writable && !options.readOnly;
}

} // namespace

bool TextArea(const char* label, std::string* text, const TextAreaOptions& options)
{
    return TextAreaCall(label, internal::text_inputs::TextWritable(text), options,
                        [&](const char* l, const internal::TextFieldParams& p) {
                            return internal::TextFieldBehavior(l, text, p);
                        });
}

bool TextArea(const char* label, char* buffer, std::size_t bufferSize, const TextAreaOptions& options)
{
    return TextAreaCall(label, internal::text_inputs::TextWritable(buffer, bufferSize), options,
                        [&](const char* l, const internal::TextFieldParams& p) {
                            return internal::TextFieldBehavior(l, buffer, bufferSize, p);
                        });
}

} // namespace facet
