#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include "pgl.hpp"


TEST_CASE_TEMPLATE("Segment orders its endpoints and preserves endpoint labels", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Segment = pgl::Segment<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Segment degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.min() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.max() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.min() == Point(Number{}, Number{}));
        CHECK(degenerate.max() == Point(Number{}, Number{}));
    }

    const Segment segment(make_point(static_cast<Number>(4), static_cast<Number>(3), "a"), make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));
    
    CHECK(segment[0].x() == Number(2));
    CHECK(segment[0].y() == Number(1));
    CHECK(segment[1].x() == Number(4));
    CHECK(segment[1].y() == Number(3));

    const Segment from_points(make_point(static_cast<Number>(3), static_cast<Number>(4)), make_point(static_cast<Number>(1), static_cast<Number>(2)));
    
    CHECK(from_points.min().x() == Number(1));
    CHECK(from_points.max().y() == Number(4));

    Number coordinate_sum{};
    for (const auto& point : segment) {
        coordinate_sum += point.x() + point.y();
    }
    CHECK(coordinate_sum == Number(10));

    if constexpr (requires { segment.min().label(); }) {
        CHECK(segment.min().label() == "b");
        CHECK(segment.max().label() == "a");
    }
}

TEST_CASE("Segment streams, scales, translates, and shifts as expected") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment segment(4, 3, 2, 1);
    std::ostringstream stream;
    stream << segment;
    CHECK(stream.str() == "(2,1)--(4,3)");

    const Point first_endpoint = segment[0];
    const auto scaled = 2 * segment;
    const auto translated = first_endpoint + segment;
    const auto shifted = translated - Point(1, 1);

    CHECK(scaled.min() == Point(4, 2));
    CHECK(scaled.max() == Point(8, 6));
    CHECK(translated.min() == Point(4, 2));
    CHECK(translated.max() == Point(6, 4));
    CHECK(shifted.min() == Point(3, 1));
    CHECK(shifted.max() == Point(5, 3));
}

TEST_CASE("Segment exposes a bounding box, vertices, and a single edge") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Segment segment(4, 3, 2, 1);
    const auto box = segment.bbox();
    CHECK(box.min() == Point(2, 1));
    CHECK(box.max() == Point(4, 3));

    const auto vertices = segment.vertices();
    CHECK(vertices.size() == 2);
    CHECK(vertices[0] == Point(2, 1));
    CHECK(vertices[1] == Point(4, 3));

    const auto edges = segment.edges();
    CHECK(edges.size() == 1);
    CHECK(edges[0] == segment);

    const auto oriented_edges = segment.orientedEdges();
    CHECK(oriented_edges.size() == 1);
    CHECK(oriented_edges[0] == OrientedSegment(Point(2, 1), Point(4, 3)));
}

TEST_CASE("Segment evaluates coordinates with yAtX and xAtY") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rational = pgl::Rational<int64_t>;

    const Segment diagonal(Point(0, 0), Point(4, 4));
    CHECK(diagonal.yAtX(2) == 2);
    CHECK(diagonal.xAtY(3) == 3);
    CHECK(diagonal.yAtX<Rational>(1).value() == Rational(1));
    CHECK(diagonal.xAtY<Rational>(1).value() == Rational(1));

    const Segment descending(Point(0, 4), Point(4, 0));
    CHECK(descending.yAtX(1) == 3);
    CHECK(descending.xAtY(3) == 1);
    CHECK_FALSE(descending.xAtY(5).has_value());

    const Segment vertical(Point(2, -1), Point(2, 3));
    CHECK(vertical.yAtX(2) == -1);
    CHECK_FALSE(vertical.yAtX(1).has_value());
    CHECK(vertical.xAtY(0) == 2);

    const Segment horizontal(Point(-2, 1), Point(4, 1));
    CHECK(horizontal.xAtY(1) == -2);
    CHECK_FALSE(horizontal.xAtY(0).has_value());
    CHECK(horizontal.yAtX(3) == 1);
}

TEST_CASE("Segment converts between labeled and unlabeled endpoints") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainSegment = pgl::Segment<PlainPoint>;
    using LabelSegment = pgl::Segment<LabelPoint>;

    const LabelSegment labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainSegment plain_source(4, 3, 2, 1);

    const PlainSegment plain_from_labeled = labeled;
    const LabelSegment labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.min() == PlainPoint(2, 1));
    CHECK(plain_from_labeled.max() == PlainPoint(4, 3));
    CHECK(labeled_from_plain.min() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.max() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.min().label().empty());
    CHECK(labeled_from_plain.max().label().empty());

    PlainSegment plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.min() == PlainPoint(2, 1));
    CHECK(plain_assigned.max() == PlainPoint(4, 3));

    LabelSegment labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.min() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.max() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.min().label().empty());
    CHECK(labeled_assigned.max().label().empty());
}

