// KeybindList: a table of actions with a KeybindCapture per chord column, a conflict mark and a
// reset IconButton, over the shortcut registry (core/input.h) or over the caller's binds. The
// capture fields and the button are the finished widgets; this file adds the columns, the group
// and column headings, the filter, the conflict search and mark, the data states and the writes
// (SetShortcutChords and ResetShortcut for the registry, the bind members for the caller's rows).
//
// With a height the rows scroll inside a scroll area and are laid out inside its padding; the
// column headings above it take the same inset (keybind_list.h).
//
// Layout per row: the label column (the label, cut with an ellipsis and a tooltip, then the
// conflict mark: the warning icon in the warning token and Strings::conflictWith with the other
// action's name in labelSecondary, as far as it fits), one capture column per chord
// (composites.keybindColumnWidth, narrower when the row is narrow) and the reset button, which
// shows only while the row differs from its default. A narrow list shrinks the label column first,
// then the chord columns evenly. Every capture field is a Tab stop; Escape while one listens
// cancels it, Backspace or Delete on a focused field clears that chord.
//
// Conflicts: a chord that another listed action also uses (in the registry: FindChordConflict,
// which knows the scopes; over the caller's rows: an equal primary or alternate chord of another
// row) marks the row. Every row of the pair is marked, each naming the other.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/input.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "facet/widgets/containers.h"
#include "facet/widgets/display.h"
#include "facet/widgets/inputs.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/text_util.h"
#include "widgets/composites/keybind_list.h"

namespace facet
{
namespace
{

// Skeleton rows the loading state shows when it has no row count to follow.
constexpr int kKeybindListLoadingRows = 3;
// The share of the label column a skeleton line takes.
constexpr float kKeybindListSkeletonShare = 0.6f;
// The narrowest chord column, in control heights, before the columns stop shrinking.
constexpr float kKeybindListMinColumnControls = 2.5f;
// The error and empty states take the height of this many rows.
constexpr float kKeybindListStateRows = 2.0f;

// One row as the list shows it.
struct KeybindListRow
{
    const char* label = "";
    const char* group = "";
    ImGuiKeyChord primary = ImGuiKey_None;
    ImGuiKeyChord alternate = ImGuiKey_None;
    bool modified = false;   // differs from its default
    ImGuiID key = 0;         // the row's id under the list
};

// Where the rows come from and where edits go: the registry or the caller's items.
struct KeybindListSource
{
    std::span<KeybindListItem> items;
    bool registry = false;

    int Count() const { return registry ? ShortcutCount() : static_cast<int>(items.size()); }

    bool Get(int index, KeybindListRow* row) const
    {
        if (registry)
        {
            ShortcutInfo info;
            if (!GetShortcut(index, &info))
                return false;
            row->label = info.label != nullptr ? info.label : "";
            row->group = info.group != nullptr ? info.group : "";
            row->primary = info.primary;
            row->alternate = info.alternate;
            row->modified = info.primary != info.defaultPrimary || info.alternate != info.defaultAlternate;
            row->key = info.id.value;
            return true;
        }
        const KeybindListItem& item = items[static_cast<std::size_t>(index)];
        row->label = item.label != nullptr ? item.label : "";
        row->group = item.group != nullptr ? item.group : "";
        row->primary = item.bind.primary;
        row->alternate = item.bind.alternate;
        row->modified = item.bind.primary != item.defaultBind.primary ||
                        item.bind.alternate != item.defaultBind.alternate || item.bind.mode != item.defaultBind.mode;
        row->key = item.id != 0 ? ImHashData(&item.id, sizeof(item.id), 0x4B42u) : ImHashData(&index, sizeof(index), 0);
        return true;
    }

    // Writes the chords of row `index`.
    void Set(int index, ImGuiKeyChord primary, ImGuiKeyChord alternate) const
    {
        if (registry)
        {
            ShortcutInfo info;
            if (GetShortcut(index, &info))
                SetShortcutChords(info.id, primary, alternate);
            return;
        }
        KeybindListItem& item = items[static_cast<std::size_t>(index)];
        item.bind.primary = primary;
        item.bind.alternate = alternate;
    }

