#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <limits>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

#include "pgl.hpp"

static_assert(std::is_same_v<pgl::Point<>, pgl::Point<int>>);
static_assert(std::is_same_v<pgl::Segment<>, pgl::Segment<pgl::Point<int>>>);
static_assert(std::is_same_v<pgl::OrientedSegment<>, pgl::OrientedSegment<pgl::Point<int>>>);
static_assert(std::is_same_v<pgl::Rectangle<>, pgl::Rectangle<pgl::Point<int>>>);

TEST_CASE_TEMPLATE("Rectangle normalizes corners and iterates over min then max", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Rectangle = pgl::Rectangle<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Rectangle empty;
    CHECK(empty.empty());
    CHECK(empty.size() == 0);
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(empty.min() == Point(Number{}, Number{}, ""));
        CHECK(empty.max() == Point(Number(-1), Number(-1), ""));
    } else {
        CHECK(empty.min() == Point(Number{}, Number{}));
        CHECK(empty.max() == Point(Number(-1), Number(-1)));
    }

    const Rectangle rectangle(
        make_point(static_cast<Number>(4), static_cast<Number>(3), "a"),
        make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));

    CHECK(rectangle[0].x() == Number(2));
    CHECK(rectangle[0].y() == Number(1));
    CHECK(rectangle[1].x() == Number(4));
    CHECK(rectangle[1].y() == Number(1));
    CHECK(rectangle[2].x() == Number(4));
    CHECK(rectangle[2].y() == Number(3));
    CHECK(rectangle[3].x() == Number(2));
    CHECK(rectangle[3].y() == Number(3));

    Number coordinate_sum{};
    for (const auto& corner : rectangle) {
        coordinate_sum += corner.x() + corner.y();
    }
    CHECK(coordinate_sum == Number(20));

    if constexpr (requires { rectangle.min().label(); }) {
        CHECK(rectangle.min().label() == "b");
        CHECK(rectangle.max().label() == "a");
    }
}

TEST_CASE("Rectangle synthesizes default labels when normalization mixes coordinates") {
    using Point = pgl::Point<int, std::string>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rectangle(Point(4, 1, "a"), Point(1, 5, "b"));

    CHECK(rectangle.min() == Point(1, 1, ""));
    CHECK(rectangle.max() == Point(4, 5, ""));
    CHECK(rectangle.min().label().empty());
    CHECK(rectangle.max().label().empty());
}

TEST_CASE("Rectangle bounding-box constructor preserves labels of exact corners when available") {
    using Point = pgl::Point<int, std::string>;
    using Rectangle = pgl::Rectangle<Point>;

    const std::vector<Point> points{{4, 1, "bottom-right"}, {1, 5, "top-left"}, {1, 1, "bottom-left"}, {4, 5, "top-right"}};
    const Rectangle rectangle(points);

    CHECK(rectangle.min() == Point(1, 1, "bottom-left"));
    CHECK(rectangle.max() == Point(4, 5, "top-right"));
}

TEST_CASE("Default template parameters make int-based point and shape types available") {
    const pgl::Point<> point(2, 3);
    const pgl::Segment<> segment(4, 3, 2, 1);
    const pgl::OrientedSegment<> oriented_segment(4, 3, 2, 1);
    const pgl::Rectangle<> rectangle(4, 3, 2, 1);

    CHECK(point == pgl::Point<int>(2, 3));
    CHECK(segment.min() == pgl::Point<int>(2, 1));
    CHECK(oriented_segment.source() == pgl::Point<int>(4, 3));
    CHECK(rectangle.max() == pgl::Point<int>(4, 3));
}

TEST_CASE("Class template argument deduction allows object declarations without empty angle brackets") {
    pgl::Point point;
    pgl::Segment segment;
    pgl::OrientedSegment oriented_segment;
    pgl::Rectangle rectangle;
    pgl::Point point_from_values(2, 3);
    pgl::Segment segment_from_values(4, 3, 2, 1);
    pgl::OrientedSegment oriented_segment_from_values(4, 3, 2, 1);
    pgl::Rectangle rectangle_from_values(4, 3, 2, 1);

    static_assert(std::is_same_v<decltype(point), pgl::Point<int>>);
    static_assert(std::is_same_v<decltype(segment), pgl::Segment<pgl::Point<int>>>);
    static_assert(std::is_same_v<decltype(oriented_segment), pgl::OrientedSegment<pgl::Point<int>>>);
    static_assert(std::is_same_v<decltype(rectangle), pgl::Rectangle<pgl::Point<int>>>);
    static_assert(std::is_same_v<decltype(point_from_values), pgl::Point<int>>);
    static_assert(std::is_same_v<decltype(segment_from_values), pgl::Segment<pgl::Point<int>>>);
    static_assert(std::is_same_v<decltype(oriented_segment_from_values), pgl::OrientedSegment<pgl::Point<int>>>);
    static_assert(std::is_same_v<decltype(rectangle_from_values), pgl::Rectangle<pgl::Point<int>>>);

    CHECK(point == pgl::Point<int>(0, 0));
    CHECK(segment.min() == pgl::Point<int>(0, 0));
    CHECK(oriented_segment.source() == pgl::Point<int>(0, 0));
    CHECK(rectangle.empty());
}

TEST_CASE("Rectangle can be built from a non-empty range of points using its bounding box") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const std::set<Point> ordered_points{{4, 1}, {1, 5}, {3, 2}, {2, 4}};
    const Rectangle from_set(ordered_points);

    CHECK(from_set.min() == Point(1, 1));
    CHECK(from_set.max() == Point(4, 5));

    const std::vector<Point> unordered_points{{3, 2}, {8, -1}, {4, 6}, {2, 5}};
    const Rectangle from_vector(unordered_points);

    CHECK(from_vector.min() == Point(2, -1));
    CHECK(from_vector.max() == Point(8, 6));
}

