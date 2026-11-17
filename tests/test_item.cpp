// Item behaviour (src/internal/item.h): the forced state request used by capture runs.

#include <doctest/doctest.h>

#include <facet/facet.h>

#include "internal/item.h"
#include "support/session.h"

using facet::internal::ForcedItemState;

TEST_CASE("A forced state request is taken once by the next widget")
{
    facet::internal::ClearForcedItemState();
    facet::internal::ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Focused, 1);
    const facet::internal::ForcedItemRequest taken = facet::internal::TakeForcedItemState();
    CHECK(Has(taken.state, ForcedItemState::Hovered));
    CHECK(Has(taken.state, ForcedItemState::Focused));
    CHECK_FALSE(Has(taken.state, ForcedItemState::Pressed));
    CHECK(taken.part == 1);
    // Taken: the widget after it gets nothing.
    const facet::internal::ForcedItemRequest next = facet::internal::TakeForcedItemState();
    CHECK(next.state == ForcedItemState::None);
    CHECK(next.part == -1);
}

TEST_CASE("A second request replaces the first")
{
    facet::internal::ClearForcedItemState();
    facet::internal::ForceNextItemState(ForcedItemState::Hovered);
    facet::internal::ForceNextItemState(ForcedItemState::Dragging, 0);
    const facet::internal::ForcedItemRequest taken = facet::internal::TakeForcedItemState();
    CHECK(taken.state == ForcedItemState::Dragging);
    CHECK(taken.part == 0);
}

TEST_CASE("A request no widget took is dropped at EndFrame")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([] { facet::internal::ForceNextItemState(ForcedItemState::Open); });
    session.Frame();
    CHECK(facet::internal::TakeForcedItemState().state == ForcedItemState::None);
}
