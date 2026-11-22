// FieldValidation and ValidationSummary. FieldValidation draws a message line under the last item
// of the window for items that have no `error` option, and reports Destructive messages to the
// error store of the control anatomy (internal/control.h). ValidationSummary reads that store: the
// errors the fields of the current window reported in the previous frame, as a list of links.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/text_field.h"

namespace facet
{
namespace
{

// Whether Dear ImGui's last item is an item of the current window's content. The id cannot
// tell (groups, stock composite widgets and text items have id 0 while idle, and Begin leaves the
// title bar as the last item); the rect can: an item has height and starts at or below the
// content start, the title bar lies above it and a child window's empty title bar has no height.
bool FieldValidationLastItemInContent(const ImGuiContext& g)
{
    const ImRect& rect = g.LastItemData.Rect;
    return rect.Max.y > rect.Min.y && rect.Min.y >= g.CurrentWindow->DC.CursorStartPos.y;
}

struct FieldValidationLook
{
    ImVec4 color;
    Icon icon;
};

FieldValidationLook FieldValidationLookOf(Tone tone, bool disabled)
{
    const Colors& c = T().colors;
    switch (tone)
    {
    case Tone::Destructive:
        return {c.destructive, Icon("error")};
    case Tone::Warning:
        return {c.warning, Icon("warning")};
    case Tone::Success:
        return {c.success, Icon("success")};
    case Tone::Accent:
        return {c.accentText, Icon("info")};
    case Tone::Neutral:
        break;
    }
    return {internal::ControlSecondaryColor(disabled), Icon()};
}

// Draws the message text with its icon at `at` (top-left), wrapped at `width` pixels; returns its
// height. Measures only when `drawList` is null.
float FieldValidationText(ImDrawList* drawList, const char* message, const FieldValidationLook& look, ImVec2 at,
                          float width)
{
    internal::ControlTextScope text(TextStyle::Footnote);
    const bool hasIcon = look.icon.IsValid();
    const float iconSize = hasIcon ? text.IconSize() : 0.0f;
    const float gap = hasIcon ? Px(T().spacing.xs) : 0.0f;
    const float wrap = std::max(1.0f, width - iconSize - gap);
    const ImVec2 size = ImGui::CalcTextSize(message, nullptr, false, wrap);
    if (drawList != nullptr)
    {
        const ImU32 color = ToU32(look.color);
        if (hasIcon)
            DrawIcon(drawList, look.icon, at, iconSize, color);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(at.x + iconSize + gap, at.y), color, message,
                          nullptr, wrap);
    }
    return std::max(size.y, iconSize);
}

// Whether `record` was reported in `window` or in a window under the same root window.
bool ValidationSummaryOwns(const internal::FieldErrorRecord& record, const ImGuiWindow* window)
{
    const ImGuiID root = window->RootWindow != nullptr ? window->RootWindow->ID : window->ID;
    return record.rootWindowId == root || record.windowId == window->ID;
}

// Scrolls the field of `record` into view and gives it the keyboard focus (a text field also
// takes the keyboard, as after Tab).
void ValidationSummaryGoTo(const internal::FieldErrorRecord& record)
{
    ImGuiWindow* target = ImGui::FindWindowByID(record.windowId);
    if (target == nullptr)
        return;
    ImGui::ScrollToRect(target, record.rect);
    if (record.itemId == 0)
        return;
    ImGui::FocusWindow(target);
    ImGui::SetFocusID(record.itemId, target);
    internal::TextFieldRequestFocus(record.itemId, internal::TextFieldFocus::SelectAll);
}

} // namespace

void FieldValidation(const char* message, const FieldValidationOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr || message == nullptr || message[0] == '\0')
        return;
    ImGuiContext& g = *GImGui;
    const ImGuiLastItemData last = g.LastItemData;
    const bool underItem = FieldValidationLastItemInContent(g);
    const bool disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    const float right = window->DC.CursorPos.x + ImGui::GetContentRegionAvail().x;
    const float x = underItem ? last.Rect.Min.x : window->DC.CursorPos.x;
    const float top = underItem ? last.Rect.Max.y + Px(T().spacing.xs) : window->DC.CursorPos.y;
    const float width = std::max(1.0f, right - x);
    const FieldValidationLook look = FieldValidationLookOf(options.tone, disabled);
    const float height = FieldValidationText(nullptr, message, look, ImVec2(x, top), width);
    const ImRect box(ImVec2(x, top), ImVec2(x + width, top + height));

    if (ImGui::IsRectVisible(box.Min, box.Max))
        FieldValidationText(window->DrawList, message, look, box.Min, width);
    if (options.outline && underItem && options.tone != Tone::Neutral &&
        ImGui::IsRectVisible(last.Rect.Min, last.Rect.Max))
    {
        // The one outline of the border rule in the tone's color, inside the item's rect, so it
        // never covers a neighbour and never changes the field's outline width.
        internal::DrawOutline(window->DrawList, last.Rect, Px(T().radii.control), look.color);
    }
    if (options.tone == Tone::Destructive)
        internal::ReportFieldError(underItem ? last.ID : 0, underItem ? last.Rect : box, message);

    // Layout only: the validated item stays the last item, so the item queries still describe it.
    ImGui::SetCursorScreenPos(box.Min);
    ImGui::ItemSize(box);
    g.LastItemData = last;
}

