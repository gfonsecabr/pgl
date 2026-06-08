#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <cstdint>
#include "pgl.hpp"

TEST_CASE_TEMPLATE("Point builds the origin, exposes coordinates, and iterates over x then y", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;
    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Point origin;
    CHECK(origin.x() == Number(0));
    CHECK(origin.y() == Number(0));

    const Point point = make_point(static_cast<Number>(13), static_cast<Number>(7));
    CHECK(point[0] == Number(13));
    CHECK(point[1] == Number(7));

    Number coordinate_sum{};
    for (const auto coordinate : point) {
        coordinate_sum += coordinate;
    }

    CHECK(coordinate_sum == Number(20));

    if constexpr (requires { point.label(); }) {
        CHECK(point.label() == "tag");
    }
}

TEST_CASE("Point converts floating coordinates downward and copies or synthesizes labels") {
    const pgl::Point<double> source(13.5, 7.0);
    const pgl::Point<int> converted = source;

    CHECK(converted.x() == 13);
    CHECK(converted.y() == 7);

    const pgl::Point<int, std::string> labeled_source(2, 3, "test");
    const pgl::Point<int, std::string> copied = labeled_source;
    const pgl::Point<int> unlabeled = labeled_source;
    const pgl::Point<int, std::string> synthesized_from_unlabeled = pgl::Point<int>(5, 6);

    CHECK(copied.label() == "test");
    CHECK(unlabeled == pgl::Point<int>(2, 3));
    CHECK(synthesized_from_unlabeled.label().empty());
}

TEST_CASE("Point label can be modified directly") {
    pgl::Point<int, std::string> point(2, 3, "first");
    point.label() = "second";
    CHECK(point.label() == "second");
}

TEST_CASE("Point exposes a degenerate bounding box and empty edge sets") {
    using Point = pgl::Point<int>;
    const Point point(2, 3);

    const auto box = point.bbox();
    CHECK(box.min() == point);
    CHECK(box.max() == point);

    const auto vertices = point.vertices();
    CHECK(vertices.size() == 1);
    CHECK(vertices[0] == point);

    const auto edges = point.edges();
    CHECK(edges.empty());

    const auto oriented_edges = point.orientedEdges();
    CHECK(oriented_edges.empty());
}

TEST_CASE("Point arithmetic preserves point labels") {
    using Point = pgl::Point<int, std::string>;

    Point point(3, 5, "center");
    const Point scaled = 2 * point;
    const Point translated = point - pgl::Point<int>(1, 2);
    point += scaled;

    CHECK(point == Point(9, 15, "center"));
    CHECK(point.label() == "center");
    CHECK(scaled == Point(6, 10, "center"));
    CHECK(scaled.label() == "center");
    CHECK(translated == Point(2, 3, "center"));
    CHECK(translated.label() == "center");
}


TEST_CASE("Point arithmetic on different number types") {

    pgl::Point<int> pi(1, 2);
    pgl::Point<double> pd(3.5, 4.5);
    pgl::Point<double> pid = pi + pd;

    CHECK(pid.x() == 4.5);
    CHECK(pid.y() == 6.5);

    pgl::Point<pgl::Rational<>> pr(pgl::Rational(5.25), pgl::Rational(19l,3l));
    pgl::Point<pgl::Rational<>>  pir = pi + pr;
    CHECK(pir.x() == pgl::Rational(25l,4l));
    CHECK(pir.y() == pgl::Rational(25l,3l));

    pgl::Point<pgl::Rational<>> pdr = pd + pr;
    CHECK(pdr.x() == pgl::Rational(35l,4l));
    CHECK(pdr.y() == pgl::Rational(65l,6l));
}

TEST_CASE("Point equality, ordering, and hashing depend only on coordinates") {
    const pgl::Point<int, std::string> first(2, 3, "first");
    const pgl::Point<int, std::string> second(2, 3, "second");

    CHECK(first == second);
    CHECK_FALSE(first < second);
    CHECK_FALSE(second < first);

    std::set<pgl::Point<int>> plain_set;
    plain_set.insert({2, 3});
    plain_set.insert({2, 3});
    CHECK(plain_set.size() == 1);

    std::unordered_set<pgl::Point<int>> plain_unordered_set;
    plain_unordered_set.insert({2, 3});
    plain_unordered_set.insert({2, 3});
    CHECK(plain_unordered_set.size() == 1);

    std::set<pgl::Point<int, std::string>> labeled_set;
    labeled_set.insert(first);
    labeled_set.insert(second);
    CHECK(labeled_set.size() == 1);

    std::unordered_set<pgl::Point<int, std::string>> labeled_unordered_set;
    labeled_unordered_set.insert(first);
    labeled_unordered_set.insert(second);
    CHECK(labeled_unordered_set.size() == 1);
}

