# facet_tests: doctest unit tests. Every test_*.cpp under tests/ is compiled in and its
# TEST_CASEs register themselves, so adding a test file needs no edit here.

file(GLOB_RECURSE FACET_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(facet_tests ${FACET_TEST_SOURCES})
target_include_directories(facet_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(facet_tests PRIVATE facet::facet doctest::doctest)
facet_apply_warnings(facet_tests)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${FACET_TEST_SOURCES})

add_test(NAME facet_tests COMMAND facet_tests --no-colors=true --duration=false)
