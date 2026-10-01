# 单测统一入口：所有 *_test.cpp 自动纳入，避免每加一个测试改构建脚本。
file(GLOB_RECURSE PM_TEST_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/doctest_main.cpp)

add_executable(pm_tests ${PM_TEST_SOURCES})

target_include_directories(pm_tests PRIVATE ${PM_ENGINE_DIR})

target_link_libraries(pm_tests PRIVATE
  pm_engine
  doctest::doctest)

if(NOT MSVC)
  target_compile_options(pm_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME pm_tests COMMAND pm_tests)