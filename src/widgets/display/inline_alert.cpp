// InlineAlert (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

InlineAlertAction InlineAlert(const char* label, bool* open, const char*, const InlineAlertOptions& options)
{
    namespace stub = internal::content;
    if (open != nullptr && !*open)
        return InlineAlertAction::None;  // a dismissed alert takes no space
    stub::Stub("InlineAlert", label, stub::ResolveWidth(options.width, 0.0f), stub::ControlRows(stub::kTallControlRows));
    return InlineAlertAction::None;
}

} // namespace facet