TEST_CASE("Point exposes exact and floating bounding boxes") {
    using Rational = pgl::Rational<int64_t>;

    const pgl::Point<int, std::string> labeled(2, 3, "p");
    const auto exact_box = labeled.bbox();

    CHECK(exact_box.min() == labeled);
    CHECK(exact_box.max() == labeled);
    CHECK(exact_box.min().label() == "p");
    CHECK(exact_box.max().label() == "p");

    const pgl::Point<Rational> rational(Rational(1, 3), Rational(5, 2));
    const auto floating_box = rational.fbox<double>();

    CHECK(floating_box.min().x() <= static_cast<double>(rational.x()));
    CHECK(floating_box.min().y() <= static_cast<double>(rational.y()));
    CHECK(static_cast<double>(rational.x()) <= floating_box.max().x());
    CHECK(static_cast<double>(rational.y()) <= floating_box.max().y());
}

TEST_CASE("Point shape predicates use coordinate equality and an empty boundary") {
    const pgl::Point<int, std::string> labeled(2, 3, "first");
    const pgl::Point<int, std::string> same_coordinates(2, 3, "second");
    const pgl::Point<double> same_coordinates_double(2.0, 3.0);
    const pgl::Point<double> different(2.0, 4.0);

    CHECK(labeled.contains(same_coordinates));
    CHECK(labeled.contains(same_coordinates_double));
    CHECK_FALSE(labeled.contains(different));

    CHECK_FALSE(labeled.boundaryContains(same_coordinates));
    CHECK_FALSE(labeled.boundaryContains(different));

    CHECK(labeled.interiorContains(same_coordinates));
    CHECK(labeled.interiorContains(same_coordinates_double));
    CHECK_FALSE(labeled.interiorContains(different));
}

TEST_CASE("Point contains only shapes that degenerate to the same point") {
    using Point = pgl::Point<int>;

    const Point point(2, 3);
    const pgl::Segment<Point> degenerate_segment({2, 3}, {2, 3});
    const pgl::Segment<Point> segment({2, 3}, {4, 5});
    const pgl::OrientedSegment<Point> degenerate_oriented_segment({2, 3}, {2, 3});
    const pgl::Line<Point> degenerate_line({2, 3}, {2, 3});
    const pgl::OrientedLine<Point> degenerate_oriented_line({2, 3}, {2, 3});
    const pgl::Ray<Point> degenerate_ray({2, 3}, {2, 3});
    const pgl::Halfplane<Point> degenerate_halfplane({2, 3}, {2, 3});
    const pgl::Rectangle<Point> point_rectangle({2, 3}, {2, 3});
    const pgl::Rectangle<Point> segment_rectangle({2, 3}, {4, 3});
    const pgl::Triangle<Point> point_triangle({2, 3}, {2, 3}, {2, 3});
    const pgl::Triangle<Point> degenerate_segment_triangle({2, 3}, {3, 3}, {4, 3});
    const pgl::Convex<Point> point_convex(std::vector<Point>{{2, 3}});
    const pgl::Convex<Point> segment_convex(std::vector<Point>{{2, 3}, {4, 5}});
    const pgl::Polygon<Point> point_polygon(std::vector<Point>{{2, 3}});
    const pgl::Polygon<Point> segment_polygon(std::vector<Point>{{2, 3}, {4, 5}});
    const pgl::Disk<Point> point_disk({2, 3}, {2, 3}, {2, 3});
    const pgl::Disk<Point> disk({1, 1}, {2, 5}, {4, 3});

    CHECK(point.contains(degenerate_segment));
    CHECK_FALSE(point.contains(segment));
    CHECK(point.contains(degenerate_oriented_segment));
    CHECK(point.contains(degenerate_line));
    CHECK(point.contains(degenerate_oriented_line));
    CHECK(point.contains(degenerate_ray));
    CHECK(point.contains(degenerate_halfplane));
    CHECK(point.contains(point_rectangle));
    CHECK_FALSE(point.contains(segment_rectangle));
    CHECK(point.contains(point_triangle));
    CHECK_FALSE(point.contains(degenerate_segment_triangle));
    CHECK(point.contains(point_convex));
    CHECK_FALSE(point.contains(segment_convex));
    CHECK(point.contains(point_polygon));
    CHECK_FALSE(point.contains(segment_polygon));

    CHECK_FALSE(point.boundaryContains(degenerate_segment));
    CHECK_FALSE(point.boundaryContains(degenerate_line));
    CHECK_FALSE(point.boundaryContains(point_rectangle));
    CHECK_FALSE(point.boundaryContains(point_triangle));

    CHECK(point.interiorContains(degenerate_segment));
    CHECK(point.interiorContains(degenerate_line));
    CHECK(point.interiorContains(point_rectangle));
    CHECK(point.interiorContains(point_triangle));
    CHECK(point.interiorContains(point_convex));
    CHECK_FALSE(point.interiorContains(segment_convex));
    CHECK(point.interiorContains(point_polygon));
    CHECK_FALSE(point.interiorContains(segment_polygon));
    CHECK(point.interiorContains(point_disk));
    CHECK_FALSE(point.interiorContains(disk));
}

