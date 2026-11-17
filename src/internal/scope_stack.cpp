#include "internal/scope_stack.h"

#include <cstring>

#include "core/core_internal.h"
#include "internal/subsystem.h"

namespace facet::internal
{
namespace
{

struct ScopeStack
{
    const char* names[kScopeStackDepth] = {};
    int depth = 0;
    int overflow = 0;  // scopes beyond the capacity: counted, not named
};

ScopeStack& Stack()
{
    static ScopeStack stack;
    return stack;
}

bool SameName(const char* a, const char* b)
{
    return a == b || (a != nullptr && b != nullptr && std::strcmp(a, b) == 0);
}

const char* Printable(const char* name)
{
    return name != nullptr ? name : "?";
}

void ClearStack()
{
    ScopeStack& stack = Stack();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace

void PushScope(const char* name)
{
    ScopeStack& stack = Stack();
    if (stack.depth >= kScopeStackDepth)
    {
        if (stack.overflow == 0)
            ReportUsageError("Facet scopes nested deeper than %d; %s and deeper ones are not tracked by name",
                             kScopeStackDepth, Printable(name));
        ++stack.overflow;
        return;
    }
    stack.names[stack.depth++] = name;
}

bool PopScope(const char* name)
{
    ScopeStack& stack = Stack();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return true;
    }
    int found = -1;
    for (int i = stack.depth - 1; i >= 0; --i)
    {
        if (SameName(stack.names[i], name))
        {
            found = i;
            break;
        }
    }
    const char* label = Printable(name);
    if (found < 0)
    {
        ReportUsageError("End of %s without an open %s", label, label);
        return false;
    }
    for (int i = stack.depth - 1; i > found; --i)
        ReportUsageError("%s was left open; closed by the End of %s", Printable(stack.names[i]), label);
    stack.depth = found;
    return true;
}

int OpenScopeCount()
{
    const ScopeStack& stack = Stack();
    return stack.depth + stack.overflow;
}

const char* InnermostScope()
{
    const ScopeStack& stack = Stack();
    return stack.depth > 0 ? stack.names[stack.depth - 1] : nullptr;
}

void CloseOpenScopes()
{
    ScopeStack& stack = Stack();
    if (stack.overflow > 0)
        ReportUsageError("%d untracked Facet scope(s) still open at EndFrame", stack.overflow);
    for (int i = stack.depth - 1; i >= 0; --i)
        ReportUsageError("%s without its End at EndFrame", Printable(stack.names[i]));
    ClearStack();
}

// Last in the table: reports scopes left open after every module has drawn.
Subsystem ScopesSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "scopes";
    subsystem.endFrame = &CloseOpenScopes;
    subsystem.shutdown = &ClearStack;
    return subsystem;
}

} // namespace facet::internal
