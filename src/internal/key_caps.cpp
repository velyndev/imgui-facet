// Key caps (key_caps.h).

#include "internal/key_caps.h"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/keys.h"

namespace facet::internal
{
namespace
{

TextStyle KeyCapTextStyle(ControlSize size)
{
    return ResolveControlSize(size) == ControlSize::Small ? TextStyle::Caption : TextStyle::Footnote;
}

// The names of the caps of a chord, modifiers first in the order of kChordModifiers. Returns the
// count (at most five).
int KeyCapNames(ImGuiKeyChord chord, const char* (&names)[5])
{
    ImGuiKeyChord mods = ChordMods(chord);
    ImGuiKey key = ChordKey(chord);
    if (IsSidedModifierKey(key))
    {
        mods |= ModifierFlagOf(key);
        key = ImGuiKey_None;
    }
    int count = 0;
    for (const ImGuiKeyChord mod : kChordModifiers)
        if ((mods & mod) != 0)
            names[count++] = KeyNameText(static_cast<ImGuiKey>(mod));
    if (key != ImGuiKey_None)
        names[count++] = KeyNameText(key);
    return count;
}

float KeyCapWidth(const char* name, float height, float padding)
{
    return std::max(height, std::round(ImGui::CalcTextSize(name).x + padding * 2.0f));
}

} // namespace

float KeyCapHeight(ControlSize size)
{
    return std::max(1.0f, ControlHeight(size) - Px(T().spacing.xs) * 2.0f);
}

float MeasureKeyCaps(ImGuiKeyChord chord, ControlSize size)
{
    const char* names[5] = {};
    const int count = KeyCapNames(chord, names);
    if (count == 0)
        return 0.0f;
    ControlTextScope text(KeyCapTextStyle(size));
    const float height = KeyCapHeight(size);
    const float padding = Px(T().spacing.xs);
    float width = 0.0f;
    for (int i = 0; i < count; ++i)
        width += KeyCapWidth(names[i], height, padding);
    return width + padding * static_cast<float>(count - 1);
}

float DrawKeyCaps(ImDrawList* drawList, ImGuiKeyChord chord, ImVec2 leftCenter, ControlSize size, bool disabled,
                  bool pressed)
{
    const char* names[5] = {};
    const int count = KeyCapNames(chord, names);
    if (count == 0 || drawList == nullptr)
        return 0.0f;
    const Theme& t = T();
    const Colors& c = t.colors;
    ControlTextScope text(KeyCapTextStyle(size));
    const float height = KeyCapHeight(size);
    const float padding = Px(t.spacing.xs);
    const float radius = std::min(Px(t.radii.element), height * 0.5f);
    const float top = std::round(leftCenter.y - height * 0.5f);
    const ImU32 fill = ToU32(pressed && !disabled ? c.fill[1] : c.bg);
    const ImU32 edge = ToU32(disabled ? c.separator : ControlBoundaryColor(c));
    const ImU32 ink = ToU32(ControlLabelColor(disabled));
    float x = std::round(leftCenter.x);
    for (int i = 0; i < count; ++i)
    {
        const float width = KeyCapWidth(names[i], height, padding);
        const ImVec2 min(x, top);
        const ImVec2 max(x + width, top + height);
        FillRect(drawList, min, max, fill, radius);
        StrokeRect(drawList, min, max, edge, radius, HairlineThickness());
        const float textWidth = ImGui::CalcTextSize(names[i]).x;
        drawList->AddText(ImVec2(std::round(x + (width - textWidth) * 0.5f), CenteredTextY(top, height)), ink, names[i]);
        x += width + padding;
    }
    return x - padding - std::round(leftCenter.x);
}

} // namespace facet::internal
