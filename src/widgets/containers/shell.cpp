// BeginShell and EndShell (containers.h). Planned: until Phase 2 implements the shell, no
// window opens and BeginShell returns false; while the caller asks for the shell to be open,
// the placeholder of src/widgets/structure_stub.h shows the request in the page flow.

#include "facet/widgets/containers.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginShell(const char* label, bool* open, const ShellOptions&)
{
    internal::StubLayer("Shell", label, open == nullptr || *open);
    return false;
}

void EndShell()
{
    // BeginShell never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginShell");
}

} // namespace facet
