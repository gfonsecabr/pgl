#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

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

    const Rectangle degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.min() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.max() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.min() == Point(Number{}, Number{}));
        CHECK(degenerate.max() == Point(Number{}, Number{}));
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
    CHECK(rectangle.max() == pgl::Point<int>(0, 0));
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

TEST_CASE("Rectangle bounding-box constructor rejects an empty range") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const std::vector<Point> empty_points;
    CHECK_THROWS_AS((void)Rectangle(empty_points), std::invalid_argument);
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
    CHECK(rectangle.pointInside() == Point(Number(3), Number(2)));
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

    CHECK(outer.contains(Point(2, 2)));
    CHECK(outer.contains(Point(4, 3)));
    CHECK_FALSE(outer.contains(Point(5, 2)));
    CHECK(outer.interiorContains(Point(2, 2)));
    CHECK_FALSE(outer.interiorContains(Point(4, 3)));
    CHECK(outer.boundaryContains(Point(4, 2)));
    CHECK_FALSE(outer.boundaryContains(Point(2, 2)));
    CHECK(outer.verticesContain(Point(0, 0)));
    CHECK_FALSE(outer.verticesContain(Point(0, 1)));

    const auto inside_intersection = outer.intersection(Point(2, 2));
    REQUIRE(inside_intersection);
    CHECK(*inside_intersection == Point(2, 2));

    const auto boundary_intersection = outer.intersection(Point(4, 3));
    REQUIRE(boundary_intersection);
    CHECK(*boundary_intersection == Point(4, 3));

    CHECK_FALSE(outer.intersection(Point(5, 2)).has_value());
    CHECK(outer.intersects(Point(2, 2)));
    CHECK(outer.intersects(Point(4, 3)));
    CHECK_FALSE(outer.intersects(Point(5, 2)));

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

    const Rectangle rectangle({0, 0}, {4, 3});
    const Segment boundary_segment({1, 0}, {3, 0});
    const Segment diagonal({0, 0}, {4, 3});

    CHECK(rectangle.boundaryContains(boundary_segment));
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

