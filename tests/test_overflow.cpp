// Overflow fitter (src/internal/overflow.h): which items of a row stay visible.

#include <array>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include <doctest/doctest.h>

#include "internal/overflow.h"

using facet::internal::FitOverflow;
using facet::internal::OverflowItem;
using facet::internal::OverflowRequiredWidth;
using facet::internal::OverflowResult;
using facet::internal::OverflowState;

namespace
{

constexpr float kGap = 4.0f;
constexpr float kButton = 30.0f;

// Five items of 50 px with equal priority: 5 * 50 + 4 * 4 = 266 px with everything visible.
std::array<OverflowItem, 5> EqualItems()
{
    std::array<OverflowItem, 5> items;
    for (OverflowItem& item : items)
        item.width = 50.0f;
    return items;
}

int CountVisible(const bool* visible, int count)
{
    int shown = 0;
    for (int i = 0; i < count; ++i)
        shown += visible[i] ? 1 : 0;
    return shown;
}

} // namespace

TEST_CASE("Overflow: a row that fits shows every item and no button")
{
    const auto items = EqualItems();
    bool visible[5] = {};
    const OverflowResult fit = FitOverflow(items, 266.0f, kGap, kButton, 0.0f, nullptr, visible);
    CHECK(fit.hiddenCount == 0);
    CHECK_FALSE(fit.showButton);
    CHECK(fit.usedWidth == doctest::Approx(266.0f));
    CHECK(CountVisible(visible, 5) == 5);
}

TEST_CASE("Overflow: the button width is reserved and the tail goes first")
{
    const auto items = EqualItems();
    bool visible[5] = {};
    // One pixel short: hiding one item gives 4 * 50 + 3 * 4 + 4 + 30 = 246 px.
    const OverflowResult fit = FitOverflow(items, 265.0f, kGap, kButton, 0.0f, nullptr, visible);
    CHECK(fit.hiddenCount == 1);
    CHECK(fit.showButton);
    CHECK(fit.usedWidth == doctest::Approx(246.0f));
    CHECK(visible[0]);
    CHECK(visible[3]);
    CHECK_FALSE(visible[4]);
    CHECK(OverflowRequiredWidth(items, 1, kGap, kButton) == doctest::Approx(246.0f));

    // 245 px no longer holds four items plus the button: three are left (192 px).
    const OverflowResult tighter = FitOverflow(items, 245.0f, kGap, kButton, 0.0f, nullptr, visible);
    CHECK(tighter.hiddenCount == 2);
    CHECK(tighter.usedWidth == doctest::Approx(3 * 50.0f + 2 * kGap + kGap + kButton));
    CHECK_FALSE(visible[3]);
    CHECK_FALSE(visible[4]);
}

TEST_CASE("Overflow: lower priorities are hidden first, whatever their position")
{
    std::array<OverflowItem, 4> items = {{{60.0f, 5}, {60.0f, 0}, {60.0f, 9}, {60.0f, 5}}};
    bool visible[4] = {};
    // All four: 252 px. Room for two plus the button: 2 * 60 + 4 + 4 + 30 = 158 px.
    const OverflowResult fit = FitOverflow(items, 160.0f, kGap, kButton, 0.0f, nullptr, visible);
    CHECK(fit.hiddenCount == 2);
    CHECK_FALSE(visible[1]);  // priority 0 goes first
    CHECK_FALSE(visible[3]);  // then the later of the two priority 5 items
    CHECK(visible[0]);
    CHECK(visible[2]);
}

TEST_CASE("Overflow: hiding is monotonic as the width shrinks")
{
    std::array<OverflowItem, 6> items = {{{40.0f, 1}, {70.0f, 3}, {25.0f, 0}, {90.0f, 2}, {55.0f, 1}, {30.0f, 4}}};
    bool visible[6] = {};
    int lastHidden = 0;
    for (float width = 400.0f; width >= 0.0f; width -= 0.5f)
    {
        const OverflowResult fit = FitOverflow(items, width, kGap, kButton, 0.0f, nullptr, visible);
        CHECK(fit.hiddenCount >= lastHidden);
        CHECK(fit.showButton == (fit.hiddenCount > 0));
        CHECK(CountVisible(visible, 6) == 6 - fit.hiddenCount);
        // What is shown fits, unless not even the button fits.
        if (fit.hiddenCount < 6)
            CHECK(fit.usedWidth <= width + 0.01f);
        lastHidden = fit.hiddenCount;
    }
    CHECK(lastHidden == 6);
}

