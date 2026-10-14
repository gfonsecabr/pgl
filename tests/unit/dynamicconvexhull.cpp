#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <variant>
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


// Every alternative of pgl::Shape over [lo, hi]^2, undefined ones left out. A
// small box makes touching, collinear and degenerate operands common.
template <class Point>
std::vector<pgl::Shape<Point>> shapesAround(Rng& rng, int lo, int hi) {
    using Number = typename Point::NumberType;
    const auto draw = [&] { return at<Point>(rng.range(lo, hi), rng.range(lo, hi)); };
    const auto draws = [&](int count) {
        std::vector<Point> points;
        for (int i = 0; i < count; ++i) {
            points.push_back(draw());
        }
        return points;
    };
    std::vector<pgl::Shape<Point>> out;
    const auto keep = [&](const auto& shape) {
        if (!shape.isUndefined()) {
            out.emplace_back(shape);
        }
    };
    out.emplace_back(pgl::EmptyShape<Point>());
    out.emplace_back(draw());
    keep(pgl::Segment<Point>(draw(), draw()));
    keep(pgl::OrientedSegment<Point>(draw(), draw()));
    keep(pgl::Line<Point>(draw(), draw()));
    keep(pgl::OrientedLine<Point>(draw(), draw()));
    keep(pgl::Ray<Point>(draw(), draw()));
    keep(pgl::Halfplane<Point>(draw(), draw()));
    keep(pgl::Rectangle<Point>(draw(), draw()));
    keep(pgl::Rectangle<Point>());
    keep(pgl::Triangle<Point>(draw(), draw(), draw()));
    keep(pgl::Disk<Point>(draw(), draw(), draw()));
    keep(pgl::Disk<Point>(draw(), Number(rng.range(0, 3))));
    keep(pgl::Convex<Point>());
    keep(pgl::Convex<Point>(draws(rng.range(1, 7))));
    // Eight vertices: more than twice those of a triangular hull.
    const int cx = rng.range(lo, hi), cy = rng.range(lo, hi);
    keep(pgl::Convex<Point>(std::vector<Point>{
        at<Point>(cx + 1, cy + 2), at<Point>(cx - 1, cy + 2), at<Point>(cx - 2, cy + 1), at<Point>(cx - 2, cy - 1),
        at<Point>(cx - 1, cy - 2), at<Point>(cx + 1, cy - 2), at<Point>(cx + 2, cy - 1), at<Point>(cx + 2, cy + 1)}));
    keep(pgl::MonotoneChain<Point>(draws(rng.range(1, 5))));
    keep(pgl::Polyline<Point>(draws(rng.range(1, 5))));
    const pgl::Convex<Point> ring(draws(rng.range(3, 7)));
    if (!ring.empty()) {
        keep(ring.asPolygon());
    }
    const pgl::Polygon<Point> simple(draws(rng.range(3, 6)));
    if (!simple.isUndefined() && simple.isSimple()) {
        out.emplace_back(simple);
    }
    if (!ring.empty() && !ring.isDegenerate()) {
        const pgl::PolygonWithHoles<Point> region(ring.asPolygon());
        if (region.isValid()) {
            keep(region);
        }
    }
    // A rectangle with a diamond hole touching all four sides.
    const int left = 2 * rng.range(lo, hi / 2), bottom = 2 * rng.range(lo, hi / 2);
    const int right = left + 2 * rng.range(1, 3), top = bottom + 2 * rng.range(1, 3);
    const int midX = (left + right) / 2, midY = (bottom + top) / 2;
    const pgl::PolygonWithHoles<Point> pinched(
        pgl::Polygon<Point>({at<Point>(left, bottom), at<Point>(right, bottom), at<Point>(right, top),
                             at<Point>(left, top)}),
        std::vector{pgl::Polygon<Point>({at<Point>(left, midY), at<Point>(midX, top), at<Point>(right, midY),
                                         at<Point>(midX, bottom)})});
    if (pinched.isValid()) {
        keep(pinched);
    }
    const auto united = pgl::Rectangle<Point>(draw(), draw()).template regularizedUnion<Number>(
        pgl::Rectangle<Point>(draw(), draw()));
    if (united.isValid()) {
        keep(united);
    }
    std::vector<pgl::Halfplane<Point>> halfplanes;
    for (int i = rng.range(1, 3); i > 0; --i) {
        const pgl::Halfplane<Point> h(draw(), draw());
        if (!h.isUndefined()) {
            halfplanes.push_back(h);
        }
    }
    if (!halfplanes.empty()) {
        keep(pgl::HalfplaneIntersection<Point>(halfplanes));
    }
    const pgl::Rectangle<Point> box(draw(), draw());
    if (!box.empty()) {
        keep(pgl::HalfplaneIntersection<Point>(box));
    }
    return out;
}

