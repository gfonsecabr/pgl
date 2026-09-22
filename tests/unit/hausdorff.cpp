#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <tuple>
#include <type_traits>
#include <vector>

#include "pgl.hpp"

using Coord = pgl::Point<int>;
using Exact = pgl::ERational;
using EPoint = pgl::EPoint;

using SegmentShape = pgl::Segment<Coord>;
using OrientedSegmentShape = pgl::OrientedSegment<Coord>;
using RectangleShape = pgl::Rectangle<Coord>;
using TriangleShape = pgl::Triangle<Coord>;
using ConvexShape = pgl::Convex<Coord>;
using ChainShape = pgl::MonotoneChain<Coord>;
using PolylineShape = pgl::Polyline<Coord>;
using PolygonShape = pgl::Polygon<Coord>;
using RegionShape = pgl::PolygonWithHoles<Coord>;
using SetShape = pgl::PolygonSet<Coord>;

namespace {

// The same shape over exact coordinates, for the Minkowski sums of the oracle.
template <class S>
auto exactly(const S& shape) {
    if constexpr (pgl::PointConcept<S>) {
        return EPoint(shape);
    } else if constexpr (pgl::OrientedSegmentConcept<S>) {
        return pgl::OrientedSegment<EPoint>(shape);
    } else if constexpr (pgl::SegmentConcept<S>) {
        return pgl::Segment<EPoint>(shape);
    } else if constexpr (pgl::RectangleConcept<S>) {
        return pgl::Rectangle<EPoint>(shape);
    } else if constexpr (pgl::TriangleConcept<S>) {
        return pgl::Triangle<EPoint>(shape);
    } else if constexpr (pgl::ConvexConcept<S>) {
        return pgl::Convex<EPoint>(shape);
    } else if constexpr (pgl::MonotoneChainConcept<S>) {
        return pgl::MonotoneChain<EPoint>(shape);
    } else if constexpr (pgl::PolylineConcept<S>) {
        return pgl::Polyline<EPoint>(shape);
    } else if constexpr (pgl::PolygonConcept<S>) {
        return pgl::Polygon<EPoint>(shape);
    } else if constexpr (pgl::PolygonWithHolesConcept<S>) {
        return pgl::PolygonWithHoles<EPoint>(shape);
    } else {
        return pgl::PolygonSet<EPoint>(shape);
    }
}

// The unit ball of the norm, scaled by r.
pgl::Convex<EPoint> ball(const Exact& r, bool linf) {
    const Exact zero{};
    if (linf) {
        return pgl::Convex<EPoint>({EPoint(r, r), EPoint(-r, r), EPoint(-r, -r), EPoint(r, -r)});
    }
    return pgl::Convex<EPoint>({EPoint(r, zero), EPoint(zero, r), EPoint(-r, zero), EPoint(zero, -r)});
}

// Whether every point of `a` is within r of `b`: a ⊆ b ⊕ rD.
template <class A, class B>
bool within(const A& a, const B& b, const Exact& r, bool linf) {
    return b.minkowskiSum(ball(r, linf)).contains(a);
}

template <class A, class B>
Exact hausdorff(const A& a, const B& b, bool linf) {
    return linf ? a.hausdorffDistanceLInf(b) : a.hausdorffDistanceL1(b);
}

// The whole contract, checked against an oracle that shares no code with the
// search: r is the Hausdorff distance exactly when each shape lies within r of
// the other and not within anything less. Also: the distance is symmetric, and
// does not depend on the coordinate type the shapes are stored in.
template <class A, class B>
void checkHausdorff(const A& a, const B& b) {
    const auto ea = exactly(a);
    const auto eb = exactly(b);
    for (const bool linf : {false, true}) {
        CAPTURE(linf);
        const Exact r = hausdorff(a, b, linf);
        CHECK(hausdorff(b, a, linf) == r);
        CHECK(hausdorff(ea, eb, linf) == r);
        if (r == Exact{}) {
            continue;
        }
        CHECK(within(ea, eb, r, linf));
        CHECK(within(eb, ea, r, linf));
        const Exact less = r * Exact(999, 1000);
        CHECK_FALSE((within(ea, eb, less, linf) && within(eb, ea, less, linf)));
    }
}

template <class A, class Tuple>
void checkAgainstEvery(const A& first, const Tuple& shapes) {
    std::apply([&](const auto&... second) { (checkHausdorff(first, second), ...); }, shapes);
}

// One shape of every bounded polygonal kind, overlapping one another so that
// the pairs meet, nest and miss in varied ways.
auto everyKind() {
    return std::make_tuple(
        Coord(4, 3),
        SegmentShape({1, 1}, {7, 2}),
        OrientedSegmentShape({6, 6}, {2, 3}),
        RectangleShape({2, 2}, {5, 4}),
        TriangleShape({0, 0}, {8, 1}, {3, 7}),
        ConvexShape({Coord(1, 3), Coord(3, 0), Coord(7, 2), Coord(6, 6), Coord(2, 6)}),
        ChainShape({Coord(0, 2), Coord(2, 6), Coord(5, 1), Coord(8, 5)}),
        PolylineShape({Coord(1, 1), Coord(7, 7), Coord(7, 1), Coord(1, 7)}),
        PolygonShape({Coord(0, 0), Coord(8, 0), Coord(8, 8), Coord(4, 3), Coord(0, 8)}),
        RegionShape(PolygonShape({Coord(0, 0), Coord(9, 0), Coord(9, 9), Coord(0, 9)}),
                    std::vector<PolygonShape>{PolygonShape({Coord(3, 3), Coord(6, 3), Coord(6, 6), Coord(3, 6)})}),
        SetShape(std::vector<RegionShape>{RegionShape(PolygonShape({Coord(0, 0), Coord(3, 0), Coord(0, 3)})),
                                          RegionShape(PolygonShape({Coord(5, 5), Coord(9, 6), Coord(6, 9)}))}));
}

}  // namespace