TEST_CASE("Overflow: a width that wobbles at the threshold does not flicker")
{
    const auto items = EqualItems();
    bool visible[5] = {};
    OverflowState state;
    const float hysteresis = 12.0f;
    // Starts wide: nothing hidden.
    CHECK(FitOverflow(items, 300.0f, kGap, kButton, hysteresis, &state, visible).hiddenCount == 0);
    // Shrinks below 266: one item hides at once.
    CHECK(FitOverflow(items, 265.0f, kGap, kButton, hysteresis, &state, visible).hiddenCount == 1);
    // Wobbles around the threshold: the item stays hidden.
    for (float width : {266.0f, 265.0f, 267.0f, 270.0f, 266.0f, 277.0f})
        CHECK(FitOverflow(items, width, kGap, kButton, hysteresis, &state, visible).hiddenCount == 1);
    // Back with 12 px to spare: it returns.
    CHECK(FitOverflow(items, 278.0f, kGap, kButton, hysteresis, &state, visible).hiddenCount == 0);
    CHECK(state.hidden == 0);

    // Without the state the decision follows the width exactly.
    CHECK(FitOverflow(items, 266.0f, kGap, kButton, hysteresis, nullptr, visible).hiddenCount == 0);
}

TEST_CASE("Overflow: hysteresis never keeps more items than fit")
{
    const auto items = EqualItems();
    bool visible[5] = {};
    OverflowState state;
    state.hidden = 0;
    // A large hysteresis cannot keep an item visible that does not fit.
    const OverflowResult fit = FitOverflow(items, 200.0f, kGap, kButton, 100.0f, &state, visible);
    CHECK(fit.usedWidth <= 200.0f);
    CHECK(state.hidden == fit.hiddenCount);
    // A stale state larger than the item count is clamped.
    state.hidden = 99;
    const OverflowResult wide = FitOverflow(items, 1000.0f, kGap, kButton, 10.0f, &state, visible);
    CHECK(wide.hiddenCount == 0);
}

TEST_CASE("Overflow: edge cases")
{
    bool visible[5] = {true, true, true, true, true};
    // No items: nothing to hide, no button.
    const OverflowResult none = FitOverflow({}, 100.0f, kGap, kButton, 0.0f, nullptr, visible);
    CHECK(none.hiddenCount == 0);
    CHECK_FALSE(none.showButton);
    CHECK(none.usedWidth == 0.0f);

    // Not even the button fits: everything is hidden, the button still shows.
    const auto items = EqualItems();
    const OverflowResult tiny = FitOverflow(items, 10.0f, kGap, kButton, 0.0f, nullptr, visible);
    CHECK(tiny.hiddenCount == 5);
    CHECK(tiny.showButton);
    CHECK(CountVisible(visible, 5) == 0);

    // Negative and NaN inputs count as 0.
    std::array<OverflowItem, 2> odd = {{{-20.0f, 0}, {10.0f, 0}}};
    bool two[2] = {};
    CHECK(FitOverflow(odd, 14.0f, kGap, kButton, 0.0f, nullptr, two).hiddenCount == 0);
    CHECK(FitOverflow(odd, std::numeric_limits<float>::quiet_NaN(), kGap, kButton, 0.0f, nullptr, two).hiddenCount == 2);
}

TEST_CASE("Overflow: rows longer than the inline buffer are fitted the same way")
{
    std::vector<OverflowItem> items(100, OverflowItem{10.0f, 0});
    std::unique_ptr<bool[]> visible(new bool[items.size()]);
    // 100 * 10 + 99 * 1 = 1099 px; room for 50: 50 * 10 + 49 + 1 + 20 = 570.
    const OverflowResult fit =
        FitOverflow(items, 570.0f, 1.0f, 20.0f, 0.0f, nullptr, std::span<bool>(visible.get(), items.size()));
    CHECK(fit.hiddenCount == 50);
    CHECK(visible[49]);
    CHECK_FALSE(visible[50]);
}
