add_executable(test_task task.cpp)
target_link_libraries(test_task GTest::gtest_main zinc)

include(GoogleTest)
gtest_discover_tests(test_task)