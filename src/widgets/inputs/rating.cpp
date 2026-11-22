// Rating: a row of icons filled up to the value. One item for the row (item behaviour); the
// pointer previews the value under it and a click writes it; the keys run through the value drag
// core (Left and Right step, Home and End go to the ends, Page Up and Down a tenth), so a key edit
// is an edit session as on a slider.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <type_traits>

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
#include "internal/value_drag.h"
#include "widgets/inputs/number_value.h"

namespace facet
{
namespace
{

// RatingOptions::count is clamped to the documented range.
constexpr int kRatingMaxIcons = 10;

// The value a pointer at `x` (pixels from the start of the icons) chooses: the icon (or half of
// it) under the pointer, counted from 1 step.
double RatingValueAt(float x, float iconBox, int count, double step)
{
    if (iconBox <= 0.0f)
        return 0.0;
    const double raw = static_cast<double>(x) / static_cast<double>(iconBox);
    const double snapped = std::ceil(raw / step) * step;
    return std::clamp(snapped, step, static_cast<double>(count));
}

template <class Value>
bool RunRating(const char* label, Value* value, const RatingOptions& options)
{
    // 1. Entry.
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const int count = std::clamp(options.count, 1, kRatingMaxIcons);
    const double step = std::is_floating_point_v<Value> && options.halfSteps ? 0.5 : 1.0;
    const ImGuiID id = window->GetID(label);

    // 2. Width: the label and the icons, at most the available width; the icons shrink to fit.
    const internal::FrontLabel natural = internal::MeasureFrontLabel(label, m.size, FLT_MAX);
    const float content = natural.width + m.height * static_cast<float>(count);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = internal::ResolveItemWidth(0.0f, next, std::min(content, available));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width);
    const float iconBox = std::max(1.0f, std::min(m.height, (width - front.width) / static_cast<float>(count)));

    // 3. One item for the row.
    ImGui::BeginGroup();
    const float top = window->DC.CursorPos.y;
    internal::ItemOptions itemOptions;
    itemOptions.readOnly = options.readOnly || value == nullptr;
    itemOptions.mixed = options.mixed;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    internal::ApplyNextItemData(next);
    internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, m.height), itemOptions);
    const bool readOnly = item.readOnly;
    const ImRect icons(item.rect.Min.x + front.width, item.rect.Min.y,
                       item.rect.Min.x + front.width + iconBox * static_cast<float>(count), item.rect.Max.y);

    double current = value != nullptr ? static_cast<double>(*value) : 0.0;
    if (item.mixed)
        current = 0.0;
    bool written = false;
    const bool live = !readOnly && !item.disabled && item.forced == internal::ForcedItemState::None;
    double preview = -1.0;
    if (live && item.hovered && ImGui::IsMouseHoveringRect(icons.Min, icons.Max))
        preview = RatingValueAt(ImGui::GetIO().MousePos.x - icons.Min.x, iconBox, count, step);
    if (Has(item.forced, internal::ForcedItemState::Hovered))
        preview = std::min(static_cast<double>(count), std::floor(current) + 1.0);
    if (live && item.pressed && !item.keyboardPressed && preview >= 0.0)
    {
        double chosen = preview;
        if (options.clearable && chosen == current)
            chosen = 0.0;
        written = internal::NumberValueStore(chosen, value);
        if (written)
            internal::MarkWidgetEdited(id);
    }

    // The keys, through the value drag core without its mouse input.
    internal::ItemState keys = item;
    keys.held = false;
    keys.pressed = false;
    keys.dragging = false;
    keys.hovered = false;
    double keyed = current;
    internal::ValueDragOptions drag;
    drag.mapping.min = 0.0;
    drag.mapping.max = static_cast<double>(count);
    drag.mapping.step = step;
    drag.wheel = false;
    drag.readOnly = readOnly;
    const internal::ValueDragResult edit = internal::ValueDragBehavior(keys, &keyed, icons, drag);
    if (edit.changed && internal::NumberValueStore(keyed, value))
        written = true;
    if (written)
        current = value != nullptr ? static_cast<double>(*value) : 0.0;

    // 4. Draw: empty icons, then the filled part over them (half an icon for a half step).
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const double shown = preview >= 0.0 ? preview : current;
        const Icon empty = options.icon.IsValid() ? options.icon : Icon("star");
        const Icon filled = options.icon.IsValid() ? options.icon : Icon("star-filled");
        const ImU32 emptyColor = ToU32(item.disabled ? c.labelTertiary : c.labelSecondary);
        const ImU32 fillColor = ToU32(item.disabled ? c.labelTertiary : c.focusRing);
        const float glyph = std::round(std::max(1.0f, iconBox - Px(t.spacing.xs) * 2.0f));
        for (int i = 0; i < count; ++i)
        {
            const float boxLeft = icons.Min.x + iconBox * static_cast<float>(i);
            const float x = std::round(boxLeft + (iconBox - glyph) * 0.5f);
            const double fraction = std::clamp(shown - static_cast<double>(i), 0.0, 1.0);
            if (fraction < 1.0)
                internal::DrawControlIcon(drawList, empty, x, icons.GetCenter().y, glyph, emptyColor);
            if (fraction <= 0.0)
                continue;
            if (fraction < 1.0)
                drawList->PushClipRect(ImVec2(x, icons.Min.y),
                                       ImVec2(x + std::round(glyph * static_cast<float>(fraction)), icons.Max.y), true);
            // An icon of the caller's has no filled form: a filled one also gets a backdrop, so the
            // value does not show by color alone.
            if (options.icon.IsValid() && !item.disabled)
                FillRect(drawList, ImVec2(boxLeft, icons.Min.y), ImVec2(boxLeft + iconBox, icons.Max.y), ToU32(c.accentSubtle),
                         Px(t.radii.element));
            internal::DrawControlIcon(drawList, filled, x, icons.GetCenter().y, glyph, fillColor);
            if (fraction < 1.0)
                drawList->PopClipRect();
        }
        if (front.text != nullptr)
        {
            internal::ControlTextScope text(m);
            internal::DrawFrontLabel(drawList, front, ImVec2(item.rect.Min.x, CenteredTextY(item.rect.Min.y, m.height)),
                                     m.size, item.disabled);
        }
        internal::DrawFocusRing(drawList, icons, m.radius, item.focusAmount);
    }

    // 5. The cut label's tooltip, the message line, the result.
    internal::CutTextTooltip(item, front.text, front.end, front.shown.truncated);
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    internal::DrawControlMessage(message, ImVec2(item.rect.Min.x + front.width, item.rect.Max.y),
                                 std::max(1.0f, width - front.width), item.disabled, id, item.rect);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace

bool Rating(const char* label, float* value, const RatingOptions& options)
{
    return RunRating(label, value, options);
}

bool Rating(const char* label, int* value, const RatingOptions& options)
{
    return RunRating(label, value, options);
}

} // namespace facet
