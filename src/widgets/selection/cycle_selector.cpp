// CycleSelector: a value between previous and next arrow buttons in a field frame, stepped by the
// arrows, Left and Right, the wheel or a click on the value, with optional page dots under it.

#include "facet/widgets/selection.h"

#include <algorithm>
#include <cmath>

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
#include "internal/scroll.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

bool CycleSelectorImpl(const char* label, int* index, const internal::SelectionItems& items,
                       const CycleSelectorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope text(m);
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& strings = GetStrings();
    const ImGuiID id = window->GetID(label);
    const int count = items.Count();
    const auto enabled = [&](int i) { return !items.At(i).disabled; };

    // ---- Layout: label in front, then the frame with an arrow button at each end ----
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect frame(origin.x + front.width, origin.y, origin.x + width, origin.y + m.height);
    const float arrowSide = m.height;
    const ImRect prevRect(frame.Min.x, frame.Min.y, frame.Min.x + arrowSide, frame.Max.y);
    const ImRect nextRect(frame.Max.x - arrowSide, frame.Min.y, frame.Max.x, frame.Max.y);
    const float dot = std::max(1.0f, PxSnapped(t.metrics.pageDotSize));
    const float dotsGap = std::round(Px(t.spacing.xs));
    const float dotsHeight = options.pageDots && count > 0 ? dotsGap + dot : 0.0f;
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);

    ImGui::BeginGroup();
    internal::ItemOptions bodyOptions;
    bodyOptions.baseline = CenteredTextY(origin.y, m.height) - origin.y;
    bodyOptions.readOnly = options.readOnly || index == nullptr;
    bodyOptions.mixed = options.mixed;
    bodyOptions.allowOverlap = true;  // the arrow buttons on top take the pointer
    bodyOptions.part = -1;
    bodyOptions.forced = &forced;
    internal::ApplyNextItemData(next);
    window->DC.CursorPos = origin;
    const internal::ItemState body = internal::ItemBehavior(id, ImVec2(width, m.height + dotsHeight), bodyOptions);
    const bool readOnly = body.readOnly;
    const bool live = body.forced == internal::ForcedItemState::None;

    const int current = index != nullptr ? *index : -1;
    const bool inRange = current >= 0 && current < count;
    const bool mixedShown = internal::SelectionMixedShown(id, body.mixed, false);
    const auto stepFrom = [&](int step) -> int {
        if (count == 0)
            return -1;
        if (!inRange)
            return internal::SelectionStepEnabled(step > 0 ? -1 : count, step, count, false, enabled);
        const int target = internal::SelectionStepEnabled(current, step, count, options.wrap, enabled);
        return target == current ? -1 : target;
    };
    const bool canPrev = !body.disabled && !readOnly && stepFrom(-1) >= 0;
    const bool canNext = !body.disabled && !readOnly && stepFrom(1) >= 0;

    // The arrow buttons: mouse targets on the frame; the keyboard steps with Left and Right.
    internal::ItemOptions arrowOptions;
    arrowOptions.navigable = false;
    arrowOptions.hitTarget = false;
    arrowOptions.repeat = true;
    internal::ForcedItemRequest none;
    arrowOptions.forced = &none;
    arrowOptions.disabled = !canPrev;
    const internal::ItemState prev = internal::PartBehavior(ImGui::GetIDWithSeed("previous", nullptr, id), prevRect, arrowOptions);
    arrowOptions.disabled = !canNext;
    const internal::ItemState nextArrow = internal::PartBehavior(ImGui::GetIDWithSeed("next", nullptr, id), nextRect, arrowOptions);

    int step = 0;
    ImGuiID editor = id;
    if (live && !readOnly && !body.disabled)
    {
        if (prev.pressed || nextArrow.pressed)
        {
            step = prev.pressed ? -1 : 1;
            editor = prev.pressed ? prev.id : nextArrow.id;
            // The arrows take no focus of their own: the selector gets it, so the keys continue there.
            if (g.NavId != id)
                ImGui::SetFocusID(id, window);
        }
        else if (body.pressed && !body.keyboardPressed)
            step = 1;  // a click on the value steps forward, like the next button
        if (body.focused)
        {
            ImGui::SetKeyOwner(ImGuiKey_LeftArrow, id);
            ImGui::SetKeyOwner(ImGuiKey_RightArrow, id);
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, id))
                step = 1;
            else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, id))
                step = -1;
            else if (body.keyboardPressed)
                step = 1;  // Space and Enter step forward too
        }
        // The wheel steps while the pointer rests on the selector, and does not scroll the window
        // then: the selector claims the wheel (key ownership) while the pointer is over it, and
        // reads it once it owned it from the previous frame, so the scroll core (scroll.h) leaves
        // it alone. A scroll gesture that is still running keeps the wheel.
        const bool pointerOver = g.HoveredWindow == window && ImGui::IsMouseHoveringRect(frame.Min, frame.Max);
        if (pointerOver && !internal::ScrollWheelBelongsToScroll())
        {
            const bool owned = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, id);
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, id);
            if (owned && g.IO.MouseWheel != 0.0f)
                step = g.IO.MouseWheel > 0.0f ? -1 : 1;
        }
    }
    bool written = false;
    if (step != 0 && index != nullptr)
    {
        const int target = stepFrom(step);
        if (target >= 0 && (target != current || mixedShown))
        {
            *index = target;
            written = true;
        }
    }
    internal::SelectionMixedShown(id, body.mixed, written);

    if (body.visible)
    {
        ImDrawList* drawList = window->DrawList;
        internal::DrawFieldFrame(drawList, frame,
                                 {.hoverAmount = body.hoverAmount, .active = false, .disabled = body.disabled,
                                  .readOnly = readOnly, .error = message.error});
        internal::DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size, body.disabled);

        // Arrows: the chevrons in labelSecondary, labelTertiary where they cannot step.
        const float iconSize = text.IconSize();
        const auto drawArrow = [&](const internal::ItemState& arrow, const ImRect& rect, Icon icon, bool can) {
            const float feedback = can ? std::max(arrow.hoverAmount, arrow.pressAmount) : 0.0f;
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], arrow.pressAmount);
                // The same inset as a segment's highlight inside its track.
                const float inset = std::max(1.0f, std::round(Px(t.metrics.segmentInset)));
                const ImRect inner(rect.Min.x + inset, rect.Min.y + inset, rect.Max.x - inset, rect.Max.y - inset);
                FillRect(drawList, inner.Min, inner.Max, ToU32(WithAlpha(tint, tint.w * feedback)),
                         std::max(0.0f, m.radius - inset));
            }
            internal::DrawControlIcon(drawList, icon, std::round(rect.GetCenter().x - iconSize * 0.5f), rect.GetCenter().y,
                                      iconSize, ToU32(can ? c.labelSecondary : c.labelTertiary));
        };
        drawArrow(prev, prevRect, Icon("chevron-left"), canPrev);
        drawArrow(nextArrow, nextRect, Icon("chevron-right"), canNext);

        // The value, centered between the arrows; the placeholder when nothing valid is chosen.
        const int shownIndex = index != nullptr ? *index : -1;
        const bool valid = shownIndex >= 0 && shownIndex < count;
        const char* value = mixedShown ? strings.mixed : (valid ? items.At(shownIndex).label : strings.select);
        const char* valueEnd = mixedShown || !valid ? nullptr : ImGui::FindRenderedTextEnd(value);
        const ImU32 valueColor = ToU32(mixedShown || !valid ? internal::ControlSecondaryColor(body.disabled)
                                                            : internal::ControlLabelColor(body.disabled));
        const float room = std::max(0.0f, nextRect.Min.x - prevRect.Max.x);
        const internal::FittedText shown = internal::FitText(value, valueEnd, room);
        drawList->AddText(ImVec2(std::round(frame.GetCenter().x - shown.width * 0.5f), CenteredTextY(frame.Min.y, m.height)),
                          valueColor, shown.text);

        // Page dots: one per item, the chosen one in label, the others in labelTertiary.
        if (options.pageDots && count > 0)
        {
            const float spacing = dot * 2.0f;
            const float total = static_cast<float>(count) * dot + static_cast<float>(count - 1) * (spacing - dot);
            if (total <= frame.GetWidth())
            {
                float x = std::round(frame.GetCenter().x - total * 0.5f);
                const float y = std::round(frame.Max.y + dotsGap);
                for (int i = 0; i < count; ++i)
                {
                    const ImVec4 color = i == shownIndex && !mixedShown ? internal::ControlLabelColor(body.disabled) : c.labelTertiary;
                    drawList->AddCircleFilled(ImVec2(x + dot * 0.5f, y + dot * 0.5f), dot * 0.5f, ToU32(color));
                    x += spacing;
                }
            }
        }
        internal::DrawFocusRing(drawList, frame, m.radius, body.focusAmount);
        if (shown.truncated)
            internal::CutTextTooltip(id, frame, body.hovered, body.focusVisible, value, valueEnd, true);
    }
    if (front.shown.truncated)
    {
        const ImRect labelRect(origin.x, origin.y, frame.Min.x, frame.Max.y);
        internal::CutTextTooltip(ImGui::GetIDWithSeed("label tooltip", nullptr, id), labelRect,
                                 ImGui::IsMouseHoveringRect(labelRect.Min, labelRect.Max) && g.HoveredWindow == window,
                                 body.focusVisible, label, front.end, true);
    }

    const float bottom = internal::DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y + dotsHeight), frame.GetWidth(),
                                                      body.disabled, id, frame);
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(frame.Max.x, bottom));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (written)
        internal::MarkWidgetEdited(editor);
    return written;
}

} // namespace

bool CycleSelector(const char* label, int* index, std::span<const ChoiceItem> items, const CycleSelectorOptions& options)
{
    return CycleSelectorImpl(label, index, {items, {}}, options);
}

bool CycleSelector(const char* label, int* index, std::span<const char* const> items, const CycleSelectorOptions& options)
{
    return CycleSelectorImpl(label, index, {{}, items}, options);
}

} // namespace facet
