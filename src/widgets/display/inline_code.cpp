// InlineCode (facet/widgets/display.h): code or an identifier in a tinted box on the text baseline
// of the line it sits in. With copyOnActivate it is a Tab stop: a click, Enter or Space copies the
// text and shows Strings::copied above it for motion.feedbackHold.

#include "facet/widgets/display.h"

#include <cmath>
#include <cstring>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/display/display_look.h"

namespace facet
{

bool InlineCode(const char* label, const InlineCodeOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const internal::DisplayMarkMetrics m = internal::DisplayMarkLook(ControlSize::Regular);
    const float baselineInBox = internal::DisplayMarkBaseline(m);
    const char* end = ImGui::FindRenderedTextEnd(label);
    internal::ControlTextScope scope(m.textStyle);
    const float padding = Px(t.spacing.xs);
    const float width = std::ceil(ImGui::CalcTextSize(label, end).x) + padding * 2.0f;
    const ImGuiID id = window->GetID(label[0] != '\0' ? label : "##InlineCode");

    internal::ItemState item;
    internal::DisplayInlineBox box;
    internal::ApplyNextItemData(next);
    if (options.copyOnActivate)
    {
        box = internal::DisplayLayoutInline(window, ImVec2(width, m.height), baselineInBox, m.lineStyle);
        internal::ItemOptions itemOptions;
        itemOptions.baseline = 0.0f;
        item = internal::PartBehavior(id, box.rect, itemOptions);
        box.visible = item.visible;
    }
    else
    {
        box = internal::DisplayPlaceInline(window, id, ImVec2(width, m.height), baselineInBox, m.lineStyle);
        item.id = id;
        item.rect = box.rect;
        item.disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    }

    bool copied = false;
    if (item.pressed)
    {
        // The clipboard needs a terminated string; a label with "##" is copied without its id part.
        if (*end == '\0')
            ImGui::SetClipboardText(label);
        else
            ImGui::SetClipboardText(std::string(label, end).c_str());
        internal::StartDisplayCopiedFeedback(id, box.rect);
        copied = true;
    }
    if (box.visible)
    {
        const Colors& c = t.colors;
        // The tint: the faint control fill, one step stronger on hover and press when it copies.
        const ImVec4 fill = options.copyOnActivate ? internal::StateFill(c.fill[2], c.fill[1], c.fill[0], item)
                                                   : (item.disabled ? c.fill[3] : c.fill[2]);
        ImDrawList* drawList = window->DrawList;
        FillRect(drawList, box.rect.Min, box.rect.Max, ToU32(fill), Px(t.radii.element));
        drawList->AddText(ImVec2(box.rect.Min.x + padding, internal::DisplayTextTop(box.baseline)),
                          ToU32(internal::ControlLabelColor(item.disabled)), label, end);
        if (options.copyOnActivate)
            internal::DrawItemFocusRing(item, Px(t.radii.element));
    }
    if (options.copyOnActivate)
        internal::DisplayCopiedFeedback(id, box.rect);
    return copied;
}

} // namespace facet
