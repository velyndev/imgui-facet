// KeyCap (facet/widgets/display.h): a chord drawn as key caps (one per modifier, one for the key)
// on the text baseline of the line it sits in; ImGuiKey_None draws Strings::unbound. With `live`
// the caps show the held look while the chord is down. Not interactive: one item, not a Tab stop.

#include "facet/widgets/display.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/input.h"
#include "internal/control.h"
#include "widgets/display/display_look.h"

namespace facet
{

void KeyCap(ImGuiKeyChord chord, const KeyCapOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    const internal::DisplayMarkMetrics m = internal::DisplayMarkLook(options.size);
    const float width = internal::DisplayCapsWidth(chord, m);
    const internal::DisplayInlineBox box = internal::DisplayPlaceInline(
        window, internal::DisplayAnonymousId(window, "##KeyCap"), ImVec2(width, m.height), internal::DisplayMarkBaseline(m),
        m.lineStyle);
    if (!box.visible)
        return;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool pressed = options.live && chord != ImGuiKey_None && !disabled && IsChordDown(chord);
    internal::DrawDisplayCaps(window->DrawList, box.rect.Min, box.baseline, chord, m, pressed, disabled);
}

} // namespace facet
