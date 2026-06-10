#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "pgl.hpp"


TEST_CASE_TEMPLATE("OrientedSegment preserves source and target order while exposing min and max", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const OrientedSegment degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.source() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.target() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.source() == Point(Number{}, Number{}));
        CHECK(degenerate.target() == Point(Number{}, Number{}));
    }

    const OrientedSegment segment(
        make_point(static_cast<Number>(4), static_cast<Number>(3), "a"),
        make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));

    CHECK(segment[0].x() == Number(4));
    CHECK(segment[0].y() == Number(3));
    CHECK(segment[1].x() == Number(2));
    CHECK(segment[1].y() == Number(1));
    CHECK(segment.source() == segment[0]);
    CHECK(segment.target() == segment[1]);
    CHECK(segment.min().x() == Number(2));
    CHECK(segment.min().y() == Number(1));
    CHECK(segment.max().x() == Number(4));
    CHECK(segment.max().y() == Number(3));

    std::array<Point, 2> iterated{};
    std::size_t index = 0;
    for (const auto& point : segment) {
        iterated[index++] = point;
    }
    CHECK(index == 2);
    CHECK(iterated[0] == segment.source());
    CHECK(iterated[1] == segment.target());

    if constexpr (requires { segment.source().label(); }) {
        CHECK(segment.source().label() == "a");
        CHECK(segment.target().label() == "b");
        CHECK(segment.min().label() == "b");
        CHECK(segment.max().label() == "a");
    }
}

TEST_CASE("OrientedSegment streams, flips, scales, translates, and converts to Segment") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const OrientedSegment segment(4, 3, 2, 1);
    std::ostringstream stream;
    stream << segment;
    CHECK(stream.str() == "(4,3)->(2,1)");

    const auto opposite = segment.opposite();
    CHECK(opposite.source() == Point(2, 1));
    CHECK(opposite.target() == Point(4, 3));

    const auto scaled = 2 * segment;
    const auto translated = Point(4, 3) + segment;
    const auto shifted = translated - Point(1, 1);

    CHECK(scaled.source() == Point(8, 6));
    CHECK(scaled.target() == Point(4, 2));
    CHECK(translated.source() == Point(8, 6));
    CHECK(translated.target() == Point(6, 4));
    CHECK(shifted.source() == Point(7, 5));
    CHECK(shifted.target() == Point(5, 3));

    const Segment unordered(segment);
    CHECK(unordered.min() == Point(2, 1));
    CHECK(unordered.max() == Point(4, 3));
    CHECK(segment.diameter() == unordered);
    CHECK(segment.bbox() == unordered.bbox());

    const auto floating_box = segment.fbox();
    CHECK(floating_box.min() == pgl::Point<double>(2.0, 1.0));
    CHECK(floating_box.max() == pgl::Point<double>(4.0, 3.0));

    const OrientedLine line(segment);
    CHECK(line.source() == Point(4, 3));
    CHECK(line.target() == Point(2, 1));

    const Ray ray(segment);
    CHECK(ray.source() == Point(4, 3));
    CHECK(ray.target() == Point(2, 1));
}

TEST_CASE("OrientedSegment exposes a bounding box, vertices, and edge views") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment segment(4, 3, 2, 1);

    const auto box = segment.bbox();
    CHECK(box.min() == Point(2, 1));
    CHECK(box.max() == Point(4, 3));

    const auto vertices = segment.vertices();
    CHECK(vertices.size() == 2);
    CHECK(vertices[0] == Point(4, 3));
    CHECK(vertices[1] == Point(2, 1));

    const auto edges = segment.edges();
    CHECK(edges.size() == 1);
    CHECK(edges[0] == Segment(Point(2, 1), Point(4, 3)));

    const auto oriented_edges = segment.orientedEdges();
    CHECK(oriented_edges.size() == 1);
    CHECK(oriented_edges[0] == segment);
}