TEST_CASE("Point intersects implemented shapes through their point-containment predicate") {
    using Point = pgl::Point<int>;

    const Point point(2, 2);
    const Point other_point(2, 2);
    const Point outside_point(2, 3);
    const pgl::Segment<Point> segment({0, 0}, {4, 4});
    const pgl::OrientedSegment<Point> oriented_segment({0, 0}, {4, 4});
    const pgl::Line<Point> line({0, 0}, {4, 4});
    const pgl::OrientedLine<Point> oriented_line({0, 0}, {4, 4});
    const pgl::Ray<Point> ray({0, 0}, {4, 4});
    const pgl::Halfplane<Point> halfplane({0, 0}, {4, 0});
    const pgl::Rectangle<Point> rectangle({1, 1}, {3, 3});
    const pgl::Triangle<Point> triangle({0, 0}, {4, 0}, {0, 4});

    CHECK(point.intersects(other_point));
    CHECK_FALSE(point.intersects(outside_point));
    CHECK(point.intersects(segment));
    CHECK(point.intersects(oriented_segment));
    CHECK(point.intersects(line));
    CHECK(point.intersects(oriented_line));
    CHECK(point.intersects(ray));
    CHECK(point.intersects(halfplane));
    CHECK(point.intersects(rectangle));
    CHECK(point.intersects(triangle));

    const Point clearly_outside(5, 5);
    CHECK_FALSE(clearly_outside.intersects(segment));
    CHECK_FALSE(clearly_outside.intersects(rectangle));
    CHECK_FALSE(clearly_outside.intersects(triangle));
}

TEST_CASE("Point intersection returns the point when it lies on the other shape") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;

    const Point point(2, 2);
    const Point other_point(2, 2);
    const pgl::Segment<Point> segment({0, 0}, {4, 4});
    const pgl::OrientedSegment<Point> oriented_segment({0, 0}, {4, 4});
    const pgl::Line<Point> line({0, 0}, {4, 4});
    const pgl::OrientedLine<Point> oriented_line({0, 0}, {4, 4});
    const pgl::Ray<Point> ray({0, 0}, {4, 4});
    const pgl::Halfplane<Point> halfplane({0, 0}, {4, 0});
    const pgl::Rectangle<Point> rectangle({1, 1}, {3, 3});
    const pgl::Triangle<Point> triangle({0, 0}, {4, 0}, {0, 4});

    CHECK(point.intersection(other_point) == Point(2, 2));
    CHECK(point.intersection(segment) == Point(2, 2));
    CHECK(point.intersection(oriented_segment) == Point(2, 2));
    CHECK(point.intersection(line) == Point(2, 2));
    CHECK(point.intersection(oriented_line) == Point(2, 2));
    CHECK(point.intersection(ray) == Point(2, 2));
    CHECK(point.intersection(halfplane) == Point(2, 2));
    CHECK(point.intersection(rectangle) == Point(2, 2));
    CHECK(point.intersection(triangle) == Point(2, 2));

    const auto rational_intersection = point.intersection<Rational>(segment);
    REQUIRE(rational_intersection);
    CHECK(*rational_intersection == pgl::Point<Rational>(Rational(2), Rational(2)));

    const Point outside(5, 5);
    CHECK_FALSE(outside.intersection(segment));
    CHECK_FALSE(outside.intersection(rectangle));
    CHECK_FALSE(outside.intersection(triangle));
}

