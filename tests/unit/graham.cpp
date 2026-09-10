#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <limits>
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

// Duplicate input points must not survive as degenerate (zero-length) hull
// edges; a std::vector lets duplicates through where a std::set would not.
TEST_CASE_TEMPLATE("Convex hull discards duplicate input points", Point, pgl::Point<int>, pgl::Point<float>, pgl::Point<pgl::Rational<int>>) {
    SUBCASE("all points coincide -> a single hull vertex") {
        std::vector<Point> points{{4,4},{4,4},{4,4}};
        auto hull = pgl::grahamScan(points);
        CHECK(hull.size() == 1);
        CHECK(hull[0] == Point(4,4));
    }

    SUBCASE("two distinct points, each repeated -> a two-vertex hull") {
        std::vector<Point> points{{1,2},{3,4},{1,2},{3,4}};
        auto hull = pgl::grahamScan(points);
        CHECK(hull.size() == 2);
    }

    SUBCASE("duplicated square corners -> the four corners only") {
        std::vector<Point> points{{0,0},{4,0},{4,4},{0,4},{0,0},{4,4},{4,0}};
        auto hull = pgl::grahamScan(points);
        CHECK(hull.size() == 4);
    }
}

// Over an integral coordinate the hull orders its points by their bits rather
// than by comparing them, once there are enough of them to pay for the passes;
// below that it compares, which every other case here covers. The order the two
// produce must be the same one, so the hulls must agree point for point --
// including the extended hull, which keeps the points interior to an edge and
// so reports the order over far more of the input than the vertices alone.
//
// The reference is the same points over an exact rational, which is compared:
// arbitrary precision, so it stays right at coordinates where a fixed-width
// cross product would not.
//
// The families differ in what the passes see: a range narrow enough that most
// of them have nothing to do, one straddling zero so the order rests on the
// flipped sign bit, and one spanning every byte of the coordinate.
TEST_CASE_TEMPLATE("Convex hull over enough points to sort them by radix", Number,
                   int, long long, short) {
    using PointType = pgl::Point<Number>;
    // Wide enough that all four low bytes vary, and inside the range where the
    // orientation predicate over `Number` is exact, so the two hulls are
    // comparable at all: this is a test of the sort, not of predicate overflow.
    constexpr long long wide =
        std::min<long long>(static_cast<long long>(std::numeric_limits<Number>::max()), 1LL << 30);
    const long long ranges[][2] = {{0, 50}, {-600, 600}, {-wide, wide}};

    for (const auto& range : ranges) {
        CAPTURE(range[0]);
        CAPTURE(range[1]);
        std::mt19937 rng(0xd1ce5u);
        std::uniform_int_distribution<long long> d(range[0], range[1]);

        // Comfortably past the threshold the radix path starts at, and past it
        // again after the Akl-Toussaint filter has dropped what it drops.
        std::vector<PointType> points;
        std::vector<pgl::EPoint> exact;
        for (int i = 0; i < 4000; ++i) {
            const Number x = static_cast<Number>(d(rng));
            const Number y = static_cast<Number>(d(rng));
            points.emplace_back(x, y);
            exact.emplace_back(pgl::ERational(x), pgl::ERational(y));
        }

        for (bool extended : {false, true}) {
            CAPTURE(extended);
            const auto hull = extended ? pgl::grahamScanExtended(points) : pgl::grahamScan(points);
            const auto want = extended ? pgl::grahamScanExtended(exact) : pgl::grahamScan(exact);
            REQUIRE(hull.size() == want.size());
            for (std::size_t i = 0; i < hull.size(); ++i) {
                CHECK(pgl::ERational(hull[i].x()) == want[i].x());
                CHECK(pgl::ERational(hull[i].y()) == want[i].y());
            }
        }
    }
}

// sortPoints is the lexicographic order std::sort gives, whichever path it
// takes to it; sortDistinctPoints is that order with the repeats gone. Sizes
// straddle the point where the radix passes take over from the comparisons.
TEST_CASE_TEMPLATE("sortPoints matches the comparison order", Number, int, long long, short) {
    using PointType = pgl::Point<Number>;
    std::mt19937 rng(4242u);

    for (int n : {0, 1, 2, 17, 255, 256, 257, 4000}) {
        CAPTURE(n);
        // A range narrower than n, so coincident points are common and the
        // duplicate-dropping half of the pair is actually exercised.
        for (int spread : {8, 1000}) {
            CAPTURE(spread);
            std::uniform_int_distribution<int> d(-spread, spread);
            std::vector<PointType> points;
            for (int i = 0; i < n; ++i) {
                points.emplace_back(static_cast<Number>(d(rng)), static_cast<Number>(d(rng)));
            }

            std::vector<PointType> want = points;
            std::sort(want.begin(), want.end());

            std::vector<PointType> got = points;
            pgl::sortPoints(got);
            CHECK(got == want);

            want.erase(std::unique(want.begin(), want.end()), want.end());
            std::vector<PointType> distinct = points;
            pgl::sortDistinctPoints(distinct);
            CHECK(distinct == want);
        }
    }
}
