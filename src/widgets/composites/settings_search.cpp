// Settings search: decides which of the caller's settings rows are submitted, counts the hits per
// section for SettingsSearchHitCount, and reveals the first hit after the filter changes.
//
// Matching: the filter against the row's label, description and keywords, each on its own, with
// the fuzzy matcher (internal/fuzzy.h: every query character in order, case-insensitive) or, when
// filterSettings is set, with PassesFilter (every query word, as FilterField matches). An empty
// filter matches every row; every row that matches counts as a hit.
//
// Reveal: on the frame the filter changes to a non-empty text, the first row that matches is
// scrolled into view when it lies outside its window's visible part, and it is highlighted: an
// accentSubtle fill behind the row that holds for motion.feedbackHold and then fades out over
// motion.durationSlow (the animation store, so reduced motion ends it without a fade). The fill is
// drawn before the row's content with the row height measured in the previous frame, so it sits
// behind the row without splitting the draw list.
//
// Hit counts: kept per search name (ImHashStr with seed 0, not the id stack) and section in a list
// that grows only when a new name or section appears; the frame hook in settings_frame.cpp turns
// the frame's counts into the last ones.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/fuzzy.h"
#include "internal/scope_stack.h"
#include "widgets/composites/settings_search.h"

namespace facet
{
namespace internal
{
namespace
{

// Searches open at once (nested searches in one frame).
constexpr int kSettingsSearchDepth = 4;
// The animation store sub key of the reveal highlight under the search id.
constexpr int kSettingsSearchFlashSub = 1;

struct SettingsSearchCount
{
    ImGuiID search = 0;
    ImGuiID section = 0;  // 0: every section
    int current = 0;      // hits in the frame being built
    int last = 0;         // hits in the last completed frame
};

// What a search remembers across frames for the reveal.
struct SettingsSearchRecord
{
    ImGuiID search = 0;
    ImGuiID filterHash = 0;    // the filter of the last frame; 0 for empty
    bool revealPending = false;  // the filter changed: the next hit is revealed
    ImGuiID flashRow = 0;      // the revealed row, highlighted while the flash lasts
    float flashAge = 0.0f;     // seconds since the reveal
    float flashHeight = 0.0f;  // the row's height in the frame before, pixels
    int lastFrame = -1;        // the frame the flash age was advanced in
};

struct SettingsSearchActive
{
    ImGuiID search = 0;
    std::string_view filter;
    FilterSettings settings;
    bool useSettings = false;
    int record = -1;
    // The row SettingsSearchMatch let through last, until the next Match or End measures it.
    ImGuiWindow* rowWindow = nullptr;
    ImGuiID rowId = 0;
    float rowTop = 0.0f;
};

struct SettingsSearchStore
{
    std::vector<SettingsSearchCount> counts;
    std::vector<SettingsSearchRecord> records;
    SettingsSearchActive active[kSettingsSearchDepth];
    int depth = 0;     // open searches, overflow included
};

SettingsSearchStore& Store()
{
    static SettingsSearchStore store;
    return store;
}

void SettingsSearchCountHit(ImGuiID search, ImGuiID section)
{
    SettingsSearchStore& store = Store();
    for (SettingsSearchCount& count : store.counts)
    {
        if (count.search == search && count.section == section)
        {
            ++count.current;
            return;
        }
    }
    try
    {
        store.counts.push_back({search, section, 1, 0});
    }
    catch (const std::bad_alloc&)
    {
    }
}

int SettingsSearchFindRecord(ImGuiID search)
{
    SettingsSearchStore& store = Store();
    for (int i = 0; i < static_cast<int>(store.records.size()); ++i)
    {
        if (store.records[static_cast<std::size_t>(i)].search == search)
            return i;
    }
    try
    {
        SettingsSearchRecord record;
        record.search = search;
        store.records.push_back(record);
        return static_cast<int>(store.records.size()) - 1;
    }
    catch (const std::bad_alloc&)
    {
        return -1;
    }
}

bool SettingsSearchMatches(const SettingsSearchActive& active, const char* text)
{
    if (text == nullptr || text[0] == '\0')
        return false;
    if (active.useSettings)
        return PassesFilter(active.filter, active.settings, text);
    return FuzzyScore(active.filter, text).matched;
}

// Measures the row the last Match let through: its height up to the cursor now, kept for the
// highlight of the next frame when it is the revealed row.
void SettingsSearchCloseRow(SettingsSearchActive& active)
{
    if (active.rowWindow == nullptr)
        return;
    SettingsSearchStore& store = Store();
    if (active.record >= 0 && active.rowWindow == ImGui::GetCurrentWindowRead())
    {
        SettingsSearchRecord& record = store.records[static_cast<std::size_t>(active.record)];
        if (record.flashRow == active.rowId)
        {
            const float bottom = active.rowWindow->DC.CursorPos.y - GImGui->Style.ItemSpacing.y;
            record.flashHeight = std::max(0.0f, bottom - active.rowTop);
        }
    }
    active.rowWindow = nullptr;
    active.rowId = 0;
}

} // namespace

void SettingsSearchEndFrame()
{
    SettingsSearchStore& store = Store();
    for (SettingsSearchCount& count : store.counts)
    {
        count.last = count.current;
        count.current = 0;
    }
    store.depth = 0;
}

void SettingsSearchShutdown()
{
    SettingsSearchStore& store = Store();
    store.counts.clear();
    store.counts.shrink_to_fit();
    store.records.clear();
    store.records.shrink_to_fit();
    store.depth = 0;
}

} // namespace internal

void BeginSettingsSearch(const char* id, const SettingsSearchOptions& options)
{
    internal::PushScope("BeginSettingsSearch");
    internal::SettingsSearchStore& store = internal::Store();
    const int slot = store.depth++;
    if (slot >= internal::kSettingsSearchDepth)
        return;
    internal::SettingsSearchActive& active = store.active[slot];
    active = internal::SettingsSearchActive{};
    active.search = ImHashStr(id != nullptr ? id : "", 0, 0);
    // Leading and trailing spaces do not make a filter.
    std::string_view filter = options.filter != nullptr ? std::string_view(options.filter) : std::string_view();
    while (!filter.empty() && filter.front() == ' ')
        filter.remove_prefix(1);
    while (!filter.empty() && filter.back() == ' ')
        filter.remove_suffix(1);
    active.filter = filter;
    active.useSettings = options.filterSettings != nullptr;
    if (active.useSettings)
        active.settings = *options.filterSettings;
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    active.record = internal::SettingsSearchFindRecord(active.search);
    if (active.record < 0)
        return;
    internal::SettingsSearchRecord& record = store.records[static_cast<std::size_t>(active.record)];
    const ImGuiID filterHash = filter.empty() ? 0 : ImHashData(filter.data(), filter.size(), active.useSettings ? 1 : 0);
    if (filterHash != record.filterHash)
    {
        record.filterHash = filterHash;
        record.revealPending = filterHash != 0;
        record.flashRow = 0;
    }
    const int frame = ImGui::GetFrameCount();
    if (record.lastFrame != frame)
    {
        record.lastFrame = frame;
        record.flashAge += ImGui::GetIO().DeltaTime;
    }
}

void EndSettingsSearch()
{
    if (!internal::PopScope("BeginSettingsSearch"))
        return;
    internal::SettingsSearchStore& store = internal::Store();
    if (store.depth <= 0)
        return;
    const int slot = --store.depth;
    if (slot < internal::kSettingsSearchDepth && ImGui::GetCurrentContext() != nullptr)
        internal::SettingsSearchCloseRow(store.active[slot]);
}

bool SettingsSearchMatch(const char* label, const SettingsSearchMatchOptions& options)
{
    internal::SettingsSearchStore& store = internal::Store();
    if (store.depth <= 0 || store.depth > internal::kSettingsSearchDepth || ImGui::GetCurrentContext() == nullptr)
        return true;
    internal::SettingsSearchActive& active = store.active[store.depth - 1];
    internal::SettingsSearchCloseRow(active);
    const bool hit = active.filter.empty() || internal::SettingsSearchMatches(active, label) ||
                     internal::SettingsSearchMatches(active, options.description) ||
                     internal::SettingsSearchMatches(active, options.keywords);
    if (!hit)
        return false;
    const ImGuiID section = options.section != nullptr ? ImHashStr(options.section, 0, 0) : 0;
    internal::SettingsSearchCountHit(active.search, 0);
    if (section != 0)
        internal::SettingsSearchCountHit(active.search, section);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window == nullptr || active.record < 0)
        return true;
    internal::SettingsSearchRecord& record = store.records[static_cast<std::size_t>(active.record)];
    const ImGuiID rowId = ImHashStr(label != nullptr ? label : "", 0, active.search ^ section);
    const float top = window->DC.CursorPos.y;
    if (record.revealPending)
    {
        record.revealPending = false;
        record.flashRow = rowId;
        record.flashAge = 0.0f;
        record.flashHeight = 0.0f;
        // Scrolled only when the row is not in view already.
        const ImRect visible = window->InnerClipRect;
        const float lineHeight = ImGui::GetFrameHeight();
        if (top < visible.Min.y || top + lineHeight > visible.Max.y)
            ImGui::SetScrollHereY(0.25f);
    }
    active.rowWindow = window;
    active.rowId = rowId;
    active.rowTop = top;

    if (record.flashRow == rowId)
    {
        const Theme& t = T();
        const float hold = t.motion.feedbackHold * t.motion.scale;
        const float amount = Tween(active.search, internal::kSettingsSearchFlashSub, record.flashAge < hold ? 1.0f : 0.0f,
                                   t.motion.durationSlow);
        if (amount > 0.0f && record.flashHeight > 0.0f)
        {
            const float pad = std::round(Px(t.spacing.xs));
            const ImVec2 min(window->DC.CursorPos.x - pad, top - pad);
            const ImVec2 max(window->WorkRect.Max.x, top + record.flashHeight + pad);
            const ImVec4 fill = t.colors.accentSubtle;
            FillRect(window->DrawList, min, max, ToU32(WithAlpha(fill, fill.w * amount)), Px(t.radii.control));
        }
    }
    return true;
}

int SettingsSearchHitCount(const char* id, const char* section)
{
    const ImGuiID search = ImHashStr(id != nullptr ? id : "", 0, 0);
    const ImGuiID key = section != nullptr ? ImHashStr(section, 0, 0) : 0;
    for (const internal::SettingsSearchCount& count : internal::Store().counts)
    {
        if (count.search == search && count.section == key)
            return count.last;
    }
    return 0;
}

} // namespace facet
