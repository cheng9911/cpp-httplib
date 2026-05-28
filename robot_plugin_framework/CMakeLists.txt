cmake_minimum_required(VERSION 3.14)
project(RobotPluginFramework VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(RPF_BUILD_EXAMPLES "Build example plugins and host app" ON)
option(RPF_BUILD_TESTS "Build tests" ON)
option(RPF_SHARED "Build as shared library" ON)

# Fetch third-party dependencies
add_subdirectory(third_party)

# Main library
set(RPF_SOURCES
    src/plugin_manager.cpp
    src/service_registry.cpp
    src/event_bus.cpp
    src/dependency_resolver.cpp
)

if(RPF_SHARED)
    add_library(rpf SHARED ${RPF_SOURCES})
    target_compile_definitions(rpf PRIVATE RPF_EXPORTS)
else()
    add_library(rpf STATIC ${RPF_SOURCES})
endif()

target_include_directories(rpf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(rpf
    PUBLIC
        dylib
        nlohmann_json::nlohmann_json
)

# Examples
if(RPF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(RPF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install (disabled for development)
# install(TARGETS rpf
#     EXPORT RobotPluginFrameworkTargets
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     RUNTIME DESTINATION bin
# )
#
# install(DIRECTORY include/rpf DESTINATION include)
#
# install(EXPORT RobotPluginFrameworkTargets
#     FILE RobotPluginFrameworkTargets.cmake
#     NAMESPACE RPF::
#     DESTINATION lib/cmake/RobotPluginFramework
# )
