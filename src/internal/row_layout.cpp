// Labelled rows (row_layout.h).

#include "internal/row_layout.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

// The marker dot of a modified row, as a share of spacing.sm.
constexpr float kMarkerShare = 0.75f;
// The longest label a row truncates without cutting it at the buffer, in bytes.
constexpr int kLabelBuffer = 256;

struct RowFrame
{
    RowLayout layout;
    RowOptions options;
    ImVec2 origin;
    float labelBottom = 0.0f;  // bottom of the label and description block
    float controlHeight = 0.0f;
    float resetSize = 0.0f;    // 0 without a reset slot
    float gap = 0.0f;
    bool navSeenBefore = false;  // the focused item was submitted before the row began
    float textLeft = 0.0f;       // x where the label text and the description start
    float columnRight = 0.0f;    // the right edge of the label text room (before the gap)
    char fullLabel[kLabelBuffer] = {};  // the label's text, for the keyboard tooltip of a cut label
};

struct RowStack
{
    RowFrame rows[kMaxRowDepth];
    int depth = 0;
    int overflow = 0;  // rows begun beyond the capacity, balanced by their EndRow
};

struct GroupFrame
{
    ImGuiID id = 0;
    float columnUnits = 0.0f;    // this frame's column, from the last frame's measurement
    float measuredUnits = 0.0f;  // the widest label plus marker of this frame
    float resetUnits = 0.0f;     // this frame's reset slot, from the last frame's measurement
    float measuredResetUnits = 0.0f;  // the widest reset slot of this frame
    float iconUnits = 0.0f;      // this frame's icon room, from the last frame's measurement
    float measuredIconUnits = 0.0f;   // the widest icon room of this frame
};

struct GroupStack
{
    GroupFrame groups[kMaxRowGroupDepth];
    int depth = 0;
    int overflow = 0;
};

struct RowGroupState
{
    float widestUnits = 0.0f;
    float resetUnits = 0.0f;
    float iconUnits = 0.0f;
};

RowStack& Rows()
{
    static RowStack stack;
    return stack;
}

GroupStack& Groups()
{
    static GroupStack stack;
    return stack;
}

ImU32 Color(const ImVec4& color)
{
    return ToU32(color);
}

float Ascent()
{
    ImFontBaked* baked = ImGui::GetFontBaked();
    if (baked == nullptr || baked->Size <= 0.0f)
        return ImGui::GetFontSize() * 0.8f;
    return baked->Ascent * ImGui::GetFontSize() / baked->Size;
}

} // namespace

float TextTopForBaseline(float baseline)
{
    return std::round(baseline - Ascent());
}

