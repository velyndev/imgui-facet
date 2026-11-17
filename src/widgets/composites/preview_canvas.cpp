// BeginPreviewCanvas and EndPreviewCanvas (facet/widgets/composites.h): planned. A placeholder
// until Phase 2 implements them; the canvas elements follow the canvas placeholder.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

void BeginPreviewCanvas(const char* id, ImVec2 referenceSize, const PreviewCanvasOptions& options)
{
    namespace stub = internal::content;
    const float width = stub::ResolveWidth(options.width, 0.0f);
    const float aspect = referenceSize.x > 0.0f && referenceSize.y > 0.0f ? referenceSize.y / referenceSize.x
                                                                          : stub::kWideAspect;
    stub::StubBegin("BeginPreviewCanvas", "PreviewCanvas", id, width, stub::ResolveHeight(options.height, width * aspect));
}

void EndPreviewCanvas()
{
    internal::content::StubEnd("BeginPreviewCanvas");
}

} // namespace facet