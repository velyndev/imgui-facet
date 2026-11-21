#pragma once

// The tinted alert surface Banner and InlineAlert share (facet/widgets/display.h): the tone's wash
// with its icon, a Headline title and a wrapped message, actions as small buttons and a close
// button. Dismissing (the close button, or Escape while an action or the close button has keyboard
// focus) writes *open = false; afterwards the surface collapses over motion.durationStandard (at
// once with reduced motion) and then takes no space.
//
//     const DisplayCalloutResult r = DisplayCallout({.label = label, .open = open, .message = message,
//                                                     .tone = options.tone, .primary = options.primaryAction});
//     if (r.action == DisplayCalloutAction::Primary) ...

#include "facet/core/icons.h"
#include "facet/widgets/common.h"

namespace facet::internal
{

enum class DisplayCalloutAction
{
    None,
    Primary,
    Secondary,
    Dismissed,
};

struct DisplayCalloutSpec
{
    const char* label = nullptr;      // the visible part is the title; the whole label is the id
    bool* open = nullptr;             // null: no close button, always shown
    const char* message = nullptr;    // wrapped body text; null for none
    Tone tone = Tone::Neutral;
    Icon icon;                        // none: the tone's icon
    const char* primary = nullptr;    // button texts; null for none
    const char* secondary = nullptr;
    bool actionsBeside = false;       // Banner: the actions sit at the end of the title line when there is room
    bool outline = false;             // InlineAlert: a hairline around the box
    float width = 0.0f;               // units, as options.width
};

struct DisplayCalloutResult
{
    DisplayCalloutAction action = DisplayCalloutAction::None;
};

DisplayCalloutResult DisplayCallout(const DisplayCalloutSpec& spec);

} // namespace facet::internal
