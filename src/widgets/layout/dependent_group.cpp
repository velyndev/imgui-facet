// BeginDependentGroup and EndDependentGroup (layout.h): settings that only apply while a parent
// setting is on.
//
// The content is indented by spacing.lg (with `indent`) and a hairline in the separator color runs
// down the middle of the indent, along the content, so it reads as belonging to the setting above.
// While the parent is off the content is disabled through ImGui::BeginDisabled: Facet widgets draw
// their disabled look, show the not-allowed cursor and drop out of the Tab order, so the keyboard
// goes from the parent straight past the group. In Hide mode the content also collapses: the
// collapse region of the containers family (widgets/containers/container_layout.h) animates its
// height to 0 over the animation store (at once with reduced motion), clips it meanwhile, keeps the
// closing items out of the keyboard focus, and BeginDependentGroup returns false once it is closed.
// The animated height is kept under the id.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "widgets/containers/container_layout.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

struct DependentGroupEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    bool hide = false;
    bool collapse = false;  // a collapse region is open
    float indent = 0.0f;
    ImVec2 start = {0.0f, 0.0f};
    internal::ContainerCollapse region;
};

struct DependentGroupStack
{
    DependentGroupEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

DependentGroupStack& Groups()
{
    static DependentGroupStack stack;
    return stack;
}

} // namespace

bool BeginDependentGroup(const char* id, bool enabled, const DependentGroupOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    DependentGroupStack& stack = Groups();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        // Past the depth the scope stack tracks: the content is only disabled.
        internal::PushScope("BeginDependentGroup");
        ++stack.overflow;
        ImGui::BeginDisabled(!enabled);
        return true;
    }
    const bool hide = options.mode == DependentMode::Hide;
    internal::ContainerCollapse region;
    if (hide && !internal::BeginContainerCollapse(window->GetID(id), enabled, &region))
        return false;

    internal::PushScope("BeginDependentGroup");
    DependentGroupEntry& entry = stack.entries[stack.depth++];
    entry = DependentGroupEntry{};
    entry.active = true;
    entry.window = window;
    entry.hide = hide;
    entry.collapse = hide;
    entry.region = region;
    entry.indent = options.indent ? Px(T().spacing.lg) : 0.0f;
    ImGui::BeginDisabled(!enabled);
    ImGui::BeginGroup();
    entry.start = window->DC.CursorPos;
    if (entry.indent > 0.0f)
        ImGui::Indent(entry.indent);
    return true;
}

void EndDependentGroup()
{
    if (!internal::PopScope("BeginDependentGroup"))
        return;
    DependentGroupStack& stack = Groups();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        ImGui::EndDisabled();
        return;
    }
    if (stack.depth == 0)
        return;
    const DependentGroupEntry entry = stack.entries[--stack.depth];
    if (!entry.active || GImGui->CurrentWindow != entry.window)
        return;  // the caller ended the window inside the group; Dear ImGui reports that itself
    if (entry.indent > 0.0f)
        ImGui::Unindent(entry.indent);
    ImGui::EndGroup();
    const ImRect content = GImGui->LastItemData.Rect;
    ImGui::EndDisabled();
    if (entry.collapse)
        internal::EndContainerCollapse(entry.region);

    // The guide down the middle of the indent, as long as the content shows.
    if (entry.indent > 0.0f && content.GetHeight() > 0.0f)
    {
        float bottom = content.Max.y;
        if (entry.collapse)
            bottom = std::min(bottom, entry.window->DC.CursorPos.y);
        const float x = std::round(entry.start.x + entry.indent * 0.5f - HairlineThickness() * 0.5f);
        if (bottom > content.Min.y)
            HairlineV(entry.window->DrawList, x, content.Min.y, bottom, ToU32(T().colors.separator));
    }
}

namespace internal
{

void ClearDependentGroupStack()
{
    DependentGroupStack& stack = Groups();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
