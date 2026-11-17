// LogView (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void LogView(const char* id, std::span<const LogEntry>, const LogViewOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("LogView", id, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, 0.0f));
}

void LogView(const char* id, int, LogGetter, void*, const LogViewOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("LogView", id, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, 0.0f));
}

} // namespace facet