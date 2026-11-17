// TransferList (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h until
// Phase 2 implements the widget. Returns false and never writes `inTarget`.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawTransferList(const char* id, const TransferListOptions& options)
{
    const int rows = options.visibleRows > 0 ? options.visibleRows : internal::kStubListRows;
    internal::StubItem("TransferList", id, internal::StubWidth(options.width, true),
                       static_cast<float>(rows) * ControlHeight(ControlSize::Inherit));
}

} // namespace

bool TransferList(const char* id, std::span<bool>, std::span<const ChoiceItem>, const TransferListOptions& options)
{
    DrawTransferList(id, options);
    return false;
}

bool TransferList(const char* id, std::span<bool>, std::span<const char* const>, const TransferListOptions& options)
{
    DrawTransferList(id, options);
    return false;
}

} // namespace facet
