// BeginScrollArea and EndScrollArea (containers.h): the scroll area of the scroll core
// (src/internal/scroll.h) as a public container. The core makes the child window, takes the wheel
// for smooth scrolling, draws the overlay bars and the edge fades, moves the focus by a page with
// Page Up and Down and to the ends with Home and End, and closes the child window itself when the
// area is out of sight.

#include "facet/widgets/containers.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"

namespace facet
{

bool BeginScrollArea(const char* id, const ScrollAreaOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::ScrollAreaDesc desc;
    desc.size.x = internal::ResolveItemWidth(options.width, next, kFill);
    // 0 fills the available height; a negative height leaves that many units free below it.
    desc.size.y = options.height > 0.0f ? Px(options.height) : (options.height < 0.0f ? -Px(-options.height) : 0.0f);
    desc.horizontal = options.horizontal;
    desc.edgeFades = options.edgeFades;
    if (!internal::BeginScrollAreaCore(id, desc))
        return false;
    internal::PushScope("BeginScrollArea");
    return true;
}

void EndScrollArea()
{
    if (!internal::PopScope("BeginScrollArea"))
        return;
    internal::EndScrollAreaCore();
}

} // namespace facet
