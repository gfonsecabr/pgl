#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

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

TEST_CASE("Shape empty answers the wrapped shape's own emptiness") {
    using Point = pgl::Point<int>;
    using EmptyShape = pgl::EmptyShape<Point>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Chain = pgl::MonotoneChain<Point>;
    using Polyline = pgl::Polyline<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Halfplanes = pgl::HalfplaneIntersection<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using Regions = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    // The EmptyShape alternative is empty, and answers so without an empty() of
    // its own.
    static_assert(Shape{}.empty());
    CHECK(Shape{EmptyShape{}}.empty());

    // Every alternative that has an empty state answers its own empty().
    CHECK(Shape{Rectangle{}}.empty());
    CHECK(Shape{Convex{}}.empty());
    CHECK(Shape{Chain{}}.empty());
    CHECK(Shape{Polyline{}}.empty());
    CHECK(Shape{Polygon{}}.empty());
    CHECK(Shape{Region{}}.empty());
    CHECK(Shape{Regions{}}.empty());

    // The same alternatives, holding points, are not empty.
    CHECK_FALSE(Shape{Rectangle({0, 0}, {4, 3})}.empty());
    CHECK_FALSE(Shape{Convex({{0, 0}, {4, 0}, {0, 3}})}.empty());
    CHECK_FALSE(Shape{Chain({{0, 0}, {2, 1}})}.empty());
    CHECK_FALSE(Shape{Polyline({{0, 0}, {2, 1}})}.empty());
    CHECK_FALSE(Shape{Polygon({{0, 0}, {4, 0}, {0, 3}})}.empty());
    CHECK_FALSE(Shape{Region(Polygon({{0, 0}, {4, 0}, {0, 3}}))}.empty());
    CHECK_FALSE(Shape{Regions(Region(Polygon({{0, 0}, {4, 0}, {0, 3}})))}.empty());

    // A default-constructed HalfplaneIntersection is the whole plane; an
    // over-constrained one is empty.
    CHECK_FALSE(Shape{Halfplanes{}}.empty());
    Halfplanes overconstrained;
    overconstrained.insert(Halfplane(0, 1, 1, 1));
    overconstrained.insert(Halfplane(1, 0, 0, 0));
    REQUIRE(overconstrained.empty());
    CHECK(Shape{overconstrained}.empty());

    // An alternative defined by the points it covers is never empty, degenerate
    // or not.
    CHECK_FALSE(Shape{Point(1, 2)}.empty());
    CHECK_FALSE(Shape{Segment({0, 0}, {2, 1})}.empty());
    CHECK_FALSE(Shape{pgl::OrientedSegment<Point>({2, 1}, {0, 0})}.empty());
    CHECK_FALSE(Shape{pgl::Line<Point>({0, 0}, {2, 1})}.empty());
    CHECK_FALSE(Shape{pgl::OrientedLine<Point>({0, 0}, {2, 1})}.empty());
    CHECK_FALSE(Shape{pgl::Ray<Point>({0, 0}, {2, 1})}.empty());
    CHECK_FALSE(Shape{Halfplane(0, 0, 2, 1)}.empty());
    CHECK_FALSE(Shape{Triangle({0, 0}, {4, 0}, {0, 3})}.empty());
    CHECK_FALSE(Shape{Triangle({1, 1}, {1, 1}, {1, 1})}.empty());
    CHECK_FALSE(Shape{pgl::Disk<Point>({0, 0}, {2, 0}, {0, 2})}.empty());

    // Emptiness is a question about the geometry, not about the alternative.
    const Shape emptyRectangle = Rectangle{};
    CHECK(emptyRectangle.empty());
    CHECK(emptyRectangle.isRectangle());
    CHECK_FALSE(emptyRectangle.holdsAlternative<EmptyShape>());
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

TEST_CASE("Shape dispatches bbox across wrapped shapes and throws on unbounded ones") {
    using Point = pgl::Point<int>;
    using EmptyShape = pgl::EmptyShape<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Shape = pgl::Shape<Point>;

    CHECK(Shape(Point(1, 2)).bbox() == Rectangle(1, 2, 1, 2));
    CHECK(Shape(Segment({1, 2}, {3, 5})).bbox() == Rectangle(1, 2, 3, 5));
    CHECK(Shape(Rectangle({0, 0}, {4, 3})).bbox() == Rectangle(0, 0, 4, 3));
    CHECK(Shape(Triangle({0, 0}, {4, 0}, {0, 3})).bbox() == Rectangle(0, 0, 4, 3));
    CHECK(Shape(Convex({{0, 0}, {4, 0}, {4, 4}, {0, 4}})).bbox() == Rectangle(0, 0, 4, 4));

    CHECK_THROWS_AS((void)Shape(EmptyShape{}).bbox(), std::logic_error);
    CHECK_THROWS_AS((void)Shape(Line({0, 0}, {1, 1})).bbox(), std::logic_error);
    CHECK_THROWS_AS((void)Shape(Ray({0, 0}, {1, 1})).bbox(), std::logic_error);
    CHECK_THROWS_AS((void)Shape(Halfplane({0, 0}, {1, 1})).bbox(), std::logic_error);
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
    using Disk = pgl::Disk<Point>;
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
    static_assert(std::is_same_v<decltype(origin.squaredDistance<int>(corner)), int>);

    // Pairs added on the concrete shapes are reachable through the wrapper.
    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    CHECK(t1.squaredDistance<int>(t2) == 64);
    CHECK(t2.squaredDistance<int>(t1) == 64);

    const Shape below = Triangle({0, 10}, {2, 10}, {1, 13});
    const Shape down = Halfplane({0, 0}, {1, 0});  // boundary y = 0
    CHECK(below.squaredDistance<int>(down) == down.squaredDistance<int>(below));

    // Disk pairs compute in a floating result type; an integral request is
    // therefore converted back by the wrapper.
    const Shape disk = Disk(Point(0, 0), 2);
    CHECK(disk.squaredDistance<int>(Point(5, 5)) == 25);
    CHECK(disk.squaredDistance<int>(t2) == t2.squaredDistance<int>(disk));
}

TEST_CASE("Shape dispatches squaredHausdorffDistance across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    CHECK(t1.squaredHausdorffDistance<int>(t2) == static_cast<Triangle>(t1).squaredHausdorffDistance<int>(static_cast<Triangle>(t2)));

    // Defined for Point/Segment/OrientedSegment/Rectangle/Triangle/Convex only:
    // an unbounded Line, or a Polygon (no overload yet), always throws.
    const Shape line = Line({0, 0}, {1, 0});
    const Shape polygon = Polygon({Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)});
    CHECK_THROWS_AS(t1.squaredHausdorffDistance<int>(line), std::logic_error);
    CHECK_THROWS_AS(t1.squaredHausdorffDistance<int>(polygon), std::logic_error);
}

TEST_CASE("Shape dispatches distanceL1/distanceLInf across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape origin = Point(0, 0);
    const Shape corner = Point(3, 4);

    // Shape against Shape, and against a concrete alternative, agree.
    CHECK(origin.distanceL1<int>(corner) == 7);
    CHECK(origin.distanceL1<int>(Point(3, 4)) == 7);
    CHECK(origin.distanceLInf<int>(corner) == 4);
    CHECK(origin.distanceL1<int>(Segment({3, 4}, {3, 10})) == 7);

    // Symmetric through the forwarding visitor (lower rank forwards to higher).
    const Shape segment = Segment({3, 4}, {3, 10});
    CHECK(origin.distanceL1<int>(segment) == segment.distanceL1<int>(origin));

    // A runtime Shape can contain a Disk, so the common default must allow an
    // irrational metric result even when this particular pair is polygonal.
    static_assert(std::is_same_v<decltype(origin.distanceL1(corner)), double>);

    // A purely horizontal gap: L1 and LInf both equal the axis gap.
    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    CHECK(t1.distanceL1<int>(t2) == 8);
    CHECK(t1.distanceLInf<int>(t2) == 8);
    CHECK(t2.distanceL1<int>(t1) == t1.distanceL1<int>(t2));

    // Disk-Point computes in a floating result type; an integral request is
    // therefore converted back by the wrapper.
    const Disk concreteDisk(Point(0, 0), 2);
    const Shape disk = concreteDisk;
    CHECK(disk.distanceL1<int>(Point(5, 5)) == static_cast<int>(concreteDisk.distanceL1(Point(5, 5))));
    CHECK(disk.distanceLInf<int>(Point(5, 5)) == static_cast<int>(concreteDisk.distanceLInf(Point(5, 5))));

    // Disk against anything but a Point is not yet implemented and throws.
    const Shape line = Line({0, 0}, {1, 0});
    CHECK_THROWS_AS(disk.distanceL1<int>(line), std::logic_error);
    CHECK_THROWS_AS(disk.distanceLInf<int>(t1), std::logic_error);
}

TEST_CASE("Shape dispatches hausdorffDistanceL1/hausdorffDistanceLInf across wrapped shapes") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    // Hausdorff to/from a Point uses the same vertex-supremum path as Segment
    // and Triangle against Point; make sure that path (fixed in this pass)
    // actually gets exercised through the wrapper.
    CHECK(t1.hausdorffDistanceL1<int>(Point(20, 0)) == static_cast<Triangle>(t1).hausdorffDistanceL1<int>(Point(20, 0)));
    CHECK(t1.hausdorffDistanceL1<int>(t2) == static_cast<Triangle>(t1).hausdorffDistanceL1<int>(static_cast<Triangle>(t2)));
    CHECK(t1.hausdorffDistanceLInf<int>(t2) == static_cast<Triangle>(t1).hausdorffDistanceLInf<int>(static_cast<Triangle>(t2)));

    // Defined for Point/Segment/OrientedSegment/Rectangle/Triangle/Convex only:
    // an unbounded Line, or a Polygon (no overload yet), always throws.
    const Shape line = Line({0, 0}, {1, 0});
    const Shape polygon = Polygon({Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)});
    CHECK_THROWS_AS(t1.hausdorffDistanceL1<int>(line), std::logic_error);
    CHECK_THROWS_AS(t1.hausdorffDistanceLInf<int>(polygon), std::logic_error);
}

