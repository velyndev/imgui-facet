// Draw data snapshots for the popup host's close motion (popup_replay.h).

#include "internal/popup_replay.h"

#include <algorithm>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>

namespace facet::internal
{
namespace
{

ID3D11ShaderResourceView* AsView(std::uintptr_t view)
{
    return reinterpret_cast<ID3D11ShaderResourceView*>(view);
}

// True while Dear ImGui still lists `texture` with the id and size it had when it was recorded,
// and does not destroy it.
bool TextureStillValid(const ImTextureData* texture, int uniqueId, int width, int height)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const ImVector<ImTextureData*>& textures = ImGui::GetPlatformIO().Textures;
    for (const ImTextureData* candidate : textures)
    {
        if (candidate != texture)
            continue;
        if (candidate->UniqueID != uniqueId || candidate->Width != width || candidate->Height != height)
            return false;
        return candidate->Status == ImTextureStatus_OK || candidate->Status == ImTextureStatus_WantUpdates;
    }
    return false;
}

ImVec4 IntersectClip(const ImVec4& a, const ImVec4& b)
{
    return ImVec4(std::max(a.x, b.x), std::max(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));
}

} // namespace

PopupSnapshot::~PopupSnapshot()
{
    SetViews(nullptr, 0);
}

void PopupSnapshot::SetViews(const std::uintptr_t* views, int count)
{
    // Reference the new set before releasing the old one, so a view in both never drops to 0.
    for (int i = 0; i < count; ++i)
        AsView(views[i])->AddRef();
    for (int i = 0; i < viewCount_; ++i)
        AsView(views_[i])->Release();
    for (int i = 0; i < count; ++i)
        views_[i] = views[i];
    viewCount_ = count;
}

bool PopupSnapshot::ReferenceView(std::uintptr_t view, std::uintptr_t* nextViews, int* nextCount)
{
    for (int i = 0; i < *nextCount; ++i)
    {
        if (nextViews[i] == view)
            return true;
    }
    if (*nextCount >= kPopupReplayMaxTextures)
        return false;
    nextViews[(*nextCount)++] = view;
    return true;
}

bool PopupSnapshot::CaptureList(const ImDrawList* list, int commandStart)
{
    if (list == nullptr)
        return true;
    for (int c = std::max(0, commandStart); c < list->CmdBuffer.Size; ++c)
    {
        const ImDrawCmd& cmd = list->CmdBuffer[c];
        if (cmd.UserCallback != nullptr || cmd.ElemCount == 0)
            continue;
        if (commands_.Size >= kPopupReplayMaxCommands)
            return false;
        const unsigned int indexEnd = cmd.IdxOffset + cmd.ElemCount;
        if (indexEnd > static_cast<unsigned int>(list->IdxBuffer.Size))
            return false;
        // The vertices this command uses: its indices are relative to VtxOffset and cover one
        // contiguous range, because the draw list adds the vertices of a command in order.
        unsigned int lo = 0xFFFFFFFFu;
        unsigned int hi = 0;
        for (unsigned int i = cmd.IdxOffset; i < indexEnd; ++i)
        {
            const unsigned int index = list->IdxBuffer.Data[i];
            lo = std::min(lo, index);
            hi = std::max(hi, index);
        }
        const unsigned int vertexCount = hi - lo + 1;
        if (cmd.VtxOffset + hi >= static_cast<unsigned int>(list->VtxBuffer.Size) || vertexCount > 0xFFFFu ||
            vertices_.Size + static_cast<int>(vertexCount) > kPopupReplayMaxVertices)
            return false;

        Command out;
        out.clip = cmd.ClipRect;
        out.texData = cmd.TexRef._TexData;
        if (out.texData != nullptr)
        {
            out.texUniqueId = out.texData->UniqueID;
            out.texWidth = out.texData->Width;
            out.texHeight = out.texData->Height;
        }
        else
        {
            out.view = static_cast<std::uintptr_t>(cmd.TexRef._TexID);
            if (out.view == 0)
                continue;
        }
        out.vertexStart = vertices_.Size;
        out.vertexCount = static_cast<int>(vertexCount);
        out.indexStart = indices_.Size;
        out.indexCount = static_cast<int>(cmd.ElemCount);

        const int vertexBase = vertices_.Size;
        vertices_.resize(vertexBase + static_cast<int>(vertexCount));
        std::memcpy(vertices_.Data + vertexBase, list->VtxBuffer.Data + cmd.VtxOffset + lo,
                    sizeof(ImDrawVert) * vertexCount);
        const int indexBase = indices_.Size;
        indices_.resize(indexBase + static_cast<int>(cmd.ElemCount));
        for (unsigned int i = 0; i < cmd.ElemCount; ++i)
            indices_.Data[indexBase + static_cast<int>(i)] = static_cast<ImDrawIdx>(list->IdxBuffer.Data[cmd.IdxOffset + i] - lo);
        commands_.push_back(out);
    }
    return true;
}

