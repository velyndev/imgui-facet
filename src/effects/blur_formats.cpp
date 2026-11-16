#include "effects/blur_formats.h"

namespace facet::internal
{
namespace
{

struct FormatRow
{
    DXGI_FORMAT typed;
    DXGI_FORMAT typeless;
    bool srgb;
};

// Color formats that can be rendered to and sampled with filtering.
constexpr FormatRow kFormats[] = {
    {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_TYPELESS, false},
    {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_TYPELESS, true},
    {DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_TYPELESS, false},
    {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_TYPELESS, true},
    {DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_TYPELESS, false},
    {DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, DXGI_FORMAT_B8G8R8X8_TYPELESS, true},
    {DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_TYPELESS, false},
    {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_TYPELESS, false},
    {DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_TYPELESS, false},
    {DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_TYPELESS, false},
    {DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT, false},
};

const FormatRow* FindTyped(DXGI_FORMAT format)
{
    for (const FormatRow& row : kFormats)
    {
        if (row.typed == format)
            return &row;
    }
    return nullptr;
}

} // namespace

DXGI_FORMAT TypelessFormat(DXGI_FORMAT format)
{
    for (const FormatRow& row : kFormats)
    {
        if (row.typed == format || row.typeless == format)
            return row.typeless;
    }
    return format;
}

CaptureFormat ChooseCaptureFormat(DXGI_FORMAT resourceFormat, DXGI_FORMAT viewFormat)
{
    CaptureFormat result;
    const DXGI_FORMAT typed = viewFormat != DXGI_FORMAT_UNKNOWN ? viewFormat : resourceFormat;
    const FormatRow* row = FindTyped(typed);
    if (row == nullptr)
        return result;
    // The view must belong to the texture's format group.
    if (TypelessFormat(resourceFormat) != row->typeless)
        return result;
    result.supported = true;
    result.copyFormat = row->typeless;
    result.viewFormat = row->typed;
    result.resolveFormat = row->typed;
    result.srgb = row->srgb;
    return result;
}

} // namespace facet::internal
