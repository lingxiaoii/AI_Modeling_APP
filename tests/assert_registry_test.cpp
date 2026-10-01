#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "scene/assert_registry.h"

namespace {

using pm::scene::AssertRegistry;

}  // namespace

TEST_CASE("assert registry registers known types with correct params") {
    AssertRegistry reg;
    std::string error;

    REQUIRE(reg.register_assert(pm::scene::assert_type::kNoIntersection, {}, {}, error));
    REQUIRE(reg.register_assert(pm::scene::assert_type::kNoFloating, {}, {}, error));
    REQUIRE(reg.register_assert(
        pm::scene::assert_type::kBboxWithin, {"table", "max_extent"}, {0.9f, 10.0f}, error));
    REQUIRE(reg.register_assert(
        pm::scene::assert_type::kObjectCount, {"max"}, {20.0f}, error));
    REQUIRE(reg.register_assert(
        pm::scene::assert_type::kTriBudget, {"max_triangles"}, {5000.0f}, error));

    CHECK(reg.entries().size() == 5);
}

TEST_CASE("assert registry rejects unknown type") {
    AssertRegistry reg;
    std::string error;
    CHECK(reg.register_assert("no_such", {}, {}, error) == false);
    CHECK(error.find("unknown_assert_type") != std::string::npos);
}

TEST_CASE("assert registry rejects param count mismatch") {
    AssertRegistry reg;
    std::string error;
    // no_intersection 期望 0 参数。
    CHECK(reg.register_assert(pm::scene::assert_type::kNoIntersection, {"x"}, {1.0f}, error) == false);
    CHECK(error.find("assert_param_count_mismatch") != std::string::npos);
    // bbox_within 期望 2 参数。
    CHECK(reg.register_assert(pm::scene::assert_type::kBboxWithin, {"t"}, {1.0f}, error) == false);
}

TEST_CASE("assert registry overwrites same type") {
    AssertRegistry reg;
    std::string error;
    REQUIRE(reg.register_assert(
        pm::scene::assert_type::kObjectCount, {"max"}, {20.0f}, error));
    REQUIRE(reg.register_assert(
        pm::scene::assert_type::kObjectCount, {"max"}, {50.0f}, error));
    CHECK(reg.entries().size() == 1);
    CHECK(reg.entries()[0].param_values[0] == doctest::Approx(50.0f));
}

TEST_CASE("assert registry clears all") {
    AssertRegistry reg;
    std::string error;
    REQUIRE(reg.register_assert(pm::scene::assert_type::kNoFloating, {}, {}, error));
    reg.clear();
    CHECK(reg.entries().empty());
}