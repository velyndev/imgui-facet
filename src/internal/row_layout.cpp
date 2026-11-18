// Labelled rows (row_layout.h).

#include "internal/row_layout.h"

#include <algorithm>
#include <cmath>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
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
    float resetSlot = options.resettable ? row.resetSize + row.gap : 0.0f;
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);

    ImGui::BeginGroup();
    ImGui::PushID(label);

    // The label in the control's text style; the marker after it.
    PushTextStyle(style);
    const float markerSize = options.modified ? Px(theme.spacing.sm) * kMarkerShare : 0.0f;
    const float markerSpace = options.modified ? markerSize + Px(theme.spacing.xs) : 0.0f;
    const float labelWidth = ImGui::CalcTextSize(label, labelEnd).x;

    // The label column: options, then the group's shared column, then the token.
    float columnUnits = options.labelColumn;
    GroupStack& groups = Groups();
    if (groups.depth > 0)
    {
        GroupFrame& group = groups.groups[groups.depth - 1];
        group.measuredUnits = std::max(group.measuredUnits, (labelWidth + markerSpace) / Scale());
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
    const float textRoom = std::max(0.0f, columns.labelWidth - markerSpace);
    char text[kLabelBuffer];
    const TruncateResult truncated =
        TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), label, labelEnd, textRoom, TruncateMode::End, text, sizeof(text));
    layout.truncated = truncated.truncated;
    const float lineHeight = ImGui::GetFontSize();
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
    layout.label = ImRect(row.origin.x, labelTop, row.origin.x + truncated.width, labelTop + lineHeight);
    drawList->AddText(ImVec2(row.origin.x, labelTop), labelColor, text);
    if (options.modified)
    {
        const float x = row.origin.x + truncated.width + Px(theme.spacing.xs);
        const float centerY = labelTop + lineHeight * 0.5f;
        drawList->AddCircleFilled(ImVec2(x + markerSize * 0.5f, centerY), markerSize * 0.5f, secondaryColor);
    }
    PopTextStyle();

    // The full label as a tooltip when it was cut.
    if (layout.truncated && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(layout.label.Min, layout.label.Max) &&
        ImGui::GetActiveID() == 0)
        ImGui::SetTooltip("%.*s", static_cast<int>(labelEnd - label), label);

    // The description under the label, wrapped in the column.
    float blockBottom = columns.stacked ? row.origin.y + lineHeight : row.origin.y + row.controlHeight;
    if (options.description != nullptr && options.description[0] != '\0')
    {
        PushTextStyle(TextStyle::Footnote);
        const float wrap = std::max(1.0f, columns.labelWidth);
        const ImVec2 descSize = ImGui::CalcTextSize(options.description, nullptr, false, wrap);
        const float top = blockTop;
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(row.origin.x, top), secondaryColor,
                          options.description, nullptr, wrap);
        blockBottom = top + descSize.y;
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

    // The message under the control: the error, or else the help text.
    const char* message = row.options.error != nullptr ? row.options.error : row.options.helpText;
    if (message != nullptr && message[0] != '\0')
    {
        const bool error = row.options.error != nullptr;
        PushTextStyle(TextStyle::Footnote);
        const float lineHeight = ImGui::GetFontSize();
        const float top = controlBottom + Px(theme.spacing.xs);
        float textX = layout.control.Min.x;
        const ImU32 color = Color(error ? c.destructive : (layout.disabled ? c.labelTertiary : c.labelSecondary));
        if (error)
        {
            const float iconSize = std::round(lineHeight);
            DrawIcon(drawList, Icon("error"), ImVec2(textX, top), iconSize, color);
            textX += iconSize + Px(theme.spacing.xs);
        }
        const float wrap = std::max(1.0f, layout.control.Max.x - textX);
        const ImVec2 textSize = ImGui::CalcTextSize(message, nullptr, false, wrap);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textX, top), color, message, nullptr, wrap);
        PopTextStyle();
        ImGui::SetCursorScreenPos(ImVec2(layout.control.Min.x, top));
        ImGui::Dummy(ImVec2(slotWidth, std::max(textSize.y, lineHeight)));
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

} // namespace facet::internal
