// Demo page: Item behaviour. Prototype controls built only from the item behaviour block
// (src/internal/item.h) and the focus ring: a push button, a disabled one, a repeat button, a
// toggle look and a small item with its minimum hit target, plus what the stock Dear ImGui item
// queries report about the button.

#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::NoteBeside;
using facet::demo::SectionHeader;

struct ItemPageState
{
    int presses = 0;
    int disabledPresses = 0;
    int repeats = 0;
    int smallPresses = 0;
    bool toggle = false;
    // What the stock queries said about the push button in the last frame.
    bool hovered = false;
    bool active = false;
    bool focused = false;
};

ItemPageState page;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kHovered,
    kPressed,
    kFocused,
};

void ForceOnButton(facet::demo::PageCtx& ctx)
{
    if (!ctx.capture)
        return;
    if (ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    else if (ctx.captureState == kPressed)
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Pressed);
    else if (ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
}

// A 12 unit square item: the visible box is small, the hit area is the minimum hit target.
void SmallItem(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const float side = facet::Px(t.spacing.md);
    const float target = facet::Px(t.metrics.minHitTarget);
    // Room for the hit area around the box, so it does not reach the text before it.
    ImGui::Dummy(ImVec2((target - side) * 0.5f, side));
    ImGui::SameLine(0.0f, 0.0f);
    const facet::internal::ItemState item = facet::internal::ItemBehavior(ImGui::GetID("small"), ImVec2(side, side));
    ctx.Probe("item.small");
    if (item.pressed)
        ++page.smallPresses;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // The hit area as a hairline outline, the box filled.
    facet::StrokeRect(drawList, item.hitRect.Min, item.hitRect.Max, facet::ToU32(c.separator), 0.0f,
                      facet::HairlineThickness());
    const ImVec4 fill = facet::Mix(c.labelSecondary, c.label, item.pressAmount);
    const float radius = facet::Px(t.radii.element) * 0.5f;
    facet::FillRect(drawList, item.rect.Min, item.rect.Max, facet::ToU32(fill), radius);
    facet::internal::DrawItemFocusRing(item, radius);
    ImGui::SameLine(0.0f, (target - side) * 0.5f + facet::Px(t.spacing.md));
    NoteBeside("Clicked %d times. The box is %.0f units, the area that takes the mouse %.0f units (the minHitTarget "
         "token, outlined).",
         page.smallPresses, t.spacing.md, t.metrics.minHitTarget);
}

} // namespace

FACET_DEMO_PAGE("item", "Item behaviour", "Blocks", 10, "default", "hovered", "pressed", "focused")
{
    const float gap = facet::Px(facet::T().spacing.lg);
    facet::demo::PageHeader("Item behaviour",
                            "The hover, press, focus and disabled logic every Facet widget shares. One call lays "
                            "out a control, registers it with Dear ImGui and returns its state, with animated "
                            "amounts for hover, press and focus.");
    Note("Prototype controls: built only from this block and the focus ring, to show the behaviour. The widgets "
         "themselves come in Phase 2.");

    SectionHeader("Push button");
    ForceOnButton(ctx);
    if (facet::demo::ProtoButton(ctx, "Prototype button", "item.button"))
        ++page.presses;
    // Stock Dear ImGui queries on the Facet item.
    page.hovered = ImGui::IsItemHovered();
    page.active = ImGui::IsItemActive();
    page.focused = ImGui::IsItemFocused();
    ImGui::SetItemTooltip("A tooltip set with ImGui::SetItemTooltip on the prototype button");
    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled();
    if (facet::demo::ProtoButton(ctx, "Disabled prototype", "item.disabled"))
        ++page.disabledPresses;
    ImGui::EndDisabled();
    // A forced capture state draws the look without real input, so the stock queries would
    // contradict the image: say what was forced instead.
    if (ctx.capture && ctx.captureState != kDefault)
        Note("The %s state is forced for this image: the button is drawn as it looks then, while the stock queries and "
             "the keyboard modality see no real input. The focus ring shows only after Tab, the arrows, Space or Enter.",
             ctx.captureState == kHovered ? "hovered" : (ctx.captureState == kPressed ? "pressed" : "focused"));
    else
        Note("Pressed %d times. Stock queries on it: IsItemHovered %s, IsItemActive %s, IsItemFocused %s. Keyboard "
             "modality %s: the focus ring shows only after Tab, the arrows, Space or Enter.",
             page.presses, page.hovered ? "yes" : "no", page.active ? "yes" : "no", page.focused ? "yes" : "no",
             facet::internal::KeyboardModality() ? "on" : "off");
    ctx.RevealInCapture(0.3f);

    SectionHeader("Repeat while held");
    if (facet::demo::ProtoButton(ctx, "Hold to count", "item.repeat", true))
        ++page.repeats;
    ImGui::SameLine(0.0f, gap);
    NoteBeside("Count: %d. Presses on the click, then repeats at the key repeat rate while the mouse button, Space or "
         "Enter is held.",
         page.repeats);

    SectionHeader("Toggle look");
    facet::demo::ProtoToggle(ctx, "Prototype toggle", &page.toggle, "item.toggle");
    ImGui::SameLine(0.0f, gap);
    NoteBeside("%s", page.toggle ? "On" : "Off");

    SectionHeader("Minimum hit target");
    SmallItem(ctx);
}