TEST_CASE("Rectangle predicates handle linear primitives and halfplanes") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});
    const Segment inner_segment({1, 1}, {3, 2});
    const Segment crossing_segment({-1, 1}, {5, 1});
    const Segment touching_segment({4, 1}, {6, 1});
    const Segment boundary_endpoint_only_segment({-1, 0}, {0, 1});
    const OrientedSegment crossing_oriented({-1, 2}, {5, 2});
    const OrientedSegment boundary_endpoint_only_oriented({0, 1}, {-1, 0});
    const Line crossing_line({-1, 1}, {5, 1});
    const Line tangent_line({0, 0}, {4, 0});
    const Line corner_line({-1, 1}, {1, -1});
    const Line outside_line({0, 5}, {4, 5});
    const OrientedLine crossing_oriented_line({2, -1}, {2, 4});
    const OrientedLine tangent_oriented_line({4, 0}, {0, 0});
    const OrientedLine corner_oriented_line({1, -1}, {-1, 1});
    const Ray crossing_ray({-2, 1}, {2, 1});
    const Ray corner_entry_ray({-1, 0}, {0, 1});
    const Ray source_inside_ray({2, 1}, {8, 1});
    const Ray boundary_entering_ray({0, 1}, {2, 1});
    const Ray boundary_leaving_ray({0, 1}, {-2, 1});
    const Ray edge_following_ray({0, 0}, {4, 0});
    const Ray corner_tangent_ray({-1, 1}, {1, -1});
    const Ray outside_ray({-2, 5}, {2, 5});
    const Halfplane upper({0, 0}, {4, 0});
    const Halfplane tangent_upper({0, 3}, {4, 3});
    const Halfplane outside_upper({0, 5}, {4, 5});

    CHECK(rectangle.contains(inner_segment));
    CHECK(rectangle.interiorContains(inner_segment));
    CHECK_FALSE(rectangle.contains(crossing_segment));
    CHECK_FALSE(rectangle.contains(crossing_line));
    CHECK(rectangle.contains(Segment({1, 0}, {3, 0})));
    CHECK_FALSE(rectangle.interiorContains(Segment({1, 0}, {3, 0})));

    CHECK(rectangle.intersects(crossing_segment));
    CHECK(rectangle.intersects(crossing_oriented));
    CHECK(rectangle.intersects(touching_segment));
    CHECK(rectangle.intersects(boundary_endpoint_only_segment));
    CHECK(rectangle.intersects(boundary_endpoint_only_oriented));
    CHECK(rectangle.intersects(crossing_line));
    CHECK(rectangle.intersects(crossing_oriented_line));
    CHECK(rectangle.intersects(tangent_line));
    CHECK(rectangle.intersects(corner_line));
    CHECK(rectangle.intersects(tangent_oriented_line));
    CHECK(rectangle.intersects(corner_oriented_line));
    CHECK_FALSE(rectangle.intersects(outside_line));
    CHECK(rectangle.intersects(crossing_ray));
    CHECK(rectangle.intersects(corner_entry_ray));
    CHECK(rectangle.intersects(source_inside_ray));
    CHECK(rectangle.intersects(boundary_entering_ray));
    CHECK(rectangle.intersects(boundary_leaving_ray));
    CHECK(rectangle.intersects(edge_following_ray));
    CHECK(rectangle.intersects(corner_tangent_ray));
    CHECK_FALSE(rectangle.intersects(outside_ray));

    CHECK(rectangle.interiorsIntersect(crossing_segment));
    CHECK(rectangle.interiorsIntersect(crossing_oriented));
    CHECK_FALSE(rectangle.interiorsIntersect(touching_segment));
    CHECK_FALSE(rectangle.interiorsIntersect(boundary_endpoint_only_segment));
    CHECK_FALSE(rectangle.interiorsIntersect(boundary_endpoint_only_oriented));
    CHECK(rectangle.interiorsIntersect(crossing_line));
    CHECK(rectangle.interiorsIntersect(crossing_oriented_line));
    CHECK_FALSE(rectangle.interiorsIntersect(tangent_line));
    CHECK_FALSE(rectangle.interiorsIntersect(corner_line));
    CHECK_FALSE(rectangle.interiorsIntersect(tangent_oriented_line));
    CHECK_FALSE(rectangle.interiorsIntersect(corner_oriented_line));
    CHECK_FALSE(rectangle.interiorsIntersect(outside_line));
    CHECK(rectangle.interiorsIntersect(crossing_ray));
    CHECK(rectangle.interiorsIntersect(corner_entry_ray));
    CHECK(rectangle.interiorsIntersect(source_inside_ray));
    CHECK(rectangle.interiorsIntersect(boundary_entering_ray));
    CHECK_FALSE(rectangle.interiorsIntersect(boundary_leaving_ray));
    CHECK_FALSE(rectangle.interiorsIntersect(edge_following_ray));
    CHECK_FALSE(rectangle.interiorsIntersect(corner_tangent_ray));
    CHECK_FALSE(rectangle.interiorsIntersect(outside_ray));

    CHECK(rectangle.separates(crossing_segment));
    CHECK(rectangle.crosses(crossing_segment));
    CHECK(rectangle.separates(crossing_oriented));
    CHECK(rectangle.crosses(crossing_oriented));
    CHECK_FALSE(rectangle.separates(touching_segment));
    CHECK(rectangle.separates(crossing_line));
    CHECK(rectangle.crosses(crossing_line));
    CHECK(rectangle.separates(crossing_oriented_line));
    CHECK(rectangle.crosses(crossing_oriented_line));
    CHECK(rectangle.separates(tangent_line));
    CHECK_FALSE(rectangle.crosses(tangent_line));
    CHECK(rectangle.separates(crossing_ray));
    CHECK(rectangle.crosses(crossing_ray));
    CHECK_FALSE(rectangle.separates(source_inside_ray));
    CHECK_FALSE(rectangle.separates(boundary_entering_ray));

    CHECK(rectangle.intersects(upper));
    CHECK(rectangle.interiorsIntersect(upper));
    CHECK(rectangle.intersects(tangent_upper));
    CHECK_FALSE(rectangle.interiorsIntersect(tangent_upper));
    CHECK_FALSE(rectangle.intersects(outside_upper));
    CHECK_FALSE(rectangle.contains(upper));
}

TEST_CASE("Rectangle interior intersection with triangles ignores boundary-only contact") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Rectangle rectangle({0, 0}, {4, 4});
    const Triangle touching_corner({4, 4}, {5, 4}, {4, 5});
    const Triangle crossing({2, -1}, {5, 2}, {2, 5});

    CHECK(rectangle.intersects(touching_corner));
    CHECK_FALSE(rectangle.interiorsIntersect(touching_corner));
    CHECK(rectangle.interiorsIntersect(crossing));
}

