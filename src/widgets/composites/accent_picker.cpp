// AccentPicker: the visible label in front, then a SwatchPalette with one swatch per preset and,
// with customColor, a ColorWell for any other color. The palette and the well are the finished
// pickers; this file adds the label, the meaning of the first preset ("each theme's own accent"),
// the preset names as tooltips, the selection ring around the well while a custom color is the
// accent, and with `apply` one ClearAccent or SetAccent call per written value. A custom color
// goes through SetAccent like a preset, so Theme::ApplyAccent derives its hover, pressed and
// on-accent colors and the focus ring the same way.
//
// Layout: the swatches sit in one row when the width allows; a narrower width wraps them into
// rows (the palette fits as many columns as the width holds), with the well beside the first row.
// Keeps no state of its own: the palette and the well keep theirs under their ids.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/overlays.h"
#include "facet/widgets/pickers.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"

namespace facet
{
namespace
{

// Presets the picker shows at most; it keeps no state, so the swatch colors live on the stack.
constexpr int kAccentPickerMaxSwatches = 32;
// The accent outline around the well while a custom color is the accent sits this many hairlines
// outside it, the gap of the palette's outline around its chosen swatch, so the two read as one mark.
constexpr float kAccentPickerRingGap = 1.0f;

// The accent a theme preset shows when no SetAccent overrides it.
ImVec4 AccentPickerThemeDefault(bool dark)
{
    static const ImVec4 light = Theme::Light().colors.accent;
    static const ImVec4 darkAccent = Theme::Dark().colors.accent;
    return dark ? darkAccent : light;
}

bool AccentPickerSameColor(const ImVec4& a, const ImVec4& b)
{
    return ToU32(WithAlpha(a, 1.0f)) == ToU32(WithAlpha(b, 1.0f));
}

} // namespace

bool AccentPicker(const char* label, bool* themeDefault, ImVec4* accent, const AccentPickerOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const Strings& s = GetStrings();
    internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || accent == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    // ---- The swatches ----
    const std::span<const AccentPreset> presets = options.presets.empty() ? AccentPresets() : options.presets;
    const int count = std::min(static_cast<int>(presets.size()), kAccentPickerMaxSwatches);
    const bool hasThemeDefault = themeDefault != nullptr && count > 0;
    ImVec4 swatches[kAccentPickerMaxSwatches];
    for (int i = 0; i < count; ++i)
        swatches[i] = i == 0 && hasThemeDefault ? AccentPickerThemeDefault(t.dark) : WithAlpha(presets[i].color, 1.0f);
    const ImVec4 current = accent != nullptr ? WithAlpha(*accent, 1.0f) : t.colors.accent;
    const bool defaultChosen = hasThemeDefault && *themeDefault;
    int presetChosen = -1;
    if (defaultChosen)
        presetChosen = 0;
    for (int i = hasThemeDefault ? 1 : 0; i < count && presetChosen < 0; ++i)
    {
        if (AccentPickerSameColor(swatches[i], current))
            presetChosen = i;
    }
    const bool customChosen = options.customColor && presetChosen < 0;

    // ---- Measure ----
    const float side = std::round(ControlHeight(options.size));
    const float swatchGap = std::round(Px(t.spacing.xs));
    const float wellGap = std::round(Px(t.spacing.sm));
    const float wellWidth = options.customColor ? side : 0.0f;
    const float rowWidth = count > 0 ? static_cast<float>(count) * (side + swatchGap) - swatchGap : side;
    const internal::FrontLabelFit fit = internal::CompactFrontLabelFit(0.0f, next);
    const internal::FrontLabel natural = internal::MeasureFrontLabel(label, m.size, FLT_MAX, fit);
    const float content = rowWidth + (options.customColor ? wellGap + wellWidth : 0.0f);
    const float width = std::round(internal::ResolveItemWidth(0.0f, next, natural.width + content));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width, fit);
    const float paletteWidth =
        std::max(side, width - std::round(front.width) - (options.customColor ? wellGap + wellWidth : 0.0f));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float paletteX = origin.x + std::round(front.width);

    // The next-item data (focus requests, item flags) goes to the palette, which stands for the
    // picker; the width was used above.
    next.data.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
    next.hasWidth = false;

    bool written = false;
    bool chooseDefault = false;
    ImVec4 chosenColor = current;

