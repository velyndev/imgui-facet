#pragma once

// The parts the curve and easing editors share (CurveEditor, BezierEditor): the canvas they draw in,
// its grid from the chart core's ticks and scales (src/internal/chart_core.h), a polyline buffer that
// draws a curve of any length without allocating, and the look of a point.
//
//     const CurveCanvas canvas = LayoutCurveCanvas(origin, width, height);
//     const Scale x = MakeScale(ScaleKind::Linear, min.x, max.x, canvas.area.Min.x, canvas.area.Max.x);
//     const Scale y = MakeScale(ScaleKind::Linear, min.y, max.y, canvas.area.Max.y, canvas.area.Min.y);
//     DrawCurveCanvas(drawList, canvas, x, y, grid, look);   // frame and grid
//     CurvePolyline line(drawList, color, canvas.frame);
//     for (...) line.Add(point);                               // flushes in chunks
//     line.Finish();
//     DrawCurvePoint(drawList, center, size, item, look);      // the slider knob look
//
// Lengths are pixels.

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/sliders/slider_core.h"

namespace facet::internal
{

// The canvas of an editor: the field frame and the area the point centres travel in (the frame
// inset by half a point and spacing.xs, so a point at an edge stays inside the frame with its ring).
struct CurveCanvas
{
    ImRect frame;
    ImRect area;
    float handle = 0.0f;  // the side of a point: composites.editorHandleSize
};

CurveCanvas LayoutCurveCanvas(ImVec2 origin, float width, float height);

// The frame (DrawFieldFrame with the item's hover, disabled and read-only look) and, with `grid`,
// hairlines at the nice ticks of both scales in the separator color.
void DrawCurveCanvas(ImDrawList* drawList, const CurveCanvas& canvas, const Scale& x, const Scale& y, bool grid,
                     const ItemState& item, bool readOnly);

// The color of a curve: the first series color of the chart palette (3:1 on every surface), or
// labelTertiary while disabled.
ImU32 CurveLineColor(bool disabled);

// A polyline drawn in chunks of a fixed buffer, clipped to `clip`, charts.lineWidth thick.
class CurvePolyline
{
public:
    CurvePolyline(ImDrawList* drawList, ImU32 color, const ImRect& clip);
    void Add(ImVec2 point);
    // Draws what is left; call once at the end.
    void Finish();

private:
    static constexpr int kChunk = 128;
    ImDrawList* drawList_ = nullptr;
    ImU32 color_ = 0;
    ImRect clip_;
    float thickness_ = 1.0f;
    ImVec2 points_[kChunk];
    int count_ = 0;
};

// A point of an editor: the slider knob look (slider_core.h) at `size` pixels around `center`, with its
// round focus ring.
void DrawCurvePoint(ImDrawList* drawList, ImVec2 center, float size, const ItemState& item, bool readOnly);

// The rectangle of a point of `size` pixels around `center`, on whole pixels.
ImRect CurvePointRect(ImVec2 center, float size);

} // namespace facet::internal
