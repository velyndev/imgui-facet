// Tests for the capacity helper used by bookkeeping that reserves ahead of use.

#include <cstddef>
#include <vector>

#include <doctest/doctest.h>

#include "internal/reserve.h"

TEST_CASE("ReserveAtLeast always covers the request and grows in few steps")
{
    std::vector<int> values;
    int allocations = 0;
    for (std::size_t need = 1; need <= 100000; ++need)
    {
        const std::size_t before = values.capacity();
        facet::internal::ReserveAtLeast(values, need);
        REQUIRE(values.capacity() >= need);
        if (values.capacity() != before)
            ++allocations;
        values.push_back(0);
    }
    // Growing by half each time reaches 100000 elements in about 30 steps; one allocation
    // per element would be 100000.
    CHECK(allocations < 64);
}

TEST_CASE("ReserveAtLeast leaves a container alone that is already large enough")
{
    std::vector<int> values;
    values.reserve(128);
    const std::size_t capacity = values.capacity();
    const int* data = values.data();
    facet::internal::ReserveAtLeast(values, 1);
    facet::internal::ReserveAtLeast(values, capacity);
    CHECK(values.capacity() == capacity);
    CHECK(values.data() == data);
}
