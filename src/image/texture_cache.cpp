// Image handles and the texture cache.
//
// The cache does not exist yet: every load returns an invalid handle, queries report a
// failed image, Image() reserves its layout space without drawing, and DrawImage draws
// nothing.

#include "facet/core/image.h"

#include "internal/subsystem.h"

namespace facet
{

ImageHandle LoadImageFile(const char*)
{
    return {};
}

ImageHandle LoadImageMemory(const void*, std::size_t)
{
    return {};
}

ImageHandle LoadImageFileAsync(const char*)
{
    return {};
}

ImageHandle CreateImageRGBA(const void*, int, int, int)
{
    return {};
}

void ReleaseImage(ImageHandle)
{
}

bool IsImageReady(ImageHandle)
{
    return false;
}

bool ImageFailed(ImageHandle)
{
    return true;
}

ImVec2 ImageSize(ImageHandle)
{
    return ImVec2(0.0f, 0.0f);
}

ImTextureRef ImageTexture(ImageHandle)
{
    return ImTextureRef();
}

void Image(ImageHandle, ImVec2 size, const ImageOptions&)
{
    ImGui::Dummy(size);
}

void Image(ID3D11ShaderResourceView*, ImVec2 size, const ImageOptions&)
{
    ImGui::Dummy(size);
}

void DrawImage(ImDrawList*, ImageHandle, ImVec2, ImVec2, const ImageOptions&)
{
}

void DrawImage(ImDrawList*, ID3D11ShaderResourceView*, ImVec2, ImVec2, const ImageOptions&)
{
}

namespace internal
{

// No hooks yet: the cache holds no textures.
Subsystem TextureSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "texture";
    return subsystem;
}

} // namespace internal

} // namespace facet
