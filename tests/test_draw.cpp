// Drawing helpers that work without a frame: continuous corners, gradients, checkerboards,
// arcs, DrawGroup vertex and clip transforms.

#include <cfloat>
#include <cmath>
#include <cstring>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/shape_path.h"

namespace
{

// A draw list outside any frame, with its own shared data set up the way ImGui::NewFrame
// would, and a clip rect so primitives can be added.
struct ScratchDrawList
{
    ScratchDrawList()
    {
        shared.SetCircleTessellationMaxError(0.3f);
        shared.CurveTessellationMaxError = 1.25f;
        shared.InitialDrawFlags = 0;  // no anti-aliasing: exact vertex positions
        list = new ImDrawList(&shared);
        list->_ResetForNewFrame();
        list->_FringeScale = 1.0f;  // one pixel per unit, as in a frame at density 1
        list->_InvFringeScale = 1.0f;
        list->_FringeScaleIsInteger = true;
        list->PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(1000.0f, 1000.0f));
    }
    ~ScratchDrawList() { delete list; }
    ScratchDrawList(const ScratchDrawList&) = delete;
    ScratchDrawList& operator=(const ScratchDrawList&) = delete;

    ImDrawListSharedData shared;
    ImDrawList* list = nullptr;
};

constexpr float kEpsilon = 1e-3f;

bool Inside(ImVec2 p, ImVec2 pMin, ImVec2 pMax, float tolerance = kEpsilon)
{
    return p.x >= pMin.x - tolerance && p.y >= pMin.y - tolerance && p.x <= pMax.x + tolerance &&
           p.y <= pMax.y + tolerance;
}

} // namespace

TEST_CASE("The continuous corner table runs from the edge to the edge without leaving the unit square")
{
    for (int segments = 4; segments <= facet::internal::kMaxCornerSegments; ++segments)
    {
        CAPTURE(segments);
        const ImVec2* table = facet::internal::ContinuousCornerTable(segments);
        CHECK(table[0].x == 1.0f);
        CHECK(table[0].y == 0.0f);
        CHECK(table[segments].x == 0.0f);
        CHECK(table[segments].y == 1.0f);
        for (int k = 0; k <= segments; ++k)
        {
            CHECK(table[k].x >= 0.0f);
            CHECK(table[k].x <= 1.0f);
            CHECK(table[k].y >= 0.0f);
            CHECK(table[k].y <= 1.0f);
            // On the superellipse |x|^3 + |y|^3 = 1.
            CHECK(std::pow(table[k].x, 3.0f) + std::pow(table[k].y, 3.0f) == doctest::Approx(1.0f).epsilon(1e-3));
            if (k > 0)
            {
                CHECK(table[k].x <= table[k - 1].x);
                CHECK(table[k].y >= table[k - 1].y);
            }
        }
        // The middle point sits where a circular arc of the same radius has it.
        if (segments % 2 == 0)
        {
            const ImVec2 middle = table[segments / 2];
            const float gap = facet::internal::kContinuousExtent * (1.0f - middle.x);
            CHECK(gap == doctest::Approx(1.0f - std::sqrt(0.5f)).epsilon(1e-3));
        }
    }
    CHECK(facet::internal::CornerSegments(1.0f) == 4);
    CHECK(facet::internal::CornerSegments(1000.0f) == facet::internal::kMaxCornerSegments);
    CHECK(facet::internal::CornerSegments(30.0f) > facet::internal::CornerSegments(10.0f));
}

TEST_CASE("A continuous rect stays inside its bounds and is symmetric")
{
    const ImVec2 pMin(10.0f, 20.0f);
    const ImVec2 pMax(110.0f, 80.0f);
    for (const float radius : {4.0f, 12.0f, 16.0f, 30.0f, 200.0f})
    {
        CAPTURE(radius);
        ImVec2 points[facet::internal::kMaxOutlinePoints];
        const int count = facet::internal::RoundRectOutline(pMin, pMax, radius, 0, true, points);
        REQUIRE(count >= 4);
        float minX = 1e9f;
        float maxX = -1e9f;
        float minY = 1e9f;
        float maxY = -1e9f;
        for (int i = 0; i < count; ++i)
        {
            CHECK(Inside(points[i], pMin, pMax));
            minX = std::min(minX, points[i].x);
            maxX = std::max(maxX, points[i].x);
            minY = std::min(minY, points[i].y);
            maxY = std::max(maxY, points[i].y);
        }
        // It touches all four edges.
        CHECK(minX == doctest::Approx(pMin.x));
        CHECK(maxX == doctest::Approx(pMax.x));
        CHECK(minY == doctest::Approx(pMin.y));
        CHECK(maxY == doctest::Approx(pMax.y));
        // Point i of the top-left corner mirrors point i of the top-right corner.
        const int perCorner = count / 4;
        for (int i = 0; i < perCorner; ++i)
        {
            const ImVec2 left = points[i];
            const ImVec2 right = points[2 * perCorner - 1 - i];
            CHECK(left.x - pMin.x == doctest::Approx(pMax.x - right.x));
            CHECK(left.y == doctest::Approx(right.y));
        }
        // The corner point itself is cut off.
        for (int i = 0; i < count; ++i)
            CHECK_FALSE((points[i].x == pMin.x && points[i].y == pMin.y));
    }
}

