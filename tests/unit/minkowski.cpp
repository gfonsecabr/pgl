#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "pgl.hpp"

// Reference implementation: the Minkowski sum of two convex shapes is the
// convex hull of the pairwise sums of their vertices. It is quadratic where the
// library merges edges in linear time, which is exactly what makes it a useful
// independent check.
// Both live in a template so that the requires-expressions are dependent: a
// non-dependent one is a hard error rather than `false` under g++.
template <class A, class B>
inline constexpr bool summable = requires(const A& a, const B& b) { a.minkowskiSum(b); };

template <class A, class B>
inline constexpr bool addable = requires(const A& a, const B& b) { a + b; };

template <class Point, class A, class B>
static pgl::Convex<Point> bruteForceSum(const A& a, const B& b) {
    std::vector<Point> sums;
    for (const auto& p : a.vertices()) {
        for (const auto& q : b.vertices()) {
            sums.emplace_back(p.x() + q.x(), p.y() + q.y());
        }
    }
    return pgl::Convex<Point>(sums);
}

TEST_CASE_TEMPLATE("Summing with a point is a translation that preserves the shape type",
                   Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<int64_t>>) {
    using Number = typename Point::NumberType;
    const auto n = [](int value) { return static_cast<Number>(value); };
    const Point t(n(3), n(-5));

    const auto sameAsTranslation = [&t](const auto& shape) {
        const auto sum = shape.minkowskiSum(t);
        static_assert(std::is_same_v<std::remove_cvref_t<decltype(sum)>, std::remove_cvref_t<decltype(shape)>>,
                      "a translation must return the operand's own type");
        auto translated = shape;
        translated += t;
        CHECK(sum == translated);
        CHECK((shape + t) == translated);
        CHECK((t + shape) == translated);
    };

    sameAsTranslation(Point(n(1), n(2)));
    sameAsTranslation(pgl::Segment<Point>(Point(n(0), n(0)), Point(n(2), n(4))));
    sameAsTranslation(pgl::OrientedSegment<Point>(Point(n(2), n(4)), Point(n(0), n(0))));
    sameAsTranslation(pgl::Line<Point>(Point(n(0), n(0)), Point(n(2), n(4))));
    sameAsTranslation(pgl::OrientedLine<Point>(Point(n(0), n(0)), Point(n(2), n(4))));
    sameAsTranslation(pgl::Ray<Point>(Point(n(0), n(0)), Point(n(2), n(4))));
    sameAsTranslation(pgl::Halfplane<Point>(Point(n(0), n(0)), Point(n(2), n(4))));
    sameAsTranslation(pgl::Rectangle<Point>(Point(n(0), n(0)), Point(n(3), n(2))));
    sameAsTranslation(pgl::Triangle<Point>(Point(n(0), n(0)), Point(n(4), n(0)), Point(n(0), n(3))));
    sameAsTranslation(pgl::Disk<Point>(Point(n(0), n(0)), n(2)));
    sameAsTranslation(pgl::Convex<Point>(std::vector<Point>{
        Point(n(0), n(0)), Point(n(4), n(0)), Point(n(4), n(3)), Point(n(0), n(3))}));
    sameAsTranslation(pgl::Polygon<Point>(std::vector<Point>{
        Point(n(0), n(0)), Point(n(4), n(0)), Point(n(4), n(4)), Point(n(2), n(1)), Point(n(0), n(4))}));
    sameAsTranslation(pgl::Polyline<Point>(std::vector<Point>{
        Point(n(0), n(0)), Point(n(2), n(3)), Point(n(5), n(1))}));
    sameAsTranslation(pgl::MonotoneChain<Point>(std::vector<Point>{
        Point(n(0), n(0)), Point(n(2), n(3)), Point(n(5), n(1))}));
    sameAsTranslation(pgl::EmptyShape<Point>{});

    pgl::HalfplaneIntersection<Point> wedge;
    wedge.insert(pgl::Halfplane<Point>(Point(n(0), n(0)), Point(n(1), n(0))));
    wedge.insert(pgl::Halfplane<Point>(Point(n(0), n(0)), Point(n(0), n(-1))));
    sameAsTranslation(wedge);
}

