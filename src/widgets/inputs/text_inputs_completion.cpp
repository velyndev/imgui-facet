// The completion list of the text-like inputs (text_inputs_shared.h): a completion level of the
// popup host under the field, holding a driven list view whose keys the field hands over while the
// list is open.

#include <algorithm>
#include <climits>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet::internal::text_inputs
{

namespace
{

// Kept under the level id.
struct CompletionState
{
    int highlight = -1;    // the active row
    bool latched = false;  // closed by Escape, Enter, Tab, a click outside or a choice: stays closed until the next edit
};

// What the list view's entry callback reads.
struct CompletionViewData
{
    const CompletionDesc* desc = nullptr;
};

void CompletionListEntry(void* user, int index, ListEntryInfo* out)
{
    const CompletionDesc& desc = *static_cast<CompletionViewData*>(user)->desc;
    CompletionEntry entry;
    if (desc.entry != nullptr)
        desc.entry(desc.user, index, &entry);
    out->label = entry.label;
    out->disabled = entry.disabled;
}

// Draws one row's content: icon, label with its matched ranges, detail at the end.
void DrawCompletionRow(const ListRowState& row, const CompletionEntry& entry)
{
    if (!row.item.visible)
        return;
    const Theme& t = T();
    const Colors& c = t.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImRect& box = row.content;
    const bool disabled = row.disabled;
    float x = box.Min.x;
    const float iconSize = std::round(ImGui::GetFontSize());
    if (entry.icon.IsValid())
    {
        DrawControlIcon(drawList, entry.icon, x, box.GetCenter().y, iconSize, ToU32(ControlSecondaryColor(disabled)));
        x += iconSize + Px(t.spacing.sm);
    }
    const char* label = entry.label != nullptr ? entry.label : "";
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    float right = box.Max.x;
    const float y = CenteredTextY(box.Min.y, box.GetHeight());
    if (entry.detail != nullptr && entry.detail[0] != '\0')
    {
        // The detail gives way to the label: it takes at most a third of the row.
        const FittedText detail = FitText(entry.detail, nullptr, box.GetWidth() / 3.0f);
        drawList->AddText(ImVec2(right - detail.width, y), ToU32(ControlSecondaryColor(disabled)), detail.text);
        right -= detail.width + Px(t.spacing.md);
    }
    const FittedText shown = FitText(label, labelEnd, std::max(0.0f, right - x));
    const ImU32 textColor = ToU32(ControlLabelColor(disabled));
    if (!shown.truncated && !entry.matches.empty() && !disabled)
    {
        HighlightStyle style;
        style.text = textColor;
        style.matchText = textColor;
        style.matchBackground = ToU32(c.accentSubtle);
        style.rounding = Px(t.radii.element) * 0.5f;
        DrawHighlightedText(drawList, ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x, y), label, labelEnd,
                            entry.matches, style);
    }
    else
    {
        drawList->AddText(ImVec2(x, y), textColor, shown.text);
    }
}

// The field hands these keys to the open list (text_field.h). Dear ImGui keeps an owner while the
// key is down and carries the owner set in one frame into the next, so once the list is closed the
// keys it owned are released at once: a key pressed right after the list closed reaches the field.
// A key that is down was used by the list in this frame and stays its own until it goes up, unless
// `includeDown` hands it on (Enter or Tab that closed a list with no active row). With
// `acceptOnly`, only Enter and Tab are released: while no row is active they belong to the field
// (Tab moves the focus, Dear ImGui's navigation reads it before any widget runs).
void ReleaseCompletionKeys(ImGuiID level, bool includeDown, bool acceptOnly = false)
{
    const ImGuiKey keys[] = {ImGuiKey_Enter,   ImGuiKey_KeypadEnter, ImGuiKey_Tab,      ImGuiKey_UpArrow,
                             ImGuiKey_DownArrow, ImGuiKey_PageUp,    ImGuiKey_PageDown, ImGuiKey_Escape};
    const int count = acceptOnly ? 3 : static_cast<int>(sizeof(keys) / sizeof(keys[0]));
    for (int i = 0; i < count; ++i)
    {
        const ImGuiKey key = keys[i];
        if (ImGui::GetKeyOwner(key) == level && (includeDown || !ImGui::IsKeyDown(key, ImGuiKeyOwner_Any)))
            ImGui::SetKeyOwner(key, ImGuiKeyOwner_NoOwner);
    }
}

} // namespace

CompletionList::CompletionList(ImGuiID fieldId)
    : field_(fieldId), level_(ImGui::GetIDWithSeed("##completion", nullptr, fieldId))
{
}

void CompletionList::BeforeField()
{
    if (!IsPopupLevelOpen(level_) || ImGui::GetActiveID() != field_)
        return;
    CompletionState& state = GetState<CompletionState>(level_);
    if (state.highlight >= 0)
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_Tab, false))
    {
        ClosePopupLevel(level_);
        state.latched = true;
        ReleaseCompletionKeys(level_, true);
    }
}

int CompletionList::Highlight() const
{
    return GetState<CompletionState>(level_).highlight;
}

