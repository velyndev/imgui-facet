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

// Widths of the items sorted by drop rank (rank 0 is hidden first), in a buffer on the stack
// for typical rows and on the heap beyond kOverflowInlineItems.
class RankedWidths
{
public:
    explicit RankedWidths(std::span<const OverflowItem> items)
    {
        const std::size_t count = items.size();
        if (count > inline_.size())
        {
            heap_.reset(new (std::nothrow) float[count]);
            if (heap_ == nullptr)
            {
                failed_ = true;  // out of memory: the caller shows every item
                return;
            }
            widths_ = heap_.get();
        }
        else
        {
            widths_ = inline_.data();
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            // The rank is the number of items dropped before item i: lower priorities, and equal
            // priorities further to the end.
            std::size_t rank = 0;
            for (std::size_t j = 0; j < count; ++j)
            {
                if (items[j].priority < items[i].priority || (items[j].priority == items[i].priority && j > i))
                    ++rank;
            }
            widths_[rank] = std::max(0.0f, items[i].width);
        }
        count_ = static_cast<int>(count);
    }

    // The width of the items that stay with `hidden` items dropped, with gaps and button.
    float Required(int hidden, float gap, float buttonWidth) const
    {
        hidden = std::clamp(hidden, 0, count_);
        float width = 0.0f;
        for (int rank = hidden; rank < count_; ++rank)
            width += widths_[rank];
        const int shown = count_ - hidden;
        if (shown > 1)
            width += gap * static_cast<float>(shown - 1);
        if (hidden > 0)
            width += buttonWidth + (shown > 0 ? gap : 0.0f);
        return width;
    }

    int Count() const { return count_; }
    bool Failed() const { return failed_; }

    // The drop rank of item `index`.
    static int RankOf(std::span<const OverflowItem> items, std::size_t index)
    {
        int rank = 0;
        for (std::size_t j = 0; j < items.size(); ++j)
        {
            if (items[j].priority < items[index].priority || (items[j].priority == items[index].priority && j > index))
                ++rank;
        }
        return rank;
    }

private:
    std::array<float, kOverflowInlineItems> inline_{};
    std::unique_ptr<float[]> heap_;
    float* widths_ = nullptr;
    int count_ = 0;
    bool failed_ = false;
};

} // namespace

float OverflowRequiredWidth(std::span<const OverflowItem> items, int hidden, float gap, float buttonWidth)
{
    const RankedWidths ranked(items);
    return ranked.Required(hidden, std::max(0.0f, gap), std::max(0.0f, buttonWidth));
}

OverflowResult FitOverflow(std::span<const OverflowItem> items, float available, float gap, float buttonWidth,
                           float hysteresis, OverflowState* state, std::span<bool> visible)
{
    gap = std::max(0.0f, gap);
    buttonWidth = std::max(0.0f, buttonWidth);
    hysteresis = std::max(0.0f, hysteresis);
    if (!(available >= 0.0f))
        available = 0.0f;

    OverflowResult result;
    const RankedWidths ranked(items);
    const int count = ranked.Count();
    if (ranked.Failed())
    {
        for (std::size_t i = 0; i < items.size() && i < visible.size(); ++i)
            visible[i] = true;
        return result;
    }
    if (count == 0)
    {
        if (state != nullptr)
            state->hidden = 0;
        return result;
    }

    const auto fits = [&](int hidden, float spare) {
        return ranked.Required(hidden, gap, buttonWidth) + spare <= available + kFitTolerance;
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

    for (std::size_t i = 0; i < items.size() && i < visible.size(); ++i)
        visible[i] = RankedWidths::RankOf(items, i) >= hidden;

    result.hiddenCount = hidden;
    result.showButton = hidden > 0;
    result.usedWidth = ranked.Required(hidden, gap, buttonWidth);
    return result;
}

} // namespace facet::internal
