// InlineRename: planned. Draws the placeholder at the text's default size until Phase 2
// implements the widget in this file on the text field core.

#include "facet/widgets/inputs.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* id, const char* text, const InlineRenameOptions& options)
{
    // While not editing the widget is as wide as its text.
    const float content = internal::StubLabelWidth(text, options.size) + 2.0f * Px(T().spacing.sm);
    internal::ControlStub("InlineRename", id, internal::StubWidth(options.width, content),
                          internal::StubHeight(options.size, options.error, nullptr));
}

} // namespace

bool InlineRename(const char* id, std::string* text, bool*, const InlineRenameOptions& options)
{
    Stub(id, text != nullptr ? text->c_str() : nullptr, options);
    return false;
}

bool InlineRename(const char* id, char* buffer, std::size_t bufferSize, bool*, const InlineRenameOptions& options)
{
    Stub(id, buffer != nullptr && bufferSize > 0 ? buffer : nullptr, options);
    return false;
}

} // namespace facet