template <class T>
std::string describe(const T& value) {
    std::ostringstream out;
    if constexpr (requires { out << value; }) {
        out << value;
    } else {
        out << "the empty shape";
    }
    return out.str();
}

// Asks every predicate of hulls over random points, degenerate ones included,
// against every shape alternative, and compares with the rebuilt Convex.
template <class Point>
void checkPredicatesAgainstConvex(std::uint64_t seed, int rounds) {
    Rng rng{seed};
    for (int round = 0; round < rounds; ++round) {
        const bool large = round % 4 == 3;
        const int lo = large ? -12 : -3, hi = large ? 12 : 3;
        const bool collinear = round % 5 == 0;
        pgl::DynamicConvexHull<Point> hull;
        for (int i = rng.range(0, large ? 60 : 9); i > 0; --i) {
            const int x = rng.range(lo, hi);
            hull.insert(at<Point>(x, collinear ? 1 - x : rng.range(lo, hi)));
        }
        const pgl::Convex<Point> expected = hull.convexHull();
        for (const pgl::Shape<Point>& shape : shapesAround<Point>(rng, lo - 1, hi + 1)) {
            std::visit([&](const auto& other) {
                INFO(describe(expected), " against ", describe(other));
                CHECK(hull.contains(other) == expected.contains(other));
                CHECK(hull.boundaryContains(other) == expected.boundaryContains(other));
                CHECK(hull.interiorContains(other) == expected.interiorContains(other));
                CHECK(hull.intersects(other) == expected.intersects(other));
                CHECK(hull.interiorsIntersect(other) == expected.interiorsIntersect(other));
                CHECK(hull.separates(other) == expected.separates(other));
                CHECK(hull.crosses(other) == expected.crosses(other));
            }, shape.variant());
            CHECK(hull.contains(shape) == expected.contains(shape));
            CHECK(hull.boundaryContains(shape) == expected.boundaryContains(shape));
            CHECK(hull.interiorContains(shape) == expected.interiorContains(shape));
            CHECK(hull.intersects(shape) == expected.intersects(shape));
            CHECK(hull.interiorsIntersect(shape) == expected.interiorsIntersect(shape));
            CHECK(hull.separates(shape) == expected.separates(shape));
            CHECK(hull.crosses(shape) == expected.crosses(shape));
        }
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

TEST_CASE_TEMPLATE("DynamicConvexHull predicates agree with its Convex against every shape", Point,
                   pgl::Point<int>, pgl::Point<long long>, pgl::Point<double>, pgl::Point<pgl::ERational>) {
    checkPredicatesAgainstConvex<Point>(11, 600);
}

TEST_CASE("DynamicConvexHull predicates on a hull of labelled points") {
    using Point = pgl::Point<int, std::string>;
    const pgl::DynamicConvexHull<Point> hull{Point(0, 0, "a"), Point(4, 0, "b"), Point(0, 4, "c")};
    CHECK(hull.intersects(pgl::Segment<pgl::Point<int>>(pgl::Point<int>(-1, 1), pgl::Point<int>(5, 1))));
    CHECK(hull.crosses(pgl::Line<pgl::Point<int>>(pgl::Point<int>(0, 1), pgl::Point<int>(1, 1))));
    CHECK(hull.boundaryContains(pgl::Segment<pgl::Point<int>>(pgl::Point<int>(1, 3), pgl::Point<int>(3, 1))));
    // The half-plane left of the hypotenuse, run from (0, 4) to (4, 0), faces away.
    CHECK_FALSE(hull.interiorsIntersect(pgl::Halfplane<pgl::Point<int>>(pgl::Point<int>(0, 4), pgl::Point<int>(4, 0))));
    CHECK(hull.intersects(pgl::Halfplane<pgl::Point<int>>(pgl::Point<int>(0, 4), pgl::Point<int>(4, 0))));
}
