# 纯 C++17 静态库：不链接 Android、不链接 JNI、不链接 SSL。
# 平台能力全部经 platform/ 接口，由宿主注入实现。

file(GLOB_RECURSE PM_ENGINE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(pm_engine STATIC ${PM_ENGINE_SOURCES})

# 加 -I<engine> 让内部 include 统一写成 "core/math_types.h" 这类模块根相对路径。
target_include_directories(pm_engine PUBLIC
  $<BUILD_INTERFACE:${PM_ENGINE_DIR}>)

# GLM_FORCE_* 双保险：math_types.h 内带 #ifndef guard 定义，
# 编译选项再统一兜底，避免未 include math_types.h 的翻译单元深度/Radian 分叉。
target_compile_definitions(pm_engine PUBLIC
  GLM_FORCE_RADIANS
  GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(pm_engine PUBLIC
  glm::glm
  nlohmann_json::nlohmann_json
  tinygltf::tinygltf
  meshoptimizer::meshoptimizer)

if(PM_WITH_MANIFOLD)
  target_compile_definitions(pm_engine PUBLIC PM_WITH_MANIFOLD=1)
  target_link_libraries(pm_engine PUBLIC manifold)
endif()

if(PM_WITH_MCP)
  target_compile_definitions(pm_engine PUBLIC PM_WITH_MCP=1)
  target_link_libraries(pm_engine PRIVATE httplib::httplib)
endif()

if(MSVC)
  target_compile_options(pm_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(pm_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()