TEST_CASE("Inserting points and containers into rectangles") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(1,2,3,4);
    CHECK(r.min() == Point(1, 2));
    CHECK(r.max() == Point(3, 4));

    r.insert(Point(2, 1));
    CHECK(r.min() == Point(1, 1));
    CHECK(r.max() == Point(3, 4));

    const std::set<Point> ordered_points{{4, 1}, {1, 5}, {3, 2}, {2, 4}};
    r.insert(ordered_points);
    CHECK(r.min() == Point(1, 1));
    CHECK(r.max() == Point(4, 5));

    const std::vector<Point> unordered_points{{3, 2}, {8, -1}, {4, 6}, {2, 5}};
    r.insert(unordered_points);
    CHECK(r.min() == Point(1, -1));
    CHECK(r.max() == Point(8, 6));

    r.insert({{0,4}, {9,0}, {3,7}});
    CHECK(r.min() == Point(0, -1));
    CHECK(r.max() == Point(9, 7));

    using Segment = pgl::Segment<Point>;
    const std::set<Segment> ordered_segments{{-1, 3, -1, 4}, {3, -2, 2, 4}};
    r.insert(ordered_segments);
    CHECK(r.min() == Point(-1, -2));
    CHECK(r.max() == Point(9, 7));

}

TEST_CASE("Construct rectangles from containers and lists of points") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    {
        const std::set<Point> ordered_points{{4, 1}, {1, 5}, {3, 2}, {2, 4}};
        Rectangle r(ordered_points);
        CHECK(r.min() == Point(1, 1));
        CHECK(r.max() == Point(4, 5));
    }

    {
        const std::vector<Point> unordered_points{{4, 1}, {1, 5}, {3, 2}, {2, 4}, {3, 2}, {8, -1}, {4, 6}, {2, 5}};
        Rectangle r(unordered_points);
        CHECK(r.min() == Point(1, -1));
        CHECK(r.max() == Point(8, 6));
    }

    {
        Rectangle r({{4, 1}, {1, 5}, {3, 2}, {2, 4}, {3, 2}, {8, -1}, {4, 6}, {2, 5}});
        CHECK(r.min() == Point(1, -1));
        CHECK(r.max() == Point(8, 6));
    }

}

TEST_CASE("Rectangle bounding-box constructor turns an empty range into the empty rectangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const std::vector<Point> empty_points;
    CHECK(Rectangle(empty_points).empty());

    // A range of shapes that are themselves empty encloses nothing either.
    const std::vector<Rectangle> empty_boxes{Rectangle(), Rectangle()};
    CHECK(Rectangle(empty_boxes).empty());
}

TEST_CASE("Rectangle streams, scales, translates, and exposes vertices and edges") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Rectangle rectangle(4, 3, 2, 1);

    std::ostringstream stream;
    stream << rectangle;
    CHECK(stream.str() == "[(2,1),(4,3)]");

    const auto scaled = 2 * rectangle;
    const auto translated = rectangle + Point(4, 3);
    const auto shifted = translated - Point(1, 1);

    CHECK(scaled.min() == Point(4, 2));
    CHECK(scaled.max() == Point(8, 6));
    CHECK(translated.min() == Point(6, 4));
    CHECK(translated.max() == Point(8, 6));
    CHECK(shifted.min() == Point(5, 3));
    CHECK(shifted.max() == Point(7, 5));

    const auto vertices = rectangle.vertices();
    CHECK(vertices[0] == Point(2, 1));
    CHECK(vertices[1] == Point(4, 1));
    CHECK(vertices[2] == Point(4, 3));
    CHECK(vertices[3] == Point(2, 3));

    const auto edges = rectangle.edges();
    CHECK(edges[0] == Segment(Point(2, 1), Point(4, 1)));
    CHECK(edges[1] == Segment(Point(4, 1), Point(4, 3)));
    CHECK(edges[2] == Segment(Point(2, 3), Point(4, 3)));
    CHECK(edges[3] == Segment(Point(2, 1), Point(2, 3)));

    std::vector<Segment> iterated_edges;
    for (auto iterator = rectangle.edgesBegin(); iterator != rectangle.edgesEnd(); ++iterator) {
        iterated_edges.push_back(*iterator);
    }
    CHECK(iterated_edges == std::vector<Segment>(edges.begin(), edges.end()));

    const auto oriented_edges = rectangle.orientedEdges();
    CHECK(oriented_edges[0] == OrientedSegment(Point(2, 1), Point(4, 1)));
    CHECK(oriented_edges[1] == OrientedSegment(Point(4, 1), Point(4, 3)));
    CHECK(oriented_edges[2] == OrientedSegment(Point(4, 3), Point(2, 3)));
    CHECK(oriented_edges[3] == OrientedSegment(Point(2, 3), Point(2, 1)));

    std::vector<OrientedSegment> iterated_oriented_edges;
    for (auto iterator = rectangle.orientedEdgesBegin(); iterator != rectangle.orientedEdgesEnd(); ++iterator) {
        iterated_oriented_edges.push_back(*iterator);
    }
    CHECK(iterated_oriented_edges == std::vector<OrientedSegment>(oriented_edges.begin(), oriented_edges.end()));
}

TEST_CASE("Rectangle converts between labeled and unlabeled corners") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainRectangle = pgl::Rectangle<PlainPoint>;
    using LabelRectangle = pgl::Rectangle<LabelPoint>;

    const LabelRectangle labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainRectangle plain_source(4, 3, 2, 1);

    const PlainRectangle plain_from_labeled = labeled;
    const LabelRectangle labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.min() == PlainPoint(2, 1));
    CHECK(plain_from_labeled.max() == PlainPoint(4, 3));
    CHECK(labeled_from_plain.min() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.max() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.min().label().empty());
    CHECK(labeled_from_plain.max().label().empty());

    PlainRectangle plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.min() == PlainPoint(2, 1));
    CHECK(plain_assigned.max() == PlainPoint(4, 3));

    LabelRectangle labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.min() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.max() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.min().label().empty());
    CHECK(labeled_assigned.max().label().empty());
}

