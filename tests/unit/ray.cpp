#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <compare>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>

#include "pgl.hpp"


TEST_CASE_TEMPLATE("Ray preserves source and target order while exposing min and max", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Ray = pgl::Ray<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Ray degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.source() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.target() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.source() == Point(Number{}, Number{}));
        CHECK(degenerate.target() == Point(Number{}, Number{}));
    }

    const Ray ray(make_point(static_cast<Number>(4), static_cast<Number>(3), "a"),
                  make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));

    CHECK(ray.source().x() == Number(4));
    CHECK(ray.source().y() == Number(3));
    CHECK(ray.target().x() == Number(2));
    CHECK(ray.target().y() == Number(1));
    CHECK(ray.min().x() == Number(2));
    CHECK(ray.min().y() == Number(1));
    CHECK(ray.max().x() == Number(4));
    CHECK(ray.max().y() == Number(3));

    Number coordinate_sum{};
    for (const auto& point : ray) {
        coordinate_sum += point.x() + point.y();
    }
    CHECK(coordinate_sum == Number(10));

    if constexpr (requires { ray.source().label(); }) {
        CHECK(ray.source().label() == "a");
        CHECK(ray.target().label() == "b");
        CHECK(ray.min().label() == "b");
        CHECK(ray.max().label() == "a");
    }
}

TEST_CASE("Ray streams, flips, scales, translates, and converts to supporting lines") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray ray(4, 3, 2, 1);

    std::ostringstream stream;
    stream << ray;
    CHECK(stream.str() == "(4,3)--(2,1)->");

    const auto opposite = ray.opposite();
    const auto translated = ray + Point(1, 2);
    const auto shifted = translated - Point(1, 1);
    const auto scaled = 2 * ray;
    const Line line = static_cast<Line>(ray);
    const OrientedLine oriented_line = static_cast<OrientedLine>(ray);

    CHECK(opposite.source() == Point(2, 1));
    CHECK(opposite.target() == Point(4, 3));
    CHECK(translated.source() == Point(5, 5));
    CHECK(translated.target() == Point(3, 3));
    CHECK(shifted.source() == Point(4, 4));
    CHECK(shifted.target() == Point(2, 2));
    CHECK(scaled.source() == Point(8, 6));
    CHECK(scaled.target() == Point(4, 2));
    CHECK(line == Line(Point(2, 1), Point(4, 3)));
    CHECK(oriented_line == OrientedLine(Point(4, 3), Point(2, 1)));
}

TEST_CASE("Ray converts between labeled and unlabeled defining points") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainRay = pgl::Ray<PlainPoint>;
    using LabelRay = pgl::Ray<LabelPoint>;

    const LabelRay labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainRay plain_source(4, 3, 2, 1);

    const PlainRay plain_from_labeled = labeled;
    const LabelRay labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.source() == PlainPoint(4, 3));
    CHECK(plain_from_labeled.target() == PlainPoint(2, 1));
    CHECK(labeled_from_plain.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.source().label().empty());
    CHECK(labeled_from_plain.target().label().empty());

    PlainRay plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.source() == PlainPoint(4, 3));
    CHECK(plain_assigned.target() == PlainPoint(2, 1));

    LabelRay labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.source().label().empty());
    CHECK(labeled_assigned.target().label().empty());
}

TEST_CASE("Ray builds geometric and orientation-dependent half-planes") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray ray({4, 4}, {0, 0});

    CHECK(ray.halfplaneAbove().contains(Point(1, 0)));
    CHECK_FALSE(ray.halfplaneAbove().contains(Point(0, 1)));

    CHECK(ray.halfplaneBelow().contains(Point(0, 1)));
    CHECK_FALSE(ray.halfplaneBelow().contains(Point(1, 0)));

    CHECK(ray.leftHalfplane().contains(Point(1, 0)));
    CHECK_FALSE(ray.leftHalfplane().contains(Point(0, 1)));

    CHECK(ray.rightHalfplane().contains(Point(0, 1)));
    CHECK_FALSE(ray.rightHalfplane().contains(Point(1, 0)));
}

TEST_CASE("Ray equality, ordering, and hashing depend on source and direction") {
    using Ray = pgl::Ray<pgl::Point<int>>;

    const Ray first({1, 2}, {3, 4});
    const Ray same({1, 2}, {5, 6});
    const Ray different_source({2, 3}, {4, 5});
    const Ray opposite({3, 4}, {1, 2});

    CHECK(first == same);
    CHECK_FALSE(first == different_source);
    CHECK_FALSE(first == opposite);
    CHECK_FALSE(first < same);
    const bool rays_are_ordered = (first < different_source) || (different_source < first);
    CHECK(rays_are_ordered);

    std::set<Ray> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(same);
    ordered_set.insert(different_source);
    ordered_set.insert(opposite);
    CHECK(ordered_set.size() == 3);

    std::unordered_set<Ray> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(same);
    unordered_set.insert(different_source);
    unordered_set.insert(opposite);
    CHECK(unordered_set.size() == 3);
}