TEST_CASE("the center of a square is farthest from its boundary") {
    const PolygonShape square({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10)});
    const PolylineShape border({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10), Coord(0, 0)});

    CHECK(square.hausdorffDistanceL1(border) == 5);
    CHECK(square.hausdorffDistanceLInf(border) == 5);
    CHECK(border.hausdorffDistanceL1(square) == 5);
    checkHausdorff(square, border);
}

TEST_CASE("the farthest point can sit in the middle of a hole") {
    const PolygonShape outer({Coord(0, 0), Coord(20, 0), Coord(20, 20), Coord(0, 20)});
    const RegionShape holed(outer, std::vector<PolygonShape>{
                                       PolygonShape({Coord(8, 8), Coord(12, 8), Coord(12, 12), Coord(8, 12)})});

    // Every point of the holed square is in the full one, and the center of the
    // hole is 2 away from the holed square in both norms.
    CHECK(holed.hausdorffDistanceL1(outer) == 2);
    CHECK(holed.hausdorffDistanceLInf(outer) == 2);

    // Against its outer boundary the farthest points ring the hole instead.
    const PolylineShape ring({Coord(0, 0), Coord(20, 0), Coord(20, 20), Coord(0, 20), Coord(0, 0)});
    CHECK(holed.hausdorffDistanceL1(ring) == 8);
    CHECK(holed.hausdorffDistanceLInf(ring) == 8);
    checkHausdorff(holed, outer);
}

TEST_CASE("the farthest point may be interior to an edge") {
    const SegmentShape bridge({0, 0}, {10, 0});
    const SetShape posts(std::vector<RegionShape>{
        RegionShape(PolygonShape({Coord(-1, 0), Coord(0, 0), Coord(0, 1), Coord(-1, 1)})),
        RegionShape(PolygonShape({Coord(10, 0), Coord(11, 0), Coord(11, 1), Coord(10, 1)}))});

    CHECK(bridge.hausdorffDistanceL1(posts) == 5);
    CHECK(posts.hausdorffDistanceLInf(bridge) == 5);
    checkHausdorff(bridge, posts);
}

TEST_CASE("a point is measured to the farthest vertex") {
    const ChainShape chain({Coord(0, 0), Coord(5, 5), Coord(10, 0)});
    const PolygonShape polygon({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10)});

    CHECK(chain.hausdorffDistanceL1(Coord(0, 0)) == 10);
    CHECK(Coord(0, 0).hausdorffDistanceLInf(chain) == 10);
    CHECK(polygon.hausdorffDistanceL1(Coord(3, 4)) == 13);
    CHECK(Coord(3, 4).hausdorffDistanceLInf(polygon) == 7);
}

