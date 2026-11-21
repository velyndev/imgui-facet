// InlineRename: a text that turns into a field in place. While idle it is one item showing the
// text (hover fill, focus ring, cut with an ellipsis and a tooltip when narrow); a double click,
// F2 or the caller's flag starts an edit on a copy kept under the id, in a text field on the text
// field core. Enter or focus loss writes the copy back once, Escape drops it.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cstring>
#include <string_view>

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

// Frames an edit may wait for the field to take the keyboard before it is given up (the focus
// request of the text field core waits two).
constexpr int kInlineRenameStartFrames = 3;

// Kept under the id.
struct InlineRenameState
{
    std::string edit;      // the copy being edited
    bool editing = false;  // used when the caller passes no flag
    bool open = false;     // an edit is under way (the field is shown)
    bool started = false;  // the field has had the keyboard during this edit
    int waited = 0;        // frames the open edit has waited for the keyboard
};

// The caller's text as it is now; the buffer form never reads past bufferSize.
std::string_view InlineRenameText(const std::string* text, const char* buffer, std::size_t bufferSize)
{
    if (text != nullptr)
        return *text;
    if (buffer == nullptr || bufferSize == 0)
        return {};
    const void* zero = std::memchr(buffer, 0, bufferSize);
    return std::string_view(buffer, zero != nullptr ? static_cast<std::size_t>(static_cast<const char*>(zero) - buffer)
                                                    : bufferSize);
}

// The byte length of the name without its extension: up to the last '.', unless that dot starts
// the name (".config") or there is none.
int InlineRenameStem(std::string_view name)
{
    const std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0)
        return static_cast<int>(name.size());
    return static_cast<int>(dot);
}

bool InlineRenameCall(const char* id, std::string* text, char* buffer, std::size_t bufferSize, bool* editing,
                      const InlineRenameOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const ImGuiID itemId = window->GetID(id);
    InlineRenameState& state = internal::GetState<InlineRenameState>(itemId);
    bool& flag = editing != nullptr ? *editing : state.editing;
    const internal::NextItemData next = internal::TakeNextItemData();
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool writable = text != nullptr || (buffer != nullptr && bufferSize > 0);
    const bool readOnly = options.readOnly || !writable || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const std::string_view current = InlineRenameText(text, buffer, bufferSize);
    const Theme& t = T();
    const internal::ControlMetrics metrics = internal::GetControlMetrics(options.size);
    const float pad = Px(t.spacing.sm);
    bool written = false;

    if ((readOnly || disabled) && flag)
        flag = false;

    // An edit starts: copy the text and ask the field for the keyboard.
    if (flag && !state.open)
    {
        try
        {
            state.edit.assign(current);
        }
        catch (...)
        {
            flag = false;
        }
        if (flag)
        {
            state.open = true;
            state.started = false;
            state.waited = 0;
            internal::TextFieldRequestFocus(itemId, internal::TextFieldFocus::SelectAll);
            if (options.selectStem)
                internal::TextFieldSetSelection(itemId, 0, InlineRenameStem(state.edit));
        }
    }
    else if (!flag && state.open)
    {
        state.open = false;  // the caller closed it: the copy is dropped
    }

    ImGui::BeginGroup();
    float frameBottom = 0.0f;
    if (state.open)
    {
        internal::TextFieldParams params;
        params.maxLength = options.maxLength;
        params.size = options.size;
        params.width = options.width > 0.0f ? options.width : kFill;
        params.labelHidden = true;
        params.error = options.error;
        internal::ApplyNextItemData(next);
        const internal::TextFieldResult field = internal::TextFieldBehavior(id, &state.edit, params);
        frameBottom = field.frame.Max.y;
        if (field.active)
            state.started = true;
        else if (!state.started && ++state.waited > kInlineRenameStartFrames)
            state.open = false;  // the field never took the keyboard (hidden, or the request was dropped)

        // Enter or focus loss ends the edit and writes the copy once; Escape drops it.
        if (state.started && !field.active)
        {
            if (!field.canceled && state.edit != current)
            {
                written = internal::text_inputs::WriteFieldText(text, buffer, bufferSize, state.edit.c_str());
            }
            state.open = false;
        }
        if (!state.open)
            flag = false;
    }
    else
    {
        // Idle: the text as one item, as wide as the text unless a width is asked for.
        const internal::ControlTextScope scope(metrics);
        const char* begin = current.data();
        const char* end = current.data() + current.size();
        const float content = ImGui::CalcTextSize(begin, end).x + pad * 2.0f;
        const float width = internal::ResolveItemWidth(options.width, next, content);
        internal::ApplyNextItemData(next);
        internal::ItemOptions itemOptions;
        itemOptions.readOnly = readOnly;
        itemOptions.handCursor = false;
        const internal::ItemState item = internal::ItemBehavior(itemId, ImVec2(width, metrics.height), itemOptions);
        const internal::FittedText shown = internal::FitText(begin, end, width - pad * 2.0f);
        if (item.visible)
        {
            ImDrawList* drawList = window->DrawList;
            if (!readOnly && !item.disabled)
            {
                const ImVec4 rest = WithAlpha(t.colors.fill[2], 0.0f);
                FillRect(drawList, item.rect.Min, item.rect.Max,
                         ToU32(internal::StateFill(rest, t.colors.fill[2], t.colors.fill[1], item)), metrics.radius);
            }
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                              ImVec2(item.rect.Min.x + pad, CenteredTextY(item.rect.Min.y, metrics.height)),
                              ToU32(internal::ControlLabelColor(item.disabled)), shown.text);
            internal::DrawItemFocusRing(item, metrics.radius);
        }
        internal::CutTextTooltip(item, begin, end, shown.truncated);
        const bool start = !readOnly && !item.disabled &&
                           ((options.startOnDoubleClick && item.hovered &&
                             ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
                            (options.startOnF2 && item.focused && ImGui::IsKeyPressed(ImGuiKey_F2, false)));
        if (start)
            flag = true;  // the field opens in the next frame
        const internal::ControlMessage message = internal::PickControlMessage(options.error, nullptr);
        frameBottom = internal::DrawControlMessage(message, ImVec2(item.rect.Min.x, item.rect.Max.y), item.rect.GetWidth(),
                                                   item.disabled, itemId, item.rect);
    }
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, frameBottom);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(itemId);
    return written;
}

} // namespace

bool InlineRename(const char* id, std::string* text, bool* editing, const InlineRenameOptions& options)
{
    return InlineRenameCall(id, text, nullptr, 0, editing, options);
}

bool InlineRename(const char* id, char* buffer, std::size_t bufferSize, bool* editing,
                  const InlineRenameOptions& options)
{
    return InlineRenameCall(id, nullptr, buffer, bufferSize, editing, options);
}

} // namespace facet
