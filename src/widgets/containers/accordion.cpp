// BeginAccordion, BeginAccordionSection and their Ends (containers.h): a stack of sections, each
// a header row (chevron, icon, title in Headline, detail at the end) over content that opens and
// closes with an animated height (container_layout.h). Sections are separated by hairlines. The
// accordion owns the open states, kept under the section ids (which are scoped by the accordion
// id) and saved in the ini file; in single-open mode the section opened last stays open and the
// others close.
//
// Keys: Up and Down on a focused header move to the previous or next header, Home and End to the
// first and last, skipping the content in between; Enter or Space toggles.

#include "facet/widgets/containers.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/list_view.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/containers/container_layout.h"

namespace facet
{
namespace
{

// Headers an accordion remembers for keyboard moves; later ones are still drawn and toggled.
constexpr int kAccordionMaxSections = 64;

struct AccordionHeaders
{
    ImGuiID ids[kAccordionMaxSections] = {};
    ImRect rects[kAccordionMaxSections];
    int count = 0;
};

// Kept under the accordion id across frames.
struct AccordionState
{
    ImGuiID openSection = 0;     // single-open mode: the section that stays open
    AccordionHeaders current;    // headers of this frame so far
    AccordionHeaders previous;   // headers of the last frame
    int frame = -1;
    // The frame of the last keyboard move. A header below the one that moved the focus is submitted
    // later in the same frame with the key still pressed (SetKeyOwner hands it the press at once),
    // so it must not move again.
    int moveFrame = -1;
};

struct AccordionEntry
{
    ImGuiWindow* window = nullptr;  // null when nothing is drawn
    ImGuiID id = 0;
    bool singleOpen = false;
    float width = 0.0f;
    internal::ContainerInset inset;
};

struct SectionEntry
{
    ImGuiWindow* window = nullptr;
    float indent = 0.0f;
    internal::ContainerCollapse collapse;
};

struct AccordionStack
{
    AccordionEntry accordions[internal::kScopeStackDepth];
    int accordionDepth = 0;
    int accordionOverflow = 0;
    SectionEntry sections[internal::kScopeStackDepth];
    int sectionDepth = 0;
    int sectionOverflow = 0;
};

AccordionStack& Stack()
{
    static AccordionStack stack;
    return stack;
}

AccordionEntry* InnermostAccordion()
{
    AccordionStack& stack = Stack();
    if (stack.accordionOverflow > 0 || stack.accordionDepth == 0)
        return nullptr;
    AccordionEntry& entry = stack.accordions[stack.accordionDepth - 1];
    return entry.window != nullptr && entry.window == GImGui->CurrentWindow ? &entry : nullptr;
}

AccordionState& StateOf(ImGuiID id)
{
    AccordionState& state = internal::GetState<AccordionState>(id);
    const int frame = ImGui::GetFrameCount();
    if (state.frame != frame)
    {
        // A frame that skipped the accordion keeps the headers it last saw.
        if (state.frame == frame - 1 || state.current.count > 0)
            state.previous = state.current;
        state.current.count = 0;
        state.frame = frame;
    }
    return state;
}

int FindHeader(const AccordionHeaders& headers, ImGuiID id)
{
    for (int i = 0; i < headers.count; ++i)
        if (headers.ids[i] == id)
            return i;
    return -1;
}

// Up, Down, Home and End on the focused header `id`: moves the keyboard focus to another header
// of the last frame.
void MoveBetweenHeaders(AccordionState& state, ImGuiWindow* window, ImGuiID id)
{
    const ImGuiKey keys[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_Home, ImGuiKey_End};
    for (const ImGuiKey key : keys)
        ImGui::SetKeyOwner(key, id);
    if (state.moveFrame == ImGui::GetFrameCount())
        return;
    const AccordionHeaders& headers = state.previous;
    const int at = FindHeader(headers, id);
    if (at < 0 || headers.count == 0)
        return;
    int target = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, id))
        target = std::max(0, at - 1);
    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, id))
        target = std::min(headers.count - 1, at + 1);
    else if (ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_None, id))
        target = 0;
    else if (ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_None, id))
        target = headers.count - 1;
    if (target < 0 || target == at)
        return;
    state.moveFrame = ImGui::GetFrameCount();
    internal::ListFocusRow(window, headers.ids[target], GImGui->CurrentFocusScopeId, headers.rects[target]);
    internal::ScrollAreaRevealRect(window, headers.rects[target]);
}

} // namespace

void BeginAccordion(const char* id, const AccordionOptions& options)
{
    internal::PushScope("BeginAccordion");
    AccordionStack& stack = Stack();
    if (stack.accordionDepth >= internal::kScopeStackDepth)
    {
        ++stack.accordionOverflow;
        return;
    }
    AccordionEntry& entry = stack.accordions[stack.accordionDepth++];
    entry = AccordionEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    entry.window = window;
    entry.id = window->GetID(id);
    entry.singleOpen = options.singleOpen;
    entry.width = internal::ResolveItemWidth(options.width, next, kFill);
    internal::BeginContainerInset(&entry.inset, entry.width, 0.0f);
    internal::OpenContainerInset(entry.inset);
    ImGui::PushID(id);
    StateOf(entry.id);
}