TEST_CASE("A point on the left translates the shape on the right") {
    const pgl::Triangle<> triangle(0, 0, 4, 0, 0, 3);
    const pgl::Point<> t(1, 2);

    const auto sum = t.minkowskiSum(triangle);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(sum)>, pgl::Triangle<>>);
    CHECK(sum == pgl::Triangle<>(1, 2, 5, 2, 1, 5));
    CHECK((t + triangle) == sum);
}

TEST_CASE("Two segments sum to a parallelogram, and degenerate to a segment or a point") {
    using Point = pgl::Point<>;
    const pgl::Segment<> horizontal(0, 0, 2, 0);
    const pgl::Segment<> vertical(0, 0, 0, 3);

    const auto box = horizontal.minkowskiSum(vertical);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(box)>, pgl::Convex<>>);
    CHECK(box == pgl::Convex<>({0, 0, 2, 0, 2, 3, 0, 3}));
    CHECK(box == vertical.minkowskiSum(horizontal));  // the sum commutes

    const pgl::Segment<> slanted(0, 0, 1, 2);
    CHECK(horizontal.minkowskiSum(slanted) == pgl::Convex<>({0, 0, 2, 0, 3, 2, 1, 2}));

    // Parallel segments fuse their edges: the sum stays one-dimensional.
    const pgl::Segment<> longer(0, 0, 5, 0);
    const auto fused = horizontal.minkowskiSum(longer);
    CHECK(fused.isSegment());
    CHECK(*fused.getIfSegment() == pgl::Segment<>(0, 0, 7, 0));

    // Anti-parallel input is the same point set, so it fuses just as well.
    CHECK(horizontal.minkowskiSum(pgl::Segment<>(5, 0, 0, 0)) == fused);

    // Two collapsed segments are two points.
    const auto collapsed = pgl::Segment<>(1, 1, 1, 1).minkowskiSum(pgl::Segment<>(2, 3, 2, 3));
    CHECK(collapsed.isPoint());
    CHECK(*collapsed.getIfPoint() == Point(3, 4));
}

TEST_CASE("Two rectangles sum to a rectangle") {
    const pgl::Rectangle<> a(1, 2, 4, 6);
    const pgl::Rectangle<> b(-1, 0, 2, 1);

    const auto sum = a.minkowskiSum(b);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(sum)>, pgl::Rectangle<>>);
    CHECK(sum == pgl::Rectangle<>(0, 2, 6, 7));
    CHECK(sum == b.minkowskiSum(a));
    CHECK((a + b) == sum);

    // A degenerate rectangle is still a rectangle.
    CHECK(a.minkowskiSum(pgl::Rectangle<>(0, 0, 0, 0)) == a);
}

TEST_CASE("Two triangles sum to a hexagon, or fewer vertices when directions repeat") {
    const pgl::Triangle<> up(0, 0, 3, 0, 0, 3);
    const pgl::Triangle<> down(0, 0, -1, 0, 0, -1);

    // Opposite orientations: none of the six edge directions repeat.
    const auto hexagon = up.minkowskiSum(down);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(hexagon)>, pgl::Convex<>>);
    CHECK(hexagon.size() == 6);
    CHECK(hexagon == bruteForceSum<pgl::Point<>>(up, down));

    // Homothetic triangles share every edge direction, so all six edges fuse
    // into three and the sum is a scaled triangle.
    const auto scaled = up.minkowskiSum(pgl::Triangle<>(0, 0, 1, 0, 0, 1));
    CHECK(scaled.size() == 3);
    CHECK(scaled == pgl::Convex<>({0, 0, 4, 0, 0, 4}));
}