TEST_CASE("OrientedSegment evaluates coordinates by delegating to Segment") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Rational = pgl::Rational<int64_t>;

    const OrientedSegment diagonal(Point(4, 4), Point(0, 0));
    CHECK(diagonal.yAtX(2) == 2);
    CHECK(diagonal.xAtY(3) == 3);
    CHECK(diagonal.yAtX<Rational>(1).value() == Rational(1));
    CHECK(diagonal.xAtY<Rational>(1).value() == Rational(1));

    const OrientedSegment descending(Point(4, 0), Point(0, 4));
    CHECK(descending.yAtX(1) == 3);
    CHECK(descending.xAtY(3) == 1);
    CHECK_FALSE(descending.xAtY(5).has_value());

    const OrientedSegment vertical(Point(2, 3), Point(2, -1));
    CHECK(vertical.yAtX(2) == -1);
    CHECK_FALSE(vertical.yAtX(1).has_value());
    CHECK(vertical.xAtY(0) == 2);

    const OrientedSegment horizontal(Point(4, 1), Point(-2, 1));
    CHECK(horizontal.xAtY(1) == -2);
    CHECK_FALSE(horizontal.xAtY(0).has_value());
    CHECK(horizontal.yAtX(3) == 1);
}

TEST_CASE("OrientedSegment converts between labeled and unlabeled endpoints") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainSegment = pgl::OrientedSegment<PlainPoint>;
    using LabelSegment = pgl::OrientedSegment<LabelPoint>;

    const LabelSegment labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainSegment plain_source(4, 3, 2, 1);

    const PlainSegment plain_from_labeled = labeled;
    const LabelSegment labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.source() == PlainPoint(4, 3));
    CHECK(plain_from_labeled.target() == PlainPoint(2, 1));
    CHECK(labeled_from_plain.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.source().label().empty());
    CHECK(labeled_from_plain.target().label().empty());

    PlainSegment plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.source() == PlainPoint(4, 3));
    CHECK(plain_assigned.target() == PlainPoint(2, 1));

    LabelSegment labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.source().label().empty());
    CHECK(labeled_assigned.target().label().empty());
}

TEST_CASE("OrientedSegment exposes axis tests, endpoint aliases, orientation, slope, collinearity,  parallelism, and point inside") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const OrientedSegment degenerate({2, 2}, {2, 2});
    const OrientedSegment horizontal({4, 0}, {0, 0});
    const OrientedSegment vertical({3, -1}, {3, 6});
    const OrientedSegment diagonal({0, 0}, {4, 4});
    const OrientedSegment descending({4, 0}, {0, 4});
    const Segment parallel_segment({1, 1}, {5, 5});
    const OrientedSegment parallel_oriented_segment({5, 5}, {1, 1});
    const Segment non_parallel_segment({0, 0}, {4, 1});
    const Line parallel_line({0, 1}, {4, 5});
    const OrientedLine parallel_oriented_line({5, 6}, {1, 2});
    const Ray parallel_ray({2, 2}, {5, 5});

    CHECK(degenerate.isDegenerate());
    CHECK_FALSE(horizontal.isDegenerate());

    CHECK(horizontal.isHorizontal());
    CHECK_FALSE(horizontal.isVertical());
    CHECK(vertical.isVertical());
    CHECK_FALSE(vertical.isHorizontal());

    CHECK(diagonal.containsEndpoint(Point(0, 0)));
    CHECK(diagonal.containsEndpoint(Point(4, 4)));
    CHECK_FALSE(diagonal.containsEndpoint(Point(2, 2)));

    CHECK(diagonal.collinear(Point(2, 2)));
    CHECK_FALSE(diagonal.collinear(Point(2, 3)));
    CHECK(diagonal.collinear(parallel_segment));
    CHECK(diagonal.collinear(parallel_oriented_segment));
    CHECK_FALSE(diagonal.collinear(descending));

    CHECK(diagonal.slope<double>() == doctest::Approx(1.0));
    CHECK(descending.slope<double>() == doctest::Approx(-1.0));
    CHECK(horizontal.slope<double>() == doctest::Approx(0.0));

    CHECK(diagonal.orientation(Point(1, 2)) == std::partial_ordering::greater);
    CHECK(diagonal.orientation(Point(2, 1)) == std::partial_ordering::less);
    CHECK(diagonal.orientation(Point(3, 3)) == std::partial_ordering::equivalent);

    CHECK(diagonal.parallel(parallel_segment));
    CHECK(diagonal.parallel(parallel_oriented_segment));
    CHECK(diagonal.parallel(parallel_line));
    CHECK(diagonal.parallel(parallel_oriented_line));
    CHECK(diagonal.parallel(parallel_ray));
    CHECK_FALSE(diagonal.parallel(non_parallel_segment));

    CHECK(horizontal.interiorContains(horizontal.pointInside<float>()));
    CHECK(vertical.interiorContains(vertical.pointInside<float>()));
    CHECK(diagonal.interiorContains(diagonal.pointInside<float>()));
    CHECK(descending.interiorContains(descending.pointInside<float>()));
}

