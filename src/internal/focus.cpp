// Focus ring and keyboard modality: whether the focus ring shows (only after keyboard
// input), Escape closing one layer at a time, the Menu key opening context menus.
// Owner from Phase 1c part A on: the interaction blocks package, which adds focus.h and the
// behaviour. For now the module only drops forced item states no widget took.

#include "internal/item.h"
#include "internal/subsystem.h"

namespace facet::internal
{
namespace
{

void FocusEndFrame()
{
    ClearForcedItemState();
}

} // namespace

Subsystem FocusSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "focus";
    subsystem.endFrame = &FocusEndFrame;
    return subsystem;
}

} // namespace facet::internal
