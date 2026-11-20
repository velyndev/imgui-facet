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
        // focusRing is the accent moved to 3:1 on every surface (the accent itself when it already
        // reaches it), so the edge of a light accent stays visible.
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
        colors.outline = ControlBoundaryColor(c);
        break;
    }
    return colors;
}

} // namespace facet::internal