TEST_CASE("OrientedSegment builds orientation-dependent half-planes") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment segment({4, 4}, {0, 0});

    CHECK(segment.leftHalfplane().contains(Point(1, 0)));
    CHECK_FALSE(segment.leftHalfplane().contains(Point(0, 1)));

    CHECK(segment.rightHalfplane().contains(Point(0, 1)));
    CHECK_FALSE(segment.rightHalfplane().contains(Point(1, 0)));
}

TEST_CASE("OrientedSegment supports direct endpoint mutation and in-place transforms") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    OrientedSegment segment(4, 3, 2, 1);
    segment.source().x() = 5;
    segment[1].y() = 0;
    CHECK(segment.source() == Point(5, 3));
    CHECK(segment.target() == Point(2, 0));

    segment += Point(1, 1);
    CHECK(segment.source() == Point(6, 4));
    CHECK(segment.target() == Point(3, 1));

    segment *= -1;
    CHECK(segment.source() == Point(-6, -4));
    CHECK(segment.target() == Point(-3, -1));
}

TEST_CASE("OrientedSegment reports exact midpoints and exact rational intersections when requested") {
    using Rational = pgl::Rational<int64_t>;
    using IntSegment = pgl::OrientedSegment<pgl::Point<int>>;
    using RationalPoint = pgl::Point<Rational>;

    const IntSegment skewed({1, 1}, {2, 4});
    const auto midpoint = skewed.midpoint<Rational>();
    const auto inside = skewed.pointInside<Rational>();
    CHECK(midpoint.x() == Rational(3, 2));
    CHECK(midpoint.y() == Rational(5, 2));
    CHECK(inside.x() == Rational(3, 2));
    CHECK(inside.y() == Rational(5, 2));
    static_assert(std::is_same_v<decltype(inside), const RationalPoint>);

    const IntSegment rising({0, 0}, {3, 3});
    const IntSegment falling({3, 0}, {0, 3});
    const auto intersection = rising.intersection<Rational>(falling);

    REQUIRE(intersection);
    REQUIRE(std::holds_alternative<RationalPoint>(*intersection));
    CHECK(std::get<RationalPoint>(*intersection) == RationalPoint(Rational(3, 2), Rational(3, 2)));
}

TEST_CASE("OrientedSegment keeps orientation-specific equality but geometry-specific predicates") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment horizontal({4, 0}, {0, 0});
    const OrientedSegment vertical({2, -2}, {2, 2});
    const OrientedSegment touching_endpoint({4, 0}, {6, 2});

    CHECK(horizontal.contains(Point(2, 0)));
    CHECK(horizontal.contains(Point(4, 0)));
    CHECK(horizontal.interiorContains(Point(2, 0)));
    CHECK_FALSE(horizontal.interiorContains(Point(4, 0)));
    CHECK(horizontal.boundaryContains(Point(4, 0)));
    CHECK(horizontal.verticesContain(Point(0, 0)));

    CHECK(horizontal.intersects(vertical));
    CHECK(vertical.intersects(horizontal));
    CHECK(horizontal.crosses(vertical));
    CHECK(horizontal.interiorsIntersect(vertical));

    CHECK(horizontal.intersects(touching_endpoint));
    CHECK_FALSE(horizontal.crosses(touching_endpoint));
    CHECK_FALSE(horizontal.interiorsIntersect(touching_endpoint));
}

