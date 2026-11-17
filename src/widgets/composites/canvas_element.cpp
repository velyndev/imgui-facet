// CanvasElement (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool CanvasElement(const char* label, CanvasElementState*, const CanvasElementOptions&)
{
    namespace stub = internal::content;
    stub::Stub("CanvasElement", label,
               stub::ContentWidth("CanvasElement", stub::TextWidth(label, TextStyle::Footnote, stub::Spans(stub::kSlotSpans))),
               stub::ControlRows(1.0f));
    return false;
}

} // namespace facet