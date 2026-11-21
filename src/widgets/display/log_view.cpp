// LogView (facet/widgets/display.h): a virtualized log on the list view block (list_view.h), so only
// the visible entries are read and the per-frame cost does not grow with the log. Each row is one
// Body line: an optional time column, the level (warning and error icons in their tone, trace and
// debug text in labelSecondary), the source tag and the text, cut at the view edge with the whole
// line on hover and keyboard focus. Rows select like any Facet list (click, Shift, Ctrl, Ctrl+A,
// box select) and Ctrl+C copies the selected lines. While scrolled to the end the view follows new
// entries; scrolling up stops that and shows Strings::jumpToLatest under the log, which, like End or
// scrolling back to the end, resumes it.

#include "facet/widgets/display.h"

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
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// The fewest rows a log takes when it fills an available height that is smaller.
constexpr float kLogMinRows = 4.0f;
// The widest time the column is measured for.
constexpr const char* kLogTimeTemplate = "00:00:00.000";

struct LogViewState
{
    ImGuiSelectionBasicStorage selection;  // keyed by entry index
    bool started = false;
    bool following = true;
    float requestedScroll = -1.0f;  // the scroll offset the view moved to while following; -1 none
};

struct LogSpan
{
    std::span<const LogEntry> entries;
};

bool LogSpanGetter(void* user, int index, LogEntry* out)
{
    const LogSpan* span = static_cast<const LogSpan*>(user);
    if (index < 0 || index >= static_cast<int>(span->entries.size()))
        return false;
    *out = span->entries[static_cast<std::size_t>(index)];
    return true;
}

// Writes a time in seconds as "mm:ss.mmm", or "h:mm:ss.mmm" from one hour on, with a sign when
// negative. Returns the length.
int FormatLogTime(char* buffer, std::size_t size, double seconds)
{
    const bool negative = seconds < 0.0;
    const double total = std::fabs(seconds);
    const long long millis = static_cast<long long>(std::floor(total * 1000.0 + 0.5));
    const long long hours = millis / 3600000;
    const int minutes = static_cast<int>((millis / 60000) % 60);
    const int secs = static_cast<int>((millis / 1000) % 60);
    const int ms = static_cast<int>(millis % 1000);
    if (hours > 0)
        return ImFormatString(buffer, size, "%s%lld:%02d:%02d.%03d", negative ? "-" : "", hours, minutes, secs, ms);
    return ImFormatString(buffer, size, "%s%02d:%02d.%03d", negative ? "-" : "", minutes, secs, ms);
}

Icon LogLevelIcon(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Warning:
        return Icon("warning");
    case LogLevel::Error:
        return Icon("error");
    default:
        return Icon();
    }
}

// Appends the selected lines, in log order, as "time  source  text" to the clipboard.
void CopyLogSelection(LogViewState& state, int count, LogGetter getter, void* user, bool timestamps)
{
    ImGuiTextBuffer text;
    void* iterator = nullptr;
    ImGuiID key = 0;
    while (state.selection.GetNextSelectedItem(&iterator, &key))
    {
        const int index = static_cast<int>(key);
        LogEntry entry;
        if (index < 0 || index >= count || !getter(user, index, &entry))
            continue;
        if (timestamps && !std::isnan(entry.time))
        {
            char time[32];
            FormatLogTime(time, sizeof(time), entry.time);
            text.appendf("%s  ", time);
        }
        if (entry.source != nullptr && entry.source[0] != '\0')
            text.appendf("%s  ", entry.source);
        text.append(entry.text.data(), entry.text.data() + entry.text.size());
        text.append("\n");
    }
    if (!text.empty())
        ImGui::SetClipboardText(text.c_str());
}

