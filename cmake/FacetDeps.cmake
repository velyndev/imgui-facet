# Third-party dependencies, fetched at configure time and pinned by release tag.
#
# Results:
#   FACET_IMGUI_LINK_TARGET  the Dear ImGui target Facet links (fetched or FACET_IMGUI_TARGET)
#   facet_imgui              static Dear ImGui core (only when fetched)
#   facet::imgui_backends    Win32 and DX11 backends (only when fetched; never part of facet)
#   lunasvg::lunasvg         SVG rasterizer
#   facet_stb                interface target with the vendored stb headers
#   doctest::doctest         unit test framework (only when FACET_BUILD_TESTS)

include(FetchContent)

set(FACET_IMGUI_TAG    "v1.93.0")
set(FACET_FREETYPE_TAG "VER-2-15-0")
set(FACET_LUNASVG_TAG  "v3.5.0")
set(FACET_DOCTEST_TAG  "v2.5.3")

# Dependencies are always static and never install themselves next to Facet.
set(BUILD_SHARED_LIBS OFF)

# --- Dear ImGui --------------------------------------------------------------
if(FACET_IMGUI_TARGET)
    if(NOT TARGET ${FACET_IMGUI_TARGET})
        message(FATAL_ERROR
            "FACET_IMGUI_TARGET is set to '${FACET_IMGUI_TARGET}', but no such target exists. "
            "Create the Dear ImGui target before adding Facet.")
    endif()
    set(FACET_IMGUI_LINK_TARGET ${FACET_IMGUI_TARGET})
    message(STATUS "Facet: using existing Dear ImGui target '${FACET_IMGUI_TARGET}'")
else()
    FetchContent_Declare(facet_imgui_src
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG        ${FACET_IMGUI_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(facet_imgui_src)
    set(imgui_dir "${facet_imgui_src_SOURCE_DIR}")

    if(FACET_USE_FREETYPE)
        set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
        set(SKIP_INSTALL_ALL    ON CACHE BOOL "" FORCE)
        FetchContent_Declare(facet_freetype
            GIT_REPOSITORY https://github.com/freetype/freetype
            GIT_TAG        ${FACET_FREETYPE_TAG}
            GIT_SHALLOW    TRUE
            EXCLUDE_FROM_ALL
            SYSTEM)
        FetchContent_MakeAvailable(facet_freetype)
        facet_silence_warnings(freetype)
    endif()

    add_library(facet_imgui STATIC
        "${imgui_dir}/imgui.cpp"
        "${imgui_dir}/imgui_draw.cpp"
        "${imgui_dir}/imgui_tables.cpp"
        "${imgui_dir}/imgui_widgets.cpp")
    target_include_directories(facet_imgui SYSTEM PUBLIC "${imgui_dir}")
    target_compile_features(facet_imgui PUBLIC cxx_std_20)
    target_compile_definitions(facet_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
    if(FACET_USE_FREETYPE)
        target_sources(facet_imgui PRIVATE "${imgui_dir}/misc/freetype/imgui_freetype.cpp")
        target_include_directories(facet_imgui SYSTEM PUBLIC "${imgui_dir}/misc/freetype")
        target_compile_definitions(facet_imgui PUBLIC IMGUI_ENABLE_FREETYPE)
        target_link_libraries(facet_imgui PRIVATE freetype)
    endif()
    facet_silence_warnings(facet_imgui)
    add_library(facet::imgui ALIAS facet_imgui)

    # Platform and renderer backends for applications. The facet library never compiles these.
    add_library(facet_imgui_backends STATIC
        "${imgui_dir}/backends/imgui_impl_win32.cpp"
        "${imgui_dir}/backends/imgui_impl_dx11.cpp")
    target_include_directories(facet_imgui_backends SYSTEM PUBLIC "${imgui_dir}/backends")
    target_link_libraries(facet_imgui_backends
        PUBLIC facet_imgui
        PRIVATE d3d11 d3dcompiler dxgi dwmapi)
    facet_silence_warnings(facet_imgui_backends)
    add_library(facet::imgui_backends ALIAS facet_imgui_backends)

    set(FACET_IMGUI_LINK_TARGET facet_imgui)
endif()

# --- lunasvg -----------------------------------------------------------------
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(facet_lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg
    GIT_TAG        ${FACET_LUNASVG_TAG}
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(facet_lunasvg)
facet_silence_warnings(lunasvg)
facet_silence_warnings(plutovg)

# --- stb (vendored, see third_party/stb/README.md) ---------------------------
add_library(facet_stb INTERFACE)
target_include_directories(facet_stb SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/third_party/stb")

# --- doctest -----------------------------------------------------------------
if(FACET_BUILD_TESTS)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(facet_doctest
        GIT_REPOSITORY https://github.com/doctest/doctest
        GIT_TAG        ${FACET_DOCTEST_TAG}
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        SYSTEM)
    FetchContent_MakeAvailable(facet_doctest)
endif()
