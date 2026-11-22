# facet_consumer: a small application that uses Facet the way another project does. It owns
# its Dear ImGui target, with its own imconfig, and adds Facet with add_subdirectory and
# FACET_IMGUI_TARGET. Facet's flags and options must not reach this project: the checks below
# fail the configure when they do.
#
# The Facet build configures and builds this directory as a nested project and runs it as the
# test facet_consumer when FACET_BUILD_CONSUMER_TEST is on (cmake/FacetConsumerTest.cmake). It
# also configures on its own, from the root of the Facet tree:
#   cmake -S examples/consumer -B build/consumer -G "Visual Studio 18 2026" -A x64
#   cmake --build build/consumer --config Release
#   ctest --test-dir build/consumer -C Release
cmake_minimum_required(VERSION 3.28)
project(FacetConsumer LANGUAGES C CXX)

set(FACET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "The Facet source tree")

# --- The consumer's own Dear ImGui ---------------------------------------------------------
# FETCHCONTENT_SOURCE_DIR_CONSUMER_IMGUI_SRC points this at an existing copy of the sources.
include(FetchContent)
FetchContent_Declare(consumer_imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        v1.93.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(consumer_imgui_src)
set(imgui_dir "${consumer_imgui_src_SOURCE_DIR}")

add_library(consumer_imgui STATIC
    "${imgui_dir}/imgui.cpp"
    "${imgui_dir}/imgui_draw.cpp"
    "${imgui_dir}/imgui_tables.cpp"
    "${imgui_dir}/imgui_widgets.cpp"
    "${imgui_dir}/backends/imgui_impl_dx11.cpp"
    "${imgui_dir}/backends/imgui_impl_win32.cpp")
target_include_directories(consumer_imgui PUBLIC
    "${imgui_dir}"
    "${imgui_dir}/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/config")
target_compile_definitions(consumer_imgui PUBLIC IMGUI_USER_CONFIG="consumer_imconfig.h")
target_link_libraries(consumer_imgui PUBLIC d3d11 d3dcompiler dxgi dwmapi)

# --- Facet -----------------------------------------------------------------------------------
# A normal variable: Facet's cache entry of the same name does not replace it.
set(FACET_IMGUI_TARGET consumer_imgui)
add_subdirectory("${FACET_SOURCE_DIR}" facet)

if(TARGET facet_demo OR TARGET facet_tests)
    message(FATAL_ERROR "Facet built its demo or its tests inside a consumer project")
endif()
if(TARGET facet_imgui)
    message(FATAL_ERROR "Facet fetched its own Dear ImGui although FACET_IMGUI_TARGET is set")
endif()
foreach(property INTERFACE_COMPILE_OPTIONS INTERFACE_COMPILE_DEFINITIONS INTERFACE_LINK_OPTIONS)
    get_target_property(value facet ${property})
    if(value)
        message(FATAL_ERROR "facet::facet passes ${property} to its consumers: ${value}")
    endif()
endforeach()

# --- The application -------------------------------------------------------------------------
add_executable(facet_consumer main.cpp)
target_link_libraries(facet_consumer PRIVATE facet::facet consumer_imgui)
# The consumer's own warning level, applied to its own code only. Facet's public headers have to
# compile cleanly under it.
if(MSVC)
    target_compile_options(facet_consumer PRIVATE /W4 /WX)
endif()
set_target_properties(facet_consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

enable_testing()
add_test(NAME facet_consumer COMMAND facet_consumer)
