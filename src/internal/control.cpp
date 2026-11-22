// Control anatomy (control.h): sizes, colors, the field frame, fitted labels, the message line
// and the error records ValidationSummary reads.

#include "internal/control.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/loading.h"
#include "internal/popup_host.h"
#include "internal/row_layout.h"

namespace facet::internal
{
namespace
{

// The spinner stroke as a share of the spinner diameter.
constexpr float kSpinnerStrokeShare = 0.125f;

// The error records of this frame and of the previous one. Frames are told apart by Dear ImGui's
// frame count, so no frame hook is needed: the first report or read of a new frame rolls them.
struct FieldErrors
{
    ImVector<FieldErrorRecord> current;
    ImVector<FieldErrorRecord> previous;
    int frame = -1;
};

FieldErrors& Errors()
{
    static FieldErrors errors;
    return errors;
}

void RollFieldErrors()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    FieldErrors& errors = Errors();
    const int frame = ImGui::GetFrameCount();
    if (frame == errors.frame)
        return;
    if (frame == errors.frame + 1)
        errors.previous.swap(errors.current);
    else
        errors.previous.resize(0);  // a gap (or a new context): the old records belong to no frame
    errors.current.resize(0);
    errors.frame = frame;
}

// Copies `text` into `out` (outSize bytes, zero terminated), cut at a whole UTF-8 character.
void CopyCut(char* out, std::size_t outSize, const char* text)
{
    std::size_t length = std::strlen(text);
    if (length >= outSize)
    {
        length = outSize - 1;
        while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80)
            --length;
    }
    std::memcpy(out, text, length);
    out[length] = '\0';
}

} // namespace

// ---- Entry -----------------------------------------------------------------------------------

ImGuiWindow* ControlWindow()
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || !context->WithinFrameScope || context->CurrentWindow == nullptr)
        return nullptr;
    ImGuiWindow* window = context->CurrentWindow;
    return window->SkipItems ? nullptr : window;
}

// ---- Size ------------------------------------------------------------------------------------

ControlMetrics GetControlMetrics(ControlSize size)
{
    const Theme& t = T();
    ControlMetrics m;
    m.size = ResolveControlSize(size);
    m.textStyle = ControlTextStyle(m.size);
    m.height = ControlHeight(m.size);
    const float padding = m.size == ControlSize::Small ? t.spacing.sm : (m.size == ControlSize::Large ? t.spacing.lg : t.spacing.md);
    m.paddingX = std::round(Px(padding));
    m.fieldPadding = std::round(Px(t.field.padding));
    m.iconGap = std::round(Px(t.spacing.sm));
    m.radius = Px(t.radii.control);
    return m;
}

ControlTextScope::ControlTextScope(const ControlMetrics& metrics) : ControlTextScope(metrics.textStyle)
{
}

ControlTextScope::ControlTextScope(TextStyle style)
{
    PushTextStyle(style);
    fontSize_ = ImGui::GetFontSize();
    iconSize_ = std::round(fontSize_);
}

ControlTextScope::~ControlTextScope()
{
    PopTextStyle();
}

// ---- Colors and feedback ----------------------------------------------------------------------

ImVec4 ControlLabelColor(bool disabled)
{
    const Colors& c = T().colors;
    return disabled ? c.labelTertiary : c.label;
}

ImVec4 ControlSecondaryColor(bool disabled)
{
    const Colors& c = T().colors;
    return disabled ? c.labelTertiary : c.labelSecondary;
}

ImVec4 StateFill(const ImVec4& rest, const ImVec4& hover, const ImVec4& pressed, const ItemState& item)
{
    if (item.disabled)
        return T().colors.fill[3];
    // At rest the color is `rest` exactly (a blend by 0 would round through OKLab).
    ImVec4 color = rest;
    if (item.hoverAmount > 0.0f)
        color = Mix(color, hover, item.hoverAmount);
    if (item.pressAmount > 0.0f)
        color = Mix(color, pressed, item.pressAmount);
    return color;
}

ImVec4 NeutralFill(const ItemState& item)
{
    const Colors& c = T().colors;
    return StateFill(c.fill[2], c.fill[1], c.fill[0], item);
}

// ---- Field frame ------------------------------------------------------------------------------