TEST_CASE("Mixed bounded convex operands sum to a convex polygon") {
    const pgl::Triangle<> triangle(0, 0, 4, 0, 0, 4);
    const pgl::Rectangle<> rectangle(0, 0, 2, 1);
    const pgl::Segment<> segment(0, 0, 1, 3);
    const pgl::OrientedSegment<> oriented(3, 1, 0, 0);
    const pgl::Convex<> convex({0, 0, 3, 0, 4, 2, 1, 4});

    CHECK(triangle.minkowskiSum(rectangle) == bruteForceSum<pgl::Point<>>(triangle, rectangle));
    CHECK(rectangle.minkowskiSum(triangle) == bruteForceSum<pgl::Point<>>(rectangle, triangle));
    CHECK(segment.minkowskiSum(triangle) == bruteForceSum<pgl::Point<>>(segment, triangle));
    CHECK(oriented.minkowskiSum(convex) == bruteForceSum<pgl::Point<>>(oriented, convex));
    CHECK(convex.minkowskiSum(convex) == bruteForceSum<pgl::Point<>>(convex, convex));
    CHECK((triangle + convex) == triangle.minkowskiSum(convex));
}

TEST_CASE("A degenerate convex operand behaves like the point set it covers") {
    const pgl::Convex<> collapsedToPoint(std::vector<pgl::Point<>>{{2, 2}, {2, 2}});
    const pgl::Convex<> collapsedToSegment({0, 0, 1, 1, 2, 2});  // collinear
    const pgl::Triangle<> triangle(0, 0, 3, 0, 0, 3);

    CHECK(collapsedToPoint.isPoint());
    CHECK(triangle.minkowskiSum(collapsedToPoint) == pgl::Convex<>({2, 2, 5, 2, 2, 5}));

    CHECK(collapsedToSegment.isSegment());
    CHECK(triangle.minkowskiSum(collapsedToSegment) ==
          bruteForceSum<pgl::Point<>>(triangle, collapsedToSegment));

    // An empty convex polygon has no points to add, so it absorbs.
    CHECK(triangle.minkowskiSum(pgl::Convex<>()).size() == 0);
}

TEST_CASE_TEMPLATE("Random convex sums agree with the hull of the pairwise vertex sums",
                   Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<int64_t>>) {
    using Number = typename Point::NumberType;
    using Convex = pgl::Convex<Point>;

    std::mt19937 rng(20260725);
    std::uniform_int_distribution<int> coordinate(-12, 12);
    std::uniform_int_distribution<int> count(1, 9);

    const auto randomConvex = [&]() {
        std::vector<Point> points;
        const int size = count(rng);
        for (int i = 0; i < size; ++i) {
            points.emplace_back(static_cast<Number>(coordinate(rng)),
                                static_cast<Number>(coordinate(rng)));
        }
        return Convex(points);
    };

    for (int trial = 0; trial < 300; ++trial) {
        const Convex a = randomConvex();
        const Convex b = randomConvex();
        if (a.size() == 0 || b.size() == 0) {
            continue;
        }

        const auto sum = a.minkowskiSum(b);
        CHECK(sum == bruteForceSum<Point>(a, b));
        // The merge walks each operand's edges once, fusing shared directions.
        CHECK(sum.size() <= a.size() + b.size());
        CHECK(sum == b.minkowskiSum(a));

        // Every pairwise vertex sum lies in the result, by definition.
        for (const auto& p : a) {
            for (const auto& q : b) {
                CHECK(sum.contains(Point(p.x() + q.x(), p.y() + q.y())));
            }
        }
    }
}

TEST_CASE("The empty shape absorbs every operand") {
    const pgl::EmptyShape<> empty;
    const pgl::Triangle<> triangle(0, 0, 4, 0, 0, 3);

    static_assert(std::is_same_v<decltype(empty.minkowskiSum(triangle)), pgl::EmptyShape<>>);
    CHECK(empty.minkowskiSum(triangle) == pgl::EmptyShape<>{});
    CHECK(triangle.minkowskiSum(empty) == pgl::EmptyShape<>{});
    CHECK(empty.minkowskiSum(pgl::Line<>(0, 0, 1, 1)) == pgl::EmptyShape<>{});
    CHECK((empty + triangle) == pgl::EmptyShape<>{});
}

