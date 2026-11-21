// Overflow fitter (overflow.h).

#include "internal/overflow.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <new>

namespace facet::internal
{
namespace
{

// Widths compare with this tolerance in pixels, so a row that fits exactly is not hidden by
// rounding noise in the measured widths.
constexpr float kFitTolerance = 1e-3f;

// The drop rank of every entry (rank 0 is hidden first; -1 for a separator, which has no rank
// of its own), in a buffer on the stack for typical rows and on the heap beyond
// kOverflowInlineItems.
class Ranked
{
public:
    explicit Ranked(std::span<const OverflowItem> items) : items_(items)
    {
        const std::size_t count = items.size();
        if (count > inline_.size())
        {
            heap_.reset(new (std::nothrow) int[count]);
            if (heap_ == nullptr)
            {
                failed_ = true;  // out of memory: the caller shows every item
                return;
            }
            ranks_ = heap_.get();
        }
        else
        {
            ranks_ = inline_.data();
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            if (items[i].kind == OverflowKind::Separator)
            {
                ranks_[i] = -1;
                continue;
            }
            ++itemCount_;
            // The rank is the number of items dropped before item i: lower priorities, and equal
            // priorities further to the end. Separators do not count.
            int rank = 0;
            for (std::size_t j = 0; j < count; ++j)
            {
                if (items[j].kind == OverflowKind::Separator)
                    continue;
                if (items[j].priority < items[i].priority || (items[j].priority == items[i].priority && j > i))
                    ++rank;
            }
            ranks_[i] = rank;
        }
    }

    // Writes the visibility of every entry with the `hidden` lowest-ranked items dropped (when
    // `visible` is not empty) and returns the width of what shows, with gaps and the button. A
    // separator shows only between two visible items; of several separators between the same
    // two items the last one shows, and none shows at the start or the end of the row.
    float Resolve(int hidden, float gap, float buttonWidth, std::span<bool> visible, bool button = true) const
    {
        hidden = std::clamp(hidden, 0, itemCount_);
        float width = 0.0f;
        int shown = 0;
        bool itemBefore = false;
        int pending = -1;  // a separator waiting for a visible item after it
        const auto show = [&](std::size_t i) {
            width += std::max(0.0f, items_[i].width);
            ++shown;
            if (i < visible.size())
                visible[i] = true;
        };
        for (std::size_t i = 0; i < items_.size(); ++i)
        {
            if (i < visible.size())
                visible[i] = false;
            if (ranks_[i] < 0)
            {
                if (itemBefore)
                    pending = static_cast<int>(i);
                continue;
            }
            if (ranks_[i] < hidden)
                continue;
            if (pending >= 0)
                show(static_cast<std::size_t>(pending));
            pending = -1;
            show(i);
            itemBefore = true;
        }
        if (shown > 1)
            width += gap * static_cast<float>(shown - 1);
        if (hidden > 0 && button)
            width += buttonWidth + (shown > 0 ? gap : 0.0f);
        return width;
    }

    int ItemCount() const { return itemCount_; }
    bool Failed() const { return failed_; }

private:
    std::span<const OverflowItem> items_;
    std::array<int, kOverflowInlineItems> inline_{};
    std::unique_ptr<int[]> heap_;
    int* ranks_ = nullptr;
    int itemCount_ = 0;
    bool failed_ = false;
};

} // namespace

float OverflowRequiredWidth(std::span<const OverflowItem> items, int hidden, float gap, float buttonWidth)
{
    const Ranked ranked(items);
    if (ranked.Failed())
        return 0.0f;
    return ranked.Resolve(hidden, std::max(0.0f, gap), std::max(0.0f, buttonWidth), {});
}

namespace
{

OverflowResult FitOverflowRow(std::span<const OverflowItem> items, float available, float gap, float buttonWidth,
                              bool button, float hysteresis, OverflowState* state, std::span<bool> visible)
{
    gap = std::max(0.0f, gap);
    buttonWidth = std::max(0.0f, buttonWidth);
    hysteresis = std::max(0.0f, hysteresis);
    if (!(available >= 0.0f))
        available = 0.0f;

    OverflowResult result;
    const Ranked ranked(items);
    if (ranked.Failed())
    {
        for (std::size_t i = 0; i < items.size() && i < visible.size(); ++i)
            visible[i] = true;
        return result;
    }
    const int count = ranked.ItemCount();
    if (count == 0)
    {
        // Separators alone never show.
        for (std::size_t i = 0; i < items.size() && i < visible.size(); ++i)
            visible[i] = false;
        if (state != nullptr)
            state->hidden = 0;
        return result;
    }

    // Every comparison uses the width of the resolved row (separators included only where they
    // show), so hiding a whole group also gives back the room of its separator.
    const auto fits = [&](int hidden, float spare) {
        return ranked.Resolve(hidden, gap, buttonWidth, {}, button) + spare <= available + kFitTolerance;
    };

    // The fewest hidden items that fit. Hiding the first item adds the button, so hidden = 0 is
    // tested on its own; from 1 on, the required width only shrinks.
    int hidden = count;
    if (fits(0, 0.0f))
    {
        hidden = 0;
    }
    else
    {
        for (int k = 1; k <= count; ++k)
        {
            if (fits(k, 0.0f))
            {
                hidden = k;
                break;
            }
        }
    }

    // Items hidden last time come back only with `hysteresis` to spare.
    if (state != nullptr)
    {
        const int previous = std::clamp(state->hidden, 0, count);
        if (hidden < previous)
        {
            int kept = previous;
            for (int k = hidden; k < previous; ++k)
            {
                if (fits(k, hysteresis))
                {
                    kept = k;
                    break;
                }
            }
            hidden = kept;
        }
        state->hidden = hidden;
    }

    result.hiddenCount = hidden;
    result.showButton = button && hidden > 0;
    result.usedWidth = ranked.Resolve(hidden, gap, buttonWidth, visible, button);
    return result;
}

} // namespace

OverflowResult FitOverflow(std::span<const OverflowItem> items, float available, float gap, float buttonWidth,
                           float hysteresis, OverflowState* state, std::span<bool> visible)
{
    return FitOverflowRow(items, available, gap, buttonWidth, true, hysteresis, state, visible);
}

OverflowResult FitOverflowWithoutButton(std::span<const OverflowItem> items, float available, float gap,
                                        float hysteresis, OverflowState* state, std::span<bool> visible)
{
    return FitOverflowRow(items, available, gap, 0.0f, false, hysteresis, state, visible);
}

} // namespace facet::internal