TEST_CASE("Concrete shapes accept a Shape argument for every distance method, symmetrically") {
    // Distance is symmetric, so every concrete shape also accepts a Shape
    // wrapper directly (not just the other way around), re-dispatching
    // through the wrapper's own throw-safe visitor. Point and Disk are the
    // most delicate cases because the Shape overload must not be selected for
    // a concrete argument through an implicit conversion. Both are covered
    // here so a forwarding recursion regression would be caught.
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;
    using Shape = pgl::Shape<Point>;

    const Point origin(0, 0);
    const Point corner(3, 4);
    const Shape shapeCorner = corner;

    CHECK(origin.distanceL1(shapeCorner) == 7);
    CHECK(origin.distanceLInf(shapeCorner) == 4);
    CHECK(origin.distanceL1<long long>(shapeCorner) == 7);
    CHECK(origin.distanceLInf<long long>(shapeCorner) == 4);
    CHECK(origin.distanceL1(shapeCorner) == shapeCorner.distanceL1(origin));

    const Segment segment({0, 0}, {0, 4});
    const Rectangle rectangle({0, 0}, {4, 4});
    const Triangle triangle({0, 0}, {4, 0}, {0, 4});
    const Convex convex(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Polygon polygon({{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Shape farPoint = Point(20, 0);

    CHECK(segment.distanceL1(farPoint) == farPoint.distanceL1(segment));
    CHECK(rectangle.distanceL1(farPoint) == farPoint.distanceL1(rectangle));
    CHECK(triangle.distanceL1(farPoint) == farPoint.distanceL1(triangle));
    CHECK(triangle.hausdorffDistanceL1<int>(farPoint) == farPoint.hausdorffDistanceL1<int>(triangle));
    CHECK(convex.distanceL1(farPoint) == farPoint.distanceL1(convex));
    CHECK(polygon.distanceL1(farPoint) == farPoint.distanceL1(polygon));

    const pgl::Polyline<Point> zigzag({0, 0, 2, 4, 4, 0});
    CHECK(zigzag.distanceL1(farPoint) == farPoint.distanceL1(zigzag));
    CHECK(zigzag.distanceLInf(farPoint) == farPoint.distanceLInf(zigzag));

    // Disk-Point works both ways; Disk against anything else still throws
    // (not yet implemented), whether reached via the Shape wrapper or via the
    // concrete Disk's own new Shape-argument overload.
    const Disk disk(Point(0, 0), 2);
    CHECK(disk.distanceL1(farPoint) == farPoint.distanceL1(disk));
    CHECK(disk.distanceLInf<long double>(farPoint) ==
          farPoint.distanceLInf<long double>(disk));
    CHECK_THROWS_AS((void)disk.distanceL1(Shape(segment)), std::logic_error);
    CHECK_THROWS_AS((void)disk.distanceLInf<long double>(Shape(segment)), std::logic_error);
}

TEST_CASE("Shape translates and scales through the wrapped value") {
    using Point = pgl::Point<int>;
    using EmptyShape = pgl::EmptyShape<Point>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    // Free translation preserves the stored alternative type and shifts it.
    const Shape segment = Segment({1, 2}, {3, 4});
    const Shape shifted = segment + Point(2, 3);
    REQUIRE(shifted.holdsAlternative<Segment>());
    CHECK(*shifted.getIf<Segment>() == Segment({3, 5}, {5, 7}));
    CHECK((Point(2, 3) + segment) == shifted);
    CHECK((shifted - Point(2, 3)) == segment);

    // Scaling and division around the origin.
    const Shape scaled = segment * 2;
    REQUIRE(scaled.holdsAlternative<Segment>());
    CHECK(*scaled.getIf<Segment>() == Segment({2, 4}, {6, 8}));
    CHECK((2 * segment) == scaled);
    CHECK((scaled / 2) == segment);

    // In-place operators mutate the active alternative.
    Shape triangle = Triangle({0, 0}, {4, 0}, {0, 3});
    triangle += Point(1, 1);
    CHECK(*triangle.getIf<Triangle>() == Triangle({1, 1}, {5, 1}, {1, 4}));
    triangle -= Point(1, 1);
    CHECK(*triangle.getIf<Triangle>() == Triangle({0, 0}, {4, 0}, {0, 3}));
    triangle *= 3;
    CHECK(*triangle.getIf<Triangle>() == Triangle({0, 0}, {12, 0}, {0, 9}));
    triangle /= 3;
    CHECK(*triangle.getIf<Triangle>() == Triangle({0, 0}, {4, 0}, {0, 3}));

    // The empty alternative is carried through every transformation unchanged.
    Shape empty;
    REQUIRE(empty.holdsAlternative<EmptyShape>());
    CHECK((empty + Point(5, 6)).holdsAlternative<EmptyShape>());
    CHECK((empty - Point(5, 6)).holdsAlternative<EmptyShape>());
    CHECK((empty * 4).holdsAlternative<EmptyShape>());
    CHECK((empty / 4).holdsAlternative<EmptyShape>());
    empty += Point(5, 6);
    empty *= 4;
    CHECK(empty.empty());
}

TEST_CASE("Shape rotates and axis-scales through the wrapped value") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;
    using Shape = pgl::Shape<Point>;

    const Triangle triangle({0, 0}, {4, 0}, {0, 3});
    const Segment segment({1, 2}, {3, 4});

    // Rotation and axis-scaling agree with the wrapped shape's own methods.
    CHECK(Shape(triangle).rotated90() == Shape(triangle.rotated90()));
    CHECK(Shape(triangle).rotated90(3) == Shape(triangle.rotated90(3)));
    CHECK(Shape(segment).scaledUpX(3) == Shape(segment.scaledUpX(3)));
    CHECK(Shape(segment).scaledUpY(3) == Shape(segment.scaledUpY(3)));
    CHECK(Shape(segment.scaledUpX(6)).scaledDownX(2) == Shape(segment.scaledUpX(3)));

    // In-place variants match the const ones.
    Shape mutable_triangle = triangle;
    mutable_triangle.rotate90();
    CHECK(mutable_triangle == Shape(triangle.rotated90()));

    Shape mutable_segment = segment;
    mutable_segment.scaleUpX(3);
    CHECK(mutable_segment == Shape(segment.scaledUpX(3)));

    // Rotation works for every alternative, including Disk.
    const Disk disk(Point(2, 3), 5);
    CHECK(Shape(disk).rotated90() == Shape(disk.rotated90()));

    // Axis-scaling a disk is structurally impossible (it would be an ellipse).
    CHECK_THROWS_AS((void)Shape(disk).scaledUpX(2), std::logic_error);
    CHECK_THROWS_AS((void)Shape(disk).scaledDownY(2), std::logic_error);
    Shape mutable_disk = disk;
    CHECK_THROWS_AS(mutable_disk.scaleUpX(2), std::logic_error);

    // The empty alternative is invariant under all named transforms.
    Shape empty;
    CHECK(empty.rotated90().empty());
    CHECK(empty.scaledUpX(4).empty());
    CHECK(empty.scaledDownY(4).empty());
    empty.rotate90();
    empty.scaleUpX(4);
    CHECK(empty.empty());
}

TEST_CASE("EmptyShape is invariant under every transformation") {
    using Point = pgl::Point<int>;
    using EmptyShape = pgl::EmptyShape<Point>;

    EmptyShape empty;
    CHECK((empty + Point(3, 4)) == EmptyShape{});
    CHECK((Point(3, 4) + empty) == EmptyShape{});
    CHECK((empty - Point(3, 4)) == EmptyShape{});
    CHECK((empty * 5) == EmptyShape{});
    CHECK((5 * empty) == EmptyShape{});
    CHECK((empty / 5) == EmptyShape{});

    CHECK((empty += Point(3, 4)) == EmptyShape{});
    CHECK((empty -= Point(3, 4)) == EmptyShape{});
    CHECK((empty *= 5) == EmptyShape{});
    CHECK((empty /= 5) == EmptyShape{});

    CHECK(empty.rotated90(3) == EmptyShape{});
    CHECK(empty.scaledUpX(5) == EmptyShape{});
    CHECK(empty.scaledUpY(5) == EmptyShape{});
    CHECK(empty.scaledDownX(5) == EmptyShape{});
    CHECK(empty.scaledDownY(5) == EmptyShape{});

    empty.rotate90();
    empty.scaleUpX(5);
    empty.scaleUpY(5);
    empty.scaleDownX(5);
    empty.scaleDownY(5);
    CHECK(empty == EmptyShape{});
}

TEST_CASE("Shape wraps a MonotoneChain") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Shape = pgl::Shape<Point>;

    const Chain zig({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
    const Shape shape = zig;
    REQUIRE(shape.holdsAlternative<Chain>());
    CHECK(*shape.getIf<Chain>() == zig);
    CHECK(shape.size() == 5);
    CHECK(shape[1] == Point(2, 4));
    CHECK(shape.get(-1) == Point(6, 0));
    CHECK(shape.get(5) == Point(0, 0));
    CHECK(shape.index(Point(4, 0)) == 2);
    CHECK_FALSE(shape.isDegenerate());
    CHECK(shape.bbox() == pgl::Rectangle<Point>({0, 0}, {6, 4}));

    std::ostringstream stream;
    stream << shape;
    CHECK(stream.str() == "MonotoneChain[(0,0),(2,4),(4,0),(4,4),(6,0)]");

    std::unordered_set<Shape> shapes;
    shapes.insert(shape);
    shapes.insert(Shape(zig));
    CHECK(shapes.size() == 1);
}

TEST_CASE("Shape dispatches predicates and measures through a MonotoneChain") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Segment = pgl::Segment<Point>;
    using Shape = pgl::Shape<Point>;

    const Shape shape = Chain({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
    const Shape crossing = Segment({1, 0}, {1, 4});
    const Shape miss = Segment({0, 5}, {1, 6});

    CHECK(shape.intersects(crossing));
    CHECK(crossing.intersects(shape));
    CHECK_FALSE(shape.intersects(miss));
    CHECK(shape.contains(Shape(Point(1, 2))));
    CHECK(shape.interiorsIntersect(crossing));
    CHECK(shape.crosses(crossing));
    CHECK(crossing.crosses(shape));
    CHECK(shape.separates(crossing));

    // The nearest pair is the peak (2,4) against the segment's side.
    CHECK(shape.squaredDistance<double>(miss) == doctest::Approx(4.5));
    CHECK(shape.distanceL1<pgl::Rational<int>>(Shape(Point(1, 3))) == pgl::Rational<int>(1, 2));
    CHECK_THROWS_AS((void)shape.squaredHausdorffDistance<double>(crossing), std::logic_error);

    // A single-point crossing re-wraps into a Point-valued Shape. (The right
    // operand stays concrete: a Shape-Shape intersection instantiates every
    // pair, and the preexisting Rectangle-Polygon result cannot be wrapped.)
    const Shape up = Chain({0, 0, 4, 4});
    const Shape cross = up.intersection<int>(Chain({0, 4, 4, 0}));
    REQUIRE(cross.holdsAlternative<Point>());
    CHECK(Point(cross) == Point(2, 2));

    // Transformations preserve the alternative.
    Shape moved = shape;
    moved += Point(1, 1);
    REQUIRE(moved.holdsAlternative<Chain>());
    CHECK(moved[0] == Point(1, 1));
    CHECK(moved.rotated90(2).holdsAlternative<Chain>());
    CHECK(moved.scaledUpX(2).holdsAlternative<Chain>());
}

TEST_CASE("Shape wraps a Polyline") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    using Shape = pgl::Shape<Point>;

    // A self-intersecting bow-tie: the traversal order is preserved, unlike a
    // MonotoneChain, which would sort these vertices.
    const Polyline bowtie({0, 0, 4, 4, 4, 0, 0, 4});
    const Shape shape = bowtie;
    REQUIRE(shape.holdsAlternative<Polyline>());
    CHECK(*shape.getIf<Polyline>() == bowtie);
    CHECK(shape.size() == 4);
    CHECK(shape[1] == Point(4, 4));
    CHECK(shape.get(-1) == Point(0, 4));
    CHECK(shape.index(Point(4, 0)) == 2);
    CHECK_FALSE(shape.isDegenerate());
    CHECK(shape.bbox() == pgl::Rectangle<Point>({0, 0}, {4, 4}));

    std::ostringstream stream;
    stream << shape;
    CHECK(stream.str() == "Polyline[(0,0),(4,4),(4,0),(0,4)]");

    std::unordered_set<Shape> shapes;
    shapes.insert(shape);
    shapes.insert(Shape(bowtie));
    // The reversal-canonical twin compares and hashes equal.
    shapes.insert(Shape(Polyline({0, 4, 4, 0, 4, 4, 0, 0})));
    CHECK(shapes.size() == 1);
}

TEST_CASE("Shape wraps a HalfplaneIntersection") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Region = pgl::HalfplaneIntersection<Point>;
    using Shape = pgl::Shape<Point>;

    const Region square{Rectangle({0, 0}, {6, 6})};
    const Shape shape = square;
    REQUIRE(shape.holdsAlternative<Region>());
    CHECK(shape.getIf<Region>() != nullptr);
    CHECK(*shape.getIf<Region>() == square);
    CHECK(shape.size() == 4);  // stored half-planes
    CHECK_FALSE(shape.isDegenerate());
    CHECK(shape.bbox() == Rectangle({0, 0}, {6, 6}));

    // The region's elements are half-planes, not points, so the point-valued
    // element accessors are not defined for this alternative.
    CHECK_THROWS_AS((void)shape.get(0), std::logic_error);
    CHECK_THROWS_AS((void)shape[0], std::logic_error);
    CHECK_THROWS_AS((void)shape.index(Point(0, 0)), std::logic_error);

    // An unbounded region has no bbox; its own bbox() throws through the wrapper.
    const Region upper{Halfplane({0, 0}, {1, 0})};
    CHECK_THROWS_AS((void)Shape(upper).bbox(), std::logic_error);

    std::ostringstream stream;
    stream << shape;
    CHECK(stream.str() ==
          "HalfplaneIntersection[^-(0,0)--(6,0)-^,^-(6,0)--(6,6)-^,^-(6,6)--(0,6)-^,^-(0,6)--(0,0)-^]");

    std::unordered_set<Shape> shapes;
    shapes.insert(shape);
    shapes.insert(Shape(Region{Rectangle({0, 0}, {6, 6})}));
    shapes.insert(Shape(upper));
    CHECK(shapes.size() == 2);

    std::set<Shape> ordered;
    ordered.insert(shape);
    ordered.insert(Shape(upper));
    CHECK(ordered.size() == 2);
}

TEST_CASE("Shape dispatches predicates, intersection, and distances through a HalfplaneIntersection") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::HalfplaneIntersection<Point>;
    using Shape = pgl::Shape<Point>;

    const Region square{Rectangle({0, 0}, {6, 6})};
    const Shape shape = square;
    const Shape crossing = Segment({-1, 3}, {7, 3});
    const Shape miss = Segment({8, 0}, {9, 4});

    CHECK(shape.contains(Shape(Point(2, 2))));
    CHECK_FALSE(shape.contains(Shape(Point(7, 7))));
    CHECK(shape.intersects(crossing));
    CHECK(crossing.intersects(shape));
    CHECK_FALSE(shape.intersects(miss));
    CHECK(shape.boundaryContains(Shape(Point(0, 3))));
    CHECK(shape.interiorContains(Shape(Point(3, 3))));
    CHECK(shape.interiorsIntersect(crossing));
    CHECK(shape.separates(crossing));
    CHECK(shape.crosses(crossing));
    CHECK(crossing.crosses(shape));

    // The concrete region accepts the wrapper directly on every predicate.
    CHECK(square.contains(Shape(Point(2, 2))));
    CHECK(square.intersects(crossing));
    CHECK_FALSE(square.boundaryContains(crossing));
    CHECK_FALSE(square.interiorContains(miss));
    CHECK(square.interiorsIntersect(crossing));
    CHECK(square.separates(crossing));
    CHECK(square.crosses(crossing));

    // The self pair dispatches through the wrapper too.
    const Shape plane = Region();
    const Shape strip = Region({Halfplane({0, 0}, {1, 0}), Halfplane({1, 1}, {0, 1})});
    CHECK(plane.contains(strip));
    CHECK_FALSE(strip.contains(plane));
    CHECK(strip.intersects(shape));
    CHECK(strip.separates(plane));
    CHECK_FALSE(plane.separates(strip));

    // Two half-planes now intersect into a wrapped HalfplaneIntersection,
    // which previously had no Shape representation and threw.
    const Shape upper = Halfplane({0, 0}, {1, 0});
    const Shape right = Halfplane({0, 1}, {0, 0});
    const Shape wedge = upper.intersection<int>(right);
    REQUIRE(wedge.holdsAlternative<Region>());
    CHECK_FALSE(Region(wedge).isBounded());
    CHECK(wedge.contains(Shape(Point(3, 3))));

    // Region-with-area intersections stay regions through the wrapper; the
    // clip of the square with a half-plane keeps half the square.
    const Shape clipped = shape.intersection<int>(upper);
    REQUIRE(clipped.holdsAlternative<Region>());
    CHECK(Region(clipped) == square);
    const Shape cell = strip.intersection<int>(shape);
    REQUIRE(cell.holdsAlternative<Region>());
    CHECK(Region(cell).twiceArea<int>() == 12);  // 6 x 1

    // One-dimensional results re-wrap as their own alternatives.
    const Shape chord = shape.intersection<int>(Shape(Segment({-1, 3}, {7, 3})));
    REQUIRE(chord.holdsAlternative<Segment>());
    CHECK(Segment(chord) == Segment({0, 3}, {6, 3}));

    // A polygon against the region comes back as its single component.
    const Shape corner = shape.intersection<int>(Shape(Polygon({0, 0, 4, 0, 4, 4, 0, 4})));
    REQUIRE(corner.holdsAlternative<Polygon>());
    CHECK(Polygon(corner) == Polygon({0, 0, 4, 0, 4, 4, 0, 4}));
    CHECK(Shape(Polygon({0, 0, 4, 0, 4, 4, 0, 4})).intersection<int>(shape) == corner);

    // A disconnected one has no single Shape to be, and a Disk has no
    // intersection with the region at all.
    const Shape uShaped = Polygon({0, 0, 6, 0, 6, 6, 4, 6, 4, 2, 2, 2, 2, 6, 0, 6});
    const Shape slab = Region({Halfplane({0, 3}, {1, 3}), Halfplane({1, 5}, {0, 5})});
    CHECK_THROWS_AS((void)slab.intersection<int>(uShaped), std::logic_error);
    CHECK_THROWS_AS((void)shape.intersection<int>(Shape(Disk(Point(3, 3), 1))), std::logic_error);

    // Distances dispatch both ways, with the explicit ResultNumber probe.
    const Shape farPoint = Point(20, 3);
    CHECK(shape.squaredDistance<int>(farPoint) == 196);
    CHECK(farPoint.squaredDistance<int>(shape) == 196);
    CHECK(shape.distanceL1<int>(farPoint) == 14);
    CHECK(shape.distanceLInf<int>(farPoint) == 14);
    CHECK(square.distanceL1<int>(farPoint) == 14);   // concrete region, Shape argument
    CHECK(square.distanceLInf<int>(farPoint) == 14);
    CHECK(shape.squaredDistance<int>(strip) == 0);
    CHECK_THROWS_AS((void)shape.squaredHausdorffDistance<int>(farPoint), std::logic_error);
    CHECK_THROWS_AS((void)shape.distanceL1<int>(Shape(Disk(Point(20, 3), 1))), std::logic_error);

    // Transformations preserve the alternative.
    Shape moved = shape;
    moved += Point(1, 1);
    REQUIRE(moved.holdsAlternative<Region>());
    CHECK(Region(moved) == Region{Rectangle({1, 1}, {7, 7})});
    CHECK((shape + Point(1, 1)) == moved);
    CHECK((moved - Point(1, 1)) == shape);
    CHECK((shape * 2).holdsAlternative<Region>());
    CHECK(Region(shape * 2) == Region{Rectangle({0, 0}, {12, 12})});
    CHECK(((shape * 2) / 2) == shape);
    CHECK(shape.rotated90(2).holdsAlternative<Region>());
    CHECK(Region(shape.rotated90(2)) == Region{Rectangle({-6, -6}, {0, 0})});
    CHECK(shape.scaledUpX(2).holdsAlternative<Region>());
    CHECK(Region(shape.scaledUpX(2)) == Region{Rectangle({0, 0}, {12, 6})});
}

TEST_CASE("Shape dispatches predicates and measures through a Polyline") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    using Segment = pgl::Segment<Point>;
    using Shape = pgl::Shape<Point>;

    const Polyline zig({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
    const Shape shape = zig;
    const Shape crossing = Segment({1, 0}, {1, 4});
    const Shape miss = Segment({0, 5}, {1, 6});

    CHECK(shape.intersects(crossing));
    CHECK(crossing.intersects(shape));
    CHECK_FALSE(shape.intersects(miss));
    CHECK(shape.contains(Shape(Point(1, 2))));
    CHECK(shape.interiorsIntersect(crossing));
    CHECK(shape.crosses(crossing));
    CHECK(crossing.crosses(shape));
    CHECK(shape.separates(crossing));

    // The concrete polyline accepts the wrapper directly on every predicate.
    CHECK(zig.intersects(crossing));
    CHECK(zig.contains(Shape(Point(1, 2))));
    CHECK_FALSE(zig.boundaryContains(crossing));
    CHECK_FALSE(zig.interiorContains(miss));
    CHECK(zig.interiorsIntersect(crossing));
    CHECK(zig.separates(crossing));
    CHECK(zig.crosses(crossing));

    // The nearest pair is the peak (2,4) against the segment's side.
    CHECK(shape.squaredDistance<double>(miss) == doctest::Approx(4.5));
    CHECK(shape.distanceL1<pgl::Rational<int>>(Shape(Point(1, 3))) == pgl::Rational<int>(1, 2));
    CHECK(zig.distanceL1<pgl::Rational<int>>(Shape(Point(1, 3))) == pgl::Rational<int>(1, 2));
    CHECK_THROWS_AS((void)shape.squaredHausdorffDistance<double>(crossing), std::logic_error);

    // A single-point crossing re-wraps into a Point-valued Shape. (The right
    // operand stays concrete, as in the MonotoneChain case above.)
    const Shape up = Polyline({0, 0, 4, 4});
    const Shape cross = up.intersection<int>(Polyline({0, 4, 4, 0}));
    REQUIRE(cross.holdsAlternative<Point>());
    CHECK(Point(cross) == Point(2, 2));

    // Transformations preserve the alternative.
    Shape moved = shape;
    moved += Point(1, 1);
    REQUIRE(moved.holdsAlternative<Polyline>());
    CHECK(moved[0] == Point(1, 1));
    CHECK(moved.rotated90(2).holdsAlternative<Polyline>());
    CHECK(moved.scaledUpX(2).holdsAlternative<Polyline>());
}

TEST_CASE("Shape wraps a PolygonWithHoles") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using Shape = pgl::Shape<Point>;

    const Region annulus(Polygon({0, 0, 6, 0, 6, 6, 0, 6}),
                         std::vector<Polygon>{Polygon({2, 2, 4, 2, 4, 4, 2, 4})});
    const Shape shape = annulus;
    REQUIRE(shape.holdsAlternative<Region>());
    CHECK(shape.getIf<Region>() != nullptr);
    CHECK(*shape.getIf<Region>() == annulus);
    CHECK_FALSE(shape.isDegenerate());
    CHECK(shape.bbox() == Rectangle({0, 0}, {6, 6}));

    // The region's vertices are spread over its rings rather than forming one
    // indexable sequence, so the element accessors are not defined for it.
    CHECK_THROWS_AS((void)shape.size(), std::logic_error);
    CHECK_THROWS_AS((void)shape.get(0), std::logic_error);
    CHECK_THROWS_AS((void)shape[0], std::logic_error);
    CHECK_THROWS_AS((void)shape.index(Point(0, 0)), std::logic_error);

    // A region collapsed onto a segment reports itself degenerate.
    CHECK(Shape(Region(Polygon({0, 0, 4, 0}))).isDegenerate());

    std::ostringstream stream;
    stream << shape;
    CHECK(stream.str() ==
          "PolygonWithHoles[Polygon[(0,0),(6,0),(6,6),(0,6)],Polygon[(2,2),(4,2),(4,4),(2,4)]]");

    // The hole is part of the value, so filling it in gives a different shape.
    const Shape filled = Region(Polygon({0, 0, 6, 0, 6, 6, 0, 6}));
    std::unordered_set<Shape> shapes;
    shapes.insert(shape);
    shapes.insert(Shape(Region(Polygon({0, 0, 6, 0, 6, 6, 0, 6}),
                               std::vector<Polygon>{Polygon({2, 2, 4, 2, 4, 4, 2, 4})})));
    shapes.insert(filled);
    CHECK(shapes.size() == 2);

    std::set<Shape> ordered;
    ordered.insert(shape);
    ordered.insert(filled);
    CHECK(ordered.size() == 2);

    // A hole-free region and the polygon it is built from are different
    // alternatives, so they are different shapes.
    CHECK_FALSE(filled == Shape(Polygon({0, 0, 6, 0, 6, 6, 0, 6})));
}

TEST_CASE("Shape dispatches predicates, intersection, and distances through a PolygonWithHoles") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using Shape = pgl::Shape<Point>;

    const Region annulus(Polygon({0, 0, 6, 0, 6, 6, 0, 6}),
                         std::vector<Polygon>{Polygon({2, 2, 4, 2, 4, 4, 2, 4})});
    const Shape shape = annulus;
    const Shape crossing = Segment({-1, 1}, {7, 1});
    const Shape miss = Segment({8, 0}, {9, 4});
    const Shape inHole = Point(3, 3);

    CHECK(shape.contains(Shape(Point(1, 1))));
    CHECK_FALSE(shape.contains(inHole));           // the hole is not material
    CHECK(shape.contains(Shape(Point(2, 2))));     // but its rim is
    CHECK_FALSE(shape.interiorContains(Shape(Point(2, 2))));
    CHECK(shape.boundaryContains(Shape(Point(2, 2))));
    CHECK(shape.intersects(crossing));
    CHECK(crossing.intersects(shape));
    CHECK_FALSE(shape.intersects(miss));
    CHECK(shape.interiorsIntersect(crossing));
    CHECK(shape.separates(crossing));
    CHECK(crossing.separates(shape));
    CHECK(shape.crosses(crossing));
    CHECK(crossing.crosses(shape));

    // The concrete region accepts the wrapper directly on every predicate.
    CHECK(annulus.contains(Shape(Point(1, 1))));
    CHECK_FALSE(annulus.contains(inHole));
    CHECK_FALSE(annulus.interiorContains(Shape(Point(2, 2))));
    CHECK(annulus.boundaryContains(Shape(Point(2, 2))));
    CHECK(annulus.intersects(crossing));
    CHECK(annulus.interiorsIntersect(crossing));
    CHECK(annulus.separates(crossing));
    CHECK(annulus.crosses(crossing));

    // The self pair dispatches through the wrapper too.
    const Shape shifted = annulus + Point(3, 0);
    CHECK(shape.intersects(shifted));
    CHECK(shape.interiorsIntersect(shifted));
    CHECK_FALSE(shape.contains(shifted));

    // A lower-ranked shape swallowing the region answers from the outer ring
    // alone, and the region is contained in the box that covers it.
    const Shape box = Rectangle({-1, -1}, {7, 7});
    CHECK(box.contains(shape));
    CHECK(box.interiorContains(shape));
    CHECK_FALSE(shape.contains(box));

    // Intersecting with an area operand yields a region again, holes and all —
    // the one intersection no vector<Polygon> could express.
    const Shape clipped = shape.intersection<int>(box);
    REQUIRE(clipped.holdsAlternative<Region>());
    CHECK(Region(clipped) == annulus);
    const Shape half = shape.intersection<int>(Shape(Rectangle({0, 0}, {6, 3})));
    REQUIRE(half.holdsAlternative<Region>());
    CHECK(Region(half).twiceArea() == 2 * (18 - 2));  // the lower half, minus half the hole

    // The rank forwarders answer the same pair the other way round. A Polygon
    // on the left forwards too, even though its own Polygon-valued
    // intersection would otherwise have answered a lower-ranked operand.
    CHECK(Shape(Rectangle({0, 0}, {6, 3})).intersection<int>(shape) == half);
    CHECK(Shape(pgl::Convex<Point>(std::vector<Point>{{0, 0}, {6, 0}, {6, 3}, {0, 3}}))
              .intersection<int>(shape) == half);
    CHECK(Shape(Polygon({0, 0, 6, 0, 6, 3, 0, 3})).intersection<int>(shape) == half);

    // An intersection that comes apart is no longer without an answer: the
    // strip across the annulus meets it left and right of the hole, and the
    // two pieces come back together as the PolygonSet alternative.
    using RegionSet = pgl::PolygonSet<Point>;
    const Shape split = shape.intersection<int>(Shape(Rectangle({-1, 2}, {7, 4})));
    REQUIRE(split.holdsAlternative<RegionSet>());
    CHECK(RegionSet(split).componentCount() == 2);
    CHECK(RegionSet(split).twiceArea() == 2 * 8);
    // A pair with no overload at all, such as a Disk, still throws.
    CHECK_THROWS_AS((void)shape.intersection<int>(Shape(Disk(Point(3, 3), 1))), std::logic_error);

    // Distances dispatch both ways, with the explicit ResultNumber probe.
    const Shape farPoint = Point(20, 3);
    CHECK(shape.squaredDistance<int>(farPoint) == 196);
    CHECK(farPoint.squaredDistance<int>(shape) == 196);
    CHECK(shape.distanceL1<int>(farPoint) == 14);
    CHECK(shape.distanceLInf<int>(farPoint) == 14);
    CHECK(annulus.distanceL1<int>(farPoint) == 14);   // concrete region, Shape argument
    CHECK(annulus.distanceLInf<int>(farPoint) == 14);
    CHECK(shape.squaredDistance<int>(Shape(Triangle({20, 0}, {24, 0}, {20, 4}))) == 196);
    CHECK_THROWS_AS((void)shape.squaredHausdorffDistance<int>(farPoint), std::logic_error);

    // Transformations preserve the alternative.
    Shape moved = shape;
    moved += Point(1, 1);
    REQUIRE(moved.holdsAlternative<Region>());
    CHECK(Region(moved) == annulus + Point(1, 1));
    CHECK((shape + Point(1, 1)) == moved);
    CHECK((moved - Point(1, 1)) == shape);
    CHECK((shape * 2).holdsAlternative<Region>());
    CHECK(Region(shape * 2) == annulus * 2);
    CHECK(((shape * 2) / 2) == shape);
    CHECK(shape.rotated90(2).holdsAlternative<Region>());
    CHECK(Region(shape.rotated90(2)) == annulus.rotated90(2));
    CHECK(shape.scaledUpX(2).holdsAlternative<Region>());
    CHECK(Region(shape.scaledUpX(2)) ==
          Region(Polygon({0, 0, 12, 0, 12, 6, 0, 6}),
                 std::vector<Polygon>{Polygon({4, 2, 8, 2, 8, 4, 4, 4})}));
    CHECK(Region(shape.scaledUpX(2).scaledDownX(2)) == annulus);

    // An affine map takes the region through the Shape wrapper as well; the
    // shear keeps every area, so the region keeps its hole.
    const pgl::Transformation<int> shear(1, 0, 1, 1, 0, 0);
    const Shape sheared = shear * shape;
    REQUIRE(sheared.holdsAlternative<Region>());
    CHECK(Region(sheared).holeCount() == 1);
    CHECK(Region(sheared).twiceArea() == annulus.twiceArea());

    // Summing with a Point is a translation and stays a region.
    const Shape summed = shape.minkowskiSum(Shape(Point(1, 1)));
    REQUIRE(summed.holdsAlternative<Region>());
    CHECK(Region(summed) == annulus + Point(1, 1));
    // A non-convex operand has no single-Shape sum; that is what the
    // PolygonWithHoles-valued minkowskiSum overloads are for.
    CHECK_THROWS_AS((void)shape.minkowskiSum(Shape(Triangle({0, 0}, {1, 0}, {0, 1}))),
                    std::logic_error);
}

TEST_CASE("Shape wraps a PolygonSet") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using RegionSet = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    // Two squares that never touch: the one shape in the variant whose point
    // set need not be connected.
    const RegionSet pair(std::vector<Region>{Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2})),
                                             Region(Polygon({4, 0, 6, 0, 6, 2, 4, 2}))});
    const Shape shape = pair;
    REQUIRE(shape.holdsAlternative<RegionSet>());
    CHECK(shape.getIf<RegionSet>() != nullptr);
    CHECK(*shape.getIf<RegionSet>() == pair);
    CHECK_FALSE(shape.isDegenerate());
    CHECK(shape.bbox() == Rectangle({0, 0}, {6, 2}));

    // The vertices are spread over the components rather than forming one
    // indexable sequence, so the element accessors are not defined for it.
    CHECK_THROWS_AS((void)shape.size(), std::logic_error);
    CHECK_THROWS_AS((void)shape.get(0), std::logic_error);
    CHECK_THROWS_AS((void)shape[0], std::logic_error);
    CHECK_THROWS_AS((void)shape.index(Point(0, 0)), std::logic_error);

    // An empty set covers nothing, and a set of collapsed components drops them.
    CHECK(Shape(RegionSet()).isDegenerate());
    CHECK(Shape(RegionSet(Region(Polygon({0, 0, 4, 0})))).isDegenerate());

    std::ostringstream stream;
    stream << shape;
    CHECK(stream.str() ==
          "PolygonSet[PolygonWithHoles[Polygon[(0,0),(2,0),(2,2),(0,2)]],"
          "PolygonWithHoles[Polygon[(4,0),(6,0),(6,2),(4,2)]]]");

    // A one-component set and the region it holds are different alternatives,
    // so they are different shapes.
    const Shape single = RegionSet(Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2})));
    CHECK_FALSE(single == Shape(Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2}))));

    std::unordered_set<Shape> shapes;
    shapes.insert(shape);
    shapes.insert(Shape(RegionSet(std::vector<Region>{Region(Polygon({4, 0, 6, 0, 6, 2, 4, 2})),
                                                      Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2}))})));
    shapes.insert(single);
    CHECK(shapes.size() == 2);  // the components are stored in canonical order

    std::set<Shape> ordered;
    ordered.insert(shape);
    ordered.insert(single);
    CHECK(ordered.size() == 2);
}