const RowLayout& BeginRow(const char* label, const RowOptions& options)
{
    static const RowLayout kEmpty;
    RowStack& stack = Rows();
    if (label == nullptr)
        label = "";
    if (stack.depth >= kMaxRowDepth)
    {
        if (stack.overflow == 0)
            ReportUsageError("BeginRow nested deeper than %d; the extra rows are laid out as plain content", kMaxRowDepth);
        ++stack.overflow;
        ImGui::PushID(label);
        return kEmpty;
    }

    RowFrame& row = stack.rows[stack.depth++];
    row = {};
    row.options = options;
    const Theme& theme = T();
    const Colors& c = theme.colors;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const ControlSize size = ResolveControlSize(options.size);
    const TextStyle style = ControlTextStyle(size);
    row.controlHeight = ControlHeight(size);
    row.gap = Px(theme.spacing.sm);
    row.origin = window->DC.CursorPos;
    row.resetSize = options.resettable ? row.controlHeight : 0.0f;
    const float available = std::max(0.0f, ImGui::GetContentRegionAvail().x);
    float resetSlot = std::max(options.resettable ? row.resetSize + row.gap : 0.0f, std::max(0.0f, options.endReserve));
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);

    ImGui::BeginGroup();
    ImGui::PushID(label);

    // The label in the control's text style; the marker after it.
    PushTextStyle(style);
    const float markerSize = options.modified ? Px(theme.spacing.sm) * kMarkerShare : 0.0f;
    const float markerSpace = options.modified ? markerSize + Px(theme.spacing.xs) : 0.0f;
    const float labelWidth = ImGui::CalcTextSize(label, labelEnd).x;
    const float lineHeight = ImGui::GetFontSize();
    const float iconSize = std::round(lineHeight);
    float iconSpace = options.icon.IsValid() ? iconSize + Px(theme.spacing.xs) : 0.0f;

    // The label column: options, then the group's shared column, then the token.
    float columnUnits = options.labelColumn;
    GroupStack& groups = Groups();
    if (groups.depth > 0)
    {
        GroupFrame& group = groups.groups[groups.depth - 1];
        // Every row of the group keeps the icon room free when one row has an icon, so the
        // label texts start in one line.
        group.measuredIconUnits = std::max(group.measuredIconUnits, iconSpace / Scale());
        iconSpace = std::max(iconSpace, group.iconUnits * Scale());
        group.measuredUnits = std::max(group.measuredUnits, (iconSpace + labelWidth + markerSpace) / Scale());
        // Every row of the group keeps the widest reset slot free, so the controls end in one line.
        group.measuredResetUnits = std::max(group.measuredResetUnits, resetSlot / Scale());
        resetSlot = std::max(resetSlot, group.resetUnits * Scale());
        if (columnUnits <= 0.0f)
        {
            const float maxUnits = available * kRowGroupMaxShare / Scale();
            columnUnits = std::max(theme.layout.labelColumn, std::min(group.columnUnits, maxUnits));
        }
    }
    const LabeledRowLayout columns = LayoutLabeledRow(available - resetSlot, columnUnits);

    RowLayout& layout = row.layout;
    layout.stacked = columns.stacked;
    // Disabled with its control (ImGui::BeginDisabled around the row): the texts go quiet too.
    layout.disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    const ImU32 labelColor = Color(layout.disabled ? c.labelTertiary : c.label);
    const ImU32 secondaryColor = Color(layout.disabled ? c.labelTertiary : c.labelSecondary);
    const float textRoom = std::max(0.0f, columns.labelWidth - markerSpace - iconSpace);
    char text[kLabelBuffer];
    const TruncateResult truncated =
        TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), label, labelEnd, textRoom, TruncateMode::End, text, sizeof(text));
    layout.truncated = truncated.truncated;
    float labelTop = 0.0f;
    float blockTop = row.origin.y;
    if (columns.stacked)
    {
        labelTop = row.origin.y;
        blockTop = row.origin.y + lineHeight;
    }
    else
    {
        labelTop = CenteredTextY(row.origin.y, row.controlHeight);
        blockTop = row.origin.y + row.controlHeight;
    }
    layout.baseline = labelTop + Ascent();
    row.textLeft = row.origin.x + iconSpace;
    row.columnRight = row.origin.x + columns.labelWidth;
    layout.label = ImRect(row.textLeft, labelTop, row.textLeft + truncated.width, labelTop + lineHeight);
    if (options.icon.IsValid())
        DrawIcon(drawList, options.icon, Snap(ImVec2(row.origin.x, labelTop + (lineHeight - iconSize) * 0.5f)), iconSize,
                 secondaryColor);
    drawList->AddText(ImVec2(row.textLeft, labelTop), labelColor, text);
    if (options.modified)
    {
        const float x = row.textLeft + truncated.width + Px(theme.spacing.xs);
        const float centerY = labelTop + lineHeight * 0.5f;
        drawList->AddCircleFilled(ImVec2(x + markerSize * 0.5f, centerY), markerSize * 0.5f, secondaryColor);
    }
    PopTextStyle();

    // The full label as a tooltip when it was cut: on hover here, on keyboard focus in EndRow.
    if (layout.truncated && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(layout.label.Min, layout.label.Max) &&
        ImGui::GetActiveID() == 0)
        ImGui::SetTooltip("%.*s", static_cast<int>(labelEnd - label), label);
    if (layout.truncated)
    {
        // Kept for EndRow, cut at a whole UTF-8 character when longer than the buffer.
        int length = std::min(static_cast<int>(labelEnd - label), kLabelBuffer - 1);
        if (length < labelEnd - label)
            while (length > 0 && (static_cast<unsigned char>(label[length]) & 0xC0) == 0x80)
                --length;
        std::memcpy(row.fullLabel, label, static_cast<size_t>(length));
        row.fullLabel[length] = '\0';
    }
    row.navSeenBefore = GImGui->NavIdIsAlive;

    // A stacked row puts the description under the label now, across the whole width. Beside a
    // control it waits for EndRow, which knows what sits under the control.
    float blockBottom = columns.stacked ? row.origin.y + lineHeight : row.origin.y + row.controlHeight;
    if (columns.stacked && options.description != nullptr && options.description[0] != '\0')
    {
        PushTextStyle(TextStyle::Footnote);
        const float wrap = std::max(1.0f, row.columnRight - row.textLeft);
        const ImVec2 descSize = ImGui::CalcTextSize(options.description, nullptr, false, wrap);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(row.textLeft, blockTop), secondaryColor,
                          options.description, nullptr, wrap);
        blockBottom = blockTop + descSize.y;
        PopTextStyle();
    }
    row.labelBottom = blockBottom;
    // The label block as an item, so the row's group covers it.
    ImGui::Dummy(ImVec2(columns.stacked ? columns.labelWidth : columns.controlOffset, blockBottom - row.origin.y));

    // The control slot.
    const float slotTop = columns.stacked ? blockBottom + Px(theme.spacing.xs) : row.origin.y;
    const float slotLeft = row.origin.x + columns.controlOffset;
    layout.control = ImRect(slotLeft, slotTop, slotLeft + columns.controlWidth, slotTop + row.controlHeight);
    ImGui::SetCursorScreenPos(layout.control.Min);
    ImGui::BeginGroup();
    ImGui::PushItemWidth(std::max(1.0f, columns.controlWidth));
    return layout;
}