TEST_CASE("Coordinates promote the way point addition promotes them") {
    const pgl::Segment<pgl::Point<int>> segment(0, 0, 2, 2);
    const pgl::Point<double> translation(0.5, 0.5);

    const auto moved = segment.minkowskiSum(translation);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(moved)>,
                                 pgl::Segment<pgl::Point<double>>>);
    CHECK(moved == pgl::Segment<pgl::Point<double>>(0.5, 0.5, 2.5, 2.5));

    const pgl::Triangle<pgl::Point<double>> triangle(0.0, 0.0, 1.0, 0.0, 0.0, 1.0);
    const auto sum = pgl::Triangle<pgl::Point<int>>(0, 0, 2, 0, 0, 2).minkowskiSum(triangle);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(sum)>,
                                 pgl::Convex<pgl::Point<double>>>);
    CHECK(sum == pgl::Convex<pgl::Point<double>>({0.0, 0.0, 3.0, 0.0, 0.0, 3.0}));

    // The vector-backed shapes promote like every other one: a wider
    // translation widens the result instead of being truncated into it.
    const pgl::Convex<pgl::Point<int>> convex({0, 0, 2, 0, 0, 2});
    const auto movedConvex = convex.minkowskiSum(translation);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(movedConvex)>,
                                 pgl::Convex<pgl::Point<double>>>);
    CHECK(movedConvex == pgl::Convex<pgl::Point<double>>({0.5, 0.5, 2.5, 0.5, 0.5, 2.5}));

    const pgl::Disk<pgl::Point<int>> disk(pgl::Point<int>(0, 0), 2);
    const auto movedDisk = disk.minkowskiSum(translation);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(movedDisk)>,
                                 pgl::Disk<pgl::Point<double>>>);
    CHECK(movedDisk == pgl::Disk<pgl::Point<double>>(pgl::Point<double>(0.5, 0.5), 2.0));
}

TEST_CASE("A concrete shape sums with a wrapper on either side") {
    const pgl::Point<> point(1, 2);
    const pgl::Shape<> wrapped = pgl::Segment<>(0, 0, 3, 0);

    const auto moved = point.minkowskiSum(wrapped);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(moved)>, pgl::Shape<>>);
    CHECK(moved.isSegment());
    CHECK(*moved.getIfSegment() == pgl::Segment<>(1, 2, 4, 2));
    CHECK((point + wrapped) == moved);
}

TEST_CASE("Integer operands stay exact and never leave the integers") {
    const pgl::Triangle<> big(0, 0, 1000000, 3, 7, 1000000);
    const pgl::Rectangle<> box(-1000000, -1000000, 0, 0);

    const auto sum = big.minkowskiSum(box);
    static_assert(std::is_same_v<typename decltype(sum)::NumberType, int>);
    CHECK(sum == bruteForceSum<pgl::Point<>>(big, box));
}

TEST_CASE("Shape dispatches on the stored alternatives at run time") {
    const pgl::Shape<> triangle = pgl::Triangle<>(0, 0, 3, 0, 0, 3);
    const pgl::Shape<> segment = pgl::Segment<>(0, 0, 1, 1);
    const pgl::Shape<> point = pgl::Point<>(2, 5);
    const pgl::Shape<> disk = pgl::Disk<>(pgl::Point<>(0, 0), 2);

    const auto convexSum = triangle.minkowskiSum(segment);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(convexSum)>, pgl::Shape<>>);
    CHECK(convexSum.isConvex());
    CHECK(*convexSum.getIfConvex() ==
          pgl::Triangle<>(0, 0, 3, 0, 0, 3).minkowskiSum(pgl::Segment<>(0, 0, 1, 1)));

    // A point keeps the other alternative's type, here through the wrapper.
    const auto moved = triangle.minkowskiSum(point);
    CHECK(moved.isTriangle());
    CHECK(*moved.getIfTriangle() == pgl::Triangle<>(2, 5, 5, 5, 2, 8));

    // Mixing a wrapper with a concrete shape works in both directions.
    CHECK(triangle.minkowskiSum(pgl::Segment<>(0, 0, 1, 1)) == convexSum);
    CHECK(pgl::Triangle<>(0, 0, 3, 0, 0, 3).minkowskiSum(segment) == convexSum);
    CHECK((triangle + segment) == convexSum);

    // A pair with no representable sum is only detectable at run time.
    CHECK_THROWS_AS(triangle.minkowskiSum(disk), std::logic_error);
    CHECK(disk.minkowskiSum(point).isDisk());
}

