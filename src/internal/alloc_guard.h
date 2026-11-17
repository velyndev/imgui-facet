#pragma once

// Guarded allocations: the places where the image and icon modules reserve memory that can
// realistically run out (file buffers, copies of caller data, SVG documents and rasters,
// slot bookkeeping). Each one is marked with CheckAllocation() inside the try block that
// turns std::bad_alloc into the function's failure value.
//
// FailGuardedAllocation lets a test make one of these allocations fail on purpose, so every
// failure path can be run without exhausting memory. The countdown is per thread: arming it
// on the frame thread never affects a decode on a worker thread.

namespace facet::internal
{

// Makes the `count`-th guarded allocation from now on the calling thread throw
// std::bad_alloc (1 for the next one). 0 disarms. For tests.
void FailGuardedAllocation(int count);

// True when the failure armed by FailGuardedAllocation was thrown on this thread since it
// was armed.
bool GuardedAllocationFailed();

// Marks a guarded allocation: throws std::bad_alloc when the armed failure is due.
void CheckAllocation();

} // namespace facet::internal
