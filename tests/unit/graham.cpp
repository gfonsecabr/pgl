#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>
#include <random>

#include "pgl.hpp"

template<class Point>
std::vector<Point> grid(size_t n) {
    std::vector<Point> ret;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            ret.emplace_back(i,j);
        }
    }

    return ret;
}


TEST_CASE_TEMPLATE("Compute convex hull of grid points", Point, pgl::Point<int>, pgl::Point<float>, pgl::Point<pgl::Rational<int>>) {
    auto points = grid<Point>(7);
    auto hull = pgl::grahamScan(points);

    CHECK(hull.size() == 4);
    CHECK(hull[0] == Point(0,0));
    CHECK(hull[1] == Point(6,0));
    CHECK(hull[2] == Point(6,6));
    CHECK(hull[3] == Point(0,6));
}

TEST_CASE_TEMPLATE("Compute convex hull of three points", Point, pgl::Point<int>, pgl::Point<float>, pgl::Point<pgl::Rational<int>>) {
    std::set<Point> points{{1,2},{3,4},{1,4}};
    auto hull = pgl::grahamScan(points);

    CHECK(hull.size() == 3);
    CHECK(hull[0] == Point(1,2));
    CHECK(hull[1] == Point(3,4));
    CHECK(hull[2] == Point(1,4));
}

TEST_CASE_TEMPLATE("Compute convex hull of two points", Point, pgl::Point<int>, pgl::Point<float>, pgl::Point<pgl::Rational<int>>) {
    std::set<Point> points{{1,2},{3,4}};
    auto hull = pgl::grahamScan(points);

    CHECK(hull.size() == 2);
    CHECK(hull[0] == Point(1,2));
    CHECK(hull[1] == Point(3,4));
}

TEST_CASE_TEMPLATE("Compute convex hull of one point", Point, pgl::Point<int>, pgl::Point<float>, pgl::Point<pgl::Rational<int>>) {
    std::set<Point> points{{1,2}};
    auto hull = pgl::grahamScan(points);

    CHECK(hull.size() == 1);
    CHECK(hull[0] == Point(1,2));
}