    void Reset(int index) const
    {
        if (registry)
        {
            ShortcutInfo info;
            if (GetShortcut(index, &info))
                ResetShortcut(info.id);
            return;
        }
        KeybindListItem& item = items[static_cast<std::size_t>(index)];
        item.bind = item.defaultBind;
    }

    // The label of another action that uses `chord` (row `index` excepted), or null.
    const char* Conflict(int index, ImGuiKeyChord chord) const
    {
        if (chord == ImGuiKey_None)
            return nullptr;
        if (registry)
        {
            ShortcutInfo self;
            if (!GetShortcut(index, &self))
                return nullptr;
            const ShortcutId other = FindChordConflict(chord, self.id);
            ShortcutInfo info;
            if (!other.IsValid() || !GetShortcut(other, &info))
                return nullptr;
            return info.label != nullptr ? info.label : "";
        }
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const KeybindListItem& item = items[static_cast<std::size_t>(i)];
            if (i != index && (item.bind.primary == chord || item.bind.alternate == chord))
                return item.label != nullptr ? item.label : "";
        }
        return nullptr;
    }
};

bool KeybindListMatches(const KeybindListRow& row, const KeybindListOptions& options)
{
    if (options.filter == nullptr || options.filter[0] == '\0')
        return true;
    char primary[64];
    char alternate[64];
    ChordToText(row.primary, primary, sizeof(primary));
    ChordToText(row.alternate, alternate, sizeof(alternate));
    for (const char* text : {row.label, static_cast<const char*>(primary), static_cast<const char*>(alternate)})
    {
        if (text == nullptr || text[0] == '\0')
            continue;
        const bool hit = options.filterSettings != nullptr ? PassesFilter(options.filter, *options.filterSettings, text)
                                                           : internal::FuzzyScore(options.filter, text).matched;
        if (hit)
            return true;
    }
    return false;
}

} // namespace

namespace internal
{

KeybindListLayout KeybindListMeasure(float outerWidth, bool alternate, bool scrolling)
{
    const Theme& t = T();
    const ControlMetrics m = GetControlMetrics(ControlSize::Inherit);
    KeybindListLayout layout;
    // The rows of a list with a height sit inside the scroll area's padding (scroll.cpp: the
    // focus ring outset on every side).
    layout.inset = scrolling ? FocusRingOutset() : 0.0f;
    const float width = std::max(1.0f, outerWidth - layout.inset * 2.0f);
    layout.width = width;
    layout.gap = std::round(Px(t.spacing.sm));
    layout.spacing = ImGui::GetStyle().ItemSpacing.y;
    layout.button = std::round(m.height);
    layout.rowHeight = std::round(m.height);
    layout.labelLine = MeasureText(TextStyle::Body, "").y;
    layout.headingHeight = MeasureText(TextStyle::Footnote, "").y + layout.spacing;
    layout.groupHeight = MeasureText(TextStyle::Headline, "").y + layout.spacing;
    // With a height the overlay scroll bar lies over the end of the rows: the trailing reset button
    // and the fields end before the bar at its hovered thickness and inset, so the bar never covers them.
    layout.barReserve = scrolling ? std::round(Px(t.scroll.barThicknessHover + t.scroll.barInset)) : 0.0f;
    const float rowEnd = std::max(1.0f, width - layout.barReserve);
    const float columns = alternate ? 2.0f : 1.0f;
    const float minColumn = std::round(m.height * kKeybindListMinColumnControls);
    const float room = rowEnd - layout.button - layout.gap * (columns + 1.0f);
    // Side by side while the label column keeps its minimum next to columns of their minimum.
    layout.stacked = room - minColumn * columns < std::round(Px(t.composites.propertyLabelMinWidth));
    if (layout.stacked)
    {
        const float fields = rowEnd - layout.button - layout.gap * columns;
        layout.column = std::max(1.0f, std::floor(fields / columns));
        layout.labelWidth = rowEnd;
        layout.primaryX = 0.0f;
    }
    else
    {
        float column = std::min(std::round(Px(t.composites.keybindColumnWidth)), std::floor(room / (columns + 1.0f)));
        column = std::max(column, std::min(minColumn, std::floor(room / columns)));
        layout.column = column;
        layout.labelWidth = std::max(0.0f, room - column * columns);
        layout.primaryX = layout.labelWidth + layout.gap;
    }
    layout.alternateX = layout.primaryX + layout.column + layout.gap;
    layout.resetX = rowEnd - layout.button;
    return layout;
}

ImRect KeybindListFirstField(const ImRect& list, bool alternate, bool groupHeading, int column, bool scrolling)
{
    const KeybindListLayout layout = KeybindListMeasure(list.GetWidth(), alternate, scrolling);
    float y = std::round(list.Min.y + layout.headingHeight + layout.inset) + (groupHeading ? layout.groupHeight : 0.0f);
    if (layout.stacked)
        y += layout.labelLine + layout.spacing;
    const float x = std::round(list.Min.x + layout.inset) + (column == 0 ? layout.primaryX : layout.alternateX);
    return ImRect(x, y, x + layout.column, y + layout.rowHeight);
}

namespace
{
KeybindListProbe& KeybindListProbeRecord()
{
    static KeybindListProbe probe;
    return probe;
}
} // namespace

const KeybindListProbe& KeybindListLastProbe()
{
    return KeybindListProbeRecord();
}

} // namespace internal