TEST_CASE("Rectangle supports in-place translation, scaling, and bounding boxes") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    Rectangle rectangle(4, 3, 2, 1);
    rectangle += Point(1, 2);
    CHECK(rectangle.min() == Point(3, 3));
    CHECK(rectangle.max() == Point(5, 5));

    rectangle -= Point(1, 1);
    CHECK(rectangle.min() == Point(2, 2));
    CHECK(rectangle.max() == Point(4, 4));

    rectangle *= -2;
    CHECK(rectangle.min() == Point(-8, -8));
    CHECK(rectangle.max() == Point(-4, -4));

    rectangle /= -2;
    CHECK(rectangle.min() == Point(2, 2));
    CHECK(rectangle.max() == Point(4, 4));

    CHECK(rectangle.bbox() == rectangle);

    const auto floating_box = rectangle.fbox();
    CHECK(floating_box.min() == pgl::Point<double>(2.0, 2.0));
    CHECK(floating_box.max() == pgl::Point<double>(4.0, 4.0));

    rectangle.insert(Point(1, 5));
    CHECK(rectangle.min() == Point(1, 2));
    CHECK(rectangle.max() == Point(4, 5));

    rectangle.insert(Segment(Point(-1, 3), Point(3, 7)));
    CHECK(rectangle.min() == Point(-1, 2));
    CHECK(rectangle.max() == Point(4, 7));

    rectangle.insert(OrientedSegment(Point(6, 0), Point(2, 6)));
    CHECK(rectangle.min() == Point(-1, 0));
    CHECK(rectangle.max() == Point(6, 7));

    rectangle.insert(Rectangle(Point(-2, -3), Point(0, 1)));
    CHECK(rectangle.min() == Point(-2, -3));
    CHECK(rectangle.max() == Point(6, 7));
}

TEST_CASE_TEMPLATE("Rectangle reports width, height, area, midpoint, and a diameter", Rectangle, pgl::Rectangle<pgl::Point<int>>, pgl::Rectangle<pgl::Point<double>>) {
    using Point = typename Rectangle::PointType;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;
    using Segment = pgl::Segment<Point>;

    const Rectangle rectangle(static_cast<Number>(4), static_cast<Number>(3), static_cast<Number>(2), static_cast<Number>(1));

    CHECK(rectangle.width() == Number(2));
    CHECK(rectangle.height() == Number(2));
    CHECK(rectangle.area() == Number(4));
    CHECK(rectangle.twiceArea() == Number(8));
    CHECK(rectangle.template midpoint<double>().x() == doctest::Approx(3.0));
    CHECK(rectangle.template midpoint<double>().y() == doctest::Approx(2.0));
    CHECK(rectangle.template centroid<double>().x() == doctest::Approx(3.0));
    CHECK(rectangle.template centroid<double>().y() == doctest::Approx(2.0));
    CHECK(rectangle.template center<double>().x() == doctest::Approx(3.0));
    CHECK(rectangle.template center<double>().y() == doctest::Approx(2.0));

    const auto circumcircle = rectangle.circumcircle();
    CHECK(circumcircle.template center<double>().x() == doctest::Approx(3.0));
    CHECK(circumcircle.template center<double>().y() == doctest::Approx(2.0));
    CHECK(circumcircle.template squaredRadius<double>() == doctest::Approx(2.0));

    CHECK(rectangle.diameter() == Segment(Point(2, 1), Point(4, 3)));
    CHECK(rectangle.convexHull() == rectangle.asConvex());
    CHECK(rectangle.template pointInside<Number>() == Point(Number(3), Number(2)));
    CHECK_FALSE(rectangle.isDegenerate());
    CHECK(Rectangle(Point(2, 1), Point(2, 3)).isDegenerate());
    CHECK(Rectangle(Point(2, 1), Point(4, 1)).isDegenerate());
}

TEST_CASE("Rectangle midpoint can be checked with exact rational coordinates") {
    using Rational = pgl::Rational<int64_t>;
    using Rectangle = pgl::Rectangle<pgl::Point<int>>;

    const Rectangle rectangle({1, 1}, {2, 4});
    const auto midpoint = rectangle.midpoint<Rational>();

    CHECK(midpoint.x() == Rational(3, 2));
    CHECK(midpoint.y() == Rational(5, 2));
}

TEST_CASE("Rectangle distinguishes containment, boundary contact, overlap, and disjointness") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle outer({0, 0}, {4, 3});
    const Rectangle inner({1, 1}, {3, 2});
    const Rectangle overlap({3, 2}, {6, 5});
    const Rectangle touching({4, 1}, {6, 2});
    const Rectangle disjoint({5, 4}, {7, 6});
    const Rectangle line({1, 1}, {1, 3});

    CHECK(outer.verticesContain(Point(0, 0)));
    CHECK_FALSE(outer.verticesContain(Point(0, 1)));

    CHECK(outer.contains(inner));
    CHECK(outer.interiorContains(inner));
    CHECK_FALSE(inner.contains(outer));
    CHECK_FALSE(outer.contains(overlap));
    CHECK_FALSE(outer.interiorContains(touching));

    CHECK(outer.intersects(inner));
    CHECK(outer.intersects(overlap));
    CHECK(outer.intersects(touching));
    CHECK_FALSE(outer.intersects(disjoint));

    CHECK(outer.interiorsIntersect(inner));
    CHECK(outer.interiorsIntersect(overlap));
    CHECK_FALSE(outer.interiorsIntersect(touching));
    CHECK_FALSE(outer.interiorsIntersect(disjoint));
    CHECK_FALSE(outer.interiorsIntersect(line));
}

