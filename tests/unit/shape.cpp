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
    static_assert(std::is_same_v<decltype(origin.squaredDistance(corner)), int>);

    // Pairs added on the concrete shapes are reachable through the wrapper.
    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    CHECK(t1.squaredDistance<int>(t2) == 64);
    CHECK(t2.squaredDistance<int>(t1) == 64);

    const Shape below = Triangle({0, 10}, {2, 10}, {1, 13});
    const Shape down = Halfplane({0, 0}, {1, 0});  // boundary y = 0
    CHECK(below.squaredDistance<int>(down) == down.squaredDistance<int>(below));

    // Disk pairs are not templated on ResultNumber, so the wrapper falls back
    // to the double-returning overload and static_casts the result.
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

    // ResultNumber defaults to the wrapper's NumberType.
    static_assert(std::is_same_v<decltype(origin.distanceL1(corner)), int>);

    // A purely horizontal gap: L1 and LInf both equal the axis gap.
    const Shape t1 = Triangle({0, 0}, {2, 0}, {0, 2});
    const Shape t2 = Triangle({10, 0}, {12, 0}, {10, 2});
    CHECK(t1.distanceL1<int>(t2) == 8);
    CHECK(t1.distanceLInf<int>(t2) == 8);
    CHECK(t2.distanceL1<int>(t1) == t1.distanceL1<int>(t2));

    // Disk-Point is not templated on ResultNumber either, so the wrapper falls
    // back to the double-returning overload and static_casts the result.
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
    // most delicate cases: their plain point-to-point/point-to-disk overloads
    // take no ResultNumber template of their own, which previously made the
    // added Shape overload either infinite-recurse (Point-Point, via an
    // unwanted implicit Point -> Shape conversion) or hard-fail to compile
    // (Disk, via forming an invalid Shape<int> while probing with an explicit
    // ResultNumber). Both are covered here so a regression would be caught.
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
    CHECK(triangle.hausdorffDistanceL1(farPoint) == farPoint.hausdorffDistanceL1(triangle));
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
    CHECK_THROWS_AS((void)disk.distanceL1(Shape(segment)), std::logic_error);
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
    const Shape cross = up.intersection(Chain({0, 4, 4, 0}));
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
    const Shape wedge = upper.intersection(right);
    REQUIRE(wedge.holdsAlternative<Region>());
    CHECK_FALSE(Region(wedge).isBounded());
    CHECK(wedge.contains(Shape(Point(3, 3))));

    // Region-with-area intersections stay regions through the wrapper; the
    // clip of the square with a half-plane keeps half the square.
    const Shape clipped = shape.intersection(upper);
    REQUIRE(clipped.holdsAlternative<Region>());
    CHECK(Region(clipped) == square);
    const Shape cell = strip.intersection(shape);
    REQUIRE(cell.holdsAlternative<Region>());
    CHECK(Region(cell).twiceArea<int>() == 12);  // 6 x 1

    // One-dimensional results re-wrap as their own alternatives.
    const Shape chord = shape.intersection(Shape(Segment({-1, 3}, {7, 3})));
    REQUIRE(chord.holdsAlternative<Segment>());
    CHECK(Segment(chord) == Segment({0, 3}, {6, 3}));

    // Unsupported pairs still throw: a Disk, or a Polygon, against the region.
    CHECK_THROWS_AS((void)shape.intersection(Shape(Disk(Point(3, 3), 1))), std::logic_error);
    CHECK_THROWS_AS((void)shape.intersection(Shape(Polygon({0, 0, 4, 0, 4, 4, 0, 4}))), std::logic_error);

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
    const Shape cross = up.intersection(Polyline({0, 4, 4, 0}));
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