bool CompletionList::IsOpen() const
{
    return IsPopupLevelOpen(level_);
}

int CompletionList::Run(const TextFieldResult& field, const CompletionDesc& desc)
{
    CompletionState& state = GetState<CompletionState>(level_);
    const PopupCloseReason closed = PopupLevelClosedReason(level_);
    if (closed == PopupCloseReason::Escape || closed == PopupCloseReason::ClickOutside)
        state.latched = true;
    if (field.edited && !field.clearPressed)
    {
        state.latched = false;
        state.highlight = -1;
    }
    // Escape belongs to the open list (the field hands it over): it closes the list, and the next
    // Escape reaches the field.
    if (IsPopupLevelOpen(level_) && ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, level_))
    {
        ClosePopupLevel(level_);
        state.latched = true;
    }
    const bool open = IsPopupLevelOpen(level_);
    // A new edit session starts with the list allowed again.
    if (!field.active && !open && ImGui::GetActiveID() != field_)
        state.latched = false;
    // A press on a row takes the focus from the field until the release that accepts the row; the
    // list stays while the button is down and in the frame it goes up.
    const ImGuiIO& io = ImGui::GetIO();
    const bool rowHeld = open &&
                         (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) &&
                         PopupLevelRect(level_).Contains(io.MouseClickedPos[ImGuiMouseButton_Left]);

    const PopupAnchor anchor = TextFieldPopupAnchor(field, TextFieldAnchor::Field);
    PopupOptions options;
    options.takeFocus = false;
    if (desc.forcedOpen)
    {
        ForcePopupLevelOpen(level_, anchor, options);
    }
    else
    {
        const bool want = (desc.wanted || rowHeld) && !state.latched;
        if (want && !open)
            OpenPopupLevel(level_, anchor, options);
        else if (!want && open)
            ClosePopupLevel(level_);
        else if (open)
            SetPopupLevelAnchor(level_, anchor);  // the field may have scrolled or the window resized
        if (!want)
            state.highlight = -1;
    }

    int accepted = -1;
    if (BeginPopupLevel(level_))
    {
        const Theme& t = T();
        const ControlSize size = ResolveControlSize(desc.size);
        const float rowHeight = ListRowHeight(size);
        const int maxRows = std::max(1, desc.maxVisibleRows > 0 ? desc.maxVisibleRows : t.list.popupRows);
        const bool rowsShown = desc.state == DataState::Ready && desc.count > 0;
        const int rows = rowsShown ? std::min(desc.count, maxRows) : 1;
        // The level adds spacing.xs on each side; the list keeps the field's width inside it.
        const float width = std::max(1.0f, std::round(field.frame.GetWidth() - 2.0f * Px(t.spacing.xs)));

        CompletionViewData data;
        data.desc = &desc;
        ListViewDesc view;
        view.rowCount = rowsShown ? desc.count : 0;
        view.size = size;
        view.areaSize = ImVec2(width, rowHeight * static_cast<float>(rows));
        view.fadeSurface = t.colors.bgElevated;
        view.entry = CompletionListEntry;
        view.user = &data;
        view.state = desc.state;
        view.emptyText = desc.emptyText != nullptr ? desc.emptyText : GetStrings().noResults;
        view.errorText = desc.errorText;
        view.highlight = &state.highlight;
        view.keyOwner = desc.forcedOpen ? 0 : level_;
        if (desc.forcedOpen)
        {
            // The capture shows the first row active, as after Down.
            state.highlight = rowsShown ? 0 : -1;
            ForceNextItemState(ForcedItemState::Hovered, 0);
        }
        if (BeginListView(ImGui::GetID("list"), view))
        {
            PushTextStyle(ControlTextStyle(size));
            RowRange range;
            while (NextListRows(&range))
            {
                for (int i = range.first; i < range.last; ++i)
                {
                    CompletionEntry entry;
                    if (desc.entry != nullptr)
                        desc.entry(desc.user, i, &entry);
                    const ListRowState row = ListViewRow(i, {.disabled = entry.disabled});
                    DrawCompletionRow(row, entry);
                }
            }
            PopTextStyle();
            const ListViewResult result = EndListView();
            if (result.activated >= 0)
                accepted = result.activated;
        }
        EndPopupLevel();
    }

    if (accepted >= 0 && !desc.forcedOpen)
    {
        ClosePopupLevel(level_);
        state.latched = true;
        state.highlight = -1;
        // A click moved the focus to the row: the field takes the keyboard back, the caret after
        // the accepted text. After Enter or Tab the field still has it; the caret moves all the same.
        TextFieldRequestFocus(field_, TextFieldFocus::End);
        TextFieldSetSelection(field_, INT_MAX, INT_MAX);
    }
    if (!desc.forcedOpen && !IsPopupLevelOpen(level_))
        ReleaseCompletionKeys(level_, false);
    else if (state.highlight < 0)
        ReleaseCompletionKeys(level_, false, true);
    return accepted;
}

} // namespace facet::internal::text_inputs