TEST_CASE("Segment supports in-place translation and scaling while preserving endpoint order") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    Segment segment(4, 3, 2, 1);
    segment += Point(1, 2);
    CHECK(segment.min() == Point(3, 3));
    CHECK(segment.max() == Point(5, 5));

    segment -= Point(1, 1);
    CHECK(segment.min() == Point(2, 2));
    CHECK(segment.max() == Point(4, 4));

    segment *= -2;
    CHECK(segment.min() == Point(-8, -8));
    CHECK(segment.max() == Point(-4, -4));

    segment /= -2;
    CHECK(segment.min() == Point(2, 2));
    CHECK(segment.max() == Point(4, 4));
}

TEST_CASE_TEMPLATE("Segment reports zero area plus correct length, midpoint, and diameter", Segment, pgl::Segment<pgl::Point<int>>, pgl::Segment<pgl::Point<double>>, pgl::Segment<pgl::Point<pgl::Rational<>>>) {
    using Point = typename Segment::PointType;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const Segment segment(static_cast<Number>(4), static_cast<Number>(3), static_cast<Number>(2), static_cast<Number>(1));

    CHECK(segment.area() == Number(0));
    CHECK(segment.twiceArea() == Number(0));
    CHECK(segment.squaredLength() == Number(8));
    CHECK(segment.template length<double>() == doctest::Approx(std::sqrt(8.0)));
    CHECK(segment.lengthL1() == Number(4));
    CHECK(segment.lengthLInf() == Number(2));
    CHECK(segment.diameter() == segment);
    CHECK(segment.template midpoint<double>().x() == 3.0);
    CHECK(segment.template midpoint<double>().y() == 2.0);
    if constexpr(!std::is_floating_point_v<Number>) {
        CHECK(segment.template midpoint<pgl::Rational<>>().x() == 3);
        CHECK(segment.template midpoint<pgl::Rational<>>().y() == 2);
    }

    if constexpr (requires { Point(static_cast<Number>(3), static_cast<Number>(2), "tag"); }) {
        CHECK(segment.pointInside() == Point(static_cast<Number>(3), static_cast<Number>(2), ""));
    } else {
        CHECK(segment.pointInside() == Point(static_cast<Number>(3), static_cast<Number>(2)));
    }
}

TEST_CASE("Segment exposes axis tests, endpoint aliases, supporting-line collinearity, slope, parallelism, and point inside") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const Segment degenerate({2, 2}, {2, 2});
    const Segment horizontal({0, 0}, {4, 0});
    const Segment vertical({3, -1}, {3, 5});
    const Segment diagonal({0, 0}, {4, 4});
    const Segment descending({4, 0}, {0, 4});
    const Segment parallel_segment({1, 1}, {5, 5});
    const Segment non_parallel_segment({0, 0}, {4, 1});
    const OrientedSegment parallel_oriented_segment({5, 5}, {1, 1});
    const Line parallel_line({0, 1}, {4, 5});
    const Line non_parallel_line({0, 0}, {4, 1});
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
    CHECK(horizontal.slope<double>() == doctest::Approx(0.0));
    CHECK(descending.slope<double>() == doctest::Approx(1.0));

    CHECK(diagonal.parallel(parallel_segment));
    CHECK(diagonal.parallel(parallel_oriented_segment));
    CHECK(diagonal.parallel(parallel_line));
    CHECK(diagonal.parallel(parallel_oriented_line));
    CHECK(diagonal.parallel(parallel_ray));
    CHECK_FALSE(diagonal.parallel(non_parallel_segment));
    CHECK_FALSE(diagonal.parallel(non_parallel_line));

    const auto supporting_line = static_cast<Line>(diagonal);
    CHECK(supporting_line == Line({1, 1}, {3, 3}));
    CHECK(supporting_line.contains(diagonal.min()));
    CHECK(supporting_line.contains(diagonal.max()));

    CHECK(horizontal.interiorContains(horizontal.pointInside<float>()));
    CHECK(vertical.interiorContains(vertical.pointInside<float>()));
    CHECK(diagonal.interiorContains(diagonal.pointInside<float>()));
    CHECK(descending.interiorContains(descending.pointInside<float>()));
}

