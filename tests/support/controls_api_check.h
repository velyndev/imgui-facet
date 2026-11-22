#pragma once

// Checks shared by the API tests of the controls families (buttons, selection, sliders, inputs,
// pickers): sample data, every value a widget could write, the Dear ImGui stacks a call must leave
// as it found them, and the macros that call one widget without input and check it. Each family's
// API test file (tests/test_api_<family>.cpp) uses them for its own widgets:
//
//     FACET_CHECK_CALL(false, facet::Slider("Slider 1", &v.number, 0.0f, 1.0f));
//
// checks that the call returned the neutral result (false), left every stack as it was, and
// submitted one new item with a size. Describe(values) before and after a frame shows that
// nothing was written.

#include <array>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"

namespace facet::test::controls
{

// Sample data. Labels name nothing real: "Option n" and built-in icon names.
inline const char* const kLabels[] = {"Option 1", "Option 2", "Option 3"};
inline const ChoiceItem kChoices[] = {
    {"Option 1", Icon("grid")},
    {"Option 2", Icon("list")},
    {"Option 3", Icon("layers")},
};
inline const OptionCard kCards[] = {{"Option 1", "Placeholder description"}, {"Option 2"}};
inline const UnitItem kUnits[] = {{"ms", 0.001}, {"s", 1.0}};
inline const float kStops[] = {0.0f, 0.25f, 1.0f};
inline const float kBins[] = {1.0f, 3.0f, 2.0f};
inline const ImVec4 kSwatches[] = {{0.2f, 0.4f, 0.8f, 1.0f}, {0.8f, 0.3f, 0.2f, 1.0f}};
inline const ThumbnailItem kThumbnails[] = {{"Option 1"}, {"Option 2"}};
inline const FileType kFileTypes[] = {{"Option 1", "*.png;*.jpg"}, {"Option 2"}};

// Every value a widget could write, set to values a call without input never produces.
struct Values
{
    bool flag = true;
    CheckState check = CheckState::Mixed;
    int index = 1;
    std::array<bool, 3> flags = {true, false, true};
    float number = 0.25f;
    int integer = 7;
    double real = 1.5;
    float low = 0.2f;
    float high = 0.8f;
    int lowInt = 2;
    int highInt = 8;
    std::array<float, 3> handles = {0.1f, 0.5f, 0.9f};
    std::array<int, 2> handlesInt = {3, 6};
    std::vector<float> handleVector = {0.2f, 0.7f};
    std::array<float, 4> handleStorage = {0.2f, 0.6f, 0.0f, 0.0f};
    int handleCount = 2;
    ImVec2 point = {0.3f, 0.4f};
    ImVec2 boxLow = {0.1f, 0.2f};
    ImVec2 boxHigh = {0.6f, 0.7f};
    std::array<float, 3> shares = {0.5f, 0.3f, 0.2f};
    std::string text = "Sample";
    std::array<char, 32> buffer = {'S', 'a', 'm', 'p', 'l', 'e'};
    FilterSettings filter = {true, false};
    std::vector<std::string> tags = {"Option 1"};
    bool editing = false;
    int unit = 1;
    std::array<float, 3> vector = {1.0f, 2.0f, 3.0f};
    std::array<int, 2> vectorInt = {4, 5};
    ImVec2 size = {640.0f, 480.0f};
    int sizeX = 640;
    int sizeY = 480;
    std::uint32_t hex32 = 0xA1B2C3D4u;
    std::uint64_t hex64 = 0x0123456789ABCDEFull;
    double seconds = 90.0;
    Keybind bind = {ImGuiKey_F5, ImGuiMod_Ctrl | ImGuiKey_K, BindMode::Toggle};
    BindMode mode = BindMode::Always;
    float rating = 3.5f;
    int ratingInt = 2;
    ImVec4 color = {0.1f, 0.2f, 0.3f, 0.4f};
    ColorHistory history = {{{0.5f, 0.5f, 0.5f, 1.0f}}, 1};
    std::vector<ImVec4> swatches = {{0.9f, 0.8f, 0.7f, 1.0f}};
    std::array<ImVec4, 3> swatchStorage = {{{0.9f, 0.8f, 0.7f, 1.0f}}};
    int swatchCount = 1;
    AnimatedColor animated = {ColorAnimation::Pulse, {0.1f, 0.2f, 0.3f, 1.0f}, {0.4f, 0.5f, 0.6f, 1.0f}, 3.0f};
    Date date = {2001, 2, 3};
    Date start = {2001, 2, 3};
    Date end = {2001, 3, 4};
    TimeOfDay time = {10, 30, 15};
    float radians = 1.0f;
    Icon icon = Icon("star");
    std::array<bool, 2> thumbnailFlags = {false, true};
};

inline void Append(std::string& out, const char* name, double value)
{
    char line[96];
    std::snprintf(line, sizeof(line), "%s=%.9g;", name, value);
    out += line;
}

inline void Append(std::string& out, const char* name, ImVec4 value)
{
    Append(out, name, value.x);
    Append(out, name, value.y);
    Append(out, name, value.z);
    Append(out, name, value.w);
}

inline void Append(std::string& out, const char* name, ImVec2 value)
{
    Append(out, name, value.x);
    Append(out, name, value.y);
}

// Every member as text, so a changed value shows up in the failure message.
inline std::string Describe(const Values& v)
{
    std::string out;
    Append(out, "flag", v.flag);
    Append(out, "check", static_cast<int>(v.check));
    Append(out, "index", v.index);
    for (bool f : v.flags)
        Append(out, "flags", f);
    Append(out, "number", v.number);
    Append(out, "integer", v.integer);
    Append(out, "real", v.real);
    Append(out, "low", v.low);
    Append(out, "high", v.high);
    Append(out, "lowInt", v.lowInt);
    Append(out, "highInt", v.highInt);
    for (float h : v.handles)
        Append(out, "handles", h);
    for (int h : v.handlesInt)
        Append(out, "handlesInt", h);
    Append(out, "handleVector.size", static_cast<double>(v.handleVector.size()));
    for (float h : v.handleStorage)
        Append(out, "handleStorage", h);
    Append(out, "handleCount", v.handleCount);
    Append(out, "swatchCount", v.swatchCount);
    for (const ImVec4& s : v.swatchStorage)
        Append(out, "swatchStorage", s);
    for (float h : v.handleVector)
        Append(out, "handleVector", h);
    Append(out, "point", v.point);
    Append(out, "boxLow", v.boxLow);
    Append(out, "boxHigh", v.boxHigh);
    for (float s : v.shares)
        Append(out, "shares", s);
    out += "text=" + v.text + ";buffer=" + std::string(v.buffer.data()) + ";";
    Append(out, "filter.matchCase", v.filter.matchCase);
    Append(out, "filter.wholeWord", v.filter.wholeWord);
    for (const std::string& tag : v.tags)
        out += "tag=" + tag + ";";
    Append(out, "editing", v.editing);
    Append(out, "unit", v.unit);
    for (float c : v.vector)
        Append(out, "vector", c);
    for (int c : v.vectorInt)
        Append(out, "vectorInt", c);
    Append(out, "size", v.size);
    Append(out, "sizeX", v.sizeX);
    Append(out, "sizeY", v.sizeY);
    Append(out, "hex32", v.hex32);
    Append(out, "hex64.high", static_cast<double>(v.hex64 >> 32));
    Append(out, "hex64.low", static_cast<double>(v.hex64 & 0xFFFFFFFFull));
    Append(out, "seconds", v.seconds);
    Append(out, "bind.primary", v.bind.primary);
    Append(out, "bind.alternate", v.bind.alternate);
    Append(out, "bind.mode", static_cast<int>(v.bind.mode));
    Append(out, "mode", static_cast<int>(v.mode));
    Append(out, "rating", v.rating);
    Append(out, "ratingInt", v.ratingInt);
    Append(out, "color", v.color);
    Append(out, "history.count", v.history.count);
    for (const ImVec4& c : v.history.colors)
        Append(out, "history", c);
    for (const ImVec4& c : v.swatches)
        Append(out, "swatches", c);
    Append(out, "animated.animation", static_cast<int>(v.animated.animation));
    Append(out, "animated.color", v.animated.color);
    Append(out, "animated.second", v.animated.second);
    Append(out, "animated.period", v.animated.period);
    Append(out, "date", v.date.year * 10000 + v.date.month * 100 + v.date.day);
    Append(out, "start", v.start.year * 10000 + v.start.month * 100 + v.start.day);
    Append(out, "end", v.end.year * 10000 + v.end.month * 100 + v.end.day);
    Append(out, "time", v.time.hour * 10000 + v.time.minute * 100 + v.time.second);
    Append(out, "radians", v.radians);
    Append(out, "icon", v.icon.id);
    for (bool f : v.thumbnailFlags)
        Append(out, "thumbnailFlags", f);
    return out;
}

// The Dear ImGui stacks a widget call must leave as it found them.
struct Stacks
{
    int ids = 0;
    int windows = 0;
    int groups = 0;
    int itemWidths = 0;
    int itemFlags = 0;
    int colors = 0;
    int styleVars = 0;
    int fonts = 0;
    int disabled = 0;

