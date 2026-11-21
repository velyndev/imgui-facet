// BeginKeyValueList, KeyValue and EndKeyValueList (facet/widgets/display.h): read-only labelled
// values whose columns come from LayoutLabeledRow, so keys line up with settings rows and the value
// goes under its key when the row is narrow. Keys in labelSecondary, cut with an ellipsis and a
// tooltip; values in label (or a tone), wrapped or cut. In a copyable list each value is a Tab stop
// that copies itself on activation.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/scope_stack.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

constexpr const char* kKeyValueScope = "BeginKeyValueList";
constexpr int kKeyValueDepth = 4;

struct KeyValueListFrame
{
    float labelWidth = 0.0f;  // units, 0 for the token
    bool copyable = false;
};

struct KeyValueStack
{
    KeyValueListFrame frames[kKeyValueDepth];
    int depth = 0;     // frames in use
    int overflow = 0;  // lists opened beyond kKeyValueDepth, still balanced by their End
};

KeyValueStack& KeyValueLists()
{
    static KeyValueStack stack;
    return stack;
}

} // namespace

void BeginKeyValueList(const char* id, const KeyValueListOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    internal::PushScope(kKeyValueScope);
    KeyValueStack& stack = KeyValueLists();
    if (stack.depth < kKeyValueDepth)
        stack.frames[stack.depth++] = {options.labelWidth, options.copyable};
    else
        ++stack.overflow;
    if (window == nullptr)
        return;
    ImGui::PushID(id != nullptr ? id : "");
    ImGui::BeginGroup();
}

void EndKeyValueList()
{
    if (!internal::PopScope(kKeyValueScope))
        return;
    KeyValueStack& stack = KeyValueLists();
    if (stack.overflow > 0)
        --stack.overflow;
    else if (stack.depth > 0)
        --stack.depth;
    if (internal::ControlWindow() == nullptr)
        return;
    ImGui::EndGroup();
    ImGui::PopID();
}

bool KeyValue(const char* label, const char* value, const KeyValueOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    if (value == nullptr)
        value = "";
    const KeyValueStack& stack = KeyValueLists();
    const KeyValueListFrame frame = stack.depth > 0 ? stack.frames[stack.depth - 1] : KeyValueListFrame{};
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const float width = internal::ResolveItemWidth(0.0f, next, kFill);
    const LabeledRowLayout columns = LayoutLabeledRow(width, frame.labelWidth);
    internal::ControlTextScope scope(TextStyle::Body);
    const float lineHeight = ImGui::GetTextLineHeight();
    const char* keyEnd = ImGui::FindRenderedTextEnd(label);
    const float valueWidth = std::max(1.0f, columns.controlWidth);
    const float valueHeight = options.wrap ? std::max(lineHeight, ImGui::CalcTextSize(value, nullptr, false, valueWidth).y)
                                           : lineHeight;
    const float valueTop = columns.stacked ? lineHeight + Px(t.spacing.xs) : 0.0f;
    const float height = std::ceil(columns.stacked ? valueTop + valueHeight : std::max(lineHeight, valueHeight));

    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    const ImRect valueRect(ImVec2(origin.x + columns.controlOffset, origin.y + valueTop),
                           ImVec2(origin.x + columns.controlOffset + valueWidth, origin.y + valueTop + valueHeight));
    const ImGuiID id = window->GetID(label);
    internal::ItemState part;
    if (frame.copyable)
    {
        ImGui::BeginGroup();
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        internal::ItemOptions itemOptions;
        itemOptions.hitTarget = false;
        // The copyable part, its feedback and focus ring hug the value text, not the whole column.
        const char* firstEnd = std::strchr(value, '\n');
        const float textWidth = options.wrap ? ImGui::CalcTextSize(value, nullptr, false, valueWidth).x
                                             : ImGui::CalcTextSize(value, firstEnd).x;
        const ImRect textRect(valueRect.Min,
                              ImVec2(valueRect.Min.x + std::clamp(std::ceil(textWidth), 1.0f, valueWidth), valueRect.Max.y));
        part = internal::PartBehavior(ImHashStr("##value", 0, id), textRect, itemOptions);
        ImGui::EndGroup();
        internal::FinishWidgetGroup(id);
    }
    else
    {
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
            return false;
    }
    bool copied = false;
    if (part.pressed)
    {
        ImGui::SetClipboardText(value);
        internal::StartDisplayCopiedFeedback(part.id, part.rect);
        copied = true;
    }
    if (!ImGui::IsRectVisible(bounds.Min, bounds.Max))
        return copied;

    const Colors& c = t.colors;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0 || part.disabled;
    ImDrawList* drawList = window->DrawList;
    // The key, cut to its column with the full text on hover.
    const internal::FittedText key = internal::FitText(label, keyEnd, std::max(0.0f, columns.labelWidth));
    drawList->AddText(origin, ToU32(internal::ControlSecondaryColor(disabled)), key.text);
    const ImRect keyRect(origin, ImVec2(origin.x + key.width, origin.y + lineHeight));
    const bool keyHovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(keyRect.Min, keyRect.Max);
    internal::CutTextTooltip(ImHashStr("##key", 0, id), keyRect, keyHovered, part.focusVisible, label, keyEnd, key.truncated);

    // The value, with a faint wash under it while a copyable value is hovered or pressed.
    if (frame.copyable && !disabled && (part.hoverAmount > 0.0f || part.pressAmount > 0.0f))
        FillRect(drawList, part.rect.Min, part.rect.Max,
                 ToU32(WithAlpha(c.fill[2], c.fill[2].w * std::max(part.hoverAmount, part.pressAmount))), Px(t.radii.element));
    const ImU32 valueColor =
        ToU32(disabled ? c.labelTertiary : (options.tone == Tone::Neutral ? c.label : internal::DisplayToneLook(options.tone, false).text));
    bool valueCut = false;
    if (options.wrap)
    {
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), valueRect.Min, valueColor, value, nullptr, valueWidth);
    }
    else
    {
        const char* lineEnd = std::strchr(value, '\n');
        const internal::FittedText shown = internal::FitText(value, lineEnd, valueWidth);
        valueCut = shown.truncated || lineEnd != nullptr;
        drawList->AddText(valueRect.Min, valueColor, shown.text);
    }
    if (frame.copyable)
    {
        internal::DrawFocusRing(drawList, part.rect, Px(t.radii.element), part.focusAmount);
        internal::DisplayCopiedFeedback(part.id, part.rect);
        internal::CutTextTooltip(part, value, nullptr, valueCut);
    }
    else if (valueCut)
    {
        const bool valueHovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(valueRect.Min, valueRect.Max);
        internal::CutTextTooltip(ImHashStr("##value", 0, id), valueRect, valueHovered, false, value, nullptr, true);
    }
    return copied;
}

} // namespace facet
