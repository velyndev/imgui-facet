// Skeleton (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void Skeleton(const SkeletonOptions& options)
{
    namespace stub = internal::content;
    if (options.shape == SkeletonShape::Text)
    {
        const float lines = static_cast<float>(std::max(1, options.lines));
        const float height = stub::ResolveHeight(options.height, stub::LineHeight(TextStyle::Body) * lines);
        stub::Stub("Skeleton", nullptr, stub::ResolveWidth(options.width, 0.0f), height);
        return;
    }
    const float height = stub::ResolveHeight(options.height, stub::ControlRows(1.0f));
    const bool circle = options.shape == SkeletonShape::Circle && options.width == 0.0f;
    stub::Stub("Skeleton", nullptr, circle ? height : stub::ResolveWidth(options.width, 0.0f), height);
}

} // namespace facet