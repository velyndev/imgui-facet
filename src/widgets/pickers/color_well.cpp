// ColorWell: a field-like swatch button (the label in front, the swatch and the optional hex text
// in a field frame) that opens a ColorPicker in a popover of the popup host. A click, Enter, Space
// or Alt+Down opens it; the popover follows the well while it is open; Escape closes it and
// restores the color from before it opened, a click outside closes it and keeps the edit, and the
// host gives the focus back to the well. Ctrl+C on the focused well copies the color as hex text,
// Ctrl+V pastes any color text ColorParseText reads. The well is a drag and drop source and target
// of Dear ImGui's color payloads.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/pickers/color_model.h"

namespace facet
{
namespace
{

// The swatch is this many control heights wide: wide enough to read the color at a glance,
// narrow enough for a table cell.
constexpr float kWellSwatchInControls = 1.5f;
// The longest hex text, measured for the default width.
constexpr const char* kWellHexSample = "#DDDDDDDD";
constexpr const char* kWellHexSampleOpaque = "#DDDDDD";

struct ColorWellState
{
    ImVec4 before = {};  // the color when the popover opened, for Escape
    bool open = false;   // the popover was open in the last frame
};

} // namespace

bool ColorWell(const char* label, ImVec4* color, const ColorWellOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope text(m);
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || color == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const ImVec4 value = color != nullptr ? *color : t.colors.accent;
    ColorWellState& state = internal::GetState<ColorWellState>(id);

    // Measure: the label in front, then the frame with the swatch and the hex text.
    const float inset = std::round(Px(t.spacing.xs));
    const float swatchWidth = std::round(m.height * kWellSwatchInControls);
    const float hexWidth = options.showHex
                               ? std::ceil(ImGui::CalcTextSize(options.alpha ? kWellHexSample : kWellHexSampleOpaque).x)
                               : 0.0f;
    const float content = swatchWidth + (options.showHex ? m.iconGap + hexWidth + m.fieldPadding - inset : 0.0f);
    const internal::FrontLabelFit fit = internal::CompactFrontLabelFit(options.width, next);
    const internal::FrontLabel natural = internal::MeasureFrontLabel(label, m.size, FLT_MAX, fit);
    const float width = std::round(internal::ResolveItemWidth(options.width, next, natural.width + content));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width, fit);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect frame(origin.x + std::round(front.width), origin.y, origin.x + width, origin.y + m.height);

    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, m.height));
    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.readOnly = readOnly;
    itemOptions.mixed = options.mixed;
    itemOptions.baseline = CenteredTextY(origin.y, m.height) - origin.y;
    const internal::ItemState item = internal::PartBehavior(id, frame, itemOptions);
    bool written = false;
    ImVec4 edited = value;
    const auto write = [&](const ImVec4& next, bool withAlpha) {
        if (readOnly)
            return;
        const ImVec4 out = internal::ColorForWrite(next, edited, options.alpha && withAlpha, options.hdr);
        if (!internal::ColorSame(out, edited, true))
        {
            edited = out;
            written = true;
        }
    };

    // Copy and paste on the focused well.
    const bool focused = item.focused && g.NavId == id && !item.disabled && item.forced == internal::ForcedItemState::None;
    if (focused && g.IO.KeyCtrl)
    {
        ImGui::SetKeyOwner(ImGuiKey_C, id);
        ImGui::SetKeyOwner(ImGuiKey_V, id);
        if (ImGui::IsKeyPressed(ImGuiKey_C, ImGuiInputFlags_None, id))
        {
            char hex[16];
            internal::ColorFormatHex(hex, sizeof(hex), value, options.alpha);
            ImGui::SetClipboardText(hex);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V, ImGuiInputFlags_None, id) && !readOnly)
        {
            ImVec4 parsed;
            bool parsedAlpha = false;
            if (internal::ColorParseText(ImGui::GetClipboardText(), &parsed, &parsedAlpha))
                write(parsed, parsedAlpha);
        }
    }

    // Drag and drop of Dear ImGui's color payloads.
    if (!item.disabled && item.forced == internal::ForcedItemState::None)
    {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            if (options.alpha)
                ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, &value, sizeof(float) * 4, ImGuiCond_Once);
            else
                ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, &value, sizeof(float) * 3, ImGuiCond_Once);
            // The preview is the swatch itself.
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(swatchWidth, m.height));
            internal::DrawColorSwatch(ImGui::GetWindowDrawList(), ImRect(at, ImVec2(at.x + swatchWidth, at.y + m.height)),
                                      value, Px(t.radii.element), true);
            ImGui::EndDragDropSource();
        }
        if (!readOnly && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
            {
                ImVec4 dropped;
                std::memcpy(&dropped, payload->Data, sizeof(float) * 4);
                write(dropped, true);
            }
            else if (const ImGuiPayload* payload3 = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
            {
                ImVec4 dropped = edited;
                std::memcpy(&dropped, payload3->Data, sizeof(float) * 3);
                write(dropped, false);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // The popover with the picker.
    // The level is named under the well, so two wells never share a popover.
    const ImGuiID level = ImHashStr("##popover", 0, id);
    const bool open = internal::IsPopupLevelOpen(level);
    const bool canOpen = !readOnly && !item.disabled && item.forced == internal::ForcedItemState::None;
    bool openNow = canOpen && !open && item.pressed;
    if (canOpen && !open && focused && g.IO.KeyAlt)
    {
        ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, id))
            openNow = true;
    }
    internal::PopupAnchor anchor;
    anchor.rect = frame;
    anchor.side = internal::PopupSide::Below;
    anchor.align = internal::PopupAlign::Start;
    internal::PopupOptions popup;
    popup.takeFocus = true;
    // While the popover is closed, the compare color follows the well, so the opening (and the
    // forced open look of captures) compares with the color the well shows.
    // On the frame it closes the popover still counts as open here, so Escape restores from it.
    if (!open && !state.open)
        state.before = edited;
    if (openNow)
        internal::OpenPopupLevel(level, anchor, popup);
    if (item.open)
        internal::ForcePopupLevelOpen(level, anchor, popup);
    internal::SetPopupLevelAnchor(level, anchor);
    if (internal::BeginPopupLevel(level))
    {
        state.open = true;
        ColorPickerOptions picker;
        picker.alpha = options.alpha;
        picker.hdr = options.hdr;
        picker.original = &state.before;
        picker.swatches = options.swatches;
        picker.width = t.metrics.pickerWidth;
        picker.readOnly = readOnly;
        ImVec4 inPicker = edited;
        // The picker's label names the popover for its tooltip.
        if (ColorPicker("##picker", &inPicker, picker))
            write(inPicker, true);
        internal::EndPopupLevel();
    }
    if (internal::PopupLevelClosedReason(level) == internal::PopupCloseReason::Escape && state.open && !readOnly)
    {
        // Escape takes back every edit made while the popover was open.
        write(state.before, true);
    }
    if (!internal::IsPopupLevelOpen(level))
        state.open = false;

    // Draw.
    if (ImGui::IsRectVisible(ImVec2(origin.x, origin.y), ImVec2(origin.x + width, origin.y + m.height)))
    {
        ImDrawList* drawList = window->DrawList;
        internal::FieldFrameLook look;
        look.hoverAmount = std::max(item.hoverAmount, item.pressAmount);
        look.active = open;
        look.disabled = item.disabled;
        look.readOnly = readOnly;
        look.error = options.error != nullptr;
        internal::DrawFieldFrame(drawList, frame, look);
        const float radius = Px(t.radii.element);
        const float swatchRight = std::min(frame.Max.x - inset, frame.Min.x + inset + swatchWidth - inset * 2.0f);
        const ImRect swatch(frame.Min.x + inset, frame.Min.y + inset, std::max(frame.Min.x + inset + 1.0f, swatchRight),
                            frame.Max.y - inset);
        const bool mixed = (options.mixed || item.mixed) && !written;
        if (mixed)
            internal::DrawColorMixedSwatch(drawList, swatch, radius);
        else if (item.disabled)
            FillRect(drawList, swatch.Min, swatch.Max,
                     ToU32(WithAlpha(internal::ColorOnChecker(options.alpha ? edited : WithAlpha(edited, 1.0f)),
                                     internal::kColorDisabledOpacity)),
                     radius);
        else
            internal::DrawColorSwatch(drawList, swatch, options.alpha ? edited : WithAlpha(edited, 1.0f), radius, true);
        internal::DrawOutline(drawList, swatch, radius, t.colors.separator);
        if (options.showHex)
        {
            char hex[16];
            const char* shown = hex;
            if (mixed)
                shown = GetStrings().mixed;
            else
                internal::ColorFormatHex(hex, sizeof(hex), edited, options.alpha);
            const float x = swatch.Max.x + m.iconGap;
            const float room = frame.Max.x - m.fieldPadding - x;
            if (room > 0.0f)
            {
                const internal::FittedText fitted = internal::FitText(shown, nullptr, room);
                drawList->AddText(ImVec2(x, CenteredTextY(frame.Min.y, m.height)),
                                  ToU32(mixed ? internal::ControlSecondaryColor(item.disabled)
                                              : internal::ControlLabelColor(item.disabled)),
                                  fitted.text);
            }
        }
        if (front.text != nullptr)
            internal::DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size,
                                     item.disabled);
        internal::DrawItemFocusRing(item, m.radius);
    }

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, ImRect(origin, ImVec2(origin.x + width, origin.y + m.height)), item.hovered,
                             item.focusVisible, label, labelEnd, front.shown.truncated);
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), item.disabled, id, frame);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    // Marked on the whole well once its group is closed: the picker in the popover, or Escape while
    // one of its parts is active, writes the value while the well is not the active item.
    if (written)
        internal::MarkWidgetEdited(id);
    if (written && color != nullptr)
        *color = edited;
    return written;
}

} // namespace facet
