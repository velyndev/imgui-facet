// BeginContextMenu and EndContextMenu (overlays.h): a menu that opens on a right click, the Menu
// key or Shift+F10 over its target.
//
// The popup host reads the request (ContextMenuRequested: the right button released over the
// target, or the keys while it has keyboard focus, through key ownership) and opens the menu as a
// Popup level at the pointer, or at the bottom-left corner of the focused item for the keys. The
// host flips and moves it into the display, closes it on Escape and on a click outside, and gives
// the focus back. The rows are the menu row block (popup_menu.h): MenuLevelBegin here, and
// facet::MenuItem and BeginMenu inside, so a row closes the whole menu when it runs its action.

#include "facet/widgets/overlays.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/scope_stack.h"

namespace facet
{

bool BeginContextMenu(const char* id, const ContextMenuOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    // A capture shows the menu open at the target (ForcedItemState::Open), without input.
    const ForcedItemRequest forced = TakeForcedItemState();
    const ImGuiID level = window->GetID(id);

    PopupOptions popup;
    popup.kind = PopupKind::Popup;
    popup.minWidth = Px(T().overlay.menuMinWidth);
    const ContextRequestTarget target =
        options.target == ContextTarget::Window ? ContextRequestTarget::Window : ContextRequestTarget::LastItem;
    ImVec2 at;
    if (ContextMenuRequested(target, &at))
        OpenPopupLevel(level, {.rect = ImRect(at, at), .side = PopupSide::Below, .align = PopupAlign::Start}, popup);
    if (Has(forced.state, ForcedItemState::Open))
    {
        const ImRect& item = GImGui->LastItemData.Rect;
        const ImVec2 corner = options.target == ContextTarget::Window ? window->DC.CursorStartPos
                                                                       : ImVec2(item.Min.x, item.Max.y);
        ForcePopupLevelOpen(level, {.rect = ImRect(corner, corner), .side = PopupSide::Below}, popup);
    }
    if (!BeginPopupLevel(level))
        return false;
    MenuLevelBegin(level);
    PushScope("BeginContextMenu");
    return true;
}

void EndContextMenu()
{
    if (!internal::PopScope("BeginContextMenu"))
        return;
    internal::EndPopupLevel();
}

} // namespace facet
