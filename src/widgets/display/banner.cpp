// Banner (facet/widgets/display.h): a full-width strip in the tone's wash for a condition of the
// whole view, with the tone's icon, a bold title, the wrapped message, up to two actions (at the
// end of the title line when there is room, under the message otherwise) and a close button. The
// surface is shared with InlineAlert (display_callout.h).

#include "facet/widgets/display.h"

#include "widgets/display/display_callout.h"

namespace facet
{

BannerAction Banner(const char* label, bool* open, const char* message, const BannerOptions& options)
{
    const internal::DisplayCalloutResult result = internal::DisplayCallout({.label = label,
                                                                            .open = open,
                                                                            .message = message,
                                                                            .tone = options.tone,
                                                                            .icon = options.icon,
                                                                            .primary = options.primaryAction,
                                                                            .secondary = options.secondaryAction,
                                                                            .actionsBeside = true,
                                                                            .width = options.width});
    switch (result.action)
    {
    case internal::DisplayCalloutAction::Primary:
        return BannerAction::Primary;
    case internal::DisplayCalloutAction::Secondary:
        return BannerAction::Secondary;
    case internal::DisplayCalloutAction::Dismissed:
        return BannerAction::Dismissed;
    case internal::DisplayCalloutAction::None:
    default:
        return BannerAction::None;
    }
}

} // namespace facet
