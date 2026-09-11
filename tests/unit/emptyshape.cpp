#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <type_traits>
#include <vector>

#include "pgl.hpp"

// Dependent on purpose: a non-dependent requires-expression is a hard error
// rather than `false` under g++.
template <class A, class B>
inline constexpr bool intersectable = requires(const A& a, const B& b) { a.template intersection<int>(b); };

// The empty set annihilates an intersection from either side, and says so in
// the return type rather than through an empty optional or vector.
template <class Shape>
static void absorbsIntersection(const Shape& shape) {
    const pgl::EmptyShape<> empty;

    static_assert(intersectable<Shape, pgl::EmptyShape<>>);
    static_assert(intersectable<pgl::EmptyShape<>, Shape>);
    static_assert(std::is_same_v<decltype(shape.template intersection<int>(empty)), pgl::EmptyShape<>>);
    static_assert(std::is_same_v<decltype(empty.intersection<int>(shape)), pgl::EmptyShape<>>);

    CHECK(shape.template intersection<int>(empty) == pgl::EmptyShape<>{});
    CHECK(empty.intersection<int>(shape) == pgl::EmptyShape<>{});
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

template <class A, class B>
inline constexpr bool relatable = requires(const A& a, const B& b) {
    a.contains(b);
    a.boundaryContains(b);
    a.interiorContains(b);
    a.intersects(b);
    a.interiorsIntersect(b);
    a.separates(b);
    a.crosses(b);
    a.samePointSet(b);
};

// The empty set lies inside every shape and meets none. Removing it leaves the
// other shape as it was, so it cuts no connected shape, and removing anything
// from it leaves nothing to come apart.
template <class Shape>
static void relatesToConnected(const Shape& shape) {
    const pgl::EmptyShape<> empty;

    static_assert(relatable<Shape, pgl::EmptyShape<>>);
    static_assert(relatable<pgl::EmptyShape<>, Shape>);

    CHECK(shape.contains(empty));
    CHECK(shape.boundaryContains(empty));
    CHECK(shape.interiorContains(empty));
    CHECK_FALSE(shape.intersects(empty));
    CHECK_FALSE(shape.interiorsIntersect(empty));
    CHECK_FALSE(shape.separates(empty));
    CHECK_FALSE(shape.crosses(empty));
    CHECK_FALSE(shape.samePointSet(empty));

    CHECK_FALSE(empty.contains(shape));
    CHECK_FALSE(empty.boundaryContains(shape));
    CHECK_FALSE(empty.interiorContains(shape));
    CHECK_FALSE(empty.intersects(shape));
    CHECK_FALSE(empty.interiorsIntersect(shape));
    CHECK_FALSE(empty.separates(shape));
    CHECK_FALSE(empty.crosses(shape));
    CHECK_FALSE(empty.samePointSet(shape));
}

TEST_CASE("The empty shape answers every predicate against every shape, both ways") {
    using Point = pgl::Point<int>;

    relatesToConnected(Point(1, 2));
    relatesToConnected(pgl::Segment<>(0, 0, 4, 3));
    relatesToConnected(pgl::OrientedSegment<>(0, 0, 4, 3));
    relatesToConnected(pgl::Line<>(0, 0, 1, 2));
    relatesToConnected(pgl::OrientedLine<>(0, 0, 1, 2));
    relatesToConnected(pgl::Ray<>(0, 0, 1, 2));
    relatesToConnected(pgl::Halfplane<>(Point(0, 0), Point(1, 0)));
    relatesToConnected(pgl::Rectangle<>(0, 0, 4, 3));
    relatesToConnected(pgl::Triangle<>(0, 0, 4, 0, 0, 3));
    relatesToConnected(pgl::Convex<>({0, 0, 4, 0, 4, 3, 0, 3}));
    relatesToConnected(pgl::Disk<>({2, 3}, 4));
    relatesToConnected(pgl::Polygon<>({0, 0, 10, 0, 10, 10, 0, 10}));
    relatesToConnected(pgl::Polyline<>({0, 0, 8, 0, 8, 8, 0, 8}));
    relatesToConnected(pgl::MonotoneChain<>(
        std::vector<Point>{Point(0, 0), Point(2, 3), Point(5, 1)}));

    pgl::HalfplaneIntersection<Point> wedge;
    wedge.insert(pgl::Halfplane<Point>(Point(0, 0), Point(1, 0)));
    wedge.insert(pgl::Halfplane<Point>(Point(0, 0), Point(0, -1)));
    relatesToConnected(wedge);
    // With no half-planes it is the whole plane, not the empty set.
    relatesToConnected(pgl::HalfplaneIntersection<Point>{});

    const pgl::Polygon<> outer({0, 0, 10, 0, 10, 10, 0, 10});
    const pgl::Polygon<> hole({4, 4, 8, 4, 8, 8, 4, 8});
    const pgl::PolygonWithHoles<> region(outer, std::vector{hole});
    relatesToConnected(region);
    relatesToConnected(pgl::PolygonSet<Point>(region));

    const pgl::EmptyShape<> empty;
    static_assert(relatable<pgl::EmptyShape<>, pgl::EmptyShape<>>);
    CHECK(empty.contains(empty));
    CHECK(empty.boundaryContains(empty));
    CHECK(empty.interiorContains(empty));
    CHECK_FALSE(empty.intersects(empty));
    CHECK_FALSE(empty.interiorsIntersect(empty));
    CHECK_FALSE(empty.separates(empty));
    CHECK_FALSE(empty.crosses(empty));
    CHECK(empty.samePointSet(empty));
}

TEST_CASE("The empty shape separates only a set already in pieces") {
    using Point = pgl::Point<int>;
    using Regions = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    const pgl::EmptyShape<> empty;
    const pgl::PolygonWithHoles<> left(pgl::Polygon<>({0, 0, 2, 0, 2, 2, 0, 2}));
    const pgl::PolygonWithHoles<> right(pgl::Polygon<>({10, 0, 12, 0, 12, 2, 10, 2}));
    const pgl::PolygonWithHoles<> corner(pgl::Polygon<>({2, 2, 4, 2, 4, 4, 2, 4}));

    const Regions apart(std::vector{left, right});
    const Regions joined(std::vector{left, corner});  // one piece through a shared corner
    REQUIRE_FALSE(apart.isConnected());
    REQUIRE(joined.isConnected());

    CHECK(empty.separates(apart));
    CHECK_FALSE(empty.separates(joined));
    CHECK_FALSE(empty.separates(Regions{}));

    // As a shape missing the set does.
    CHECK(Point(100, 100).separates(apart));

    // The empty set stays in one piece, so it is never cut and never crosses.
    CHECK_FALSE(apart.separates(empty));
    CHECK_FALSE(empty.crosses(apart));
    CHECK_FALSE(apart.crosses(empty));

    // Through the variant, from either side.
    CHECK(empty.separates(Shape(apart)));
    CHECK(Shape(empty).separates(apart));
    CHECK(Shape(empty).separates(Shape(apart)));
    CHECK_FALSE(empty.separates(Shape(joined)));
    CHECK_FALSE(empty.separates(Shape(empty)));
}
