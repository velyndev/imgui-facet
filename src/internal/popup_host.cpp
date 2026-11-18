// Popup host: anchoring with flip and clamp, the nested popup stack, and the close animation
// that replays a popup's last draw data while it fades.
// Planned for Phase 1c part B: popup_host.h and the behaviour, including the Escape routing of
// docs/api-conventions.md rule 8 (one Escape acts on the innermost layer: the active edit, then
// the topmost popup, the modal, a shell with closeOnEscape) through key ownership. For now the
// module has no hooks; edit sessions already take Escape with SetKeyOwner.

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