TEST_CASE("Shape dispatches predicates, intersection, and distances through a PolygonSet") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using RegionSet = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    const RegionSet pair(std::vector<Region>{Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2})),
                                             Region(Polygon({4, 0, 6, 0, 6, 2, 4, 2}))});
    const Shape shape = pair;
    const Shape crossing = Segment({-1, 1}, {7, 1});
    const Shape gap = Point(3, 1);

    CHECK(shape.contains(Shape(Point(1, 1))));
    CHECK_FALSE(shape.contains(gap));  // the space between components is not material
    CHECK(shape.boundaryContains(Shape(Point(0, 1))));
    CHECK_FALSE(shape.interiorContains(Shape(Point(0, 1))));
    CHECK(shape.intersects(crossing));
    CHECK(crossing.intersects(shape));
    CHECK_FALSE(shape.intersects(Shape(Segment({0, 5}, {6, 5}))));
    CHECK(shape.interiorsIntersect(crossing));
    CHECK(shape.separates(crossing));
    CHECK(crossing.separates(shape));
    CHECK(shape.crosses(crossing));

    // The concrete set accepts the wrapper directly on every predicate.
    CHECK(pair.contains(Shape(Point(1, 1))));
    CHECK_FALSE(pair.contains(gap));
    CHECK(pair.boundaryContains(Shape(Point(0, 1))));
    CHECK(pair.intersects(crossing));
    CHECK(pair.interiorsIntersect(crossing));
    CHECK(pair.separates(crossing));
    CHECK(pair.crosses(crossing));

    // The self pair dispatches through the wrapper too.
    const Shape shifted = pair + Point(1, 0);
    CHECK(shape.intersects(shifted));
    CHECK(shape.interiorsIntersect(shifted));
    CHECK_FALSE(shape.contains(shifted));
    CHECK(shape.contains(Shape(pair)));

    // Intersecting with an area operand keeps the whole set when it stays in
    // several pieces, and unwraps to the tighter region alternative when the
    // answer is a single one.
    const Shape band = shape.intersection<int>(Shape(Rectangle({-1, 0}, {7, 1})));
    REQUIRE(band.holdsAlternative<RegionSet>());
    CHECK(RegionSet(band).componentCount() == 2);
    CHECK(RegionSet(band).twiceArea() == 2 * 4);
    const Shape one = shape.intersection<int>(Shape(Rectangle({0, 0}, {2, 2})));
    REQUIRE(one.holdsAlternative<Region>());
    CHECK(Region(one) == Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2})));
    // The self pair, and a region operand, answer through the wrapper too.
    CHECK(shape.intersection<int>(Shape(pair)) == shape);
    CHECK(shape.intersection<int>(Shape(Region(Polygon({0, 0, 2, 0, 2, 2, 0, 2})))) == one);
    // A pair with no intersection overload at all still throws, and so does one
    // against a one-dimensional operand, which a set has no overload for.
    CHECK_THROWS_AS((void)shape.intersection<int>(Shape(Disk(Point(3, 1), 1))), std::logic_error);
    CHECK_THROWS_AS((void)shape.intersection<int>(crossing), std::logic_error);

    // Distances are the minimum over the components, and dispatch both ways.
    const Shape farPoint = Point(10, 1);
    CHECK(shape.squaredDistance<int>(farPoint) == 16);
    CHECK(farPoint.squaredDistance<int>(shape) == 16);
    CHECK(shape.distanceL1<int>(farPoint) == 4);
    CHECK(shape.distanceLInf<int>(farPoint) == 4);
    CHECK(pair.distanceL1<int>(farPoint) == 4);  // concrete set, Shape argument
    CHECK(pair.distanceLInf<int>(farPoint) == 4);
    CHECK(shape.squaredDistance<int>(Shape(pair + Point(20, 0))) == 196);
    // An L1 or L-infinity distance to a Disk is nowhere defined in the library,
    // and neither is any Hausdorff distance to a set.
    CHECK_THROWS_AS((void)shape.distanceL1<int>(Shape(Disk(Point(20, 1), 1))), std::logic_error);
    CHECK_THROWS_AS((void)shape.squaredHausdorffDistance<int>(farPoint), std::logic_error);

    // Transformations preserve the alternative.
    Shape moved = shape;
    moved += Point(1, 1);
    REQUIRE(moved.holdsAlternative<RegionSet>());
    CHECK(RegionSet(moved) == pair + Point(1, 1));
    CHECK((shape + Point(1, 1)) == moved);
    CHECK((moved - Point(1, 1)) == shape);
    CHECK((shape * 2).holdsAlternative<RegionSet>());
    CHECK(RegionSet(shape * 2) == pair * 2);
    CHECK(((shape * 2) / 2) == shape);
    CHECK(shape.rotated90(2).holdsAlternative<RegionSet>());
    CHECK(RegionSet(shape.rotated90(2)) == pair.rotated90(2));
    CHECK(shape.scaledUpX(2).holdsAlternative<RegionSet>());
    CHECK(RegionSet(shape.scaledUpX(2).scaledDownX(2)) == pair);

    // An affine map takes the set through the wrapper as well; the shear keeps
    // every area, so the components stay two and keep their size.
    const pgl::Transformation<int> shear(1, 0, 1, 1, 0, 0);
    const Shape sheared = shear * shape;
    REQUIRE(sheared.holdsAlternative<RegionSet>());
    CHECK(RegionSet(sheared).componentCount() == 2);
    CHECK(RegionSet(sheared).twiceArea() == pair.twiceArea());

    // Summing with a Point is a translation and stays a set; nothing else is a
    // single-Shape sum, a set of regions being anything but convex.
    const Shape summed = shape.minkowskiSum(Shape(Point(1, 1)));
    REQUIRE(summed.holdsAlternative<RegionSet>());
    CHECK(RegionSet(summed) == pair + Point(1, 1));
    CHECK_THROWS_AS((void)shape.minkowskiSum(Shape(Triangle({0, 0}, {1, 0}, {0, 1}))),
                    std::logic_error);
}