TEST_CASE("Corner flags keep unrounded corners square")
{
    const ImVec2 pMin(0.0f, 0.0f);
    const ImVec2 pMax(100.0f, 50.0f);
    ImVec2 points[facet::internal::kMaxOutlinePoints];
    const int count = facet::internal::RoundRectOutline(pMin, pMax, 20.0f, ImDrawFlags_RoundTopLeft, true, points);
    bool hasTopRight = false;
    bool hasBottomRight = false;
    bool hasBottomLeft = false;
    bool hasTopLeft = false;
    for (int i = 0; i < count; ++i)
    {
        hasTopRight = hasTopRight || (points[i].x == 100.0f && points[i].y == 0.0f);
        hasBottomRight = hasBottomRight || (points[i].x == 100.0f && points[i].y == 50.0f);
        hasBottomLeft = hasBottomLeft || (points[i].x == 0.0f && points[i].y == 50.0f);
        hasTopLeft = hasTopLeft || (points[i].x == 0.0f && points[i].y == 0.0f);
    }
    CHECK(hasTopRight);
    CHECK(hasBottomRight);
    CHECK(hasBottomLeft);
    CHECK_FALSE(hasTopLeft);
    CHECK(facet::internal::RoundRectOutline(pMin, pMax, 20.0f, ImDrawFlags_RoundNone, true, points) == 4);
}

TEST_CASE("Filled and stroked continuous rects draw only inside their bounds")
{
    ScratchDrawList scratch;
    ImDrawList* list = scratch.list;
    const ImVec2 pMin(10.0f, 10.0f);
    const ImVec2 pMax(90.0f, 60.0f);
    facet::FillContinuousRect(list, pMin, pMax, IM_COL32_WHITE, 16.0f);
    const int filled = list->VtxBuffer.Size;
    CHECK(filled > 8);
    facet::StrokeContinuousRect(list, pMin, pMax, IM_COL32_WHITE, 16.0f, 3.0f);
    CHECK(list->VtxBuffer.Size > filled);
    for (int i = 0; i < list->VtxBuffer.Size; ++i)
        CHECK(Inside(list->VtxBuffer[i].pos, pMin, pMax, 0.01f));
}

TEST_CASE("A gradient with rounded corners interpolates the colors across the rect")
{
    ScratchDrawList scratch;
    ImDrawList* list = scratch.list;
    const ImU32 from = IM_COL32(0, 0, 0, 255);
    const ImU32 to = IM_COL32(200, 100, 0, 255);
    facet::FillGradientRect(list, ImVec2(0.0f, 0.0f), ImVec2(100.0f, 40.0f), from, to, facet::GradientDir::Horizontal,
                            12.0f);
    REQUIRE(list->VtxBuffer.Size > 4);
    for (int i = 0; i < list->VtxBuffer.Size; ++i)
    {
        const ImDrawVert& vertex = list->VtxBuffer[i];
        const float t = vertex.pos.x / 100.0f;
        const float red = static_cast<float>((vertex.col >> IM_COL32_R_SHIFT) & 0xFFu);
        CHECK(std::fabs(red - 200.0f * t) <= 1.0f);
        CHECK(((vertex.col >> IM_COL32_A_SHIFT) & 0xFFu) == 255u);
    }
}

TEST_CASE("Checkerboard cells stay inside the rounded outline")
{
    ScratchDrawList scratch;
    ImDrawList* list = scratch.list;
    const ImVec2 pMin(0.0f, 0.0f);
    const ImVec2 pMax(64.0f, 40.0f);
    const float radius = 12.0f;
    facet::FillCheckerboard(list, pMin, pMax, IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 255), 8.0f, radius);
    ImVec2 outline[facet::internal::kMaxOutlinePoints];
    const int count = facet::internal::RoundRectOutline(pMin, pMax, radius, 0,
                                                        facet::internal::UsesContinuousCorners(radius), outline);
    int black = 0;
    for (int i = 0; i < list->VtxBuffer.Size; ++i)
    {
        const ImDrawVert& vertex = list->VtxBuffer[i];
        if ((vertex.col & 0x00FFFFFFu) != 0)
            continue;  // the base fill
        ++black;
        // Inside (or on) every edge of the clockwise outline.
        for (int e = 0; e < count; ++e)
        {
            const ImVec2 a = outline[e];
            const ImVec2 b = outline[(e + 1) % count];
            const float side = (b.x - a.x) * (vertex.pos.y - a.y) - (b.y - a.y) * (vertex.pos.x - a.x);
            CHECK(side >= -0.01f);
        }
    }
    CHECK(black > 0);
}

