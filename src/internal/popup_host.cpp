// Popup host: anchoring with flip and clamp, the nested popup stack, and the close animation
// that replays a popup's last draw data while it fades.
// Owner from Phase 1c part B on: the surface blocks package, which adds popup_host.h and the
// behaviour. For now the module has no hooks.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem PopupHostSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "popup_host";
    return subsystem;
}

} // namespace facet::internal