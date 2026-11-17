// CodeView (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

bool CodeView(const char* id, std::string_view, const CodeViewOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("CodeView", id, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::Spans(stub::kPanelHeightSpans)));
    return false;
}

} // namespace facet