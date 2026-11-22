#pragma once

// Checks shared by the API tests of the structure families (lists, navigation, containers,
// layout, overlays): sample data, every value a widget receives through a pointer, and a snapshot
// of the Dear ImGui and Facet stacks a call must leave as it found them. Each family's API test
// file uses them for its own widgets:
//
//     facet::test::structure::Values v;
//     CHECK_BALANCED(CHECK_FALSE(facet::Dropdown("Dropdown", &v.index, kItems)));
//
// runs the statement and checks that every stack, the window's work rect and the values are as
// they were and that no usage error was reported. CHECK_BALANCED reads a variable named `v`.

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"

namespace facet::test::structure
{

// Every value a widget receives through a pointer or a writable span. A call without input must
// leave all of it alone.
struct Values
{
    int index = 2;
    std::array<bool, 3> checked = {true, false, true};
    std::array<CheckState, 3> states = {CheckState::On, CheckState::Mixed, CheckState::Off};
    ReorderMove move = {7, 8};
    bool open = true;
    bool closed = false;
    bool pinned = true;
    bool visible = true;
    float ratio = 0.25f;
    std::array<float, 2> sizes = {0.4f, 0.6f};
    int step = 1;
    int page = 3;
    int picked = 4;
    ShortcutId pickedShortcut = {5};
    FramePlacement placement = {
        .position = {10.0f, 20.0f}, .restoredSize = {300.0f, 200.0f}, .scale = 1.5f, .maximized = true};

    bool operator==(const Values& o) const
    {
        const FramePlacement& p = placement;
        const FramePlacement& q = o.placement;
        return index == o.index && checked == o.checked && states == o.states && move.from == o.move.from &&
               move.to == o.move.to && open == o.open && closed == o.closed && pinned == o.pinned &&
               visible == o.visible && ratio == o.ratio && sizes == o.sizes && step == o.step && page == o.page &&
               picked == o.picked && pickedShortcut == o.pickedShortcut && p.position.x == q.position.x &&
               p.position.y == q.position.y && p.restoredSize.x == q.restoredSize.x && p.restoredSize.y == q.restoredSize.y &&
               p.scale == q.scale && p.maximized == q.maximized;
    }
};

// The Dear ImGui and Facet stacks a call must leave as it found them (rule 11 of the
// conventions), a container's narrowing of the window included, plus the values and the usage
// error count.
struct Snapshot
{
    int ids = 0;
    int windows = 0;
    int groups = 0;
    int fonts = 0;
    int styleVars = 0;
    int colors = 0;
    int itemFlagStack = 0;
    int focusScopes = 0;
    int itemWidths = 0;
    int textWrapPos = 0;
    int scopes = 0;
    int errors = 0;
    ImGuiItemFlags itemFlags = 0;
    float indent = 0.0f;
    float workMaxX = 0.0f;
    float contentMaxX = 0.0f;
    Values values;

    static Snapshot Take(const Values& values)
    {
        ImGuiContext& g = *GImGui;
        Snapshot s;
        s.ids = g.CurrentWindow->IDStack.Size;
        s.windows = g.CurrentWindowStack.Size;
        s.groups = g.GroupStack.Size;
        s.fonts = g.FontStack.Size;
        s.styleVars = g.StyleVarStack.Size;
        s.colors = g.ColorStack.Size;
        s.itemFlagStack = g.ItemFlagsStack.Size;
        s.focusScopes = g.FocusScopeStack.Size;
        s.itemWidths = g.CurrentWindow->DC.ItemWidthStack.Size;
        s.textWrapPos = g.CurrentWindow->DC.TextWrapPosStack.Size;
        s.scopes = internal::OpenScopeCount();
        s.errors = internal::UsageErrorCount();
        s.itemFlags = g.CurrentItemFlags;
        s.indent = g.CurrentWindow->DC.Indent.x;
        s.workMaxX = g.CurrentWindow->WorkRect.Max.x;
        s.contentMaxX = g.CurrentWindow->ContentRegionRect.Max.x;
        s.values = values;
        return s;
    }

    bool operator==(const Snapshot&) const = default;
};

inline const ChoiceItem kItems[] = {{"Option 1"}, {"Option 2"}, {"Option 3"}};
inline const char* const kLabels[] = {"Option 1", "Option 2", "Option 3"};
inline const CheckTreeItem kTree[] = {{"Group 1"}, {.label = "Option 1", .depth = 1}, {.label = "Option 2", .depth = 1}};
inline const WatermarkField kFields[] = {{"Field 1", "Value 1"}};

inline bool Getter(void*, int index, ChoiceItem* out)
{
    out->label = kLabels[index % 3];
    return true;
}

// Every vertex the draw list received since `from` lies within `rect`, give or take the
// anti-aliasing fringe.
inline bool VerticesInside(const ImDrawList* drawList, int from, const ImRect& rect)
{
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        const ImVec2 p = drawList->VtxBuffer[i].pos;
        if (p.x < rect.Min.x - 1.0f || p.x > rect.Max.x + 1.0f || p.y < rect.Min.y - 1.0f || p.y > rect.Max.y + 1.0f)
            return false;
    }
    return true;
}

inline int VerticesWithColor(const ImDrawList* drawList, int from, ImU32 color)
{
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        count += drawList->VtxBuffer[i].col == color ? 1 : 0;
    return count;
}

// Rule 7 of the conventions: a data view carries the data block (`state`, `emptyText`,
// `errorText`) in that order, after its own members and before the common block.
template <class T>
consteval bool HasDataBlock()
{
    if constexpr (!requires { &T::state; &T::emptyText; &T::errorText; })
    {
        return false;
    }
    else
    {
        bool ok = std::is_same_v<decltype(T::state), DataState> && std::is_same_v<decltype(T::emptyText), const char*> &&
                  std::is_same_v<decltype(T::errorText), const char*> && offsetof(T, state) < offsetof(T, emptyText) &&
                  offsetof(T, emptyText) < offsetof(T, errorText);
        const std::size_t end = offsetof(T, errorText);
        if constexpr (requires { &T::size; })
            ok = ok && end < offsetof(T, size);
        if constexpr (requires { &T::width; })
            ok = ok && end < offsetof(T, width);
        if constexpr (requires { &T::height; })
            ok = ok && end < offsetof(T, height);
        if constexpr (requires { &T::readOnly; })
            ok = ok && end < offsetof(T, readOnly);
        return ok;
    }
}

// The framed lists that are fields of a form carry the validation members of rule 7.
template <class T>
consteval bool HasFieldMessages()
{
    return requires { &T::error; &T::helpText; };
}

} // namespace facet::test::structure

// Runs a statement and checks that it left every stack, the window rect and the values `v` as
// they were and reported no usage error.
#define CHECK_BALANCED(...)                                                                        \
    do                                                                                             \
    {                                                                                              \
        const ::facet::test::structure::Snapshot before_ = ::facet::test::structure::Snapshot::Take(v); \
        __VA_ARGS__;                                                                               \
        const ::facet::test::structure::Snapshot after_ = ::facet::test::structure::Snapshot::Take(v); \
        CHECK_MESSAGE(after_ == before_, #__VA_ARGS__);                                            \
    } while (false)
