// ItemTooltip, BeginRichTooltip and EndRichTooltip (overlays.h). Planned: no tooltip shows until
// Phase 2 implements them; like a tooltip that is not shown, they draw nothing.

#include "facet/widgets/overlays.h"
#include "internal/scope_stack.h"

namespace facet
{

void ItemTooltip(const char*, ...)
{
}

bool BeginRichTooltip(const TooltipOptions&)
{
    return false;
}

void EndRichTooltip()
{
    // BeginRichTooltip never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginRichTooltip");
}

} // namespace facet
