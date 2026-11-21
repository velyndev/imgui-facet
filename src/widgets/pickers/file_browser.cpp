// FileBrowser: a file browser drawn inside the app. A path bar (the parent folder button and the
// folder's path), a framed list of the folder's entries (a list view of the list core: folders
// first, the chosen row on the selection pill, typeahead), and a bottom bar with the file name
// field (Save), the file type dropdown and the confirm button.
//
// Every file system call runs on the folder reader's worker thread (file_browser_parts.h): the
// browser asks for its start folder, for listings, and every kFileBrowserCheckSeconds whether the
// folder still exists and changed, and shows the loading state while it waits. An unreadable folder
// shows the error state, an empty one the empty state, a folder that disappears while it is shown
// turns into the error state at the next check; the parent folder button stays usable. A very
// large folder costs what its visible rows cost: the list view submits only those.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/lists.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"
#include "widgets/pickers/file_browser_parts.h"

namespace facet
{
namespace
{

using internal::FileBrowserEntry;
using internal::FileBrowserJob;
using internal::FileBrowserResult;

// Seconds between two checks of the shown folder (does it still exist, did it change). A polling
// interval of the reader, not a motion: it is not scaled by motion.scale.
constexpr double kFileBrowserCheckSeconds = 2.0;
// The file type dropdown offers at most this many types.
constexpr int kFileBrowserTypeCapacity = 32;
constexpr int kFileBrowserNameSize = 260;

enum class FileBrowserData
{
    Ready,
    Loading,
    Error,
};

struct FileBrowserState
{
    std::string folder;   // the folder shown, UTF-8
    std::string parent;   // its parent; empty at a root
    std::vector<FileBrowserEntry> entries;
    std::vector<int> shown;  // the listed rows: indices into entries
    FileBrowserData data = FileBrowserData::Loading;
    unsigned generation = 0;
    bool started = false;
    bool checking = false;
    double checkTime = 0.0;
    long long stamp = 0;
    int choice = -1;       // the chosen row
    int fileType = 0;
    bool rebuild = true;
    int builtType = -1;
    bool builtHidden = false;
    FileBrowserMode builtMode = FileBrowserMode::Open;
    char name[kFileBrowserNameSize] = {};  // Save: the file name typed
    bool listFocused = false;
    std::string target;   // the path the confirm button would write; kept so its storage is reused
};

struct FileBrowserRows
{
    const FileBrowserState* state = nullptr;
};

void FileBrowserEntryInfo(void* user, int row, internal::ListEntryInfo* out)
{
    const FileBrowserState& state = *static_cast<const FileBrowserRows*>(user)->state;
    const FileBrowserEntry& entry = state.entries[static_cast<std::size_t>(state.shown[static_cast<std::size_t>(row)])];
    out->label = entry.name.c_str();
    out->id = ImHashStr(entry.name.c_str());
}

// Asks the reader for a folder (or the start folder) under a new generation.
void FileBrowserLoad(ImGuiID id, FileBrowserState& state, FileBrowserJob kind, const std::string& folder)
{
    ++state.generation;
    state.data = FileBrowserData::Loading;
    state.checking = false;
    state.choice = -1;
    try
    {
        internal::FileBrowserRequest(id, {.kind = kind, .generation = state.generation, .folder = folder});
    }
    catch (...)
    {
        state.data = FileBrowserData::Error;
    }
}

// The joined path of the folder and a name, UTF-8.
void FileBrowserJoin(std::string* out, const std::string& folder, const char* name)
{
    out->assign(folder);
    if (name != nullptr && name[0] != '\0')
    {
        if (!out->empty() && out->back() != '\\' && out->back() != '/')
            out->push_back('\\');
        out->append(name);
    }
}

bool FileBrowserImpl(const char* label, std::string* path, char* buffer, std::size_t bufferSize,
                     const FileBrowserOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const Strings& strings = GetStrings();
    const ImGuiID id = window->GetID(label);
    FileBrowserState& state = GetState<FileBrowserState>(id);
    const bool hasTarget = path != nullptr || (buffer != nullptr && bufferSize > 0);
    const bool readOnly = options.readOnly || !hasTarget || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const ControlMetrics m = GetControlMetrics(ControlSize::Inherit);

    // ---- The reader ----
    bool written = false;
    try
    {
        if (!state.started)
        {
            state.started = true;
            std::string start;
            if (path != nullptr)
                start = *path;
            else if (buffer != nullptr)
                start.assign(buffer, strnlen(buffer, bufferSize));
            FileBrowserLoad(id, state, FileBrowserJob::Start, start);
        }
        FileBrowserResult result;
        if (FileBrowserTake(id, &result) && result.generation == state.generation)
        {
            if (result.kind == FileBrowserJob::Check)
            {
                state.checking = false;
                if (!result.ok)
                {
                    // The folder disappeared (or cannot be reached any more).
                    state.data = FileBrowserData::Error;
                    state.entries.clear();
                    state.rebuild = true;
                }
                else if (result.stamp != state.stamp)
                {
                    // It changed: read it again, keeping the rows shown until the new ones arrive.
                    ++state.generation;
                    FileBrowserRequest(id, {.kind = FileBrowserJob::List, .generation = state.generation, .folder = state.folder});
                }
            }
            else
            {
                const bool sameFolder = result.folder == state.folder;
                std::string chosen;
                if (sameFolder && state.choice >= 0 && state.choice < static_cast<int>(state.shown.size()))
                    chosen = state.entries[static_cast<std::size_t>(state.shown[static_cast<std::size_t>(state.choice)])].name;
                state.folder = std::move(result.folder);
                state.parent = std::move(result.parent);
                state.entries = std::move(result.entries);
                state.stamp = result.stamp;
                state.data = result.ok ? FileBrowserData::Ready : FileBrowserData::Error;
                state.rebuild = true;
                state.choice = -1;
                state.checkTime = ImGui::GetTime();
                if (!chosen.empty())
                {
                    // A refresh keeps the chosen entry by name.
                    for (std::size_t i = 0; i < state.entries.size(); ++i)
                        if (state.entries[i].name == chosen)
                            state.choice = static_cast<int>(i);  // an entry index until the rows are built
                    state.choice = state.choice >= 0 ? -2 - state.choice : -1;
                }
            }
        }
        if (state.data != FileBrowserData::Loading && !state.checking && !state.folder.empty() &&
            ImGui::GetTime() - state.checkTime >= kFileBrowserCheckSeconds && !FileBrowserBusy(id))
        {
            state.checking = true;
            state.checkTime = ImGui::GetTime();
            FileBrowserRequest(id, {.kind = FileBrowserJob::Check, .generation = state.generation, .folder = state.folder});
        }

        // ---- The rows: hidden entries, the mode and the file type ----
        const int typeCount = std::min(static_cast<int>(options.fileTypes.size()), kFileBrowserTypeCapacity);
        state.fileType = typeCount > 0 ? std::clamp(state.fileType, 0, typeCount - 1) : 0;
        if (state.rebuild || state.builtType != state.fileType || state.builtHidden != options.showHidden ||
            state.builtMode != options.mode)
        {
            const char* patterns = typeCount > 0 ? options.fileTypes[static_cast<std::size_t>(state.fileType)].patterns : nullptr;
            int keepEntry = state.choice <= -2 ? -2 - state.choice : -1;
            if (state.choice >= 0 && state.choice < static_cast<int>(state.shown.size()))
                keepEntry = state.shown[static_cast<std::size_t>(state.choice)];
            state.shown.clear();
            state.choice = -1;
            for (int i = 0; i < static_cast<int>(state.entries.size()); ++i)
            {
                const FileBrowserEntry& entry = state.entries[static_cast<std::size_t>(i)];
                if (entry.hidden && !options.showHidden)
                    continue;
                if (!entry.folder && (options.mode == FileBrowserMode::Folder || !FileBrowserMatches(entry.name.c_str(), patterns)))
                    continue;
                if (i == keepEntry)
                    state.choice = static_cast<int>(state.shown.size());
                state.shown.push_back(i);
            }
            state.rebuild = false;
            state.builtType = state.fileType;
            state.builtHidden = options.showHidden;
            state.builtMode = options.mode;
        }
    }
    catch (...)
    {
        state.data = FileBrowserData::Error;
        state.entries.clear();
        state.shown.clear();
        state.choice = -1;
    }

    // ---- Layout ----
    const float width = ResolveItemWidth(options.width, next, kFill);
    const float gap = PxSnapped(t.spacing.sm);
    const float bar = m.height;
    const bool saveMode = options.mode == FileBrowserMode::Save;
    const float bars = bar + gap + (saveMode ? bar + gap : 0.0f) + bar + gap;
    const float listHeight = options.height > 0.0f ? std::max(ListRowHeight(ControlSize::Inherit), std::round(Px(options.height)) - bars)
                                                   : std::round(Px(t.metrics.panelHeight));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    ImGui::BeginGroup();
    ImGui::PushOverrideID(id);

    std::string& target = state.target;
    target.clear();
    bool navigateParent = false;
    int navigateRow = -1;
    bool confirm = false;
    const bool ready = state.data == FileBrowserData::Ready;
    const auto chosenEntry = [&]() -> const FileBrowserEntry* {
        if (!ready || state.choice < 0 || state.choice >= static_cast<int>(state.shown.size()))
            return nullptr;
        return &state.entries[static_cast<std::size_t>(state.shown[static_cast<std::size_t>(state.choice)])];
    };

    // The path bar: the parent folder button and the folder, cut in the middle.
    {
        char parentLabel[96];
        std::snprintf(parentLabel, sizeof(parentLabel), "%s###parent", strings.parentFolder);
        ImGui::SetCursorScreenPos(origin);
        ImGui::BeginDisabled(state.parent.empty() || state.data == FileBrowserData::Loading);
        if (IconButton(parentLabel, Icon("arrow-up")))
            navigateParent = true;
        ImGui::EndDisabled();
        const float textLeft = origin.x + bar + gap;
        const ImRect pathRect(textLeft, origin.y, std::max(textLeft + 1.0f, origin.x + width), origin.y + bar);
        ControlTextScope text(m);
        const FittedText shown = FitText(state.folder.c_str(), nullptr, pathRect.GetWidth(), TruncateMode::Middle);
        if (ImGui::IsRectVisible(pathRect.Min, pathRect.Max))
            window->DrawList->AddText(ImVec2(textLeft, CenteredTextY(origin.y, bar)), ToU32(c.labelSecondary), shown.text);
        const bool pathHovered = ImGui::IsMouseHoveringRect(pathRect.Min, pathRect.Max) && ImGui::IsWindowHovered();
        if (shown.truncated && BeginPopupTooltip(ImHashStr("##path", 0, id), pathRect, pathHovered, false))
        {
            facet::Text(TextStyle::Footnote, "%s", state.folder.c_str());
            EndPopupTooltip();
        }
    }

    // The list, framed like the inline lists.
    const ImRect listFrame(origin.x, origin.y + bar + gap, origin.x + width, origin.y + bar + gap + listHeight);
    StrokeRect(window->DrawList, listFrame.Min, listFrame.Max, ToU32(ControlBoundaryColor(c)), m.radius, HairlineThickness());
    ImGui::SetCursorScreenPos(listFrame.Min);
    FileBrowserRows rows{&state};
    ListViewDesc view;
    view.rowCount = ready ? static_cast<int>(state.shown.size()) : 0;
    view.areaSize = listFrame.GetSize();
    view.choice = &state.choice;
    view.pill = true;
    view.entry = &FileBrowserEntryInfo;
    view.user = &rows;
    view.state = state.data == FileBrowserData::Loading ? DataState::Loading
                 : state.data == FileBrowserData::Error  ? DataState::Error
                                                         : DataState::Ready;
    view.errorText = strings.loadFailed;
    const ImGuiID listId = ImHashStr("##entries", 0, id);
    bool listFocused = false;
    if (BeginListView(listId, view))
    {
        RowRange range;
        while (NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                const ListRowState row = ListViewRow(i);
                listFocused = listFocused || row.focused;
                const FileBrowserEntry& entry = state.entries[static_cast<std::size_t>(state.shown[static_cast<std::size_t>(i)])];
                ChoiceItem item;
                item.label = entry.name.c_str();
                item.icon = Icon(entry.folder ? "folder" : "file");
                ListsRowLook look;
                look.selected = row.selected;
                DrawListsRow(row, item, look, ControlSize::Inherit);
            }
        }
        const ListViewResult picked = EndListView();
        if (picked.activated >= 0)
            navigateRow = picked.activated;
        if (picked.changed && saveMode)
        {
            // A file chosen in Save mode gives the field its name.
            if (const FileBrowserEntry* entry = chosenEntry(); entry != nullptr && !entry->folder)
                std::snprintf(state.name, sizeof(state.name), "%s", entry->name.c_str());
        }
    }
    // Backspace in the list goes to the parent folder.
    if (state.listFocused || listFocused)
    {
        ImGui::SetKeyOwner(ImGuiKey_Backspace, listId);
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, ImGuiInputFlags_Repeat, listId) && !state.parent.empty())
            navigateParent = true;
    }
    state.listFocused = listFocused;

    // The bottom bar: the name field (Save), the file type dropdown, the confirm button.
    float y = listFrame.Max.y + gap;
    if (saveMode)
    {
        char nameLabel[96];
        std::snprintf(nameLabel, sizeof(nameLabel), "%s###name", strings.fileName);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
        TextField(nameLabel, state.name, sizeof(state.name), {.width = width / Scale(), .readOnly = readOnly});
        if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
            confirm = true;
        y += bar + gap;
    }
    const char* confirmText = options.mode == FileBrowserMode::Save     ? strings.save
                              : options.mode == FileBrowserMode::Folder ? strings.chooseFolder
                                                                        : strings.open;
    {
        ControlTextScope text(m);
        float buttonWidth = std::ceil(ImGui::CalcTextSize(confirmText).x + m.paddingX * 2.0f);
        const int typeCount = std::min(static_cast<int>(options.fileTypes.size()), kFileBrowserTypeCapacity);
        const bool types = typeCount > 0 && options.mode != FileBrowserMode::Folder;
        // In a narrow browser the type dropdown takes a line of its own above the button, and a
        // button wider than the browser is cut to its width (its label then ends in an ellipsis).
        const bool stacked = types && width - buttonWidth - gap < m.height * 3.0f;
        buttonWidth = std::min(buttonWidth, width);
        if (types)
        {
            const char* labels[kFileBrowserTypeCapacity] = {};
            for (int i = 0; i < typeCount; ++i)
            {
                const char* name = options.fileTypes[static_cast<std::size_t>(i)].label;
                labels[i] = name != nullptr ? name : "";
            }
            char typeLabel[96];
            std::snprintf(typeLabel, sizeof(typeLabel), "%s###type", strings.fileType);
            ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
            const float dropdownWidth = stacked ? width : width - buttonWidth - gap;
            Dropdown(typeLabel, &state.fileType, std::span<const char* const>(labels, static_cast<std::size_t>(typeCount)),
                     {.width = dropdownWidth / Scale()});
            if (stacked)
                y += bar + gap;
        }
        // What the confirm button would write.
        const FileBrowserEntry* entry = chosenEntry();
        if (ready)
        {
            switch (options.mode)
            {
            case FileBrowserMode::Open:
                if (entry != nullptr && !entry->folder)
                    FileBrowserJoin(&target, state.folder, entry->name.c_str());
                break;
            case FileBrowserMode::Save:
                if (state.name[0] != '\0')
                    FileBrowserJoin(&target, state.folder, state.name);
                break;
            case FileBrowserMode::Folder:
                FileBrowserJoin(&target, state.folder, entry != nullptr ? entry->name.c_str() : nullptr);
                break;
            }
        }
        const bool fits = path != nullptr || (buffer != nullptr && target.size() + 1 <= bufferSize);
        const bool canConfirm = !readOnly && !target.empty() && fits;
        char confirmLabel[96];
        std::snprintf(confirmLabel, sizeof(confirmLabel), "%s###confirm", confirmText);
        ImGui::SetCursorScreenPos(ImVec2(std::max(origin.x, origin.x + width - buttonWidth), y));
        ImGui::BeginDisabled(!canConfirm);
        if (Button(confirmLabel, {.variant = ButtonVariant::Primary, .width = buttonWidth / Scale()}))
            confirm = true;
        ImGui::EndDisabled();

        // Activating a row: a folder opens; a file confirms (Save takes its name first).
        if (navigateRow >= 0 && navigateRow < static_cast<int>(state.shown.size()))
        {
            const FileBrowserEntry& row = state.entries[static_cast<std::size_t>(state.shown[static_cast<std::size_t>(navigateRow)])];
            if (row.folder)
            {
                std::string folder;
                FileBrowserJoin(&folder, state.folder, row.name.c_str());
                FileBrowserLoad(id, state, FileBrowserJob::List, folder);
                target.clear();
            }
            else if (options.mode != FileBrowserMode::Folder)
            {
                if (saveMode)
                    std::snprintf(state.name, sizeof(state.name), "%s", row.name.c_str());
                FileBrowserJoin(&target, state.folder, row.name.c_str());
                confirm = true;
            }
        }
        if (confirm && !readOnly && !target.empty() && (path != nullptr || target.size() + 1 <= bufferSize))
        {
            if (path != nullptr)
            {
                if (*path != target)
                {
                    path->assign(target);
                    written = true;
                }
            }
            else if (std::strncmp(buffer, target.c_str(), bufferSize) != 0)
            {
                std::memcpy(buffer, target.c_str(), target.size() + 1);
                written = true;
            }
        }
    }
    if (navigateParent && !state.parent.empty())
        FileBrowserLoad(id, state, FileBrowserJob::List, std::string(state.parent));

    // The browser as one block.
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(ImVec2(width, y + bar - origin.y));
    ImGui::PopID();
    ImGui::EndGroup();
    const ImRect panel(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    FinishWidgetGroup(id);
    // The visible label part is the panel's tooltip (pickers.h), on hover and keyboard focus.
    const char* end = ImGui::FindRenderedTextEnd(label);
    const bool hovered = ImGui::IsMouseHoveringRect(listFrame.Min, listFrame.Max, false) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    if (end != label && BeginPopupTooltip(id, listFrame, hovered, listFocused && KeyboardModality()))
    {
        facet::Text(TextStyle::Footnote, "%.*s", static_cast<int>(end - label), label);
        EndPopupTooltip();
    }
    (void)panel;
    if (written)
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
    return written;
}

} // namespace