bool EndRow()
{
    RowStack& stack = Rows();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        ImGui::PopID();
        return false;
    }
    if (stack.depth == 0)
    {
        ReportUsageError("EndRow without BeginRow");
        return false;
    }
    RowFrame& row = stack.rows[stack.depth - 1];
    const Theme& theme = T();
    const Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const RowLayout& layout = row.layout;

    ImGui::PopItemWidth();
    ImGui::EndGroup();
    const float controlBottom = std::max(ImGui::GetItemRectMax().y, layout.control.Max.y);
    const float slotWidth = layout.control.GetWidth();

    // The full text of a cut label for keyboard users: while the visible focus is on an item of
    // this slot (Dear ImGui marks the focused item alive when it is submitted), the tooltip shows
    // under the label.
    ImGuiContext& g = *GImGui;
    if (layout.truncated && !row.navSeenBefore && g.NavIdIsAlive && g.NavWindow == g.CurrentWindow && KeyboardModality())
    {
        ImGui::SetNextWindowPos(ImVec2(layout.label.Min.x, layout.label.Max.y + Px(theme.spacing.xs)));
        ImGui::SetTooltip("%s", row.fullLabel);
    }

    // The message under the control: the error, or else the help text.
    const ControlMessage message = PickControlMessage(row.options.error, row.options.helpText);
    if (message.IsSet())
    {
        const float top = controlBottom + Px(theme.spacing.xs);
        const float bottom = DrawControlMessage(message, ImVec2(layout.control.Min.x, controlBottom), slotWidth, layout.disabled);
        ImGui::SetCursorScreenPos(ImVec2(layout.control.Min.x, top));
        ImGui::Dummy(ImVec2(slotWidth, bottom - top));
    }

    // The description of a row beside its control, right under the label line. Nothing under the
    // control: it wraps across the control slot too. Otherwise it keeps to the label column.
    const char* description = row.options.description;
    if (!layout.stacked && description != nullptr && description[0] != '\0')
    {
        const bool controlColumnFree = !message.IsSet() && controlBottom <= layout.control.Max.y + 0.5f;
        const float right = controlColumnFree ? layout.control.Max.x : row.columnRight;
        const float top = row.origin.y + row.controlHeight;
        PushTextStyle(TextStyle::Footnote);
        const float wrap = std::max(1.0f, right - row.textLeft);
        const ImVec2 descSize = ImGui::CalcTextSize(description, nullptr, false, wrap);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(row.textLeft, top),
                          Color(layout.disabled ? c.labelTertiary : c.labelSecondary), description, nullptr, wrap);
        PopTextStyle();
        ImGui::SetCursorScreenPos(ImVec2(row.textLeft, top));
        ImGui::Dummy(ImVec2(wrap, descSize.y));
    }

    // The reset button at the end of the row, while the value is modified.
    bool reset = false;
    if (row.resetSize > 0.0f && row.options.modified)
    {
        const ImVec2 pos(layout.control.Max.x + row.gap, layout.control.Min.y);
        ImGui::SetCursorScreenPos(pos);
        const ItemState item = ItemBehavior(ImGui::GetID("##reset"), ImVec2(row.resetSize, row.resetSize));
        reset = item.pressed;
        ImGui::SetItemTooltip("%s", GetStrings().reset);
        if (item.visible)
        {
            const float radius = Px(theme.radii.control);
            const ImVec4 fill = item.disabled ? c.fill[3] : Mix(c.fill[2], c.fill[1], item.pressAmount);
            FillRect(drawList, item.rect.Min, item.rect.Max, Color(WithAlpha(fill, fill.w * std::max(item.hoverAmount, item.pressAmount))),
                     radius);
            const float iconSize = std::round(Px(theme.type[TextStyle::Body].size));
            const ImVec2 center = item.rect.GetCenter();
            const ImVec2 iconPos = Snap(ImVec2(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f));
            DrawIcon(drawList, Icon("reset"), iconPos, iconSize, Color(item.disabled ? c.labelTertiary : c.labelSecondary));
            DrawItemFocusRing(item, radius);
        }
    }

    ImGui::PopID();
    ImGui::EndGroup();
    --stack.depth;
    return reset;
}

