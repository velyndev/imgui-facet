// Behaviour of the inventory, card and people composites and the public drag and drop calls
// (InventoryGrid, InventoryTile, InventorySlot, InventoryDetailCard, MediaCard, UserChip,
// ProfileSwitcher, BeginDragSource and BeginDropTarget). The drag and drop core is tested in
// tests/test_drag_drop.cpp.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_composites_cards.cpp. No widget of this group is implemented yet.

#include <doctest/doctest.h>
