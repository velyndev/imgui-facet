// StatusPill (facet/widgets/display.h): the state of a connection, job or device as a pill with the
// tone's wash, an optional leading icon or pulsing live dot in the tone, and the visible label part
// in label. A tone change crossfades over motion.durationFast under the id. Not interactive.

#include "facet/widgets/display.h"

#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Animation sub keys under the widget id: the crossfading wash and glyph colors.
constexpr int kPillAnimWash = 1;
constexpr int kPillAnimGlyph = 2;
// Exponential smoothing speed per duration: e^-3 leaves 5 % of a change after one duration.
constexpr float kPillSettle = 3.0f;

} // namespace

void StatusPill(const char* label, const StatusPillOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const internal::DisplayMarkMetrics m = internal::DisplayMarkLook(options.size);
    const float baselineInBox = internal::DisplayMarkBaseline(m);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    const bool hasIcon = options.icon.IsValid() && !options.liveDot;

    internal::ControlTextScope scope(m.textStyle);
    const float markSize = options.liveDot ? PxSnapped(t.metrics.statusDotSize) : scope.IconSize();
    const bool hasMark = options.liveDot || hasIcon;
    const float padding = Px(t.spacing.sm);
    const float gap = Px(t.spacing.xs);
    const float textWidth = hasLabel ? std::ceil(ImGui::CalcTextSize(label, labelEnd).x) : 0.0f;
    float width = padding * 2.0f + textWidth;
    if (hasMark)
        width += markSize + (hasLabel ? gap : 0.0f);
    if (width < m.height)
        width = m.height;

    const ImGuiID id = window->GetID(label[0] != '\0' ? label : "##StatusPill");
    const internal::DisplayInlineBox box =
        internal::DisplayPlaceInline(window, id, ImVec2(width, m.height), baselineInBox, m.lineStyle);

    // The colors are smoothed every frame, also while clipped, so a pill scrolled back into view
    // shows its current tone.
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::DisplayToneColors tone = internal::DisplayToneLook(options.tone, disabled);
    const float speed = kPillSettle / std::max(t.motion.durationFast, 1e-3f);
    const ImVec4 wash = SmoothColor(id, kPillAnimWash, tone.wash, speed);
    const ImVec4 glyph = SmoothColor(id, kPillAnimGlyph, tone.glyph, speed);
    if (!box.visible)
        return;

    ImDrawList* drawList = window->DrawList;
    FillRoundRect(drawList, box.rect.Min, box.rect.Max, ToU32(wash), box.rect.GetHeight() * 0.5f);
    const float textTop = internal::DisplayTextTop(box.baseline);
    const float capsCenter = internal::DisplayCapsCenter(textTop);
    float x = box.rect.Min.x + padding;
    if (options.liveDot)
        internal::DrawDisplayDot(drawList, ImVec2(x + markSize * 0.5f, std::round(capsCenter)), markSize, ToU32(glyph),
                                 !disabled);
    else if (hasIcon)
        internal::DrawControlIcon(drawList, options.icon, x, capsCenter, markSize, ToU32(glyph));
    if (hasMark)
        x += markSize + (hasLabel ? gap : 0.0f);
    if (hasLabel)
        drawList->AddText(ImVec2(x, textTop), ToU32(internal::ControlLabelColor(disabled)), label, labelEnd);
}

} // namespace facet
