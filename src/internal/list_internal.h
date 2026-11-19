#pragma once

// Links between list_core.cpp (row ranges and selection) and list_view.cpp (views and trees).
// Not for other modules: they use list_core.h and list_view.h.

#include <cstdint>

#include "internal/list_core.h"
#include "internal/list_view.h"

namespace facet::internal
{

// BeginListSelection with entry information: disabled entries (ListEntryInfo::disabled) are left
// out of Shift ranges and of select-all before the requests reach the storage. Closed by
// EndListSelection like the plain form.
void BeginListSelectionFiltered(ListSelection* selection, int itemCount, const ListSelectionOptions& options,
                                ListEntryFn entry, void* user);

// Applies `io`'s requests to `selection` (ApplyRequests, so ranges and select-all are batched),
// leaving disabled entries out when `entry` is set. io->ItemsCount must be `itemCount`.
void ApplyListRequests(ListSelection* selection, ImGuiMultiSelectIO* io, int itemCount, ListEntryFn entry, void* user);

// A summary of the ids selected in `selection`: two summaries differ when the set differs (up to
// hash collisions). Linear in the storage size; for frames that carry selection requests.
std::uint64_t ListSelectionFingerprint(const ListSelection& selection);

// list_core.cpp: reports and resets open row ranges and selections.
void ListRowsEndFrame();
void ListRowsShutdown();

} // namespace facet::internal
