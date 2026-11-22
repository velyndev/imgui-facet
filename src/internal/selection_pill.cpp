// Selection pill (selection_pill.h).

#include "internal/selection_pill.h"

#include <cmath>

#include "facet/core/anim.h"
#include "facet/core/draw.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

// A target that moves less than this many pixels keeps the direction of travel.
constexpr float kDirectionThreshold = 0.5f;
// Edges count as arrived within this many pixels.
constexpr float kArrived = 0.5f;
// Sub keys per generation: four edges.
constexpr int kEdgeCount = 4;
constexpr int kGenerations = 1024;

struct PillState
{
    ImRect target;          // relative to the origin
    bool hasTarget = false; // a target was given in the last frame that had a pill
    int lastFrame = -1;     // the frame of the last End that drew the pill
    bool held = false;      // that frame had no target: the pill was held in place (HoldThroughGap)
    int generation = 0;     // new sub keys when the pill starts over, so it starts on its target
    float directionX = 0.0f;
    float directionY = 0.0f;
};

struct SplitterPool
{
    ImDrawListSplitter splitters[kMaxPillDepth];
    int depth = 0;
};

SplitterPool& Pool()
{
    static SplitterPool pool;
    return pool;
}

float Sign(float value)
{
    return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
}

} // namespace

SelectionPill::~SelectionPill()
{
    // End was not called: put the draw list back together so later drawing is not lost.
    if (open_ && splitter_ != nullptr && drawList_ != nullptr)
    {
        splitter_->Merge(drawList_);
        --Pool().depth;
    }
}

void SelectionPill::Begin(ImGuiID groupId, ImDrawList* drawList, ImVec2 origin)
{
    id_ = groupId;
    drawList_ = drawList;
    origin_ = origin;
    hasTarget_ = false;
    hold_ = false;
    open_ = true;
    splitter_ = nullptr;
    SplitterPool& pool = Pool();
    if (drawList_ != nullptr && pool.depth < kMaxPillDepth)
    {
        splitter_ = &pool.splitters[pool.depth++];
        splitter_->Split(drawList_, 2);
        splitter_->SetCurrentChannel(drawList_, 1);
    }
}

void SelectionPill::Target(const ImRect& rect)
{
    target_ = rect;
    hasTarget_ = true;
}

void SelectionPill::HoldThroughGap()
{
    hold_ = true;
}

void PushPillLabelStyle(TextStyle style, bool selected)
{
    const TextStyleSpec& spec = T().type[style];
    FontWeight weight = spec.weight;
    if (selected && static_cast<int>(weight) < static_cast<int>(FontWeight::Semibold))
        weight = FontWeight::Semibold;
    // Entry labels are text sizes: the text face, as PushTextStyle uses for them.
    ImGui::PushFont(WeightFont(weight), spec.size);
}

ImRect SelectionPill::End(ImU32 color, float radius)
{
    drawn_ = ImRect();
    moving_ = false;
    if (!open_)
        return drawn_;
    open_ = false;
    if (splitter_ != nullptr)
        splitter_->SetCurrentChannel(drawList_, 0);

    PillState& state = GetState<PillState>(id_);
    const int frame = ImGui::GetFrameCount();
    // A held pill bridges one frame without a target: it stays where the frame before left it.
    const bool held = !hasTarget_ && hold_ && state.hasTarget && state.lastFrame == frame - 1 && !state.held;
    if ((hasTarget_ || held) && drawList_ != nullptr)
    {
        const ImRect target = held ? state.target
                                   : ImRect(target_.Min.x - origin_.x, target_.Min.y - origin_.y,
                                            target_.Max.x - origin_.x, target_.Max.y - origin_.y);
        state.held = held;
        // A pill that was not drawn in the previous frame starts on the new target.
        if (!state.hasTarget || state.lastFrame != frame - 1)
        {
            state.generation = (state.generation + 1) % kGenerations;
            state.directionX = 0.0f;
            state.directionY = 0.0f;
        }
        else
        {
            const ImVec2 now = target.GetCenter();
            const ImVec2 was = state.target.GetCenter();
            const ImVec2 move(now.x - was.x, now.y - was.y);
            if (std::fabs(move.x) > kDirectionThreshold)
                state.directionX = Sign(move.x);
            if (std::fabs(move.y) > kDirectionThreshold)
                state.directionY = Sign(move.y);
        }
        state.target = target;
        state.hasTarget = true;
        state.lastFrame = frame;

        const SpringSpec& standard = T().motion.standard;
        SpringSpec leading = standard;
        leading.response *= kPillLeadingResponse;
        // Moving right, the right edge leads; moving left, the left edge.
        const SpringSpec& minX = state.directionX < 0.0f ? leading : standard;
        const SpringSpec& maxX = state.directionX > 0.0f ? leading : standard;
        const SpringSpec& minY = state.directionY < 0.0f ? leading : standard;
        const SpringSpec& maxY = state.directionY > 0.0f ? leading : standard;
        const int sub = state.generation * kEdgeCount;
        ImRect pill;
        pill.Min.x = Spring(id_, sub + 0, target.Min.x, minX);
        pill.Max.x = Spring(id_, sub + 1, target.Max.x, maxX);
        pill.Min.y = Spring(id_, sub + 2, target.Min.y, minY);
        pill.Max.y = Spring(id_, sub + 3, target.Max.y, maxY);
        if (pill.Max.x < pill.Min.x)
            pill.Max.x = pill.Min.x;
        if (pill.Max.y < pill.Min.y)
            pill.Max.y = pill.Min.y;
        moving_ = std::fabs(pill.Min.x - target.Min.x) > kArrived || std::fabs(pill.Max.x - target.Max.x) > kArrived ||
                  std::fabs(pill.Min.y - target.Min.y) > kArrived || std::fabs(pill.Max.y - target.Max.y) > kArrived;
        drawn_ = ImRect(pill.Min.x + origin_.x, pill.Min.y + origin_.y, pill.Max.x + origin_.x, pill.Max.y + origin_.y);
        FillRect(drawList_, drawn_.Min, drawn_.Max, color, radius);
    }
    else
    {
        state.hasTarget = false;
    }

    if (splitter_ != nullptr)
    {
        splitter_->Merge(drawList_);
        --Pool().depth;
        splitter_ = nullptr;
    }
    return drawn_;
}

} // namespace facet::internal
