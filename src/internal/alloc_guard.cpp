#include "internal/alloc_guard.h"

#include <new>

namespace facet::internal
{
namespace
{

thread_local int countdown = 0;
thread_local bool failed = false;

} // namespace

void FailGuardedAllocation(int count)
{
    countdown = count > 0 ? count : 0;
    failed = false;
}

bool GuardedAllocationFailed()
{
    return failed;
}

void CheckAllocation()
{
    if (countdown == 0)
        return;
    if (--countdown == 0)
    {
        failed = true;
        throw std::bad_alloc();
    }
}

} // namespace facet::internal
