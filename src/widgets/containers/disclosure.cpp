// BeginDisclosure and EndDisclosure (containers.h): an inline toggle that reveals the content
// below it with an animated height. The Row style is a full-width row (chevron, label, the
// summary after the label while closed) whose content lines up with the label; the Link style is
// accent text with a chevron, as wide as its content, whose content starts at the link's edge.
// Click, Enter or Space toggles. The open state is the caller's `open`, or kept under the id.

#include "facet/widgets/containers.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

struct DisclosureEntry
{
    ImGuiWindow* window = nullptr;
    float indent = 0.0f;
    internal::ContainerCollapse collapse;
};

struct DisclosureStack
{
    DisclosureEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

DisclosureStack& Disclosures()
{
    static DisclosureStack stack;
    return stack;
}

} // namespace

bool BeginDisclosure(const char* label, bool* open, const DisclosureOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";

    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const bool link = options.style == DisclosureStyle::Link;
    bool isOpen = open != nullptr ? *open : internal::ContainerOpenState(id, options.defaultOpen, next);

    internal::ContainerHeaderDesc header;
    header.label = label;
    header.labelEnd = ImGui::FindRenderedTextEnd(label);
    header.trailing = isOpen ? nullptr : options.summary;
    header.trailingAfterLabel = true;
    header.style = TextStyle::Body;
    header.look = link ? internal::ContainerHeaderStyle::Link : internal::ContainerHeaderStyle::Row;
    header.open = isOpen;
    header.height = ControlHeight(ControlSize::Inherit);
    header.width = link ? 0.0f : internal::ResolveItemWidth(0.0f, next, kFill);
    internal::ApplyNextItemData(next);
    g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasOpen;
    const internal::ItemState item = internal::ContainerHeader(id, header);
    if (item.pressed && !item.readOnly)
    {
        isOpen = !isOpen;
        if (open != nullptr)
            *open = isOpen;
        else
            internal::SetContainerOpenState(id, isOpen);
    }

    DisclosureEntry entry;
    entry.window = window;
    if (!internal::BeginContainerCollapse(id, isOpen, &entry.collapse))
        return false;
    entry.indent = link ? 0.0f : internal::ContainerHeaderLabelOffset(TextStyle::Body);
    if (entry.indent > 0.0f)
        ImGui::Indent(entry.indent);

    internal::PushScope("BeginDisclosure");
    DisclosureStack& stack = Disclosures();
    if (stack.depth >= internal::kScopeStackDepth)
        ++stack.overflow;
    else
        stack.entries[stack.depth++] = entry;
    return true;
}

void EndDisclosure()
{
    if (!internal::PopScope("BeginDisclosure"))
        return;
    DisclosureStack& stack = Disclosures();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    const DisclosureEntry entry = stack.entries[--stack.depth];
    if (GImGui->CurrentWindow != entry.window)
        return;
    if (entry.indent > 0.0f)
        ImGui::Unindent(entry.indent);
    internal::EndContainerCollapse(entry.collapse);
}

namespace internal
{

void ClearDisclosureStack()
{
    DisclosureStack& stack = Disclosures();
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