TEST_CASE("the distance can be a fraction") {
    const SetShape set(std::vector<RegionShape>{
        RegionShape(PolygonShape({Coord(2, 5), Coord(3, 5), Coord(7, 8)})),
        RegionShape(PolygonShape({Coord(0, 0), Coord(7, 1), Coord(8, 2), Coord(5, 2), Coord(8, 5)})),
        RegionShape(PolygonShape({Coord(0, 3), Coord(1, 1), Coord(2, 4), Coord(3, 3), Coord(0, 7), Coord(0, 5)}))});
    const ChainShape chain({Coord(0, 0), Coord(2, 5), Coord(5, 8), Coord(6, 6), Coord(8, 6)});

    CHECK(set.hausdorffDistanceL1(chain) == Exact(396, 73));
    checkHausdorff(set, chain);

    // An integer result truncates the exact distance; a floating-point one
    // computes in floating point.
    CHECK(set.hausdorffDistanceL1<int>(chain) == 5);
    CHECK(chain.hausdorffDistanceL1<double>(set) == doctest::Approx(396.0 / 73.0));
}

TEST_CASE("every pair of bounded polygonal shapes meets the Minkowski characterization") {
    const auto shapes = everyKind();
    std::apply([&](const auto&... first) { (checkAgainstEvery(first, shapes), ...); }, shapes);
}

TEST_CASE("degenerate shapes stand for their point sets") {
    // A polygon of zero area is the segments it retraces, and a one-vertex chain
    // its vertex.
    const PolygonShape flat({Coord(0, 0), Coord(6, 0), Coord(3, 0)});
    const ChainShape dot({Coord(2, 5)});
    const PolylineShape retraced({Coord(0, 0), Coord(6, 0), Coord(0, 0)});

    CHECK(flat.hausdorffDistanceL1(retraced) == 0);
    CHECK(flat.hausdorffDistanceL1(dot) == 9);
    CHECK(dot.hausdorffDistanceLInf(SegmentShape({0, 0}, {6, 0})) == 5);
    checkHausdorff(flat, dot);
    checkHausdorff(retraced, PolygonShape({Coord(0, 0), Coord(6, 0), Coord(6, 2)}));
}

TEST_CASE("the default result type divides only where the distance can be a fraction") {
    const PolygonShape polygon({Coord(0, 0), Coord(4, 0), Coord(0, 4)});
    const SegmentShape segment({0, 0}, {1, 1});
    static_assert(std::is_same_v<decltype(polygon.hausdorffDistanceL1(Coord(1, 1))), int>);
    static_assert(std::is_same_v<decltype(Coord(1, 1).hausdorffDistanceLInf(polygon)), int>);
    static_assert(std::is_same_v<decltype(polygon.hausdorffDistanceL1(segment)), Exact>);
    static_assert(std::is_same_v<decltype(segment.hausdorffDistanceLInf(polygon)), Exact>);
    static_assert(std::is_same_v<decltype(polygon.hausdorffDistanceL1<double>(segment)), double>);
    CHECK(polygon.hausdorffDistanceL1(segment) == 4);
}

TEST_CASE("floating-point coordinates compute in floating point") {
    using Real = pgl::Point<double>;
    const pgl::Polygon<Real> square({Real(0, 0), Real(1, 0), Real(1, 1), Real(0, 1)});
    const pgl::Polyline<Real> diagonal({Real(0, 0), Real(1, 1)});

    // The corners (1,0) and (0,1) are farthest from the diagonal: 1 in L1 and
    // 1/2 in LInf.
    CHECK(square.hausdorffDistanceL1(diagonal) == doctest::Approx(1.0));
    CHECK(square.hausdorffDistanceLInf(diagonal) == doctest::Approx(0.5));
}

TEST_CASE("the Shape wrapper dispatches to the concrete pairs") {
    const pgl::Shape<Coord> polygon = PolygonShape({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10)});
    const pgl::Shape<Coord> border = PolylineShape({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10), Coord(0, 0)});

    CHECK(polygon.hausdorffDistanceL1<Exact>(border) == 5);
    CHECK(polygon.hausdorffDistanceLInf<Exact>(PolylineShape({Coord(0, 0), Coord(10, 0)})) == 10);
}

