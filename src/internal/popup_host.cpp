// Popup host (popup_host.h): anchoring with flip and clamp, the nested level stack, the close
// motion that replays a level's last draw data while it fades, and the Escape and context menu
// key routing between layers.
// The functions below are linking stubs of the frozen interface until the popup host is
// implemented in this file: no level ever opens, Escape is claimed by no layer and no context
// menu is requested. The module has no hooks yet; edit sessions already take Escape with
// SetKeyOwner.

#include "internal/popup_host.h"

#include "internal/subsystem.h"

namespace facet::internal
{

void OpenPopupLevel(ImGuiID, const PopupAnchor&, const PopupOptions&)
{
}

void ClosePopupLevel(ImGuiID)
{
}

bool IsPopupLevelOpen(ImGuiID)
{
    return false;
}

bool BeginPopupLevel(ImGuiID)
{
    return false;
}

void EndPopupLevel()
{
}

PopupCloseReason PopupLevelClosedReason(ImGuiID)
{
    return PopupCloseReason::None;
}

ImGuiID TopPopupLevel()
{
    return 0;
}

void ForcePopupLevelOpen(ImGuiID, const PopupAnchor&, const PopupOptions&)
{
}

bool ClaimEscape(ImGuiID, EscapeLayer)
{
    return false;
}

bool ContextMenuRequested(ContextRequestTarget, ImVec2* at)
{
    if (at != nullptr)
        *at = ImVec2(0.0f, 0.0f);
    return false;
}

Subsystem PopupHostSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "popup_host";
    return subsystem;
}

} // namespace facet::internal
