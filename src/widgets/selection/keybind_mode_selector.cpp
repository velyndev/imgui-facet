// KeybindModeSelector: how a key bind activates (Hold, Toggle, Always, Off), as a segmented control
// on the segment strip, or as a dropdown (lists.h) when the row is too short for the segments.

#include "facet/widgets/selection.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/lists.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/selection/segment_strip.h"

namespace facet
{
namespace
{

constexpr int kModeCount = 4;  // the members of BindMode

// The modes the selector shows, in BindMode order: the offered ones, and the current mode while it
// is one that is not offered (shown, but disabled, so it cannot be chosen again once left).
struct ModeList
{
    ChoiceItem items[kModeCount];
    BindMode modes[kModeCount] = {};
    int count = 0;
    int current = -1;  // the entry of the current mode, -1 when it is out of range
};

ModeList BuildModes(BindMode current, bool hasValue, const KeybindModeSelectorOptions& options)
{
    const Strings& strings = GetStrings();
    const char* const names[kModeCount] = {strings.bindHold, strings.bindToggle, strings.bindAlways, strings.bindOff};
    const bool offered[kModeCount] = {true, true, options.allowAlways, options.allowOff};
    ModeList list;
    for (int i = 0; i < kModeCount; ++i)
    {
        const BindMode mode = static_cast<BindMode>(i);
        const bool isCurrent = hasValue && mode == current;
        if (!offered[i] && !isCurrent)
            continue;
        ChoiceItem& item = list.items[list.count];
        item.label = names[i];
        item.disabled = !offered[i];
        item.id = static_cast<ImGuiID>(i + 1);  // the mode, so an entry keeps its id when others come and go
        list.modes[list.count] = mode;
        if (isCurrent)
            list.current = list.count;
        ++list.count;
    }
    return list;
}

} // namespace

bool KeybindModeSelector(const char* label, BindMode* mode, const KeybindModeSelectorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const bool hasValue = mode != nullptr && static_cast<int>(*mode) >= 0 && static_cast<int>(*mode) < kModeCount;
    const ModeList list = BuildModes(hasValue ? *mode : BindMode::Hold, hasValue, options);
    const std::span<const ChoiceItem> items(list.items, static_cast<std::size_t>(list.count));
    int index = list.current;

    // Segments when they fit the width rule 6 gives, a dropdown otherwise.
    const internal::NextItemData next = internal::TakeNextItemData();
    const float content = internal::SegmentStripContentWidth(label, {items, {}}, options.size, false);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = internal::ResolveItemWidth(options.width, next, std::min(content, available));
    internal::ApplyNextItemData(next);
    bool written = false;
    if (width + 0.5f >= content)
    {
        internal::SegmentStripDesc desc;
        desc.kind = internal::SegmentStripKind::Choice;
        desc.roving = true;
        desc.dragSelect = true;
        desc.collapseToIcons = false;
        desc.size = options.size;
        desc.width = options.width;
        desc.readOnly = options.readOnly || mode == nullptr;
        desc.mixed = options.mixed;
        desc.error = options.error;
        desc.helpText = options.helpText;
        written = internal::SegmentStrip(label, {items, {}}, mode != nullptr ? &index : nullptr, {}, desc);
    }
    else
    {
        DropdownOptions dropdown;
        dropdown.size = options.size;
        dropdown.width = options.width;
        dropdown.readOnly = options.readOnly || mode == nullptr;
        dropdown.mixed = options.mixed;
        dropdown.error = options.error;
        dropdown.helpText = options.helpText;
        written = Dropdown(label, mode != nullptr ? &index : nullptr, items, dropdown);
    }
    if (written && mode != nullptr && index >= 0 && index < list.count && !list.items[index].disabled)
    {
        *mode = list.modes[index];
        return true;
    }
    return false;
}

} // namespace facet
