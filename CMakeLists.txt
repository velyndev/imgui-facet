cmake_minimum_required(VERSION 3.24)

# The version is written down once, in include/facet/core/version.h.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/facet/core/version.h" FACET_VERSION_LINES
    REGEX "^#define FACET_VERSION_(MAJOR|MINOR|PATCH) [0-9]+$")
foreach(part MAJOR MINOR PATCH)
    if(NOT FACET_VERSION_LINES MATCHES "FACET_VERSION_${part} ([0-9]+)")
        message(FATAL_ERROR "FACET_VERSION_${part} not found in include/facet/core/version.h")
    endif()
    set(FACET_VERSION_${part} ${CMAKE_MATCH_1})
endforeach()

project(Facet
    VERSION ${FACET_VERSION_MAJOR}.${FACET_VERSION_MINOR}.${FACET_VERSION_PATCH}
    LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(FACET_IS_TOP_LEVEL ON)
else()
    set(FACET_IS_TOP_LEVEL OFF)
endif()

option(FACET_BUILD_DEMO "Build the facet_demo gallery application" ${FACET_IS_TOP_LEVEL})
option(FACET_BUILD_TESTS "Build the unit tests" ${FACET_IS_TOP_LEVEL})
option(FACET_USE_FREETYPE "Rasterize fonts with FreeType (fetched Dear ImGui only)" ON)
option(FACET_WARNINGS_AS_ERRORS "Treat compiler warnings in Facet code as errors" OFF)
set(FACET_IMGUI_TARGET "" CACHE STRING
    "Existing Dear ImGui target to link instead of fetching Dear ImGui")

if(NOT WIN32)
    message(FATAL_ERROR "Facet targets Windows with Direct3D 11 only.")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(FacetWarnings)
include(FacetDeps)
include(FacetEmbed)

# Shared output folder so the demo, the tests and their DLL-free dependencies sit together.
if(FACET_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

# --- Embedded assets ---------------------------------------------------------
# Every SVG under assets/icons is compiled into the library. New files are picked up
# at build time without editing this file.
file(GLOB_RECURSE FACET_ICON_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/icons/*.svg")
facet_embed_assets(
    OUTPUT "${PROJECT_BINARY_DIR}/generated/facet_embedded_assets.cpp"
    BASE_DIR "${PROJECT_SOURCE_DIR}/assets"
    FILES ${FACET_ICON_FILES})

# --- Library -----------------------------------------------------------------
file(GLOB_RECURSE FACET_LIB_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE FACET_LIB_HEADERS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.h")

add_library(facet STATIC
    ${FACET_LIB_SOURCES}
    ${FACET_LIB_HEADERS}
    "${PROJECT_BINARY_DIR}/generated/facet_embedded_assets.cpp")
add_library(facet::facet ALIAS facet)

target_compile_features(facet PUBLIC cxx_std_20)
target_include_directories(facet
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src")
# The Windows macros stay PRIVATE: no public header includes windows.h, and consumers
# must remain free to include the full windows.h (shellapi.h, mmsystem.h, winsock).
target_compile_definitions(facet
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN)
target_link_libraries(facet
    PUBLIC
        ${FACET_IMGUI_LINK_TARGET}
    PRIVATE
        lunasvg::lunasvg
        facet_stb
        d3d11
        dxgi
        d3dcompiler
        dwmapi)
facet_apply_warnings(facet)
source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${FACET_LIB_SOURCES} ${FACET_LIB_HEADERS})

# --- Demo and tests ----------------------------------------------------------
if(FACET_BUILD_DEMO)
    add_subdirectory(demo)
endif()

if(FACET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
