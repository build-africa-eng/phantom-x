find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(gtest_tests test_main.cpp)
target_link_libraries(gtest_tests GTest::gtest_main)
gtest_discover_tests(gtest_tests)
