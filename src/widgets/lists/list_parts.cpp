// Parts the widgets of the lists family share (list_parts.h).

#include "widgets/lists/list_parts.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

// The longest group text compared between neighbouring items; longer texts compare by this prefix.
constexpr int kGroupCompareSize = 128;
// The share of a row's content width the detail text may take before it is cut.
constexpr float kDetailMaxShare = 0.4f;
// The share of a checkbox side its corner radius may take, so a box never reads as a circle.
constexpr float kCheckboxRadiusShare = 0.25f;
// Seeds of the row ids the entries of a display row get when the item has no id of its own.
constexpr ImGuiID kItemIdSeed = 0x4C495354u;    // items
constexpr ImGuiID kHeaderIdSeed = 0x48454144u;  // group headers

bool IsBlank(const char* text)
{
    if (text == nullptr)
        return true;
    for (; *text != '\0'; ++text)
        if (*text != ' ')
            return false;
    return true;
}

void CopyGroup(char* out, const char* group)
{
    if (group == nullptr)
    {
        out[0] = '\0';
        return;
    }
    ImStrncpy(out, group, kGroupCompareSize);
}

// The colors of a checkbox for its state.
struct CheckboxColors
{
    ImVec4 fill;
    ImVec4 outline;
    ImVec4 glyph;
};

CheckboxColors CheckboxLook(bool on, const ItemState& item, bool readOnly)
{
    const Colors& c = T().colors;
    CheckboxColors look;
    if (item.disabled)
    {
        look.fill = c.fill[3];
        look.outline = c.separator;
        look.glyph = on ? c.labelTertiary : WithAlpha(c.labelTertiary, 0.0f);
        return look;
    }
    if (!on)
    {
        const ImVec4 clear = WithAlpha(c.fill[1], 0.0f);
        look.fill = readOnly ? clear : StateFill(clear, c.fill[1], c.fill[0], item);
        look.outline = ControlBoundaryColor(c);
        look.glyph = WithAlpha(c.onAccent, 0.0f);
        return look;
    }
    // Read-only marks are labelSecondary, so a value that cannot change does not claim the accent.
    look.fill = readOnly ? c.labelSecondary : StateFill(c.accent, c.accentHover, c.accentPressed, item);
    look.outline = readOnly ? c.labelSecondary : c.focusRing;
    look.glyph = readOnly ? c.bg : c.onAccent;
    return look;
}

struct ListsFieldState
{
    bool pressStartedOpen = false;  // the mouse press under way began while the list was open
};

// ---- The open list ---------------------------------------------------------------------------

struct ListsPopupState
{
    ListsRows rows;
    int highlight = -1;
    char query[kListsQueryCapacity] = {};
    int builtCount = -1;
    bool built = false;
    ListTypeahead typed;
};

bool RowEnabled(const ListsSource& source, const ListsRows& rows, int row)
{
    if (row < 0 || row >= rows.Count() || rows.IsHeader(row))
        return false;
    if (rows.IsSelectAll(row))
        return true;
    ChoiceItem item;
    return source.Get(rows.rows[row], &item) && !item.disabled;
}

int FirstEnabledRow(const ListsSource& source, const ListsRows& rows)
{
    for (int i = 0; i < rows.Count(); ++i)
        if (RowEnabled(source, rows, i))
            return i;
    return -1;
}

// Whether every shown enabled item is checked (On), none (Off) or some (Mixed).
CheckState SelectAllState(const ListsSource& source, const ListsRows& rows, std::span<const bool> checked)
{
    int on = 0;
    int total = 0;
    for (int i = 0; i < rows.Count(); ++i)
    {
        const int value = rows.rows[i];
        if (value < 0)
            continue;
        ChoiceItem item;
        if (!source.Get(value, &item) || item.disabled)
            continue;
        ++total;
        if (ListsFlag(checked, value))
            ++on;
    }
    if (on == 0 || total == 0)
        return CheckState::Off;
    return on == total ? CheckState::On : CheckState::Mixed;
}

