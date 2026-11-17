// Demo page: Overflow. A row of prototype buttons fitted into a width that a prototype slider
// sets: the overflow fitter (internal/overflow.h) decides which stay, the rest go behind a
// "More" button. The current item has the highest priority, so it never leaves the row.

#include <algorithm>
#include <cstdio>
#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/overflow.h"
#include "internal/row_layout.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::SectionHeader;

enum CaptureState
{
    kDefault,
    kNarrow,
    kOpen,
};

constexpr const char* kItems[] = {"One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight"};
constexpr int kItemCount = static_cast<int>(std::size(kItems));
constexpr double kMinRow = 120.0;     // units
constexpr double kNarrowRow = 240.0;  // units, the narrow capture
constexpr int kCurrentPriority = 1;   // above every other item

struct OverflowPageState
{
    double row = 0.0;  // units; 0 until the first frame sets the full width
    int current = 0;
    bool open = false;
    facet::internal::OverflowState fit;
    bool visible[kItemCount] = {};
    int hidden = 0;
    float required = 0.0f;   // units the row needs with every item
    float available = 0.0f;  // units the slider offers at most
};

OverflowPageState page;

float ButtonWidth(const char* label)
{
    facet::PushTextStyle(facet::TextStyle::Body);
    const float width = ImGui::CalcTextSize(label, nullptr, true).x + facet::Px(facet::T().spacing.md) * 2.0f;
    facet::PopTextStyle();
    return width;
}

} // namespace

FACET_DEMO_PAGE("overflow", "Overflow", "Blocks", 40, "default", "narrow", "open")
{
    const facet::Theme& t = facet::T();
    facet::demo::PageHeader("Overflow",
                            "Fitting a row of items into the available width. Items that do not fit move behind a "
                            "\"More\" button, lowest priority first, and come back only with room to spare, so a "
                            "width at the threshold does not make them blink.");
    Note("Prototype buttons fitted by the overflow block. The current item has the highest priority and stays in the "
         "row; choose another from the row or from the overflow list.");

    page.available = facet::AvailableWidthUnits();
    if (page.row <= 0.0)
        page.row = page.available;
    if (ctx.capture)
    {
        page.row = ctx.captureState == kDefault ? page.available : kNarrowRow;
        page.open = ctx.captureState == kOpen;
    }
    page.row = std::clamp(page.row, kMinRow, std::max(kMinRow, static_cast<double>(page.available)));

    SectionHeader("Row width");
    facet::internal::BeginRow("Width");
    facet::demo::ProtoSlider(ctx, "##width", &page.row,
                             {.min = kMinRow, .max = std::max(kMinRow + 1.0, static_cast<double>(page.available)),
                              .step = 1.0},
                             "overflow.width");
    facet::internal::EndRow();

    // Measure, then fit.
    const float gap = facet::Px(t.spacing.sm);
    facet::internal::OverflowItem items[kItemCount];
    for (int i = 0; i < kItemCount; ++i)
        items[i] = {ButtonWidth(kItems[i]), i == page.current ? kCurrentPriority : 0};
    char moreLabel[32];
    std::snprintf(moreLabel, sizeof(moreLabel), "More (%d)###more", kItemCount);
    const float buttonWidth = ButtonWidth(moreLabel);
    const float width = facet::Px(static_cast<float>(page.row));
    const facet::internal::OverflowResult fit =
        facet::internal::FitOverflow(items, width, gap, buttonWidth, facet::Px(t.spacing.lg), &page.fit, page.visible);
    page.hidden = fit.hiddenCount;
    page.required = facet::internal::OverflowRequiredWidth(items, 0, gap, buttonWidth) / facet::Scale();

    SectionHeader("The row");
    // The width the row may use as a faint band, so the fit can be judged.
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    const float pad = facet::Px(t.spacing.xs);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    facet::FillRect(drawList, ImVec2(pos.x - pad, pos.y - pad), ImVec2(pos.x + width + pad, pos.y + height + pad),
                    facet::ToU32(t.colors.fill[3]), facet::Px(t.radii.control) + pad);
    bool first = true;
    char probe[32];
    for (int i = 0; i < kItemCount; ++i)
    {
        if (!page.visible[i])
            continue;
        if (!first)
            ImGui::SameLine(0.0f, gap);
        first = false;
        std::snprintf(probe, sizeof(probe), "overflow.item%d", i);
        if (facet::demo::ProtoButton(ctx, kItems[i], probe))
            page.current = i;
    }
    if (fit.showButton)
    {
        if (!first)
            ImGui::SameLine(0.0f, gap);
        std::snprintf(moreLabel, sizeof(moreLabel), "More (%d)###more", fit.hiddenCount);
        if (facet::demo::ProtoButton(ctx, moreLabel, "overflow.more"))
            page.open = !page.open;
    }
    else
    {
        page.open = false;
    }
    Note("Current: %s. %d of %d items in the row%s.", kItems[page.current], kItemCount - fit.hiddenCount, kItemCount,
         fit.showButton ? ", the rest behind More" : "");

    if (page.open)
    {
        SectionHeader("Overflow list");
        for (int i = 0; i < kItemCount; ++i)
        {
            if (page.visible[i])
                continue;
            std::snprintf(probe, sizeof(probe), "overflow.list%d", i);
            char label[32];
            std::snprintf(label, sizeof(label), "%s##list", kItems[i]);
            if (facet::demo::ProtoButton(ctx, label, probe))
            {
                page.current = i;
                page.open = false;
            }
        }
        ctx.RevealInCapture(0.7f);
    }
}

FACET_DEMO_SELFTEST("overflow")
{
    using T = facet::demo::Target;
    test.Do([] { page = {}; });

    test.Drag({T::Item("overflow.width.track", 1.0f), T::Item("overflow.width.track", -0.5f)});
    test.ExpectNear("the width slider reaches its minimum", [] { return page.row; }, kMinRow, 0.0);
    test.Expect("a narrow row hides items", [] { return page.hidden > 0; });
    test.ExpectVisible("overflow.more");
    test.Expect("the current item stays in the row", [] { return page.visible[page.current]; });

    test.Click("overflow.more");
    test.Expect("More opens the overflow list", [] { return page.open; });
    test.Click("overflow.list7");
    test.ExpectNear("choosing from the list makes it current", [] { return page.current; }, 7.0, 0.0);
    test.Expect("the list closes", [] { return !page.open; });
    test.Expect("and the chosen item is now in the row", [] { return page.visible[7]; });

    // By keyboard: More opens and closes with Space.
    test.Click("overflow.item7");
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus where it is
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("overflow.more");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space opens the list", [] { return page.open; });
    test.KeyPress(ImGuiKey_Space);
    test.Expect("and closes it", [] { return !page.open; });

    // Full width: everything fits when the slider allows the width the row needs. Hidden items
    // return with the hysteresis (spacing.lg) to spare.
    test.Click("overflow.width");
    test.KeyPress(ImGuiKey_End);
    test.Expect("at full width the row hides items only when they cannot fit", [] {
        if (page.available < page.required)
            return page.hidden > 0;
        return page.available < page.required + facet::T().spacing.lg || page.hidden == 0;
    });
}
