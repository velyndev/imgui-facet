// BeginInspectorPane, InspectorPaneNext and EndInspectorPane (containers.h). Planned: shows the
// main content and, while the caller shows the pane, the pane content one after the other inside
// the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the pane.
// *visible is never written. Each Begin pushes its visible flag and the End pops it, so an outer
// pane still answers for itself after a pane nested in its main content has ended.

#include "facet/widgets/containers.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

struct PaneStub
{
    int scopes = 0;          // open scopes before the Begin; the entry lives while more are open
    bool shown = false;
};

// One entry per open BeginInspectorPane, innermost last. Levels past the scope stack depth are
// reported by the scope stack and share the innermost stored entry.
PaneStub g_stack[internal::kScopeStackDepth];
int g_depth = 0;

// Drops the entries whose scope is closed: by their End, by an outer End that closed them
// too, or by facet::EndFrame for a pane left open.
void Prune()
{
    while (g_depth > 0 && g_stack[g_depth - 1].scopes >= internal::OpenScopeCount())
        --g_depth;
}

} // namespace

void BeginInspectorPane(const char* label, bool* visible, const InspectorPaneOptions&)
{
    Prune();
    if (g_depth < internal::kScopeStackDepth)
        g_stack[g_depth++] = {internal::OpenScopeCount(), visible == nullptr || *visible};
    internal::BeginStubFrame("BeginInspectorPane", "InspectorPane", label, false);
}

bool InspectorPaneNext()
{
    Prune();
    return g_depth > 0 && g_stack[g_depth - 1].shown && internal::StubCanDraw();
}

void EndInspectorPane()
{
    internal::EndStubFrame("BeginInspectorPane");
    Prune();
}

} // namespace facet
