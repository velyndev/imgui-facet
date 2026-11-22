// Button variant colors (button_look.h).

#include "widgets/buttons/button_look.h"

#include "facet/core/color.h"
#include "facet/core/theme.h"
#include "internal/control.h"

namespace facet::internal
{

ButtonColors ButtonLook(ButtonVariant variant, const ItemState& item)
{
    const Colors& c = T().colors;
    const ImVec4 none = WithAlpha(c.fill[1], 0.0f);
    ButtonColors colors;
    colors.outline = WithAlpha(c.separator, 0.0f);
    if (item.disabled)
    {
        // One disabled look for every variant, so a disabled primary action does not still
        // claim the accent.
        colors.fill = variant == ButtonVariant::Plain ? none : c.fill[3];
        colors.content = ControlLabelColor(true);
        if (variant != ButtonVariant::Plain)
            colors.outline = c.separator;
        return colors;
    }
    switch (variant)
    {
    case ButtonVariant::Primary:
        colors.fill = StateFill(c.accent, c.accentHover, c.accentPressed, item);
        colors.content = c.onAccent;
        // The fill alone shapes the button while the accent reaches 3:1 on every surface, so no
        // lighter rim shows around it in the dark theme. A light accent that does not reach it gets
        // the focusRing edge (the accent moved to 3:1), so the button's edge stays visible.
        if (!ButtonAccentStandsOut(c))
            colors.outline = c.focusRing;
        break;
    case ButtonVariant::Plain:
        colors.fill = StateFill(none, c.fill[1], c.fill[0], item);
        colors.content = c.label;
        break;
    case ButtonVariant::Destructive:
        // The destructive color reaches 3:1 on the content surfaces, so it needs no outline.
        colors.fill = StateFill(c.destructive, c.destructiveHover, c.destructivePressed, item);
        colors.content = c.onDestructive;
        break;
    case ButtonVariant::Secondary:
    default:
        colors.fill = NeutralFill(item);
        colors.content = c.label;
        {
            // The one controlBorder hairline, controlBorderHover under the pointer: the state
            // changes the color and the fill, never the width.
            OutlineLook outline;
            outline.hoverAmount = item.hoverAmount;
            colors.outline = ControlOutlineColor(outline);
        }
        break;
    }
    return colors;
}

bool ButtonAccentStandsOut(const Colors& c)
{
    constexpr float kNonTextContrast = 3.0f;
    const ImVec4 surfaces[] = {c.bg, c.bgSecondary, c.bgTertiary, c.ElevatedSurface(1), c.ElevatedSurface(2),
                               c.ElevatedSurface(3)};
    for (const ImVec4& surface : surfaces)
        if (ContrastRatio(c.accent, surface) < kNonTextContrast)
            return false;
    return true;
}

} // namespace facet::internal
