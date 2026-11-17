// BeginListDetail, the pane Begins and their Ends (layout.h). Planned: shows the panes one under
// the other inside frame placeholders of src/widgets/structure_stub.h until Phase 2 implements
// the layout. In a Compact width only the pane that *detailOpen selects is shown, as declared;
// there is no back button yet and *detailOpen is never written. Each BeginListDetail pushes its
// state and EndListDetail pops it, so the panes of an outer list-detail still answer for it after
// a list-detail nested in one of its panes has ended.

#include "facet/core/layout.h"
#include "facet/widgets/layout.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

struct ListDetailStub
{
    int scopes = 0;          // open scopes before the Begin; the entry lives while more are open
    bool compact = false;
    bool detailOpen = false;
};

// One entry per open BeginListDetail, innermost last. Levels past the scope stack depth are
// reported by the scope stack and share the innermost stored entry.
ListDetailStub g_stack[internal::kScopeStackDepth];
int g_depth = 0;

// Drops the entries whose scope is closed: by their End, by an outer End that closed them
// too, or by facet::EndFrame for a list-detail left open.
void Prune()
{
    while (g_depth > 0 && g_stack[g_depth - 1].scopes >= internal::OpenScopeCount())
        --g_depth;
}

ListDetailStub Top()
{
    Prune();
    return g_depth > 0 ? g_stack[g_depth - 1] : ListDetailStub{};
}

} // namespace

void BeginListDetail(const char* id, bool* detailOpen, const ListDetailOptions&)
{
    Prune();
    if (g_depth < internal::kScopeStackDepth)
    {
        ListDetailStub& entry = g_stack[g_depth++];
        entry.scopes = internal::OpenScopeCount();
        entry.compact = internal::StubCanDraw() && AvailableWidthClass() == WidthClass::Compact;
        entry.detailOpen = detailOpen != nullptr && *detailOpen;
    }
    internal::BeginStubFrame("BeginListDetail", "ListDetail", id, false);
}

void EndListDetail()
{
    internal::EndStubFrame("BeginListDetail");
    Prune();
}

bool BeginListPane()
{
    const ListDetailStub top = Top();
    if ((top.compact && top.detailOpen) || !internal::StubCanDraw())
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
    const ListDetailStub top = Top();
    if ((top.compact && !top.detailOpen) || !internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginDetailPane", "DetailPane", nullptr, false);
    return true;
}

void EndDetailPane()
{
    internal::EndStubFrame("BeginDetailPane");
}

} // namespace facet
