// Drawing helpers that work without a frame: DrawGroup vertex transforms.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

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
        list->PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(1000.0f, 1000.0f));
    }
    ~ScratchDrawList() { delete list; }
    ScratchDrawList(const ScratchDrawList&) = delete;
    ScratchDrawList& operator=(const ScratchDrawList&) = delete;

    ImDrawListSharedData shared;
    ImDrawList* list = nullptr;
};

} // namespace

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
