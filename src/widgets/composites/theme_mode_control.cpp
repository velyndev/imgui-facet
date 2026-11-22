// ThemeModeControl: a SegmentedControl with the segments Light, Dark and System (sun, moon and
// monitor icons, so the segments can collapse to icons when narrow). It adds only the mapping
// between the segment index and ThemeMode and, with `apply`, one SetThemeMode call on the frame
// the mode is written; the core then runs its theme transition (or switches at once while motion
// is reduced). Everything else (the label in front, the pill, keys, focus, disabled and read-only
// looks, forced capture states) is the segmented control's.

#include "facet/widgets/composites.h"

#include <imgui.h>

#include "facet/core/theme.h"
#include "facet/widgets/selection.h"
#include "internal/control.h"

namespace facet
{

bool ThemeModeControl(const char* label, ThemeMode* mode, const ThemeModeControlOptions& options)
{
    if (internal::ControlWindow() == nullptr)
        return false;
    const Strings& s = GetStrings();
    const ChoiceItem items[] = {
        {s.themeLight, Icon("sun")},
        {s.themeDark, Icon("moon")},
        {s.themeSystem, Icon("monitor")},
    };
    // A value outside the enum shows no segment as chosen until the user picks one.
    int index = -1;
    if (mode != nullptr)
    {
        const int value = static_cast<int>(*mode);
        index = value >= 0 && value < 3 ? value : -1;
    }
    SegmentedControlOptions segmented;
    segmented.size = options.size;
    segmented.width = options.width;
    segmented.readOnly = options.readOnly || mode == nullptr;
    if (!SegmentedControl(label, &index, items, segmented) || mode == nullptr || index < 0 || index > 2)
        return false;
    const ThemeMode chosen = static_cast<ThemeMode>(index);
    if (chosen == *mode)
        return false;
    *mode = chosen;
    if (options.apply)
        SetThemeMode(chosen);
    return true;
}

} // namespace facet
