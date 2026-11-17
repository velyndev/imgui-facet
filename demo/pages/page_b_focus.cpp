// Demo page: Focus ring and keyboard modality. Prototype buttons on every surface the ring has
// to stay visible on, and a scrolling region whose edge cuts the ring of a control scrolled
// partly out of view.

#include <cstdio>
#include <iterator>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/focus.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kFocused,
    kClipped,
};

struct FocusPageState
{
    int pressed = -1;      // the last surface button pressed
    int listPressed = -1;  // the last row of the scrolling region pressed
};

FocusPageState page;

struct Surface
{
    const char* name;
    ImVec4 facet::Colors::*color;
};

constexpr Surface kSurfaces[] = {
    {"bg", &facet::Colors::bg},
    {"bgSecondary", &facet::Colors::bgSecondary},
    {"bgTertiary", &facet::Colors::bgTertiary},
    {"bgElevated", &facet::Colors::bgElevated},
};
constexpr int kListRows = 8;

// One surface swatch with a prototype button on it.
void SurfaceCard(facet::demo::PageCtx& ctx, int index, float width)
{
    const facet::Theme& t = facet::T();
    const Surface& surface = kSurfaces[index];
    const float padding = facet::Px(t.spacing.lg);
    const float caption = facet::MeasureText(facet::TextStyle::Footnote, surface.name).y;
    // Between the caption and the button: room for the focus ring plus a small gap, so the ring
    // never touches the caption's descenders.
    const float captionGap = facet::internal::FocusRingOutset() + facet::Px(t.spacing.xs);
    const float height = facet::ControlHeight(facet::ControlSize::Regular) + padding * 2.0f + caption + captionGap;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max(pos.x + width, pos.y + height);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    facet::FillRect(drawList, pos, max, facet::ToU32(t.colors.*surface.color), facet::Px(t.radii.card));
    facet::StrokeRect(drawList, pos, max, facet::ToU32(t.colors.separator), facet::Px(t.radii.card),
                      facet::HairlineThickness());
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(width, height));
    facet::PushTextStyle(facet::TextStyle::Footnote);
    drawList->AddText(ImVec2(pos.x + padding, pos.y + padding), facet::ToU32(t.colors.labelSecondary), surface.name);
    facet::PopTextStyle();
    ImGui::SetCursorScreenPos(ImVec2(pos.x + padding, pos.y + padding + caption + captionGap));
    if (ctx.capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    char label[64];
    char probe[32];
    std::snprintf(label, sizeof(label), "Prototype button##%s", surface.name);
    std::snprintf(probe, sizeof(probe), "focus.surface%d", index);
    if (facet::demo::ProtoButton(ctx, label, probe))
        page.pressed = index;
    ImGui::EndGroup();
}

} // namespace

FACET_DEMO_PAGE("focus", "Focus", "Blocks", 20, "default", "focused", "clipped")
{
    const facet::Theme& t = facet::T();
    facet::demo::PageHeader("Focus ring",
                            "Where the keyboard focus is, shown only to keyboard users: the ring appears after Tab, "
                            "the arrows, Space, Enter or Escape, and goes away after a mouse click. It follows the "
                            "control's corners just outside its edge.");
    Note("Prototype buttons built only from the item behaviour and focus ring blocks. Keyboard modality: %s.",
         facet::internal::KeyboardModality() ? "on" : "off");

    SectionHeader("On every surface");
    const float gap = facet::Px(t.spacing.md);
    const float right = facet::demo::ContentRight();
    const float cardWidth = facet::Px(t.spacing.xxxl * 6.0f);
    for (int i = 0; i < static_cast<int>(std::size(kSurfaces)); ++i)
    {
        if (i > 0)
            facet::demo::SameLineIfFits(right, cardWidth, gap);
        SurfaceCard(ctx, i, cardWidth);
    }
    if (page.pressed >= 0)
        Note("Last pressed: the button on %s.", kSurfaces[page.pressed].name);
    else
        Note("No button pressed yet.");

    SectionHeader("Inside a scrolling region");
    Note("The ring is drawn with the region's clip rectangle, so a control scrolled partly out of view has its ring "
         "cut at the same edge as the control.");
    const float rowHeight = facet::ControlHeight(facet::ControlSize::Regular) + ImGui::GetStyle().ItemSpacing.y;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, t.colors.bgSecondary);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, facet::Px(t.radii.card));
    // Two and a half rows tall, so the third row is cut by the bottom edge until scrolled.
    ImGui::BeginChild("##focus_list", ImVec2(facet::Px(t.spacing.xxxl * 8.0f), rowHeight * 2.5f),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    for (int i = 0; i < kListRows; ++i)
    {
        char label[48];
        char probe[32];
        std::snprintf(label, sizeof(label), "Prototype row %d", i + 1);
        std::snprintf(probe, sizeof(probe), "focus.row%d", i);
        if (ctx.capture && ctx.captureState == kClipped && i == 2)
            ctx.ForceNextItemState(ForcedItemState::Focused);
        if (facet::demo::ProtoButton(ctx, label, probe))
            page.listPressed = i;
    }
    ImGui::EndChild();
    ctx.RevealInCapture(0.6f);
    if (page.listPressed >= 0)
        Note("Last pressed: row %d.", page.listPressed + 1);
    else
        Note("No row pressed yet.");
}

FACET_DEMO_SELFTEST("focus")
{
    test.Do([] { page = {}; });
    test.Click("focus.surface0");
    test.Expect("a click presses the button and leaves the ring off",
                [] { return page.pressed == 0 && !facet::internal::KeyboardModality(); });
    // After a click, the first Tab shows the focus where it is (Dear ImGui tabbing).
    test.KeyPress(ImGuiKey_Tab);
    test.Expect("Tab turns the keyboard modality on", [] { return facet::internal::KeyboardModality(); });
    test.ExpectFocused("focus.surface0");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("focus.surface1");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space presses the focused button", [] { return page.pressed == 1; });
    test.Click("focus.surface3");
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("focus.surface2");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter presses it", [] { return page.pressed == 2; });
    test.Click("focus.surface2");
    test.Expect("a mouse click turns the modality off again", [] { return !facet::internal::KeyboardModality(); });

    // Into the scrolling region: Tab reaches rows below its edge and scrolls them into view.
    test.Click("focus.row0");
    test.Expect("the first row presses", [] { return page.listPressed == 0; });
    for (int i = 0; i < 6; ++i)
        test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("focus.row5");
    test.ExpectVisible("focus.row5");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter presses the focused row", [] { return page.listPressed == 5; });
}