TEST_CASE("random shapes meet the Minkowski characterization") {
    std::mt19937 generator(20260919);
    auto coordinate = [&generator] { return std::uniform_int_distribution<int>(0, 6)(generator); };
    auto point = [&coordinate] { return Coord(coordinate(), coordinate()); };
    auto polyline = [&point] {
        std::vector<Coord> vertices{point()};
        while (vertices.size() < 4) {
            const Coord next = point();
            if (next != vertices.back()) {
                vertices.push_back(next);
            }
        }
        return PolylineShape(vertices);
    };
    auto chain = [&point] {
        std::vector<Coord> vertices;
        for (int i = 0; i < 4; ++i) {
            vertices.push_back(point());
        }
        std::sort(vertices.begin(), vertices.end());
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
        return ChainShape(vertices);
    };
    // A simple polygon: a triangle with a notch cut into one side.
    auto polygon = [&point] {
        while (true) {
            const TriangleShape triangle(point(), point(), point());
            if (triangle.isDegenerate()) {
                continue;
            }
            const Coord a = triangle[0];
            const Coord b = triangle[1];
            const Coord c = triangle[2];
            const PolygonShape notched({a, b, Coord((a.x() + b.x() + c.x() + 1) / 3, (a.y() + b.y() + c.y() + 1) / 3), c});
            if (notched.isSimple() && !notched.isDegenerate()) {
                return notched;
            }
        }
    };

    for (int trial = 0; trial < 6; ++trial) {
        checkHausdorff(polyline(), polyline());
        checkHausdorff(polygon(), polyline());
        checkHausdorff(polygon(), polygon());
        checkHausdorff(chain(), polygon());
    }
}

// -----------------------------------------------------------------------------
// Euclidean