void EndAccordion()
{
    if (!internal::PopScope("BeginAccordion"))
        return;
    AccordionStack& stack = Stack();
    if (stack.accordionOverflow > 0)
    {
        --stack.accordionOverflow;
        return;
    }
    if (stack.accordionDepth == 0)
        return;
    const AccordionEntry entry = stack.accordions[--stack.accordionDepth];
    if (entry.window == nullptr)
        return;
    if (GImGui->CurrentWindow != entry.window)
    {
        internal::RestoreContainerInset(entry.inset);
        return;
    }
    ImGui::PopID();
    internal::EndContainerInset(entry.inset, 0.0f);
}

bool BeginAccordionSection(const char* label, const AccordionSectionOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& theme = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    AccordionEntry* accordion = InnermostAccordion();
    AccordionState* state = accordion != nullptr ? &StateOf(accordion->id) : nullptr;

    bool open = internal::ContainerOpenState(id, options.defaultOpen, next);
    if (accordion != nullptr && accordion->singleOpen)
    {
        const bool forcedOpen = (next.data.HasFlags & ImGuiNextItemDataFlags_HasOpen) != 0 && next.data.OpenVal;
        if (open && (state->openSection == 0 || forcedOpen))
            state->openSection = id;
        else if (open && state->openSection != id)
        {
            open = false;
            internal::SetContainerOpenState(id, false);
        }
    }

    // A hairline between this section and the one above it.
    const int index = state != nullptr ? state->current.count : 0;
    const float width = accordion != nullptr ? accordion->width : internal::ResolveItemWidth(0.0f, next, kFill);
    if (index > 0)
    {
        const ImVec2 pos = window->DC.CursorPos;
        if (window->ClipRect.Min.y <= pos.y && pos.y <= window->ClipRect.Max.y)
            HairlineH(window->DrawList, pos.x, pos.x + width, pos.y - std::floor(g.Style.ItemSpacing.y * 0.5f),
                      ToU32(theme.colors.separator));
    }

    internal::ContainerHeaderDesc header;
    header.label = label;
    header.labelEnd = ImGui::FindRenderedTextEnd(label);
    header.icon = options.icon;
    header.trailing = options.detail;
    header.style = TextStyle::Headline;
    header.open = open;
    header.width = width;
    header.height = internal::ListRowHeight(ControlSize::Inherit);
    internal::ApplyNextItemData(next);
    g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasOpen;
    const internal::ItemState item = internal::ContainerHeader(id, header);
    if (state != nullptr && state->current.count < kAccordionMaxSections)
    {
        state->current.ids[state->current.count] = id;
        state->current.rects[state->current.count] = item.rect;
        ++state->current.count;
    }
    if (item.pressed && !item.readOnly)
    {
        open = !open;
        internal::SetContainerOpenState(id, open);
        if (state != nullptr && accordion->singleOpen)
            state->openSection = open ? id : 0;
    }
    if (item.focused && !item.disabled && state != nullptr)
        MoveBetweenHeaders(*state, window, id);

    SectionEntry entry;
    entry.window = window;
    if (!internal::BeginContainerCollapse(id, open, &entry.collapse))
        return false;
    entry.indent = internal::ContainerHeaderLabelOffset(TextStyle::Headline);
    ImGui::Indent(entry.indent);

    internal::PushScope("BeginAccordionSection");
    AccordionStack& stack = Stack();
    if (stack.sectionDepth >= internal::kScopeStackDepth)
        ++stack.sectionOverflow;
    else
        stack.sections[stack.sectionDepth++] = entry;
    return true;
}

void EndAccordionSection()
{
    if (!internal::PopScope("BeginAccordionSection"))
        return;
    AccordionStack& stack = Stack();
    if (stack.sectionOverflow > 0)
    {
        --stack.sectionOverflow;
        return;
    }
    if (stack.sectionDepth == 0)
        return;
    const SectionEntry entry = stack.sections[--stack.sectionDepth];
    if (GImGui->CurrentWindow != entry.window)
        return;
    ImGui::Unindent(entry.indent);
    internal::EndContainerCollapse(entry.collapse);
}

namespace internal
{

void ClearAccordionStack()
{
    AccordionStack& stack = Stack();
    for (int i = stack.accordionDepth - 1; i >= 0; --i)
        if (stack.accordions[i].window != nullptr)
            RestoreContainerInset(stack.accordions[i].inset);
    stack.accordionDepth = 0;
    stack.accordionOverflow = 0;
    stack.sectionDepth = 0;
    stack.sectionOverflow = 0;
}

} // namespace internal

} // namespace facet
