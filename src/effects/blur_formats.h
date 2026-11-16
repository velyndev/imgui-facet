#pragma once

// Render target formats the backdrop blur can read.
//
// The blur copies (or resolves) the bound render target into a texture of its own and
// samples it. The copy has the typeless format of the target's format group, so the same
// texture works with CopySubresourceRegion and ResolveSubresource, and it is read through a
// view with the typed format of the target's render target view. An sRGB view is read as
// sRGB, so the blur runs on linear values and the renderer encodes them again when it draws
// the result into the sRGB target.

#include <dxgiformat.h>

namespace facet::internal
{

struct CaptureFormat
{
    bool supported = false;
    DXGI_FORMAT copyFormat = DXGI_FORMAT_UNKNOWN;     // texture format of the copy
    DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;     // shader resource view of the copy
    DXGI_FORMAT resolveFormat = DXGI_FORMAT_UNKNOWN;  // format argument of ResolveSubresource
    bool srgb = false;
};

// Chooses the formats for a render target whose texture has `resourceFormat` and whose
// render target view has `viewFormat` (DXGI_FORMAT_UNKNOWN when the view takes the texture
// format). A typeless texture needs a typed view format. Formats that cannot be sampled
// with filtering (integer, depth, video and compressed formats) are not supported.
CaptureFormat ChooseCaptureFormat(DXGI_FORMAT resourceFormat, DXGI_FORMAT viewFormat);

// The typeless member of the format's group, or the format itself when the group has none.
DXGI_FORMAT TypelessFormat(DXGI_FORMAT format);

} // namespace facet::internal