// Checks or clears every shown enabled item; returns whether an entry changed.
bool ApplySelectAll(const ListsSource& source, const ListsRows& rows, std::span<bool> checked, bool value)
{
    bool changed = false;
    for (int i = 0; i < rows.Count(); ++i)
    {
        const int item = rows.rows[i];
        if (item < 0 || item >= static_cast<int>(checked.size()))
            continue;
        ChoiceItem entry;
        if (!source.Get(item, &entry) || entry.disabled)
            continue;
        if (checked[static_cast<std::size_t>(item)] != value)
        {
            checked[static_cast<std::size_t>(item)] = value;
            changed = true;
        }
    }
    return changed;
}

} // namespace

// ---- Item sources ---------------------------------------------------------------------------

bool ListsSource::Get(int index, ChoiceItem* out) const
{
    *out = ChoiceItem();
    if (index < 0 || index >= count)
        return false;
    if (!items.empty())
        *out = items[static_cast<std::size_t>(index)];
    else if (!labels.empty())
        out->label = labels[static_cast<std::size_t>(index)];
    else if (getter == nullptr || !getter(user, index, out))
        return false;
    if (out->label == nullptr)
        out->label = "";
    return true;
}

ListsSource ListsSourceOf(std::span<const ChoiceItem> items)
{
    ListsSource source;
    source.items = items;
    source.count = static_cast<int>(items.size());
    return source;
}

ListsSource ListsSourceOf(std::span<const char* const> labels)
{
    ListsSource source;
    source.labels = labels;
    source.count = static_cast<int>(labels.size());
    return source;
}

ListsSource ListsSourceOf(int count, ChoiceGetter getter, void* user)
{
    ListsSource source;
    source.getter = getter;
    source.user = user;
    source.count = getter != nullptr ? std::max(0, count) : 0;
    return source;
}

// ---- Display rows ---------------------------------------------------------------------------

int ListsRows::ItemAt(int row) const
{
    if (row < 0 || row >= rows.Size || rows[row] == kListsSelectAllRow)
        return -1;
    const int value = rows[row];
    return value >= 0 ? value : -1 - value;
}

int ListsRows::RowOf(int item) const
{
    if (item < 0)
        return -1;
    for (int i = 0; i < rows.Size; ++i)
        if (rows[i] == item)
            return i;
    return -1;
}

void BuildListsRows(ListsRows* out, const ListsSource& source, const ListsRowsDesc& desc)
{
    out->rows.resize(0);
    out->matches.resize(0);
    const bool filtering = !IsBlank(desc.query);
    const std::string_view query = filtering ? std::string_view(desc.query) : std::string_view();
    for (int i = 0; i < source.count; ++i)
    {
        // A missing entry of `exclude` counts as false.
        if (desc.useExclude && ListsFlag(desc.exclude, i) == desc.excludeValue)
            continue;
        ChoiceItem item;
        if (!source.Get(i, &item))
            continue;
        int score = 0;
        if (filtering)
        {
            const char* end = ImGui::FindRenderedTextEnd(item.label);
            const FuzzyMatch match = FuzzyScore(query, std::string_view(item.label, static_cast<size_t>(end - item.label)));
            if (!match.matched)
                continue;
            score = match.score;
        }
        out->matches.push_back({i, score});
    }
    // Best first while filtering an ungrouped list; equal scores keep the item order. std::sort with
    // the index as the tie-break needs no buffer, unlike a stable sort.
    if (filtering && !desc.groups)
        std::sort(out->matches.begin(), out->matches.end(), [](const FuzzyResult& a, const FuzzyResult& b) {
            return a.score != b.score ? a.score > b.score : a.index < b.index;
        });
    if (desc.selectAll && out->matches.Size > 0)
        out->rows.push_back(kListsSelectAllRow);

    char previous[kGroupCompareSize] = {};
    bool hasPrevious = false;
    // Two passes when some items come first (checkedFirst): those, then the others.
    const int passes = desc.first.empty() ? 1 : 2;
    for (int pass = 0; pass < passes; ++pass)
    {
        for (const FuzzyResult& match : out->matches)
        {
            if (passes == 2 && ListsFlag(desc.first, match.index) != (pass == 0))
                continue;
            if (desc.groups)
            {
                ChoiceItem item;
                source.Get(match.index, &item);
                char group[kGroupCompareSize];
                CopyGroup(group, item.group);
                if (group[0] != '\0' && (!hasPrevious || std::strcmp(group, previous) != 0))
                    out->rows.push_back(-1 - match.index);
                std::memcpy(previous, group, sizeof(previous));
                hasPrevious = true;
            }
            out->rows.push_back(match.index);
        }
    }
}