bool FileBrowser(const char* label, std::string* path, const FileBrowserOptions& options)
{
    try
    {
        return FileBrowserImpl(label, path, nullptr, 0, options);
    }
    catch (...)
    {
        return false;  // no exception leaves the public API (rule 14)
    }
}

bool FileBrowser(const char* label, char* buffer, std::size_t bufferSize, const FileBrowserOptions& options)
{
    try
    {
        return FileBrowserImpl(label, nullptr, buffer, bufferSize, options);
    }
    catch (...)
    {
        return false;
    }
}

namespace internal
{

bool FileBrowserViewOf(ImGuiID browserId, FileBrowserView* out)
{
    const FileBrowserState* state =
        static_cast<const FileBrowserState*>(GlobalStateStore().Find(browserId, StateTypeOf<FileBrowserState>()));
    if (state == nullptr || out == nullptr)
        return false;
    out->folder = state->folder;
    out->rows = static_cast<int>(state->shown.size());
    out->state = static_cast<int>(state->data);
    return true;
}

const char* FileBrowserRowName(ImGuiID browserId, int row)
{
    const FileBrowserState* state =
        static_cast<const FileBrowserState*>(GlobalStateStore().Find(browserId, StateTypeOf<FileBrowserState>()));
    if (state == nullptr || row < 0 || row >= static_cast<int>(state->shown.size()))
        return nullptr;
    return state->entries[static_cast<std::size_t>(state->shown[static_cast<std::size_t>(row)])].name.c_str();
}

} // namespace internal

} // namespace facet