TEST_CASE("Rectangle covers the non-Convex contract for interiorsIntersect") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    CHECK(rectangle.interiorsIntersect(Point(2, 1)));  // interior point
    CHECK_FALSE(rectangle.interiorsIntersect(Point(0, 0)));  // corner (boundary)
    CHECK(rectangle.interiorsIntersect(Line({-1, 1}, {5, 1})));
    CHECK(rectangle.interiorsIntersect(OrientedLine({2, -1}, {2, 4})));
    CHECK(rectangle.interiorsIntersect(Segment({-1, 1}, {5, 1})));
    CHECK(rectangle.interiorsIntersect(OrientedSegment({-1, 2}, {5, 2})));
    CHECK(rectangle.interiorsIntersect(Ray({-2, 1}, {2, 1})));
    CHECK(rectangle.interiorsIntersect(Halfplane({0, 0}, {4, 0})));
    CHECK(rectangle.interiorsIntersect(Triangle({1, 1}, {5, 1}, {2, 4})));
    CHECK(rectangle.interiorsIntersect(Shape(Rectangle({1, 1}, {3, 2}))));
}

TEST_CASE("Rectangle covers the non-Convex contract for separates") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    CHECK_FALSE(rectangle.separates(Point(2, 1)));
    CHECK(rectangle.separates(Line({-1, 1}, {5, 1})));
    CHECK(rectangle.separates(OrientedLine({2, -1}, {2, 4})));
    CHECK(rectangle.separates(Segment({-1, 1}, {5, 1})));
    CHECK(rectangle.separates(OrientedSegment({-1, 2}, {5, 2})));
    CHECK(rectangle.separates(Ray({-2, 1}, {2, 1})));
    CHECK_FALSE(rectangle.separates(Halfplane({0, 0}, {4, 0})));
    CHECK(rectangle.separates(Triangle({1, -1}, {3, -1}, {2, 4})));
    CHECK(rectangle.separates(Shape(Segment({-1, 1}, {5, 1}))));
}

TEST_CASE("Rectangle covers the non-Convex contract for crosses") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    CHECK_FALSE(rectangle.crosses(Point(2, 1)));
    CHECK(rectangle.crosses(Line({-1, 1}, {5, 1})));
    CHECK(rectangle.crosses(OrientedLine({2, -1}, {2, 4})));
    CHECK(rectangle.crosses(Segment({-1, 1}, {5, 1})));
    CHECK(rectangle.crosses(OrientedSegment({-1, 2}, {5, 2})));
    CHECK(rectangle.crosses(Ray({-2, 1}, {2, 1})));
    CHECK_FALSE(rectangle.crosses(Halfplane({0, 0}, {4, 0})));
    CHECK(rectangle.crosses(Triangle({1, -1}, {3, -1}, {2, 4})));
    CHECK(rectangle.crosses(Shape(Segment({-1, 1}, {5, 1}))));
}

TEST_CASE("Linear primitives separate rectangles only when clipped through the interior") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    const Segment side_to_side({-1, 1}, {5, 1});
    const Segment corner_to_corner({0, 0}, {4, 3});
    const Segment starts_inside({2, 1}, {5, 1});
    const Segment along_edge({0, 0}, {4, 0});

    CHECK(side_to_side.separates(rectangle));
    CHECK(side_to_side.crosses(rectangle));
    CHECK(OrientedSegment({5, 1}, {-1, 1}).separates(rectangle));
    CHECK(corner_to_corner.separates(rectangle));

    CHECK_FALSE(starts_inside.separates(rectangle));
    CHECK_FALSE(along_edge.separates(rectangle));

    CHECK(Line({-1, 1}, {5, 1}).separates(rectangle));
    CHECK(OrientedLine({5, 1}, {-1, 1}).separates(rectangle));
    CHECK(Ray({-1, 1}, {5, 1}).separates(rectangle));
    CHECK_FALSE(Ray({2, 1}, {5, 1}).separates(rectangle));
    CHECK_FALSE(Line({0, 0}, {4, 0}).separates(rectangle));
}

TEST_CASE("Rectangles can separate triangles when they form a genuine barrier") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle({0, 0}, {6, 0}, {0, 6});
    const Rectangle vertical_bar({2, 0}, {3, 4});
    const Rectangle corner_box({0, 0}, {1, 1});

    CHECK(vertical_bar.separates(triangle));
    CHECK_FALSE(corner_box.separates(triangle));
}

