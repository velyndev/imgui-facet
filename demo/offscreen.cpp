#include "offscreen.h"

#include <cfloat>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "gallery.h"

namespace facet::demo
{

bool OffscreenTarget::Create(ID3D11Device* device, int targetWidth, int targetHeight)
{
    Release();
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(targetWidth);
    desc.Height = static_cast<UINT>(targetHeight);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = device->CreateRenderTargetView(texture.Get(), nullptr, view.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr = device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf());
    }
    if (FAILED(hr))
    {
        std::fprintf(stderr, "error: cannot create a %dx%d offscreen target (0x%08lX)\n", targetWidth,
                     targetHeight, static_cast<unsigned long>(hr));
        Release();
        return false;
    }
    width = targetWidth;
    height = targetHeight;
    return true;
}

void OffscreenTarget::Release()
{
    staging.Reset();
    view.Reset();
    texture.Reset();
    width = height = 0;
}

void OffscreenTarget::BindAndClear(ID3D11DeviceContext* context, const float clearColor[4])
{
    ID3D11RenderTargetView* views[] = {view.Get()};
    context->OMSetRenderTargets(1, views, nullptr);
    context->ClearRenderTargetView(view.Get(), clearColor);
}

bool OffscreenTarget::Readback(ID3D11DeviceContext* context, std::vector<unsigned char>& rgba)
{
    if (!texture)
        return false;
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        std::fprintf(stderr, "error: cannot map the staging texture (0x%08lX)\n",
                     static_cast<unsigned long>(hr));
        return false;
    }
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    rgba.resize(rowBytes * static_cast<size_t>(height));
    const auto* source = static_cast<const unsigned char*>(mapped.pData);
    for (int y = 0; y < height; ++y)
        std::memcpy(rgba.data() + rowBytes * static_cast<size_t>(y),
                    source + static_cast<size_t>(mapped.RowPitch) * static_cast<size_t>(y), rowBytes);
    context->Unmap(staging.Get(), 0);
    return true;
}

HeadlessSession::~HeadlessSession()
{
    End();
}

bool HeadlessSession::Begin(AppShell& owner, const HeadlessOptions& options)
{
    End();
    shell = &owner;
    healthy = true;
    if (!target.Create(owner.Device(), options.width, options.height))
    {
        healthy = false;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    active = true;
    ConfigureImGuiIo(true);
    ApplyStyle(options.scale);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(options.width), static_cast<float>(options.height));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = kTimeStep;
    // No platform backend: the only input is what a run queues explicitly.
    io.AddFocusEvent(true);
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

    if (!ImGui_ImplDX11_Init(owner.Device(), owner.Context()))
    {
        std::fprintf(stderr, "error: ImGui_ImplDX11_Init failed\n");
        healthy = false;
        return false;
    }
    backendReady = true;

    InitDesc desc;
    desc.device = owner.Device();
    desc.context = owner.Context();
    if (!facet::Init(desc))
    {
        std::fprintf(stderr, "error: facet::Init failed\n");
        healthy = false;
        return false;
    }
    return true;
}

void HeadlessSession::End()
{
    if (!active)
        return;
    facet::Shutdown();
    if (backendReady)
        ImGui_ImplDX11_Shutdown();
    backendReady = false;
    ImGui::DestroyContext();
    active = false;
    target.Release();
    if (shell != nullptr && shell->Context() != nullptr)
    {
        shell->Context()->ClearState();
        shell->Context()->Flush();
    }
}

void HeadlessSession::Frame(const std::function<void()>& ui)
{
    if (!active || !backendReady)
        return;
    // Keep the hidden window's message queue moving so Windows never flags the process as hung.
    shell->PumpMessages();

    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = kTimeStep;
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    facet::NewFrame();
    ui();
    facet::EndFrame();
    ImGui::Render();

    float clear[4];
    ClearColor(clear);
    target.BindAndClear(shell->Context(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool HeadlessSession::Readback(std::vector<unsigned char>& rgba)
{
    if (!active || !target.Readback(shell->Context(), rgba))
    {
        healthy = false;
        return false;
    }
    if (!shell->DeviceHealthy())
    {
        healthy = false;
        return false;
    }
    return true;
}

} // namespace facet::demo