TEST_CASE("Ray distinguishes boundary, containment, collinearity, orientation, and parallelism") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {4, 4});
    const Ray nested({2, 2}, {3, 3});
    const Line support({1, 1}, {3, 3});
    const OrientedLine oriented_support({3, 3}, {1, 1});
    const Rectangle off_ray_box({1, 1}, {3, 2});
    const Segment subsegment({1, 1}, {3, 3});
    const OrientedSegment oriented_subsegment({3, 3}, {1, 1});
    const Ray parallel({0, 1}, {4, 5});

    CHECK(diagonal.verticesContain(Point(4, 4)));
    CHECK(diagonal.boundaryContains(Point(0, 0)));
    CHECK_FALSE(diagonal.boundaryContains(Point(4, 4)));
    CHECK(diagonal.contains(Point(2, 2)));
    CHECK_FALSE(diagonal.contains(Point(-1, -1)));
    CHECK(diagonal.interiorContains(Point(2, 2)));
    CHECK_FALSE(diagonal.interiorContains(Point(0, 0)));

    CHECK(diagonal.contains(subsegment));
    CHECK(diagonal.contains(oriented_subsegment));
    CHECK(diagonal.contains(nested));
    CHECK_FALSE(diagonal.contains(off_ray_box));
    CHECK_FALSE(diagonal.contains(support));
    CHECK_FALSE(diagonal.contains(oriented_support));

    CHECK(diagonal.collinear(Point(-1, -1)));
    CHECK(diagonal.collinear(support));
    CHECK(diagonal.collinear(oriented_support));
    CHECK(diagonal.collinear(subsegment));
    CHECK(diagonal.collinear(oriented_subsegment));
    CHECK(diagonal.collinear(nested));

    CHECK(diagonal.orientation(Point(0, 1)) == std::partial_ordering::greater);
    CHECK(diagonal.orientation(Point(1, 0)) == std::partial_ordering::less);
    CHECK(diagonal.orientation(Point(2, 2)) == std::partial_ordering::equivalent);

    CHECK(diagonal.parallel(parallel));
    CHECK(diagonal.parallel(Line({0, 2}, {4, 6})));
    CHECK(diagonal.parallel(OrientedLine({4, 6}, {0, 2})));
}

TEST_CASE("Ray evaluates coordinates with yAtX and xAtY") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Rational = pgl::Rational<int64_t>;

    const Ray diagonal(Point(0, 0), Point(4, 4));
    CHECK(diagonal.yAtX(3) == 3);
    CHECK(diagonal.xAtY(2) == 2);
    CHECK(diagonal.yAtX<Rational>(1).value() == Rational(1));
    CHECK(diagonal.xAtY<Rational>(1).value() == Rational(1));
    CHECK_FALSE(diagonal.yAtX(-1).has_value());
    CHECK_FALSE(diagonal.xAtY(-1).has_value());

    const Ray descending(Point(0, 4), Point(4, 0));
    CHECK(descending.yAtX(5) == -1);
    CHECK(descending.xAtY(1) == 3);
    CHECK_FALSE(descending.xAtY(5).has_value());

    const Ray vertical(Point(2, -1), Point(2, 3));
    CHECK(vertical.yAtX(2) == -1);
    CHECK_FALSE(vertical.yAtX(1).has_value());
    CHECK(vertical.xAtY(4) == 2);
    CHECK_FALSE(vertical.xAtY(-2).has_value());

    const Ray horizontal(Point(4, 1), Point(-2, 1));
    CHECK(horizontal.xAtY(1) == -2);
    CHECK_FALSE(horizontal.xAtY(0).has_value());
    CHECK(horizontal.yAtX(3) == 1);
    CHECK_FALSE(horizontal.yAtX(5).has_value());
}

