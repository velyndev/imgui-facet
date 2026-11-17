// BeginListDetail, the pane Begins and their Ends (layout.h). Planned: shows both panes one
// under the other inside frame placeholders of src/widgets/structure_stub.h until Phase 2
// implements the layout. *detailOpen is never written.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginListDetail(const char* id, bool*, const ListDetailOptions&)
{
    internal::BeginStubFrame("BeginListDetail", "ListDetail", id, false);
}

void EndListDetail()
{
    internal::EndStubFrame("BeginListDetail");
}

bool BeginListPane()
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginListPane", "ListPane", nullptr, false);
    return true;
}

void EndListPane()
{
    internal::EndStubFrame("BeginListPane");
}

bool BeginDetailPane()
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginDetailPane", "DetailPane", nullptr, false);
    return true;
}

void EndDetailPane()
{
    internal::EndStubFrame("BeginDetailPane");
}

} // namespace facet