bool PopupSnapshot::Capture(ImGuiWindow* root, int rootCommandStart)
{
    vertices_.resize(0);
    indices_.resize(0);
    commands_.resize(0);
    valid_ = false;
    if (root == nullptr)
    {
        Clear();
        return false;
    }

    bool ok = CaptureList(root->DrawList, rootCommandStart);
    // Visible child windows, in the order Dear ImGui renders them (depth first).
    ImGuiWindow* stack[32];
    int depth = 0;
    for (int i = root->DC.ChildWindows.Size - 1; ok && i >= 0 && depth < IM_COUNTOF(stack); --i)
        stack[depth++] = root->DC.ChildWindows[i];
    while (ok && depth > 0)
    {
        ImGuiWindow* child = stack[--depth];
        if (!child->Active || child->Hidden || (child->Flags & ImGuiWindowFlags_Popup) != 0)
            continue;
        ok = CaptureList(child->DrawList, 0);
        for (int i = child->DC.ChildWindows.Size - 1; ok && i >= 0 && depth < IM_COUNTOF(stack); --i)
            stack[depth++] = child->DC.ChildWindows[i];
    }

    // Reference the views of this frame and let go of the ones no longer used.
    std::uintptr_t next[kPopupReplayMaxTextures] = {};
    int nextCount = 0;
    for (int c = 0; ok && c < commands_.Size; ++c)
    {
        if (commands_[c].view != 0)
            ok = ReferenceView(commands_[c].view, next, &nextCount);
    }
    if (!ok)
    {
        Clear();
        return false;
    }
    SetViews(next, nextCount);
    valid_ = true;
    return true;
}

bool PopupSnapshot::Valid() const
{
    if (!valid_)
        return false;
    for (const Command& cmd : commands_)
    {
        if (cmd.texData != nullptr && !TextureStillValid(cmd.texData, cmd.texUniqueId, cmd.texWidth, cmd.texHeight))
            return false;
    }
    return true;
}

void PopupSnapshot::Replay(ImDrawList* drawList, const ImVec4& clip) const
{
    if (drawList == nullptr || !valid_)
        return;
    for (const Command& cmd : commands_)
    {
        const ImVec4 rect = IntersectClip(cmd.clip, clip);
        if (rect.z <= rect.x || rect.w <= rect.y)
            continue;
        drawList->PushClipRect(ImVec2(rect.x, rect.y), ImVec2(rect.z, rect.w), false);
        ImTextureRef texture;
        if (cmd.texData != nullptr)
            texture._TexData = cmd.texData;
        else
            texture._TexID = static_cast<ImTextureID>(cmd.view);
        drawList->PushTexture(texture);
        drawList->PrimReserve(cmd.indexCount, cmd.vertexCount);
        const unsigned int base = drawList->_VtxCurrentIdx;
        std::memcpy(drawList->_VtxWritePtr, vertices_.Data + cmd.vertexStart, sizeof(ImDrawVert) * static_cast<size_t>(cmd.vertexCount));
        drawList->_VtxWritePtr += cmd.vertexCount;
        for (int i = 0; i < cmd.indexCount; ++i)
            drawList->_IdxWritePtr[i] = static_cast<ImDrawIdx>(base + indices_.Data[cmd.indexStart + i]);
        drawList->_IdxWritePtr += cmd.indexCount;
        drawList->_VtxCurrentIdx += static_cast<unsigned int>(cmd.vertexCount);
        drawList->PopTexture();
        drawList->PopClipRect();
    }
}

void PopupSnapshot::Clear()
{
    vertices_.resize(0);
    indices_.resize(0);
    commands_.resize(0);
    SetViews(nullptr, 0);
    valid_ = false;
}

void PopupSnapshot::Release()
{
    Clear();
    vertices_.clear();
    indices_.clear();
    commands_.clear();
}

void PopupTransformVertices(ImDrawList* drawList, int vertexBegin, int vertexEnd, float alpha, float scale,
                            ImVec2 pivot, ImVec2 translate)
{
    if (drawList == nullptr)
        return;
    vertexBegin = std::max(0, vertexBegin);
    vertexEnd = std::min(vertexEnd, drawList->VtxBuffer.Size);
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const bool move = scale != 1.0f || translate.x != 0.0f || translate.y != 0.0f;
    for (int i = vertexBegin; i < vertexEnd; ++i)
    {
        ImDrawVert& v = drawList->VtxBuffer.Data[i];
        if (move)
        {
            v.pos.x = pivot.x + (v.pos.x - pivot.x) * scale + translate.x;
            v.pos.y = pivot.y + (v.pos.y - pivot.y) * scale + translate.y;
        }
        if (alpha < 1.0f)
        {
            const ImU32 a = (v.col >> IM_COL32_A_SHIFT) & 0xFFu;
            const ImU32 scaled = static_cast<ImU32>(static_cast<float>(a) * alpha + 0.5f);
            v.col = (v.col & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
        }
    }
}

void PopupTransformClipRects(ImDrawList* drawList, int commandBegin, int commandEnd, float scale, ImVec2 pivot,
                             ImVec2 translate, const ImVec4& clipBound)
{
    if (drawList == nullptr)
        return;
    commandBegin = std::max(0, commandBegin);
    commandEnd = std::min(commandEnd, drawList->CmdBuffer.Size);
    for (int c = commandBegin; c < commandEnd; ++c)
    {
        ImVec4& r = drawList->CmdBuffer.Data[c].ClipRect;
        const ImVec4 moved(pivot.x + (r.x - pivot.x) * scale + translate.x, pivot.y + (r.y - pivot.y) * scale + translate.y,
                           pivot.x + (r.z - pivot.x) * scale + translate.x, pivot.y + (r.w - pivot.y) * scale + translate.y);
        r = IntersectClip(moved, clipBound);
        if (r.z < r.x)
            r.z = r.x;
        if (r.w < r.y)
            r.w = r.y;
    }
}

} // namespace facet::internal
