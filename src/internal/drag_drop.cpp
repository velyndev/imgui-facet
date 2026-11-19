// Drag and drop core (drag_drop.h): the drag in progress, drop zones, and the drag preview drawn
// on top of everything at facet::EndFrame.
// The functions below are linking stubs of the frozen interface until the drag and drop core is
// implemented in this file: no item becomes a drag source, no drag is ever in progress and no
// zone is hovered. The module has no hooks yet.

#include "internal/drag_drop.h"

#include "internal/subsystem.h"

namespace facet::internal
{

bool DragDropBeginSource(const char*, const void*, std::size_t, const DragDropPreview&)
{
    return false;
}

void DragDropEndSource()
{
}

bool DragDropActive(const char*, DragDropInfo* info)
{
    if (info != nullptr)
        *info = DragDropInfo{};
    return false;
}

DragDropZoneResult DragDropZone(ImGuiID, const ImRect&, const char*, const DragDropZoneOptions&)
{
    return {};
}

int DragDropInsertIndex(std::span<const ImRect>, Orientation, ImVec2)
{
    return -1;
}

Subsystem DragDropSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "drag_drop";
    return subsystem;
}

} // namespace facet::internal