    friend bool operator==(const Stacks&, const Stacks&) = default;
};

inline Stacks Snapshot()
{
    ImGuiContext& g = *GImGui;
    return {g.CurrentWindow->IDStack.Size, g.CurrentWindowStack.Size, g.GroupStack.Size,
            g.CurrentWindow->DC.ItemWidthStack.Size, g.ItemFlagsStack.Size, g.ColorStack.Size,
            g.StyleVarStack.Size, g.FontStack.Size, g.DisabledStackSize};
}

// Counts the calls made, so a test can check that its list reaches every function. Each test
// case resets it before its calls.
inline int g_calls = 0;

template <class Result>
void CheckCall(const char* call, const Stacks& before, ImGuiID itemBefore, Result result, Result neutral)
{
    ++g_calls;
    INFO(call);
    CHECK(result == neutral);
    CHECK(Snapshot() == before);
    // One new item, with a size, was submitted.
    ImGuiContext& g = *GImGui;
    CHECK(g.LastItemData.ID != itemBefore);
    CHECK(g.LastItemData.Rect.GetWidth() >= 1.0f);
    CHECK(g.LastItemData.Rect.GetHeight() >= 1.0f);
}

// A void call that only lays out under the last item: it leaves the stacks alone, moves the
// cursor down and keeps the last item as it was.
inline void CheckLayoutCall(const char* call, const Stacks& before, ImGuiID itemBefore, float cursorBefore)
{
    ++g_calls;
    INFO(call);
    CHECK(Snapshot() == before);
    CHECK(GImGui->LastItemData.ID == itemBefore);
    CHECK(ImGui::GetCursorScreenPos().y > cursorBefore);
}

// Every options struct with a printf format also takes a formatter, with the same names in the
// same order (designated initializers must follow the declaration order).
template <class Options>
constexpr bool HasFormatMembers()
{
    return requires {
        Options{.format = nullptr, .formatter = nullptr, .formatterUser = nullptr};
        requires std::same_as<decltype(Options::formatter), FormatFn>;
        requires std::same_as<decltype(Options::formatterUser), void*>;
    };
}

} // namespace facet::test::controls

