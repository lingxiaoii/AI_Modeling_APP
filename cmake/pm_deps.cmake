include_guard(GLOBAL)

include(FetchContent)

# 依赖策略：优先吃系统/预置包（Android 与 CI 缓存友好），缺失才抓取源码。
# 全部依赖均为 header-only 或纯静态，不引入 GPL/LGPL 传染性协议。

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

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
  set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(nlohmann_json)
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
  find_package(httplib QUIET)
  if(NOT httplib_FOUND)
    FetchContent_Declare(httplib
      GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
      GIT_TAG v0.18.7
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(httplib)
  endif()
endif()

if(PM_BUILD_TESTS)
  find_package(doctest QUIET)
  if(NOT doctest_FOUND)
    FetchContent_Declare(doctest
      GIT_REPOSITORY https://github.com/doctest/doctest.git
      GIT_TAG v2.4.11
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
  endif()
endif()