void LogViewImpl(const char* id, int count, LogGetter getter, void* user, const LogViewOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    if (getter == nullptr || count < 0)
        count = 0;
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const ImGuiID viewId = window->GetID(id);
    LogViewState& state = internal::GetState<LogViewState>(viewId);
    if (!state.started)
    {
        state.started = true;
        state.following = options.autoScroll;
    }
    if (!options.autoScroll)
        state.following = false;

    const float rowHeight = internal::ListRowHeight(ControlSize::Small);
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    float height = options.height > 0.0f ? Px(options.height) : ImGui::GetContentRegionAvail().y;
    height = std::max(height, rowHeight * kLogMinRows);
    const internal::ControlMetrics small = internal::GetControlMetrics(ControlSize::Small);
    const bool ready = options.state == DataState::Ready && count > 0;
    const bool showJump = options.autoScroll && ready && !state.following;
    const float barHeight = showJump ? small.height + ImGui::GetStyle().ItemSpacing.y : 0.0f;

    const ImVec2 origin = window->DC.CursorPos;
    const ImVec2 areaSize(width, std::max(rowHeight, height - barHeight));
    // The log sits on the grouped surface with a hairline, like a code view, so its edges show
    // where its rows scroll.
    const Colors& c = t.colors;
    const float radius = Px(t.radii.control);
    FillRect(window->DrawList, origin, ImVec2(origin.x + areaSize.x, origin.y + areaSize.y), ToU32(c.bgSecondary), radius);
    StrokeRect(window->DrawList, origin, ImVec2(origin.x + areaSize.x, origin.y + areaSize.y), ToU32(c.separator), radius,
               HairlineThickness());
    ImGui::BeginGroup();
    internal::ListViewDesc desc;
    desc.rowCount = count;
    desc.rowHeight = rowHeight;
    desc.areaSize = areaSize;
    desc.fadeSurface = c.bgSecondary;
    desc.selection = &state.selection;
    desc.boxSelect = true;
    desc.state = options.state;
    desc.emptyText = options.emptyText;
    desc.errorText = options.errorText;
    if (internal::BeginListView(viewId, desc))
    {
        ImGuiWindow* area = ImGui::GetCurrentWindow();
        const ImGuiIO& io = ImGui::GetIO();
        const bool focused = GImGui->NavWindow == area;
        if (state.following)
        {
            // Any scroll up stops following: the scroll moved above where following put it, a smooth
            // scroll heads up, the wheel turned up over the log, or a key moves up.
            const bool movedUp = state.requestedScroll >= 0.0f && area->Scroll.y < state.requestedScroll - 0.5f;
            const bool headingUp = internal::ScrollAreaTarget(area).y < area->Scroll.y - 0.5f;
            const bool wheelUp = io.MouseWheel > 0.0f && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            const bool keyUp = focused && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_PageUp) ||
                                           ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_None, viewId));
            if (movedUp || headingUp || wheelUp || keyUp)
            {
                state.following = false;
                state.requestedScroll = -1.0f;
            }
        }
        else if (options.autoScroll && focused && ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_None, viewId))
        {
            state.following = true;
        }

        // Columns: the time (measured for the widest time, so rows do not shift), the level icon,
        // then the source and the text.
        PushTextStyle(TextStyle::Body);
        const float iconSize = std::round(ImGui::GetFontSize());
        const float gap = Px(t.spacing.sm);
        const float timeWidth = options.timestamps ? std::ceil(ImGui::CalcTextSize(kLogTimeTemplate).x) : 0.0f;
        ImDrawList* drawList = area->DrawList;
        RowRange rows;
        while (internal::NextListRows(&rows))
        {
            for (int i = rows.first; i < rows.last; ++i)
            {
                const internal::ListRowState row = internal::ListViewRow(i);
                LogEntry entry;
                if (!getter(user, i, &entry))
                    continue;
                const bool disabled = row.disabled || row.item.disabled;
                const float textTop = CenteredTextY(row.content.Min.y, row.content.GetHeight());
                float x = row.content.Min.x;
                if (options.timestamps)
                {
                    if (!std::isnan(entry.time))
                    {
                        char time[32];
                        FormatLogTime(time, sizeof(time), entry.time);
                        drawList->AddText(ImVec2(x, textTop), ToU32(internal::ControlSecondaryColor(disabled)), time);
                    }
                    x += timeWidth + gap;
                }
                const Icon icon = LogLevelIcon(entry.level);
                if (icon.IsValid())
                {
                    const Tone tone = entry.level == LogLevel::Error ? Tone::Destructive : Tone::Warning;
                    internal::DrawControlIcon(drawList, icon, x, internal::DisplayCapsCenter(textTop), iconSize,
                                              ToU32(internal::DisplayToneLook(tone, disabled).glyph));
                }
                x += iconSize + gap;
                if (entry.source != nullptr && entry.source[0] != '\0')
                {
                    const internal::FittedText source =
                        internal::FitText(entry.source, nullptr, std::max(0.0f, row.content.Max.x - x));
                    drawList->AddText(ImVec2(x, textTop), ToU32(internal::ControlSecondaryColor(disabled)), source.text);
                    x += source.width + gap;
                }
                const bool quiet = entry.level == LogLevel::Trace || entry.level == LogLevel::Debug;
                const ImU32 textColor =
                    ToU32(quiet ? internal::ControlSecondaryColor(disabled) : internal::ControlLabelColor(disabled));
                const char* text = entry.text.data();
                const char* textEnd = text + entry.text.size();
                const char* newline = text != nullptr ? static_cast<const char*>(std::memchr(text, '\n', entry.text.size())) : nullptr;
                const internal::FittedText shown =
                    internal::FitText(text, newline != nullptr ? newline : textEnd, std::max(0.0f, row.content.Max.x - x));
                if (text != nullptr)
                    drawList->AddText(ImVec2(x, textTop), textColor, shown.text);
                internal::CutTextTooltip(row.item.id, row.rect, row.item.hovered, row.item.focusVisible, text, textEnd,
                                         shown.truncated || newline != nullptr);
            }
        }
        PopTextStyle();

        if (focused && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteFocused))
            CopyLogSelection(state, count, getter, user, options.timestamps);

        // Scrolled back to the end by the user: follow again.
        if (!state.following && options.autoScroll && area->ScrollMax.y > 0.0f &&
            area->Scroll.y >= area->ScrollMax.y - 0.5f && internal::ScrollAreaTarget(area).y >= area->ScrollMax.y - 0.5f)
            state.following = true;
        if (state.following)
        {
            // A target past the end of every row: Dear ImGui clamps it to the scroll range when the
            // window begins next frame, which by then includes the entries added this frame.
            ImGui::SetScrollY(area, rowHeight * static_cast<float>(count) + area->InnerRect.GetHeight());
            state.requestedScroll = area->ScrollMax.y;
        }
        internal::EndListView();
    }

    if (showJump)
    {
        // Under the log, at its end: resumes following.
        const char* jump = GetStrings().jumpToLatest;
        PushTextStyle(small.textStyle);
        const float buttonWidth = std::ceil(small.paddingX * 2.0f + std::round(ImGui::GetFontSize()) + small.iconGap +
                                            ImGui::CalcTextSize(jump, ImGui::FindRenderedTextEnd(jump)).x);
        PopTextStyle();
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(at.x + std::max(0.0f, width - buttonWidth), at.y));
        ImGui::PushOverrideID(viewId);
        if (Button(jump, {.icon = Icon("arrow-down"), .size = ControlSize::Small}))
            state.following = true;
        ImGui::PopID();
    }
    // The log takes its whole rectangle also when the area is out of sight.
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, origin.y + height));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(viewId);
}

} // namespace

void LogView(const char* id, std::span<const LogEntry> entries, const LogViewOptions& options)
{
    LogSpan span{entries};
    LogViewImpl(id, static_cast<int>(entries.size()), &LogSpanGetter, &span, options);
}

void LogView(const char* id, int entryCount, LogGetter getter, void* user, const LogViewOptions& options)
{
    LogViewImpl(id, entryCount, getter, user, options);
}

namespace internal
{

bool DisplayLogViewFollowing(ImGuiID viewId)
{
    // Find without creating: a view never drawn does not follow anything yet.
    const void* found = GlobalStateStore().Find(viewId, StateTypeOf<LogViewState>());
    return found != nullptr && static_cast<const LogViewState*>(found)->following;
}

} // namespace internal

} // namespace facet
