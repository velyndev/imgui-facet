// ListBox (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h until
// Phase 2 implements the widget. Returns false and never writes *index.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawListBox(const char* label, const ListBoxOptions& options)
{
    const int rows = options.visibleRows > 0 ? options.visibleRows : internal::StubListRows();
    internal::StubItem("ListBox", label, internal::StubWidth(options.width, true),
                       static_cast<float>(rows) * ControlHeight(options.size));
}

} // namespace

bool ListBox(const char* label, int*, std::span<const ChoiceItem>, const ListBoxOptions& options)
{
    DrawListBox(label, options);
    return false;
}

bool ListBox(const char* label, int*, std::span<const char* const>, const ListBoxOptions& options)
{
    DrawListBox(label, options);
    return false;
}

bool ListBox(const char* label, int*, int, ChoiceGetter, void*, const ListBoxOptions& options)
{
    DrawListBox(label, options);
    return false;
}

} // namespace facet
