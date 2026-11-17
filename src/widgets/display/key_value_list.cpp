// BeginKeyValueList, KeyValue and EndKeyValueList (facet/widgets/display.h): planned. A
// placeholder until Phase 2 implements them; the rows follow the header placeholder.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void BeginKeyValueList(const char* id, const KeyValueListOptions&)
{
    namespace stub = internal::content;
    stub::StubBegin("BeginKeyValueList", "KeyValueList", id, stub::ResolveWidth(0.0f, 0.0f), stub::ControlRows(1.0f));
}

void EndKeyValueList()
{
    internal::content::StubEnd("BeginKeyValueList");
}

bool KeyValue(const char* label, const char*, const KeyValueOptions&)
{
    namespace stub = internal::content;
    stub::Stub("KeyValue", label, stub::ResolveWidth(0.0f, 0.0f), stub::ControlRows(1.0f));
    return false;
}

} // namespace facet