namespace {

using Real = pgl::Point<double>;

// The same shape over double coordinates, for the sampling oracle.
template <class S>
auto approximately(const S& shape) {
    if constexpr (pgl::PointConcept<S>) {
        return Real(shape);
    } else if constexpr (pgl::OrientedSegmentConcept<S>) {
        return pgl::OrientedSegment<Real>(shape);
    } else if constexpr (pgl::SegmentConcept<S>) {
        return pgl::Segment<Real>(shape);
    } else if constexpr (pgl::RectangleConcept<S>) {
        return pgl::Rectangle<Real>(shape);
    } else if constexpr (pgl::TriangleConcept<S>) {
        return pgl::Triangle<Real>(shape);
    } else if constexpr (pgl::ConvexConcept<S>) {
        return pgl::Convex<Real>(shape);
    } else if constexpr (pgl::MonotoneChainConcept<S>) {
        return pgl::MonotoneChain<Real>(shape);
    } else if constexpr (pgl::PolylineConcept<S>) {
        return pgl::Polyline<Real>(shape);
    } else if constexpr (pgl::PolygonConcept<S>) {
        return pgl::Polygon<Real>(shape);
    } else if constexpr (pgl::PolygonWithHolesConcept<S>) {
        return pgl::PolygonWithHoles<Real>(shape);
    } else {
        return pgl::PolygonSet<Real>(shape);
    }
}

// A regular polygon of `sides` vertices at distance `circumradius` from the
// origin, its coordinates rounded to multiples of 2^-24.
pgl::Convex<EPoint> regular(double circumradius, int sides) {
    const double pi = std::acos(-1.0);
    const long long scale = 1LL << 24;
    std::vector<EPoint> vertices;
    for (int i = 0; i < sides; ++i) {
        const double angle = 2 * pi * i / sides;
        vertices.emplace_back(Exact(std::llround(circumradius * std::cos(angle) * scale), scale),
                              Exact(std::llround(circumradius * std::sin(angle) * scale), scale));
    }
    return pgl::Convex<EPoint>(vertices);
}

// Whether every point of `a` is within the polygon `ball` of `b`: a ⊆ b ⊕ ball.
template <class A, class B>
bool withinPolygon(const A& a, const B& b, const pgl::Convex<EPoint>& ball) {
    return b.minkowskiSum(ball).contains(a);
}

template <class A, class B>
double euclidean(const A& a, const B& b) {
    return a.template squaredHausdorffDistance<double>(b);
}

// The contract, checked against an oracle that shares no code with the search:
// a regular 32-gon inscribed in the disk of radius r(1 + 10^-4) / cos(pi/32)
// contains the disk of radius r, and one circumscribed about the disk of radius
// 0.99 r lies inside the disk of radius r; so the first must bring each shape
// within the other and the second must not. Also: the distance is symmetric, and
// does not depend on the coordinate type the shapes are stored in.
template <class A, class B>
void checkEuclidean(const A& a, const B& b) {
    const auto ea = exactly(a);
    const auto eb = exactly(b);
    const double r2 = euclidean(a, b);
    CHECK(euclidean(b, a) == doctest::Approx(r2));
    CHECK(euclidean(ea, eb) == doctest::Approx(r2));
    CHECK(euclidean(approximately(a), approximately(b)) == doctest::Approx(r2));
    if (r2 == 0) {
        return;
    }
    const double r = std::sqrt(r2);
    const double slack = std::cos(std::acos(-1.0) / 32);
    const auto outer = regular(r * 1.0001 / slack, 32);
    CHECK(withinPolygon(ea, eb, outer));
    CHECK(withinPolygon(eb, ea, outer));
    const auto inner = regular(r * 0.99 / slack, 32);
    CHECK_FALSE((withinPolygon(ea, eb, inner) && withinPolygon(eb, ea, inner)));
}

template <class A, class Tuple>
void checkEuclideanAgainstEvery(const A& first, const Tuple& shapes) {
    std::apply([&](const auto&... second) { (checkEuclidean(first, second), ...); }, shapes);
}

// The shapes of `everyKind` that make a pair approximate.
auto everyNonConvexKind() {
    const auto all = everyKind();
    return std::make_tuple(std::get<6>(all), std::get<7>(all), std::get<8>(all), std::get<9>(all),
                           std::get<10>(all));
}

// The largest squared distance to `b` over a grid of spacing `step` on `a`,
// its edges sampled ten times more finely: at most the directed Hausdorff
// distance, and within `step` of it.
template <class A, class B>
double sampledDirected(const A& a, const B& b, double step) {
    const auto ra = approximately(a);
    const auto rb = approximately(b);
    double best = 0;
    const auto measure = [&](const Real& p) {
        const double value = rb.template squaredDistance<double>(p);
        best = value < best ? best : value;
    };
    for (const auto& edge : ra.edges()) {
        const double length = std::sqrt(edge[0].squaredDistance(edge[1]));
        const int count = static_cast<int>(length / (step / 10)) + 1;
        for (int i = 0; i <= count; ++i) {
            const double t = static_cast<double>(i) / count;
            measure(Real(edge[0].x() + t * (edge[1].x() - edge[0].x()), edge[0].y() + t * (edge[1].y() - edge[0].y())));
        }
    }
    if constexpr (pgl::PolygonalRegionConcept<A>) {
        const auto box = ra.bbox();
        for (double x = box.min().x(); x <= box.max().x(); x += step) {
            for (double y = box.min().y(); y <= box.max().y(); y += step) {
                if (ra.contains(Real(x, y))) {
                    measure(Real(x, y));
                }
            }
        }
    }
    return best;
}

template <class A, class B>
void checkSampled(const A& a, const B& b) {
    const double step = 0.02;
    const double sampled = std::max(sampledDirected(a, b, step), sampledDirected(b, a, step));
    const double r2 = euclidean(a, b);
    CHECK(sampled <= r2 * (1 + 1e-12) + 1e-12);
    CHECK(std::sqrt(r2) <= std::sqrt(sampled) + step);
}

}  // namespace

TEST_CASE("Euclidean: the farthest point can sit in the middle of a hole") {
    const PolygonShape outer({Coord(0, 0), Coord(20, 0), Coord(20, 20), Coord(0, 20)});
    const RegionShape holed(outer, std::vector<PolygonShape>{
                                       PolygonShape({Coord(8, 8), Coord(12, 8), Coord(12, 12), Coord(8, 12)})});
    CHECK(holed.squaredHausdorffDistance(outer) == doctest::Approx(4.0));
    CHECK(outer.squaredHausdorffDistance(holed) == doctest::Approx(4.0));

    const PolylineShape ring({Coord(0, 0), Coord(20, 0), Coord(20, 20), Coord(0, 20), Coord(0, 0)});
    CHECK(outer.squaredHausdorffDistance(ring) == doctest::Approx(100.0));
    // The holed square's farthest points from its outer boundary are the four
    // corners of the hole, (8,8) and its images.
    CHECK(holed.squaredHausdorffDistance(ring) == doctest::Approx(64.0));
    checkEuclidean(holed, outer);
}