TEST_CASE("Pairs with no representable sum are rejected at compile time") {
    using Point = pgl::Point<>;

    // Unbounded convex operands: the sum needs a HalfplaneIntersection.
    static_assert(!summable<pgl::Halfplane<>, pgl::Segment<>>);
    static_assert(!summable<pgl::Line<>, pgl::Triangle<>>);
    static_assert(!summable<pgl::Ray<>, pgl::Ray<>>);
    static_assert(!summable<pgl::HalfplaneIntersection<>, pgl::Convex<>>);

    // Curved operands: the sum is not a disk and not exactly representable.
    static_assert(!summable<pgl::Disk<>, pgl::Disk<>>);
    static_assert(!summable<pgl::Disk<>, pgl::Triangle<>>);

    // Non-convex operands with no area to fill: still no sum at all.
    static_assert(!summable<pgl::Polyline<>, pgl::Segment<>>);
    static_assert(!summable<pgl::MonotoneChain<>, pgl::Segment<>>);

    // A non-convex *area* operand is the exception, and it is not a widening of
    // this concept: its sum can enclose a hole, so it is a set of regions rather
    // than one shape and lives in a separate overload set on `Polygon` and
    // `PolygonWithHoles` (see polygonwithholes_minkowski.cpp). `summable` sees
    // it because it asks only whether the call compiles.
    static_assert(!pgl::MinkowskiSummableConcept<pgl::Polygon<>, pgl::Triangle<>>);
    static_assert(summable<pgl::Polygon<>, pgl::Triangle<>>);
    static_assert(
        std::is_same_v<decltype(std::declval<const pgl::Polygon<>&>().minkowskiSum(
                           std::declval<const pgl::Triangle<>&>())),
                       std::vector<pgl::PolygonWithHoles<Point>>>);

    // Translation stays available for all of them.
    static_assert(summable<pgl::Halfplane<>, Point>);
    static_assert(summable<pgl::Disk<>, Point>);
    static_assert(summable<pgl::Polygon<>, Point>);
    static_assert(summable<Point, pgl::Polygon<>>);

    // operator+ is constrained by the concept, not by the overload set, so it
    // does not turn an unsupported pair into a hard error somewhere inside the
    // sum -- and it stays out of the region-valued case, whose result no single
    // shape can hold.
    static_assert(!addable<pgl::Polygon<>, pgl::Triangle<>>);
    static_assert(!addable<pgl::Segment<>, int>);
    static_assert(addable<pgl::Segment<>, pgl::Triangle<>>);
    static_assert(addable<pgl::Polygon<>, Point>);
}

TEST_CASE("The sum is associative and commutes on bounded convex shapes") {
    const pgl::Segment<> a(0, 0, 2, 1);
    const pgl::Triangle<> b(0, 0, 3, 0, 1, 4);
    const pgl::Rectangle<> c(0, 0, 1, 2);

    CHECK(a.minkowskiSum(b).minkowskiSum(c) == a.minkowskiSum(b.minkowskiSum(c)));
    CHECK(a.minkowskiSum(b) == b.minkowskiSum(a));
    CHECK((a + b) + c == a + (b + c));
}
