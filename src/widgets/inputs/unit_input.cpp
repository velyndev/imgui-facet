// UnitInput: a number in base units shown in a unit the user picks, on the number core
// (src/internal/text_field_number.h). The unit sits at the end of the frame as a menu button in
// the core's trailing slot; its menu is a popup level with menu rows (popup_menu.h). Typed text
// may carry a unit suffix ("250ms", "1.5 s"): the parser converts it into the shown unit and the
// commit switches the shown unit to it, so what the field shows always reads back.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;
using internal::NumberParse;
using internal::ParseNumber;

// What the parser and the unit slot share during one call.
struct UnitInputContext
{
    std::span<const UnitItem> units;
    int shown = 0;          // the unit the field shows
    int typed = -1;         // the unit of the last parsed suffix, -1 for a plain number
    bool menu = false;      // the slot holds the unit menu button
    ImGuiID menuId = 0;
    bool menuPressed = false;
    ImRect button;
    ForcedItemState forced = ForcedItemState::None;
};

double UnitFactor(const UnitItem& unit)
{
    return unit.factor > 0.0 && std::isfinite(unit.factor) ? unit.factor : 1.0;
}

bool UnitLabelEquals(const char* label, const char* begin, const char* end, bool ignoreCase)
{
    if (label == nullptr)
        return false;
    const std::size_t length = static_cast<std::size_t>(end - begin);
    if (std::strlen(label) != length)
        return false;
    for (std::size_t i = 0; i < length; ++i)
    {
        char a = label[i];
        char b = begin[i];
        if (ignoreCase)
        {
            a = static_cast<char>(ImToUpper(a));
            b = static_cast<char>(ImToUpper(b));
        }
        if (a != b)
            return false;
    }
    return true;
}

// The end of the number at the start of [begin, end): a sign, digits and separators, and an
// exponent only where digits follow the "e", so a suffix such as "em" is not read as one.
const char* UnitNumberEnd(const char* begin, const char* end)
{
    const char* p = begin;
    if (p < end && (*p == '+' || *p == '-'))
        ++p;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ','))
        ++p;
    if (p < end && (*p == 'e' || *p == 'E'))
    {
        const char* q = p + 1;
        if (q < end && (*q == '+' || *q == '-'))
            ++q;
        if (q < end && *q >= '0' && *q <= '9')
        {
            while (q < end && *q >= '0' && *q <= '9')
                ++q;
            p = q;
        }
    }
    return p;
}

// Reads "12", "12 ms", "250ms" or "1,5s": a plain number in the shown unit, or a number with a
// unit label (exact, then ignoring ASCII case) converted into the shown unit.
NumberParse ParseUnitText(void* user, const char* text, double* out)
{
    UnitInputContext& context = *static_cast<UnitInputContext*>(user);
    context.typed = -1;
    const char* begin = text;
    const char* end = text + std::strlen(text);
    while (begin < end && (*begin == ' ' || *begin == '\t'))
        ++begin;
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t'))
        --end;
    if (begin == end)
        return NumberParse::Empty;
    const char* numberEnd = UnitNumberEnd(begin, end);
    const char* suffix = numberEnd;
    while (suffix < end && (*suffix == ' ' || *suffix == '\t'))
        ++suffix;
    char number[64];
    const std::size_t numberLength = static_cast<std::size_t>(numberEnd - begin);
    if (numberLength == 0 || numberLength >= sizeof(number))
        return NumberParse::Invalid;
    std::memcpy(number, begin, numberLength);
    number[numberLength] = 0;
    double parsed = 0.0;
    const NumberParse result = ParseNumber(number, &parsed);
    if (result != NumberParse::Ok)
        return result;
    if (suffix == end)
    {
        *out = parsed;
        return NumberParse::Ok;
    }
    int found = -1;
    for (int pass = 0; pass < 2 && found < 0; ++pass)
        for (int i = 0; i < static_cast<int>(context.units.size()); ++i)
            if (UnitLabelEquals(context.units[i].label, suffix, end, pass == 1))
            {
                found = i;
                break;
            }
    if (found < 0)
        return NumberParse::Invalid;
    const double base = parsed * UnitFactor(context.units[found]);
    const double shown = base / UnitFactor(context.units[context.shown]);
    if (!std::isfinite(shown))
        return NumberParse::Invalid;
    *out = shown;
    context.typed = found;
    return NumberParse::Ok;
}

// Width in pixels of the unit menu button: the widest label, the chevron and the paddings.
float UnitButtonWidth(std::span<const UnitItem> units, ControlSize size)
{
    const internal::ControlMetrics m = internal::GetControlMetrics(size);
    internal::ControlTextScope text(m);
    float widest = 0.0f;
    for (const UnitItem& unit : units)
        if (unit.label != nullptr)
            widest = std::max(widest, ImGui::CalcTextSize(unit.label).x);
    const float pad = std::round(Px(T().spacing.xs));
    return std::ceil(pad * 2.0f + widest + m.iconGap * 0.5f + text.IconSize());
}