TEST_CASE("Point separates linear primitives only when it removes an interior point") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Ray = pgl::Ray<Point>;

    const Point midpoint(2, 0);
    const Point endpoint(0, 0);
    const Segment segment({0, 0}, {4, 0});
    const Line line({0, 0}, {4, 0});
    const Ray ray({0, 0}, {4, 0});

    CHECK(midpoint.separates(segment));
    CHECK(midpoint.separates(line));
    CHECK(midpoint.separates(ray));

    CHECK_FALSE(endpoint.separates(segment));
    CHECK_FALSE(endpoint.separates(ray));
    CHECK_FALSE(Point(1, 1).separates(line));
    CHECK_FALSE(midpoint.crosses(segment));
}

TEST_CASE("Point never crosses any implemented shape") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Point point(2, 0);

    CHECK_FALSE(point.crosses(Point(2, 0)));
    CHECK_FALSE(point.crosses(Segment({0, 0}, {4, 0})));
    CHECK_FALSE(point.crosses(OrientedSegment({0, 0}, {4, 0})));
    CHECK_FALSE(point.crosses(Line({0, 0}, {4, 0})));
    CHECK_FALSE(point.crosses(OrientedLine({0, 0}, {4, 0})));
    CHECK_FALSE(point.crosses(Ray({0, 0}, {4, 0})));
    CHECK_FALSE(point.crosses(Halfplane({0, 0}, {4, 0})));
    CHECK_FALSE(point.crosses(Rectangle({1, -1}, {3, 1})));
    CHECK_FALSE(point.crosses(Triangle({0, 0}, {4, 0}, {2, 3})));
}

TEST_CASE_TEMPLATE("Point supports negation, scaling, translation, and subtraction", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<>>) {
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const Point point = [] {
        if constexpr (std::same_as<Number, int>) {
            return Point(13, 7);
        } else {
            return Point(Number(27)/2, 7);
        }
    }();
    const auto opposite = -point;
    const auto scaled = 2 * point;
    const auto shifted = scaled + pgl::Point<int, std::string>(2, 3, "test");
    const auto translated_back = shifted - pgl::Point<int>(1, 2);

    CHECK(opposite.x() == Number(-point.x()));
    CHECK(opposite.y() == Number(-7));
    CHECK(scaled.x() == Number(2 * point.x()));
    CHECK(scaled.y() == Number(14));
    CHECK(shifted.x() == Number(2 * point.x() + 2));
    CHECK(shifted.y() == Number(17));
    CHECK(translated_back.x() == Number(2 * point.x() + 1));
    CHECK(translated_back.y() == Number(15));
}

TEST_CASE_TEMPLATE("Point supports scaling, translation, and subtraction in place", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<>>) {
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const Point point = [] {
        if constexpr (std::same_as<Number, int>) {
            return Point(13, 7);
        } else {
            return Point(Number(27)/2, 7);
        }
    }();
    auto scaled = point;
    scaled *= 2;
    auto shifted = scaled;
    shifted += pgl::Point<int, std::string>(2, 3, "test");
    auto translated_back = shifted;
    translated_back -= pgl::Point<int>(1, 2);

    CHECK(scaled.x() == Number(2 * point.x()));
    CHECK(scaled.y() == Number(14));
    CHECK(shifted.x() == Number(2 * point.x() + 2));
    CHECK(shifted.y() == Number(17));
    CHECK(translated_back.x() == Number(2 * point.x() + 1));
    CHECK(translated_back.y() == Number(15));
}

TEST_CASE_TEMPLATE("Point computes dot product and Euclidean, L1, and Linf distances", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<>>) {
    const Point first(1, 2);
    const Point second(3, 4);

    CHECK(first * second == 11);
    CHECK(first.squaredDistance(second) == 8);
    CHECK(first.template distance<double>(second) == doctest::Approx(std::sqrt(8.0)));
    CHECK(first.distanceL1(second) == 4);
    CHECK(first.distanceLInf(second) == 2);
}

