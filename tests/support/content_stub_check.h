#pragma once

// Checks shared by the API tests of the content families (display, charts, media, composites):
// every call runs inside a real Dear ImGui frame, in a window and again inside
// ImGui::BeginDisabled; afterwards the id, window, group and Facet scope stacks must be as before,
// a widget must have submitted one item of non-zero size (with the id of its label), and no value
// argument may have changed:
//
//     Check("Badge", Expect::Item, "Label", [] { facet::Badge("Label"); });
//
// Each family's API test file uses them for its own widgets.

#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/session.h"

namespace facet::test::content
{

// The stacks a call must leave as it found them.
struct Stacks
{
    int ids = 0;
    int windows = 0;
    int groups = 0;
    int scopes = 0;
    int usageErrors = 0;

    bool operator==(const Stacks&) const = default;
};

inline Stacks CurrentStacks()
{
    const ImGuiContext& g = *GImGui;
    return {g.CurrentWindow->IDStack.Size, g.CurrentWindowStack.Size, g.GroupStack.Size, internal::OpenScopeCount(),
            internal::UsageErrorCount()};
}

// The bytes of an object or array before a call, to prove afterwards that the call wrote
// nothing into it: no member, no element (padding is copied as well, and a widget that writes
// one member leaves it as it was).
template <class T>
class Snapshot
{
public:
    explicit Snapshot(const T& value) : source(&value) { std::memcpy(bytes, &value, sizeof(T)); }
    bool Unchanged() const { return std::memcmp(bytes, source, sizeof(T)) == 0; }

private:
    const T* source;
    unsigned char bytes[sizeof(T)];
};

// How a call is checked besides the stacks.
enum class Expect
{
    Item,     // submits one item of non-zero size; with a label, the item id is that of the label
    NoItem,   // a query or a Begin that returns false: submits nothing that is checked here
};

// Calls `call` and checks the stacks, and for Expect::Item the last item.
template <class Call>
void Check(const char* name, Expect expect, const char* label, Call&& call)
{
    INFO("call: " << std::string(name));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));  // the last item now has a zero size
    const Stacks before = CurrentStacks();
    call();
    CHECK(CurrentStacks() == before);
    if (expect == Expect::Item)
    {
        CHECK(ImGui::GetItemRectSize().x > 0.0f);
        CHECK(ImGui::GetItemRectSize().y > 0.0f);
        if (label != nullptr)
            CHECK(ImGui::GetItemID() == ImGui::GetID(label));
    }
}

// Runs `calls` once in the test window and once inside ImGui::BeginDisabled, in one frame.
template <class Calls>
void RunFrame(Session& session, int* runs, Calls&& calls)
{
    session.SetUi([&] {
        calls();
        ImGui::BeginDisabled();
        calls();
        ImGui::EndDisabled();
        ++*runs;
    });
    session.Frame();
}

// Views of caller data have the data block of rule 7 (state, emptyText, errorText) in that
// order, before the common block.
template <class T>
consteval bool HasDataBlock()
{
    if constexpr (requires {
                      requires std::is_same_v<decltype(T::state), DataState>;
                      requires std::is_same_v<decltype(T::emptyText), const char*>;
                      requires std::is_same_v<decltype(T::errorText), const char*>;
                  })
    {
        bool ordered = offsetof(T, state) < offsetof(T, emptyText) && offsetof(T, emptyText) < offsetof(T, errorText);
        if constexpr (requires { &T::width; })
            ordered = ordered && offsetof(T, errorText) < offsetof(T, width);
        return ordered;
    }
    else
    {
        return false;
    }
}

} // namespace facet::test::content
