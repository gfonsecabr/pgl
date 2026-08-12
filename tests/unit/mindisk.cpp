#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "pgl.hpp"

namespace {

using IntPoint = pgl::Point<int>;
using IntDisk = pgl::Disk<IntPoint>;

static_assert(std::is_same_v<decltype(pgl::smallestEnclosingDisk(
                                 std::declval<const std::vector<IntPoint>&>())),
                             IntDisk>);
static_assert(std::is_same_v<decltype(pgl::smallestEnclosingDisk(
                                 std::declval<const std::vector<pgl::Point<double>>&>())),
                             pgl::Disk<pgl::Point<double>>>);
static_assert(std::is_same_v<decltype(pgl::smallestEnclosingDisk(
                                 std::declval<const std::vector<pgl::Point<int, std::string>>&>())),
                             IntDisk>);

template <class Point>
auto deterministicSmallestEnclosingDisk(const std::vector<Point>& points) {
    std::mt19937 generator(123456);
    return pgl::smallestEnclosingDisk(points, generator);
}

}  // namespace

TEST_CASE("smallestEnclosingDisk handles a single point and duplicate points") {
    const std::vector<IntPoint> points{{4, -2}, {4, -2}, {4, -2}};
    const auto disk = deterministicSmallestEnclosingDisk(points);

    CHECK(disk.isPoint());
    CHECK(disk.center<int>() == IntPoint(4, -2));
    CHECK(disk.squaredRadius<int>() == 0);
}

TEST_CASE("smallestEnclosingDisk retains the coordinate type for even integral inputs") {
    const std::vector<IntPoint> points{{0, 0}, {2, 0}};
    const auto disk = pgl::smallestEnclosingDisk(points);

    CHECK(disk.center<int>() == IntPoint(1, 0));
    CHECK(disk.squaredRadius<int>() == 1);
    CHECK(disk.contains(points[0]));
    CHECK(disk.contains(points[1]));
}

TEST_CASE("smallestEnclosingDisk uses the extreme pair for collinear points") {
    const std::vector<IntPoint> points{{4, 2}, {-8, 2}, {10, 2}, {0, 2}, {-8, 2}};
    const auto disk = deterministicSmallestEnclosingDisk(points);

    CHECK(disk.center<int>() == IntPoint(1, 2));
    CHECK(disk.squaredRadius<int>() == 81);
    for (const auto& point : points) {
        CHECK(disk.contains(point));
    }
}

TEST_CASE("smallestEnclosingDisk finds a three-point support circle for an acute triangle") {
    const std::vector<IntPoint> points{{0, 0}, {8, 0}, {4, 8}, {4, 2}, {4, 4}};
    const auto disk = deterministicSmallestEnclosingDisk(points);

    CHECK(disk.center<int>() == IntPoint(4, 3));
    CHECK(disk.squaredRadius<int>() == 25);
    CHECK(disk.boundaryContains(points[0]));
    CHECK(disk.boundaryContains(points[1]));
    CHECK(disk.boundaryContains(points[2]));
    CHECK(disk.contains(points[3]));
    CHECK(disk.contains(points[4]));
}

TEST_CASE("smallestEnclosingDisk uses a diameter for an obtuse triangle") {
    const std::vector<IntPoint> points{{0, 0}, {12, 0}, {2, 2}};
    const auto disk = deterministicSmallestEnclosingDisk(points);

    CHECK(disk.center<int>() == IntPoint(6, 0));
    CHECK(disk.squaredRadius<int>() == 36);
    for (const auto& point : points) {
        CHECK(disk.contains(point));
    }
}