// The trailing slot: the unit label and a chevron as one button that opens the menu.
void UnitMenuSlot(const internal::TextFieldSlot& slot, void* user)
{
    UnitInputContext& context = *static_cast<UnitInputContext*>(user);
    const Theme& t = T();
    const Colors& c = t.colors;
    const internal::ControlMetrics m = internal::GetControlMetrics(slot.size);
    const float gap = std::round(Px(t.spacing.xs));
    const float side = std::max(1.0f, m.height - gap * 2.0f);
    const float top = std::round(slot.rect.GetCenter().y - side * 0.5f);
    const ImRect rect(slot.rect.Min.x, top, slot.rect.Max.x, top + side);
    internal::ItemOptions options;
    options.disabled = slot.readOnly;
    options.forced = slot.forced;
    options.part = 1;
    const internal::ItemState item = internal::PartBehavior(context.menuId, rect, options);
    context.menuPressed = item.pressed;
    context.button = rect;
    context.forced = item.forced;
    if (!item.visible)
        return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = Px(t.radii.element);
    if (!item.disabled)
    {
        ImVec4 fill = WithAlpha(c.fill[1], 0.0f);
        fill = Mix(fill, c.fill[1], item.hoverAmount);
        fill = Mix(fill, c.fill[0], item.pressAmount);
        if (internal::IsPopupLevelOpen(context.menuId) || Has(item.forced, ForcedItemState::Open))
            fill = c.fill[1];
        FillRect(drawList, rect.Min, rect.Max, ToU32(fill), radius);
    }
    const bool strong = !item.disabled && (item.hovered || item.held);
    const ImU32 color = ToU32(item.disabled ? c.labelTertiary : (strong ? c.label : c.labelSecondary));
    internal::ControlTextScope text(m);
    const char* label = context.units[context.shown].label != nullptr ? context.units[context.shown].label : "";
    const float iconSize = text.IconSize();
    const float pad = gap;
    const float textRoom = std::max(0.0f, rect.GetWidth() - pad * 2.0f - iconSize - m.iconGap * 0.5f);
    const internal::FittedText shown = internal::FitText(label, nullptr, textRoom);
    drawList->AddText(ImVec2(rect.Min.x + pad, CenteredTextY(rect.Min.y, rect.GetHeight())), color, shown.text);
    internal::DrawControlIcon(drawList, Icon("chevron-down"), rect.Max.x - pad - iconSize, rect.GetCenter().y, iconSize,
                              color);
    internal::DrawItemFocusRing(item, radius);
}

} // namespace

bool UnitInput(const char* label, double* value, int* unit, std::span<const UnitItem> units,
               const UnitInputOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ImGuiID id = window->GetID(label);
    const int unitCount = static_cast<int>(units.size());
    const bool readOnly = options.readOnly || value == nullptr;

    UnitInputContext context;
    context.units = units;
    context.shown = unit != nullptr && unitCount > 0 ? std::clamp(*unit, 0, unitCount - 1) : 0;
    context.menu = unit != nullptr && unitCount > 0;
    context.menuId = ImGui::GetIDWithSeed("units", nullptr, id);
    const double factor = unitCount > 0 ? UnitFactor(units[context.shown]) : 1.0;

    internal::NumberFieldParams params;
    params.format = options.format;
    params.formatter = options.formatter;
    params.formatterUser = options.formatterUser;
    params.min = std::min(options.min, options.max) / factor;
    params.max = std::max(options.min, options.max) / factor;
    params.step = options.step;
    params.stepFast = options.stepFast;
    if (unitCount > 0)
    {
        params.parser = ParseUnitText;
        params.parserUser = &context;
    }
    if (context.menu)
    {
        params.trailingWidth = internal::ValueInputUnits(UnitButtonWidth(units, options.size));
        params.trailingSlot = UnitMenuSlot;
        params.trailingUser = &context;
    }
    else if (unitCount > 0)
    {
        params.unit = units[0].label;
    }
    params.size = options.size;
    params.width = options.width;
    params.readOnly = readOnly;
    params.mixed = options.mixed;
    params.error = internal::ValueInputError(id, options.error);
    params.helpText = options.helpText;

    double shown = value != nullptr ? *value / factor : 0.0;
    const internal::NumberFieldResult result =
        internal::NumberFieldBehavior(label, value != nullptr ? &shown : nullptr, params);
    bool written = false;
    if (result.changed && value != nullptr && std::isfinite(shown))
    {
        const double base = shown * factor;
        if (base != *value)
        {
            *value = base;
            written = true;
        }
    }
    // A committed suffix switches the shown unit; the quantity stays.
    if (result.field.edited && !result.typing && !result.parseFailed && context.typed >= 0 && unit != nullptr &&
        !readOnly && context.typed != *unit)
    {
        *unit = context.typed;
        written = true;
    }
    internal::ValueInputTrackRejected(id, result.parseFailed, written);

    // The unit menu.
    if (context.menu)
    {
        const internal::PopupAnchor anchor{.rect = context.button, .side = internal::PopupSide::Below,
                                           .align = internal::PopupAlign::End};
        if (context.menuPressed && !readOnly)
            internal::OpenPopupLevel(context.menuId, anchor);
        if (Has(context.forced, ForcedItemState::Open))
            internal::ForcePopupLevelOpen(context.menuId, anchor, {.takeFocus = false});
        if (internal::BeginPopupLevel(context.menuId))
        {
            internal::MenuLevelBegin(context.menuId);
            for (int i = 0; i < unitCount; ++i)
            {
                ImGui::PushID(i);
                const char* name = units[i].label != nullptr ? units[i].label : "";
                if (internal::MenuRow(context.menuId, name, {.checked = i == context.shown}).pressed && i != *unit)
                {
                    if (!options.convert && value != nullptr)
                    {
                        const double number = *value / UnitFactor(units[context.shown]);
                        const double base = std::clamp(number * UnitFactor(units[i]), std::min(options.min, options.max),
                                                       std::max(options.min, options.max));
                        if (std::isfinite(base))
                            *value = base;
                    }
                    *unit = i;
                    written = true;
                }
                ImGui::PopID();
            }
            internal::EndPopupLevel();
        }
    }
    return written;
}

} // namespace facet
