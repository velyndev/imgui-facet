// Scroll core (scroll.h): overlay scrollbars, smooth wheel scrolling and edge fades.
// The functions below are linking stubs of the frozen interface until the scroll core is
// implemented in this file: they move nothing and report that nothing scrolls. The module has
// no hooks yet.

#include "internal/scroll.h"

#include "internal/subsystem.h"

namespace facet::internal
{

void ScrollAreaTo(ImGuiWindow*, ImVec2, bool)
{
}

void ScrollAreaRevealRect(ImGuiWindow*, const ImRect&, ScrollAlign, bool)
{
}

ImVec2 ScrollAreaTarget(ImGuiWindow* window)
{
    return window != nullptr ? window->Scroll : ImVec2(0.0f, 0.0f);
}

bool IsScrollAreaScrolling(ImGuiWindow*)
{
    return false;
}

ImGuiWindow* ScrollAreaAt(ImVec2)
{
    return nullptr;
}

bool RequestScrollAreaEdgeScroll(ImGuiWindow*, ImVec2)
{
    return false;
}

Subsystem ScrollSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "scroll";
    return subsystem;
}

} // namespace facet::internal