    ImGui::BeginGroup();
    // The parts are named under the picker's id, so two pickers in one window never share them.
    ImGui::PushOverrideID(id);
    // The palette: a forced request for any part but the well goes to it.
    ImGui::SetCursorScreenPos(ImVec2(paletteX, origin.y));
    const int wellPart = count;
    if (forced.state != internal::ForcedItemState::None && forced.part != wellPart)
        internal::ForceNextItemState(forced.state);
    internal::ApplyNextItemData(next);
    ImGui::SetNextItemWidth(paletteWidth);
    // While a custom color is the accent no swatch shows the ring, also when the custom color
    // happens to equal the theme default swatch, so that swatch can still be chosen. A color
    // outside 0 to 1 never equals a swatch.
    const ImVec4 kNoSwatch(-1.0f, -1.0f, -1.0f, 1.0f);
    ImVec4 inPalette = presetChosen >= 0 ? swatches[presetChosen] : kNoSwatch;
    // The preset names are the swatches' tooltips, on hover and on keyboard focus.
    const char* names[kAccentPickerMaxSwatches] = {};
    for (int i = 0; i < count; ++i)
        names[i] = i == 0 && hasThemeDefault ? s.themeDefaultAccent : presets[i].name;
    SwatchPaletteOptions palette;
    palette.size = options.size;
    palette.readOnly = readOnly;
    palette.names = std::span<const char* const>(names, static_cast<std::size_t>(count));
    const bool picked = SwatchPalette("##swatches", &inPalette, std::span<const ImVec4>(swatches, static_cast<std::size_t>(count)),
                                      nullptr, palette);
    if (picked && !readOnly)
    {
        // The palette wrote the swatch's color; the first swatch equal to it is the choice.
        int index = -1;
        for (int i = 0; i < count && index < 0; ++i)
        {
            if (AccentPickerSameColor(swatches[i], inPalette))
                index = i;
        }
        if (index == 0 && hasThemeDefault)
        {
            chooseDefault = true;
            written = !*themeDefault;
        }
        else if (index >= 0)
        {
            chosenColor = swatches[index];
            written = defaultChosen || !AccentPickerSameColor(chosenColor, current);
        }
    }

    // The well for a custom color.
    if (options.customColor)
    {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + width - wellWidth, origin.y));
        if (forced.state != internal::ForcedItemState::None && forced.part == wellPart)
            internal::ForceNextItemState(forced.state);
        ImVec4 custom = current;
        ColorWellOptions well;
        well.size = options.size;
        well.width = wellWidth / Scale();
        well.readOnly = readOnly;
        if (ColorWell("##custom", &custom, well) && !readOnly)
        {
            chosenColor = WithAlpha(custom, 1.0f);
            chooseDefault = false;
            written = defaultChosen || !AccentPickerSameColor(chosenColor, current);
        }
        const ImRect wellRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ItemTooltip("%s", s.customColor);
        if (customChosen && !defaultChosen && ImGui::IsRectVisible(wellRect.Min, wellRect.Max))
        {
            const float out = HairlineThickness() * (kAccentPickerRingGap + 1.0f);
            ImRect ring = wellRect;
            ring.Expand(out);
            internal::OutlineLook chosenEdge;
            chosenEdge.selected = true;
            chosenEdge.disabled = disabled;
            internal::DrawOutline(window->DrawList, ring, m.radius + out, internal::ControlOutlineColor(chosenEdge));
        }
    }
    ImGui::PopID();

    // The label in front, centered on the first row.
    if (front.text != nullptr)
    {
        const ImVec2 at(origin.x, CenteredTextY(origin.y, side));
        if (ImGui::IsRectVisible(origin, ImVec2(origin.x + front.width, origin.y + side)))
        {
            internal::ControlTextScope text(m);
            internal::DrawFrontLabel(window->DrawList, front, at, m.size, disabled);
        }
        const ImRect labelRect(origin, ImVec2(origin.x + front.width, origin.y + side));
        internal::CutTextTooltip(id, labelRect, ImGui::IsMouseHoveringRect(labelRect.Min, labelRect.Max) && ImGui::IsWindowHovered(),
                                 false, front.text, front.end, front.shown.truncated);
    }
    // The group covers the label and the whole width even when the swatches end earlier.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, side));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);

    if (!written)
        return false;
    if (chooseDefault)
    {
        *themeDefault = true;
        if (options.apply)
        {
            ClearAccent();
            SetThemeMode(GetThemeMode());
        }
    }
    else
    {
        *accent = ImVec4(chosenColor.x, chosenColor.y, chosenColor.z, accent->w);
        if (themeDefault != nullptr)
            *themeDefault = false;
        if (options.apply)
            SetAccent(chosenColor);
    }
    internal::MarkWidgetEdited(id);
    return true;
}

} // namespace facet