#define FACET_CHECK_CALL(neutral, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        const ::facet::test::controls::Stacks facetBefore = ::facet::test::controls::Snapshot();   \
        const ImGuiID facetItemBefore = GImGui->LastItemData.ID;                                   \
        const auto facetResult = (__VA_ARGS__);                                                    \
        ::facet::test::controls::CheckCall(#__VA_ARGS__, facetBefore, facetItemBefore, facetResult, neutral); \
    } while (false)

#define FACET_CHECK_LAYOUT_CALL(...)                                                               \
    do                                                                                             \
    {                                                                                              \
        const ::facet::test::controls::Stacks facetBefore = ::facet::test::controls::Snapshot();   \
        const ImGuiID facetItemBefore = GImGui->LastItemData.ID;                                   \
        const float facetCursorBefore = ImGui::GetCursorScreenPos().y;                             \
        __VA_ARGS__;                                                                               \
        ::facet::test::controls::CheckLayoutCall(#__VA_ARGS__, facetBefore, facetItemBefore, facetCursorBefore); \
    } while (false)

// Runs `calls(values)` in one frame, again inside ImGui::BeginDisabled with every id pushed under
// a scope of its own, checks that `expected` checks ran each time and that nothing was written.
#define FACET_CHECK_FAMILY_CALLS(calls, expected)                                                  \
    do                                                                                             \
    {                                                                                              \
        ::facet::test::Session facetSession;                                                       \
        REQUIRE(facetSession.Ready());                                                             \
        ::facet::test::controls::Values facetValues;                                               \
        const std::string facetPristine = ::facet::test::controls::Describe(facetValues);          \
        int facetFrames = 0;                                                                       \
        facetSession.SetUi([&] {                                                                   \
            ::facet::test::controls::g_calls = 0;                                                  \
            calls(facetValues);                                                                    \
            CHECK(::facet::test::controls::g_calls == (expected));                                 \
            ImGui::BeginDisabled();                                                                \
            ImGui::PushID("disabled");                                                             \
            calls(facetValues);                                                                    \
            ImGui::PopID();                                                                        \
            ImGui::EndDisabled();                                                                  \
            ++facetFrames;                                                                         \
        });                                                                                        \
        facetSession.Frames(2);                                                                    \
        CHECK(facetFrames == 2);                                                                   \
        CHECK(::facet::test::controls::Describe(facetValues) == facetPristine);                    \
        CHECK_FALSE(ImGui::IsAnyItemActive());                                                     \
    } while (false)
