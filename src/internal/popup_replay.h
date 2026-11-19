#pragma once

// Draw data snapshots for the popup host's close motion (popup_host.cpp). Dear ImGui forgets a
// popup's content the frame it closes, so while a level is shown the host copies what its window
// drew in every frame, and after it closes it appends the copy of the last frame into a window of
// its own while the level fades out. Only the popup host uses this header.
//
// What a snapshot keeps, and why it cannot dangle:
//   - Vertices and indices are copied into buffers the snapshot owns; nothing points into a draw
//     list. The buffers keep their capacity, so a level shown every frame copies without
//     allocating once warm, and a level whose content is larger than kPopupReplayMaxVertices
//     vertices or kPopupReplayMaxCommands commands keeps no snapshot (it closes without motion).
//   - Draw callbacks are left out (a callback acts on state of the frame it was recorded in).
//   - A texture owned by Dear ImGui (the font atlas, ImTextureRef::_TexData) is kept as a pointer
//     with its unique id and size; before every replay the snapshot checks that the texture is
//     still in the platform texture list, unchanged and not being destroyed. When it is not, the
//     replay stops (the level disappears at once) instead of drawing with stale coordinates.
//   - Any other texture id is a Direct3D 11 shader resource view (the convention of the Dear
//     ImGui DX11 renderer and of every Facet texture). The snapshot holds a reference on each
//     view it uses, so an image released by the application while the level fades stays alive
//     until the snapshot lets it go. Release drops the references (the host calls it when the
//     motion ends, on device invalidation, on scale or theme changes and at Shutdown).
//   - No font, window or draw list pointer is kept.

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

namespace facet::internal
{

inline constexpr int kPopupReplayMaxVertices = 1 << 17;
inline constexpr int kPopupReplayMaxCommands = 1024;
inline constexpr int kPopupReplayMaxTextures = 16;

class PopupSnapshot
{
public:
    PopupSnapshot() = default;
    ~PopupSnapshot();
    PopupSnapshot(const PopupSnapshot&) = delete;
    PopupSnapshot& operator=(const PopupSnapshot&) = delete;

    // Replaces the content with what `root` drew this frame from command `rootCommandStart` on,
    // followed by the draw lists of its visible child windows. Returns false (and holds nothing)
    // when the content is too large or a texture could not be referenced.
    bool Capture(ImGuiWindow* root, int rootCommandStart);

    // True while the snapshot holds content whose textures are all still valid.
    bool Valid() const;

    // Appends the content to `drawList`, every clip rectangle intersected with `clip`.
    void Replay(ImDrawList* drawList, const ImVec4& clip) const;

    // Drops the content and every texture reference; keeps the buffer capacity.
    void Clear();
    // Clear, and frees the buffers.
    void Release();

    int VertexCapacity() const { return vertices_.Capacity; }
    int TextureReferences() const { return viewCount_; }

private:
    struct Command
    {
        ImVec4 clip;
        ImTextureData* texData = nullptr;  // Dear ImGui's texture, or null for a view
        int texUniqueId = 0;
        int texWidth = 0;
        int texHeight = 0;
        std::uintptr_t view = 0;           // a shader resource view (referenced below)
        int vertexStart = 0;
        int vertexCount = 0;
        int indexStart = 0;
        int indexCount = 0;
    };

    bool CaptureList(const ImDrawList* list, int commandStart);
    bool ReferenceView(std::uintptr_t view, std::uintptr_t* nextViews, int* nextCount);
    void SetViews(const std::uintptr_t* views, int count);

    ImVector<ImDrawVert> vertices_;
    ImVector<ImDrawIdx> indices_;
    ImVector<Command> commands_;
    std::uintptr_t views_[kPopupReplayMaxTextures] = {};
    int viewCount_ = 0;
    bool valid_ = false;
};

// Moves, scales and fades the vertices [vertexBegin, vertexEnd) of `drawList`: each position p
// becomes pivot + (p - pivot) * scale + translate, each alpha is multiplied by `alpha`. The clip
// rectangles of the commands [commandBegin, commandEnd) are transformed the same way and kept
// inside `clipBound`.
void PopupTransformVertices(ImDrawList* drawList, int vertexBegin, int vertexEnd, float alpha, float scale,
                            ImVec2 pivot, ImVec2 translate);
void PopupTransformClipRects(ImDrawList* drawList, int commandBegin, int commandEnd, float scale, ImVec2 pivot,
                             ImVec2 translate, const ImVec4& clipBound);

} // namespace facet::internal
