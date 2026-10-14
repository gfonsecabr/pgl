#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

struct Rng {
    std::uint64_t state;

    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }

    int range(int low, int high) {
        return low + static_cast<int>(next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

template <class Point>
Point at(int x, int y) {
    using Number = typename Point::NumberType;
    return Point(Number(x), Number(y));
}

// The dot product, in a type wide enough to hold it exactly.
template <class Point>
auto dot(const Point& d, const Point& p) {
    using Wide = pgl::detail::promoted_number_t<typename Point::NumberType>;
    return Wide(d.x()) * Wide(p.x()) + Wide(d.y()) * Wide(p.y());
}

// Checks every query of `hull` against a Convex built from scratch over `points`.
template <class Point>
void checkAgainstRebuild(const pgl::DynamicConvexHull<Point>& hull, const std::set<Point>& points,
                         Rng& rng, int lo, int hi) {
    REQUIRE(hull.size() == points.size());
    CHECK(hull.empty() == points.empty());
    CHECK(std::vector<Point>(hull.begin(), hull.end()) == std::vector<Point>(points.begin(), points.end()));

    std::vector<Point> queries;
    for (int k = 0; k < 8; ++k) {
        queries.push_back(at<Point>(rng.range(lo - 1, hi + 1), rng.range(lo - 1, hi + 1)));
    }

    if (points.empty()) {
        CHECK(hull.hullSize() == 0);
        CHECK(hull.vertices().empty());
        for (const Point& q : queries) {
            CHECK_FALSE(hull.contains(q));
            CHECK_FALSE(hull.boundaryContains(q));
            CHECK_FALSE(hull.interiorContains(q));
            CHECK(hull.index(q) == -1);
            CHECK_FALSE(hull.has(q));
        }
        return;
    }

    const pgl::Convex<Point> expected(std::vector<Point>(points.begin(), points.end()));
    const std::vector<Point> vertices = expected.vertices();
    REQUIRE(hull.vertices() == vertices);
    REQUIRE(hull.hullSize() == vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        CHECK(hull.vertex(static_cast<std::ptrdiff_t>(i)) == vertices[i]);
        CHECK(hull.index(vertices[i]) == static_cast<std::ptrdiff_t>(i));
        CHECK(hull.verticesContain(vertices[i]));
    }
    CHECK(hull.vertex(-1) == vertices.back());
    CHECK(hull.vertex(static_cast<std::ptrdiff_t>(vertices.size())) == vertices.front());
    CHECK(hull.convexHull().vertices() == vertices);
    CHECK(hull.lowerHull().vertices() == expected.lowerHull().vertices());
    CHECK(hull.upperHull().vertices() == expected.upperHull().vertices());
    CHECK(hull.bbox() == expected.bbox());

    for (int k = 0; k < 8; ++k) {
        Point d = at<Point>(rng.range(-3, 3), rng.range(-3, 3));
        if (k == 0) d = at<Point>(1, 0);
        if (k == 1) d = at<Point>(-1, 0);
        if (k == 2) d = at<Point>(0, 1);
        if (k == 3) d = at<Point>(0, -1);
        if (d == at<Point>(0, 0)) continue;
        // The lexicographically smallest maximizer, by brute force.
        const Point* best = nullptr;
        for (const Point& p : points) {
            if (best == nullptr || dot(d, *best) < dot(d, p)) best = &p;
        }
        CHECK(hull.extremeVertex(d) == *best);
    }

    for (const Point& v : vertices) {
        queries.push_back(v);
    }
    for (const Point& p : points) {
        queries.push_back(p);
    }
    for (const Point& q : queries) {
        CHECK(hull.contains(q) == expected.contains(q));
        CHECK(hull.boundaryContains(q) == expected.boundaryContains(q));
        CHECK(hull.interiorContains(q) == expected.interiorContains(q));
        CHECK(hull.has(q) == (points.count(q) > 0));
        if (std::find(vertices.begin(), vertices.end(), q) == vertices.end()) {
            CHECK(hull.index(q) == -1);
            CHECK_FALSE(hull.verticesContain(q));
        }
    }
}

// Random insertions and erasures over [lo, hi]^2, checked after every step.
template <class Point>
void runSequence(std::uint64_t seed, int lo, int hi, int steps) {
    Rng rng{seed};
    pgl::DynamicConvexHull<Point> hull;
    std::set<Point> points;
    for (int step = 0; step < steps; ++step) {
        const bool insert = points.empty() || rng.range(0, 99) < 60;
        Point p = at<Point>(rng.range(lo, hi), rng.range(lo, hi));
        if (!insert && rng.range(0, 3) != 0) {
            p = *std::next(points.begin(), rng.range(0, static_cast<int>(points.size()) - 1));
        }
        if (insert) {
            CHECK(hull.insert(p) == points.insert(p).second);
        } else {
            CHECK(hull.erase(p) == (points.erase(p) > 0));
        }
        checkAgainstRebuild(hull, points, rng, lo, hi);
    }
}

}  // namespace

TEST_CASE_TEMPLATE("DynamicConvexHull agrees with a rebuilt Convex over random updates", Point,
                   pgl::Point<int>, pgl::Point<long long>, pgl::Point<double>, pgl::Point<pgl::ERational>) {
    // Tiny grids: collinear points, vertical edges and repeats on most steps.
    const int ranges[] = {1, 2, 3, 5, 9};
    for (std::uint64_t round = 0; round < 25; ++round) {
        const int r = ranges[round % std::size(ranges)];
        runSequence<Point>(round + 1, -r / 2, r - r / 2, 120);
    }
}

TEST_CASE_TEMPLATE("DynamicConvexHull keeps exact bridges for large coordinates", Point,
                   pgl::Point<int>, pgl::Point<long long>) {
    for (std::uint64_t round = 0; round < 10; ++round) {
        runSequence<Point>(100 + round, -(1 << 29), 1 << 29, 200);
    }
}

TEST_CASE("DynamicConvexHull built from a container matches one built by insertions") {
    using Point = pgl::Point<int>;
    Rng rng{7};
    std::vector<Point> input;
    for (int i = 0; i < 500; ++i) {
        input.push_back(Point(rng.range(-50, 50), rng.range(-50, 50)));
    }
    input.push_back(input.front());  // a repeat is stored once
    const pgl::DynamicConvexHull bulk(input);
    pgl::DynamicConvexHull<Point> incremental;
    for (const Point& p : input) {
        incremental.insert(p);
    }
    CHECK(bulk.size() == incremental.size());
    CHECK(bulk.vertices() == incremental.vertices());
    CHECK(bulk.vertices() == pgl::Convex<Point>(input).vertices());

    // Erasing every point in turn empties it with the hull right at each step.
    std::set<Point> remaining(input.begin(), input.end());
    pgl::DynamicConvexHull<Point> shrinking = bulk;
    for (const Point& p : input) {
        if (remaining.erase(p) == 0) continue;
        REQUIRE(shrinking.erase(p));
        if (!remaining.empty()) {
            CHECK(shrinking.vertices() ==
                  pgl::Convex<Point>(std::vector<Point>(remaining.begin(), remaining.end())).vertices());
        }
    }
    CHECK(shrinking.empty());
    CHECK(shrinking.hullSize() == 0);
}

TEST_CASE("DynamicConvexHull is a value") {
    using Point = pgl::Point<int>;
    pgl::DynamicConvexHull<Point> a{Point(0, 0), Point(4, 0), Point(0, 4), Point(1, 1)};
    const pgl::DynamicConvexHull<Point> b = a;
    a.insert(Point(5, 5));
    a.erase(Point(0, 0));
    CHECK(b.hullSize() == 3);
    CHECK(b.vertices() == std::vector<Point>{Point(0, 0), Point(4, 0), Point(0, 4)});
    CHECK(a.vertices() == std::vector<Point>{Point(0, 4), Point(1, 1), Point(4, 0), Point(5, 5)});
    a = b;
    CHECK(a.vertices() == b.vertices());
    a.clear();
    CHECK(a.empty());
    CHECK(a.begin() == a.end());
    CHECK(b.size() == 4);
}

TEST_CASE("DynamicConvexHull returns points with their labels") {
    using Point = pgl::Point<int, std::string>;
    pgl::DynamicConvexHull<Point> hull;
    CHECK(hull.insert(Point(0, 0, "a")));
    CHECK(hull.insert(Point(2, 0, "b")));
    CHECK(hull.insert(Point(1, 2, "c")));
    CHECK_FALSE(hull.insert(Point(1, 2, "repeat")));
    CHECK(hull.vertex(0).label() == "a");
    CHECK(hull.vertex(1).label() == "b");
    CHECK(hull.vertex(2).label() == "c");
    CHECK(hull.extremeVertex(Point(0, 1)).label() == "c");
    CHECK(hull.erase(pgl::Point<int>(1, 2)));
    CHECK(hull.hullSize() == 2);
}

TEST_CASE("DynamicConvexHull breaks extreme ties toward the lexicographically smallest vertex") {
    using Point = pgl::Point<int>;
    // A square: every axis direction has two maximizers.
    const pgl::DynamicConvexHull<Point> hull{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)};
    CHECK(hull.extremeVertex(Point(1, 0)) == Point(2, 0));
    CHECK(hull.extremeVertex(Point(-1, 0)) == Point(0, 0));
    CHECK(hull.extremeVertex(Point(0, 1)) == Point(0, 2));
    CHECK(hull.extremeVertex(Point(0, -1)) == Point(0, 0));
}
