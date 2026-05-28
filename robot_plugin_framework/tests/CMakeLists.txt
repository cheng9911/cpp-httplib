find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# 元数据测试
add_executable(test_metadata test_metadata.cpp)
target_link_libraries(test_metadata PRIVATE rpf GTest::gtest_main)
add_test(NAME test_metadata COMMAND test_metadata)

# 依赖解析测试
add_executable(test_dependency_resolver test_dependency_resolver.cpp)
target_link_libraries(test_dependency_resolver PRIVATE rpf GTest::gtest_main)
add_test(NAME test_dependency_resolver COMMAND test_dependency_resolver)

# 事件总线测试
add_executable(test_event_bus test_event_bus.cpp)
target_link_libraries(test_event_bus PRIVATE rpf GTest::gtest_main)
add_test(NAME test_event_bus COMMAND test_event_bus)

# 插件管理器测试
add_executable(test_plugin_manager test_plugin_manager.cpp)
target_link_libraries(test_plugin_manager PRIVATE rpf GTest::gtest_main)
add_test(NAME test_plugin_manager COMMAND test_plugin_manager)

# 综合测试
add_executable(test_comprehensive test_comprehensive.cpp)
target_link_libraries(test_comprehensive PRIVATE rpf GTest::gtest_main)
add_test(NAME test_comprehensive COMMAND test_comprehensive WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
