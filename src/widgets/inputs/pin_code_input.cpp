// PinCodeInput: a one-time code as separate cells. One text field on the text field core takes the
// keyboard, the clipboard and IME for the whole code; it is frameless and draws no text, and this
// file draws the cells from its text: one field frame per cell, the character (or a dot when
// masked) centered, a caret line in the next empty cell, the active look on that cell, and a short
// shake of the cells when an error appears.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// Cells are clamped to the documented range of PinCodeInputOptions::length.
constexpr int kPinMaxCells = 16;
// The shake: this many swings, fading out over motion.durationSlow.
constexpr float kPinShakeSwings = 3.0f;
// Animation sub key of the shake under the widget id (below the item behaviour's keys).
constexpr int kPinShakeSub = 1;

// Keeps digits only, or any visible character but spaces.
int PinCodeCharFilter(ImGuiInputTextCallbackData* data)
{
    const bool numericOnly = *static_cast<const bool*>(data->UserData);
    const ImWchar c = data->EventChar;
    if (numericOnly)
        return (c >= '0' && c <= '9') ? 0 : 1;
    return c > ' ' ? 0 : 1;
}

struct PinCodeLayout
{
    float cell = 0.0f;     // side of a cell, pixels
    int perRow = 1;        // cells in one row
    int rows = 1;
    float width = 0.0f;    // pixels of the widest row
    float height = 0.0f;   // pixels of all rows
};

// The cells laid out in `available` pixels: full size when they fit, shrunk down to a small
// control height, then wrapped onto more rows.
PinCodeLayout PinCodeLayoutFor(int cells, int groupSize, float fullCell, float minCell, float gap, float groupGap,
                               float available)
{
    const auto rowWidth = [&](int count, float cell) {
        float width = static_cast<float>(count) * cell + static_cast<float>(std::max(0, count - 1)) * gap;
        if (groupSize > 0)
            width += static_cast<float>(std::max(0, (count - 1) / groupSize)) * (groupGap - gap);
        return width;
    };
    PinCodeLayout layout;
    layout.perRow = cells;
    const float spare = available - rowWidth(cells, 0.0f);
    layout.cell = std::clamp(std::floor(spare / static_cast<float>(cells)), minCell, fullCell);
    if (rowWidth(cells, layout.cell) > available)
    {
        layout.cell = minCell;
        while (layout.perRow > 1 && rowWidth(layout.perRow, minCell) > available)
            --layout.perRow;
        // Not even one cell of the small size fits: the cells take what there is.
        if (layout.perRow == 1 && minCell > available)
            layout.cell = std::max(1.0f, std::floor(available));
    }
    layout.rows = (cells + layout.perRow - 1) / layout.perRow;
    layout.width = rowWidth(layout.perRow, layout.cell);
    layout.height = static_cast<float>(layout.rows) * layout.cell + static_cast<float>(layout.rows - 1) * gap;
    return layout;
}

