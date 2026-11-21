// ShortcutSheet (navigation.h): a modal level of the popup host listing the registered shortcuts
// (core/input.h) by group, each with its chords as key caps (key_caps.h). The title row carries
// the visible label part and a close button; under it a search field filters by fuzzy match on
// the label, the group and the chord text (fuzzy.h); the list scrolls in a scroll area (scroll.h)
// that is at most kSheetListShare of the display's work area tall.
//
// Opening and closing: the sheet shows while *open is true. Escape (after it cleared a query in the
// field), the close button and a click on the scrim close it and write *open = false; the focus
// then returns to the item that had it. The search field takes the keyboard focus when it opens.
//
// Groups come in the order their first shortcut was registered, shortcuts in registration order;
// shortcuts without a group come last under no heading. Disabled shortcuts are drawn in
// labelTertiary with disabled caps. No shortcut in the group: Strings::empty; none matching the
// query: Strings::noResults.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/inputs.h"
#include "internal/control.h"
#include "internal/fuzzy.h"
#include "internal/key_caps.h"
#include "internal/popup_host.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/navigation/navigation_parts.h"

namespace facet
{
namespace
{

using namespace internal;

// The tallest the list grows before it scrolls: this share of the display's work area, so the
// title and the search field always stay on screen with the scrim around them.
constexpr float kSheetListShare = 0.5f;
// Shortcuts the sheet lists; later ones are left out.
constexpr int kSheetMaxShortcuts = 256;
constexpr int kSheetQuerySize = 64;
// The empty and no-results text stands in a band this many list rows tall, so the sheet does not
// collapse to a sliver.
constexpr float kSheetEmptyRows = 2.0f;

struct SheetState
{
    char query[kSheetQuerySize] = {};
    bool wasOpen = false;
    bool focusField = false;  // the search field takes the keyboard in the sheet's next frame
};

struct SheetScratch
{
    int order[kSheetMaxShortcuts] = {};  // shortcut indices, grouped
    bool heading[kSheetMaxShortcuts] = {};  // a group heading comes before this entry
    int count = 0;
};

SheetScratch& Scratch()
{
    static SheetScratch scratch;  // one sheet is laid out at a time; no allocation per frame
    return scratch;
}

bool SheetMatches(const char* query, const ShortcutInfo& info)
{
    if (query[0] == '\0')
        return true;
    if (FuzzyScore(query, info.label).matched || FuzzyScore(query, info.group).matched)
        return true;
    char chord[64];
    ChordToText(info.primary, chord, sizeof(chord));
    if (FuzzyScore(query, chord).matched)
        return true;
    ChordToText(info.alternate, chord, sizeof(chord));
    return chord[0] != '\0' && FuzzyScore(query, chord).matched;
}

// Collects the shortcuts to list, grouped; returns how many shortcuts the group filter keeps
// before the query, so the empty state can tell "none in the group" from "none matching".
int CollectSheet(const ShortcutSheetOptions& options, const char* query, SheetScratch& s)
{
    s.count = 0;
    const int total = std::min(ShortcutCount(), kSheetMaxShortcuts);
    int inGroup = 0;
    const auto groupOf = [](int index) {
        ShortcutInfo info;
        return GetShortcut(index, &info) ? info.group : "";
    };
    bool done[kSheetMaxShortcuts] = {};
    for (int i = 0; i < total; ++i)
    {
        if (done[i])
            continue;
        ShortcutInfo first;
        if (!GetShortcut(i, &first))
            continue;
        const char* group = first.group;
        // A named group goes in the pass of its first shortcut; ungrouped ones come last.
        if (group[0] == '\0')
            continue;
        bool heading = true;
        for (int j = i; j < total; ++j)
        {
            if (done[j] || std::strcmp(groupOf(j), group) != 0)
                continue;
            done[j] = true;
            ShortcutInfo info;
            if (!GetShortcut(j, &info))
                continue;
            if (options.group != nullptr && std::strcmp(info.group, options.group) != 0)
                continue;
            ++inGroup;
            if (!SheetMatches(query, info))
                continue;
            s.heading[s.count] = heading;
            s.order[s.count++] = j;
            heading = false;
        }
    }
    for (int j = 0; j < total; ++j)
    {
        ShortcutInfo info;
        if (done[j] || !GetShortcut(j, &info) || info.group[0] != '\0')
            continue;
        if (options.group != nullptr && options.group[0] != '\0')
            continue;
        ++inGroup;
        if (!SheetMatches(query, info))
            continue;
        s.heading[s.count] = false;
        s.order[s.count++] = j;
    }
    return inGroup;
}

void SheetRows(ImGuiID sheetId, const SheetScratch& s, float width)
{
    const Theme& t = T();
    const Colors& c = t.colors;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const float rowHeight = std::round(Px(t.list.rowHeight));
    for (int k = 0; k < s.count; ++k)
    {
        ShortcutInfo info;
        if (!GetShortcut(s.order[k], &info))
            continue;
        if (s.heading[k])
        {
            PushTextStyle(TextStyle::Footnote);
            const ImVec2 at = window->DC.CursorPos;
            const float height = std::round(ImGui::GetFontSize() + Px(t.spacing.md));
            ImGui::Dummy(ImVec2(width, height));
            const FittedText shown = FitText(info.group, nullptr, width);
            drawList->AddText(ImVec2(at.x, std::round(at.y + height - ImGui::GetFontSize() - Px(t.spacing.xs))),
                              ToU32(c.labelSecondary), shown.text);
            PopTextStyle();
        }
        const ImVec2 at = window->DC.CursorPos;
        ImGui::Dummy(ImVec2(width, rowHeight));
        NavigationRecordPart(sheetId, k, ImRect(at, ImVec2(at.x + width, at.y + rowHeight)));
        if (!ImGui::IsRectVisible(at, ImVec2(at.x + width, at.y + rowHeight)))
            continue;
        const bool disabled = !info.enabled;
        const float centerY = at.y + rowHeight * 0.5f;
        const float gap = Px(t.spacing.sm);
        const float primary = MeasureKeyCaps(info.primary, ControlSize::Regular);
        const float alternate = MeasureKeyCaps(info.alternate, ControlSize::Regular);
        float right = at.x + width;
        if (alternate > 0.0f)
        {
            DrawKeyCaps(drawList, info.alternate, ImVec2(right - alternate, centerY), ControlSize::Regular, disabled);
            right -= alternate + gap;
        }
        if (primary > 0.0f)
        {
            DrawKeyCaps(drawList, info.primary, ImVec2(right - primary, centerY), ControlSize::Regular, disabled);
            right -= primary + gap;
        }
        else
        {
            const char* unbound = GetStrings().unbound;
            PushTextStyle(TextStyle::Footnote);
            const float w = ImGui::CalcTextSize(unbound).x;
            drawList->AddText(ImVec2(right - w, CenteredTextY(at.y, rowHeight)), ToU32(ControlSecondaryColor(disabled)), unbound);
            right -= w + gap;
            PopTextStyle();
        }
        PushTextStyle(TextStyle::Body);
        const FittedText shown = FitText(info.label, nullptr, std::max(0.0f, right - at.x));
        drawList->AddText(ImVec2(at.x, CenteredTextY(at.y, rowHeight)), ToU32(ControlLabelColor(disabled)), shown.text);
        PopTextStyle();
        if (shown.truncated)
        {
            const ImRect rect(at, ImVec2(right, at.y + rowHeight));
            CutTextTooltip(ImHashData(&k, sizeof(k), window->ID), rect,
                           ImGui::IsMouseHoveringRect(rect.Min, rect.Max) && GImGui->HoveredWindow == window, false,
                           info.label, nullptr, true);
        }
    }
}

} // namespace

void ShortcutSheet(const char* label, bool* open, const ShortcutSheetOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr || open == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID id = window->GetID(label);
    SheetState& state = GetState<SheetState>(id);
    // Escape and outside closes happen in the host; the flag follows.
    if (PopupLevelClosedReason(id) != PopupCloseReason::None)
        *open = false;
    if (!*open)
    {
        if (IsPopupLevelOpen(id))
            ClosePopupLevel(id);
        state.wasOpen = false;
        return;
    }
    if (!IsPopupLevelOpen(id))
    {
        PopupOptions popup;
        popup.kind = PopupKind::Modal;
        popup.size = ImVec2(std::round(Px(t.overlay.modalWidth)), 0.0f);
        OpenPopupLevel(id, {}, popup);
        if (!state.wasOpen)
        {
            state.query[0] = '\0';
            state.focusField = true;
        }
        state.wasOpen = true;
    }
    if (!BeginPopupLevel(id))
    {
        if (PopupLevelClosedReason(id) != PopupCloseReason::None)
            *open = false;
        return;
    }
    if (PopupLevelScrimClicked(id))
        *open = false;

    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const char* end = ImGui::FindRenderedTextEnd(label);
    // Title row: the label, the close button at the end.
    char closeLabel[96];
    std::snprintf(closeLabel, sizeof(closeLabel), "%s###close", GetStrings().close);
    const float closeSide = ControlHeight(ControlSize::Inherit);
    const ImVec2 top = ImGui::GetCursorScreenPos();
    {
        PushTextStyle(TextStyle::Title3);
        const FittedText shown = FitText(label, end, std::max(0.0f, width - closeSide - Px(t.spacing.sm)));
        ImGui::GetWindowDrawList()->AddText(ImVec2(top.x, CenteredTextY(top.y, closeSide)), ToU32(c.label), shown.text);
        PopTextStyle();
    }
    ImGui::SetCursorScreenPos(ImVec2(top.x + width - closeSide, top.y));
    if (IconButton(closeLabel, Icon("close")))
        *open = false;
    NavigationRecordPart(id, kNavigationPartClose, GImGui->LastItemData.Rect);
    ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + closeSide + Px(t.spacing.md)));

    if (options.searchable)
    {
        // The field takes the keyboard when the sheet opens, so typing filters at once.
        if (state.focusField)
            ImGui::SetKeyboardFocusHere();
        state.focusField = false;
        (void)SearchField("##filter", state.query, sizeof(state.query), {.width = kFill});
        ImGui::SetItemDefaultFocus();
        NavigationRecordPart(id, kNavigationPartFilter, GImGui->LastItemData.Rect);
        ImGui::Dummy(ImVec2(0.0f, Px(t.spacing.xs)));
    }

    SheetScratch& s = Scratch();
    const int inGroup = CollectSheet(options, options.searchable ? state.query : "", s);
    const float rowHeight = std::round(Px(t.list.rowHeight));
    if (s.count == 0)
    {
        // The empty state takes one row.
        const char* text = inGroup == 0 ? GetStrings().empty : GetStrings().noResults;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float height = rowHeight * kSheetEmptyRows;
        ImGui::Dummy(ImVec2(width, height));
        NavigationRecordPart(id, inGroup == 0 ? kNavigationPartEmpty : kNavigationPartNoResults,
                             ImRect(at, ImVec2(at.x + width, at.y + height)));
        PushTextStyle(TextStyle::Body);
        const float w = ImGui::CalcTextSize(text).x;
        ImGui::GetWindowDrawList()->AddText(ImVec2(std::round(at.x + (width - w) * 0.5f), CenteredTextY(at.y, height)),
                                            ToU32(c.labelSecondary), text);
        PopTextStyle();
    }
    else
    {
        float content = 0.0f;
        PushTextStyle(TextStyle::Footnote);
        const float heading = std::round(ImGui::GetFontSize() + Px(t.spacing.md));
        PopTextStyle();
        for (int k = 0; k < s.count; ++k)
            content += rowHeight + (s.heading[k] ? heading : 0.0f);
        const float maxHeight = std::round(ImGui::GetMainViewport()->WorkSize.y * kSheetListShare);
        ScrollAreaDesc desc;
        desc.size = ImVec2(width, std::min(content, maxHeight));
        desc.padding = 0.0f;
        if (BeginScrollAreaCore("##list", desc))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
            SheetRows(id, s, std::max(1.0f, ImGui::GetContentRegionAvail().x));
            ImGui::PopStyleVar();
            EndScrollAreaCore();
        }
    }
    EndPopupLevel();
    if (!*open)
        ClosePopupLevel(id);
}

} // namespace facet
