// Per-frame work of the charts family (charts.h): every facet::NewFrame records io.DeltaTime in
// the frame time ring that FrameTimeGraph draws, and Shutdown frees the scratch buffers of the
// decimated lines. The entry is in the subsystem table (src/core/subsystems.cpp).

#include <imgui.h>

#include "internal/subsystem.h"
#include "widgets/charts/chart_parts.h"

namespace facet::internal
{
namespace
{

struct ChartFrameRing
{
    float times[kChartFrameHistory] = {};
    int count = 0;  // slots filled, up to kChartFrameHistory
    int next = 0;   // the slot the next sample goes to: the oldest once the ring is full
};

ChartFrameRing& Ring()
{
    static ChartFrameRing ring;
    return ring;
}

bool ChartsInit()
{
    ResetChartFrameTimes();
    return true;
}

void ChartsNewFrame()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    ChartFrameRing& ring = Ring();
    ring.times[ring.next] = ImGui::GetIO().DeltaTime;
    ring.next = (ring.next + 1) % kChartFrameHistory;
    if (ring.count < kChartFrameHistory)
        ++ring.count;
}

void ChartsShutdown()
{
    ResetChartFrameTimes();
    ReleaseChartScratch();
}

} // namespace

SeriesView ChartFrameTimes()
{
    const ChartFrameRing& ring = Ring();
    SeriesView view;
    view.y = ring.times;
    view.count = ring.count;
    // Full: the oldest sample sits at the write slot. Filling: slot 0 is the oldest.
    view.offset = ring.count == kChartFrameHistory ? ring.next : 0;
    return view;
}

void ResetChartFrameTimes()
{
    Ring() = ChartFrameRing{};
}

Subsystem WidgetsChartsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_charts";
    subsystem.init = &ChartsInit;
    subsystem.newFrame = &ChartsNewFrame;
    subsystem.shutdown = &ChartsShutdown;
    return subsystem;
}

} // namespace facet::internal