void DrawFieldFrame(ImDrawList* drawList, const ImRect& frame, const FieldFrameLook& look)
{
    const Theme& t = T();
    const Colors& c = t.colors;
    const float radius = Px(t.radii.control);
    ImVec4 fill = look.disabled ? c.fill[3] : (look.readOnly ? WithAlpha(c.fill[3], 0.0f) : Mix(c.fill[2], c.fill[1], look.hoverAmount));
    FillRect(drawList, frame.Min, frame.Max, ToU32(fill), radius);
    ImVec4 outline = look.disabled ? c.separator : ControlBoundaryColor(c);
    if (look.active && !look.disabled)
        outline = c.focusRing;
    if (look.error)
        outline = c.destructive;
    StrokeRect(drawList, frame.Min, frame.Max, ToU32(outline), radius, HairlineThickness());
}

// ---- Text that fits ---------------------------------------------------------------------------

FittedText FitText(const char* text, const char* end, float maxWidth, TruncateMode mode)
{
    FittedText fitted;
    if (text == nullptr)
        return fitted;
    const TruncateResult result =
        TruncateText(ImGui::GetFont(), ImGui::GetFontSize(), text, end, maxWidth, mode, fitted.text, sizeof(fitted.text));
    fitted.width = result.width;
    fitted.truncated = result.truncated;
    return fitted;
}

void CutTextTooltip(const ItemState& item, const char* text, const char* end, bool truncated)
{
    CutTextTooltip(item.id, item.rect, item.hovered, item.focusVisible, text, end, truncated);
}

void CutTextTooltip(ImGuiID id, const ImRect& anchor, bool hovered, bool focusVisible, const char* text,
                    const char* end, bool truncated)
{
    if (!truncated || text == nullptr || id == 0)
        return;
    if (BeginPopupTooltip(id, anchor, hovered, focusVisible))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, T().colors.label);
        PushTextStyle(TextStyle::Footnote);
        ImGui::TextUnformatted(text, end);
        PopTextStyle();
        ImGui::PopStyleColor();
        EndPopupTooltip();
    }
}

// ---- Labels -----------------------------------------------------------------------------------

FrontLabel MeasureFrontLabel(const char* label, ControlSize size, float totalWidth, FrontLabelFit fit)
{
    FrontLabel front;
    if (label == nullptr)
        return front;
    const char* end = ImGui::FindRenderedTextEnd(label);
    if (end == label)
        return front;
    front.text = label;
    front.end = end;
    const float gap = std::round(Px(T().spacing.sm));
    ControlTextScope text(ControlTextStyle(size));
    const float share = std::max(0.0f, std::floor(totalWidth * kFrontLabelMaxShare));
    const float column = RowFrontLabelColumn();
    if (fit == FrontLabelFit::Column && totalWidth >= column + std::round(Px(T().layout.minRowControl)))
    {
        // The whole column, so the control starts at one x in every row; the text is cut to it.
        // Narrower, where a settings row would stack, the label takes its text width as below so
        // the control keeps its room.
        front.shown = FitText(label, end, std::max(0.0f, column - gap));
        front.width = column;
        return front;
    }
    front.shown = FitText(label, end, std::max(0.0f, share - gap));
    front.width = std::round(front.shown.width + gap);
    return front;
}

FrontLabelFit CompactFrontLabelFit(float optionsWidth, const NextItemData& next)
{
    const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    const bool stacked = window != nullptr && !window->DC.ItemWidthStack.empty();
    return optionsWidth != 0.0f || next.hasWidth || stacked ? FrontLabelFit::Column : FrontLabelFit::Text;
}

void DrawFrontLabel(ImDrawList* drawList, const FrontLabel& label, ImVec2 pos, ControlSize size, bool disabled)
{
    if (label.text == nullptr || label.shown.text[0] == '\0')
        return;
    ControlTextScope text(ControlTextStyle(size));
    drawList->AddText(pos, ToU32(ControlLabelColor(disabled)), label.shown.text);
}

AboveLabel MeasureAboveLabel(const char* label, ControlSize size)
{
    AboveLabel above;
    if (label == nullptr)
        return above;
    const char* end = ImGui::FindRenderedTextEnd(label);
    if (end == label)
        return above;
    above.text = label;
    above.end = end;
    ControlTextScope text(ControlTextStyle(size));
    above.height = std::round(text.FontSize() + Px(T().spacing.xs));
    return above;
}

