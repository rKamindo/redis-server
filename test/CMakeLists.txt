
add_executable(resp_test resp_test.cc ${CMAKE_SOURCE_DIR}/src/resp.c)

target_link_libraries(
  resp_test
  GTest::gtest_main
)


include(GoogleTest)
gtest_discover_tests(resp_test)

target_include_directories(resp_test PRIVATE ${CMAKE_SOURCE_DIR}/src)