TEST_CASE("Rectangle boundary containment accepts boundary-aligned segments and rejects diagonals") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});
    const Rectangle inner_rectangle({1, 1}, {3, 2});
    const Segment boundary_segment({1, 0}, {3, 0});
    const Segment diagonal({0, 0}, {4, 3});
    const Triangle inner_triangle({1, 1}, {3, 1}, {2, 2});

    CHECK(rectangle.boundaryContains(boundary_segment));
    CHECK_FALSE(rectangle.boundaryContains(inner_rectangle));
    CHECK_FALSE(rectangle.boundaryContains(inner_triangle));
    CHECK_FALSE(rectangle.boundaryContains(diagonal));
}

TEST_CASE("Rectangle separates larger rectangles only when it splits them into two components") {
    using Rectangle = pgl::Rectangle<pgl::Point<int>>;

    const Rectangle vertical_bar({2, 0}, {4, 6});
    const Rectangle horizontal_bar({0, 2}, {6, 4});
    const Rectangle outer_square({0, 0}, {6, 6});
    const Rectangle inner_square({2, 2}, {4, 4});

    CHECK(vertical_bar.separates(outer_square));
    CHECK(horizontal_bar.separates(outer_square));
    CHECK_FALSE(outer_square.separates(vertical_bar));
    CHECK_FALSE(inner_square.separates(outer_square));

    CHECK_FALSE(vertical_bar.crosses(outer_square));
    CHECK(vertical_bar.crosses(horizontal_bar));
    CHECK_FALSE(inner_square.crosses(outer_square));
}

TEST_CASE("Rectangle predicates handle linear primitives") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});
    const Segment inner_segment({1, 1}, {3, 2});
    const Segment crossing_segment({-1, 1}, {5, 1});
    const Segment touching_segment({4, 1}, {6, 1});
    const Segment boundary_endpoint_only_segment({-1, 0}, {0, 1});
    const Line crossing_line({-1, 1}, {5, 1});
    const Line tangent_line({0, 0}, {4, 0});
    const Line corner_line({-1, 1}, {1, -1});
    const Line outside_line({0, 5}, {4, 5});

    CHECK(rectangle.contains(inner_segment));
    CHECK(rectangle.interiorContains(inner_segment));
    CHECK_FALSE(rectangle.contains(crossing_segment));
    CHECK_FALSE(rectangle.contains(crossing_line));
    CHECK(rectangle.contains(Segment({1, 0}, {3, 0})));
    CHECK_FALSE(rectangle.interiorContains(Segment({1, 0}, {3, 0})));

    CHECK(rectangle.intersects(crossing_segment));
    CHECK(rectangle.intersects(touching_segment));
    CHECK(rectangle.intersects(boundary_endpoint_only_segment));
    CHECK(rectangle.intersects(crossing_line));
    CHECK(rectangle.intersects(tangent_line));
    CHECK(rectangle.intersects(corner_line));
    CHECK_FALSE(rectangle.intersects(outside_line));

    CHECK(rectangle.interiorsIntersect(crossing_segment));
    CHECK_FALSE(rectangle.interiorsIntersect(touching_segment));
    CHECK_FALSE(rectangle.interiorsIntersect(boundary_endpoint_only_segment));
    CHECK(rectangle.interiorsIntersect(crossing_line));
    CHECK_FALSE(rectangle.interiorsIntersect(tangent_line));
    CHECK_FALSE(rectangle.interiorsIntersect(corner_line));
    CHECK_FALSE(rectangle.interiorsIntersect(outside_line));

    CHECK(rectangle.separates(crossing_segment));
    CHECK(rectangle.crosses(crossing_segment));
    CHECK_FALSE(rectangle.separates(touching_segment));
    CHECK(rectangle.separates(crossing_line));
    CHECK(rectangle.crosses(crossing_line));
    CHECK(rectangle.separates(tangent_line));
    CHECK_FALSE(rectangle.crosses(tangent_line));
}

TEST_CASE("Rectangle covers the non-Convex contract for interiorsIntersect") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    CHECK(rectangle.interiorsIntersect(Line({-1, 1}, {5, 1})));
    CHECK(rectangle.interiorsIntersect(Segment({-1, 1}, {5, 1})));
    CHECK(rectangle.interiorsIntersect(Shape(Rectangle({1, 1}, {3, 2}))));
}

TEST_CASE("Rectangle covers the non-Convex contract for separates") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    CHECK(rectangle.separates(Line({-1, 1}, {5, 1})));
    CHECK(rectangle.separates(Segment({-1, 1}, {5, 1})));
    CHECK(rectangle.separates(Shape(Segment({-1, 1}, {5, 1}))));
}

TEST_CASE("Rectangle covers the non-Convex contract for crosses") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    CHECK(rectangle.crosses(Line({-1, 1}, {5, 1})));
    CHECK(rectangle.crosses(Segment({-1, 1}, {5, 1})));
    CHECK(rectangle.crosses(Shape(Segment({-1, 1}, {5, 1}))));
}

TEST_CASE("Linear primitives separate rectangles only when clipped through the interior") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    const Segment side_to_side({-1, 1}, {5, 1});
    const Segment corner_to_corner({0, 0}, {4, 3});
    const Segment starts_inside({2, 1}, {5, 1});
    const Segment along_edge({0, 0}, {4, 0});

    CHECK(side_to_side.separates(rectangle));
    CHECK(side_to_side.crosses(rectangle));
    CHECK(corner_to_corner.separates(rectangle));

    CHECK_FALSE(starts_inside.separates(rectangle));
    CHECK_FALSE(along_edge.separates(rectangle));

    CHECK(Line({-1, 1}, {5, 1}).separates(rectangle));
    CHECK_FALSE(Line({0, 0}, {4, 0}).separates(rectangle));
}

