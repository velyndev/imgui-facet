#include "gallery.h"

#include <imgui.h>

#include "app_shell.h"

namespace facet::demo
{
namespace
{

// Navigation width in multiples of the font size, so it follows the UI scale.
constexpr float kNavWidthEm = 14.0f;

void DrawTitleStrip(AppShell* shell)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float stripHeight = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;

    ImGui::BeginChild("##title_strip", ImVec2(0.0f, stripHeight), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Facet demo");

    const bool maximized = shell != nullptr && shell->IsWindowMaximized();
    const char* labels[] = {"Minimize", maximized ? "Restore" : "Maximize", "Close"};
    float buttonsWidth = 0.0f;
    for (const char* label : labels)
        buttonsWidth += ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
    buttonsWidth += style.ItemSpacing.x * 2.0f;

    ImGui::SameLine();
    const float freeWidth = ImGui::GetContentRegionAvail().x - buttonsWidth;
    if (freeWidth > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + freeWidth);
    const float buttonsScreenX = ImGui::GetCursorScreenPos().x;

    if (ImGui::Button(labels[0]) && shell != nullptr)
        shell->Minimize();
    ImGui::SameLine();
    if (ImGui::Button(labels[1]) && shell != nullptr)
        shell->ToggleMaximize();
    ImGui::SameLine();
    if (ImGui::Button(labels[2]) && shell != nullptr)
        shell->Close();

    // Everything in the strip left of the buttons drags the window, unless a popup is open
    // or another window (a combo list, a popup, a tooltip that takes input) lies over the
    // strip under the mouse: then clicks there belong to the UI.
    if (shell != nullptr)
    {
        const ImVec2 stripMin = ImGui::GetWindowPos();
        const ImVec2 captionMax(buttonsScreenX - style.ItemSpacing.x, stripMin.y + stripHeight);
        shell->SetCaptionRect(stripMin.x, stripMin.y, captionMax.x, captionMax.y);
        const bool popupOpen =
            ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        const bool coveredUnderMouse =
            ImGui::IsMouseHoveringRect(stripMin, captionMax, false) && !ImGui::IsWindowHovered();
        shell->SetCaptionBlocked(popupOpen || coveredUnderMouse);
    }

    ImGui::EndChild();
}

void DrawNavigation(GalleryState& state)
{
    const std::span<const PageInfo> pages = Pages();
    ImGui::BeginChild("##navigation", ImVec2(ImGui::GetFontSize() * kNavWidthEm, 0.0f),
                      ImGuiChildFlags_Borders);
    const std::string* currentGroup = nullptr;
    for (int i = 0; i < static_cast<int>(pages.size()); ++i)
    {
        const PageInfo& page = pages[static_cast<size_t>(i)];
        if (currentGroup == nullptr || *currentGroup != page.group)
        {
            ImGui::SeparatorText(page.group.c_str());
            currentGroup = &page.group;
        }
        ImGui::PushID(page.id.c_str());
        if (ImGui::Selectable(page.title.c_str(), state.selectedPage == i))
            state.selectedPage = i;
        ImGui::PopID();
    }
    if (pages.empty())
        ImGui::TextDisabled("No pages registered.");
    ImGui::EndChild();
}

void DrawContent(GalleryState& state, PageCtx& ctx)
{
    const std::span<const PageInfo> pages = Pages();
    ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (state.selectedPage >= 0 && state.selectedPage < static_cast<int>(pages.size()))
    {
        const PageInfo& page = pages[static_cast<size_t>(state.selectedPage)];
        ImGui::PushID(page.id.c_str());
        page.draw(ctx);
        ImGui::PopID();
    }
    else
    {
        ImGui::TextDisabled("Select a page on the left.");
    }
    ImGui::EndChild();
}

} // namespace

void ConfigureImGuiIo(bool deterministic)
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (deterministic)
    {
        io.ConfigInputTextCursorBlink = false;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
}

void ApplyStyle(float scale)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
}

void ClearColor(float out[4])
{
    const ImVec4& color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    out[0] = color.x;
    out[1] = color.y;
    out[2] = color.z;
    out[3] = 1.0f;
}

void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##gallery", nullptr, flags);
    ImGui::PopStyleVar(2);

    DrawTitleStrip(shell);
    DrawNavigation(state);
    ImGui::SameLine();
    DrawContent(state, ctx);

    ImGui::End();
}

} // namespace facet::demo
