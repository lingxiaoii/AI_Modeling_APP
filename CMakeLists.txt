cmake_minimum_required(VERSION 3.22)

# Android NDK 工具链由 Gradle 注入；桌面构建走默认工具链，便于单测与 CI。
project(pocket_modeler VERSION 0.0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# 未定位的模块必须由本开关关掉，否则 M1 阶段就会被 Manifold/bgfx 的缺失拖死。
option(PM_BUILD_TESTS "Build doctest unit tests" ON)
option(PM_WITH_MANIFOLD "Enable Manifold boolean bridge" OFF)
option(PM_WITH_RENDER "Enable bgfx renderer and jpeg encoder" OFF)
option(PM_WITH_MCP "Enable cpp-httplib MCP server" OFF)

set(PM_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PM_ENGINE_DIR ${PM_ROOT}/engine)

list(APPEND CMAKE_MODULE_PATH ${PM_ROOT}/cmake)
include(pm_deps)

add_subdirectory(engine)

if(PM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()