TEST_CASE("Ray intersections cover point, segment, ray, and empty cases") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray horizontal({0, 0}, {4, 0});
    const Point on_ray(2, 0);
    const Point off_ray(-1, 1);

    const auto with_point = horizontal.intersection(on_ray);
    REQUIRE(with_point);
    CHECK(*with_point == on_ray);

    CHECK_FALSE(horizontal.intersection(off_ray).has_value());
    CHECK(horizontal.intersects(on_ray));
    CHECK_FALSE(horizontal.intersects(off_ray));

    const auto with_line = horizontal.intersection(Line({2, -2}, {2, 2}));
    REQUIRE(with_line);
    REQUIRE(std::holds_alternative<Point>(*with_line));
    CHECK(std::get<Point>(*with_line) == Point(2, 0));

    const auto with_oriented_line = horizontal.intersection(OrientedLine({2, 2}, {2, -2}));
    REQUIRE(with_oriented_line);
    REQUIRE(std::holds_alternative<Point>(*with_oriented_line));
    CHECK(std::get<Point>(*with_oriented_line) == Point(2, 0));

    const auto with_segment = horizontal.intersection(Segment({-2, 0}, {2, 0}));
    REQUIRE(with_segment);
    REQUIRE(std::holds_alternative<Segment>(*with_segment));
    CHECK(std::get<Segment>(*with_segment) == Segment(Point(0, 0), Point(2, 0)));

    const auto with_oriented_segment = horizontal.intersection(OrientedSegment({3, 0}, {1, 0}));
    REQUIRE(with_oriented_segment);
    REQUIRE(std::holds_alternative<Segment>(*with_oriented_segment));
    CHECK(std::get<Segment>(*with_oriented_segment) == Segment(Point(1, 0), Point(3, 0)));

    const auto with_same_direction_ray = horizontal.intersection(Ray({2, 0}, {6, 0}));
    REQUIRE(with_same_direction_ray);
    REQUIRE(std::holds_alternative<Ray>(*with_same_direction_ray));
    CHECK(std::get<Ray>(*with_same_direction_ray) == Ray(Point(2, 0), Point(6, 0)));

    const auto with_opposite_same_source = horizontal.intersection(Ray({0, 0}, {-3, 0}));
    REQUIRE(with_opposite_same_source);
    REQUIRE(std::holds_alternative<Point>(*with_opposite_same_source));
    CHECK(std::get<Point>(*with_opposite_same_source) == Point(0, 0));

    const auto with_opposite_overlap = horizontal.intersection(Ray({3, 0}, {1, 0}));
    REQUIRE(with_opposite_overlap);
    REQUIRE(std::holds_alternative<Segment>(*with_opposite_overlap));
    CHECK(std::get<Segment>(*with_opposite_overlap) == Segment(Point(0, 0), Point(3, 0)));

    CHECK(horizontal.intersects(Line({2, -2}, {2, 2})));
    CHECK(horizontal.intersects(Segment({-2, 0}, {2, 0})));
    CHECK(horizontal.intersects(Ray({2, 0}, {6, 0})));
    CHECK_FALSE(horizontal.intersects(Line({-2, -2}, {-2, 2})));
    CHECK(horizontal.separates(Line({0, -2}, {0, 2})));
    CHECK(horizontal.separates(Line({2, -2}, {2, 2})));
    CHECK(horizontal.separates(Segment({0, -1}, {0, 1})));
    CHECK(horizontal.separates(Segment({2, -1}, {2, 1})));
    // The ray reaches past its target (x = 4), so it still cuts a segment it
    // only crosses beyond that point.
    CHECK(horizontal.separates(Segment({6, -1}, {6, 1})));
    CHECK_FALSE(horizontal.separates(Segment({0, 0}, {2, 0})));
    CHECK(horizontal.separates(Ray({2, -1}, {2, 1})));
    CHECK_FALSE(horizontal.separates(Ray({2, 0}, {2, 1})));

    CHECK_FALSE(horizontal.interiorsIntersect(Line({0, -2}, {0, 2})));
    CHECK(horizontal.interiorsIntersect(Line({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Line({4, -2}, {4, 2})));
    CHECK_FALSE(horizontal.crosses(Line({0, -2}, {0, 2})));
    CHECK(horizontal.crosses(Line({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.crosses(Point(2, 0)));
    CHECK(horizontal.crosses(pgl::Rectangle<Point>({1, -1}, {3, 1})));
    CHECK(horizontal.crosses(pgl::Triangle<Point>({1, -1}, {3, -1}, {2, 2})));
    CHECK_FALSE(horizontal.crosses(pgl::Halfplane<Point>({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Segment({0, 0}, {2, 0})));
    CHECK(horizontal.interiorsIntersect(Segment({-2, 0}, {2, 0})));
    CHECK_FALSE(horizontal.interiorsIntersect(Segment({2, 0}, {2, 1})));
    CHECK_FALSE(horizontal.interiorsIntersect(Segment({2, -1}, {2, 0})));
    CHECK(horizontal.interiorsIntersect(Ray({2, 0}, {6, 0})));
    CHECK(horizontal.interiorsIntersect(Ray({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.interiorsIntersect(Ray({0, 0}, {-3, 0})));
}

TEST_CASE("Ray covers the non-Convex contract for interiorsIntersect") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Ray horizontal({0, 0}, {4, 0});

    CHECK(horizontal.interiorsIntersect(Point(2, 0)));  // interior point of the ray
    CHECK_FALSE(horizontal.interiorsIntersect(Point(0, 0)));  // source (boundary)
    CHECK(horizontal.interiorsIntersect(Line({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(OrientedLine({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Segment({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(OrientedSegment({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Ray({2, -2}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Halfplane({0, -1}, {4, -1})));
    CHECK(horizontal.interiorsIntersect(Rectangle({1, -1}, {3, 1})));
    CHECK(horizontal.interiorsIntersect(Triangle({1, -1}, {3, -1}, {2, 2})));
    CHECK(horizontal.interiorsIntersect(Shape(Rectangle({1, -1}, {3, 1}))));
}

TEST_CASE("Ray covers the non-Convex contract for separates") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Ray horizontal({0, 0}, {4, 0});

    CHECK_FALSE(horizontal.separates(Point(2, 0)));
    CHECK(horizontal.separates(Line({2, -2}, {2, 2})));
    CHECK(horizontal.separates(OrientedLine({2, -2}, {2, 2})));
    CHECK(horizontal.separates(Segment({2, -2}, {2, 2})));
    CHECK(horizontal.separates(OrientedSegment({2, -2}, {2, 2})));
    CHECK(horizontal.separates(Ray({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.separates(Halfplane({0, -1}, {4, -1})));
    CHECK(horizontal.separates(Rectangle({1, -1}, {3, 1})));
    CHECK(horizontal.separates(Triangle({1, -1}, {3, -1}, {2, 2})));
    CHECK(horizontal.separates(Shape(Rectangle({1, -1}, {3, 1}))));
}

TEST_CASE("Ray covers the non-Convex contract for crosses") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Ray horizontal({0, 0}, {4, 0});

    CHECK_FALSE(horizontal.crosses(Point(2, 0)));
    CHECK(horizontal.crosses(Line({2, -2}, {2, 2})));
    CHECK(horizontal.crosses(OrientedLine({2, -2}, {2, 2})));
    CHECK(horizontal.crosses(Segment({2, -2}, {2, 2})));
    CHECK(horizontal.crosses(OrientedSegment({2, -2}, {2, 2})));
    CHECK(horizontal.crosses(Ray({2, -2}, {2, 2})));
    CHECK_FALSE(horizontal.crosses(Halfplane({0, -1}, {4, -1})));
    CHECK(horizontal.crosses(Rectangle({1, -1}, {3, 1})));
    CHECK(horizontal.crosses(Triangle({1, -1}, {3, -1}, {2, 2})));
    CHECK(horizontal.crosses(Shape(Rectangle({1, -1}, {3, 1}))));
}

TEST_CASE("Ray intersection and distances support exact rational results") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using RationalPoint = pgl::Point<Rational>;

    const Ray rising({0, 0}, {4, 4});
    const Ray falling({6, 0}, {0, 3});
    const Ray leftward({-5, 0}, {-6, 0});
    const Ray vertical({5, 5}, {5, 6});

    const auto intersection = rising.intersection<Rational>(falling);
    REQUIRE(intersection);
    REQUIRE(std::holds_alternative<RationalPoint>(*intersection));
    CHECK(std::get<RationalPoint>(*intersection) == RationalPoint(Rational(2), Rational(2)));

    CHECK(rising.squaredDistance(Point(2, 0)) == doctest::Approx(2.0));
    CHECK(rising.squaredDistance(Point(-1, 0)) == doctest::Approx(1.0));
    CHECK(leftward.squaredDistance(Ray({0, 0}, {4, 0})) == doctest::Approx(25.0));
    CHECK(Ray({0, 0}, {4, 0}).squaredDistance(vertical) == doctest::Approx(25.0));
    CHECK(Ray({0, 0}, {4, 0}).squaredDistance(pgl::Line<Point>({-2, -2}, {-2, 2})) == doctest::Approx(4.0));
}

TEST_CASE("Ray interiorContains another ray") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray ray({0, 0}, {1, 0});  // source (0,0), pointing toward +x

    // A collinear sub-ray whose source is strictly past this source.
    CHECK(ray.interiorContains(Ray({2, 0}, {5, 0})));
    // Sharing the source: the source lies on the boundary, not the interior.
    CHECK_FALSE(ray.interiorContains(Ray({0, 0}, {3, 0})));
    CHECK_FALSE(ray.interiorContains(ray));
    // A source behind this ray's source is not contained at all.
    CHECK_FALSE(ray.interiorContains(Ray({-1, 0}, {5, 0})));
    // Not collinear: the target leaves the ray.
    CHECK_FALSE(ray.interiorContains(Ray({2, 0}, {2, 5})));
}
