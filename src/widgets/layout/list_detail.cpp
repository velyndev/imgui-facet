// BeginListDetail, the pane Begins and their Ends (layout.h). Planned: shows the panes one under
// the other inside frame placeholders of src/widgets/structure_stub.h until Phase 2 implements
// the layout. In a Compact width only the pane that *detailOpen selects is shown, as declared;
// there is no back button yet and *detailOpen is never written. The stub keeps the state of the
// latest BeginListDetail only, so a list-detail nested in a pane answers for itself until its End.

#include "facet/core/layout.h"
#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

struct ListDetailStub
{
    bool compact = false;
    bool detailOpen = false;
};

ListDetailStub g_listDetail;

} // namespace

void BeginListDetail(const char* id, bool* detailOpen, const ListDetailOptions&)
{
    g_listDetail.compact = internal::StubCanDraw() && AvailableWidthClass() == WidthClass::Compact;
    g_listDetail.detailOpen = detailOpen != nullptr && *detailOpen;
    internal::BeginStubFrame("BeginListDetail", "ListDetail", id, false);
}

void EndListDetail()
{
    internal::EndStubFrame("BeginListDetail");
    g_listDetail = {};
}

bool BeginListPane()
{
    if ((g_listDetail.compact && g_listDetail.detailOpen) || !internal::StubCanDraw())
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
    if ((g_listDetail.compact && !g_listDetail.detailOpen) || !internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginDetailPane", "DetailPane", nullptr, false);
    return true;
}

void EndDetailPane()
{
    internal::EndStubFrame("BeginDetailPane");
}

} // namespace facet
