include(GoogleTest)
include_directories(${CMAKE_SOURCE_DIR}/src)

set(COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/resp.c
    ${CMAKE_SOURCE_DIR}/src/ring_buffer.c
    ${CMAKE_SOURCE_DIR}/src/command_handler.c
    ${CMAKE_SOURCE_DIR}/src/database.c
    ${CMAKE_SOURCE_DIR}/src/client.c
    ${CMAKE_SOURCE_DIR}/src/commands.c
    ${CMAKE_SOURCE_DIR}/src/util.c
    ${CMAKE_SOURCE_DIR}/src/linked_list.c
)

set(TEST_EXECUTABLES
    resp_test
    ring_buffer_test
    dictionary_test
    command_test
    linked_list_test
)

function(add_gtest_executable name)
    add_executable(${name} ${name}.cc ${ARGN})
    target_link_libraries(${name} PRIVATE GTest::gtest_main)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    gtest_discover_tests(${name})
endfunction()

add_gtest_executable(resp_test ${COMMON_SOURCES} ${CMAKE_SOURCE_DIR}/test/mock_handler.c)
add_gtest_executable(ring_buffer_test ${CMAKE_SOURCE_DIR}/src/ring_buffer.c)
add_gtest_executable(dictionary_test ${COMMON_SOURCES})
add_gtest_executable(command_test ${COMMON_SOURCES})
add_gtest_executable(linked_list_test ${CMAKE_SOURCE_DIR}/src/linked_list.c)



