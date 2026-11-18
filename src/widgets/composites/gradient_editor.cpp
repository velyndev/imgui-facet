// GradientEditor (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool GradientEditor(const char* label, std::span<GradientStop>, int*, const GradientEditorOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("GradientEditor", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(stub::kTallControlRows)));
    return false;
}

bool GradientEditor(const char* label, std::vector<GradientStop>*, const GradientEditorOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("GradientEditor", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(stub::kTallControlRows)));
    return false;
}

} // namespace facet