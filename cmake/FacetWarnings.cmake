# Compiler settings for Facet's own targets (library, demo, tests).
# Third-party targets never receive these flags.

function(facet_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /Zc:__cplusplus
            /Zc:preprocessor
            /external:W0
            /external:anglebrackets)
        if(FACET_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(FACET_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Silences warnings in a third-party target that is compiled as part of this build.
function(facet_silence_warnings target)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    get_target_property(aliased ${target} ALIASED_TARGET)
    if(aliased)
        set(target ${aliased})
    endif()
    if(MSVC)
        # Drop any /W level the dependency set itself, then disable warnings.
        get_target_property(opts ${target} COMPILE_OPTIONS)
        if(opts)
            list(FILTER opts EXCLUDE REGEX "^[/-]W[0-4all]+$")
            set_target_properties(${target} PROPERTIES COMPILE_OPTIONS "${opts}")
        endif()
        target_compile_options(${target} PRIVATE /W0 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -w)
    endif()
endfunction()
