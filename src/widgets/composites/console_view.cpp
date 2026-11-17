// ConsoleView (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

void ConsoleViewStub(const char* id, const ConsoleViewOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("ConsoleView", id, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, 0.0f));
}

} // namespace

bool ConsoleView(const char* id, std::string*, std::span<const LogEntry>, const ConsoleViewOptions& options)
{
    ConsoleViewStub(id, options);
    return false;
}

bool ConsoleView(const char* id, std::string*, int, LogGetter, void*, const ConsoleViewOptions& options)
{
    ConsoleViewStub(id, options);
    return false;
}

bool ConsoleView(const char* id, char*, std::size_t, std::span<const LogEntry>, const ConsoleViewOptions& options)
{
    ConsoleViewStub(id, options);
    return false;
}

bool ConsoleView(const char* id, char*, std::size_t, int, LogGetter, void*, const ConsoleViewOptions& options)
{
    ConsoleViewStub(id, options);
    return false;
}

} // namespace facet