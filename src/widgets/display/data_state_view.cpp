// BeginDataStateView, EndDataStateView and DataStateViewRetryPressed (facet/widgets/display.h):
// one wrapper for a data view that draws skeleton rows while Loading, the error state with an
// optional retry button, the empty state (Ready with a count of 0) or opens a group for the
// caller's content. The height of the last Ready frame is kept under the id, so the layout does not
// jump when the state changes, and a state that appears fades in over motion.durationFast.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/loading.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

constexpr const char* kDataStateScope = "BeginDataStateView";
// Animation sub key of the fade of a newly shown state under the view id.
constexpr int kDataStateAnimFade = 1;
// The share of a skeleton row its bar fills (about a line of text), and the shorter last bar.
constexpr float kDataStateBarShare = 0.35f;
constexpr float kDataStateLastBarShare = 0.6f;
// Views opened at once (a view inside the content of another).
constexpr int kDataStateDepth = 8;
// The height of the empty and error states before any Ready frame, in control heights: room for the
// icon, the title and a button.
constexpr float kDataStateEmptyRows = 4.0f;

struct DataStateViewState
{
    float readyHeight = 0.0f;  // pixels at the last Ready frame with content; 0 before one
    float readyScale = 0.0f;   // Scale() then, so a scale change measures afresh
    int shownState = -1;       // the state drawn last frame: Ready content 0, Loading 1, Error 2, empty 3
};

// The views whose content is open, innermost last.
struct DataStateStack
{
    ImGuiID ids[kDataStateDepth] = {};
    int depth = 0;
};

DataStateStack& OpenViews()
{
    static DataStateStack stack;
    return stack;
}

// Scales the alpha of the vertices drawList received from irst on: the fade of a state that
// just appeared, buttons included.
void DataStateFade(ImDrawList* drawList, int first, float amount)
{
    for (int i = first; i < drawList->VtxBuffer.Size; ++i)
    {
        ImU32& color = drawList->VtxBuffer[i].col;
        const ImU32 alpha = static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFu) * amount + 0.5f);
        color = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
    }
}

bool& RetryPressed()
{
    static bool pressed = false;
    return pressed;
}

} // namespace

bool BeginDataStateView(const char* id, DataState state, int count, const DataStateViewOptions& options)
{
    RetryPressed() = false;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const ImGuiID viewId = window->GetID(id);
    DataStateViewState& view = internal::GetState<DataStateViewState>(viewId);
    if (view.readyScale != Scale())
        view.readyHeight = 0.0f;
    const bool content = state == DataState::Ready && count > 0;
    const int shown = content ? 0 : (state == DataState::Loading ? 1 : (state == DataState::Error ? 2 : 3));
    const bool changed = view.shownState != shown;
    view.shownState = shown;

    if (content)
    {
        DataStateStack& stack = OpenViews();
        if (stack.depth >= kDataStateDepth)
            return false;
        stack.ids[stack.depth] = viewId;
        ++stack.depth;
        internal::PushScope(kDataStateScope);
        internal::ApplyNextItemData(next);
        ImGui::BeginGroup();
        return true;
    }

    // A state the view draws itself: as tall as options.height, else as the last Ready frame, else
    // as the state needs.
    const float rowHeight = internal::ListRowHeight(ControlSize::Inherit);
    float height = options.height > 0.0f ? Px(options.height) : view.readyHeight;
    if (height <= 0.0f)
    {
        if (state == DataState::Loading)
            height = rowHeight * static_cast<float>(std::max(1, options.skeletonRows));
        else
            height = ControlHeight(ControlSize::Inherit) * kDataStateEmptyRows;
    }
    const float width = internal::ResolveItemWidth(0.0f, next, kFill);
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + std::round(height)));
    // A state that just appeared fades in; every frame after, it is drawn at full strength.
    float fade = 1.0f;
    if (changed)
        Tween(viewId, kDataStateAnimFade, 0.0f, 0.0f);
    fade = Tween(viewId, kDataStateAnimFade, 1.0f, t.motion.durationFast);
    const ImVec2 savedMax = window->DC.CursorMaxPos;
    if (ImGui::IsRectVisible(bounds.Min, bounds.Max))
    {
        ImGui::PushOverrideID(viewId);
        const int firstVertex = window->DrawList->VtxBuffer.Size;
        if (state == DataState::Loading)
        {
            // Skeleton rows in the list row height, the last one shorter.
            const int rows = std::max(1, options.skeletonRows);
            const float rowStep = std::min(rowHeight, bounds.GetHeight() / static_cast<float>(rows));
            const float bar = std::max(1.0f, std::round(rowStep * kDataStateBarShare));
            for (int i = 0; i < rows; ++i)
            {
                const float top = std::round(origin.y + rowStep * static_cast<float>(i) + (rowStep - bar) * 0.5f);
                const float right = i == rows - 1 && rows > 1 ? origin.x + std::round(width * kDataStateLastBarShare)
                                                              : bounds.Max.x;
                internal::DrawSkeleton(window->DrawList, ImVec2(origin.x, top), ImVec2(right, top + bar),
                                       std::min(Px(t.radii.element), bar * 0.5f));
            }
        }
        else
        {
            const Strings& strings = GetStrings();
            const bool error = state == DataState::Error;
            const char* text = error ? (options.errorText != nullptr ? options.errorText : strings.loadFailed)
                                     : (options.emptyText != nullptr ? options.emptyText : strings.empty);
            ImGui::SetCursorScreenPos(origin);
            const bool retry = EmptyState(text, {.icon = error ? Icon("error") : Icon(),
                                                 .actionLabel = error ? options.retryLabel : nullptr,
                                                 .tone = error ? Tone::Destructive : Tone::Neutral,
                                                 // Lower than the full state needs: one line.
                                                 .compact = bounds.GetHeight() < ControlHeight(ControlSize::Inherit) * kDataStateEmptyRows,
                                                 .width = width / Scale(),
                                                 .height = bounds.GetHeight() / Scale()});
            RetryPressed() = error && retry;
        }
        if (fade < 1.0f)
            DataStateFade(window->DrawList, firstVertex, fade);
        ImGui::PopID();
    }
    window->DC.CursorMaxPos = savedMax;
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(bounds.GetSize());
    ImGui::ItemAdd(bounds, viewId, nullptr, ImGuiItemFlags_NoNav);
    return false;
}

void EndDataStateView()
{
    if (!internal::PopScope(kDataStateScope))
        return;
    DataStateStack& stack = OpenViews();
    if (stack.depth <= 0)
        return;
    --stack.depth;
    ImGui::EndGroup();
    const ImGuiID viewId = stack.ids[stack.depth];
    DataStateViewState& view = internal::GetState<DataStateViewState>(viewId);
    view.readyHeight = ImGui::GetItemRectSize().y;
    view.readyScale = Scale();
}

bool DataStateViewRetryPressed()
{
    return RetryPressed();
}

} // namespace facet
