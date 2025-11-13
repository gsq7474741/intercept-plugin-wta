cmake_minimum_required(VERSION 3.26)

project(wta_tests)

add_executable(wta_test_event_bus test_event_bus.cpp)
target_link_libraries(wta_test_event_bus PRIVATE wta_core GTest::gtest GTest::gtest_main)
add_test(NAME EventBusTest COMMAND wta_test_event_bus)

add_executable(wta_test_types test_types.cpp)
target_link_libraries(wta_test_types PRIVATE wta_core GTest::gtest GTest::gtest_main)
add_test(NAME TypesTest COMMAND wta_test_types)