void ListsEntry(void* user, int row, ListEntryInfo* out)
{
    const ListsEntryContext& context = *static_cast<const ListsEntryContext*>(user);
    const ListsRows& rows = *context.rows;
    if (row < 0 || row >= rows.Count())
        return;
    const int value = rows.rows[row];
    if (value == kListsSelectAllRow)
    {
        out->label = GetStrings().selectAll;
        out->id = ImHashStr("##facet.selectAll");
        return;
    }
    if (value < 0)
    {
        out->disabled = true;
        out->id = ImHashData(&value, sizeof(value), kHeaderIdSeed);
        return;
    }
    ChoiceItem item;
    if (!context.source->Get(value, &item))
    {
        out->disabled = true;
        out->id = ImHashData(&value, sizeof(value), kItemIdSeed);
        return;
    }
    out->label = item.label;
    out->disabled = item.disabled;
    out->id = item.id != 0 ? item.id : ImHashData(&value, sizeof(value), kItemIdSeed);
}

void ListsItemEntry(void* user, int index, ListEntryInfo* out)
{
    const ListsSource& source = *static_cast<const ListsSource*>(user);
    ChoiceItem item;
    if (!source.Get(index, &item))
    {
        out->disabled = true;
        return;
    }
    out->label = item.label;
    out->disabled = item.disabled;
    out->id = item.id;
}

// ---- Row content ------------------------------------------------------------------------------

float ListsGlyphSize(ControlSize size)
{
    PushTextStyle(ControlTextStyle(size));
    const float glyph = std::round(ImGui::GetFontSize());
    PopTextStyle();
    return glyph;
}

void DrawListsCheckbox(ImDrawList* drawList, const ImRect& box, CheckState state, const ItemState& item, bool readOnly)
{
    const CheckboxColors look = CheckboxLook(state != CheckState::Off, item, readOnly);
    const float radius = std::min(Px(T().radii.element), std::round(box.GetWidth() * kCheckboxRadiusShare));
    FillRect(drawList, box.Min, box.Max, ToU32(look.fill), radius);
    StrokeRect(drawList, box.Min, box.Max, ToU32(look.outline), radius, HairlineThickness());
    if (state != CheckState::Off)
    {
        // The glyphs are drawn at the box size: their artwork keeps a margin to the box edge. Mixed
        // differs from On by its shape, not by color alone.
        const Icon glyph = state == CheckState::Mixed ? Icon("minus") : Icon("check");
        DrawControlIcon(drawList, glyph, box.Min.x, box.GetCenter().y, box.GetWidth(), ToU32(look.glyph));
    }
}