template <class Call, class Read>
bool PinCodeCall(const char* label, bool writable, const PinCodeInputOptions& options, Call call, Read read)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const int cells = std::clamp(options.length, 1, kPinMaxCells);
    const ImGuiID id = window->GetID(label);
    const internal::NextItemData next = internal::TakeNextItemData();
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool readOnly = options.readOnly || !writable || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const internal::ControlMetrics metrics = internal::GetControlMetrics(options.size);
    const float gap = Px(t.spacing.xs);
    const float groupGap = Px(t.spacing.md);
    const float fullCell = metrics.height;
    const float minCell = std::min(fullCell, ControlHeight(ControlSize::Small));

    // Width: the label and the cells at full size, at most the available width (rule 6 otherwise).
    const PinCodeLayout full = PinCodeLayoutFor(cells, options.groupSize, fullCell, minCell, gap, groupGap, FLT_MAX);
    const internal::FrontLabelFit fit = internal::CompactFrontLabelFit(options.width, next);
    const internal::FrontLabel measured = internal::MeasureFrontLabel(label, metrics.size, FLT_MAX, fit);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float totalWidth =
        internal::ResolveItemWidth(options.width, next, std::min(available, measured.width + full.width));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, metrics.size, totalWidth, fit);
    const float cellsWidth = std::max(1.0f, totalWidth - front.width);
    const PinCodeLayout layout =
        PinCodeLayoutFor(cells, options.groupSize, fullCell, minCell, gap, groupGap, cellsWidth);

    const ImVec2 origin = window->DC.CursorPos;
    const float cellsLeft = origin.x + front.width;
    const float rowsHeight = std::max(layout.height, metrics.height);
    const ImRect area(ImVec2(cellsLeft, origin.y), ImVec2(cellsLeft + cellsWidth, origin.y + rowsHeight));

    ImGui::BeginGroup();
    ImDrawList* drawList = window->DrawList;
    if (front.text != nullptr)
        internal::DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, metrics.height)), metrics.size,
                                 disabled);

    // The text field: over the first row, frameless and drawing no text.
    bool numericOnly = options.numericOnly;
    internal::TextFieldParams params;
    params.frameless = true;
    params.hideText = true;
    params.labelHidden = true;
    params.password = options.masked;
    params.maxLength = cells;
    params.flags = ImGuiInputTextFlags_CallbackCharFilter;
    params.callback = PinCodeCharFilter;
    params.callbackUser = &numericOnly;
    params.size = options.size;
    params.width = std::min(layout.width, cellsWidth) / Px(1.0f);
    params.readOnly = readOnly;
    ImGui::SetCursorScreenPos(ImVec2(cellsLeft, origin.y + (layout.cell - metrics.height) * 0.5f));
    const internal::TextFieldResult field = call(label, params);
    const bool forcedFocus = Has(field.forced, internal::ForcedItemState::Focused);
    const bool active = field.active || forcedFocus;

    // The code fills from the start: the caret stays at the end unless a range is selected.
    const std::string_view shown = read();
    const char* shownText = shown.data();
    const char* shownEnd = shown.data() + shown.size();
    const int bytes = static_cast<int>(shown.size());
    const int length = std::min(cells, ImTextCountCharsFromUtf8(shownText, shownEnd));
    int selStart = 0;
    int selEnd = 0;
    int caret = 0;
    if (field.active && internal::TextFieldGetSelection(id, &selStart, &selEnd, &caret) && selStart == selEnd &&
        caret != bytes)
        internal::TextFieldSetSelection(id, bytes, bytes);
    // A click anywhere on the cells (other rows included) gives the field the keyboard.
    const bool hovered = !disabled && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(area.Min, area.Max);
    if (hovered && !field.active && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        internal::TextFieldRequestFocus(id, internal::TextFieldFocus::End);

    // The shake: once each time an error appears, unless motion is reduced (the tween jumps then).
    const bool error = options.error != nullptr && options.error[0] != '\0';
    const float shake = Tween(id, kPinShakeSub, error ? 1.0f : 0.0f, t.motion.durationSlow, Ease::Linear);
    const float shakeOffset = error && !ReduceMotion()
                                  ? std::round(Px(t.spacing.xs) * std::sin(shake * kPinShakeSwings * 2.0f *
                                                                           std::numbers::pi_v<float>) *
                                               (1.0f - shake))
                                  : 0.0f;

    // The cells.
    if (ImGui::IsRectVisible(area.Min, area.Max))
    {
        const internal::ControlTextScope scope(metrics);
        const int current = std::min(length, cells - 1);
        for (int i = 0; i < cells; ++i)
        {
            const int row = i / layout.perRow;
            const int column = i % layout.perRow;
            float x = cellsLeft + shakeOffset + static_cast<float>(column) * (layout.cell + gap);
            if (options.groupSize > 0)
                x += static_cast<float>(column / options.groupSize) * (groupGap - gap);
            const float y = origin.y + static_cast<float>(row) * (layout.cell + gap);
            const ImRect cell(ImVec2(x, y), ImVec2(x + layout.cell, y + layout.cell));
            const bool isCurrent = active && i == current;
            internal::DrawFieldFrame(drawList, cell,
                                     {.hoverAmount = hovered ? 1.0f : 0.0f, .active = isCurrent, .disabled = disabled,
                                      .readOnly = readOnly, .error = error});
            const ImVec2 center = cell.GetCenter();
            const ImU32 color = ToU32(internal::ControlLabelColor(disabled));
            if (i < length)
            {
                if (options.masked)
                {
                    drawList->AddCircleFilled(center, Px(t.metrics.statusDotSize) * 0.5f, color);
                }
                else
                {
                    const char* begin = shownText;
                    for (int skip = 0; skip < i; ++skip)
                        begin += std::max(1, ImTextCountUtf8BytesFromChar(begin, shownEnd));
                    const char* end = begin + std::max(1, ImTextCountUtf8BytesFromChar(begin, shownEnd));
                    const ImVec2 size = ImGui::CalcTextSize(begin, end);
                    drawList->AddText(ImVec2(std::round(center.x - size.x * 0.5f), std::round(center.y - size.y * 0.5f)),
                                      color, begin, end);
                }
            }
            else if (isCurrent && field.active)
            {
                const float caretWidth = std::max(1.0f, std::round(Px(t.field.caretWidth)));
                const float half = std::round(ImGui::GetFontSize() * 0.5f);
                FillRect(drawList, ImVec2(std::round(center.x - caretWidth * 0.5f), center.y - half),
                         ImVec2(std::round(center.x - caretWidth * 0.5f) + caretWidth, center.y + half),
                         ToU32(internal::ControlLabelColor(false)));
            }
        }
        const bool focusVisible = (g.NavId == id && internal::KeyboardModality() && !disabled) || forcedFocus;
        if (focusVisible)
            internal::DrawFocusRing(drawList, ImRect(ImVec2(cellsLeft + shakeOffset, origin.y),
                                                     ImVec2(cellsLeft + shakeOffset + layout.width, origin.y + layout.height)),
                                    metrics.radius);
    }

    // The whole widget's space; the message line goes under the cells.
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(ImRect(origin, ImVec2(origin.x + totalWidth, origin.y + rowsHeight)));
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    const float bottom = internal::DrawControlMessage(message, ImVec2(cellsLeft, origin.y + layout.height), cellsWidth,
                                                      disabled, id, area);
    window->DC.CursorMaxPos.y = std::max(window->DC.CursorMaxPos.y, bottom);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return field.edited && writable && !readOnly;
}

} // namespace

bool PinCodeInput(const char* label, std::string* code, const PinCodeInputOptions& options)
{
    return PinCodeCall(
        label, code != nullptr, options,
        [&](const char* l, const internal::TextFieldParams& p) { return internal::TextFieldBehavior(l, code, p); },
        [&]() { return code != nullptr ? std::string_view(*code) : std::string_view(); });
}

bool PinCodeInput(const char* label, char* buffer, std::size_t bufferSize, const PinCodeInputOptions& options)
{
    const bool writable = buffer != nullptr && bufferSize > 0;
    return PinCodeCall(
        label, writable, options,
        [&](const char* l, const internal::TextFieldParams& p) {
            return internal::TextFieldBehavior(l, buffer, bufferSize, p);
        },
        [&]() {
            if (!writable)
                return std::string_view();
            const void* zero = std::memchr(buffer, 0, bufferSize);
            return std::string_view(buffer, zero != nullptr
                                                ? static_cast<std::size_t>(static_cast<const char*>(zero) - buffer)
                                                : bufferSize);
        });
}

} // namespace facet
