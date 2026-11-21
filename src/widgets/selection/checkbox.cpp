// Checkbox: a box and its label, two-state and tri-state, with drag painting across boxes, and
// the checkbox item that CheckboxGroup builds on (selection_parts.h).

#include "facet/widgets/selection.h"

#include <algorithm>
#include <cmath>

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
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace internal
{
namespace
{

// A press on a checkbox that may paint its new value onto the boxes the pointer crosses while
// the button stays down. One at a time: there is one mouse.
struct CheckboxPaint
{
    ImGuiID origin = 0;       // the box the press started on; 0 when none
    ImGuiID window = 0;       // painting stays inside the window of the first box
    SelectionMark value = SelectionMark::Off;  // what the first box becomes
    bool painting = false;    // the pointer reached another box
    int seenFrame = -1;       // the last frame the first box ran
};

CheckboxPaint& Paint()
{
    static CheckboxPaint paint;
    return paint;
}

SelectionMark Flipped(SelectionMark mark)
{
    return mark == SelectionMark::On ? SelectionMark::Off : SelectionMark::On;
}

} // namespace

bool SelectionCheckbox(const char* label, ImGuiID id, SelectionMark* mark, const NextItemData& next,
                       const SelectionCheckboxDesc& desc)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    const ControlMetrics m = GetControlMetrics(desc.size);
    ControlTextScope text(m);
    const Theme& t = T();

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    const float box = std::max(1.0f, PxSnapped(t.metrics.checkboxSize));
    const float gap = hasLabel ? std::round(Px(t.spacing.sm)) : 0.0f;
    const float labelWidth = hasLabel ? ImGui::CalcTextSize(label, labelEnd).x : 0.0f;
    const float width = ResolveItemWidth(0.0f, next, std::ceil(box + gap + labelWidth));
    const ControlMessage message = PickControlMessage(desc.error, desc.helpText);
    const bool grouped = message.IsSet();

    const float top = window->DC.CursorPos.y;
    const bool readOnly = desc.readOnly || mark == nullptr;
    ItemOptions itemOptions;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    itemOptions.readOnly = readOnly;
    itemOptions.disabled = desc.disabled;
    itemOptions.handCursor = !readOnly && (next.itemFlags & ImGuiItemFlags_ReadOnly) == 0;
    if (grouped)
        ImGui::BeginGroup();
    ApplyNextItemData(next);
    const ItemState item = ItemBehavior(id, ImVec2(width, m.height), itemOptions);

    // Input, with painting: the first box flips on the release as usual unless the pointer
    // painted other boxes on the way, which then all take its new value.
    const SelectionMark current = mark != nullptr ? *mark : SelectionMark::Off;
    SelectionMark result = current;
    bool edited = false;
    const bool interactive = !item.readOnly && !item.disabled && item.forced == ForcedItemState::None;
    CheckboxPaint& paint = Paint();
    const int frame = ImGui::GetFrameCount();
    if (paint.origin != 0 && paint.origin != id && frame - paint.seenFrame > 1)
        paint = {};  // the first box is gone
    bool pressHandled = false;
    bool painted = false;  // written by a drag that started on another box
    if (interactive)
    {
        if (item.activated && g.ActiveIdSource == ImGuiInputSource_Mouse)
            paint = {id, window->ID, Flipped(current), false, frame};
        if (paint.origin == id)
        {
            paint.seenFrame = frame;
            if (paint.painting && current != paint.value)
            {
                result = paint.value;
                edited = true;
            }
            if (!item.active)
            {
                pressHandled = paint.painting;
                paint = {};
            }
        }
        else if (paint.origin != 0 && paint.window == window->ID && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                 item.rect.Contains(g.IO.MousePos))
        {
            paint.painting = true;
            if (current != paint.value)
            {
                result = paint.value;
                edited = true;
                painted = true;
            }
        }
        if (item.pressed && !pressHandled && !edited)
        {
            result = Flipped(current);
            edited = true;
        }
    }
    const bool written = edited && mark != nullptr && result != current;
    if (written)
    {
        *mark = result;
        if (painted)
        {
            // The first box holds the mouse, so this box is not Dear ImGui's active item and
            // ImGui::MarkItemEdited would assert: its edited status is set as that call sets it.
            ImGuiLastItemData& last = g.LastItemData;
            last.StatusFlags |= ImGuiItemStatusFlags_EditedInternal;
            if ((last.ItemFlags & ImGuiItemFlags_NoMarkEdited) == 0)
                last.StatusFlags |= ImGuiItemStatusFlags_Edited;
        }
        else
        {
            ImGui::MarkItemEdited(id);
        }
    }

    const float onAmount = SelectionMarkAmount(id, result != SelectionMark::Off);
    bool truncated = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const ImRect boxRect = SelectionMarkRect(item.rect.Min.x, item.rect.GetCenter().y);
        // Fading out, the box keeps the check it had.
        const SelectionMark glyph = result == SelectionMark::Mixed ? SelectionMark::Mixed : SelectionMark::On;
        DrawSelectionCheckbox(drawList, boxRect, glyph, onAmount, item);
        if (hasLabel)
        {
            const float x = boxRect.Max.x + gap;
            const FittedText shown = FitText(label, labelEnd, std::max(0.0f, item.rect.Max.x - x));
            truncated = shown.truncated;
            drawList->AddText(ImVec2(x, CenteredTextY(item.rect.Min.y, m.height)), ToU32(ControlLabelColor(item.disabled)),
                              shown.text);
        }
        DrawFocusRing(drawList, boxRect, SelectionCheckboxRadius(boxRect), item.focusAmount);
    }

    CutTextTooltip(item, label, labelEnd, truncated);
    if (grouped)
    {
        const float bottom =
            DrawControlMessage(message, ImVec2(item.rect.Min.x, item.rect.Max.y), item.rect.GetWidth(), item.disabled, id, item.rect);
        window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(item.rect.Max.x, bottom));
        ImGui::EndGroup();
        FinishWidgetGroup(id);
        if (written)
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
    }
    return written;
}

} // namespace internal

bool Checkbox(const char* label, bool* value, const CheckboxOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::SelectionMark mark = value != nullptr && *value ? internal::SelectionMark::On : internal::SelectionMark::Off;
    const internal::SelectionCheckboxDesc desc{options.size, options.readOnly, false, options.error, options.helpText};
    const bool written =
        internal::SelectionCheckbox(label, window->GetID(label), value != nullptr ? &mark : nullptr, next, desc);
    if (written)
        *value = mark == internal::SelectionMark::On;
    return written;
}

bool Checkbox(const char* label, CheckState* state, const CheckboxOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::SelectionMark mark = internal::SelectionMark::Off;
    if (state != nullptr)
        mark = *state == CheckState::On      ? internal::SelectionMark::On
               : *state == CheckState::Mixed ? internal::SelectionMark::Mixed
                                             : internal::SelectionMark::Off;
    const internal::SelectionCheckboxDesc desc{options.size, options.readOnly, false, options.error, options.helpText};
    const bool written =
        internal::SelectionCheckbox(label, window->GetID(label), state != nullptr ? &mark : nullptr, next, desc);
    if (written)
        *state = mark == internal::SelectionMark::On ? CheckState::On : CheckState::Off;
    return written;
}

} // namespace facet
