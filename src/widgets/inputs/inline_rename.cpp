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

void Stub(const char* id, float textWidth, const InlineRenameOptions& options)
{
    // While not editing the widget is as wide as its text.
    const float content = textWidth + 2.0f * Px(T().spacing.sm);
    internal::ControlStub("InlineRename", id, internal::StubWidth(options.width, content),
                          internal::StubHeight(options.size, options.error, nullptr));
}

} // namespace

bool InlineRename(const char* id, std::string* text, bool*, const InlineRenameOptions& options)
{
    Stub(id, text != nullptr ? internal::StubTextWidth(text->data(), text->size(), options.size) : 0.0f, options);
    return false;
}

bool InlineRename(const char* id, char* buffer, std::size_t bufferSize, bool*, const InlineRenameOptions& options)
{
    // The buffer may lack a terminator: never read past bufferSize.
    Stub(id, internal::StubTextWidth(buffer, bufferSize, options.size), options);
    return false;
}

} // namespace facet
