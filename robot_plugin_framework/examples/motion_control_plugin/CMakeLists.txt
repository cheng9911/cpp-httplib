add_library(motion_control_plugin SHARED motion_control_plugin.cpp)

target_link_libraries(motion_control_plugin PRIVATE rpf)

# 复制元数据文件到输出目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/motion_control_plugin.meta.json
    ${CMAKE_CURRENT_BINARY_DIR}/motion_control_plugin.meta.json
    COPYONLY
)
