// Behaviour behind widgets/common.h: the control size stack, the built-in strings, the edit
// session queries (fed by the item behaviour and the value drag core) and the visible row ranges.

#include "facet/widgets/common.h"

#include <array>
#include <iterator>
#include <new>
#include <string>

#include "core/core_internal.h"
#include "facet/core/scale.h"
#include "internal/row_layout.h"
#include "internal/subsystem.h"
#include "internal/value_drag.h"

namespace facet
{
namespace
{

struct ControlSizeStack
{
    ControlSize sizes[kControlSizeStackDepth] = {};
    int depth = 0;
    int overflow = 0;  // pushes beyond the capacity, ignored but balanced by their pops
};

ControlSizeStack& SizeStack()
{
    static ControlSizeStack stack;
    return stack;
}

// Reports sizes, rows and row groups still open at the end of the frame and clears them.
void CommonEndFrame()
{
    internal::EndFrameRows();
    ControlSizeStack& stack = SizeStack();
    const int open = stack.depth + stack.overflow;
    if (open > 0)
        internal::ReportUsageError("PushControlSize without PopControlSize: %d size(s) still pushed at EndFrame", open);
    stack.depth = 0;
    stack.overflow = 0;
}

void CommonShutdown()
{
    ControlSizeStack& stack = SizeStack();
    stack.depth = 0;
    stack.overflow = 0;
}

// Every member of Strings. The static_assert below fails when a member is added to the struct
// but not to this list.
constexpr const char* Strings::*kStringMembers[] = {
    &Strings::mixed,      &Strings::loading,    &Strings::empty,     &Strings::noResults,
    &Strings::loadFailed, &Strings::select,     &Strings::confirm,   &Strings::cancel,
    &Strings::close,      &Strings::more,       &Strings::unbound,   &Strings::listening,
    &Strings::clear,      &Strings::search,     &Strings::copy,      &Strings::copied,
    &Strings::show,       &Strings::hide,       &Strings::remove,    &Strings::selectAll,
    &Strings::reset,      &Strings::back,       &Strings::next,      &Strings::previous,
    &Strings::finish,     &Strings::skip,       &Strings::browse,    &Strings::today,
    &Strings::capsLock,   &Strings::bindHold,   &Strings::bindToggle, &Strings::bindAlways,
    &Strings::bindOff,    &Strings::minimize,   &Strings::maximize,  &Strings::restore,
    &Strings::pin,        &Strings::unpin,      &Strings::apply,     &Strings::revert,
    &Strings::pageOf,     &Strings::unsavedChange, &Strings::unsavedChanges,
    &Strings::themeLight, &Strings::themeDark,  &Strings::themeSystem,
    &Strings::monthJanuary, &Strings::monthFebruary, &Strings::monthMarch, &Strings::monthApril,
    &Strings::monthMay,   &Strings::monthJune,  &Strings::monthJuly, &Strings::monthAugust,
    &Strings::monthSeptember, &Strings::monthOctober, &Strings::monthNovember, &Strings::monthDecember,
    &Strings::weekdayMonday, &Strings::weekdayTuesday, &Strings::weekdayWednesday, &Strings::weekdayThursday,
    &Strings::weekdayFriday, &Strings::weekdaySaturday, &Strings::weekdaySunday,
    &Strings::am,         &Strings::pm,         &Strings::matchCase, &Strings::wholeWord,
    &Strings::aspectLock, &Strings::swap,       &Strings::linkValues, &Strings::increase,
    &Strings::decrease,   &Strings::jumpToLatest,
};
constexpr size_t kStringCount = std::size(kStringMembers);
static_assert(sizeof(Strings) == kStringCount * sizeof(const char*), "kStringMembers misses a Strings member");

// The table handed out by GetStrings, pointing into owned copies.
struct StringTable
{
    Strings table;
    std::array<std::string, kStringCount> storage;
};

StringTable& Table()
{
    static StringTable strings;
    return strings;
}

} // namespace

bool NextVisibleRows(RowRange* rows)
{
    // Stub until the list and selection core exists (Phase 1c part B).
    if (rows != nullptr)
        *rows = {};
    return false;
}

void PushControlSize(ControlSize size)
{
    ControlSizeStack& stack = SizeStack();
    if (stack.depth >= kControlSizeStackDepth)
    {
        if (stack.overflow == 0)
            internal::ReportUsageError("PushControlSize nested deeper than %d; the extra sizes are ignored",
                                       kControlSizeStackDepth);
        ++stack.overflow;
        return;
    }
    stack.sizes[stack.depth] = ResolveControlSize(size);
    ++stack.depth;
}

void PopControlSize()
{
    ControlSizeStack& stack = SizeStack();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
    {
        internal::ReportUsageError("PopControlSize without PushControlSize");
        return;
    }
    --stack.depth;
}

ControlSize ResolveControlSize(ControlSize size)
{
    if (size != ControlSize::Inherit)
        return size;
    const ControlSizeStack& stack = SizeStack();
    return stack.depth > 0 ? stack.sizes[stack.depth - 1] : ControlSize::Regular;
}

float ControlHeight(ControlSize size)
{
    const Metrics& metrics = T().metrics;
    switch (ResolveControlSize(size))
    {
    case ControlSize::Small:
        return Px(metrics.controlHeightSmall);
    case ControlSize::Large:
        return Px(metrics.controlHeightLarge);
    default:
        return Px(metrics.controlHeight);
    }
}

TextStyle ControlTextStyle(ControlSize size)
{
    return ResolveControlSize(size) == ControlSize::Small ? TextStyle::Footnote : TextStyle::Body;
}

bool IsItemEditCommitted()
{
    // The session of the last item ended with an edit (Dear ImGui's own deactivation query, fed
    // by the item behaviour and the value drag core), and Escape did not end it.
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    return ImGui::IsItemDeactivatedAfterEdit() && !IsItemEditCanceled();
}

bool IsItemEditCanceled()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    return internal::WasEditCanceled(ImGui::GetItemID());
}

void SetStrings(const Strings& strings)
{
    StringTable& current = Table();
    const Strings defaults;
    std::array<std::string, kStringCount> storage;
    try
    {
        for (size_t i = 0; i < kStringCount; ++i)
        {
            const char* text = strings.*kStringMembers[i];
            storage[i] = text != nullptr ? text : defaults.*kStringMembers[i];
        }
    }
    catch (const std::bad_alloc&)
    {
        // The old table stays in use.
        internal::LogWarning("SetStrings ran out of memory; the previous strings stay in use");
        return;
    }
    current.storage.swap(storage);
    for (size_t i = 0; i < kStringCount; ++i)
        current.table.*kStringMembers[i] = current.storage[i].c_str();
}

const Strings& GetStrings()
{
    return Table().table;
}

namespace internal
{

// Checks the control size stack at the end of every frame.
Subsystem WidgetsCommonSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_common";
    subsystem.shutdown = &CommonShutdown;
    subsystem.endFrame = &CommonEndFrame;
    return subsystem;
}

} // namespace internal

} // namespace facet
