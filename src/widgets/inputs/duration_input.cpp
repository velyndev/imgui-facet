// DurationInput: a duration in seconds as hh:mm:ss(.mmm) segments. While not typing, the field
// is one item (item behaviour) whose segments step and drag through the value drag core: Left
// and Right pick the segment, Up and Down step it with the carry into the next one (the edit is
// on the total, so 59 seconds plus one is 1:00), and a vertical drag on a segment moves it in
// whole units of that segment. Enter, Space, a click without a drag or a typed digit switch to a
// text field (the text field core) that takes "1h2m3s", "1m30", "90" or the shown "01:30:00", and
// writes on Enter, Tab or focus loss. Typing and display share one grammar, so the shown text
// always reads back.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "internal/value_drag.h"
#include "widgets/inputs/value_inputs_common.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

constexpr int kDurationSegments = 4;
constexpr int kDurationTextSize = 48;
// Seconds per unit of each segment: hours, minutes, seconds, milliseconds.
constexpr double kDurationUnits[kDurationSegments] = {3600.0, 60.0, 1.0, 0.001};

struct DurationInputState
{
    int segment = -1;            // the segment the keys edit, -1 for the smallest shown one
    bool typing = false;         // the text field is up
    int typingFrames = 0;        // frames the text field waited for the keyboard
    bool typedOnce = false;      // the text field had the keyboard in this typing session
    bool seeded = false;         // typing started with typed characters: they count as an edit
    char edit[kDurationTextSize] = {};
    double dragStart = 0.0;      // the value at the press that started a drag
};

// The segments a field shows, as indices into kDurationUnits.
struct DurationLayout
{
    int units[kDurationSegments] = {};
    int count = 0;
};

DurationLayout DurationSegments(const DurationInputOptions& options)
{
    DurationLayout layout;
    if (options.showHours)
        layout.units[layout.count++] = 0;
    layout.units[layout.count++] = 1;
    layout.units[layout.count++] = 2;
    if (options.showMilliseconds)
        layout.units[layout.count++] = 3;
    return layout;
}

// The shown text of each segment and the sign, from a value in seconds.
struct DurationParts
{
    bool negative = false;
    char text[kDurationSegments][24] = {};
};

DurationParts DurationSplit(double seconds, const DurationLayout& layout)
{
    DurationParts parts;
    if (!std::isfinite(seconds))
        seconds = 0.0;
    parts.negative = seconds < 0.0;
    const double magnitude = std::fabs(seconds);
    const bool ms = layout.units[layout.count - 1] == 3;
    const long long totalMs = ms ? std::llround(magnitude * 1000.0) : std::llround(magnitude) * 1000;
    if (totalMs == 0)
        parts.negative = false;
    const long long totalS = totalMs / 1000;
    const bool hours = layout.units[0] == 0;
    for (int i = 0; i < layout.count; ++i)
    {
        switch (layout.units[i])
        {
        case 0:
            std::snprintf(parts.text[i], sizeof(parts.text[i]), "%02lld", totalS / 3600);
            break;
        case 1:
            std::snprintf(parts.text[i], sizeof(parts.text[i]), "%02lld", hours ? (totalS / 60) % 60 : totalS / 60);
            break;
        case 2:
            std::snprintf(parts.text[i], sizeof(parts.text[i]), "%02lld", totalS % 60);
            break;
        default:
            std::snprintf(parts.text[i], sizeof(parts.text[i]), "%03lld", totalMs % 1000);
            break;
        }
    }
    return parts;
}

// The whole text as shown: "01:30:00", "-05:00.250".
void DurationFormat(char* out, std::size_t size, double seconds, const DurationLayout& layout)
{
    const DurationParts parts = DurationSplit(seconds, layout);
    std::size_t length = 0;
    out[0] = 0;
    const auto append = [&](const char* text) {
        const int written = std::snprintf(out + length, size - length, "%s", text);
        if (written > 0)
            length = std::min(size - 1, length + static_cast<std::size_t>(written));
    };
    if (parts.negative)
        append("-");
    for (int i = 0; i < layout.count; ++i)
    {
        if (i > 0)
            append(layout.units[i] == 3 ? "." : ":");
        append(parts.text[i]);
    }
}

bool DurationIsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Reads one plain number (digits, one dot or comma) from [*p, end) into *out.
bool DurationNumber(const char** p, const char* end, double* out)
{
    const char* begin = *p;
    const char* q = begin;
    while (q < end && (DurationIsDigit(*q) || *q == '.' || *q == ','))
        ++q;
    if (q == begin)
        return false;
    char number[32];
    const std::size_t length = static_cast<std::size_t>(q - begin);
    if (length >= sizeof(number))
        return false;
    std::memcpy(number, begin, length);
    number[length] = 0;
    if (internal::ParseNumber(number, out) != internal::NumberParse::Ok)
        return false;
    *p = q;
    return true;
}