bool DrawListsRowContent(const ImRect& content, const ItemState& item, const ChoiceItem& entry,
                         const ListsRowLook& look, ControlSize size)
{
    const Theme& theme = T();
    const Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool disabled = item.disabled;
    const float gap = Px(theme.spacing.sm);
    const float glyph = ListsGlyphSize(size);
    const float centerY = std::round(content.GetCenter().y);
    float x = content.Min.x;

    if (look.checkSlot)
    {
        if (look.selected)
            DrawControlIcon(drawList, Icon("check"), x, centerY, glyph, ToU32(disabled ? c.labelTertiary : c.accentText));
        x += glyph + gap;
    }
    if (look.checkbox)
    {
        const float side = Px(theme.metrics.checkboxSize);
        const float top = std::round(centerY - side * 0.5f);
        const ImRect box(std::round(x), top, std::round(x) + side, top + side);
        DrawListsCheckbox(drawList, box, look.check, item, look.readOnly);
        x += side + gap;
    }
    if (entry.icon.IsValid())
    {
        DrawControlIcon(drawList, entry.icon, x, centerY, glyph, ToU32(ControlSecondaryColor(disabled)));
        x += glyph + gap;
    }

    PushPillLabelStyle(ControlTextStyle(size), look.selected);
    float right = content.Max.x;
    if (entry.detail != nullptr && entry.detail[0] != '\0')
    {
        const float room = std::max(0.0f, (right - x) * kDetailMaxShare);
        const FittedText detail = FitText(entry.detail, nullptr, room);
        if (detail.text[0] != '\0')
        {
            drawList->AddText(ImVec2(right - detail.width, CenteredTextY(content.Min.y, content.GetHeight())),
                              ToU32(ControlSecondaryColor(disabled)), detail.text);
            right -= detail.width + gap;
        }
    }
    const char* label = entry.label != nullptr ? entry.label : "";
    const char* end = ImGui::FindRenderedTextEnd(label);
    const FittedText shown = FitText(label, end, std::max(0.0f, right - x));
    const ImVec2 pos(x, CenteredTextY(content.Min.y, content.GetHeight()));
    const ImU32 color = ToU32(ControlLabelColor(disabled));
    if (look.query != nullptr && !IsBlank(look.query) && !disabled)
    {
        // The match stands out by contrast: matched letters in label on a neutral wash, the rest of
        // the label in labelSecondary (still 4.5:1).
        TextRange ranges[kFuzzyMaxRanges];
        const FuzzyMatch match = FuzzyScore(look.query, std::string_view(label, static_cast<size_t>(end - label)), ranges);
        int count = std::min(match.rangeCount, kFuzzyMaxRanges);
        // A cut label ends in the ellipsis: no range reaches into it.
        const int kept = static_cast<int>(std::strlen(shown.text)) - (shown.truncated ? 3 : 0);
        for (int i = 0; i < count; ++i)
            ranges[i].end = std::min(ranges[i].end, std::max(kept, 0));
        while (count > 0 && ranges[count - 1].begin >= ranges[count - 1].end)
            --count;
        const HighlightStyle style{.text = ToU32(c.labelSecondary),
                                   .matchText = ToU32(c.label),
                                   .matchBackground = ToU32(c.fill[1]),
                                   .rounding = Px(theme.radii.element) * 0.5f,
                                   .padding = 0.0f};
        DrawHighlightedText(drawList, ImGui::GetFont(), ImGui::GetFontSize(), pos, shown.text, nullptr,
                            std::span<const TextRange>(ranges, static_cast<size_t>(count)), style);
    }
    else
    {
        drawList->AddText(pos, color, shown.text);
    }
    PopTextStyle();
    return shown.truncated;
}

void DrawListsRow(const ListRowState& row, const ChoiceItem& entry, const ListsRowLook& look, ControlSize size)
{
    bool truncated = false;
    if (row.item.visible)
        truncated = DrawListsRowContent(row.content, row.item, entry, look, size);
    const char* label = entry.label != nullptr ? entry.label : "";
    CutTextTooltip(row.item, label, ImGui::FindRenderedTextEnd(label), truncated);
}

void DrawListsHeader(const ImRect& content, const char* group)
{
    if (group == nullptr || group[0] == '\0')
        return;
    PushTextStyle(TextStyle::Footnote);
    const FittedText shown = FitText(group, nullptr, content.GetWidth());
    ImGui::GetWindowDrawList()->AddText(ImVec2(content.Min.x, CenteredTextY(content.Min.y, content.GetHeight())),
                                        ToU32(T().colors.labelSecondary), shown.text);
    PopTextStyle();
}

