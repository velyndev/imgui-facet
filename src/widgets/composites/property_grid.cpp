// The property grid, its categories and rows (facet/widgets/composites.h): planned. A
// placeholder until Phase 2 implements them. Categories that start open and every row return
// true, so the caller's controls follow their placeholders.

#include <cstring>

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

void BeginPropertyGrid(const char* id, const PropertyGridOptions&)
{
    namespace stub = internal::content;
    stub::StubBegin("BeginPropertyGrid", "PropertyGrid", id, stub::ResolveWidth(0.0f, 0.0f), stub::ControlRows(1.0f));
}

void EndPropertyGrid()
{
    internal::content::StubEnd("BeginPropertyGrid");
}

bool BeginPropertyCategory(const char* label, const PropertyCategoryOptions& options)
{
    namespace stub = internal::content;
    if (!stub::CanSubmit())
        return false;
    if (!options.defaultOpen)
    {
        stub::Stub("PropertyCategory", label, stub::ResolveWidth(0.0f, 0.0f), stub::ControlRows(1.0f));
        return false;
    }
    stub::StubBegin("BeginPropertyCategory", "PropertyCategory", label, stub::ResolveWidth(0.0f, 0.0f),
                    stub::ControlRows(1.0f));
    return true;
}

void EndPropertyCategory()
{
    internal::content::StubEnd("BeginPropertyCategory");
}

bool BeginPropertyRow(const char* label, const PropertyRowOptions&)
{
    namespace stub = internal::content;
    if (!stub::CanSubmit())
        return false;
    stub::StubBegin("BeginPropertyRow", "PropertyRow", label, stub::ResolveWidth(0.0f, 0.0f), stub::ControlRows(1.0f));
    ImGui::PushID(label != nullptr ? label : "");
    return true;
}

void EndPropertyRow()
{
    // The label is on the id stack only when BeginPropertyRow ran and is the innermost scope.
    const char* inner = internal::InnermostScope();
    if (internal::content::CanSubmit() && inner != nullptr && std::strcmp(inner, "BeginPropertyRow") == 0)
        ImGui::PopID();
    internal::content::StubEnd("BeginPropertyRow");
}

} // namespace facet