TEST_CASE("Shape dispatches unionWith and throws for the pairs that have no set-valued union") {
    using Point = pgl::Point<int>;
    using PolygonShape = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using RegionSet = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rect({0, 0}, {4, 4});
    const Rectangle offset({2, 2}, {6, 6});
    const Triangle roof({0, 4}, {4, 4}, {2, 6});
    const Convex square(std::vector<Point>{{0, 0}, {3, 0}, {3, 3}, {0, 3}});
    const PolygonShape big({0, 0, 8, 0, 8, 8, 0, 8});
    const Region holed(big, std::vector{PolygonShape({2, 2, 6, 2, 6, 6, 2, 6})});
    const RegionSet set(holed);

    SUBCASE("the answer is a PolygonSet, not a re-wrapped Shape") {
        const auto result = Shape(rect).unionWith<int>(Shape(offset));
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        CHECK(result.twiceArea() == 2 * (16 + 16 - 4));
        // and it is the same answer the concrete call gives
        CHECK(result == rect.unionWith<int>(offset));
    }

    SUBCASE("a bare alternative is accepted on either side") {
        CHECK(Shape(roof).unionWith<int>(rect) == roof.unionWith<int>(rect));
        CHECK(Shape(rect).unionWith<int>(roof) == Shape(roof).unionWith<int>(rect));
    }

    SUBCASE("the default result type follows the wrapper's coordinate type") {
        const auto exact = Shape(rect).unionWith(Shape(offset));
        CHECK(std::is_same_v<decltype(exact),
                             const pgl::PolygonSet<pgl::Point<pgl::division_result_t<int>>>>);
        CHECK(exact.twiceArea() == 2 * (16 + 16 - 4));
    }

    SUBCASE("every ordered pair of the six region alternatives answers") {
        const std::vector<Shape> regions{Shape(rect), Shape(roof), Shape(square),
                                         Shape(big),  Shape(holed), Shape(set)};
        for (const Shape& a : regions) {
            for (const Shape& b : regions) {
                CHECK_NOTHROW((void)a.unionWith<int>(b));
                // A union covers at least as much as either operand alone.
                CHECK(a.unionWith<int>(b).twiceArea() >= a.unionWith<int>(a).twiceArea());
                // and does not depend on which side the wrapper visits first.
                CHECK(a.unionWith<int>(b) == b.unionWith<int>(a));
            }
        }
    }

    SUBCASE("every other alternative throws, whatever it is paired with") {
        const std::vector<Shape> regions{Shape(rect), Shape(big), Shape(set)};
        const std::vector<Shape> others{
            Shape(pgl::EmptyShape<Point>{}),
            Shape(Point(1, 1)),
            Shape(pgl::Segment<Point>({0, 0}, {1, 1})),
            Shape(pgl::OrientedSegment<Point>({0, 0}, {1, 1})),
            Shape(pgl::Line<Point>({0, 0}, {1, 1})),
            Shape(pgl::OrientedLine<Point>({0, 0}, {1, 1})),
            Shape(pgl::Ray<Point>({0, 0}, {1, 1})),
            Shape(pgl::Halfplane<Point>({0, 0}, {1, 0})),
            Shape(pgl::Disk<Point>({0, 0}, 2)),
            Shape(pgl::MonotoneChain<Point>(std::vector<Point>{{0, 0}, {1, 1}})),
            Shape(pgl::Polyline<Point>({0, 0, 1, 1})),
            Shape(pgl::HalfplaneIntersection<Point>(rect))};

        for (const Shape& a : others) {
            for (const Shape& b : regions) {
                CHECK_THROWS_AS((void)a.unionWith<int>(b), std::logic_error);
                CHECK_THROWS_AS((void)b.unionWith<int>(a), std::logic_error);
            }
            for (const Shape& b : others) {
                CHECK_THROWS_AS((void)a.unionWith<int>(b), std::logic_error);
            }
        }
    }

    SUBCASE("the empty shape is not an identity here but a throw") {
        // A union's identity is the empty set, so `empty ∪ A` is A — which is a
        // PolygonSet only when A is a region. It takes the throw rather than
        // becoming a special case reachable no other way.
        CHECK_THROWS_AS((void)Shape(pgl::EmptyShape<Point>{}).unionWith<int>(Shape(rect)),
                        std::logic_error);
    }
}