TEST_CASE("Rectangle intersections return clipped shapes") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    const auto rectangle_overlap = rectangle.intersection<int>(Rectangle({2, 1}, {6, 5}));
    REQUIRE(rectangle_overlap);
    CHECK(*rectangle_overlap == Rectangle({2, 1}, {4, 3}));

    const auto rectangle_touch = rectangle.intersection<int>(Rectangle({4, 1}, {6, 2}));
    REQUIRE(rectangle_touch);
    CHECK(*rectangle_touch == Rectangle({4, 1}, {4, 2}));

    CHECK_FALSE(rectangle.intersection<int>(Rectangle({5, 4}, {6, 5})));

    const auto crossing_segment = rectangle.intersection<int>(Segment({-1, 1}, {5, 1}));
    REQUIRE(crossing_segment);
    REQUIRE(std::holds_alternative<Segment>(*crossing_segment));
    CHECK(std::get<Segment>(*crossing_segment) == Segment({0, 1}, {4, 1}));

    const auto inner_segment = rectangle.intersection<int>(Segment({1, 1}, {3, 2}));
    REQUIRE(inner_segment);
    REQUIRE(std::holds_alternative<Segment>(*inner_segment));
    CHECK(std::get<Segment>(*inner_segment) == Segment({1, 1}, {3, 2}));

    const auto fractional_segment = rectangle.intersection<double>(Segment({-1, -1}, {5, 2}));
    REQUIRE(fractional_segment);
    REQUIRE(std::holds_alternative<pgl::Segment<pgl::Point<double>>>(*fractional_segment));
    const auto& clipped_fractional_segment = std::get<pgl::Segment<pgl::Point<double>>>(*fractional_segment);
    CHECK(clipped_fractional_segment.min().x() == doctest::Approx(1.0));
    CHECK(clipped_fractional_segment.min().y() == doctest::Approx(0.0));
    CHECK(clipped_fractional_segment.max().x() == doctest::Approx(4.0));
    CHECK(clipped_fractional_segment.max().y() == doctest::Approx(1.5));

    const auto touching_segment = rectangle.intersection<int>(Segment({4, 1}, {6, 1}));
    REQUIRE(touching_segment);
    REQUIRE(std::holds_alternative<Point>(*touching_segment));
    CHECK(std::get<Point>(*touching_segment) == Point(4, 1));

    CHECK_FALSE(rectangle.intersection<int>(Segment({5, 4}, {6, 4})));

    const auto crossing_line = rectangle.intersection<int>(Line({-1, 1}, {5, 1}));
    REQUIRE(crossing_line);
    REQUIRE(std::holds_alternative<Segment>(*crossing_line));
    CHECK(std::get<Segment>(*crossing_line) == Segment({0, 1}, {4, 1}));

    const auto tangent_line = rectangle.intersection<int>(Line({0, 0}, {4, 0}));
    REQUIRE(tangent_line);
    REQUIRE(std::holds_alternative<Segment>(*tangent_line));
    CHECK(std::get<Segment>(*tangent_line) == Segment({0, 0}, {4, 0}));

    const auto vertex_line = rectangle.intersection<int>(Line({-1, 1}, {1, -1}));
    REQUIRE(vertex_line);
    REQUIRE(std::holds_alternative<Point>(*vertex_line));
    CHECK(std::get<Point>(*vertex_line) == Point(0, 0));

    CHECK_FALSE(rectangle.intersection<int>(Line({0, 5}, {4, 5})));

    const auto crossing_ray = rectangle.intersection<int>(Ray({-2, 1}, {2, 1}));
    REQUIRE(crossing_ray);
    REQUIRE(std::holds_alternative<Segment>(*crossing_ray));
    CHECK(std::get<Segment>(*crossing_ray) == Segment({0, 1}, {4, 1}));

    const auto source_inside_ray = rectangle.intersection<int>(Ray({2, 1}, {8, 1}));
    REQUIRE(source_inside_ray);
    REQUIRE(std::holds_alternative<Segment>(*source_inside_ray));
    CHECK(std::get<Segment>(*source_inside_ray) == Segment({2, 1}, {4, 1}));

    const auto tangent_ray = rectangle.intersection<int>(Ray({-2, 0}, {2, 0}));
    REQUIRE(tangent_ray);
    REQUIRE(std::holds_alternative<Segment>(*tangent_ray));
    CHECK(std::get<Segment>(*tangent_ray) == Segment({0, 0}, {4, 0}));

    CHECK_FALSE(rectangle.intersection<int>(Ray({-2, 5}, {2, 5})));
}

