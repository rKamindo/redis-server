include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(resp_test 
    resp_test.cc 
    ${CMAKE_SOURCE_DIR}/src/resp.c
    ${CMAKE_SOURCE_DIR}/src/ring_buffer.c
    ${CMAKE_SOURCE_DIR}/test/mock_handler.c

)
add_executable(ring_buffer_test ring_buffer_test.cc ${CMAKE_SOURCE_DIR}/src/ring_buffer.c)

add_executable(dictionary_test
dictionary_test.cc 
    ${CMAKE_SOURCE_DIR}/src/resp.c
    ${CMAKE_SOURCE_DIR}/src/ring_buffer.c
    ${CMAKE_SOURCE_DIR}/src/command_handler.c
    ${CMAKE_SOURCE_DIR}/src/database.c
    ${CMAKE_SOURCE_DIR}/src/client.c
    ${CMAKE_SOURCE_DIR}/src/commands.c
)

target_link_libraries(
  resp_test
  GTest::gtest_main
)

target_link_libraries(
  ring_buffer_test
  GTest::gtest_main
)

target_link_libraries(
  dictionary_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(resp_test)
gtest_discover_tests(ring_buffer_test)
gtest_discover_tests(dictionary_test)

target_include_directories(resp_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(ring_buffer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(dictionary_test PRIVATE ${CMAKE_SOURCE_DIR}/src)