FittedText DrawAboveLabel(ImDrawList* drawList, const AboveLabel& label, ImVec2 pos, float width, ControlSize size,
                          bool disabled, const char* trailing)
{
    ControlTextScope text(ControlTextStyle(size));
    float room = width;
    if (trailing != nullptr && trailing[0] != '\0')
    {
        const float trailingWidth = ImGui::CalcTextSize(trailing).x;
        drawList->AddText(ImVec2(std::round(pos.x + width - trailingWidth), pos.y), ToU32(ControlSecondaryColor(disabled)),
                          trailing);
        room = width - trailingWidth - std::round(Px(T().spacing.sm));
    }
    FittedText shown;
    if (label.text == nullptr)
        return shown;
    shown = FitText(label.text, label.end, std::max(0.0f, room));
    drawList->AddText(pos, ToU32(ControlLabelColor(disabled)), shown.text);
    return shown;
}

// ---- Icons and the loading spinner -------------------------------------------------------------

void DrawControlIcon(ImDrawList* drawList, Icon icon, float x, float centerY, float size, ImU32 color)
{
    DrawIcon(drawList, icon, ImVec2(std::round(x), std::round(centerY - size * 0.5f)), size, color);
}

void DrawControlSpinner(ImDrawList* drawList, ImVec2 center, float diameter, ImU32 color)
{
    const float thickness = std::max(HairlineThickness(), std::round(diameter * kSpinnerStrokeShare));
    DrawSpinner(drawList, ImVec2(std::round(center.x), std::round(center.y)), diameter * 0.5f - thickness * 0.5f, color,
                thickness);
}

// ---- Help and error text under a control ------------------------------------------------------

ControlMessage PickControlMessage(const char* error, const char* helpText)
{
    ControlMessage message;
    if (error != nullptr && error[0] != '\0')
    {
        message.text = error;
        message.error = true;
    }
    else if (helpText != nullptr && helpText[0] != '\0')
    {
        message.text = helpText;
    }
    return message;
}

namespace
{

// The height of the message text without the gap above it; draws it at `at` when `drawList` is set.
float MessageText(const ControlMessage& message, ImVec2 at, float width, bool disabled, ImDrawList* drawList)
{
    ControlTextScope text(TextStyle::Footnote);
    const float iconSize = message.error ? text.IconSize() : 0.0f;
    const float gap = message.error ? Px(T().spacing.xs) : 0.0f;
    const float wrap = std::max(1.0f, width - iconSize - gap);
    const ImVec2 size = ImGui::CalcTextSize(message.text, nullptr, false, wrap);
    if (drawList != nullptr)
    {
        const ImU32 color = ToU32(message.error ? T().colors.destructive : ControlSecondaryColor(disabled));
        if (message.error)
            DrawIcon(drawList, Icon("error"), at, iconSize, color);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(at.x + iconSize + gap, at.y), color, message.text,
                          nullptr, wrap);
    }
    return std::max(size.y, iconSize);
}

} // namespace

float ControlMessageHeight(const ControlMessage& message, float width)
{
    if (!message.IsSet())
        return 0.0f;
    return Px(T().spacing.xs) + MessageText(message, ImVec2(0.0f, 0.0f), width, false, nullptr);
}

float DrawControlMessage(const ControlMessage& message, ImVec2 at, float width, bool disabled, ImGuiID itemId,
                         const ImRect& itemRect)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (!message.IsSet() || context == nullptr || context->CurrentWindow == nullptr)
        return at.y;
    ImGuiWindow* window = context->CurrentWindow;
    const ImVec2 top(at.x, at.y + Px(T().spacing.xs));
    const float bottom = top.y + MessageText(message, top, width, disabled, window->DrawList);
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(at.x + width, bottom));
    if (message.error && itemId != 0)
        ReportFieldError(itemId, itemRect, message.text);
    return bottom;
}

// ---- Reported errors -------------------------------------------------------------------------

void ReportFieldError(ImGuiID itemId, const ImRect& itemRect, const char* message)
{
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || message == nullptr)
        return;
    RollFieldErrors();
    FieldErrorRecord record;
    record.windowId = context->CurrentWindow->ID;
    record.rootWindowId = context->CurrentWindow->RootWindow != nullptr ? context->CurrentWindow->RootWindow->ID
                                                                        : record.windowId;
    record.itemId = itemId;
    record.rect = itemRect;
    CopyCut(record.text, sizeof(record.text), message);
    Errors().current.push_back(record);
}

std::span<const FieldErrorRecord> PreviousFrameFieldErrors()
{
    RollFieldErrors();
    const ImVector<FieldErrorRecord>& previous = Errors().previous;
    return std::span<const FieldErrorRecord>(previous.Data, static_cast<std::size_t>(previous.Size));
}

void ClearFieldErrors()
{
    FieldErrors& errors = Errors();
    errors.current.clear();
    errors.previous.clear();
    errors.frame = -1;
}

} // namespace facet::internal