TEST_CASE("Euclidean: the distance is generally irrational") {
    // Along the edge from (0,0) to (2,2) the nearest part of the polyline is its
    // corner (0,0) and then its top side, which tie where sqrt(2) t = 2 - t.
    const TriangleShape triangle({0, 0}, {2, 2}, {-3, 2});
    const PolylineShape polyline({Coord(0, 0), Coord(-2, 2), Coord(2, 2)});
    const double expected = 24 - 16 * std::sqrt(2.0);
    CHECK(triangle.squaredHausdorffDistance(polyline) == doctest::Approx(expected).epsilon(1e-12));
    CHECK(polyline.squaredHausdorffDistance(triangle) == doctest::Approx(expected).epsilon(1e-12));
    checkEuclidean(triangle, polyline);
    checkSampled(triangle, polyline);
}

TEST_CASE("Euclidean: the farthest point may be a vertex of the medial axis") {
    // Three posts around an empty middle: the point farthest from all three is
    // where their distances tie, inside the triangle.
    const TriangleShape triangle({0, 0}, {12, 0}, {6, 10});
    const SetShape posts(std::vector<RegionShape>{
        RegionShape(PolygonShape({Coord(0, 0), Coord(2, 0), Coord(1, 1)})),
        RegionShape(PolygonShape({Coord(10, 0), Coord(12, 0), Coord(11, 1)})),
        RegionShape(PolygonShape({Coord(5, 9), Coord(7, 9), Coord(6, 10)}))});
    checkEuclidean(triangle, posts);
    checkSampled(triangle, posts);

    // Between two parallel walls every point of the mid-line ties.
    const RectangleShape corridor({0, 0}, {10, 4});
    const PolylineShape walls({Coord(0, 0), Coord(10, 0), Coord(10, 4), Coord(0, 4)});
    CHECK(corridor.squaredHausdorffDistance(walls) == doctest::Approx(4.0));
    checkSampled(corridor, walls);
}

TEST_CASE("Euclidean: the farthest point may tie between vertices only") {
    // Four diamonds point their corners at the origin from 4 away, and a plus
    // sign holds them all, so only the plus is ever far from the diamonds: at
    // its center, which is nearer to the four corners than to any edge.
    const auto diamond = [](int dx, int dy) {
        // Corner at (4 dx, 4 dy), pointing back at the origin.
        const int px = -dy;
        const int py = dx;
        return RegionShape(PolygonShape({Coord(4 * dx, 4 * dy), Coord(6 * dx + 2 * px, 6 * dy + 2 * py),
                                         Coord(8 * dx, 8 * dy), Coord(6 * dx - 2 * px, 6 * dy - 2 * py)}));
    };
    const SetShape diamonds(std::vector<RegionShape>{diamond(1, 0), diamond(0, 1), diamond(-1, 0), diamond(0, -1)});
    const PolygonShape plus({Coord(8, 0), Coord(6, 2), Coord(2, 2), Coord(2, 6), Coord(0, 8), Coord(-2, 6),
                             Coord(-2, 2), Coord(-6, 2), Coord(-8, 0), Coord(-6, -2), Coord(-2, -2), Coord(-2, -6),
                             Coord(0, -8), Coord(2, -6), Coord(2, -2), Coord(6, -2)});
    CHECK(plus.squaredHausdorffDistance(diamonds) == doctest::Approx(16.0));
    checkSampled(plus, diamonds);
}

TEST_CASE("Euclidean: the farthest point may tie between edges only") {
    // The incenter of a 5-12-13 triangle is 2 from each side.
    const TriangleShape triangle({0, 0}, {12, 0}, {0, 5});
    const PolylineShape sides({Coord(0, 0), Coord(12, 0), Coord(0, 5), Coord(0, 0)});
    CHECK(triangle.squaredHausdorffDistance(sides) == doctest::Approx(4.0));
    checkSampled(triangle, sides);
}

TEST_CASE("Euclidean: a point is measured exactly to the farthest vertex") {
    const PolygonShape polygon({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10)});
    const ChainShape chain({Coord(0, 0), Coord(5, 5), Coord(10, 0)});
    CHECK(polygon.squaredHausdorffDistance(Coord(3, 4)) == 7 * 7 + 6 * 6);
    CHECK(Coord(0, 0).squaredHausdorffDistance(chain) == 100);
    static_assert(std::is_same_v<decltype(polygon.squaredHausdorffDistance(Coord(3, 4))), int>);
    static_assert(std::is_same_v<decltype(Coord(3, 4).squaredHausdorffDistance(polygon)), int>);
}