TEST_CASE("Shape dispatches difference and symmetricDifference over the same grid") {
    using Point = pgl::Point<int>;
    using PolygonShape = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using RegionSet = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rect({0, 0}, {4, 4});
    const Rectangle offset({2, 2}, {6, 6});
    const Triangle roof({0, 4}, {4, 4}, {2, 6});
    const Convex square(std::vector<Point>{{0, 0}, {3, 0}, {3, 3}, {0, 3}});
    const PolygonShape big({0, 0, 8, 0, 8, 8, 0, 8});
    const Region holed(big, std::vector{PolygonShape({2, 2, 6, 2, 6, 6, 2, 6})});
    const RegionSet set(holed);

    SUBCASE("the answer is a PolygonSet, not a re-wrapped Shape") {
        const auto result = Shape(rect).difference<int>(Shape(offset));
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        CHECK(result.twiceArea() == 2 * (16 - 4));
        CHECK(result == rect.difference<int>(offset));
    }

    SUBCASE("a bare alternative is accepted on either side") {
        CHECK(Shape(roof).difference<int>(rect) == roof.difference<int>(rect));
        CHECK(Shape(rect).symmetricDifference<int>(roof) ==
              Shape(roof).symmetricDifference<int>(rect));
    }

    SUBCASE("the receiver decides which shape is removed from which") {
        // The one of the three that is not symmetric, so the wrapper's two
        // orders are genuinely different answers.
        CHECK(Shape(rect).difference<int>(Shape(offset)) != Shape(offset).difference<int>(Shape(rect)));
        CHECK(Shape(big).difference<int>(Shape(rect)).twiceArea() == 2 * (64 - 16));
        CHECK(Shape(rect).difference<int>(Shape(big)).empty());
    }

    SUBCASE("every ordered pair of the six region alternatives answers") {
        const std::vector<Shape> regions{Shape(rect), Shape(roof), Shape(square),
                                         Shape(big),  Shape(holed), Shape(set)};
        for (const Shape& a : regions) {
            for (const Shape& b : regions) {
                CHECK_NOTHROW((void)a.difference<int>(b));
                CHECK_NOTHROW((void)a.symmetricDifference<int>(b));
                // A difference keeps no more than the receiver had.
                CHECK(a.difference<int>(b).twiceArea() <= a.unionWith<int>(a).twiceArea());
                // A symmetric difference does not depend on the order, and is
                // the union of the two differences.
                CHECK(a.symmetricDifference<int>(b) == b.symmetricDifference<int>(a));
                CHECK(a.symmetricDifference<int>(b) ==
                      a.difference<int>(b).unionWith<int>(b.difference<int>(a)));
                // A shape against itself leaves nothing, either way round.
                CHECK(a.difference<int>(a).empty());
                CHECK(a.symmetricDifference<int>(a).empty());
            }
        }
    }

    SUBCASE("an unbounded region can be removed from a bounded one, but only that way") {
        // `A ∖ B` is inside `A`, so it is bounded whenever the *receiver* is,
        // however far `B` reaches. The wrapper follows the concrete shapes in
        // taking those pairs — and in taking them one way only.
        const std::vector<Shape> regions{Shape(rect), Shape(big), Shape(set)};
        const std::vector<Shape> unbounded{Shape(pgl::Halfplane<Point>({0, 0}, {1, 0})),
                                           Shape(pgl::HalfplaneIntersection<Point>(rect))};

        for (const Shape& a : unbounded) {
            for (const Shape& b : regions) {
                CHECK_NOTHROW((void)b.difference<int>(a));
                CHECK(b.difference<int>(a).twiceArea() <= b.unionWith<int>(b).twiceArea());
                // The other way round is unbounded, and the symmetric ones are
                // unbounded in either order.
                CHECK_THROWS_AS((void)a.difference<int>(b), std::logic_error);
                CHECK_THROWS_AS((void)a.symmetricDifference<int>(b), std::logic_error);
                CHECK_THROWS_AS((void)b.symmetricDifference<int>(a), std::logic_error);
                CHECK_THROWS_AS((void)a.unionWith<int>(b), std::logic_error);
            }
        }

        // Removing a half-plane and removing its opposite partition the
        // receiver, since the two cover the plane and share only their line.
        const Shape upper(pgl::Halfplane<Point>({0, 2}, {1, 2}));
        const Shape lower(pgl::Halfplane<Point>({1, 2}, {0, 2}));
        CHECK(Shape(rect).difference<int>(upper).twiceArea() +
                  Shape(rect).difference<int>(lower).twiceArea() ==
              Shape(rect).unionWith<int>(rect).twiceArea());
    }

    SUBCASE("an alternative with no area, or a round one, throws either way") {
        const std::vector<Shape> regions{Shape(rect), Shape(big), Shape(set)};
        const std::vector<Shape> others{
            Shape(pgl::EmptyShape<Point>{}),
            Shape(Point(1, 1)),
            Shape(pgl::Segment<Point>({0, 0}, {1, 1})),
            Shape(pgl::Line<Point>({0, 0}, {1, 1})),
            Shape(pgl::Disk<Point>({0, 0}, 2)),
            Shape(pgl::Polyline<Point>({0, 0, 1, 1}))};

        for (const Shape& a : others) {
            for (const Shape& b : regions) {
                CHECK_THROWS_AS((void)a.difference<int>(b), std::logic_error);
                CHECK_THROWS_AS((void)b.difference<int>(a), std::logic_error);
                CHECK_THROWS_AS((void)a.symmetricDifference<int>(b), std::logic_error);
                CHECK_THROWS_AS((void)b.symmetricDifference<int>(a), std::logic_error);
            }
        }
    }

    SUBCASE("the default result type follows the wrapper's coordinate type") {
        const auto exact = Shape(rect).difference(Shape(offset));
        CHECK(std::is_same_v<decltype(exact),
                             const pgl::PolygonSet<pgl::Point<pgl::division_result_t<int>>>>);
        CHECK(exact.twiceArea() == 2 * (16 - 4));
    }
}