bool ValidationSummary(const char* label, const ValidationSummaryOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    // A forced capture state asked for the summary is taken even when it shows nothing, so it
    // never lands on the next widget.
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const std::span<const internal::FieldErrorRecord> records = internal::PreviousFrameFieldErrors();
    int count = 0;
    for (const internal::FieldErrorRecord& record : records)
        count += ValidationSummaryOwns(record, window) ? 1 : 0;
    if (count == 0)
        return false;

    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const Theme& theme = T();
    const float pad = Px(theme.spacing.sm);
    const float gap = Px(theme.spacing.xs);
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const char* headingEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasHeading = headingEnd != label;

    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const Colors& c = theme.colors;

    const float headingHeight = hasHeading ? MeasureText(TextStyle::Headline, "Ag").y + gap : 0.0f;
    const float lineHeight = MeasureText(TextStyle::Body, "Ag").y;
    const float rowHeight = std::max(internal::GetControlMetrics(ControlSize::Small).height, lineHeight + 2.0f * gap);
    const float height = pad * 2.0f + headingHeight + rowHeight * static_cast<float>(count);
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect block(origin, ImVec2(origin.x + width, origin.y + height));
    ImDrawList* drawList = window->DrawList;
    const bool visible = ImGui::IsRectVisible(block.Min, block.Max);

    ImGui::BeginGroup();
    ImGui::ItemSize(block.GetSize());
    if (visible)
        FillRect(drawList, block.Min, block.Max, ToU32(c.destructiveSubtle), Px(theme.radii.control));
    if (hasHeading && visible)
    {
        internal::ControlTextScope heading(TextStyle::Headline);
        const internal::FittedText shown = internal::FitText(label, headingEnd, width - pad * 2.0f);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(block.Min.x + pad, block.Min.y + pad),
                          ToU32(internal::ControlLabelColor(disabled)), shown.text);
    }

    bool activated = false;
    internal::ControlTextScope body(TextStyle::Body);
    const float iconSize = body.IconSize();
    // The rows reach into the padding by half, so the hover fill and the focus ring have room.
    const float rowLeft = block.Min.x + pad * 0.5f;
    const float rowRight = block.Max.x - pad * 0.5f;
    const float textLeft = block.Min.x + pad + iconSize + gap;
    const float textWidth = std::max(1.0f, block.Max.x - pad - textLeft);
    float rowTop = block.Min.y + pad + headingHeight;
    int index = 0;
    for (const internal::FieldErrorRecord& record : records)
    {
        if (!ValidationSummaryOwns(record, window))
            continue;
        const ImRect row(ImVec2(rowLeft, rowTop), ImVec2(rowRight, rowTop + rowHeight));
        const ImGuiID entryId = ImGui::GetIDWithSeed(index, id);
        const internal::ItemState item = internal::PartBehavior(entryId, row, {.part = index, .forced = &forced});
        if (item.visible)
        {
            if (!item.disabled)
            {
                const ImVec4 rest = WithAlpha(c.fill[2], 0.0f);
                FillRect(drawList, row.Min, row.Max, ToU32(internal::StateFill(rest, c.fill[2], c.fill[1], item)),
                         Px(theme.radii.element));
            }
            const float centerY = (row.Min.y + row.Max.y) * 0.5f;
            internal::DrawControlIcon(drawList, Icon("error"), block.Min.x + pad, centerY, iconSize,
                                      ToU32(item.disabled ? internal::ControlSecondaryColor(true) : c.destructive));
            const internal::FittedText shown = internal::FitText(record.text, nullptr, textWidth);
            const float textTop = CenteredTextY(row.Min.y, rowHeight);
            const ImU32 textColor = ToU32(internal::ControlLabelColor(item.disabled));
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textLeft, textTop), textColor, shown.text);
            // A link: always underlined, so it does not depend on color.
            const float underline = std::floor(textTop + ImGui::GetFontSize()) + HairlineThickness() * 0.5f;
            drawList->AddLine(ImVec2(textLeft, underline), ImVec2(textLeft + shown.width, underline), textColor,
                              HairlineThickness());
            internal::DrawItemFocusRing(item, Px(theme.radii.element));
            internal::CutTextTooltip(item, record.text, nullptr, shown.truncated);
        }
        if (item.pressed)
        {
            ValidationSummaryGoTo(record);
            activated = true;
        }
        rowTop += rowHeight;
        ++index;
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return activated;
}

} // namespace facet