TEST_CASE("Rectangle intersections return clipped shapes") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rectangle({0, 0}, {4, 3});

    const auto rectangle_overlap = rectangle.intersection(Rectangle({2, 1}, {6, 5}));
    REQUIRE(rectangle_overlap);
    CHECK(*rectangle_overlap == Rectangle({2, 1}, {4, 3}));

    const auto rectangle_touch = rectangle.intersection(Rectangle({4, 1}, {6, 2}));
    REQUIRE(rectangle_touch);
    CHECK(*rectangle_touch == Rectangle({4, 1}, {4, 2}));

    CHECK_FALSE(rectangle.intersection(Rectangle({5, 4}, {6, 5})));

    const auto crossing_segment = rectangle.intersection(Segment({-1, 1}, {5, 1}));
    REQUIRE(crossing_segment);
    REQUIRE(std::holds_alternative<Segment>(*crossing_segment));
    CHECK(std::get<Segment>(*crossing_segment) == Segment({0, 1}, {4, 1}));

    const auto inner_segment = rectangle.intersection(Segment({1, 1}, {3, 2}));
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

    const auto touching_segment = rectangle.intersection(Segment({4, 1}, {6, 1}));
    REQUIRE(touching_segment);
    REQUIRE(std::holds_alternative<Point>(*touching_segment));
    CHECK(std::get<Point>(*touching_segment) == Point(4, 1));

    CHECK_FALSE(rectangle.intersection(Segment({5, 4}, {6, 4})));

    const auto oriented_segment = rectangle.intersection(OrientedSegment({-1, 2}, {5, 2}));
    REQUIRE(oriented_segment);
    REQUIRE(std::holds_alternative<Segment>(*oriented_segment));
    CHECK(std::get<Segment>(*oriented_segment) == Segment({0, 2}, {4, 2}));

    const auto crossing_line = rectangle.intersection(Line({-1, 1}, {5, 1}));
    REQUIRE(crossing_line);
    REQUIRE(std::holds_alternative<Segment>(*crossing_line));
    CHECK(std::get<Segment>(*crossing_line) == Segment({0, 1}, {4, 1}));

    const auto tangent_line = rectangle.intersection(Line({0, 0}, {4, 0}));
    REQUIRE(tangent_line);
    REQUIRE(std::holds_alternative<Segment>(*tangent_line));
    CHECK(std::get<Segment>(*tangent_line) == Segment({0, 0}, {4, 0}));

    const auto vertex_line = rectangle.intersection(Line({-1, 1}, {1, -1}));
    REQUIRE(vertex_line);
    REQUIRE(std::holds_alternative<Point>(*vertex_line));
    CHECK(std::get<Point>(*vertex_line) == Point(0, 0));

    const auto oriented_line = rectangle.intersection(OrientedLine({2, -1}, {2, 4}));
    REQUIRE(oriented_line);
    REQUIRE(std::holds_alternative<Segment>(*oriented_line));
    CHECK(std::get<Segment>(*oriented_line) == Segment({2, 0}, {2, 3}));

    CHECK_FALSE(rectangle.intersection(Line({0, 5}, {4, 5})));

    const auto crossing_ray = rectangle.intersection(Ray({-2, 1}, {2, 1}));
    REQUIRE(crossing_ray);
    REQUIRE(std::holds_alternative<Segment>(*crossing_ray));
    CHECK(std::get<Segment>(*crossing_ray) == Segment({0, 1}, {4, 1}));

    const auto source_inside_ray = rectangle.intersection(Ray({2, 1}, {8, 1}));
    REQUIRE(source_inside_ray);
    REQUIRE(std::holds_alternative<Segment>(*source_inside_ray));
    CHECK(std::get<Segment>(*source_inside_ray) == Segment({2, 1}, {4, 1}));

    const auto tangent_ray = rectangle.intersection(Ray({-2, 0}, {2, 0}));
    REQUIRE(tangent_ray);
    REQUIRE(std::holds_alternative<Segment>(*tangent_ray));
    CHECK(std::get<Segment>(*tangent_ray) == Segment({0, 0}, {4, 0}));

    CHECK_FALSE(rectangle.intersection(Ray({-2, 5}, {2, 5})));
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

    CHECK(rectangle.squaredDistance(Point(2, 2)) == 0);
    CHECK(rectangle.squaredDistance(Point(6, 5)) == 8);
    CHECK(rectangle.squaredDistance(inner) == 0);
    CHECK(rectangle.squaredDistance(touching) == 0);
    CHECK(rectangle.squaredDistance(disjoint) == 8);
    CHECK(rectangle.squaredHausdorffDistance(inner) == 2);
    CHECK(rectangle.squaredDistance(Line({0, 5}, {4, 5})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance(OrientedLine({6, 0}, {6, 3})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance(Segment({6, 1}, {8, 1})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance(OrientedSegment({2, 5}, {3, 5})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance(Ray({6, 1}, {8, 1})) == doctest::Approx(4.0));
    CHECK(rectangle.squaredDistance(Ray({6, 1}, {2, 1})) == doctest::Approx(0.0));
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
