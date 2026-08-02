#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <type_traits>
#include <vector>

#include "pgl.hpp"

// Dependent on purpose: a non-dependent requires-expression is a hard error
// rather than `false` under g++.
template <class A, class B>
inline constexpr bool intersectable = requires(const A& a, const B& b) { a.intersection(b); };

// The empty set annihilates an intersection from either side, and says so in
// the return type rather than through an empty optional or vector.
template <class Shape>
static void absorbsIntersection(const Shape& shape) {
    const pgl::EmptyShape<> empty;

    static_assert(intersectable<Shape, pgl::EmptyShape<>>);
    static_assert(intersectable<pgl::EmptyShape<>, Shape>);
    static_assert(std::is_same_v<decltype(shape.intersection(empty)), pgl::EmptyShape<>>);
    static_assert(std::is_same_v<decltype(empty.intersection(shape)), pgl::EmptyShape<>>);

    CHECK(shape.intersection(empty) == pgl::EmptyShape<>{});
    CHECK(empty.intersection(shape) == pgl::EmptyShape<>{});
}

TEST_CASE("The empty shape absorbs every intersection operand") {
    using Point = pgl::Point<int>;

    absorbsIntersection(Point(1, 2));
    absorbsIntersection(pgl::Segment<>(0, 0, 4, 3));
    absorbsIntersection(pgl::OrientedSegment<>(0, 0, 4, 3));
    absorbsIntersection(pgl::Line<>(0, 0, 1, 2));
    absorbsIntersection(pgl::OrientedLine<>(0, 0, 1, 2));
    absorbsIntersection(pgl::Ray<>(0, 0, 1, 2));
    absorbsIntersection(pgl::Halfplane<>(Point(0, 0), Point(1, 0)));
    absorbsIntersection(pgl::Rectangle<>(0, 0, 4, 3));
    absorbsIntersection(pgl::Triangle<>(0, 0, 4, 0, 0, 3));
    absorbsIntersection(pgl::Convex<>({0, 0, 4, 0, 4, 3, 0, 3}));
    absorbsIntersection(pgl::Disk<>({2, 3}, 4));
    absorbsIntersection(pgl::Polygon<>({0, 0, 10, 0, 10, 10, 0, 10}));
    absorbsIntersection(pgl::Polyline<>({0, 0, 8, 0, 8, 8, 0, 8}));
    absorbsIntersection(pgl::MonotoneChain<>(
        std::vector<Point>{Point(0, 0), Point(2, 3), Point(5, 1)}));
    absorbsIntersection(pgl::EmptyShape<>{});

    pgl::HalfplaneIntersection<Point> wedge;
    wedge.insert(pgl::Halfplane<Point>(Point(0, 0), Point(1, 0)));
    wedge.insert(pgl::Halfplane<Point>(Point(0, 0), Point(0, -1)));
    absorbsIntersection(wedge);

    const pgl::Polygon<> outer({0, 0, 10, 0, 10, 10, 0, 10});
    const pgl::Polygon<> hole({4, 4, 8, 4, 8, 8, 4, 8});
    absorbsIntersection(pgl::PolygonWithHoles<>(outer, std::vector{hole}));
}