TEST_CASE("Arcs with round caps stay within the stroke band")
{
    ScratchDrawList scratch;
    ImDrawList* list = scratch.list;
    const ImVec2 center(100.0f, 100.0f);
    const float radius = 40.0f;
    const float thickness = 8.0f;
    facet::StrokeArc(list, center, radius, 0.0f, 3.0f, IM_COL32_WHITE, thickness, true);
    REQUIRE(list->VtxBuffer.Size > 8);
    for (int i = 0; i < list->VtxBuffer.Size; ++i)
    {
        const ImVec2 p = list->VtxBuffer[i].pos;
        const float distance = std::sqrt((p.x - center.x) * (p.x - center.x) + (p.y - center.y) * (p.y - center.y));
        CHECK(distance <= radius + thickness * 0.5f + 0.01f);
        CHECK(distance >= radius - thickness * 0.5f - 0.01f);
    }
}

TEST_CASE("DrawGroup scales, moves and fades only what was drawn inside it")
{
    ScratchDrawList scratch;
    ImDrawList* list = scratch.list;
    list->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(10.0f, 10.0f), IM_COL32(255, 255, 255, 255));
    const int before = list->VtxBuffer.Size;
    const ImVec2 untouched = list->VtxBuffer[0].pos;
    {
        facet::DrawGroup group(list, 0.5f, 2.0f, ImVec2(100.0f, 100.0f), ImVec2(5.0f, 0.0f));
        list->AddRectFilled(ImVec2(100.0f, 100.0f), ImVec2(110.0f, 110.0f), IM_COL32(255, 255, 255, 200));
    }
    REQUIRE(list->VtxBuffer.Size > before);
    CHECK(list->VtxBuffer[0].pos.x == untouched.x);
    CHECK(list->VtxBuffer[0].pos.y == untouched.y);

    float maxX = 0.0f;
    for (int i = before; i < list->VtxBuffer.Size; ++i)
    {
        const ImDrawVert& vertex = list->VtxBuffer[i];
        maxX = vertex.pos.x > maxX ? vertex.pos.x : maxX;
        CHECK(((vertex.col >> IM_COL32_A_SHIFT) & 0xFFu) <= 100u);
    }
    // The right edge at x = 110 scales to 100 + 10 * 2 = 120, plus the 5 px move.
    CHECK(maxX == doctest::Approx(125.0f));
}

TEST_CASE("DrawGroup moves the clip rects of its commands and keeps the others")
{
    ScratchDrawList scratch;
    ImDrawList* list = scratch.list;
    list->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(10.0f, 10.0f), IM_COL32_WHITE);
    const int firstCommand = list->CmdBuffer.Size - 1;
    {
        facet::DrawGroup group(list, 1.0f, 1.0f, ImVec2(0.0f, 0.0f), ImVec2(30.0f, 0.0f));
        list->PushClipRect(ImVec2(100.0f, 100.0f), ImVec2(200.0f, 150.0f));
        list->AddRectFilled(ImVec2(100.0f, 100.0f), ImVec2(150.0f, 150.0f), IM_COL32_WHITE);
        list->PopClipRect();
    }
    list->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(5.0f, 5.0f), IM_COL32_WHITE);

    // The command from before the group is untouched.
    const ImVec4 first = list->CmdBuffer[firstCommand].ClipRect;
    CHECK(first.x == 0.0f);
    CHECK(first.z == 1000.0f);
    // The group's clipped command moved with it.
    bool foundMoved = false;
    for (const ImDrawCmd& command : list->CmdBuffer)
    {
        if (command.ElemCount > 0 && command.ClipRect.x == 130.0f)
        {
            foundMoved = true;
            CHECK(command.ClipRect.z == 230.0f);
            CHECK(command.ClipRect.y == 100.0f);
        }
    }
    CHECK(foundMoved);
    // What is drawn after the group uses the outer clip rect again.
    const ImDrawCmd& last = list->CmdBuffer.back();
    CHECK(last.ClipRect.x == 0.0f);
    CHECK(last.ClipRect.z == 1000.0f);
    CHECK(last.ElemCount > 0);
}