void DrawListsSelectAllRow(const ListRowState& row, CheckState state, bool readOnly, ControlSize size)
{
    if (!row.item.visible)
        return;
    ChoiceItem entry;
    entry.label = GetStrings().selectAll;
    ListsRowLook look;
    look.checkbox = true;
    look.check = state;
    look.readOnly = readOnly;
    DrawListsRowContent(row.content, row.item, entry, look, size);
}

// ---- The field of a popup list ------------------------------------------------------------------

ImGuiID ListsLevelId(ImGuiID fieldId)
{
    return ImHashStr("##facet.list", 0, fieldId);
}

PopupAnchor ListsFieldAnchor(const ImRect& frame)
{
    PopupAnchor anchor;
    anchor.rect = frame;
    anchor.side = PopupSide::Below;
    anchor.align = PopupAlign::Start;
    anchor.matchWidth = true;
    return anchor;
}

void ListsClaimListKeys(ImGuiID level, bool tab)
{
    const ImGuiKey keys[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_PageUp,     ImGuiKey_PageDown, ImGuiKey_Home,
                             ImGuiKey_End,     ImGuiKey_Enter,     ImGuiKey_KeypadEnter, ImGuiKey_Space};
    for (const ImGuiKey key : keys)
        ImGui::SetKeyOwner(key, level, ImGuiInputFlags_LockThisFrame);
    if (tab)
        ImGui::SetKeyOwner(ImGuiKey_Tab, level, ImGuiInputFlags_LockThisFrame);
}

ListsFieldResult ListsField(ImGuiID id, const char* label, const ListsFieldDesc& desc)
{
    ListsFieldResult result;
    ImGuiWindow* window = GImGui->CurrentWindow;
    const Theme& theme = T();
    const ControlMetrics m = GetControlMetrics(desc.size);
    ControlTextScope text(m);
    static const NextItemData kNoNext;
    const NextItemData& next = desc.next != nullptr ? *desc.next : kNoNext;
    const float width = ResolveItemWidth(desc.width, next, kFill);

    ImGui::BeginGroup();
    const ImVec2 origin = window->DC.CursorPos;
    const FrontLabel front = MeasureFrontLabel(label, m.size, width);
    const ImRect frame(origin.x + front.width, origin.y, origin.x + width, origin.y + m.height);
    result.frame = frame;

    ItemOptions options;
    options.forced = desc.forced;
    options.readOnly = desc.readOnly;
    options.handCursor = !desc.readOnly;
    options.baseline = CenteredTextY(origin.y, m.height) - origin.y;
    ApplyNextItemData(next);
    const ItemState item = ItemBehavior(id, ImVec2(width, m.height), options);
    result.item = item;
    // A press on the field of an open list closes the list (a click outside it); its release must not
    // open the list again.
    ListsFieldState& fieldState = GetState<ListsFieldState>(id);
    if (item.activated)
        fieldState.pressStartedOpen =
            desc.open || PopupLevelClosedReason(ListsLevelId(id)) == PopupCloseReason::ClickOutside;
    result.openRequest = item.pressed && !item.readOnly && item.forced == ForcedItemState::None &&
                         (item.keyboardPressed || !fieldState.pressStartedOpen);

    const bool disabled = item.disabled;
    bool truncated = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        FieldFrameLook look;
        look.hoverAmount = item.hoverAmount;
        look.active = desc.open || item.open;
        look.disabled = disabled;
        look.readOnly = item.readOnly;
        look.error = desc.error != nullptr;
        DrawFieldFrame(drawList, frame, look);
        DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(frame.Min.y, m.height)), m.size, disabled);

        const float icon = text.IconSize();
        const float centerY = frame.GetCenter().y;
        float x = frame.Min.x + m.fieldPadding;
        float right = frame.Max.x - m.fieldPadding;
        if (!item.readOnly)
        {
            DrawControlIcon(drawList, Icon("chevron-down"), right - icon, centerY, icon,
                            ToU32(ControlSecondaryColor(disabled)));
            right -= icon + m.iconGap;
        }
        if (desc.itemsError)
        {
            DrawControlIcon(drawList, Icon("error"), right - icon, centerY, icon, ToU32(theme.colors.destructive));
            right -= icon + m.iconGap;
        }
        if (desc.icon.IsValid())
        {
            DrawControlIcon(drawList, desc.icon, x, centerY, icon, ToU32(ControlSecondaryColor(disabled)));
            x += icon + m.iconGap;
        }
        const char* value = desc.text != nullptr ? desc.text : "";
        const FittedText shown = FitText(value, desc.textEnd, std::max(0.0f, right - x));
        truncated = shown.truncated;
        const ImVec4 color = desc.secondaryText ? ControlSecondaryColor(disabled) : ControlLabelColor(disabled);
        drawList->AddText(ImVec2(x, CenteredTextY(frame.Min.y, m.height)), ToU32(color), shown.text);
        DrawFocusRing(drawList, frame, m.radius, item.focusAmount);
    }

    // A cut label or value shows in full on hover and keyboard focus (rule 11): the label while the
    // pointer is on it, the value otherwise.
    const bool overLabel = item.hovered && ImGui::GetIO().MousePos.x < frame.Min.x;
    CutTextTooltip(ImHashStr("##facet.label", 0, id), ImRect(origin, ImVec2(frame.Min.x, frame.Max.y)), overLabel,
                   false, front.text, front.end, front.shown.truncated);
    CutTextTooltip(id, frame, item.hovered && !overLabel, item.focusVisible && !desc.open,
                   desc.text != nullptr ? desc.text : "", desc.textEnd, truncated);

    const ControlMessage message = PickControlMessage(desc.error, desc.helpText);
    DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), disabled, id, frame);
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    return result;
}