namespace {

// A concrete type is not dependent, so a bare requires-expression naming a
// member it does not have is a hard error rather than a false result. Going
// through a concept makes the operands dependent and the probe well-formed.
template <class A, class B>
concept HasUnionWith = requires(const A& a, const B& b) { a.unionWith(b); };

template <class A, class B>
concept HasIntersection = requires(const A& a, const B& b) { a.intersection(b); };

template <class A, class B>
concept HasDifference = requires(const A& a, const B& b) { a.difference(b); };

template <class A, class B>
concept HasSymmetricDifference =
    requires(const A& a, const B& b) { a.symmetricDifference(b); };

}  // namespace

TEST_CASE("A concrete shape takes a Shape argument on intersection and unionWith") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using PolygonShape = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using RegionSet = pgl::PolygonSet<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rect({0, 0}, {4, 4});
    const Rectangle offset({2, 2}, {6, 6});
    const Segment diagonal({0, 0}, {4, 4});
    const PolygonShape big({0, 0, 8, 0, 8, 8, 0, 8});

    SUBCASE("intersection forwards and answers as a Shape") {
        const auto result = rect.intersection<int>(Shape(offset));
        CHECK(std::is_same_v<decltype(result), const Shape>);
        // Same answer as the wrapper gives with the operands the other way up.
        CHECK(result == Shape(offset).intersection<int>(rect));
        // The wrapper still unwraps to the tightest alternative it can.
        REQUIRE(result.holdsAlternative<Rectangle>());
        CHECK(Rectangle(result) == Rectangle({2, 2}, {4, 4}));
    }

    SUBCASE("intersection forwards from every receiver that has one") {
        CHECK(diagonal.intersection<int>(Shape(rect)) == Shape(rect).intersection<int>(diagonal));
        CHECK(big.intersection<int>(Shape(rect)) == Shape(rect).intersection<int>(big));
        CHECK(Region(big).intersection<int>(Shape(rect)) ==
              Shape(rect).intersection<int>(Region(big)));
    }

    SUBCASE("an unsupported pair throws instead of failing to compile") {
        CHECK_THROWS_AS((void)diagonal.intersection<int>(Shape(Disk({0, 0}, 2))), std::logic_error);
        CHECK_THROWS_AS((void)rect.intersection<int>(Shape(Disk({0, 0}, 2))), std::logic_error);
    }

    SUBCASE("the empty shape keeps its own absorbing answer") {
        // EmptyShape::intersection is generic and already took a Shape, so it
        // gains no forwarder and still answers with the empty shape itself.
        const auto result = pgl::EmptyShape<Point>{}.intersection<int>(Shape(rect));
        CHECK(std::is_same_v<decltype(result), const pgl::EmptyShape<Point>>);
    }

    SUBCASE("unionWith forwards and answers as a PolygonSet") {
        const auto result = rect.unionWith<int>(Shape(offset));
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        CHECK(result == rect.unionWith<int>(offset));          // same as the concrete call
        CHECK(result == Shape(rect).unionWith<int>(offset));   // and as the wrapper's own
        CHECK(big.unionWith<int>(Shape(rect)) == Shape(rect).unionWith<int>(big));
    }

    SUBCASE("unionWith throws when the wrapper turns out to hold a non-region") {
        CHECK_THROWS_AS((void)rect.unionWith<int>(Shape(diagonal)), std::logic_error);
        CHECK_THROWS_AS((void)big.unionWith<int>(Shape(Disk({0, 0}, 2))), std::logic_error);
    }

    SUBCASE("only the receivers that have a union at all gain the forwarder") {
        // A shape with no area can never have a set-valued union, whatever the
        // wrapper holds, so that stays a compile error rather than becoming a
        // call that is guaranteed to throw.
        static_assert(!HasUnionWith<Segment, Shape>);
        static_assert(!HasUnionWith<Disk, Shape>);
        static_assert(!HasUnionWith<pgl::Halfplane<Point>, Shape>);
        static_assert(!HasUnionWith<pgl::Polyline<Point>, Shape>);
        static_assert(HasUnionWith<Rectangle, Shape>);
        static_assert(HasUnionWith<pgl::Triangle<Point>, Shape>);
        static_assert(HasUnionWith<Convex, Shape>);
        static_assert(HasUnionWith<PolygonShape, Shape>);
        static_assert(HasUnionWith<Region, Shape>);
        static_assert(HasUnionWith<RegionSet, Shape>);
        // Every receiver with an intersection takes the wrapper, including the
        // ones whose only supported pair is a Point.
        static_assert(HasIntersection<Segment, Shape>);
        static_assert(HasIntersection<Disk, Shape>);
        static_assert(HasIntersection<pgl::Line<Point>, Shape>);
    }

    SUBCASE("difference and symmetricDifference take the wrapper too") {
        // The symmetric one hands the pair to the wrapper as the union does.
        // The difference cannot — `A ∖ B` is not `B ∖ A` — so it wraps the
        // receiver instead and lets the wrapper visit both sides; either way
        // the answer is the concrete one.
        CHECK(rect.difference<int>(Shape(offset)) == rect.difference<int>(offset));
        CHECK(rect.difference<int>(Shape(offset)) == Shape(rect).difference<int>(offset));
        CHECK(rect.difference<int>(Shape(offset)) != offset.difference<int>(Shape(rect)));
        CHECK(big.difference<int>(Shape(rect)) == Shape(big).difference<int>(rect));
        CHECK(Region(big).difference<int>(Shape(rect)) == Region(big).difference<int>(rect));
        CHECK(RegionSet(Region(big)).difference<int>(Shape(rect)) ==
              RegionSet(Region(big)).difference<int>(rect));

        CHECK(rect.symmetricDifference<int>(Shape(offset)) == rect.symmetricDifference<int>(offset));
        CHECK(std::is_same_v<decltype(rect.difference<int>(Shape(offset))), RegionSet>);
        CHECK(std::is_same_v<decltype(rect.symmetricDifference<int>(Shape(offset))), RegionSet>);
    }

    SUBCASE("difference and symmetricDifference throw on a non-region alternative") {
        CHECK_THROWS_AS((void)rect.difference<int>(Shape(diagonal)), std::logic_error);
        CHECK_THROWS_AS((void)big.difference<int>(Shape(Disk({0, 0}, 2))), std::logic_error);
        CHECK_THROWS_AS((void)rect.symmetricDifference<int>(Shape(diagonal)), std::logic_error);
    }

    SUBCASE("only the six receivers with area gain the two forwarders") {
        static_assert(!HasDifference<Segment, Shape>);
        static_assert(!HasDifference<Disk, Shape>);
        static_assert(!HasDifference<pgl::Halfplane<Point>, Shape>);
        static_assert(!HasSymmetricDifference<Segment, Shape>);
        static_assert(!HasSymmetricDifference<pgl::Polyline<Point>, Shape>);
        static_assert(HasDifference<Rectangle, Shape>);
        static_assert(HasDifference<pgl::Triangle<Point>, Shape>);
        static_assert(HasDifference<Convex, Shape>);
        static_assert(HasDifference<PolygonShape, Shape>);
        static_assert(HasDifference<Region, Shape>);
        static_assert(HasDifference<RegionSet, Shape>);
        static_assert(HasSymmetricDifference<Rectangle, Shape>);
        static_assert(HasSymmetricDifference<Convex, Shape>);
        static_assert(HasSymmetricDifference<RegionSet, Shape>);

        // And the concrete grid itself: every ordered pair of the six, for both.
        static_assert(HasDifference<Rectangle, Rectangle>);
        static_assert(HasDifference<Rectangle, RegionSet>);
        static_assert(HasDifference<pgl::Triangle<Point>, Rectangle>);
        static_assert(HasDifference<Convex, pgl::Triangle<Point>>);
        static_assert(HasDifference<PolygonShape, RegionSet>);
        static_assert(HasDifference<Region, RegionSet>);
        static_assert(HasSymmetricDifference<Rectangle, Rectangle>);
        static_assert(HasSymmetricDifference<pgl::Triangle<Point>, Rectangle>);
        static_assert(HasSymmetricDifference<Convex, Convex>);
        static_assert(HasSymmetricDifference<PolygonShape, RegionSet>);
        static_assert(HasSymmetricDifference<Region, RegionSet>);
        // and nothing without area, on either side.
        static_assert(!HasDifference<Rectangle, Segment>);
        static_assert(!HasDifference<Rectangle, Disk>);
        static_assert(!HasSymmetricDifference<Rectangle, Segment>);

        // An unbounded region can be removed from a bounded one — the result is
        // inside the receiver, so it is bounded — but not the other way round,
        // and never symmetrically.
        using Halfplane = pgl::Halfplane<Point>;
        using HalfplaneIntersection = pgl::HalfplaneIntersection<Point>;
        static_assert(HasDifference<Rectangle, Halfplane>);
        static_assert(HasDifference<Rectangle, HalfplaneIntersection>);
        static_assert(HasDifference<pgl::Triangle<Point>, Halfplane>);
        static_assert(HasDifference<Convex, HalfplaneIntersection>);
        static_assert(HasDifference<PolygonShape, Halfplane>);
        static_assert(HasDifference<Region, Halfplane>);
        static_assert(HasDifference<RegionSet, HalfplaneIntersection>);
        static_assert(!HasDifference<Halfplane, Rectangle>);
        static_assert(!HasDifference<HalfplaneIntersection, Rectangle>);
        static_assert(!HasSymmetricDifference<Rectangle, Halfplane>);
        static_assert(!HasSymmetricDifference<Region, HalfplaneIntersection>);
        static_assert(!HasUnionWith<Rectangle, Halfplane>);
    }

    SUBCASE("a concrete argument does not reach the Shape overload by conversion") {
        // Shape's converting constructor is implicit, so the point type is
        // deduced from an actual Shape to keep a concrete pair on its own
        // overload — which still answers with the tight type, not a wrapper.
        CHECK(std::is_same_v<decltype(rect.unionWith<int>(offset)), RegionSet>);
        CHECK(std::is_same_v<decltype(rect.intersection<int>(offset)), std::optional<Rectangle>>);
    }
}

