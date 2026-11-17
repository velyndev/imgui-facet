// Item behaviour (item.h). For now only the forced state request; the behaviour itself is
// added by the owner, the interaction blocks package.

#include "internal/item.h"

namespace facet::internal
{
namespace
{

ForcedItemRequest& Pending()
{
    static ForcedItemRequest request;
    return request;
}

} // namespace

void ForceNextItemState(ForcedItemState state, int part)
{
    ForcedItemRequest& request = Pending();
    request.state = state;
    request.part = part;
}

ForcedItemRequest TakeForcedItemState()
{
    const ForcedItemRequest request = Pending();
    Pending() = {};
    return request;
}

void ClearForcedItemState()
{
    Pending() = {};
}

} // namespace facet::internal