TEST_CASE("OrientedSegment distinguishes containment of unoriented and oriented subsegments") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment host({6, 6}, {0, 0});
    const Segment inner_unoriented({2, 2}, {4, 4});
    const OrientedSegment inner_oriented({4, 4}, {2, 2});
    const OrientedSegment touching_endpoint({6, 6}, {4, 4});

    CHECK(host.contains(inner_unoriented));
    CHECK(host.contains(inner_oriented));
    CHECK(host.interiorContains(inner_unoriented));
    CHECK(host.interiorContains(inner_oriented));
    CHECK_FALSE(host.interiorContains(touching_endpoint));
}

TEST_CASE("OrientedSegment intersects and intersects-with-point delegate to Segment") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment segment({4, 4}, {0, 0});
    const Point on_segment(2, 2);
    const Point endpoint(0, 0);
    const Point off_segment(2, 3);

    CHECK(segment.intersects(on_segment));
    CHECK(segment.intersects(endpoint));
    CHECK_FALSE(segment.intersects(off_segment));

    CHECK(segment.intersection(on_segment) == Point(2, 2));
    CHECK(segment.intersection(endpoint) == Point(0, 0));
    CHECK_FALSE(segment.intersection(off_segment));
}

TEST_CASE("OrientedSegment covers the non-Convex contract through Segment delegation") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const OrientedSegment horizontal({4, 0}, {0, 0});

    CHECK(horizontal.interiorsIntersect(Point(2, 0)));  // interior point of the segment
    CHECK(horizontal.interiorsIntersect(Line({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(OrientedLine({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Ray({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.interiorsIntersect(Halfplane({0, 0}, {4, 0})));
    CHECK(horizontal.interiorsIntersect(Rectangle({1, -1}, {3, 1})));
    CHECK(horizontal.interiorsIntersect(Triangle({1, -1}, {3, -1}, {2, 2})));

    CHECK_FALSE(horizontal.separates(Point(2, 0)));
    CHECK(horizontal.separates(Line({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.separates(Halfplane({0, -1}, {4, -1})));
    CHECK(horizontal.separates(Triangle({1, -1}, {3, -1}, {2, 2})));

    CHECK_FALSE(horizontal.crosses(Point(2, 0)));
    CHECK(horizontal.crosses(Line({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.crosses(Halfplane({0, 0}, {4, 0})));
    CHECK(horizontal.crosses(Rectangle({1, -1}, {3, 1})));
}

TEST_CASE("OrientedSegment ordering and hashing keep opposite directions distinct") {
    using OrientedSegment = pgl::OrientedSegment<pgl::Point<int>>;

    const OrientedSegment forward({4, 3}, {2, 1});
    const OrientedSegment backward({2, 1}, {4, 3});

    CHECK_FALSE(forward == backward);

    std::set<OrientedSegment> ordered_set;
    ordered_set.insert(forward);
    ordered_set.insert(backward);
    CHECK(ordered_set.size() == 2);

    std::unordered_set<OrientedSegment> unordered_set;
    unordered_set.insert(forward);
    unordered_set.insert(backward);
    CHECK(unordered_set.size() == 2);
}

TEST_CASE("OrientedSegment separates and crosses another oriented segment") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment horizontal({-2, 0}, {2, 0});
    const OrientedSegment vertical({0, -2}, {0, 2});       // crosses horizontal at the origin
    const OrientedSegment reversed_vertical({0, 2}, {0, -2});
    const OrientedSegment touching({2, 0}, {4, 2});        // meets only at the endpoint (2,0)
    const OrientedSegment disjoint({3, 1}, {5, 1});

    SUBCASE("crossing at an interior point") {
        CHECK(horizontal.separates(vertical));
        CHECK(vertical.separates(horizontal));
        CHECK(horizontal.crosses(vertical));
        // Separation delegates to the unoriented segments, so orientation is irrelevant.
        CHECK(horizontal.separates(reversed_vertical));
    }

    SUBCASE("touching only at an endpoint does not separate") {
        CHECK_FALSE(horizontal.separates(touching));
        CHECK_FALSE(horizontal.crosses(touching));
    }

    SUBCASE("disjoint segments do not separate") {
        CHECK_FALSE(horizontal.separates(disjoint));
        CHECK_FALSE(horizontal.crosses(disjoint));
    }
}
