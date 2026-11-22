// Subsystem table: order of the real table and the start and stop drivers.

#include <cstring>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "internal/subsystem.h"

namespace
{

std::vector<std::string> calls;
int failingInit = -1;     // index of the module whose init fails, -1 for none
int failingCreate = -1;   // index of the module whose createDeviceObjects fails

template <int N>
bool Init()
{
    calls.push_back("init" + std::to_string(N));
    return N != failingInit;
}

template <int N>
void Shutdown()
{
    calls.push_back("shutdown" + std::to_string(N));
}

template <int N>
bool Create()
{
    calls.push_back("create" + std::to_string(N));
    return N != failingCreate;
}

template <int N>
void Invalidate()
{
    calls.push_back("invalidate" + std::to_string(N));
}

template <int N>
facet::internal::Subsystem Recording()
{
    facet::internal::Subsystem subsystem;
    subsystem.name = "recording";
    subsystem.init = &Init<N>;
    subsystem.shutdown = &Shutdown<N>;
    subsystem.createDeviceObjects = &Create<N>;
    subsystem.invalidateDeviceObjects = &Invalidate<N>;
    return subsystem;
}

void Reset()
{
    calls.clear();
    failingInit = -1;
    failingCreate = -1;
}

using Calls = std::vector<std::string>;

} // namespace

TEST_CASE("The subsystem table lists every module once in dependency order")
{
    const auto table = facet::internal::Subsystems();
    const char* expected[] = {"theme",          "scale",
                              "state_storage",  "anim",
                              "font",           "texture",
                              "icon",           "shadow",
                              "blur",           "input",
                              "focus",          "popup_host",
                              "scroll",         "drag_drop",
                              "text_field",     "widgets_common",
                              "widgets_buttons", "widgets_selection",
                              "widgets_sliders", "widgets_text_inputs",
                              "widgets_value_inputs", "widgets_lists",
                              "widgets_containers", "widgets_display",
                              "widgets_color_pickers", "widgets_date_pickers",
                              "widgets_navigation", "widgets_layout",
                              "widgets_overlays", "widgets_structure",
                              "widgets_data_table", "widgets_charts",
                              "widgets_media", "widgets_cards",
                              "widgets_settings", "widgets_editors", "scopes"};
    REQUIRE(table.size() == std::size(expected));
    for (size_t i = 0; i < table.size(); ++i)
    {
        CAPTURE(i);
        REQUIRE(table[i].name != nullptr);
        CHECK(std::strcmp(table[i].name, expected[i]) == 0);
    }
}

TEST_CASE("Start runs init in order, then device creation; stop runs both in reverse")
{
    Reset();
    const facet::internal::Subsystem table[] = {Recording<0>(), Recording<1>(), Recording<2>()};

    CHECK(facet::internal::StartSubsystems(table));
    CHECK(calls == Calls{"init0", "init1", "init2", "create0", "create1", "create2"});

    calls.clear();
    facet::internal::StopSubsystems(table);
    CHECK(calls == Calls{"invalidate2", "invalidate1", "invalidate0", "shutdown2", "shutdown1",
                         "shutdown0"});
}

TEST_CASE("A failing init shuts down only the modules started before it")
{
    Reset();
    failingInit = 2;
    const facet::internal::Subsystem table[] = {Recording<0>(), Recording<1>(), Recording<2>(),
                                                Recording<3>()};

    CHECK_FALSE(facet::internal::StartSubsystems(table));
    CHECK(calls == Calls{"init0", "init1", "init2", "shutdown1", "shutdown0"});
}

TEST_CASE("A failing device creation invalidates and shuts down every module")
{
    Reset();
    failingCreate = 1;
    const facet::internal::Subsystem table[] = {Recording<0>(), Recording<1>(), Recording<2>()};

    CHECK_FALSE(facet::internal::StartSubsystems(table));
    CHECK(calls == Calls{"init0", "init1", "init2", "create0", "create1", "invalidate2",
                         "invalidate1", "invalidate0", "shutdown2", "shutdown1", "shutdown0"});
}

TEST_CASE("Null hooks are skipped and count as success")
{
    Reset();
    facet::internal::Subsystem empty;
    empty.name = "empty";
    const facet::internal::Subsystem table[] = {empty, Recording<1>(), empty};

    CHECK(facet::internal::StartSubsystems(table));
    facet::internal::StopSubsystems(table);
    CHECK(calls == Calls{"init1", "create1", "invalidate1", "shutdown1"});
}