TEST_CASE("Shape exposes named is/getIf accessors for every alternative") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    // Each accessor recognizes exactly its own alternative.
    const Shape point = Point(3, 7);
    CHECK(point.isPoint());
    CHECK(point.getIfPoint() != nullptr);
    CHECK(*point.getIfPoint() == Point(3, 7));
    CHECK_FALSE(point.isSegment());
    CHECK(point.getIfSegment() == nullptr);

    const Shape segment = Segment({1, 2}, {3, 4});
    CHECK(segment.isSegment());
    CHECK(*segment.getIfSegment() == Segment({1, 2}, {3, 4}));
    CHECK_FALSE(segment.isOrientedSegment());
    CHECK_FALSE(segment.isPoint());

    // The default (EmptyShape) state answers false to all of them.
    const Shape empty;
    CHECK_FALSE(empty.isPoint());
    CHECK_FALSE(empty.isSegment());
    CHECK_FALSE(empty.isPolygon());
    CHECK(empty.getIfPoint() == nullptr);

    // The accessors agree with holdsAlternative/getIf on every alternative.
    CHECK(Shape(pgl::OrientedSegment<Point>({1, 2}, {3, 4})).isOrientedSegment());
    CHECK(Shape(pgl::Line<Point>({0, 0}, {1, 1})).isLine());
    CHECK(Shape(pgl::OrientedLine<Point>({0, 0}, {1, 1})).isOrientedLine());
    CHECK(Shape(pgl::Ray<Point>({0, 0}, {1, 1})).isRay());
    CHECK(Shape(pgl::Halfplane<Point>({0, 0}, {1, 1})).isHalfplane());
    CHECK(Shape(pgl::Rectangle<Point>({0, 0}, {4, 4})).isRectangle());
    CHECK(Shape(Triangle({0, 0}, {4, 0}, {0, 4})).isTriangle());
    CHECK(Shape(pgl::Disk<Point>({0, 0}, {4, 0}, {0, 4})).isDisk());
    CHECK(Shape(pgl::Convex<Point>({{0, 0}, {4, 0}, {0, 4}})).isConvex());
    CHECK(Shape(pgl::MonotoneChain<Point>({{0, 0}, {1, 1}, {2, 0}})).isMonotoneChain());
    CHECK(Shape(pgl::Polyline<Point>({{0, 0}, {1, 1}, {2, 0}})).isPolyline());
    CHECK(Shape(pgl::Polygon<Point>({{0, 0}, {4, 0}, {0, 4}})).isPolygon());
    CHECK(Shape(pgl::HalfplaneIntersection<Point>(pgl::Rectangle<Point>({0, 0}, {4, 4})))
              .isHalfplaneIntersection());
    CHECK(Shape(pgl::PolygonWithHoles<Point>(pgl::Polygon<Point>({0, 0, 4, 0, 4, 4, 0, 4})))
              .isPolygonWithHoles());
    CHECK(Shape(pgl::PolygonSet<Point>(
                    pgl::PolygonWithHoles<Point>(pgl::Polygon<Point>({0, 0, 4, 0, 4, 4, 0, 4}))))
              .isPolygonSet());

    // The test is on the stored alternative, not on the geometry: a triangle
    // collapsed to a point is still the Triangle alternative.
    const Shape collapsed = Triangle({2, 2}, {2, 2}, {2, 2});
    CHECK(collapsed.isTriangle());
    CHECK_FALSE(collapsed.isPoint());
    CHECK(collapsed.getIfTriangle()->isPoint());

    // The mutable overload hands out a writable pointer into the variant.
    Shape mutablePoint = Point(3, 7);
    *mutablePoint.getIfPoint() = Point(8, 9);
    CHECK(mutablePoint == Shape(Point(8, 9)));
}

