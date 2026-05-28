add_executable(debug_server main.cpp debug_api.cpp)

target_link_libraries(debug_server PRIVATE rpf pthread)

target_include_directories(debug_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # cpp-httplib include path (root directory)
)

# 复制插件到输出目录
add_custom_command(TARGET debug_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/../mujoco_hardware_plugin
        $<TARGET_FILE_DIR:debug_server>/plugins
    COMMENT "Copying plugins to output directory"
)
