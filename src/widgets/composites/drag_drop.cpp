// The public drag and drop calls (facet/widgets/composites.h): planned. They wait for the drag
// and drop core of Phase 1c part B; until then no item is a drag source or a drop target, so
// every Begin returns false and AcceptDrop returns null.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool BeginDragSource(const char*, const void*, std::size_t, const DragSourceOptions&)
{
    return false;
}

void EndDragSource()
{
    internal::content::StubEnd("BeginDragSource");
}

bool BeginDropTarget()
{
    return false;
}

const ImGuiPayload* AcceptDrop(const char*, const AcceptDropOptions&)
{
    return nullptr;
}

void EndDropTarget()
{
    internal::content::StubEnd("BeginDropTarget");
}

} // namespace facet