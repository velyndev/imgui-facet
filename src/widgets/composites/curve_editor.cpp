// CurveEditor (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool CurveEditor(const char* label, std::span<ImVec2>, int*, ImVec2, ImVec2, const CurveEditorOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("CurveEditor", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::Spans(stub::kChartHeightSpans)));
    return false;
}

bool CurveEditor(const char* label, std::vector<ImVec2>*, ImVec2, ImVec2, const CurveEditorOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("CurveEditor", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::Spans(stub::kChartHeightSpans)));
    return false;
}

} // namespace facet