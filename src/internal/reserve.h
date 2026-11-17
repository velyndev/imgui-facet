#pragma once

// Capacity helper for bookkeeping that reserves room ahead of use.

#include <algorithm>
#include <cstddef>

namespace facet::internal
{

// Makes sure `container` can hold `need` elements without allocating again. When it has to
// grow it adds at least half of the current capacity, so asking for one more element on
// every call stays cheap: a plain reserve(need) may allocate exactly `need` and would then
// move every element on each call.
template <typename Container>
void ReserveAtLeast(Container& container, std::size_t need)
{
    const std::size_t capacity = container.capacity();
    if (capacity < need)
        container.reserve(std::max(need, capacity + capacity / 2));
}

} // namespace facet::internal
