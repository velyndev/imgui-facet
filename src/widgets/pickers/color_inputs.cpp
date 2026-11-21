// ColorInputs: one DragNumber per channel of a color model (RGB, HSV or HSL, with alpha), or one
// HexInput, with a Dropdown in front that switches the model. Every field is a DragNumber or
// HexInput of the inputs family, so dragging, stepping, typing and Escape work as they do there.
// The fields are marked with the first letter of their channel name (Strings::colorRed and the
// others). Pasting color text (Ctrl+V on a focused field) reads it with ColorParseText and writes
// the whole color. HSV and HSL fields keep the hue of a grey through the hue memory of
// color_model.h, and a field writes only its own channel, so the others keep their exact values.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/lists.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "widgets/pickers/color_model.h"

namespace facet
{
namespace
{

constexpr int kColorModelCount = 4;
constexpr int kColorInputsMaxFields = 4;
// The field units: degrees of hue, percent, and the bytes of 8-bit channels.
constexpr double kColorDegrees = 360.0;
constexpr double kColorPercent = 100.0;
constexpr double kColorByte = 255.0;
// Characters of a channel letter (one UTF-8 code point and its terminator).
constexpr int kColorLetterSize = 8;

struct ColorInputsState
{
    int model = -1;  // the model the user chose; -1: options.model
};

// The first code point of `name` with "##" and a field id after it, as a DragNumber label.
void ChannelLabel(char* out, std::size_t size, const char* name, const char* id)
{
    char letter[kColorLetterSize] = {};
    if (name != nullptr && name[0] != 0)
    {
        unsigned int c = 0;
        const int length = ImTextCharFromUtf8(&c, name, nullptr);
        const int copy = std::clamp(length, 1, kColorLetterSize - 1);
        std::memcpy(letter, name, static_cast<std::size_t>(copy));
        letter[copy] = 0;
    }
    std::snprintf(out, size, "%s##%s", letter, id);
}

std::uint32_t PackHex(const ImVec4& color, bool alpha)
{
    const std::uint32_t r = static_cast<std::uint32_t>(internal::ColorChannelByte(color.x));
    const std::uint32_t g = static_cast<std::uint32_t>(internal::ColorChannelByte(color.y));
    const std::uint32_t b = static_cast<std::uint32_t>(internal::ColorChannelByte(color.z));
    if (!alpha)
        return (r << 16) | (g << 8) | b;
    const std::uint32_t a = static_cast<std::uint32_t>(internal::ColorChannelByte(color.w));
    return (r << 24) | (g << 16) | (b << 8) | a;
}

// The channels of a hex value written into `color`: a byte that did not change keeps the channel
// as it was, so a color that is not on the 8-bit grid survives an edit of another channel.
ImVec4 UnpackHex(std::uint32_t value, bool alpha, const ImVec4& color)
{
    int bytes[4] = {};
    if (alpha)
    {
        bytes[0] = static_cast<int>((value >> 24) & 0xFFu);
        bytes[1] = static_cast<int>((value >> 16) & 0xFFu);
        bytes[2] = static_cast<int>((value >> 8) & 0xFFu);
        bytes[3] = static_cast<int>(value & 0xFFu);
    }
    else
    {
        bytes[0] = static_cast<int>((value >> 16) & 0xFFu);
        bytes[1] = static_cast<int>((value >> 8) & 0xFFu);
        bytes[2] = static_cast<int>(value & 0xFFu);
        bytes[3] = internal::ColorChannelByte(color.w);
    }
    const float channels[4] = {color.x, color.y, color.z, color.w};
    float out[4];
    for (int i = 0; i < 4; ++i)
        out[i] = internal::ColorChannelByte(channels[i]) == bytes[i] ? channels[i] : static_cast<float>(bytes[i]) / 255.0f;
    return ImVec4(out[0], out[1], out[2], out[3]);
}

// One field of a channel model.
struct ChannelField
{
    const char* id = "";          // the field's id under the widget
    const char* name = nullptr;   // Strings member: the letter in front and the accessible name
    double scale = 1.0;           // field units per channel unit (255, 360, 100, or 1 with floats)
    double max = 1.0;             // in field units
    bool wrap = false;            // the hue
    bool percent = false;         // shows "%" after the number
};

int ModelFields(ColorModel model, bool alpha, bool floats, bool hdr, ChannelField* out)
{
    const Strings& s = GetStrings();
    int count = 0;
    const double byte = floats ? 1.0 : kColorByte;
    const double degrees = kColorDegrees;
    const double share = floats ? 1.0 : kColorPercent;
    const double open = HUGE_VAL;
    switch (model)
    {
    case ColorModel::Rgb:
        out[count++] = {"r", s.colorRed, byte, hdr ? open : byte};
        out[count++] = {"g", s.colorGreen, byte, hdr ? open : byte};
        out[count++] = {"b", s.colorBlue, byte, hdr ? open : byte};
        if (alpha)
            out[count++] = {"a", s.colorAlpha, byte, byte};
        break;
    case ColorModel::Hsv:
        out[count++] = {"h", s.colorHue, degrees, degrees, true};
        out[count++] = {"s", s.colorSaturation, share, share, false, !floats};
        out[count++] = {"v", s.colorValue, share, hdr ? open : share, false, !floats};
        if (alpha)
            out[count++] = {"a", s.colorAlpha, share, share, false, !floats};
        break;
    case ColorModel::Hsl:
        out[count++] = {"h", s.colorHue, degrees, degrees, true};
        out[count++] = {"s", s.colorSaturation, share, share, false, !floats};
        out[count++] = {"l", s.colorLightness, share, share, false, !floats};
        if (alpha)
            out[count++] = {"a", s.colorAlpha, share, share, false, !floats};
        break;
    case ColorModel::Hex:
        break;
    }
    return count;
}

// The channel values of a model, in channel units (0 to 1, hue in turns).
void ModelValues(ColorModel model, const ImVec4& color, const internal::ColorHsv& hsv, const internal::ColorHsl& hsl,
                 double* out)
{
    switch (model)
    {
    case ColorModel::Hsv: out[0] = hsv.h; out[1] = hsv.s; out[2] = hsv.v; break;
    case ColorModel::Hsl: out[0] = hsl.h; out[1] = hsl.s; out[2] = hsl.l; break;
    default: out[0] = color.x; out[1] = color.y; out[2] = color.z; break;
    }
    out[3] = color.w;
}

// The color after field `index` of `model` was set to `channel` (channel units).
ImVec4 ApplyField(ColorModel model, int index, double channel, const ImVec4& color, internal::ColorHsv* hsv,
                  internal::ColorHsl* hsl)
{
    ImVec4 out = color;
    if (index == 3)
    {
        out.w = static_cast<float>(channel);
        return out;
    }
    if (model == ColorModel::Hsv)
    {
        double* parts[3] = {&hsv->h, &hsv->s, &hsv->v};
        *parts[index] = channel;
        if (index == 0 && hsv->h >= 1.0)
            hsv->h = 0.0;
        out = internal::ColorFromHsv(*hsv, color.w);
        return out;
    }
    if (model == ColorModel::Hsl)
    {
        double* parts[3] = {&hsl->h, &hsl->s, &hsl->l};
        *parts[index] = channel;
        if (index == 0 && hsl->h >= 1.0)
            hsl->h = 0.0;
        out = internal::ColorFromHsl(*hsl, color.w);
        return out;
    }
    float* parts[3] = {&out.x, &out.y, &out.z};
    *parts[index] = static_cast<float>(channel);
    return out;
}

const char* ModelName(int model)
{
    const Strings& s = GetStrings();
    switch (model)
    {
    case 1: return s.colorModelHsv;
    case 2: return s.colorModelHsl;
    case 3: return s.colorModelHex;
    default: return s.colorModelRgb;
    }
}

} // namespace

bool ColorInputs(const char* label, ImVec4* color, const ColorInputsOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || color == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool floats = options.floats || options.hdr;
    const ImVec4 value = color != nullptr ? *color : T().colors.accent;

    ColorInputsState& state = internal::GetState<ColorInputsState>(id);
    if (state.model < 0 || state.model >= kColorModelCount)
        state.model = std::clamp(static_cast<int>(options.model), 0, kColorModelCount - 1);
    const ColorModel model = static_cast<ColorModel>(state.model);

    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float gap = std::round(Px(T().spacing.xs));
    internal::FrontLabel front;
    float menuWidth = 0.0f;
    float fieldMin = 0.0f;
    {
        internal::ControlTextScope text(m);
        front = internal::MeasureFrontLabel(label, m.size, width);
        // The narrowest field that shows its letter and a full value.
        const char* sample = options.alpha ? "DDDDDDDD" : "DDDDDD";
        if (model != ColorModel::Hex)
            sample = floats ? "0.000" : "255";
        fieldMin = std::ceil(ImGui::CalcTextSize("W").x + Px(T().spacing.sm) + ImGui::CalcTextSize(sample).x +
                             m.fieldPadding * 2.0f);
        if (options.modeSwitch)
        {
            float widest = 0.0f;
            for (int i = 0; i < kColorModelCount; ++i)
                widest = std::max(widest, ImGui::CalcTextSize(ModelName(i)).x);
            // The text, the chevron and the field paddings of a Dropdown field.
            menuWidth = std::ceil(widest + text.IconSize() + m.iconGap + m.fieldPadding * 2.0f);
        }
    }
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    float x = origin.x + std::round(front.width);
    const float right = origin.x + width;
    ChannelField channelFields[kColorInputsMaxFields];
    const int fieldCount = model == ColorModel::Hex ? 1 : ModelFields(model, options.alpha, floats, options.hdr, channelFields);
    // When the menu and the fields do not fit one line, the menu keeps the first line and the
    // fields take the whole width of a second one.
    const float fieldsNeed = fieldMin * static_cast<float>(fieldCount) + gap * static_cast<float>(fieldCount - 1);
    const bool stacked = menuWidth > 0.0f && right - x - menuWidth - gap < fieldsNeed;
    const float fieldsY = stacked ? origin.y + m.height + gap : origin.y;

    ImGui::BeginGroup();
    ImGui::PushOverrideID(id);
    bool written = false;
    ImVec4 edited = value;
    internal::ColorHsv hsv = internal::ColorTrackHsv(id, value);
    internal::ColorHsl hsl = internal::ColorTrackHsl(id, value);

    // The ids of the fields, for the paste and for the group's last item.
    char labels[kColorInputsMaxFields][32] = {};
    ImGuiID fieldIds[kColorInputsMaxFields] = {};
    if (model == ColorModel::Hex)
    {
        // "#" in front of the digits; the space keeps it apart from the "##" that starts the id.
        std::snprintf(labels[0], sizeof(labels[0]), "# ##hex");
        fieldIds[0] = ImGui::GetID(labels[0]);
    }
    else
    {
        for (int i = 0; i < fieldCount; ++i)
        {
            ChannelLabel(labels[i], sizeof(labels[i]), channelFields[i].name, channelFields[i].id);
            fieldIds[i] = ImGui::GetID(labels[i]);
        }
    }

    // Ctrl+V on a focused field reads color text before the field sees the keys.
    int pasteField = -1;
    for (int i = 0; i < fieldCount; ++i)
    {
        if (g.NavId == fieldIds[i] || g.ActiveId == fieldIds[i])
            pasteField = i;
    }
    if (pasteField >= 0 && !readOnly && !disabled && g.IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        ImVec4 parsed;
        bool parsedAlpha = false;
        const char* clipboard = ImGui::GetClipboardText();
        if (internal::ColorParseText(clipboard, &parsed, &parsedAlpha))
        {
            ImGui::SetKeyOwner(ImGuiKey_V, id, ImGuiInputFlags_LockThisFrame);
            if (!parsedAlpha)
                parsed.w = value.w;
            const ImVec4 out = internal::ColorForWrite(parsed, value, options.alpha, options.hdr);
            if (!internal::ColorSame(out, value, true))
            {
                edited = out;
                written = true;
            }
            hsv = internal::ColorTrackHsv(id, edited);
            hsl = internal::ColorTrackHsl(id, edited);
            // A field that is being typed in shows the pasted color's channel, so Enter keeps it.
            if (g.ActiveId == fieldIds[pasteField])
            {
                char text[64] = {};
                if (model == ColorModel::Hex)
                {
                    char hex[16];
                    internal::ColorFormatHex(hex, sizeof(hex), edited, options.alpha);
                    std::snprintf(text, sizeof(text), "%s", hex + 1);
                }
                else
                {
                    double channels[4];
                    ModelValues(model, edited, hsv, hsl, channels);
                    const ChannelField& f = channelFields[pasteField];
                    const double shown = channels[pasteField == 3 ? 3 : pasteField] * f.scale;
                    internal::FormatNumber(text, static_cast<int>(sizeof(text)), floats ? shown : std::round(shown),
                                           floats ? "%.3f" : "%.0f", nullptr, nullptr, !floats, true);
                }
                internal::TextFieldSetSelection(fieldIds[pasteField], 0, INT_MAX);
                internal::TextFieldInsert(fieldIds[pasteField], text);
            }
        }
    }

    // The model menu.
    float rowHeight = m.height;
    if (menuWidth > 0.0f)
    {
        ImGui::SetCursorScreenPos(ImVec2(x, origin.y));
        const char* names[kColorModelCount];
        for (int i = 0; i < kColorModelCount; ++i)
            names[i] = ModelName(i);
        int choice = state.model;
        DropdownOptions menu;
        menu.size = options.size;
        menu.width = menuWidth / Px(1.0f);
        if (facet::Dropdown("##model", &choice, std::span<const char* const>(names, kColorModelCount), menu))
            state.model = std::clamp(choice, 0, kColorModelCount - 1);
        if (!stacked)
            x += menuWidth + gap;
    }

    // The fields share the rest of the row.
    const float area = std::max(1.0f, right - x);
    const float fieldWidth = std::max(1.0f, std::floor((area - gap * static_cast<float>(fieldCount - 1)) / static_cast<float>(fieldCount)));
    const auto forceFor = [&](int index) {
        const int target = forced.part < 0 ? 0 : forced.part;
        if (forced.state != internal::ForcedItemState::None && target == index)
            internal::ForceNextItemState(forced.state);
    };
    const char* fieldError = options.error != nullptr ? "" : nullptr;
    if (model == ColorModel::Hex)
    {
        ImGui::SetCursorScreenPos(ImVec2(x, fieldsY));
        ImGui::SetNextItemWidth(right - x);
        std::uint32_t hex = PackHex(edited, options.alpha);
        HexInputOptions hexOptions;
        hexOptions.digits = options.alpha ? 8 : 6;
        hexOptions.size = options.size;
        hexOptions.readOnly = readOnly;
        hexOptions.mixed = options.mixed && !written;
        hexOptions.error = fieldError;
        forceFor(0);
        if (facet::HexInput(labels[0], &hex, hexOptions) && !readOnly)
        {
            const ImVec4 out = internal::ColorForWrite(UnpackHex(hex, options.alpha, edited), edited, options.alpha, options.hdr);
            if (!internal::ColorSame(out, edited, true))
            {
                edited = out;
                written = true;
                hsv = internal::ColorTrackHsv(id, edited);
                hsl = internal::ColorTrackHsl(id, edited);
            }
        }
        rowHeight = std::max(rowHeight, ImGui::GetItemRectSize().y);
    }
    else
    {
        double channels[4];
        ModelValues(model, edited, hsv, hsl, channels);
        for (int i = 0; i < fieldCount; ++i)
        {
            const ChannelField& f = channelFields[i];
            const float fx = x + static_cast<float>(i) * (fieldWidth + gap);
            const float fw = i == fieldCount - 1 ? std::max(1.0f, right - fx) : fieldWidth;
            ImGui::SetCursorScreenPos(ImVec2(std::round(fx), fieldsY));
            ImGui::SetNextItemWidth(std::round(fw));
            DragNumberOptions drag;
            drag.min = 0.0;
            drag.max = f.max;
            drag.wrap = f.wrap;
            drag.unit = f.percent ? "%" : nullptr;
            drag.size = options.size;
            drag.readOnly = readOnly;
            drag.mixed = options.mixed && !written;
            drag.error = fieldError;
            const int channelIndex = i == 3 ? 3 : i;
            const double shown = channels[channelIndex] * f.scale;
            forceFor(i);
            bool changed = false;
            double typed = shown;
            if (floats)
            {
                drag.format = f.wrap ? "%.1f" : "%.3f";
                changed = facet::DragNumber(labels[i], &typed, drag);
            }
            else
            {
                int whole = static_cast<int>(std::lround(shown));
                changed = facet::DragNumber(labels[i], &whole, drag);
                typed = whole;
            }
            if (changed && !readOnly)
            {
                double channel = typed / f.scale;
                if (f.wrap)
                    channel -= std::floor(channel);
                const ImVec4 out = internal::ColorForWrite(ApplyField(model, i, channel, edited, &hsv, &hsl), edited,
                                                          options.alpha, options.hdr);
                if (!internal::ColorSame(out, edited, true))
                {
                    edited = out;
                    written = true;
                }
                if (model == ColorModel::Rgb || i == 3)
                {
                    hsv = internal::ColorTrackHsv(id, edited);
                    hsl = internal::ColorTrackHsl(id, edited);
                }
                // The other model keeps its own hue memory in step with the edit.
                if (model == ColorModel::Hsv)
                    internal::ColorRememberHsv(id, edited, hsv);
                if (model == ColorModel::Hsl)
                    internal::ColorRememberHsl(id, edited, hsl);
                ModelValues(model, edited, hsv, hsl, channels);
            }
            rowHeight = std::max(rowHeight, ImGui::GetItemRectSize().y);
        }
    }
    ImGui::PopID();

    if (written)
    {
        *color = edited;
        if (model != ColorModel::Hsv)
            hsv = internal::ColorTrackHsv(id, edited);
        if (model != ColorModel::Hsl)
            hsl = internal::ColorTrackHsl(id, edited);
    }

    const ImRect row(origin, ImVec2(right, origin.y + m.height));
    if (front.text != nullptr)
    {
        internal::ControlTextScope text(m);
        internal::DrawFrontLabel(window->DrawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size,
                                 disabled);
    }
    const bool rowHovered = ImGui::IsMouseHoveringRect(row.Min, row.Max) && ImGui::IsWindowHovered();
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    bool fieldFocused = false;
    for (int i = 0; i < fieldCount; ++i)
        fieldFocused = fieldFocused || g.NavId == fieldIds[i];
    internal::CutTextTooltip(id, ImRect(origin, ImVec2(origin.x + front.width, row.Max.y)), rowHovered,
                             fieldFocused && g.NavCursorVisible, label, labelEnd, front.shown.truncated);
    // The row as one block, so the widget is as wide as its width rule says.
    const float totalHeight = fieldsY - origin.y + rowHeight;
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, totalHeight));
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    if (message.IsSet())
    {
        const float mx = origin.x + std::round(front.width);
        internal::DrawControlMessage(message, ImVec2(mx, origin.y + totalHeight), right - mx, disabled, id, row);
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
