// InlineAlert (facet/widgets/display.h): a tinted callout box inside the content, with a hairline
// around it, the tone's icon, a Headline title, the wrapped message, one action under the message
// and a close button. The surface is shared with Banner (display_callout.h).

#include "facet/widgets/display.h"

#include "widgets/display/display_callout.h"

namespace facet
{

InlineAlertAction InlineAlert(const char* label, bool* open, const char* message, const InlineAlertOptions& options)
{
    const internal::DisplayCalloutResult result = internal::DisplayCallout({.label = label,
                                                                            .open = open,
                                                                            .message = message,
                                                                            .tone = options.tone,
                                                                            .icon = options.icon,
                                                                            .primary = options.actionLabel,
                                                                            .outline = true,
                                                                            .width = options.width});
    switch (result.action)
    {
    case internal::DisplayCalloutAction::Primary:
        return InlineAlertAction::Action;
    case internal::DisplayCalloutAction::Dismissed:
        return InlineAlertAction::Dismissed;
    default:
        return InlineAlertAction::None;
    }
}

} // namespace facet