void BeginRowGroup(const char* id)
{
    GroupStack& stack = Groups();
    if (stack.depth >= kMaxRowGroupDepth)
    {
        if (stack.overflow == 0)
            ReportUsageError("BeginRowGroup nested deeper than %d; the extra groups are ignored", kMaxRowGroupDepth);
        ++stack.overflow;
        return;
    }
    GroupFrame& group = stack.groups[stack.depth++];
    group = {};
    group.id = ImGui::GetID(id != nullptr ? id : "");
    const RowGroupState& state = GetState<RowGroupState>(group.id);
    group.columnUnits = state.widestUnits > 0.0f ? state.widestUnits + T().spacing.sm : 0.0f;
    group.resetUnits = state.resetUnits;
    group.iconUnits = state.iconUnits;
}

void EndRowGroup()
{
    GroupStack& stack = Groups();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
    {
        ReportUsageError("EndRowGroup without BeginRowGroup");
        return;
    }
    const GroupFrame& group = stack.groups[--stack.depth];
    RowGroupState& state = GetState<RowGroupState>(group.id);
    state.widestUnits = group.measuredUnits;
    state.resetUnits = group.measuredResetUnits;
    state.iconUnits = group.measuredIconUnits;
}

void EndFrameRows()
{
    RowStack& rows = Rows();
    GroupStack& groups = Groups();
    if (rows.depth + rows.overflow > 0)
        ReportUsageError("BeginRow without EndRow: %d row(s) still open at EndFrame", rows.depth + rows.overflow);
    if (groups.depth + groups.overflow > 0)
        ReportUsageError("BeginRowGroup without EndRowGroup: %d group(s) still open at EndFrame",
                         groups.depth + groups.overflow);
    rows.depth = rows.overflow = 0;
    groups.depth = groups.overflow = 0;
}

namespace
{

struct RowColumnRecord
{
    int frame = -1;
    ImGuiID key = 0;
};

RowColumnRecord& RowColumn()
{
    static RowColumnRecord record;
    return record;
}

} // namespace

ImGuiID RowColumnKey()
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    const RowColumnRecord& record = RowColumn();
    return g != nullptr && record.frame == g->FrameCount ? record.key : 0;
}

ImGuiID SetRowColumnKey(ImGuiID key)
{
    const ImGuiID previous = RowColumnKey();
    const ImGuiContext* g = ImGui::GetCurrentContext();
    RowColumnRecord& record = RowColumn();
    record.frame = g != nullptr ? g->FrameCount : -1;
    record.key = key;
    return previous;
}

float RowFrontLabelColumn()
{
    return std::round(Px(T().layout.labelColumn));
}

namespace
{

struct RowTrailingRecord
{
    ImGuiID key = 0;
    int frame = -1;          // the frame `current` belongs to
    float current = 0.0f;    // the widest column asked for in that frame
    float previous = 0.0f;   // the widest column of the frame before it
};

RowTrailingRecord* RowTrailingRecords()
{
    static RowTrailingRecord records[kRowTrailingColumns];
    return records;
}

} // namespace

float RowTrailingColumn(float right, float width, int kind)
{
    const ImGuiContext* g = ImGui::GetCurrentContext();
    if (g == nullptr || g->CurrentWindow == nullptr)
        return width;
    const int frame = g->FrameCount;
    struct
    {
        ImGuiID window;
        ImGuiID column;
        int right;
        int kind;
    } const keyData{g->CurrentWindow->ID, RowColumnKey(), static_cast<int>(std::lround(right)), kind};
    ImGuiID key = ImHashData(&keyData, sizeof(keyData));
    if (key == 0)
        key = 1;
    RowTrailingRecord* records = RowTrailingRecords();
    RowTrailingRecord* found = nullptr;
    RowTrailingRecord* spare = nullptr;
    for (int i = 0; i < kRowTrailingColumns; ++i)
    {
        RowTrailingRecord& r = records[i];
        if (r.key == key)
        {
            found = &r;
            break;
        }
        // A record nobody used in this frame or the one before is free again.
        if (spare == nullptr && (r.key == 0 || r.frame < frame - 1 || r.frame > frame))
            spare = &r;
    }
    if (found == nullptr)
    {
        if (spare == nullptr)
            return width;
        *spare = RowTrailingRecord{key, frame, 0.0f, 0.0f};
        found = spare;
    }
    if (found->frame != frame)
    {
        found->previous = found->frame == frame - 1 ? found->current : 0.0f;
        found->current = 0.0f;
        found->frame = frame;
    }
    found->current = std::max(found->current, width);
    return std::max(found->previous, found->current);
}

void ClearRowTrailingColumns()
{
    RowTrailingRecord* records = RowTrailingRecords();
    for (int i = 0; i < kRowTrailingColumns; ++i)
        records[i] = RowTrailingRecord{};
}

} // namespace facet::internal
