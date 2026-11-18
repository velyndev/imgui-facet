// SectionHeader (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

bool SectionHeader(const char* label, const SectionHeaderOptions& options)
{
    namespace stub = internal::content;
    float height = stub::LineHeight(TextStyle::Title3) + Px(T().spacing.sm);
    if (options.description != nullptr)
        height += stub::LineHeight(TextStyle::Footnote);
    stub::Stub("SectionHeader", label, stub::ResolveWidth(options.width, 0.0f), height);
    return false;
}

} // namespace facet