// ---- Framed inline lists ------------------------------------------------------------------------

ListsBoxLayout BeginListsBox(const char* label, const ListsBoxDesc& desc)
{
    ListsBoxLayout layout;
    ImGuiWindow* window = GImGui->CurrentWindow;
    const Theme& theme = T();
    static const NextItemData kNoNext;
    const NextItemData& next = desc.next != nullptr ? *desc.next : kNoNext;
    layout.width = ResolveItemWidth(desc.width, next, kFill);
    layout.disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const ControlSize size = ResolveControlSize(desc.size);

    ImGui::BeginGroup();
    layout.origin = window->DC.CursorPos;
    const AboveLabel above = MeasureAboveLabel(label, size);
    if (above.height > 0.0f || desc.trailing != nullptr)
    {
        // A trailing text without a label still gets its line.
        const float lineHeight = above.height > 0.0f ? above.height : MeasureAboveLabel("x", size).height;
        DrawAboveLabel(window->DrawList, above, layout.origin, layout.width, size, layout.disabled, desc.trailing);
        layout.header.Min = ImVec2(layout.origin.x, layout.origin.y + lineHeight);
    }
    else
    {
        layout.header.Min = layout.origin;
    }
    layout.header.Max = ImVec2(layout.origin.x + layout.width, layout.header.Min.y + desc.headerHeight);
    const int rows = desc.visibleRows > 0 ? desc.visibleRows : theme.list.inlineRows;
    const float height = ListRowHeight(size) * static_cast<float>(std::max(rows, 1)) + FocusRingOutset() * 2.0f;
    const float top = layout.header.Max.y;
    layout.frame = ImRect(layout.origin.x, top, layout.origin.x + layout.width, top + height);
    layout.areaSize = layout.frame.GetSize();
    if (desc.headerHeight <= 0.0f)
        layout.header = ImRect();

    FieldFrameLook look;
    look.disabled = layout.disabled;
    look.readOnly = !layout.disabled;  // no fill: the rows carry the hover and selection fills
    look.error = desc.error != nullptr;
    DrawFieldFrame(window->DrawList, layout.frame, look);
    window->DC.CursorPos = layout.frame.Min;
    return layout;
}