// The unit index a suffix names at [*p, end): h, m or min, s or sec, ms; -1 for none.
int DurationUnit(const char** p, const char* end)
{
    const char* q = *p;
    while (q < end && *q == ' ')
        ++q;
    const auto match = [&](const char* word) {
        const std::size_t length = std::strlen(word);
        if (static_cast<std::size_t>(end - q) < length)
            return false;
        for (std::size_t i = 0; i < length; ++i)
            if (ImToUpper(q[i]) != ImToUpper(word[i]))
                return false;
        // The word ends here: no letter follows.
        const char next = q + length < end ? q[length] : 0;
        return !((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z'));
    };
    struct Word
    {
        const char* text;
        int unit;
    };
    static constexpr Word kWords[] = {{"ms", 3}, {"min", 1}, {"sec", 2}, {"h", 0}, {"m", 1}, {"s", 2}};
    for (const Word& word : kWords)
        if (match(word.text))
        {
            *p = q + std::strlen(word.text);
            return word.unit;
        }
    return -1;
}

// Reads a typed or shown duration: "01:30:00", "1:30" (minutes and seconds), "-0:05.5",
// "1h2m3s", "1m30" (a number after a unit is in the next smaller unit), "2,5 min", "90" (seconds).
internal::NumberParse DurationParse(const char* text, double* out)
{
    const char* begin = text;
    const char* end = text + std::strlen(text);
    while (begin < end && (*begin == ' ' || *begin == '\t'))
        ++begin;
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t'))
        --end;
    if (begin == end)
        return internal::NumberParse::Empty;
    double sign = 1.0;
    if (*begin == '-' || *begin == '+')
    {
        sign = *begin == '-' ? -1.0 : 1.0;
        ++begin;
    }
    double total = 0.0;
    const char* p = begin;
    if (std::memchr(begin, ':', static_cast<std::size_t>(end - begin)) != nullptr)
    {
        // Colon form: two parts are minutes and seconds, three are hours, minutes and seconds.
        double parts[3] = {};
        int count = 0;
        while (true)
        {
            if (count == 3 || !DurationNumber(&p, end, &parts[count]))
                return internal::NumberParse::Invalid;
            ++count;
            if (p == end)
                break;
            if (*p != ':')
                return internal::NumberParse::Invalid;
            ++p;
        }
        if (count < 2)
            return internal::NumberParse::Invalid;
        for (int i = 0; i < count - 1; ++i)
            if (parts[i] != std::floor(parts[i]))
                return internal::NumberParse::Invalid;  // only the seconds have a fraction
        total = count == 3 ? parts[0] * 3600.0 + parts[1] * 60.0 + parts[2] : parts[0] * 60.0 + parts[1];
    }
    else
    {
        // Unit form; a number without a unit takes the unit after the previous one.
        int previous = -1;
        bool any = false;
        while (p < end)
        {
            while (p < end && *p == ' ')
                ++p;
            if (p == end)
                break;
            double number = 0.0;
            if (!DurationNumber(&p, end, &number))
                return internal::NumberParse::Invalid;
            int unit = DurationUnit(&p, end);
            if (unit < 0)
                unit = previous < 0 ? 2 : std::min(previous + 1, kDurationSegments - 1);
            else if (unit <= previous)
                return internal::NumberParse::Invalid;  // "1s2m": units go from large to small
            if (previous >= 0 && unit <= previous)
                return internal::NumberParse::Invalid;
            total += number * kDurationUnits[unit];
            previous = unit;
            any = true;
        }
        if (!any)
            return internal::NumberParse::Invalid;
    }
    if (!std::isfinite(total))
        return internal::NumberParse::Invalid;
    *out = total == 0.0 ? 0.0 : sign * total;
    return internal::NumberParse::Ok;
}

// The x of every segment's left edge and its width, for drawing and for hit testing.
struct DurationGeometry
{
    float signWidth = 0.0f;
    float x[kDurationSegments] = {};
    float width[kDurationSegments] = {};
    float end = 0.0f;
};

DurationGeometry DurationMeasure(const DurationParts& parts, const DurationLayout& layout, float left)
{
    DurationGeometry geometry;
    float x = left;
    if (parts.negative)
    {
        geometry.signWidth = ImGui::CalcTextSize("-").x;
        x += geometry.signWidth;
    }
    for (int i = 0; i < layout.count; ++i)
    {
        if (i > 0)
            x += ImGui::CalcTextSize(layout.units[i] == 3 ? "." : ":").x;
        geometry.x[i] = x;
        geometry.width[i] = ImGui::CalcTextSize(parts.text[i]).x;
        x += geometry.width[i];
    }
    geometry.end = x;
    return geometry;
}

} // namespace

bool DurationInput(const char* label, double* seconds, const DurationInputOptions& options)
{
    // 1. Entry.
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID id = window->GetID(label);
    DurationInputState& state = internal::GetState<DurationInputState>(id);
    const DurationLayout layout = DurationSegments(options);
    const double low = std::isnan(options.min) ? 0.0 : std::min(options.min, std::isnan(options.max) ? options.min : options.max);
    const double high = std::isnan(options.max) ? HUGE_VAL : std::max(options.min, options.max);
    const bool readOnly = options.readOnly || seconds == nullptr || ((next.itemFlags | g.CurrentItemFlags) & ImGuiItemFlags_ReadOnly) != 0;
    const char* error = internal::ValueInputError(id, options.error);
    double current = seconds != nullptr ? *seconds : 0.0;
    if (state.segment < 0 || state.segment >= layout.count)
        state.segment = layout.count - (options.showMilliseconds ? 2 : 1);

    // 2. Width: a field widget, the available width by default.
    const float width = internal::ResolveItemWidth(options.width, next, kFill);

    // Typing: the text field core in the same place, under the same id.
    if (state.typing)
    {
        internal::ApplyNextItemData(next);
        const internal::TextFieldResult field =
            internal::TextFieldBehavior(label, state.edit, sizeof(state.edit),
                                        {.selectAllOnFocus = true,
                                         .size = options.size,
                                         .width = internal::ValueInputUnits(width),
                                         .error = error,
                                         .helpText = options.helpText});
        bool written = false;
        if (field.active)
        {
            state.typedOnce = true;
        }
        else if (state.typedOnce || ++state.typingFrames > 2)
        {
            // The session ended: a commit writes what parses, a cancel or an unchanged text nothing.
            state.typing = false;
            const bool edit = field.committed || (state.seeded && state.typedOnce && !field.canceled);
            if (edit && seconds != nullptr && !readOnly)
            {
                double parsed = 0.0;
                if (DurationParse(state.edit, &parsed) == internal::NumberParse::Ok)
                {
                    parsed = std::clamp(parsed, low, high);
                    if (parsed != *seconds)
                    {
                        *seconds = parsed;
                        written = true;
                    }
                    internal::ValueInputSetRejected(id, false);
                }
                else
                {
                    internal::ValueInputSetRejected(id, true);
                }
            }
        }
        return written;
    }

    // 3. Segments: one item for the label and the frame.
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width);
    ImGui::BeginGroup();
    const float top = window->DC.CursorPos.y;
    internal::ItemOptions itemOptions;
    itemOptions.press = internal::PressMode::ClickRelease;
    itemOptions.readOnly = readOnly;
    itemOptions.mixed = options.mixed;
    itemOptions.handCursor = false;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    internal::ApplyNextItemData(next);
    internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, m.height), itemOptions);
    const ImRect frame(item.rect.Min.x + front.width, item.rect.Min.y, item.rect.Max.x, item.rect.Max.y);
    const bool live = !item.disabled && item.forced == ForcedItemState::None;
    internal::ControlTextScope text(m);
    const DurationParts parts = DurationSplit(current, layout);
    const DurationGeometry geometry = DurationMeasure(parts, layout, frame.Min.x + m.fieldPadding);

    bool written = false;
    // A press picks the segment under the pointer.
    if (live && item.activated && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float mouse = g.IO.MousePos.x;
        for (int i = 0; i < layout.count; ++i)
            if (mouse >= geometry.x[i] - m.fieldPadding * 0.5f)
                state.segment = i;
        state.dragStart = current;
    }
    if (live && item.hovered && !readOnly)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

    // Left and Right pick the segment while focused.
    if (live && item.focused && !item.active)
    {
        ImGui::SetKeyOwner(ImGuiKey_LeftArrow, id);
        ImGui::SetKeyOwner(ImGuiKey_RightArrow, id);
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, id))
            state.segment = std::max(0, state.segment - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, id))
            state.segment = std::min(layout.count - 1, state.segment + 1);
    }

    // Up, Down and a vertical drag edit the total by the segment's unit.
    const double unit = kDurationUnits[layout.units[state.segment]];
    const float threshold = Px(t.drag.startThreshold);
    const bool pastThreshold = g.IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= threshold * threshold;
    internal::ItemState dragItem = item;
    if (dragItem.held && !pastThreshold)
        dragItem.held = false;
    dragItem.pressed = false;
    internal::ValueDragOptions drag;
    drag.mapping.min = low;
    drag.mapping.max = high;
    drag.orientation = Orientation::Vertical;
    drag.grabRadius = -1.0f;
    drag.speed = unit / std::max(1.0, static_cast<double>(Px(t.spacing.sm)));
    drag.keyStep = unit;
    drag.readOnly = readOnly;
    double edited = current;
    const internal::ValueDragResult edit = internal::ValueDragBehavior(dragItem, &edited, frame, drag);
    if (edit.changed && seconds != nullptr && !readOnly)
    {
        if (edit.dragging)
            edited = std::clamp(state.dragStart + std::round((edited - state.dragStart) / unit) * unit, low, high);
        if (edited != *seconds && std::isfinite(edited))
        {
            *seconds = edited;
            current = edited;
            written = true;
            internal::ValueInputSetRejected(id, false);
        }
    }

    // Enter, Space, a click without a drag or a typed digit start typing.
    bool startTyping = false;
    bool fromChars = false;
    if (live && !readOnly)
    {
        if (item.pressed && (item.keyboardPressed || !pastThreshold))
            startTyping = true;
        if (item.focused && !item.active && g.IO.InputQueueCharacters.Size > 0)
        {
            const ImWchar first = g.IO.InputQueueCharacters[0];
            if ((first >= '0' && first <= '9') || first == '-' || first == ':')
            {
                startTyping = true;
                fromChars = true;
                int length = 0;
                for (const ImWchar ch : g.IO.InputQueueCharacters)
                    if (ch < 0x80 && length < kDurationTextSize - 1)
                        state.edit[length++] = static_cast<char>(ch);
                state.edit[length] = 0;
            }
        }
    }
    if (startTyping)
    {
        if (!fromChars)
            DurationFormat(state.edit, sizeof(state.edit), current, layout);
        state.typing = true;
        state.typingFrames = 0;
        state.typedOnce = false;
        state.seeded = fromChars;
        internal::TextFieldRequestFocus(id, fromChars ? internal::TextFieldFocus::End : internal::TextFieldFocus::SelectAll);
    }

    // 4. Draw.
    ImDrawList* drawList = window->DrawList;
    if (item.visible)
    {
        internal::DrawFieldFrame(drawList, frame,
                                 {.hoverAmount = item.hoverAmount,
                                  .active = item.active && edit.dragging,
                                  .disabled = item.disabled,
                                  .readOnly = readOnly,
                                  .error = error != nullptr});
        const DurationParts shown = DurationSplit(current, layout);
        const DurationGeometry at = DurationMeasure(shown, layout, frame.Min.x + m.fieldPadding);
        const float textY = CenteredTextY(frame.Min.y, frame.GetHeight());
        ImGui::PushClipRect(ImVec2(frame.Min.x + m.fieldPadding * 0.5f, frame.Min.y),
                            ImVec2(frame.Max.x - m.fieldPadding * 0.5f, frame.Max.y), true);
        if (item.mixed)
        {
            drawList->AddText(ImVec2(frame.Min.x + m.fieldPadding, textY), ToU32(internal::ControlSecondaryColor(item.disabled)),
                              GetStrings().mixed);
        }
        else
        {
            // The segment the keys edit: accentSubtle behind it while focused or dragging.
            const bool showSegment = !item.disabled && (item.focusVisible || edit.dragging ||
                                                        Has(item.forced, ForcedItemState::Dragging));
            if (showSegment)
            {
                const float pad = std::round(Px(t.spacing.xs) * 0.5f);
                const ImVec2 min(at.x[state.segment] - pad, frame.Min.y + Px(t.spacing.xs));
                const ImVec2 max(at.x[state.segment] + at.width[state.segment] + pad, frame.Max.y - Px(t.spacing.xs));
                FillRect(drawList, min, max, ToU32(c.accentSubtle), Px(t.radii.element));
            }
            const ImU32 digits = ToU32(internal::ControlLabelColor(item.disabled));
            const ImU32 marks = ToU32(internal::ControlSecondaryColor(item.disabled));
            if (shown.negative)
                drawList->AddText(ImVec2(frame.Min.x + m.fieldPadding, textY), digits, "-");
            for (int i = 0; i < layout.count; ++i)
            {
                if (i > 0)
                {
                    const char* mark = layout.units[i] == 3 ? "." : ":";
                    drawList->AddText(ImVec2(at.x[i] - ImGui::CalcTextSize(mark).x, textY), marks, mark);
                }
                drawList->AddText(ImVec2(at.x[i], textY), digits, shown.text[i]);
            }
        }
        ImGui::PopClipRect();
        if (front.text != nullptr)
            internal::DrawFrontLabel(drawList, front, ImVec2(item.rect.Min.x, textY), m.size, item.disabled);
        internal::DrawFocusRing(drawList, frame, m.radius, item.focusAmount);
    }

    // 5. The cut label's tooltip, the message line, the result.
    internal::CutTextTooltip(item, front.text, front.end, front.shown.truncated);
    const internal::ControlMessage message = internal::PickControlMessage(error, options.helpText);
    internal::DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), item.disabled, id, frame);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
