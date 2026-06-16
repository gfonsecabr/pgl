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
std::vector<pgl::Segment<Point>> randomSegments(size_t n1, size_t n2, size_t seed = 1) {
    std::mt19937 rgen(seed); // Seed
    std::set<pgl::Segment<Point>> ret_set;
    static std::uniform_int_distribution<int> base(-15,15);
    static std::uniform_int_distribution<int> len(0,20);

    for (size_t i = 0; i < n1; i++) {
        Point p(base(rgen),base(rgen));
        Point q(p.x()+ len(rgen), p.y() + len(rgen));
        if (p != q)
            ret_set.emplace(p,q);
    }

    static std::uniform_int_distribution<int> base2(-15,15);
    static std::uniform_int_distribution<int> len2(-6,6);
    static std::uniform_int_distribution<int> k2(0,2);
    Point center(base2(rgen),base2(rgen));
    for (size_t i = 0; i < n2; i++) {
        Point v(len2(rgen), len2(rgen));
        Point p1 = center + v;
        Point p2 = center - v*k2(rgen);
        if (p1 != p2)
            ret_set.emplace(p1,p2);
    }

    return std::vector<pgl::Segment<Point>>(ret_set.begin(),ret_set.end());
}

// FIXME: Breaks for points with labels and rational coordinates?
TEST_CASE_TEMPLATE("Find crossings among segments", Point, pgl::Point<int>, pgl::Point<float>) {
    auto segs = randomSegments<Point>(15,10);
    auto bf = pgl::bruteForceCrossings(segs);
    auto bo = pgl::findCrossings(segs);

    CHECK(bf.size() == bo.size());
    std::sort(bf.begin(),bf.end());
    std::sort(bo.begin(),bo.end());
    CHECK(bf == bo);
}

TEST_CASE_TEMPLATE("Find intersections among segments", Point, pgl::Point<int>, pgl::Point<float>) {
    auto segs = randomSegments<Point>(15,10);
    auto bf = pgl::bruteForceIntersections(segs);
    auto bo = pgl::findIntersections(segs);

    CHECK(bf.size() == bo.size());
    std::sort(bf.begin(),bf.end());
    std::sort(bo.begin(),bo.end());
    CHECK(bf == bo);
}

// Regression: a convex polygon is always simple, but isSimple()'s sweep path
// (n > 8) used to report a spurious self-intersection when the polygon has a
// unique leftmost vertex whose two incident edges share that left endpoint.
TEST_CASE("Convex polygon with a unique leftmost vertex is simple") {
    using Point = pgl::Point<int>;
    std::vector<Point> vertices = {
        {0,4},{4,0},{6,0},{8,1},{9,4},{8,7},{6,8},{4,8},{1,6},
    };
    pgl::Polygon<Point> poly(vertices);
    CHECK(poly.isSimple());
}

TEST_CASE_TEMPLATE("Detect crossings and intersections among segments", Point, pgl::Point<int>, pgl::Point<float>) {
    std::vector<pgl::Segment<Point>> segs;
    for(int i = 1; i < 5; i++) {
        segs.emplace_back(i,i*i,i+1,(i+1)*(i+1));
    }

    bool cross = pgl::detectCrossings(segs);
    CHECK_FALSE(cross);
    bool isec = pgl::detectIntersections(segs);
    CHECK(isec);
    segs.emplace_back(0,0,11,20);
    bool cross2 = pgl::detectCrossings(segs);
    CHECK(cross2);
}
