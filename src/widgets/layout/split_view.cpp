// BeginSplitView, SplitViewNext and EndSplitView (layout.h). Planned: shows both panes one
// under the other inside the frame placeholder of src/widgets/structure_stub.h until Phase 2
// implements the split view. *ratio is never written. The panes get ids under the view's id,
// as they will in their child windows.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginSplitView(const char* id, float*, const SplitViewOptions&)
{
    internal::BeginStubFrame("BeginSplitView", "SplitView", id, true);
}

void SplitViewNext()
{
}

void EndSplitView()
{
    internal::EndStubFrame("BeginSplitView");
}

} // namespace facet
