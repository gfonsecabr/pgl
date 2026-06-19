#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <set>
#include <sstream>
#include <string>
#include <unordered_set>

#include "pgl.hpp"

TEST_CASE("Shape defaults to empty and stores the active alternative") {
    using Point = pgl::Point<int>;
    using EmptyShape = pgl::EmptyShape<Point>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape empty;
    REQUIRE(empty.empty());
    REQUIRE(empty.holdsAlternative<EmptyShape>());
    CHECK(empty.getIf<EmptyShape>() != nullptr);
    CHECK(empty.size() == 0);

    const Shape segment = Segment({1, 2}, {3, 4});
    REQUIRE(segment.holdsAlternative<Segment>());
    CHECK(segment.getIf<Segment>() != nullptr);
    CHECK(*segment.getIf<Segment>() == Segment({1, 2}, {3, 4}));

    const Shape triangle = Triangle({0, 0}, {4, 0}, {0, 3});
    REQUIRE(triangle.holdsAlternative<Triangle>());
    CHECK(triangle.getIf<Triangle>() != nullptr);
    CHECK(*triangle.getIf<Triangle>() == Triangle({0, 0}, {4, 0}, {0, 3}));

    const Convex square({{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Shape convex = square;
    REQUIRE(convex.holdsAlternative<Convex>());
    CHECK(convex.getIf<Convex>() != nullptr);
    CHECK(*convex.getIf<Convex>() == square);
    CHECK(convex.size() == 4);
    CHECK(convex[0] == Point(0, 0));
    CHECK(convex.get(0) == Point(0, 0));
    CHECK(convex.get(4) == Point(0, 0));
    CHECK(convex.get(-1) == Point(0, 4));
}

TEST_CASE("Shape streams, compares, and hashes through the wrapped value") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape first = Segment({1, 2}, {3, 4});
    const Shape second = Segment({3, 4}, {1, 2});
    const Shape other = Rectangle({1, 2}, {3, 4});
    const Shape triangle = Triangle({0, 0}, {4, 0}, {0, 3});
    const Shape convex = Convex({{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Shape convex_dup = Convex({{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    std::ostringstream stream;
    stream << first;
    CHECK(stream.str() == "(1,2)--(3,4)");

    std::ostringstream triangle_stream;
    triangle_stream << triangle;
    CHECK(triangle_stream.str() == "<(0,0)(4,0)(0,3)>");

    std::ostringstream convex_stream;
    convex_stream << convex;
    CHECK(convex_stream.str() == "Convex[(0,0),(4,0),(4,4),(0,4)]");

    CHECK(first == second);
    CHECK_FALSE(first == other);
    CHECK(convex == convex_dup);
    CHECK_FALSE(convex == triangle);
    const bool ordered = (first < other) || (other < first);
    CHECK(ordered);
    const bool convex_ordered = (convex < triangle) || (triangle < convex);
    CHECK(convex_ordered);

    std::set<Shape> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(second);
    ordered_set.insert(other);
    ordered_set.insert(triangle);
    ordered_set.insert(convex);
    ordered_set.insert(convex_dup);
    CHECK(ordered_set.size() == 4);

    std::unordered_set<Shape> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(second);
    unordered_set.insert(other);
    unordered_set.insert(triangle);
    unordered_set.insert(convex);
    unordered_set.insert(convex_dup);
    CHECK(unordered_set.size() == 4);
}

TEST_CASE("Shape dispatches isDegenerate across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Shape = pgl::Shape<Point>;

    CHECK_FALSE(Shape(Point(0, 0)).isDegenerate());
    CHECK(Shape(Segment({1, 2}, {1, 2})).isDegenerate());
    CHECK_FALSE(Shape(Segment({1, 2}, {3, 4})).isDegenerate());
    CHECK(Shape(Line({1, 2}, {1, 2})).isDegenerate());
    CHECK(Shape(Rectangle({0, 0}, {4, 0})).isDegenerate());
    CHECK_FALSE(Shape(Rectangle({0, 0}, {4, 3})).isDegenerate());
    CHECK(Shape(Convex({{0, 0}, {1, 0}, {2, 0}})).isDegenerate());
    CHECK_FALSE(Shape(Convex({{0, 0}, {4, 0}, {4, 4}, {0, 4}})).isDegenerate());
}

TEST_CASE("Shape dispatches contains and intersects across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape point = Point(2, 0);
    const Shape segment = Segment({0, 0}, {4, 0});
    const Shape rectangle = Rectangle({0, 0}, {4, 3});
    const Shape upper = Halfplane({0, 0}, {4, 0});
    const Shape triangle = Triangle({0, 0}, {4, 0}, {0, 4});
    const Shape convex = Convex({{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Shape outside_point = Point(5, 5);
    const Shape inside_convex = Convex({{1, 1}, {3, 1}, {3, 3}, {1, 3}});

    CHECK(segment.contains(point));
    CHECK_FALSE(point.contains(segment));

    CHECK(segment.intersects(point));
    CHECK(point.intersects(segment));

    CHECK(rectangle.contains(segment));
    CHECK(rectangle.intersects(segment));
    CHECK(rectangle.intersects(point));
    CHECK_FALSE(point.intersects(Shape(Point(2, 1))));

    CHECK(upper.contains(segment));
    CHECK(upper.intersects(rectangle));
    CHECK(rectangle.intersects(upper));

    CHECK(triangle.contains(point));
    CHECK(triangle.intersects(segment));
    CHECK(segment.intersects(triangle));

    CHECK(convex.contains(point));
    CHECK(convex.contains(segment));
    CHECK(convex.contains(triangle));
    CHECK(convex.contains(inside_convex));
    CHECK_FALSE(convex.contains(outside_point));
    CHECK(convex.intersects(segment));
    CHECK(convex.intersects(upper));
    CHECK(convex.intersects(inside_convex));
    CHECK_FALSE(convex.intersects(outside_point));
    CHECK(segment.intersects(convex));
    CHECK(rectangle.intersects(convex));
}

TEST_CASE("Shape dispatches boundaryContains across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape edge_point = Point(2, 0);
    const Shape inside_point = Point(2, 1);
    const Shape edge_segment = Segment({0, 1}, {0, 2});
    const Shape interior_segment = Segment({1, 1}, {3, 1});
    const Shape rectangle = Rectangle({0, 0}, {4, 3});
    const Shape triangle = Triangle({0, 0}, {4, 0}, {0, 4});
    const Shape convex = Convex({{0, 0}, {4, 0}, {4, 3}, {0, 3}});

    CHECK(rectangle.boundaryContains(edge_point));
    CHECK_FALSE(rectangle.boundaryContains(inside_point));
    CHECK(rectangle.boundaryContains(edge_segment));
    CHECK_FALSE(rectangle.boundaryContains(interior_segment));

    CHECK(triangle.boundaryContains(edge_point));
    CHECK_FALSE(triangle.boundaryContains(inside_point));
    CHECK_FALSE(edge_point.boundaryContains(rectangle));

    CHECK(convex.boundaryContains(edge_point));
    CHECK(convex.boundaryContains(edge_segment));
    CHECK_FALSE(convex.boundaryContains(inside_point));
    CHECK_FALSE(convex.boundaryContains(interior_segment));
}

TEST_CASE("Shape dispatches interior and topological predicates when available") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape rectangle = Rectangle({0, 0}, {4, 3});
    const Shape inside_segment = Segment({1, 1}, {3, 2});
    const Shape boundary_segment = Segment({0, 1}, {0, 2});
    const Shape crossing_segment = Segment({-1, 1}, {5, 1});

    CHECK(rectangle.interiorContains(inside_segment));
    CHECK_FALSE(rectangle.interiorContains(boundary_segment));

    CHECK(rectangle.interiorsIntersect(crossing_segment));
    CHECK(rectangle.separates(crossing_segment));
    CHECK(rectangle.crosses(crossing_segment));
    CHECK_FALSE(boundary_segment.crosses(rectangle));

    const Shape triangle = Triangle({0, 0}, {6, 0}, {0, 6});
    const Shape triangle_cut = Segment({-1, 2}, {5, 2});
    CHECK(triangle.interiorsIntersect(triangle_cut));
    CHECK(triangle.separates(triangle_cut));
    CHECK(triangle.crosses(triangle_cut));

    using Convex = pgl::Convex<Point>;
    const Shape convex = Convex({{0, 0}, {6, 0}, {6, 6}, {0, 6}});
    const Shape convex_cut = Segment({-1, 2}, {7, 2});
    const Shape inside_convex = Convex({{1, 1}, {5, 1}, {5, 5}, {1, 5}});
    const Shape boundary_convex_edge = Segment({0, 1}, {0, 5});
    CHECK(convex.interiorContains(inside_convex));
    CHECK_FALSE(convex.interiorContains(boundary_convex_edge));
    CHECK(convex.interiorsIntersect(convex_cut));
    CHECK(convex.interiorsIntersect(inside_convex));
    CHECK(convex.separates(convex_cut));
    CHECK(convex.crosses(convex_cut));
    CHECK_FALSE(boundary_convex_edge.crosses(convex));
}

TEST_CASE("Shape stores a Polygon and dispatches its predicates") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Shape = pgl::Shape<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Shape shape = square;
    REQUIRE(shape.holdsAlternative<Polygon>());
    CHECK(shape.getIf<Polygon>() != nullptr);
    CHECK(*shape.getIf<Polygon>() == square);

    // Point containment through the wrapper.
    CHECK(shape.contains(Point(5, 5)));
    CHECK_FALSE(shape.contains(Point(50, 50)));

    // intersects is symmetric across the wrapper boundary.
    const Shape chord = Segment({5, -5}, {5, 15});
    const Shape away = Segment({50, 0}, {50, 10});
    CHECK(shape.intersects(chord));
    CHECK(chord.intersects(shape));
    CHECK_FALSE(shape.intersects(away));

    // A chord clear across separates and crosses the polygon, both directions.
    CHECK(shape.separates(chord));
    CHECK(shape.crosses(chord));
    CHECK(chord.crosses(shape));

    // A line that meets the polygon separates it; one that misses does not.
    const Shape cutting_line = Line({5, -1}, {5, 20});
    const Shape missing_line = Line({50, 0}, {50, 20});
    CHECK(cutting_line.separates(shape));
    CHECK_FALSE(missing_line.separates(shape));
}

TEST_CASE("Shape dispatches squaredDistance across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape origin = Point(0, 0);
    const Shape corner = Point(3, 4);

    // Shape against Shape, and against a concrete alternative, agree.
    CHECK(origin.squaredDistance<int>(corner) == 25);
    CHECK(origin.squaredDistance<int>(Point(3, 4)) == 25);
    CHECK(origin.squaredDistance<int>(Segment({3, 4}, {3, 10})) == 25);

    // Symmetric through the forwarding visitor (lower rank forwards to higher).
    const Shape segment = Segment({3, 4}, {3, 10});
    CHECK(origin.squaredDistance<int>(segment) == segment.squaredDistance<int>(origin));

    // ResultNumber defaults to the wrapper's NumberType.
    static_assert(std::is_same_v<decltype(origin.squaredDistance(corner)), int>);

    // Pairs added on the concrete shapes are reachable through the wrapper.
    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    CHECK(t1.squaredDistance<int>(t2) == 64);
    CHECK(t2.squaredDistance<int>(t1) == 64);

    const Shape below = Triangle({0, 10}, {2, 10}, {1, 13});
    const Shape down = Halfplane({0, 0}, {1, 0});  // boundary y = 0
    CHECK(below.squaredDistance<int>(down) == down.squaredDistance<int>(below));
}