TEST_CASE("Segment midpoint and pointInside can be checked with exact rational coordinates") {
    using Rational = pgl::Rational<int64_t>;
    using IntSegment = pgl::Segment<pgl::Point<int>>;
    using RationalSegment = pgl::Segment<pgl::Point<Rational>>;

    const IntSegment integer_segment({1, 1}, {2, 4});
    const auto exact_midpoint = integer_segment.midpoint<Rational>();
    const auto exact_inside = integer_segment.pointInside<Rational>();

    CHECK(exact_midpoint.x() == Rational(3, 2));
    CHECK(exact_midpoint.y() == Rational(5, 2));
    CHECK(exact_inside.x() == Rational(3, 2));
    CHECK(exact_inside.y() == Rational(5, 2));

    static_assert(std::is_same_v<decltype(exact_inside), const pgl::Point<Rational>>);

    const auto integer_fbox = integer_segment.fbox();
    CHECK(integer_fbox.min().x() == doctest::Approx(1.0));
    CHECK(integer_fbox.min().y() == doctest::Approx(1.0));
    CHECK(integer_fbox.max().x() == doctest::Approx(2.0));
    CHECK(integer_fbox.max().y() == doctest::Approx(4.0));

    const RationalSegment rational_segment({Rational(1, 3), Rational(1, 2)}, {Rational(5, 3), Rational(7, 2)});
    CHECK(rational_segment.pointInside().x() == Rational(1));
    CHECK(rational_segment.pointInside().y() == Rational(2));
}

TEST_CASE("Segment distinguishes containment in the interior, boundary, and endpoints") {
    using Segment = pgl::Segment<pgl::Point<int>>;

    const Segment segment(4, 3, 2, 1);
    const Segment inner(3, 2, 4, 3);
    const Segment shifted(3, 2, 5, 4);
    const Segment wide_segment(0, 0, 6, 6);
    const Segment strict_inner(2, 2, 4, 4);

    CHECK(segment.contains(pgl::Point<int>(3, 2)));
    CHECK(segment.contains(pgl::Point<int>(4, 3)));
    CHECK(segment.contains(inner));
    CHECK(segment.interiorContains(pgl::Point<int>(3, 2)));
    CHECK(wide_segment.interiorContains(strict_inner));
    CHECK_FALSE(segment.contains(shifted));
    CHECK_FALSE(segment.interiorContains(pgl::Point<int>(4, 3)));
    CHECK_FALSE(segment.boundaryContains(pgl::Point<int>(3, 2)));
    CHECK(segment.boundaryContains(pgl::Point<int>(4, 3)));
    CHECK_FALSE(segment.verticesContain(pgl::Point<int>(3, 2)));
    CHECK(segment.verticesContain(pgl::Point<int>(4, 3)));
}

TEST_CASE("Segment distinguishes crossing, touching, overlap, containment, and T-junctions") {
    using Segment = pgl::Segment<pgl::Point<int>>;

    const Segment horizontal({0, 0}, {4, 0});
    const Segment vertical({2, -2}, {2, 2});
    const Segment touching_endpoint({4, 0}, {6, 2});
    const Segment disjoint({5, 0}, {7, 0});
    const Segment overlap({1, 0}, {6, 0});
    const Segment tee({2, 0}, {2, 2});
    const Segment outer({0, 0}, {6, 0});
    const Segment inner({2, 0}, {4, 0});

    SUBCASE("two segments cross at an interior point") {
        CHECK(horizontal.separates(vertical));
        CHECK(vertical.separates(horizontal));
        CHECK(horizontal.intersects(vertical));
        CHECK(horizontal.crosses(vertical));
        CHECK(horizontal.interiorsIntersect(vertical));
    }

    SUBCASE("two segments meet only at one endpoint") {
        CHECK_FALSE(horizontal.separates(touching_endpoint));
        CHECK_FALSE(touching_endpoint.separates(horizontal));
        CHECK(horizontal.intersects(touching_endpoint));
        CHECK_FALSE(horizontal.crosses(touching_endpoint));
        CHECK_FALSE(horizontal.interiorsIntersect(touching_endpoint));
    }

    SUBCASE("colinear segments can still be completely disjoint") {
        CHECK_FALSE(horizontal.separates(disjoint));
        CHECK_FALSE(disjoint.separates(horizontal));
        CHECK_FALSE(horizontal.intersects(disjoint));
        CHECK_FALSE(horizontal.crosses(disjoint));
        CHECK_FALSE(horizontal.interiorsIntersect(disjoint));
    }

    SUBCASE("colinear segments can overlap or one can strictly contain the other") {
        CHECK_FALSE(horizontal.separates(overlap));
        CHECK_FALSE(overlap.separates(horizontal));
        CHECK(horizontal.intersects(overlap));
        CHECK_FALSE(horizontal.crosses(overlap));
        CHECK(horizontal.interiorsIntersect(overlap));

        CHECK_FALSE(outer.separates(inner));
        CHECK(inner.separates(outer));
        CHECK_FALSE(outer.crosses(inner));
        CHECK(outer.interiorsIntersect(inner));
    }

    SUBCASE("a T junction makes separates directional") {
        CHECK_FALSE(horizontal.separates(tee));
        CHECK(tee.separates(horizontal));
        CHECK(horizontal.intersects(tee));
        CHECK_FALSE(horizontal.crosses(tee));
        CHECK_FALSE(horizontal.interiorsIntersect(tee));
    }
}

