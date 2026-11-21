// Parts shared by the navigation widgets (navigation_parts.h).

#include "widgets/navigation/navigation_parts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/state_storage.h"
#include "widgets/display/display_look.h"

namespace facet::internal
{
namespace
{

struct NavigationPartRecord
{
    ImRect rect;
    int frame = -1;
};

NavigationPartRecord& PartRecord(ImGuiID widgetId, int part)
{
    return GetState<NavigationPartRecord>(ImHashData(&part, sizeof(part), ImHashStr("##facet.nav.part", 0, widgetId)));
}

} // namespace

void NavigationRecordPart(ImGuiID widgetId, int part, const ImRect& rect)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    NavigationPartRecord& record = PartRecord(widgetId, part);
    record.rect = rect;
    record.frame = ImGui::GetFrameCount();
}

ImRect NavigationPartRect(ImGuiID widgetId, int part)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return ImRect();
    const NavigationPartRecord& record = PartRecord(widgetId, part);
    return record.frame >= ImGui::GetFrameCount() - 1 ? record.rect : ImRect();
}

NavigationBadgeMetrics MeasureNavigationBadge(const char* text, const char* end)
{
    const Theme& t = T();
    PushTextStyle(TextStyle::Caption);
    const float textWidth = ImGui::CalcTextSize(text, end).x;
    const float lineHeight = ImGui::GetFontSize();
    PopTextStyle();
    NavigationBadgeMetrics m;
    m.height = std::round(lineHeight + Px(t.spacing.xs));
    // At least as wide as it is tall, so a single digit reads as a mark and not as a sliver.
    m.width = std::max(m.height, std::ceil(textWidth + Px(t.spacing.sm)));
    return m;
}

void DrawNavigationBadge(ImDrawList* drawList, float x, float centerY, const char* text, const char* end, Tone tone,
                         bool disabled)
{
    const NavigationBadgeMetrics m = MeasureNavigationBadge(text, end);
    const DisplayToneColors look = DisplayToneLook(tone, disabled);
    const bool neutral = tone == Tone::Neutral || disabled;
    const ImVec4 fill = neutral ? look.wash : look.solid;
    const ImVec4 ink = disabled ? T().colors.labelTertiary : (neutral ? T().colors.label : look.onSolid);
    const ImVec2 min(std::round(x), std::round(centerY - m.height * 0.5f));
    const ImVec2 max(min.x + m.width, min.y + m.height);
    FillRect(drawList, min, max, ToU32(fill), Px(T().radii.element));
    PushTextStyle(TextStyle::Caption);
    const float textWidth = ImGui::CalcTextSize(text, end).x;
    drawList->AddText(ImVec2(std::round((min.x + max.x - textWidth) * 0.5f), CenteredTextY(min.y, m.height)), ToU32(ink),
                      text, end);
    PopTextStyle();
}

int FormatNavigationCount(int count, char* buffer, int bufferSize)
{
    if (buffer == nullptr || bufferSize <= 0)
        return 0;
    const int written = std::snprintf(buffer, static_cast<std::size_t>(bufferSize), "%d", count);
    return written < 0 ? 0 : std::min(written, bufferSize - 1);
}

void NavigationRoving::BeginFrame()
{
    const int now = ImGui::GetFrameCount();
    if (frame == now)
        return;
    if (frame >= 0)
    {
        std::copy(ids, ids + count, last);
        std::copy(enabled, enabled + count, lastEnabled);
        lastCount = count;
        lastSelected = selectedNext;
    }
    frame = now;
    count = 0;
    selectedNext = 0;
    stop = 0;
    const ImGuiID nav = GImGui->NavId;
    int firstEnabled = -1;
    for (int i = 0; i < lastCount; ++i)
    {
        if (!lastEnabled[i])
            continue;
        if (firstEnabled < 0)
            firstEnabled = i;
        if (nav != 0 && last[i] == nav)
            stop = nav;
    }
    if (stop == 0 && lastSelected != 0)
        for (int i = 0; i < lastCount; ++i)
            if (last[i] == lastSelected && lastEnabled[i])
                stop = lastSelected;
    if (stop == 0 && firstEnabled >= 0)
        stop = last[firstEnabled];
}

bool NavigationRoving::IsTabStop(ImGuiID id) const
{
    return stop == 0 || stop == id;
}

void NavigationRoving::Add(ImGuiID id, bool isEnabled, bool selected)
{
    if (selected && isEnabled)
        selectedNext = id;
    if (count >= kNavigationRovingMax)
        return;
    ids[count] = id;
    enabled[count] = isEnabled;
    ++count;
}

ImGuiID NavigationRoving::Step(ImGuiID id, bool vertical)
{
    const auto take = [id](ImGuiKey key) {
        ImGui::SetKeyOwner(key, id);
        return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id);
    };
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl || io.KeyAlt || io.KeySuper || stepFrame == ImGui::GetFrameCount())
        return 0;
    const bool back = take(vertical ? ImGuiKey_UpArrow : ImGuiKey_LeftArrow);
    const bool forward = take(vertical ? ImGuiKey_DownArrow : ImGuiKey_RightArrow);
    const bool home = take(ImGuiKey_Home);
    const bool end = take(ImGuiKey_End);
    int at = -1;
    for (int i = 0; i < lastCount; ++i)
        if (last[i] == id)
            at = i;
    if (at < 0)
        return 0;
    int target = -1;
    if (home)
    {
        for (int i = 0; i < lastCount && target < 0; ++i)
            if (lastEnabled[i])
                target = i;
    }
    else if (end)
    {
        for (int i = lastCount - 1; i >= 0 && target < 0; --i)
            if (lastEnabled[i])
                target = i;
    }
    else if (back != forward)
    {
        const int step = forward ? 1 : -1;
        for (int i = at + step; i >= 0 && i < lastCount && target < 0; i += step)
            if (lastEnabled[i])
                target = i;
    }
    if (target < 0 || target == at)
        return 0;
    // The entry that takes the focus may be submitted later in this frame and sees the same press:
    // one step per frame.
    stepFrame = ImGui::GetFrameCount();
    // Every entry of a bar sits in the same window and focus scope as the focused one.
    ImGui::SetFocusID(last[target], GImGui->CurrentWindow);
    ImGui::SetNavCursorVisible(true);
    SetKeyboardModality(true);
    return last[target];
}


} // namespace facet::internal
