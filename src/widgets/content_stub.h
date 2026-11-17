#pragma once

// The placeholder of the content widget families (display, charts, media, composites) while
// their widgets are declared but not implemented. Every stub in src/widgets/<family>/ is one
// call to these helpers: it submits one item of the size the finished widget will have, drawn
// by internal::PlannedWidget (a hairline box with "<Widget>: planned" in labelSecondary,
// Footnote, cut off where the box ends), and returns the neutral result. Phase 2 replaces a
// stub body with the widget and drops the include.
//
// Sizes come from the theme tokens through Px(). Where the theme has no token for a default
// yet (chart height, panel height, card width), the default is a named multiple of a spacing
// or control height token below, so it still follows the theme and the scale.
//
// Begin and End pairs: StubBegin pushes the Facet scope (internal::PushScope) and opens a
// Dear ImGui group after the placeholder, so the caller's content follows it; StubEnd closes
// both. Nothing here writes through a caller's pointer.

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/common.h"
#include "internal/planned_widget.h"
#include "internal/scope_stack.h"

namespace facet::internal::content
{

// Default lengths that have no theme token yet, as multiples of a token.
inline constexpr float kChartHeightSpans = 5.0f;   // charts and editors: 5 x spacing.xxxl
inline constexpr float kPanelHeightSpans = 6.0f;   // code, log and table views: 6 x spacing.xxxl
inline constexpr float kCardWidthSpans = 8.0f;     // cards and dialogs: 8 x spacing.xxxl
inline constexpr float kSlotSpans = 2.0f;          // inventory slots: 2 x spacing.xxxl
inline constexpr float kThumbnailSpans = 4.0f;     // thumbnails: 4 x spacing.xxxl
inline constexpr float kTallControlRows = 2.0f;    // scrubbers, brushes, gradient bars: 2 x control height
inline constexpr float kZoneControlRows = 4.0f;    // drop zones and empty states: 4 x control height
inline constexpr float kWideAspect = 9.0f / 16.0f; // carousels: height per width

// True inside a frame with a current window, where an item can be submitted.
inline bool CanSubmit()
{
    return ImGui::GetCurrentContext() != nullptr && GImGui->CurrentWindow != nullptr;
}

inline float Spans(float spans)
{
    return Px(T().spacing.xxxl) * spans;
}

inline float ControlRows(float rows, ControlSize size = ControlSize::Inherit)
{
    return ControlHeight(size) * rows;
}

// Pixel width of a widget per rule 6 of docs/api-conventions.md: options.width (> 0 units,
// kFill the available width, < 0 the available width minus that many units), then
// ImGui::SetNextItemWidth, then ImGui::PushItemWidth, then `fallback` pixels (<= 0: the
// available width). Never below one pixel.
inline float ResolveWidth(float optionWidth, float fallback)
{
    if (!CanSubmit())
        return 1.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    float width = fallback > 0.0f ? fallback : available;
    if (optionWidth > 0.0f)
        width = Px(optionWidth);
    else if (optionWidth < 0.0f)
        width = available + Px(optionWidth);
    else if ((GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) != 0 ||
             GImGui->CurrentWindow->DC.ItemWidthStack.Size > 0)
        width = ImGui::CalcItemWidth();
    return std::max(1.0f, width);
}

// Pixel height from options.height (> 0 units, < 0 the available height minus that many
// units) or `fallback` pixels (<= 0: the available height). Never below one pixel.
inline float ResolveHeight(float optionHeight, float fallback)
{
    if (!CanSubmit())
        return 1.0f;
    const float available = ImGui::GetContentRegionAvail().y;
    float height = fallback > 0.0f ? fallback : available;
    if (optionHeight > 0.0f)
        height = Px(optionHeight);
    else if (optionHeight < 0.0f)
        height = available + Px(optionHeight);
    return std::max(1.0f, height);
}

// Height of one line of text in `style`, in pixels.
inline float LineHeight(TextStyle style)
{
    if (!CanSubmit())
        return 1.0f;
    return MeasureText(style, "").y;
}

// Width of a content-sized widget: the text in `style` plus the sm spacing on each side, at
// least `minimum` pixels.
inline float TextWidth(const char* text, TextStyle style, float minimum = 0.0f)
{
    if (!CanSubmit())
        return 1.0f;
    const float textWidth = text != nullptr ? MeasureText(style, text, ImGui::FindRenderedTextEnd(text)).x : 0.0f;
    return std::max(minimum, textWidth + 2.0f * Px(T().spacing.sm));
}

// Width of a content-sized placeholder: `natural` pixels (the size the finished widget will
// have), but at least the placeholder text "<widget>: planned" in Footnote plus the sm
// spacing on each side, so a small widget still says what it is and that it is planned.
// Widths the caller sets through options.width are not widened.
inline float ContentWidth(const char* widget, float natural)
{
    if (!CanSubmit() || widget == nullptr)
        return natural;
    char text[64];
    ImFormatString(text, sizeof(text), "%s: planned", widget);
    return std::max(natural, MeasureText(TextStyle::Footnote, text).x + 2.0f * Px(T().spacing.sm));
}

// The placeholder item: `widget` is the public function name, `label` the label, id or text
// the widget was given (null for none). Sizes in pixels. A null or empty label is replaced by
// "##<widget>": an empty string hashes to the id of the window itself, which Dear ImGui
// rejects for an item at the root of a window.
inline void Stub(const char* widget, const char* label, float width, float height)
{
    if (!CanSubmit())
        return;
    char hidden[64];
    if (label == nullptr || label[0] == '\0')
    {
        ImFormatString(hidden, sizeof(hidden), "##%s", widget != nullptr ? widget : "");
        label = hidden;
    }
    PlannedWidget(widget, label, std::max(1.0f, width), std::max(1.0f, height));
}

// Placeholder plus the open half of a container whose content follows: pushes `scope` (the
// Begin function name, a string literal) and opens a group for the content.
inline void StubBegin(const char* scope, const char* widget, const char* id, float width, float height)
{
    Stub(widget, id, width, height);
    PushScope(scope);
    if (CanSubmit())
        ImGui::BeginGroup();
}

// The close half: pops `scope` and closes the group StubBegin opened. A call without its
// Begin is reported by PopScope and changes nothing else.
inline void StubEnd(const char* scope)
{
    if (PopScope(scope) && CanSubmit())
        ImGui::EndGroup();
}

} // namespace facet::internal::content
