// CheckboxTree (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h until
// Phase 2 implements the widget. Returns false and never writes `states`.

#include "facet/core/scale.h"
#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool CheckboxTree(const char* id, std::span<CheckState>, std::span<const CheckTreeItem>,
                  const CheckboxTreeOptions& options)
{
    const float height = options.height > 0.0f
                             ? Px(options.height)
                             : static_cast<float>(internal::StubListRows()) * ControlHeight(options.size);
    internal::StubItem("CheckboxTree", id, internal::StubWidth(options.width, true), height);
    return false;
}

} // namespace facet
