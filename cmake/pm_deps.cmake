include_guard(GLOBAL)

include(FetchContent)

# 依赖策略：优先使用 vendored 头（third_party/，CI 零网络），
# 缺失才退回系统包 / FetchContent 抓取。
# 全部依赖均为 header-only 或纯静态，不引入 GPL/LGPL 传染性协议。
# vendored 头不带 CMake target，直接定义 interface target 供 engine 链接。

set(PM_THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ---------- glm (vendored) ----------
if(EXISTS ${PM_THIRD_PARTY}/glm/glm.hpp)
  if(NOT TARGET glm::glm)
    add_library(glm::glm INTERFACE IMPORTED)
    set_target_properties(glm::glm PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${PM_THIRD_PARTY})
  endif()
else()
  find_package(glm QUIET)
  if(NOT glm_FOUND)
    FetchContent_Declare(glm
      GIT_REPOSITORY https://github.com/g-truc/glm.git
      GIT_TAG 1.0.1
      GIT_SHALLOW TRUE)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glm)
  endif()
endif()

# ---------- nlohmann_json (vendored) ----------
if(EXISTS ${PM_THIRD_PARTY}/nlohmann/json.hpp)
  if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${PM_THIRD_PARTY})
  endif()
else()
  find_package(nlohmann_json QUIET)
  if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.3
      GIT_SHALLOW TRUE)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
  endif()
endif()

if(PM_WITH_MANIFOLD)
  find_package(manifold QUIET)
  if(NOT manifold_FOUND)
    FetchContent_Declare(manifold
      GIT_REPOSITORY https://github.com/elalish/manifold.git
      GIT_TAG v3.0.1
      GIT_SHALLOW TRUE)
    set(MANIFOLD_PAR OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(manifold)
  endif()
endif()

if(PM_WITH_MCP)
  if(EXISTS ${PM_THIRD_PARTY}/httplib.h)
    if(NOT TARGET httplib::httplib)
      add_library(httplib::httplib INTERFACE IMPORTED)
      set_target_properties(httplib::httplib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${PM_THIRD_PARTY})
    endif()
  else()
    find_package(httplib QUIET)
    if(NOT httplib_FOUND)
      FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.18.7
        GIT_SHALLOW TRUE)
      FetchContent_MakeAvailable(httplib)
    endif()
  endif()
endif()

if(PM_BUILD_TESTS)
  if(EXISTS ${PM_THIRD_PARTY}/doctest/doctest.h)
    if(NOT TARGET doctest::doctest)
      add_library(doctest::doctest INTERFACE IMPORTED)
      set_target_properties(doctest::doctest PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${PM_THIRD_PARTY}/doctest)
    endif()
  else()
    find_package(doctest QUIET)
    if(NOT doctest_FOUND)
      FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
        GIT_SHALLOW TRUE)
      FetchContent_MakeAvailable(doctest)
    endif()
  endif()
endif()