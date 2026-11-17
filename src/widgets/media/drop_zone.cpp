// DropZone (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it. The
// placeholder accepts no payload.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{

DropZoneResult DropZone(const char* label, std::span<const char* const>, const DropZoneOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("DropZone", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(stub::kZoneControlRows)));
    return DropZoneResult::None;
}

const ImGuiPayload* DropZonePayload()
{
    return nullptr;
}

} // namespace facet