TEST_CASE("Euclidean: a pair that need not be convex answers in floating point") {
    const PolygonShape polygon({Coord(0, 0), Coord(4, 0), Coord(0, 4)});
    const SegmentShape segment({0, 0}, {1, 1});
    const ConvexShape convex({Coord(0, 0), Coord(4, 0), Coord(4, 4)});
    static_assert(std::is_same_v<decltype(polygon.squaredHausdorffDistance(segment)), double>);
    static_assert(std::is_same_v<decltype(segment.squaredHausdorffDistance(polygon)), double>);
    static_assert(std::is_same_v<decltype(convex.squaredHausdorffDistance(polygon)), double>);
    static_assert(std::is_same_v<decltype(convex.squaredHausdorffDistance<float>(polygon)), float>);
    // Two convex shapes stay exact.
    static_assert(std::is_same_v<decltype(convex.squaredHausdorffDistance(segment)), Exact>);
    // An exact type receives the floating-point value converted.
    static_assert(std::is_same_v<decltype(polygon.squaredHausdorffDistance<Exact>(convex)), Exact>);
    CHECK(polygon.squaredHausdorffDistance<Exact>(convex) == 8);
    CHECK(polygon.squaredHausdorffDistance(segment) == doctest::Approx(10.0));
}

TEST_CASE("Euclidean: degenerate shapes stand for their point sets") {
    const PolygonShape flat({Coord(0, 0), Coord(6, 0), Coord(3, 0)});
    const ChainShape dot({Coord(2, 5)});
    const PolylineShape retraced({Coord(0, 0), Coord(6, 0), Coord(0, 0)});
    CHECK(flat.squaredHausdorffDistance(retraced) == 0);
    CHECK(flat.squaredHausdorffDistance(dot) == doctest::Approx(41.0));
    checkEuclidean(flat, dot);
    checkEuclidean(retraced, PolygonShape({Coord(0, 0), Coord(6, 0), Coord(6, 2)}));
}

TEST_CASE("Euclidean: the Shape wrapper dispatches to the concrete pairs") {
    const pgl::Shape<Coord> polygon = PolygonShape({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10)});
    const pgl::Shape<Coord> border = PolylineShape({Coord(0, 0), Coord(10, 0), Coord(10, 10), Coord(0, 10), Coord(0, 0)});
    CHECK(polygon.squaredHausdorffDistance<double>(border) == doctest::Approx(25.0));
    CHECK(border.squaredHausdorffDistance<double>(SegmentShape({0, 0}, {10, 0})) == doctest::Approx(100.0));
}

TEST_CASE("Euclidean: every pair involving a shape that need not be convex meets the characterization") {
    const auto shapes = everyKind();
    std::apply([&](const auto&... first) { (checkEuclideanAgainstEvery(first, everyNonConvexKind()), ...); }, shapes);
}

TEST_CASE("Euclidean: random shapes agree with the oracles") {
    std::mt19937 generator(20260922);
    auto coordinate = [&generator] { return std::uniform_int_distribution<int>(0, 6)(generator); };
    auto point = [&coordinate] { return Coord(coordinate(), coordinate()); };
    auto polyline = [&point] {
        std::vector<Coord> vertices{point()};
        while (vertices.size() < 4) {
            const Coord next = point();
            if (next != vertices.back()) {
                vertices.push_back(next);
            }
        }
        return PolylineShape(vertices);
    };
    auto polygon = [&point] {
        while (true) {
            const TriangleShape triangle(point(), point(), point());
            if (triangle.isDegenerate()) {
                continue;
            }
            const Coord a = triangle[0];
            const Coord b = triangle[1];
            const Coord c = triangle[2];
            const PolygonShape notched({a, b, Coord((a.x() + b.x() + c.x() + 1) / 3, (a.y() + b.y() + c.y() + 1) / 3), c});
            if (notched.isSimple() && !notched.isDegenerate()) {
                return notched;
            }
        }
    };

    for (int trial = 0; trial < 6; ++trial) {
        const auto first = polygon();
        const auto second = polygon();
        const auto line = polyline();
        checkEuclidean(first, line);
        checkEuclidean(first, second);
        checkSampled(first, line);
        checkSampled(first, second);
        checkSampled(line, polyline());
    }
}
