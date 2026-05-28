add_library(vision_plugin SHARED vision_plugin.cpp)

target_link_libraries(vision_plugin PRIVATE rpf)

# 复制元数据文件到输出目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/vision_plugin.meta.json
    ${CMAKE_CURRENT_BINARY_DIR}/vision_plugin.meta.json
    COPYONLY
)
