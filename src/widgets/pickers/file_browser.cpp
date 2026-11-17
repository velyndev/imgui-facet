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

} // namespace

bool FileBrowser(const char* label, std::string*, const FileBrowserOptions& options)
{
    const float height = options.height > 0.0f ? Px(options.height) : ControlHeight(ControlSize::Inherit) * kBrowserInControls;
    internal::ControlStub("FileBrowser", label, internal::StubWidth(options.width, 0.0f), height);
    return false;
}

} // namespace facet