void EndListsBox(ImGuiID id, const ListsBoxLayout& layout, const ListsBoxDesc& desc)
{
    ImGuiWindow* window = GImGui->CurrentWindow;
    window->DC.CursorPos = ImVec2(layout.frame.Min.x, layout.frame.Max.y);
    ImGui::Dummy(ImVec2(layout.width, 0.0f));
    const ControlMessage message = PickControlMessage(desc.error, desc.helpText);
    DrawControlMessage(message, ImVec2(layout.frame.Min.x, layout.frame.Max.y), layout.width, layout.disabled, id,
                       layout.frame);
    ImGui::EndGroup();
    FinishWidgetGroup(id);
}

// ---- The open list ----------------------------------------------------------------------------

ListsPopupResult ListsPopupList(const ListsPopupDesc& desc)
{
    ListsPopupResult result;
    ImGuiWindow* window = GImGui->CurrentWindow;
    const Theme& theme = T();
    ListsPopupState& state = GetState<ListsPopupState>(desc.level);
    if (desc.opened)
    {
        state.query[0] = '\0';
        state.built = false;
        state.typed.length = 0;
        state.typed.text[0] = '\0';
    }
    const float contentWidth = std::max(1.0f, desc.width - Px(theme.spacing.xs) * 2.0f);
    const bool input = !desc.forced;

    bool filterChanged = false;
    if (desc.searchable)
    {
        const ImGuiID fieldId = window->GetID("##facet.filter");
        if (desc.opened && input)
            TextFieldRequestFocus(fieldId, TextFieldFocus::SelectAll);
        TextFieldParams params;
        params.placeholder = GetStrings().search;
        params.icon = Icon("search");
        params.completionPopup = desc.level;
        params.size = desc.size;
        params.width = contentWidth / Px(1.0f);
        params.frameless = true;
        const TextFieldResult field = TextFieldBehavior("##facet.filter", state.query, sizeof(state.query), params);
        filterChanged = field.edited;
        const float y = window->DC.CursorPos.y - ImGui::GetStyle().ItemSpacing.y * 0.5f;
        HairlineH(window->DrawList, field.frame.Min.x, field.frame.Min.x + contentWidth, y, ToU32(theme.colors.separator));
        if (input && field.active && ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, desc.level))
            result.close = true;
    }

    const ListsEntryContext entries{&desc.source, &state.rows};
    int reveal = -1;
    if (!state.built || filterChanged || state.builtCount != desc.source.count)
    {
        ListsRowsDesc rowsDesc;
        rowsDesc.query = desc.searchable ? state.query : nullptr;
        rowsDesc.groups = true;
        rowsDesc.selectAll = desc.multi && desc.selectAllRow;
        BuildListsRows(&state.rows, desc.source, rowsDesc);
        int highlight = filterChanged ? -1 : state.rows.RowOf(desc.chosen);
        if (highlight < 0)
            highlight = FirstEnabledRow(desc.source, state.rows);
        state.highlight = highlight;
        reveal = std::max(highlight, 0);
        state.built = true;
        state.builtCount = desc.source.count;
    }

    // Typeahead in a list without a filter field moves the active row.
    if (!desc.searchable && input && !desc.opened && state.typed.Update())
    {
        const int target = state.typed.Find(state.rows.Count(), state.highlight, &ListsEntry,
                                            const_cast<ListsEntryContext*>(&entries));
        if (target >= 0)
        {
            state.highlight = target;
            reveal = target;
        }
    }
    const bool spacePressed = !desc.searchable && input && !desc.opened &&
                              ImGui::IsKeyPressed(ImGuiKey_Space, ImGuiInputFlags_None, desc.level);
    // In a multi-select list Tab closes the list instead of toggling the active row.
    const bool tabCloses = desc.multi && input && !desc.opened &&
                           ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_None, desc.level);
    if (tabCloses)
        result.close = true;

    const float rowHeight = ListRowHeight(desc.size);
    const int maxRows = desc.maxVisibleRows > 0 ? desc.maxVisibleRows : theme.list.popupRows;
    const int visibleRows = std::clamp(state.rows.Count(), 1, std::max(1, maxRows));
    ListViewDesc view;
    view.rowCount = state.rows.Count();
    view.size = desc.size;
    view.areaSize = ImVec2(contentWidth, rowHeight * static_cast<float>(visibleRows) + FocusRingOutset() * 2.0f);
    // The level's own surface (a Popup level floats at elevation 2): a fade surface pushed by the
    // page under the field would otherwise reach into the level.
    view.fadeSurface = theme.colors.ElevatedSurface(2);
    view.entry = &ListsEntry;
    view.user = const_cast<ListsEntryContext*>(&entries);
    view.state = desc.state;
    view.emptyText = desc.emptyText != nullptr
                         ? desc.emptyText
                         : (desc.searchable && !IsBlank(state.query) ? GetStrings().noResults : GetStrings().empty);
    view.errorText = desc.errorText;
    view.revealRow = reveal;
    view.readOnly = desc.readOnly;
    view.highlight = &state.highlight;
    view.keyOwner = (input && !desc.opened) ? desc.level : 0;

    if (BeginListView(window->GetID("##facet.rows"), view))
    {
        const char* query = desc.searchable ? state.query : nullptr;
        RowRange range;
        while (NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                const ListRowState row = ListViewRow(i);
                const int value = state.rows.rows[i];
                if (value == kListsSelectAllRow)
                {
                    DrawListsSelectAllRow(row, SelectAllState(desc.source, state.rows, desc.checked), desc.readOnly,
                                          desc.size);
                    continue;
                }
                ChoiceItem item;
                if (value < 0)
                {
                    // A header is not a choice: no hover, and the arrow cursor instead of not-allowed.
                    desc.source.Get(-1 - value, &item);
                    if (row.item.visible)
                        DrawListsHeader(row.content, item.group);
                    if (ImGui::IsMouseHoveringRect(row.rect.Min, row.rect.Max) && ImGui::IsWindowHovered())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                    continue;
                }
                desc.source.Get(value, &item);
                ListsRowLook look;
                look.query = query;
                look.readOnly = desc.readOnly;
                if (desc.multi)
                {
                    look.checkbox = true;
                    look.check = ListsFlag(desc.checked, value) ? CheckState::On : CheckState::Off;
                }
                else
                {
                    look.checkSlot = true;
                    look.selected = value == desc.chosen;
                }
                DrawListsRow(row, item, look, desc.size);
            }
        }
        const ListViewResult list = EndListView();
        int activated = list.activated;
        if (spacePressed && RowEnabled(desc.source, state.rows, state.highlight))
            activated = state.highlight;
        if (activated >= 0 && input && !desc.readOnly && !tabCloses)
        {
            // Rows take the pointer on its release, so an activation without one came from a key.
            result.byKey = !ImGui::IsMouseReleased(ImGuiMouseButton_Left);
            const int value = state.rows.rows[activated];
            if (desc.multi)
            {
                if (value == kListsSelectAllRow)
                {
                    const bool on = SelectAllState(desc.source, state.rows, desc.checked) != CheckState::On;
                    result.changed = ApplySelectAll(desc.source, state.rows, desc.checked, on);
                }
                else if (value >= 0 && value < static_cast<int>(desc.checked.size()))
                {
                    desc.checked[static_cast<std::size_t>(value)] = !desc.checked[static_cast<std::size_t>(value)];
                    result.changed = true;
                }
            }
            else if (value >= 0)
            {
                result.activated = value;
            }
        }
    }
    return result;
}

const char* ListsPopupQuery(ImGuiID level)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return "";
    return GetState<ListsPopupState>(level).query;
}

void ListsCloseByKey(ImGuiID level)
{
    ClosePopupLevel(level);
    ImGui::SetNavCursorVisible(true);
    SetKeyboardModality(true);
}

void ListsMarkEdited()
{
    if (ImGui::GetCurrentContext() != nullptr)
        GImGui->LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
}

} // namespace facet::internal
