// PresetList: a preset manager on the list view block (internal/list_view.h) with an InlineRename
// field for the name, a menu of actions behind a "more" IconButton and a ConfirmDialog before a
// delete. The list view gives the rows, the selection pill on the active preset, keyboard
// navigation and typeahead; this file adds the row content (the name in the pill's weight, the
// Modified and Built-in marks), the keys F2 and Delete, the inline create and rename, the menu and
// the result for the caller.
//
// Names: a name entered for a new or renamed preset is trimmed; an empty one keeps the old name (or
// becomes Strings::newPreset for a new preset), and a name another preset already has (ASCII case
// ignored) gets the first free number appended ("Preset 2"), so the caller never receives two
// presets of one name from this widget.
//
// Focus: a rename or a create that ends gives the keyboard focus back to its row, and a declined
// delete leaves it there; the confirmation dialog restores it itself. The menu acts on the row that
// last had the keyboard focus, or on the active preset when no row had it.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <new>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/display.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/overlays.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"

namespace facet
{
namespace
{

// Skeleton rows the loading state shows when the list has no rows to keep the place of.
constexpr int kPresetListLoadingRows = 3;
// The share of the row width a skeleton line takes.
constexpr float kPresetListSkeletonShare = 0.6f;
// Numbers tried for a free name before the name is taken as it is.
constexpr int kPresetListMaxSuffix = 999;

struct PresetListState
{
    int lastFocus = -1;       // the row that last had the keyboard focus
    int menuTarget = -1;      // the preset the open menu acts on
    int renaming = -1;        // the row being renamed; the preset count for a new preset
    bool editing = false;     // the rename field is open
    std::string editText;     // its copy of the name
    int refocus = -1;         // the row that gets the keyboard focus back in this frame
    int deleting = -1;        // the preset the confirmation asks about
    bool confirmOpen = false;
    std::string deleteMessage;
    std::string resultName;   // PresetListResult::name, valid until the next call
};

struct PresetListEntries
{
    std::span<const PresetItem> presets;
    const PresetListState* state = nullptr;
};

void PresetListEntry(void* user, int index, internal::ListEntryInfo* out)
{
    const PresetListEntries& entries = *static_cast<const PresetListEntries*>(user);
    if (index < static_cast<int>(entries.presets.size()))
    {
        const PresetItem& item = entries.presets[static_cast<std::size_t>(index)];
        out->label = item.label != nullptr ? item.label : "";
        out->id = item.id;
        return;
    }
    out->label = entries.state->editText.c_str();
}

bool PresetListSameName(const char* a, const char* b)
{
    if (a == nullptr || b == nullptr)
        return false;
    for (; *a != '\0' && *b != '\0'; ++a, ++b)
    {
        if (ImToUpper(*a) != ImToUpper(*b))
            return false;
    }
    return *a == *b;
}

bool PresetListNameTaken(std::span<const PresetItem> presets, const char* name, int except)
{
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        if (i != except && PresetListSameName(presets[static_cast<std::size_t>(i)].label, name))
            return true;
    }
    return false;
}

// `name` trimmed and made unique among the presets other than `except` (see the top of the file).
// Writes into *out; returns false when the trimmed name is empty.
bool PresetListFreeName(std::span<const PresetItem> presets, const std::string& name, int except, std::string* out)
{
    std::size_t begin = name.find_first_not_of(' ');
    if (begin == std::string::npos)
        return false;
    const std::size_t end = name.find_last_not_of(' ');
    try
    {
        out->assign(name, begin, end - begin + 1);
        if (!PresetListNameTaken(presets, out->c_str(), except))
            return true;
        const std::size_t base = out->size();
        char suffix[16];
        for (int n = 2; n <= kPresetListMaxSuffix; ++n)
        {
            std::snprintf(suffix, sizeof(suffix), " %d", n);
            out->resize(base);
            out->append(suffix);
            if (!PresetListNameTaken(presets, out->c_str(), except))
                return true;
        }
        out->resize(base);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

bool PresetListAssign(std::string* target, const char* text)
{
    try
    {
        target->assign(text != nullptr ? text : "");
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
}

} // namespace

PresetListResult PresetList(const char* id, int* active, std::span<const PresetItem> presets, const PresetListOptions& options)
{
    PresetListResult result;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return result;
    if (id == nullptr)
        id = "";
    const Theme& t = T();
    const Strings& s = GetStrings();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const ImGuiID widgetId = window->GetID(id);
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool readOnly = options.readOnly || active == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool ready = options.state == DataState::Ready;
    PresetListState& state = internal::GetState<PresetListState>(widgetId);
    const int count = static_cast<int>(presets.size());
    const auto builtIn = [&](int i) { return i >= 0 && i < count && presets[static_cast<std::size_t>(i)].builtIn; };
    const auto labelOf = [&](int i) {
        const char* label = i >= 0 && i < count ? presets[static_cast<std::size_t>(i)].label : nullptr;
        return label != nullptr ? label : "";
    };

    // A rename of a preset that is gone, or any edit while the list cannot be edited, ends.
    if (state.renaming > count || ((readOnly || disabled || !ready) && state.editing))
    {
        state.renaming = -1;
        state.editing = false;
    }
    if (state.lastFocus >= count)
        state.lastFocus = -1;
    const bool creating = state.renaming == count && state.editing;
    const int rows = count + (creating ? 1 : 0);

    // ---- Layout ----
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float rowHeight = internal::ListRowHeight(ControlSize::Inherit);
    const float available = ImGui::GetContentRegionAvail().y;
    float areaHeight = rowHeight * static_cast<float>(std::max(1, ready ? rows : kPresetListLoadingRows));
    if (options.height > 0.0f)
        areaHeight = Px(options.height);
    else if (options.height < 0.0f)
        areaHeight = std::max(rowHeight, available + Px(options.height));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));

    // Starts the inline field for row `row` with `text`.
    const auto startRename = [&](int row, const char* text) {
        if (!PresetListAssign(&state.editText, text))
            return;
        state.renaming = row;
        state.editing = true;
    };
    const auto choose = [&](int i) {
        if (readOnly || disabled || i < 0 || i >= count || *active == i)
            return;
        *active = i;
        result.action = PresetAction::Selected;
        result.index = i;
    };
    const auto askDelete = [&](int i) {
        if (readOnly || builtIn(i) || i < 0 || i >= count)
            return;
        const internal::TextToken token{"name", 0, labelOf(i)};
        char message[512];
        internal::FormatTokens(message, sizeof(message), s.deletePresetMessage, std::span<const internal::TextToken>(&token, 1));
        if (!PresetListAssign(&state.deleteMessage, message))
            return;
        state.deleting = i;
        state.confirmOpen = true;
    };

    ImGui::BeginGroup();
    internal::ApplyNextItemData(next);
    const int menuPart = count;
    if (!ready && options.state == DataState::Loading)
    {
        // Loading: skeleton lines in the place of the rows.
        const int lines = std::max(1, static_cast<int>(areaHeight / rowHeight));
        SkeletonOptions skeleton;
        skeleton.width = width * kPresetListSkeletonShare / Scale();
        for (int i = 0; i < lines; ++i)
        {
            const float lineHeight = MeasureText(TextStyle::Body, "").y;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + m.paddingX, origin.y + rowHeight * static_cast<float>(i) +
                                                                     std::round((rowHeight - lineHeight) * 0.5f)));
            Skeleton(skeleton);
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + areaHeight));
        ImGui::Dummy(ImVec2(width, 0.0f));
    }
    else
    {
        int choice = active != nullptr && *active >= 0 && *active < count ? *active : -1;
        const PresetListEntries entries{presets, &state};
        internal::ListViewDesc view;
        view.rowCount = rows;
        view.areaSize = ImVec2(width, areaHeight);
        view.choice = &choice;
        view.pill = true;
        view.readOnly = true;  // the arrows move the focus; Enter or a click chooses
        view.entry = &PresetListEntry;
        view.user = const_cast<PresetListEntries*>(&entries);
        view.state = options.state;
        view.emptyText = options.emptyText;
        view.errorText = options.errorText;
        view.revealRow = creating ? count : (state.refocus >= 0 ? state.refocus : -1);
        if (forced.state != internal::ForcedItemState::None && forced.part != menuPart)
            internal::ForceNextItemState(forced.state, forced.part);
        if (internal::BeginListView(widgetId, view))
        {
            RowRange range;
            while (internal::NextListRows(&range))
            {
                for (int i = range.first; i < range.last; ++i)
                {
                    const bool renamingRow = state.editing && state.renaming == i;
                    if (renamingRow)
                        ImGui::SetNextItemAllowOverlap();
                    const internal::ListRowState row = internal::ListViewRow(i);
                    if (row.focused && i < count)
                        state.lastFocus = i;
                    if (state.refocus == i)
                    {
                        ImGui::SetFocusID(row.item.id, ImGui::GetCurrentWindow());
                        ImGui::SetNavCursorVisible(true);
                        internal::SetKeyboardModality(true);
                        state.refocus = -1;
                    }
                    if (renamingRow)
                    {
                        // The name as an inline field in the row.
                        ImGui::SetCursorScreenPos(ImVec2(row.content.Min.x, row.rect.Min.y + std::round((row.rect.GetHeight() - m.height) * 0.5f)));
                        InlineRenameOptions rename;
                        rename.startOnDoubleClick = false;
                        rename.startOnF2 = false;
                        rename.width = std::max(1.0f, row.content.GetWidth()) / Scale();
                        (void)InlineRename("##rename", &state.editText, &state.editing, rename);
                        if (!state.editing)
                        {
                            // The field wrote the text only when it changed; Escape drops the edit.
                            const bool canceled = IsItemEditCanceled() || ImGui::GetKeyData(ImGuiKey_Escape)->DownDuration == 0.0f;
                            std::string name;
                            const bool hasName = !canceled && PresetListFreeName(presets, state.editText, i, &name);
                            if (i == count)
                            {
                                // A new preset: an empty name takes the default name.
                                if (!canceled && !hasName)
                                    PresetListFreeName(presets, s.newPreset, -1, &name);
                                if (!canceled && !name.empty() && PresetListAssign(&state.resultName, name.c_str()))
                                {
                                    result.action = PresetAction::Create;
                                    result.index = -1;
                                    result.name = state.resultName.c_str();
                                }
                            }
                            else if (hasName && name != labelOf(i) && PresetListAssign(&state.resultName, name.c_str()))
                            {
                                result.action = PresetAction::Rename;
                                result.index = i;
                                result.name = state.resultName.c_str();
                            }
                            state.refocus = i == count ? (active != nullptr ? *active : -1) : i;
                            state.renaming = -1;
                        }
                        continue;
                    }

                    // The name in the pill's weight, then the marks at the end.
                    if (row.item.visible && i < count)
                    {
                        const PresetItem& item = presets[static_cast<std::size_t>(i)];
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        float right = row.content.Max.x;
                        {
                            internal::ControlTextScope note(TextStyle::Footnote);
                            const ImU32 secondary = ToU32(internal::ControlSecondaryColor(row.disabled || disabled));
                            const float gap = Px(t.spacing.sm);
                            for (const char* mark : {item.builtIn ? s.builtIn : nullptr, item.modified ? s.modified : nullptr})
                            {
                                if (mark == nullptr)
                                    continue;
                                const float w = ImGui::CalcTextSize(mark).x;
                                if (right - w < row.content.Min.x + row.content.GetWidth() * 0.5f)
                                    break;
                                right -= w;
                                drawList->AddText(ImVec2(right, CenteredTextY(row.rect.Min.y, row.rect.GetHeight())), secondary, mark);
                                right -= gap;
                            }
                        }
                        internal::PushPillLabelStyle(TextStyle::Body, row.selected);
                        const internal::FittedText shown = internal::FitText(labelOf(i), nullptr, right - row.content.Min.x);
                        drawList->AddText(ImVec2(row.content.Min.x, CenteredTextY(row.rect.Min.y, row.rect.GetHeight())),
                                          ToU32(internal::ControlLabelColor(row.disabled || disabled)), shown.text);
                        PopTextStyle();
                        internal::CutTextTooltip(row.item, labelOf(i), nullptr, shown.truncated);
                    }

                    // A click chooses (a press with the mouse button going down over the row: the
                    // arrows also press a row of the list, without a click); F2 renames and Delete
                    // asks to delete the focused row.
                    if (i < count && row.item.pressed && row.item.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        choose(i);
                    if (i < count && row.focused && !readOnly && !disabled && GImGui->ActiveId == 0 &&
                        row.item.forced == internal::ForcedItemState::None)
                    {
                        if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && !builtIn(i))
                            startRename(i, labelOf(i));
                        else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                            askDelete(i);
                    }
                }
            }
            const internal::ListViewResult list = internal::EndListView();
            if (list.activated >= 0 && list.activated < count)
                choose(list.activated);
        }
    }

    // ---- The menu ----
    if (!readOnly)
    {
        const float button = std::round(m.height);
        ImGui::SetCursorScreenPos(ImVec2(origin.x + width - button, window->DC.CursorPos.y));
        ImGui::PushOverrideID(widgetId);
        char menuLabel[96];
        std::snprintf(menuLabel, sizeof(menuLabel), "%s###menu", s.more);
        if (forced.state != internal::ForcedItemState::None && forced.part == menuPart)
            internal::ForceNextItemState(forced.state & ~internal::ForcedItemState::Open);
        const bool pressed = IconButton(menuLabel, Icon("more-horizontal"));
        const ImRect buttonRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        const ImGuiID menuLevel = ImGui::GetID("##menu level");
        internal::PopupOptions popup;
        popup.minWidth = Px(t.overlay.menuMinWidth);
        const internal::PopupAnchor anchor{buttonRect, internal::PopupSide::Below, internal::PopupAlign::End};
        if (pressed)
        {
            state.menuTarget = state.lastFocus >= 0 ? state.lastFocus : (active != nullptr ? *active : -1);
            if (state.menuTarget >= count)
                state.menuTarget = -1;
            internal::OpenPopupLevel(menuLevel, anchor, popup);
        }
        if (forced.state != internal::ForcedItemState::None && forced.part == menuPart &&
            internal::Has(forced.state, internal::ForcedItemState::Open))
        {
            state.menuTarget = active != nullptr ? *active : -1;
            internal::ForcePopupLevelOpen(menuLevel, anchor, popup);
        }
        if (internal::BeginPopupLevel(menuLevel))
        {
            internal::MenuLevelBegin(menuLevel);
            const int target = state.menuTarget >= 0 && state.menuTarget < count ? state.menuTarget : -1;
            const bool canTarget = ready && target >= 0;
            const auto row = [&](const char* label, Icon icon, bool enabled, bool destructive = false) {
                internal::MenuRowDesc desc;
                desc.icon = icon;
                desc.disabled = !enabled;
                desc.destructive = destructive;
                return internal::MenuRow(menuLevel, label, desc).pressed;
            };
            if (row(s.newPreset, Icon("plus"), true))
            {
                std::string name;
                PresetListFreeName(presets, s.newPreset, -1, &name);
                startRename(count, name.c_str());
            }
            if (row(s.save, Icon("download"), canTarget))
                result = {PresetAction::Save, target, nullptr};
            if (row(s.rename, Icon("edit"), canTarget && !builtIn(target)))
                startRename(target, labelOf(target));
            if (row(s.duplicate, Icon("copy"), canTarget))
                result = {PresetAction::Duplicate, target, nullptr};
            if (options.importExport)
            {
                if (row(s.importAction, Icon("upload"), true))
                    result = {PresetAction::Import, -1, nullptr};
                if (row(s.exportAction, Icon("external-link"), canTarget))
                    result = {PresetAction::Export, target, nullptr};
            }
            if (row(s.deleteAction, Icon("trash"), canTarget && !builtIn(target), true))
                askDelete(target);
            internal::EndPopupLevel();
        }
        ImGui::PopID();
    }

    // ---- The delete confirmation ----
    if (state.confirmOpen || state.deleting >= 0)
    {
        char title[160];
        std::snprintf(title, sizeof(title), "%s###%08X delete", s.deletePresetTitle, widgetId);
        ConfirmDialogOptions confirm;
        confirm.confirmLabel = s.deleteAction;
        confirm.destructive = true;
        const DialogResult decided = ConfirmDialog(title, &state.confirmOpen, state.deleteMessage.c_str(), confirm);
        if (decided == DialogResult::Confirmed && state.deleting >= 0 && state.deleting < count)
            result = {PresetAction::Delete, state.deleting, nullptr};
        if (!state.confirmOpen)
            state.deleting = -1;
    }

    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (result.action == PresetAction::Selected)
        internal::MarkWidgetEdited(widgetId);
    return result;
}

} // namespace facet
