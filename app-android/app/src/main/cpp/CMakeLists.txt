cmake_minimum_required(VERSION 3.22)
project(pm_jni LANGUAGES CXX)

# AGP externalNativeBuild 的 source dir 是 src/main/cpp：
# workspace = ../../../..（cpp→main→src→app→app-android→workspace 共 5 级）。
set(PM_ROOT ${CMAKE_SOURCE_DIR}/../../../../)
set(PM_ENGINE_DIR ${PM_ROOT}/engine)

# 复用顶层依赖引导（find_package 优先 + FetchContent 兜底；PM_WITH_* 由 AGP -D 传入）。
include(${PM_ROOT}/cmake/pm_deps.cmake)

# 引擎静态库：AGP externalNativeBuild 指向本文件，源码收集必须在此完成
# （与 engine/CMakeLists.txt 的 GLOB 纪律一致，D-010）。
file(GLOB_RECURSE PM_ENGINE_SOURCES CONFIGURE_DEPENDS
  ${PM_ENGINE_DIR}/*.cpp)

add_library(pm_engine STATIC ${PM_ENGINE_SOURCES})
target_include_directories(pm_engine PUBLIC ${PM_ENGINE_DIR})
target_link_libraries(pm_engine PUBLIC glm::glm nlohmann_json::nlohmann_json)
target_compile_definitions(pm_engine PUBLIC
  GLM_FORCE_RADIANS
  GLM_FORCE_DEPTH_ZERO_TO_ONE)
# Android NDK 自带标准库；glm/nlohmann 由 pm_deps.cmake 提供（MCP/manifold/render 默认 OFF）。

add_library(pm_engine_jni SHARED jni_bridge.cpp)
target_link_libraries(pm_engine_jni PRIVATE pm_engine log android)
target_include_directories(pm_engine_jni PRIVATE ${PM_ENGINE_DIR})