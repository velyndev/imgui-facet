// SearchFilterBar (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool SearchFilterBar(const char* label, std::string*, std::span<FilterChip>, const SearchFilterBarOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("SearchFilterBar", label, stub::ResolveWidth(options.width, 0.0f), stub::ControlRows(1.0f));
    return false;
}

bool SearchFilterBar(const char* label, char*, std::size_t, std::span<FilterChip>, const SearchFilterBarOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("SearchFilterBar", label, stub::ResolveWidth(options.width, 0.0f), stub::ControlRows(1.0f));
    return false;
}

} // namespace facet