namespace
{

bool KeybindListImpl(const char* id, const KeybindListSource& source, const KeybindListOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const Theme& t = T();
    const Strings& s = GetStrings();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const ImGuiID widgetId = window->GetID(id);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool filtering = options.filter != nullptr && options.filter[0] != '\0';
    const int count = source.Count();

    // ---- Columns ----
    const bool scrolling = options.height != 0.0f;
    const float outerWidth = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const internal::KeybindListLayout layout = internal::KeybindListMeasure(outerWidth, options.alternate, scrolling);
    // Every row length below is the rows' width, inside the scroll area's padding with a height.
    const float width = layout.width;
    const int columns = options.alternate ? 2 : 1;
    const float gap = layout.gap;
    const float column = layout.column;
    const float labelWidth = layout.labelWidth;
    const float rowHeight = layout.rowHeight;
    const float rowGap = std::round(Px(t.spacing.xs));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float primaryX = layout.primaryX;
    const float alternateX = layout.alternateX;
    const float resetX = layout.resetX;

    bool written = false;
    ImGui::BeginGroup();
    internal::ApplyNextItemData(next);
    // The id is an item of its own, so the group has the list's id when nothing inside is active.
    ImGui::PushOverrideID(widgetId);

    internal::KeybindListProbe& probe = internal::KeybindListProbeRecord();
    probe = internal::KeybindListProbe{};
    probe.id = widgetId;

    // ---- Column headings ----
    {
        internal::ControlTextScope text(TextStyle::Footnote);
        const float lineHeight = ImGui::GetTextLineHeight();
        const ImU32 color = ToU32(internal::ControlSecondaryColor(disabled));
        // The headings take the rows' inset, so each stands over its column.
        const float left = std::round(origin.x + layout.inset);  // the rows round their origin too
        const auto heading = [&](float x, float w, const char* label) {
            const internal::FittedText shown = internal::FitText(label, nullptr, w);
            window->DrawList->AddText(ImVec2(left + x, origin.y), color, shown.text);
        };
        if (!layout.stacked)
            heading(0.0f, labelWidth, s.action);
        heading(primaryX, column, s.primaryBind);
        probe.primaryHeadingX = left + primaryX;
        if (options.alternate)
            heading(alternateX, column, s.alternateBind);
        ImGui::Dummy(ImVec2(outerWidth, lineHeight));
    }

    // ---- Rows ----
    bool areaOpen = true;
    if (scrolling)
    {
        ScrollAreaOptions area;
        area.width = outerWidth / Scale();
        area.height = options.height;
        areaOpen = BeginScrollArea("##rows", area);
    }
    if (areaOpen)
    {
        const ImVec2 rowsOrigin(std::round(ImGui::GetCursorScreenPos().x), std::round(ImGui::GetCursorScreenPos().y));
        if (options.state == DataState::Loading)
        {
            const int lines = std::max(1, std::min(count > 0 ? count : kKeybindListLoadingRows, kKeybindListLoadingRows * 2));
            SkeletonOptions skeleton;
            skeleton.width = std::max(1.0f, labelWidth * kKeybindListSkeletonShare) / Scale();
            SkeletonOptions field;
            field.shape = SkeletonShape::Rect;
            field.width = column / Scale();
            for (int i = 0; i < lines; ++i)
            {
                const float y = rowsOrigin.y + static_cast<float>(i) * (rowHeight + rowGap);
                const float lineHeight = MeasureText(TextStyle::Body, "").y;
                ImGui::SetCursorScreenPos(ImVec2(rowsOrigin.x, y + std::round((rowHeight - lineHeight) * 0.5f)));
                Skeleton(skeleton);
                if (!layout.stacked)
                {
                    ImGui::SetCursorScreenPos(ImVec2(rowsOrigin.x + primaryX, y));
                    Skeleton(field);
                }
            }
            ImGui::SetCursorScreenPos(ImVec2(rowsOrigin.x, rowsOrigin.y + static_cast<float>(lines) * (rowHeight + rowGap)));
        }
        else if (options.state == DataState::Error)
        {
            const ImRect rect(rowsOrigin, ImVec2(rowsOrigin.x + width, rowsOrigin.y + rowHeight * kKeybindListStateRows));
            internal::DrawListState(rect, DataState::Error, nullptr, options.errorText);
            ImGui::Dummy(ImVec2(width, rect.GetHeight()));
        }
        else
        {
            const char* previousGroup = nullptr;
            int shown = 0;
            for (int i = 0; i < count; ++i)
            {
                KeybindListRow row;
                if (!source.Get(i, &row) || !KeybindListMatches(row, options))
                    continue;
                // A group heading where the group changes.
                if (options.groups && row.group[0] != '\0' && (previousGroup == nullptr || std::strcmp(previousGroup, row.group) != 0))
                {
                    internal::ControlTextScope text(TextStyle::Headline);
                    const float lineHeight = ImGui::GetTextLineHeight();
                    if (shown > 0)
                        ImGui::Dummy(ImVec2(width, rowGap));
                    const ImVec2 headingAt = ImGui::GetCursorScreenPos();
                    const internal::FittedText fitted = internal::FitText(row.group, nullptr, width);
                    ImGui::GetWindowDrawList()->AddText(headingAt, ToU32(internal::ControlLabelColor(disabled)), fitted.text);
                    ImGui::Dummy(ImVec2(width, lineHeight));
                }
                previousGroup = row.group;
                ++shown;

                ImGui::PushOverrideID(ImHashData(&row.key, sizeof(row.key), widgetId));
                const ImVec2 at(std::round(ImGui::GetCursorScreenPos().x), std::round(ImGui::GetCursorScreenPos().y));
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                // Side by side the label is centered on the fields; stacked it has a line of its own.
                const float labelHeight = layout.stacked ? layout.labelLine : rowHeight;
                const float fieldsY = at.y + (layout.stacked ? layout.labelLine + layout.spacing : 0.0f);
                const float textY = layout.stacked ? at.y : CenteredTextY(at.y, rowHeight);

                // The label column: the label, then the conflict mark as far as it fits.
                const char* conflict = source.Conflict(i, row.primary);
                if (conflict == nullptr)
                    conflict = source.Conflict(i, row.alternate);
                if (labelWidth > 0.0f)
                {
                    float x = at.x;
                    const float end = at.x + labelWidth;
                    {
                        internal::ControlTextScope text(m);
                        const internal::FittedText label = internal::FitText(row.label, nullptr, end - x);
                        drawList->AddText(ImVec2(x, textY), ToU32(internal::ControlLabelColor(disabled)), label.text);
                        const ImRect labelRect(at, ImVec2(at.x + label.width, at.y + labelHeight));
                        const bool hovered = ImGui::IsMouseHoveringRect(labelRect.Min, labelRect.Max) && ImGui::IsWindowHovered();
                        internal::CutTextTooltip(ImGui::GetID("##label"), labelRect, hovered, false, row.label, nullptr,
                                                 label.truncated);
                        x += label.width + gap;
                    }
                    if (conflict != nullptr && x < end)
                    {
                        internal::ControlTextScope text(TextStyle::Footnote);
                        const float icon = text.IconSize();
                        char message[192];
                        const internal::TextToken token{"name", 0, conflict};
                        internal::FormatTokens(message, sizeof(message), s.conflictWith, std::span<const internal::TextToken>(&token, 1));
                        const ImRect markRect(ImVec2(x, at.y), ImVec2(end, at.y + labelHeight));
                        if (x + icon <= end)
                            internal::DrawControlIcon(drawList, Icon("warning"), x, at.y + labelHeight * 0.5f, icon,
                                                      ToU32(disabled ? t.colors.labelTertiary : t.colors.warning));
                        x += icon + std::round(Px(t.spacing.xs));
                        const internal::FittedText shownText = internal::FitText(message, nullptr, std::max(0.0f, end - x));
                        if (shownText.text[0] != '\0')
                            drawList->AddText(ImVec2(x, CenteredTextY(at.y, labelHeight)),
                                              ToU32(internal::ControlSecondaryColor(disabled)), shownText.text);
                        const bool hovered = ImGui::IsMouseHoveringRect(markRect.Min, markRect.Max) && ImGui::IsWindowHovered();
                        if (internal::BeginPopupTooltip(ImGui::GetID("##conflict"), markRect, hovered, false))
                        {
                            Text(TextStyle::Footnote, "%s", message);
                            internal::EndPopupTooltip();
                        }
                    }
                }

                // The chord columns.
                KeybindCaptureOptions capture;
                capture.readOnly = readOnly;
                ImGuiKeyChord chords[2] = {row.primary, row.alternate};
                for (int c = 0; c < columns; ++c)
                {
                    ImGui::SetCursorScreenPos(ImVec2(at.x + (c == 0 ? primaryX : alternateX), fieldsY));
                    if (forced.state != internal::ForcedItemState::None && c == 0 &&
                        (forced.part == i || (forced.part < 0 && shown == 1)))
                        internal::ForceNextItemState(forced.state);
                    ImGui::SetNextItemWidth(column);
                    Keybind bind;
                    bind.primary = chords[c];
                    const bool captured = KeybindCapture(c == 0 ? "##primary" : "##alternate", &bind, capture);
                    if (c == 0 && shown == 1)
                        probe.firstPrimary = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                    if (captured && !readOnly)
                    {
                        chords[c] = bind.primary;
                        source.Set(i, chords[0], chords[1]);
                        written = true;
                    }
                }

                // The reset button, while the row differs from its default.
                if (row.modified && !readOnly)
                {
                    ImGui::SetCursorScreenPos(ImVec2(at.x + resetX, fieldsY));
                    char resetLabel[96];
                    std::snprintf(resetLabel, sizeof(resetLabel), "%s###reset", s.resetToDefault);
                    if (IconButton(resetLabel, Icon("reset")))
                    {
                        source.Reset(i);
                        written = true;
                    }
                }
                ImGui::SetCursorScreenPos(at);
                ImGui::Dummy(ImVec2(width, fieldsY - at.y + rowHeight));
                ImGui::PopID();
            }
            if (shown == 0)
            {
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const ImRect rect(at, ImVec2(at.x + width, at.y + rowHeight * kKeybindListStateRows));
                const char* empty = options.emptyText != nullptr ? options.emptyText : (filtering ? s.noResults : s.empty);
                internal::DrawListState(rect, DataState::Ready, empty, nullptr);
                ImGui::Dummy(ImVec2(width, rect.GetHeight()));
            }
        }
        if (scrolling)
            EndScrollArea();
    }
    ImGui::PopID();
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (written)
        internal::MarkWidgetEdited(widgetId);
    return written;
}

} // namespace

bool KeybindList(const char* id, const KeybindListOptions& options)
{
    KeybindListSource source;
    source.registry = true;
    return KeybindListImpl(id, source, options);
}

bool KeybindList(const char* id, std::span<KeybindListItem> items, const KeybindListOptions& options)
{
    KeybindListSource source;
    source.items = items;
    return KeybindListImpl(id, source, options);
}

} // namespace facet
