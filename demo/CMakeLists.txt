# facet_demo: gallery and capture application.
# Every .cpp and .h under demo/ is part of the program, so new pages need no edit here.
# Pages register themselves through static objects; they must stay in this executable
# (a static library would let the linker drop them).

if(NOT TARGET facet::imgui_backends)
    message(FATAL_ERROR
        "facet_demo needs facet::imgui_backends, which exists only when Facet fetches "
        "Dear ImGui itself. Set FACET_BUILD_DEMO=OFF when using FACET_IMGUI_TARGET.")
endif()

file(GLOB_RECURSE FACET_DEMO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# Console subsystem on purpose, see the comment at the top of main.cpp.
add_executable(facet_demo
    ${FACET_DEMO_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/facet_demo.manifest")

target_include_directories(facet_demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(facet_demo PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00)
target_link_libraries(facet_demo PRIVATE
    facet::facet
    facet::imgui_backends
    facet_stb
    d3d11
    dxgi
    dwmapi)
facet_apply_warnings(facet_demo)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${FACET_DEMO_SOURCES})

set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT facet_demo)
