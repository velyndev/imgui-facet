// SearchableChecklist (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. Returns false and never writes `checked`.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawSearchableChecklist(const char* label, const SearchableChecklistOptions& options)
{
    // The filter field plus the list rows.
    const int rows = 1 + (options.visibleRows > 0 ? options.visibleRows : internal::kStubListRows);
    internal::StubItem("SearchableChecklist", label, internal::StubWidth(options.width, true),
                       static_cast<float>(rows) * ControlHeight(ControlSize::Inherit));
}

} // namespace

bool SearchableChecklist(const char* label, std::span<bool>, std::span<const ChoiceItem>,
                         const SearchableChecklistOptions& options)
{
    DrawSearchableChecklist(label, options);
    return false;
}

bool SearchableChecklist(const char* label, std::span<bool>, std::span<const char* const>,
                         const SearchableChecklistOptions& options)
{
    DrawSearchableChecklist(label, options);
    return false;
}

bool SearchableChecklist(const char* label, std::span<bool>, int, ChoiceGetter, void*,
                         const SearchableChecklistOptions& options)
{
    DrawSearchableChecklist(label, options);
    return false;
}

} // namespace facet