TEST_CASE("Segment accepts runtime Shape arguments for core predicates") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Segment horizontal({0, 0}, {4, 0});
    const Shape midpoint = Point(2, 0);
    const Shape off_segment = Point(2, 1);
    const Shape inner = Segment({1, 0}, {3, 0});
    const Shape crossing = Segment({2, -2}, {2, 2});
    const Shape centered_box = Rectangle({1, -1}, {3, 1});

    CHECK(horizontal.contains(midpoint));
    CHECK(horizontal.contains(inner));
    CHECK_FALSE(horizontal.contains(off_segment));

    CHECK(horizontal.intersects(midpoint));
    CHECK(horizontal.intersects(crossing));
    CHECK(horizontal.intersects(centered_box));

    CHECK(horizontal.interiorsIntersect(inner));
    CHECK_FALSE(horizontal.interiorsIntersect(midpoint));

    CHECK(horizontal.separates(crossing));
    CHECK_FALSE(horizontal.separates(midpoint));

    CHECK(horizontal.crosses(crossing));
    CHECK(horizontal.crosses(centered_box));
    CHECK_FALSE(horizontal.crosses(midpoint));
}

TEST_CASE("Segment interiorsIntersect covers the non-Convex Shape contract") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Segment horizontal({0, 0}, {4, 0});

    CHECK_FALSE(horizontal.interiorsIntersect(Point(2, 0)));

    CHECK(horizontal.interiorsIntersect(OrientedSegment({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Line({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.interiorsIntersect(Line({0, 0}, {0, 3})));
    CHECK(horizontal.interiorsIntersect(OrientedLine({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Ray({2, -2}, {2, 2})));

    CHECK_FALSE(horizontal.interiorsIntersect(Halfplane({0, 0}, {4, 0})));
    CHECK(horizontal.interiorsIntersect(Halfplane({0, -1}, {4, -1})));

    CHECK(horizontal.interiorsIntersect(Rectangle({1, -1}, {3, 1})));
    CHECK(horizontal.interiorsIntersect(Triangle({1, -1}, {3, -1}, {2, 2})));
}

TEST_CASE("Segment separates covers the non-Convex Shape contract") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Segment horizontal({0, 0}, {4, 0});
    const Shape point = Point(2, 0);
    const Shape line = pgl::Line<Point>({2, -2}, {2, 2});
    const Shape halfplane = Halfplane({0, -1}, {4, -1});
    const Shape triangle = Triangle({1, -1}, {3, -1}, {2, 2});

    CHECK_FALSE(horizontal.separates(point));
    CHECK(horizontal.separates(line));
    CHECK_FALSE(horizontal.separates(halfplane));
    CHECK(horizontal.separates(triangle));
}

TEST_CASE("Segment crosses other shapes only when both set differences disconnect") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Segment horizontal({0, 0}, {4, 0});
    const OrientedSegment vertical_oriented({2, -2}, {2, 2});
    const Line vertical_line({2, -2}, {2, 2});
    const OrientedLine vertical_oriented_line({2, -2}, {2, 2});
    const Ray upward_ray({2, -2}, {2, 2});
    const Ray source_touching_ray({0, 0}, {0, 3});
    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle centered_box({1, -1}, {3, 1});
    const Rectangle left_box({-3, -1}, {-1, 1});
    const Triangle centered_triangle({1, -1}, {3, -1}, {2, 2});
    const Triangle vertex_touch_triangle({4, 0}, {6, 0}, {5, 2});

    SUBCASE("point-shaped queries never cross a segment") {
        CHECK_FALSE(horizontal.crosses(Point(2, 0)));
        CHECK_FALSE(horizontal.crosses(Point(5, 0)));
    }

    SUBCASE("an oriented segment delegates to the segment case") {
        CHECK(horizontal.crosses(vertical_oriented));
    }

    SUBCASE("a line crosses exactly when it cuts the segment at an interior point") {
        CHECK(horizontal.crosses(vertical_line));
        CHECK(horizontal.crosses(vertical_oriented_line));
        CHECK_FALSE(horizontal.crosses(Line({4, 0}, {4, 3})));
    }

    SUBCASE("a ray crosses exactly when the mutual cut happens away from boundaries") {
        CHECK(horizontal.crosses(upward_ray));
        CHECK_FALSE(horizontal.crosses(source_touching_ray));
    }

    SUBCASE("a halfplane does not cross a segment") {
        CHECK_FALSE(horizontal.crosses(upper));
    }

    SUBCASE("an area crosses only when the segment truly cuts it and is itself cut") {
        CHECK(horizontal.crosses(centered_box));
        CHECK_FALSE(horizontal.crosses(left_box));
        CHECK(horizontal.crosses(centered_triangle));
        CHECK_FALSE(horizontal.crosses(vertex_touch_triangle));
    }
}

TEST_CASE("Segment distances handle projections, intersections, degenerate cases, and Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment horizontal({0, 0}, {4, 0});
    const Segment parallel({1, 2}, {3, 2});
    const Segment far_right({6, 0}, {7, 0});
    const Segment vertical({2, -2}, {2, 2});
    const Segment inner({1, 0}, {3, 0});
    const Segment point_segment({2, 0}, {2, 0});

    CHECK(horizontal.squaredDistance(Point(2, 3)) == doctest::Approx(9.0));
    CHECK(horizontal.squaredDistance(Point(-1, 0)) == doctest::Approx(1.0));
    CHECK(horizontal.squaredDistance(Point(5, 3)) == doctest::Approx(10.0));

    CHECK(horizontal.squaredDistance(parallel) == doctest::Approx(4.0));
    CHECK(horizontal.squaredDistance(far_right) == doctest::Approx(4.0));
    CHECK(horizontal.squaredDistance(vertical) == doctest::Approx(0.0));
    CHECK(point_segment.squaredDistance(Point(5, 0)) == doctest::Approx(9.0));

    CHECK(horizontal.squaredHausdorffDistance(parallel) == doctest::Approx(5.0));
    CHECK(horizontal.squaredHausdorffDistance(inner) == doctest::Approx(1.0));
}

TEST_CASE("Segment intersection can return an exact rational crossing point") {
    using Rational = pgl::Rational<int64_t>;
    using Segment = pgl::Segment<pgl::Point<int>>;
    using RationalPoint = pgl::Point<Rational>;

    const Segment rising({0, 0}, {3, 3});
    const Segment falling({0, 3}, {3, 0});
    const auto intersection = rising.intersection<Rational>(falling);

    REQUIRE(intersection);
    REQUIRE(std::holds_alternative<RationalPoint>(*intersection));

    const RationalPoint expected(Rational(3, 2), Rational(3, 2));
    CHECK(std::get<RationalPoint>(*intersection) == expected);
}

TEST_CASE("Segment intersects and intersects-with-point use point containment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment segment({0, 0}, {4, 4});
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

TEST_CASE("Degenerate segments have empty interiors even when they intersect other segments") {
    using Segment = pgl::Segment<pgl::Point<int>>;

    const Segment point({2, 0}, {2, 0});
    const Segment same_point({2, 0}, {2, 0});
    const Segment horizontal({0, 0}, {4, 0});

    CHECK(point.intersects(horizontal));
    CHECK(horizontal.intersects(point));

    CHECK_FALSE(point.interiorsIntersect(horizontal));
    CHECK_FALSE(horizontal.interiorsIntersect(point));
    CHECK_FALSE(point.interiorsIntersect(same_point));
}

TEST_CASE("Segment ordering and hashing ignore the input endpoint order") {
    using Segment = pgl::Segment<pgl::Point<int>>;

    std::set<Segment> ordered_set;
    ordered_set.insert({{4, 3}, {2, 1}});
    ordered_set.insert({{2, 1}, {4, 3}});
    CHECK(ordered_set.size() == 1);

    std::unordered_set<Segment> unordered_set;
    unordered_set.insert({{4, 3}, {2, 1}});
    unordered_set.insert({{2, 1}, {4, 3}});
    CHECK(unordered_set.size() == 1);
}

TEST_CASE("Segment arithmetic on different number types") {

    pgl::Point<int> pi(1, 2);
    pgl::Segment<pgl::Point<int>> si(3,4,5,6);
    pgl::Point<double> pd(6.5, 7.5);
    pgl::Segment<pgl::Point<double>> sd(8.5, 9.5, 10.5, 11.5);

    pgl::Segment<pgl::Point<double>> sid = pi + sd;
    CHECK(sid.min().x() == 9.5);
    CHECK(sid.min().y() == 11.5);
    CHECK(sid.max().x() == 11.5);
    CHECK(sid.max().y() == 13.5);

    pgl::Point<pgl::Rational<>> pr(pgl::Rational(5.25), pgl::Rational(19l,3l));
    pgl::Segment<pgl::Point<pgl::Rational<>>> sri = pr + si;
    CHECK(sri.min().x() == pgl::Rational(33l,4l));
    CHECK(sri.min().y() == pgl::Rational(31l,3l));
    CHECK(sri.max().x() == pgl::Rational(41l,4l));
    CHECK(sri.max().y() == pgl::Rational(37l,3l));

    pgl::Segment<pgl::Point<pgl::Rational<>>> srd = pr + sd;
    CHECK(srd.min().x() == pgl::Rational(55l,4l));
    CHECK(srd.min().y() == pgl::Rational(95l,6l));
    CHECK(srd.max().x() == pgl::Rational(63l,4l));
    CHECK(srd.max().y() == pgl::Rational(107l,6l));
}


TEST_CASE("Segment mixing number types") {
    pgl::Segment<pgl::Point<int>> s1(0,0,1,1);
    CHECK(s1.contains(pgl::Point<double>(0.5,0.5)));
    CHECK_FALSE(s1.contains(pgl::Point<double>(1.1,1.1)));
    CHECK_FALSE(s1.contains(pgl::Point<pgl::Rational<int>>({4,3},{4,3})));

    pgl::Segment<pgl::Point<int>> s2(0,0,1,3);
    CHECK(s2.contains(pgl::Point<pgl::Rational<int>>({1,3},1)));
}

TEST_CASE("Segment rotated90 and rotate90") {
    using Seg = pgl::Segment<pgl::Point<int>>;

    // Rotation is correct and constructor re-normalizes
    // (3,1)-(5,4): k=1 gives (-1,3)-(-4,5), stored as (-4,5)-(-1,3)
    const Seg s(3,1,5,4);
    CHECK(s.rotated90(0) == s);
    CHECK(s.rotated90(1) == Seg(-4,5,-1,3));
    CHECK(s.rotated90(2) == Seg(-5,-4,-3,-1));
    CHECK(s.rotated90(3) == Seg(1,-3,4,-5));

    // Wraparound
    CHECK(s.rotated90(4) == s.rotated90(0));
    CHECK(s.rotated90(-1) == s.rotated90(3));

    // Four rotations bring us back
    CHECK(s.rotated90(1).rotated90(1).rotated90(1).rotated90(1) == s);

    // In-place
    Seg q(3,1,5,4);
    q.rotate90();
    CHECK(q == Seg(-4,5,-1,3));
    q.rotate90(3);
    CHECK(q == s);
}

TEST_CASE("Segment per-axis scaling") {
    using Seg = pgl::Segment<pgl::Point<int>>;
    const Seg s(1, 2, 4, 6);

    // Returning forms touch one axis and leave the original intact.
    CHECK(s.scaledUpX(2) == Seg(2, 2, 8, 6));
    CHECK(s.scaledUpY(3) == Seg(1, 6, 4, 18));
    CHECK(s.scaledDownX(1) == s);
    CHECK(s == Seg(1, 2, 4, 6));

    // A negative factor flips the lexicographic endpoint order; the
    // constructor re-normalizes so equality still holds.
    CHECK(s.scaledUpX(-1) == Seg(-1, 2, -4, 6));

    // In-place forms.
    Seg t = s;
    t.scaleUpY(2);
    CHECK(t == Seg(1, 4, 4, 12));
    t.scaleDownY(2);
    CHECK(t == s);
}
