TEST_CASE("Rectangle distances handle outside points, disjoint rectangles, and Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});
    const Rectangle inner({1, 1}, {3, 2});
    const Rectangle touching({4, 1}, {6, 2});
    const Rectangle disjoint({6, 5}, {7, 6});

    CHECK(rectangle.squaredDistance<int>(Point(2, 2)) == 0);
    CHECK(rectangle.squaredDistance<int>(Point(6, 5)) == 8);
    CHECK(rectangle.squaredDistance<int>(inner) == 0);
    CHECK(rectangle.squaredDistance<int>(touching) == 0);
    CHECK(rectangle.squaredDistance<int>(disjoint) == 8);
    CHECK(rectangle.squaredHausdorffDistance<int>(inner) == 2);
    CHECK(rectangle.squaredDistance<int>(Line({0, 5}, {4, 5})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance<int>(OrientedLine({6, 0}, {6, 3})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance<int>(Segment({6, 1}, {8, 1})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance<int>(OrientedSegment({2, 5}, {3, 5})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance<int>(Ray({6, 1}, {8, 1})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance<int>(Ray({6, 1}, {2, 1})) == doctest::Approx(0.0));

    CHECK(rectangle.distanceL1(Point(2, 2)) == 0);
    CHECK(rectangle.distanceL1(Point(6, 5)) == 4);
    CHECK(rectangle.distanceL1(inner) == 0);
    CHECK(rectangle.distanceL1(touching) == 0);
    CHECK(rectangle.distanceL1(disjoint) == 4);
    CHECK(rectangle.hausdorffDistanceL1<int>(inner) == 2);
    CHECK(rectangle.distanceL1(Line({0, 5}, {4, 5})) == 2);
    CHECK(rectangle.distanceL1(OrientedLine({6, 0}, {6, 3})) == 2);
    CHECK(rectangle.distanceL1(Segment({6, 1}, {8, 1})) == 2);
    CHECK(rectangle.distanceL1(OrientedSegment({2, 5}, {3, 5})) == 2);
    CHECK(rectangle.distanceL1(Ray({6, 1}, {8, 1})) == 2);
    CHECK(rectangle.distanceL1(Ray({6, 1}, {2, 1})) == 0);

    CHECK(rectangle.distanceLInf(Point(2, 2)) == 0);
    CHECK(rectangle.distanceLInf(Point(6, 5)) == 2);
    CHECK(rectangle.distanceLInf(inner) == 0);
    CHECK(rectangle.distanceLInf(touching) == 0);
    CHECK(rectangle.distanceLInf(disjoint) == 2);
    CHECK(rectangle.hausdorffDistanceLInf<int>(inner) == 1);
    CHECK(rectangle.distanceLInf(Line({0, 5}, {4, 5})) == 2);
    CHECK(rectangle.distanceLInf(OrientedLine({6, 0}, {6, 3})) == 2);
    CHECK(rectangle.distanceLInf(Segment({6, 1}, {8, 1})) == 2);
    CHECK(rectangle.distanceLInf(OrientedSegment({2, 5}, {3, 5})) == 2);
    CHECK(rectangle.distanceLInf(Ray({6, 1}, {8, 1})) == 2);
    CHECK(rectangle.distanceLInf(Ray({6, 1}, {2, 1})) == 0);
}

TEST_CASE("Rectangle ordering and hashing ignore input corner order and point labels") {
    using Rectangle = pgl::Rectangle<pgl::Point<int, std::string>>;

    const Rectangle first({4, 3, "a"}, {2, 1, "b"});
    const Rectangle second({2, 1, "x"}, {4, 3, "y"});

    CHECK(first == second);
    CHECK_FALSE(first < second);
    CHECK_FALSE(second < first);

    std::set<Rectangle> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(second);
    CHECK(ordered_set.size() == 1);

    std::unordered_set<Rectangle> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(second);
    CHECK(unordered_set.size() == 1);
}

TEST_CASE("Rectangle converts to a half-plane intersection") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    SUBCASE("a full-dimensional rectangle") {
        const Rectangle rect(Point(0, 0), Point(4, 2));
        const auto region = rect.asHalfplaneIntersection();
        static_assert(std::is_same_v<decltype(region), const pgl::HalfplaneIntersection<Point>>);
        CHECK(!region.empty());
        CHECK(!region.isDegenerate());
        CHECK(region.isBounded());
        CHECK(region.interiorContains(Point(2, 1)));
        CHECK(region.contains(Point(0, 0)));   // corner on the boundary
        CHECK(!region.contains(Point(5, 1)));  // outside
        CHECK(region == pgl::HalfplaneIntersection<Point>(rect));
    }

    SUBCASE("a zero-height rectangle collapses to a horizontal segment") {
        const Rectangle rect(Point(0, 3), Point(4, 3));
        const auto region = rect.asHalfplaneIntersection();
        CHECK(region.isDegenerate());
        CHECK(region.contains(Point(0, 3)));
        CHECK(region.contains(Point(2, 3)));
        CHECK(region.contains(Point(4, 3)));
        CHECK(!region.contains(Point(5, 3)));  // beyond the corner
        CHECK(!region.contains(Point(2, 4)));  // off the segment
    }

    SUBCASE("a zero-width rectangle collapses to a vertical segment") {
        const Rectangle rect(Point(1, 0), Point(1, 5));
        const auto region = rect.asHalfplaneIntersection();
        CHECK(region.isDegenerate());
        CHECK(region.contains(Point(1, 0)));
        CHECK(region.contains(Point(1, 2)));
        CHECK(region.contains(Point(1, 5)));
        CHECK(!region.contains(Point(1, 6)));  // beyond the corner
        CHECK(!region.contains(Point(2, 2)));  // off the segment
    }

    SUBCASE("a single-point rectangle collapses to a point") {
        const Rectangle rect(Point(2, 2), Point(2, 2));
        const auto region = rect.asHalfplaneIntersection();
        CHECK(region.isDegenerate());
        CHECK(region.contains(Point(2, 2)));
        CHECK(!region.contains(Point(3, 2)));
        CHECK(!region.contains(Point(2, 3)));
    }
}

TEST_CASE("Rectangle unites with Rectangle into a set of regions") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect(Point(0, 0), Point(4, 4));

    SUBCASE("two overlapping rectangles make one staircase region") {
        const auto result = rect.regularizedUnion<int>(Rectangle(Point(2, 2), Point(6, 6)));
        static_assert(std::is_same_v<decltype(result), const pgl::PolygonSet<Point>>);
        REQUIRE(result.componentCount() == 1);
        CHECK(!result.component(0).hasHoles());
        CHECK(result.twiceArea() == 2 * (16 + 16 - 4));
        // Two diagonally offset squares meet in a re-entrant step on each side,
        // so the outline is eight corners and no vertex sits mid-edge.
        CHECK(result.component(0).outer().size() == 8);
    }

    SUBCASE("two disjoint rectangles stay two components") {
        const auto result = rect.regularizedUnion<int>(Rectangle(Point(10, 10), Point(12, 12)));
        CHECK(result.componentCount() == 2);
        CHECK(result.twiceArea() == 2 * (16 + 4));
    }

    SUBCASE("rectangles meeting at a corner stay two components") {
        // A region may not have a self-touching outer ring, so the pinch splits.
        const auto result = rect.regularizedUnion<int>(Rectangle(Point(4, 4), Point(6, 6)));
        CHECK(result.componentCount() == 2);
        CHECK(result.twiceArea() == 2 * (16 + 4));
    }

    SUBCASE("a rectangle covering another gives just the cover") {
        const auto result = rect.regularizedUnion<int>(Rectangle(Point(1, 1), Point(2, 2)));
        REQUIRE(result.componentCount() == 1);
        CHECK(result.component(0) == pgl::PolygonWithHoles<Point>(rect.asPolygon()));
    }

    SUBCASE("a union with itself is idempotent") {
        const auto result = rect.regularizedUnion<int>(rect);
        REQUIRE(result.componentCount() == 1);
        CHECK(result.component(0) == pgl::PolygonWithHoles<Point>(rect.asPolygon()));
    }

    SUBCASE("a rectangle with no area contributes nothing") {
        const Rectangle flat(Point(0, 0), Point(0, 4));
        CHECK(flat.regularizedUnion<int>(rect) == rect.asPolygonSet());
        CHECK(flat.regularizedUnion<int>(flat).empty());
    }
}

TEST_CASE("The empty rectangle is the empty set of points") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using SegmentShape = pgl::Segment<Point>;
    using TriangleShape = pgl::Triangle<Point>;

    const Rectangle empty;
    const Rectangle rect(0, 0, 4, 4);
    const TriangleShape triangle(Point(0, 0), Point(4, 0), Point(0, 4));
    const SegmentShape segment(Point(1, 1), Point(3, 3));

    SUBCASE("it is reached by default construction and by inverted corners") {
        CHECK(empty.empty());
        CHECK(empty.min() == Point(0, 0));
        CHECK(empty.max() == Point(-1, -1));

        // Every inverted pair normalizes to the one canonical empty value, so
        // equality, ordering, and hashing keep working.
        CHECK(Rectangle(Point(5, 5), Point(1, 1), true) == empty);
        CHECK(Rectangle(3, 7, 3, 2, true) == empty);
        CHECK(std::hash<Rectangle>{}(Rectangle(Point(5, 5), Point(1, 1), true)) ==
              std::hash<Rectangle>{}(empty));

        // Without the minmax flag the corners are normalized instead, so the
        // ordinary constructors can never produce it by accident.
        CHECK_FALSE(Rectangle(Point(5, 5), Point(1, 1)).empty());
    }

    SUBCASE("it has no vertices, no extent, and no area") {
        CHECK(empty.size() == 0);
        CHECK(empty.begin() == empty.end());
        CHECK(empty.edgesBegin() == empty.edgesEnd());
        CHECK(empty.orientedEdgesBegin() == empty.orientedEdgesEnd());
        CHECK(empty.index(Point(0, 0)) == -1);
        CHECK_FALSE(empty.verticesContain(Point(0, 0)));
        CHECK(empty.width() == 0);
        CHECK(empty.height() == 0);
        CHECK(empty.area() == 0);
        CHECK(empty.twiceArea() == 0);
    }

    SUBCASE("it is degenerate but well defined, and neither a point nor a segment") {
        CHECK(empty.isDegenerate());
        CHECK_FALSE(empty.isUndefined());
        CHECK_FALSE(empty.isPoint());
        CHECK_FALSE(empty.isSegment());
        CHECK_FALSE(empty.getIfPoint().has_value());
        CHECK_FALSE(empty.getIfSegment().has_value());
    }

    SUBCASE("it contains nothing but itself") {
        CHECK_FALSE(empty.contains(Point(0, 0)));
        CHECK_FALSE(empty.contains(segment));
        CHECK_FALSE(empty.contains(triangle));
        CHECK_FALSE(empty.contains(rect));
        CHECK_FALSE(empty.boundaryContains(Point(0, 0)));

        CHECK(empty.contains(empty));
        CHECK(empty.boundaryContains(empty));
        CHECK(empty.interiorContains(empty));
    }

    SUBCASE("every shape contains it, and none meets it") {
        CHECK(rect.contains(empty));
        CHECK(rect.boundaryContains(empty));
        CHECK(rect.interiorContains(empty));
        CHECK(triangle.contains(empty));
        CHECK(segment.contains(empty));
        CHECK(Point(0, 0).contains(empty));

        CHECK_FALSE(rect.intersects(empty));
        CHECK_FALSE(rect.interiorsIntersect(empty));
        CHECK_FALSE(rect.separates(empty));
        CHECK_FALSE(rect.crosses(empty));

        CHECK_FALSE(empty.intersects(rect));
        CHECK_FALSE(empty.intersects(triangle));
        CHECK_FALSE(empty.intersects(segment));
        CHECK_FALSE(empty.interiorsIntersect(rect));
        CHECK_FALSE(empty.separates(triangle));
        CHECK_FALSE(empty.crosses(triangle));
    }

    SUBCASE("it leaves an already-split set of regions split") {
        // separates asks whether B minus A is disconnected. A set of regions is
        // the one target that can already be in pieces, so removing the empty
        // set from it still answers true -- exactly as any remover that misses
        // it does.
        const Rectangle far_away(10, 10, 12, 12);
        const auto two_pieces = rect.regularizedUnion<int>(far_away);
        CHECK(empty.separates(two_pieces));
        CHECK_FALSE(empty.separates(rect.asPolygonSet()));

        // The empty set as the target is never disconnected by anything.
        CHECK_FALSE(rect.separates(pgl::PolygonSet<Point>()));
    }

    SUBCASE("inserting a point turns it into that point") {
        Rectangle box;
        box.insert(Point(3, 4));
        CHECK_FALSE(box.empty());
        CHECK(box.isPoint());
        CHECK(box.min() == Point(3, 4));
        CHECK(box.max() == Point(3, 4));

        // ... rather than growing the placeholder corners into a box.
        box.insert(Point(5, 1));
        CHECK(box == Rectangle(3, 1, 5, 4));
    }

    SUBCASE("inserting the empty rectangle changes nothing") {
        Rectangle box(rect);
        box.insert(Rectangle());
        CHECK(box == rect);

        Rectangle still_empty;
        still_empty.insert(Rectangle());
        CHECK(still_empty.empty());

        // A shape whose bounding box is empty contributes nothing either.
        still_empty.insert(pgl::Polygon<Point>());
        CHECK(still_empty.empty());
    }

    SUBCASE("it converts to the empty Convex, Polygon, and region") {
        CHECK(empty.asConvex().empty());
        CHECK(empty.asPolygon().empty());
        CHECK(empty.asPolygonWithHoles().empty());
        CHECK(empty.asPolygonSet().empty());
        CHECK(empty.asHalfplaneIntersection().empty());
        CHECK(empty.convexHull().empty());
    }

    SUBCASE("it is its own bounding box, and the bounding box of empty shapes") {
        CHECK(empty.bbox() == empty);
        CHECK(empty.fbox<double>().empty());
        CHECK(pgl::Convex<Point>().bbox().empty());
        CHECK(pgl::Polygon<Point>().bbox().empty());
    }

    SUBCASE("transformations leave it empty and canonical") {
        CHECK((empty + Point(5, 5)) == empty);
        CHECK((empty - Point(5, 5)) == empty);
        CHECK((empty * 3) == empty);
        CHECK((empty / 2) == empty);
        CHECK(empty.rotated90() == empty);
        CHECK(empty.scaledUpX(3) == empty);
        CHECK(empty.scaledUpY(3) == empty);
        CHECK(empty.scaledDownX(2) == empty);
        CHECK(empty.scaledDownY(2) == empty);

        Rectangle box;
        box += Point(7, 7);
        CHECK(box == empty);
    }

    SUBCASE("intersecting with it is empty, uniting with it is the other shape") {
        CHECK_FALSE(empty.intersection(rect).has_value());
        CHECK_FALSE(rect.intersection(empty).has_value());
        CHECK_FALSE(empty.intersection(segment).has_value());
        CHECK_FALSE(empty.intersection(Point(0, 0)).has_value());

        CHECK(empty.regularizedUnion<int>(rect) == rect.asPolygonSet());
        CHECK(rect.regularizedUnion<int>(empty) == rect.asPolygonSet());
        CHECK(empty.regularizedUnion<int>(empty).empty());

        CHECK(empty.minkowskiSum(rect).empty());
        CHECK(rect.minkowskiSum(empty).empty());
        CHECK(empty.minkowskiSum(Point(3, 3)).empty());
    }

    SUBCASE("it streams as an empty box") {
        std::ostringstream out;
        out << empty;
        CHECK(out.str() == "[]");
    }

    SUBCASE("it behaves the same wrapped in a Shape") {
        const pgl::Shape<Point> wrapped(empty);
        CHECK_FALSE(wrapped.contains(Point(0, 0)));
        CHECK_FALSE(wrapped.intersects(rect));
        CHECK(pgl::Shape<Point>(rect).contains(wrapped));
    }
}

TEST_CASE("Rectangle latticePoints pairs the integers of its two sides") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    CHECK(Rectangle(Point(0, 0), Point(2, 1)).latticePoints()
          == std::vector<Point>{{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 0}, {2, 1}});

    // Degenerate boxes fall back to the segment and the point they are.
    CHECK(Rectangle(Point(3, 1), Point(3, 3)).latticePoints()
          == std::vector<Point>{{3, 1}, {3, 2}, {3, 3}});
    CHECK(Rectangle(Point(5, 5), Point(5, 5)).latticePoints() == std::vector<Point>{{5, 5}});

    // Fractional corners keep the whole points between them, and a box that
    // spans no integer in one direction holds none at all.
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalRectangle = pgl::Rectangle<RationalPoint>;
    using Result = std::vector<pgl::Point<int64_t>>;
    CHECK(RationalRectangle(RationalPoint(Rational(1, 2), Rational(1, 2)),
                            RationalPoint(Rational(5, 2), Rational(3, 2)))
              .latticePoints()
          == Result{{1, 1}, {2, 1}});
    CHECK(RationalRectangle(RationalPoint(Rational(1, 3), Rational(0)),
                            RationalPoint(Rational(2, 3), Rational(9)))
              .latticePoints()
              .empty());
}

TEST_CASE("Rectangle latticePoints reaches the edge of the coordinate range and refuses the whole of it") {
    using Point = pgl::Point<int>;
    using Box = pgl::Rectangle<Point>;
    const int top = std::numeric_limits<int>::max();
    const int bottom = std::numeric_limits<int>::min();
    // The last integer of the range is reached, and not stepped past.
    const auto corner = Box(Point(top - 1, top - 1), Point(top, top)).latticePoints();
    REQUIRE(corner.size() == 4);
    CHECK(corner.back() == Point(top, top));
    CHECK(Box(Point(bottom, bottom), Point(bottom, bottom + 1)).latticePoints().size() == 2);
    // The whole range holds more points than a vector can, which is refused
    // rather than counted modulo the coordinate type.
    CHECK_THROWS_AS((void)Box(Point(bottom, bottom), Point(top, top)).latticePoints(), std::length_error);
}
