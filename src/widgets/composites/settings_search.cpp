// Settings search (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements
// it. SettingsSearchMatch matches every row, as for an empty query, so the caller's rows show.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

void BeginSettingsSearch(const char* id, const char*)
{
    namespace stub = internal::content;
    stub::StubBegin("BeginSettingsSearch", "SettingsSearch", id, stub::ResolveWidth(0.0f, 0.0f), stub::ControlRows(1.0f));
}

void EndSettingsSearch()
{
    internal::content::StubEnd("BeginSettingsSearch");
}

bool SettingsSearchMatch(const char*, const SettingsSearchMatchOptions&)
{
    return true;
}

int SettingsSearchHitCount(const char*, const char*)
{
    return 0;
}

} // namespace facet