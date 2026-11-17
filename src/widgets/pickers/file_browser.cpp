// FileBrowser: planned. Draws the placeholder at the browser's default size until Phase 2
// implements the widget in this file on the list and selection core.

#include "facet/widgets/pickers.h"

#include "facet/core/scale.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Default height in control heights until the theme has a token for it.
constexpr float kBrowserInControls = 10.0f;

void Stub(const char* label, const FileBrowserOptions& options)
{
    const float height = options.height > 0.0f ? Px(options.height) : ControlHeight(ControlSize::Inherit) * kBrowserInControls;
    internal::ControlStub("FileBrowser", label, internal::StubWidth(options.width, 0.0f), height);
}

} // namespace

bool FileBrowser(const char* label, std::string*, const FileBrowserOptions& options)
{
    Stub(label, options);
    return false;
}

bool FileBrowser(const char* label, char*, std::size_t, const FileBrowserOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
