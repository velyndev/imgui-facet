// BeginSettingsRow, EndSettingsRow and SettingsRowResetPressed (layout.h): one labelled row of the
// row layout block (src/internal/row_layout.h), which draws the label, its icon, the description,
// the modified mark and the reset button, and stacks the control under the label when the row is
// too narrow for both.
//
// Rows outside a row group take the label column of the options or the labelColumn token, so in a
// column of settings rows every control starts at one x whatever the label lengths; a label longer
// than its column ends in an ellipsis and shows in full as a tooltip. The rows of one window under one
// id stack top form a column: each keeps free at its end the widest reset slot the column had in the
// last frame (RowOptions::endReserve), so the controls also end in one line (rows indented by a
// dependent group included) and every row of the column stacks at the same width.
//
// The label lines up with the text of the control: the block centers it on a control of the size
// it is given. The row measures the height of its control in each frame (the last item submitted in
// the control slot, when it starts at the slot's top) and keeps it under the row's id; when that
// height is the height of one of the three control sizes, the next frame lines the label up with a
// control of that size, otherwise with the inherited control size (PushControlSize). So a Large
// button in a row gets its label on the button's text baseline without an option for it.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/row_layout.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// The height of the row's control in the last frame, under the row's id.
struct SettingsRowMeasure
{
    float controlHeight = 0.0f;  // pixels; 0 before the first measurement or without a control
    float scale = 0.0f;          // the scale it was measured at
};

// The settings rows of one window under one id stack top form a column. Every row of a column keeps
// free at its end the widest reset slot of the column in the last frame, so their controls end in
// one line and the rows stack at the same width.
struct SettingsRowColumn
{
    int frame = -1;
    float reserve = 0.0f;   // pixels, from the last frame
    float measured = 0.0f;  // pixels, the widest slot of this frame so far
};

struct SettingsRowEntry
{
    bool active = false;  // BeginRow ran
    ImGuiID id = 0;       // the row's id (the label hashed in the parent's id stack)
    ImGuiWindow* window = nullptr;
    internal::RowLayout layout;  // as BeginRow returned it
    ImRect lastBefore;           // the last item's rect when the control slot opened
    internal::LayoutWorkRect work;  // narrowed to the control slot
};

struct SettingsRowStack
{
    SettingsRowEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

SettingsRowStack& Rows()
{
    static SettingsRowStack stack;
    return stack;
}

struct SettingsRowReset
{
    int frame = -1;
    bool pressed = false;
};

SettingsRowReset& LastReset()
{
    static SettingsRowReset reset;
    return reset;
}

internal::RowLayout& LastLayout()
{
    static internal::RowLayout layout;
    return layout;
}

// The control size whose height is `height` (within half a pixel), else Inherit.
ControlSize SettingsRowSizeOfHeight(float height)
{
    const ControlSize sizes[] = {ControlSize::Small, ControlSize::Regular, ControlSize::Large};
    for (const ControlSize size : sizes)
        if (std::fabs(ControlHeight(size) - height) <= 0.5f)
            return size;
    return ControlSize::Inherit;
}

bool SameRect(const ImRect& a, const ImRect& b)
{
    return a.Min.x == b.Min.x && a.Min.y == b.Min.y && a.Max.x == b.Max.x && a.Max.y == b.Max.y;
}

} // namespace

void BeginSettingsRow(const char* label, const SettingsRowOptions& options)
{
    internal::PushScope("BeginSettingsRow");
    SettingsRowStack& stack = Rows();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    SettingsRowEntry& entry = stack.entries[stack.depth++];
    entry = SettingsRowEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";

    entry.id = window->GetID(label);
    entry.window = window;
    const SettingsRowMeasure& measure = internal::GetState<SettingsRowMeasure>(entry.id);
    internal::RowOptions row;
    row.icon = options.icon;
    row.description = options.description;
    row.modified = options.modified;
    row.resettable = options.resetButton;
    row.labelColumn = options.labelWidth > 0.0f ? options.labelWidth : 0.0f;
    // A height measured at another scale is no guide: every control height changed with it.
    row.size = measure.scale == Scale() ? SettingsRowSizeOfHeight(measure.controlHeight) : ControlSize::Inherit;

    // The column this row belongs to, and the room its reset slots take.
    ImGuiContext& g = *GImGui;
    const ImGuiID columnId = ImHashStr("##facet.settingscolumn", 0, window->IDStack.back());
    SettingsRowColumn& column = internal::GetState<SettingsRowColumn>(columnId);
    if (column.frame != g.FrameCount)
    {
        column.reserve = column.frame == g.FrameCount - 1 ? column.measured : 0.0f;
        column.measured = 0.0f;
        column.frame = g.FrameCount;
    }
    const float slot = options.resetButton ? ControlHeight(row.size) + Px(T().spacing.sm) : 0.0f;
    column.measured = std::max(column.measured, slot);
    row.endReserve = column.reserve;

    entry.layout = internal::BeginRow(label, row);
    // Stock items that fill the available width (a width of -FLT_MIN) stop at the slot's end too,
    // before the reset button.
    entry.work = internal::NarrowLayoutWorkRect(window, entry.layout.control.Max.x);
    entry.active = true;
    entry.lastBefore = g.LastItemData.Rect;
}

void EndSettingsRow()
{
    if (!internal::PopScope("BeginSettingsRow"))
        return;
    SettingsRowStack& stack = Rows();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    const SettingsRowEntry entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    ImGuiContext& g = *GImGui;
    internal::RestoreLayoutWorkRect(entry.work);
    if (g.CurrentWindow != entry.window)
        return;  // the caller ended the window inside the row; Dear ImGui reports that itself

    // The control's height: the last item of the slot, when it starts at the slot's top.
    SettingsRowMeasure& measure = internal::GetState<SettingsRowMeasure>(entry.id);
    measure.scale = Scale();
    measure.controlHeight = 0.0f;
    const ImRect last = g.LastItemData.Rect;
    if (!SameRect(last, entry.lastBefore) && std::fabs(last.Min.y - entry.layout.control.Min.y) <= 0.5f)
        measure.controlHeight = last.GetHeight();

    const bool reset = internal::EndRow();
    LastLayout() = entry.layout;
    SettingsRowReset& record = LastReset();
    record.frame = g.FrameCount;
    record.pressed = reset;
}

bool SettingsRowResetPressed()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const SettingsRowReset& record = LastReset();
    return record.frame == GImGui->FrameCount && record.pressed;
}

namespace internal
{

const RowLayout& LastSettingsRowLayout()
{
    return LastLayout();
}

void ClearSettingsRowStack()
{
    SettingsRowStack& stack = Rows();
    for (int i = stack.depth - 1; i >= 0; --i)
        if (stack.entries[i].active)
            RestoreLayoutWorkRect(stack.entries[i].work);
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
