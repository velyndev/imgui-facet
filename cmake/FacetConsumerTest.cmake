# The consumer smoke test (FACET_BUILD_CONSUMER_TEST).
#
# Configures and builds examples/consumer as a separate CMake project in
# <build>/consumer, with the generator, platform and toolset of this build. That project owns
# its Dear ImGui target with its own imconfig and adds Facet through add_subdirectory and
# FACET_IMGUI_TARGET, so it sees Facet exactly as another project does: none of this build's
# cache entries, flags or targets reach it. The nested build runs as part of every build of
# this one, and CTest runs its program as the test facet_consumer.
#
# Sources this build already has are passed on, so the nested configure downloads nothing:
# Dear ImGui (when this build fetched it) and lunasvg.

include(ExternalProject)

set(facet_consumer_args "-DFACET_SOURCE_DIR=${PROJECT_SOURCE_DIR}")
if(DEFINED facet_imgui_src_SOURCE_DIR)
    list(APPEND facet_consumer_args "-DFETCHCONTENT_SOURCE_DIR_CONSUMER_IMGUI_SRC=${facet_imgui_src_SOURCE_DIR}")
endif()
if(DEFINED facet_lunasvg_SOURCE_DIR)
    list(APPEND facet_consumer_args "-DFETCHCONTENT_SOURCE_DIR_FACET_LUNASVG=${facet_lunasvg_SOURCE_DIR}")
endif()
get_property(facet_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT facet_multi_config)
    list(APPEND facet_consumer_args "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
endif()

set(facet_consumer_dir "${PROJECT_BINARY_DIR}/consumer")
ExternalProject_Add(facet_consumer_build
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/examples/consumer"
    BINARY_DIR "${facet_consumer_dir}"
    CMAKE_ARGS ${facet_consumer_args}
    INSTALL_COMMAND ""
    # The nested project's own build system decides what is out of date.
    BUILD_ALWAYS TRUE)

if(facet_multi_config)
    set(facet_consumer_exe "${facet_consumer_dir}/bin/$<CONFIG>/facet_consumer.exe")
else()
    set(facet_consumer_exe "${facet_consumer_dir}/bin/facet_consumer.exe")
endif()
add_test(NAME facet_consumer COMMAND "${facet_consumer_exe}")