TEST_CASE("Point with rational coordinates preserves exact fractions under scaling and division") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;

    const Point point(Rational(1, 2), Rational(3, 4));
    const auto scaled = point * Rational(4, 3);
    const auto divided = point / Rational(2);
    const auto translated = point + Point(Rational(1, 6), Rational(1, 4));

    CHECK(scaled.x() == Rational(2, 3));
    CHECK(scaled.y() == Rational(1));
    CHECK(divided.x() == Rational(1, 4));
    CHECK(divided.y() == Rational(3, 8));
    CHECK(translated.x() == Rational(2, 3));
    CHECK(translated.y() == Rational(1));
}

TEST_CASE("Point streams as coordinates or label colon coordinates") {
    {
        std::ostringstream plain_stream;
        plain_stream << pgl::Point<double>(13.5, -7.0);
        CHECK(plain_stream.str() == "(13.5,-7)");
    }

    {
        std::ostringstream plain_stream;
        plain_stream << pgl::Point<int>(123, -456);
        CHECK(plain_stream.str() == "(123,-456)");
    }

#if defined(__SIZEOF_INT128__)
    {
        std::ostringstream plain_stream;
        plain_stream << pgl::Point<__int128_t>(123, -456);
        CHECK(plain_stream.str() == "(123,-456)");
    }
#endif

    {
        std::ostringstream plain_stream;
        plain_stream << pgl::Point<pgl::Rational<int>>(123, -456)/2;
        CHECK(plain_stream.str() == "(123/2,-228)");
    }

    {
        std::ostringstream plain_stream;
        plain_stream << pgl::Point<int, std::string>(2, 3, "test");
        CHECK(plain_stream.str() == "test:(2,3)");
    }
}

TEST_CASE_TEMPLATE("Point-line duality", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<>>) {
    const Point p0{0,0}, p1{3,7}, p2{2,-5}, p3{-4,2};

    CHECK(p0.dual().dual() == p0);
    CHECK(p1.dual().dual() == p1);
    CHECK(p2.dual().dual() == p2);
    CHECK(p3.dual().dual() == p3);
}

TEST_CASE("Point-line polarity") {
    using Point = pgl::Point<pgl::Rational<>>;
    const Point p0{1,1},p1{3,7}, p2{2,-5}, p3{-4,2};

    CHECK(p0.polar().polar() == p0);
    CHECK(p1.polar().polar() == p1);
    CHECK(p2.polar().polar() == p2);
    CHECK(p3.polar().polar() == p3);
}

TEST_CASE("Point rotated90 and rotate90") {
    using Point = pgl::Point<int>;
    const Point p{3, 1};

    CHECK(p.rotated90(0) == Point( 3,  1));
    CHECK(p.rotated90(1) == Point(-1,  3));
    CHECK(p.rotated90(2) == Point(-3, -1));
    CHECK(p.rotated90(3) == Point( 1, -3));

    CHECK(p.rotated90(4)  == p.rotated90(0));
    CHECK(p.rotated90(-1) == p.rotated90(3));
    CHECK(p.rotated90(-3) == p.rotated90(1));

    Point q{3, 1};
    q.rotate90();
    CHECK(q == Point(-1, 3));
    q.rotate90(3);
    CHECK(q == Point(3, 1));

    pgl::Point<int, std::string> labeled{3, 1, "a"};
    labeled.rotate90();
    CHECK(labeled == pgl::Point<int, std::string>(-1, 3, "a"));
    CHECK(labeled.label() == "a");
}

TEST_CASE("Point per-axis scaling up and down") {
    using Point = pgl::Point<int>;
    const Point p{3, 5};

    // Returning forms leave the original untouched and touch only one axis.
    CHECK(p.scaledUpX(2) == Point(6, 5));
    CHECK(p.scaledUpY(2) == Point(3, 10));
    CHECK(p.scaledDownX(3) == Point(1, 5));
    CHECK(p.scaledDownY(5) == Point(3, 1));
    CHECK(p == Point(3, 5));

    // In-place forms mutate and preserve the label.
    pgl::Point<int, std::string> q{4, 6, "tag"};
    q.scaleUpX(2);
    CHECK(q == pgl::Point<int, std::string>(8, 6, "tag"));
    q.scaleUpY(3);
    CHECK(q.y() == 18);
    q.scaleDownX(4);
    CHECK(q.x() == 2);
    q.scaleDownY(2);
    CHECK(q.y() == 9);
    CHECK(q.label() == "tag");
}
