add_library(mujoco_hardware_plugin SHARED mujoco_hardware_plugin.cpp)

target_link_libraries(mujoco_hardware_plugin PRIVATE rpf)

target_include_directories(mujoco_hardware_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# 复制元数据文件到输出目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/mujoco_hardware_plugin.meta.json
    ${CMAKE_CURRENT_BINARY_DIR}/mujoco_hardware_plugin.meta.json
    COPYONLY
)