TEST_CASE("Shape measures a distance to a Disk in a floating result type") {
    // A distance realized on a circle is generally irrational, so the whole
    // Disk family reports in detail::floating_result_t<ResultNumber>: the
    // requested type when it is floating-point, and double otherwise. The
    // wrapper converts that back to ResultNumber explicitly, which is what
    // lets an exact request reach a Disk pair at all.
    using Point = pgl::Point<pgl::ERational>;
    using Disk = pgl::Disk<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using Shape = pgl::Shape<Point>;

    const Disk disk(Point(20, 0), 1);
    const Region region(Polygon({0, 0, 4, 0, 4, 4, 0, 4}));

    // The pair that motivated the change: PolygonWithHoles (like
    // HalfplaneIntersection) declares squaredDistance<ResultNumber>(Disk) but
    // answers in a floating type, so the wrapper's exact instantiation used to
    // fail to compile on the double -> Rational conversion.
    const Shape wrapped = region;
    const auto viaShape = wrapped.squaredDistance<pgl::ERational>(disk);
    const auto direct = region.squaredDistance<pgl::ERational>(disk);
    CHECK(static_cast<double>(viaShape) == doctest::Approx(direct));
    CHECK(direct == doctest::Approx(15.0 * 15.0));

    // A floating ResultNumber is honoured as asked rather than forced to
    // double, on the concrete shape and through the wrapper alike.
    static_assert(std::is_same_v<decltype(region.squaredDistance<long double>(disk)), long double>);
    static_assert(std::is_same_v<decltype(disk.squaredDistance<float>(Point(0, 0))), float>);
    static_assert(std::is_same_v<decltype(disk.squaredDistance<pgl::ERational>(Point(0, 0))), double>);
    CHECK(wrapped.squaredDistance<double>(disk) == doctest::Approx(direct));

    // The L1/LInf side of the family behaves the same way.
    static_assert(std::is_same_v<decltype(disk.distanceL1<long double>(Point(0, 0))), long double>);
    static_assert(std::is_same_v<decltype(disk.distanceLInf<float>(Point(0, 0))), float>);
    const Shape origin = Point(0, 0);
    CHECK(origin.distanceL1<pgl::ERational>(disk) > pgl::ERational(0));
}
