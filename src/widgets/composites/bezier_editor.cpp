// BezierEditor (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool BezierEditor(const char* label, CubicBezier*, const BezierEditorOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("BezierEditor", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::Spans(stub::kChartHeightSpans)));
    return false;
}

} // namespace facet