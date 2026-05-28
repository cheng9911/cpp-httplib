add_executable(host_app main.cpp)

target_link_libraries(host_app PRIVATE rpf)

# 复制插件到输出目录
add_custom_command(TARGET host_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/../motion_control_plugin
        $<TARGET_FILE_DIR:host_app>/plugins
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/../vision_plugin
        $<TARGET_FILE_DIR:host_app>/plugins
    COMMENT "Copying plugins to output directory"
)