FACET_DEMO_SELFTEST("item")
{
    using T = facet::demo::Target;
    test.Do([] { page = {}; });
    test.Click("item.button");
    test.ExpectNear("a click presses the button once", [] { return page.presses; }, 1.0, 0.0);
    test.Hover(T::Item("item.button"), 0.2f);
    test.Expect("ImGui::IsItemHovered reports the Facet item", [] { return page.hovered; });
    test.MouseDown();
    test.Expect("ImGui::IsItemActive reports it while held", [] { return page.active; });
    test.MouseUp();
    test.ExpectNear("released inside: a second press", [] { return page.presses; }, 2.0, 0.0);
    test.Expect("a click leaves the keyboard modality off", [] { return !facet::internal::KeyboardModality(); });
    // After a click, the first Tab shows the focus where it is (Dear ImGui tabbing), then
    // Space and Enter activate.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("item.button");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Space and Enter press the focused button", [] { return page.presses; }, 4.0, 0.0);
    test.Expect("the keys turn the keyboard modality on", [] { return facet::internal::KeyboardModality(); });
    test.Expect("ImGui::IsItemFocused reports the focus", [] { return page.focused; });

    // Disabled: no press, and Tab passes over it.
    test.Click("item.disabled");
    test.ExpectNear("the disabled button does not press", [] { return page.disabledPresses; }, 0.0, 0.0);
    test.Click("item.button");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("item.repeat");

    // Repeat by keyboard and by mouse.
    test.Do([] { page.repeats = 0; });
    test.HoldKey(ImGuiKey_Space, 0.6f);
    test.Expect("holding Space repeats", [] { return page.repeats >= 4; });
    test.Do([] { page.repeats = 0; });
    test.MouseMove(T::Item("item.repeat"));
    test.MouseDown();
    test.WaitSeconds(0.6f);
    test.MouseUp();
    test.Expect("holding the mouse button repeats", [] { return page.repeats >= 4; });

    // The toggle by keyboard and mouse.
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("item.toggle");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space turns the toggle on", [] { return page.toggle; });
    test.Click("item.toggle");
    test.Expect("a click turns it off", [] { return !page.toggle; });

    // The hit area around the small box.
    test.MouseMove(T::Item("item.small", 1.4f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click beside the box, inside the hit area, presses it", [] { return page.smallPresses; }, 1.0,
                    0.0);
    test.MouseMove(T::Item("item.small", 3.5f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click outside the hit area does not", [] { return page.smallPresses; }, 1.0, 0.0);
    test.Click("item.toggle");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("item.small");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter presses it", [] { return page.smallPresses; }, 2.0, 0.0);
}
