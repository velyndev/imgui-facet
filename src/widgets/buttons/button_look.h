#pragma once

// The colors of the button variants (ButtonVariant in facet/widgets/buttons.h), shared by the
// widgets of the buttons family so that Button, IconButton, CopyButton and ToggleButtonGroup
// show one variant the same way:
//
//     const ButtonColors colors = ButtonLook(options.variant, item);
//     FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), radius);
//     if (colors.outline.w > 0.0f)
//         DrawOutline(drawList, item.rect, radius, colors.outline);   // src/internal/control.h
//     ... label and icon in colors.content ...
//
// Every variant takes the hover and press feedback from StateFill (src/internal/control.h): a state
// changes the fill and the outline color, never the outline width. Secondary has the one
// controlBorder hairline (controlBorderHover on hover); Primary and Destructive are shaped by their
// fill alone; Plain has neither at rest. Every variant has the same disabled look: fill[3] (none
// for Plain), a separator hairline around a filled one, and labelTertiary content.

#include <imgui.h>

#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/item.h"

namespace facet::internal
{

struct ButtonColors
{
    ImVec4 fill;     // the button surface; transparent for a Plain button at rest
    ImVec4 content;  // the label, the icon and the loading spinner
    ImVec4 outline;  // a hairline around the surface; alpha 0 for none
};

// The colors of `variant` for an item in the state `item` (hover and press amounts, disabled).
ButtonColors ButtonLook(ButtonVariant variant, const ItemState& item);

// True when the accent of `colors` reaches 3:1 on every surface of the theme, so an accent filled
// button needs no edge of its own; false for a light accent, which then gets the focusRing edge.
bool ButtonAccentStandsOut(const Colors& colors);

} // namespace facet::internal
