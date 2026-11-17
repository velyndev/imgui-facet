// Demo page: Selection pill. A prototype segmented control and a prototype list, both built only
// from the item behaviour, the focus ring and the selection pill, so the one gesture they share
// can be compared; a reduce motion switch shows the pill moving at once instead.

#include <algorithm>
#include <cstdio>
#include <iterator>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/row_layout.h"
#include "internal/selection_pill.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kHovered,
    kFocused,
};

constexpr const char* kSegments[] = {"Day", "Week", "Month", "Year"};
constexpr const char* kEntries[] = {"First entry", "Second entry", "Third entry", "Fourth entry", "Fifth entry"};
constexpr int kEntryCount = static_cast<int>(std::size(kEntries));

struct PillPageState
{
    int segment = 1;
    int entry = 0;
    bool reduce = false;
};

PillPageState page;

// A list whose rows share the selection pill: Up and Down move between rows (Dear ImGui
// navigation); Enter, Space or a click selects.
void List(facet::demo::PageCtx& ctx)
{
    const facet::Theme& t = facet::T();
    const facet::Colors& c = t.colors;
    const float rowHeight = facet::ControlHeight(facet::ControlSize::Regular);
    const float width = facet::Px(t.spacing.xxxl * 7.0f);
    const float radius = facet::Px(t.radii.control);
    const float inset = facet::Px(t.spacing.md);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float pad = facet::Px(t.spacing.xs);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float total = (rowHeight + spacing) * static_cast<float>(kEntryCount) - spacing;
    facet::FillRect(drawList, ImVec2(pos.x - pad, pos.y - pad), ImVec2(pos.x + width + pad, pos.y + total + pad),
                    facet::ToU32(c.bgSecondary), facet::Px(t.radii.card));

    facet::internal::SelectionPill pill;
    pill.Begin(ImGui::GetID("##list_pill"), drawList,
               ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
    char probe[32];
    for (int i = 0; i < kEntryCount; ++i)
    {
        ImGui::PushID(i);
        if (ctx.capture && i == 3)
        {
            if (ctx.captureState == kHovered)
                ctx.ForceNextItemState(ForcedItemState::Hovered);
            else if (ctx.captureState == kFocused)
                ctx.ForceNextItemState(ForcedItemState::Focused);
        }
        const facet::internal::ItemState row =
            facet::internal::ItemBehavior(ImGui::GetID("row"), ImVec2(width, rowHeight));
        std::snprintf(probe, sizeof(probe), "pill.entry%d", i);
        ctx.Probe(probe);
        if (row.pressed)
            page.entry = i;
        if (i == page.entry)
            pill.Target(row.rect);
        if (row.visible)
        {
            const float feedback = i == page.entry ? 0.0f : std::max(row.hoverAmount, row.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 fill = facet::Mix(c.fill[2], c.fill[1], row.pressAmount);
                facet::FillRect(drawList, row.rect.Min, row.rect.Max,
                                facet::ToU32(facet::WithAlpha(fill, fill.w * feedback)), radius);
            }
            facet::PushTextStyle(facet::TextStyle::Body);
            drawList->AddText(ImVec2(row.rect.Min.x + inset, facet::CenteredTextY(row.rect.Min.y, rowHeight)),
                              facet::ToU32(c.label), kEntries[i]);
            facet::PopTextStyle();
            facet::internal::DrawItemFocusRing(row, radius);
        }
        ImGui::PopID();
    }
    pill.End(facet::ToU32(c.accentSubtle), radius);
}

} // namespace

FACET_DEMO_PAGE("pill", "Selection pill", "Blocks", 70, "default", "hovered", "focused")
{
    facet::demo::PageHeader("Selection pill",
                            "The identity motif: one soft highlight behind the selected entry that springs to the "
                            "next one when the selection changes. Tabs, the sidebar, segmented controls and lists "
                            "share it; the navigation of this gallery uses it too.");
    Note("Prototype controls built only from the item behaviour, focus ring and selection pill blocks. The leading "
         "edge of the pill moves a little faster than the trailing one, so it stretches on the way and settles on "
         "the new entry.");

    SectionHeader("Segmented control");
    if (ctx.capture)
    {
        if (ctx.captureState == kHovered)
            ctx.ForceNextItemState(ForcedItemState::Hovered, 2);
        else if (ctx.captureState == kFocused)
            ctx.ForceNextItemState(ForcedItemState::Focused, 1);
    }
    facet::demo::ProtoSegmented(ctx, "segments", &page.segment, kSegments, "pill.segments");
    Note("Selected: %s. One Tab stop; Left and Right move the selection.", kSegments[page.segment]);

    SectionHeader("List");
    List(ctx);
    Note("Selected: %s.", kEntries[page.entry]);
    ctx.RevealInCapture(0.5f);

    SectionHeader("Motion");
    facet::internal::BeginRow("Reduce motion", {.description = "The pill then moves at once."});
    page.reduce = facet::T().motion.reduceMotion;
    if (facet::demo::ProtoToggle(ctx, "##value", &page.reduce, "pill.reduce"))
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = page.reduce;
        facet::SetTheme(theme, false);
    }
    facet::internal::EndRow();
}

FACET_DEMO_SELFTEST("pill")
{
    test.Do([] { page = {}; });
    test.Click("pill.segments.3");
    test.ExpectNear("a click selects the segment", [] { return page.segment; }, 3.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left selects the one before", [] { return page.segment; }, 2.0, 0.0);
    test.ExpectFocused("pill.segments.2");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("and stops at the first", [] { return page.segment; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right moves back", [] { return page.segment; }, 1.0, 0.0);

    // One Tab stop: Tab leaves the control for the list.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("pill.entry0");
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectFocused("pill.entry2");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter selects the focused entry", [] { return page.entry; }, 2.0, 0.0);
    test.Click("pill.entry4");
    test.ExpectNear("a click selects an entry", [] { return page.entry; }, 4.0, 0.0);

    // Reduced motion, switched on and off again so later pages keep the default.
    test.Click("pill.reduce");
    test.Expect("the switch turns reduced motion on", [] { return facet::T().motion.reduceMotion; });
    test.Click("pill.entry1");
    test.ExpectNear("the selection still changes", [] { return page.entry; }, 1.0, 0.0);
    test.Click("pill.reduce");
    test.Expect("and off again", [] { return !facet::T().motion.reduceMotion; });
}
