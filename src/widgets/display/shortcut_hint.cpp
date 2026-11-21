// ShortcutHint (facet/widgets/display.h): a text in labelSecondary at the start of its width and a
// chord as key caps at the end, both on one text baseline. When the width is too small for both,
// the caps go first; then the text is cut with an ellipsis and shows in full on hover. Not
// interactive: one item that is not a Tab stop.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

void ShortcutHintImpl(const char* text, ImGuiKeyChord chord, bool hasChord, const ShortcutHintOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (text == nullptr)
        text = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::DisplayMarkMetrics m = internal::DisplayMarkLook(ControlSize::Regular);
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float gap = Px(T().spacing.md);
    const float capsWidth = hasChord ? internal::DisplayCapsWidth(chord, m) : 0.0f;
    internal::ControlTextScope scope(m.lineStyle);
    const float textWidth = std::ceil(ImGui::CalcTextSize(text).x);
    const bool showCaps = hasChord && textWidth + (textWidth > 0.0f ? gap : 0.0f) + capsWidth <= width;

    internal::ApplyNextItemData(next);
    const ImGuiID id = internal::DisplayAnonymousId(window, text[0] != '\0' ? text : "##ShortcutHint");
    const internal::DisplayInlineBox box =
        internal::DisplayPlaceInline(window, id, ImVec2(width, m.height), internal::DisplayMarkBaseline(m), m.lineStyle);
    if (!box.visible)
        return;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImDrawList* drawList = window->DrawList;
    const float textRoom = showCaps ? width - capsWidth - gap : width;
    const internal::FittedText shown = internal::FitText(text, nullptr, std::max(0.0f, textRoom));
    const float textTop = internal::DisplayTextTop(box.baseline);
    drawList->AddText(ImVec2(box.rect.Min.x, textTop), ToU32(internal::ControlSecondaryColor(disabled)), shown.text);
    if (showCaps)
        internal::DrawDisplayCaps(drawList, ImVec2(box.rect.Max.x - capsWidth, box.rect.Min.y), box.baseline, chord, m, false,
                                  disabled);
    internal::CutTextTooltip(id, box.rect, ImGui::IsItemHovered(), false, text, nullptr, shown.truncated);
}

} // namespace

void ShortcutHint(const char* text, ImGuiKeyChord chord, const ShortcutHintOptions& options)
{
    ShortcutHintImpl(text, chord, true, options);
}

void ShortcutHint(const char* text, ShortcutId shortcut, const ShortcutHintOptions& options)
{
    ShortcutInfo info;
    const bool known = shortcut.IsValid() && GetShortcut(shortcut, &info) && info.primary != ImGuiKey_None;
    ShortcutHintImpl(text, known ? info.primary : ImGuiKey_None, known, options);
}

} // namespace facet
