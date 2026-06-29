#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "pgl.hpp"

TEST_CASE_TEMPLATE("Halfplane preserves source and target order while exposing min and max", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Halfplane = pgl::Halfplane<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Halfplane degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.source() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.target() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.source() == Point(Number{}, Number{}));
        CHECK(degenerate.target() == Point(Number{}, Number{}));
    }

    const Halfplane halfplane(make_point(static_cast<Number>(4), static_cast<Number>(3), "a"),
                              make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));

    CHECK(halfplane.source().x() == Number(4));
    CHECK(halfplane.source().y() == Number(3));
    CHECK(halfplane.target().x() == Number(2));
    CHECK(halfplane.target().y() == Number(1));
    CHECK(halfplane.min().x() == Number(2));
    CHECK(halfplane.min().y() == Number(1));
    CHECK(halfplane.max().x() == Number(4));
    CHECK(halfplane.max().y() == Number(3));

    Number coordinate_sum{};
    for (const auto& point : halfplane) {
        coordinate_sum += point.x() + point.y();
    }
    CHECK(coordinate_sum == Number(10));

    if constexpr (requires { halfplane.source().label(); }) {
        CHECK(halfplane.source().label() == "a");
        CHECK(halfplane.target().label() == "b");
        CHECK(halfplane.min().label() == "b");
        CHECK(halfplane.max().label() == "a");
    }
}

TEST_CASE("Halfplane streams, flips, scales, translates, and converts to boundary lines") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Halfplane halfplane(4, 3, 2, 1);

    std::ostringstream stream;
    stream << halfplane;
    CHECK(stream.str() == "^-(4,3)--(2,1)-^");

    const auto opposite = halfplane.opposite();
    const auto translated = halfplane + Point(1, 2);
    const auto shifted = translated - Point(1, 1);
    const auto scaled = 2 * halfplane;
    const Line line = static_cast<Line>(halfplane);
    const OrientedLine oriented_line = static_cast<OrientedLine>(halfplane);

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

TEST_CASE("Halfplane converts between labeled and unlabeled defining points") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainHalfplane = pgl::Halfplane<PlainPoint>;
    using LabelHalfplane = pgl::Halfplane<LabelPoint>;

    const LabelHalfplane labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainHalfplane plain_source(4, 3, 2, 1);

    const PlainHalfplane plain_from_labeled = labeled;
    const LabelHalfplane labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.source() == PlainPoint(4, 3));
    CHECK(plain_from_labeled.target() == PlainPoint(2, 1));
    CHECK(labeled_from_plain.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.source().label().empty());
    CHECK(labeled_from_plain.target().label().empty());

    PlainHalfplane plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.source() == PlainPoint(4, 3));
    CHECK(plain_assigned.target() == PlainPoint(2, 1));

    LabelHalfplane labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.source().label().empty());
    CHECK(labeled_assigned.target().label().empty());
}

TEST_CASE("Halfplane equality, ordering, and hashing depend on the oriented boundary line") {
    using Halfplane = pgl::Halfplane<pgl::Point<int>>;

    const Halfplane first({1, 2}, {3, 4});
    const Halfplane same({2, 3}, {5, 6});
    const Halfplane opposite({3, 4}, {1, 2});
    const Halfplane different({1, 2}, {1, 5});

    CHECK(first == same);
    CHECK_FALSE(first == opposite);
    CHECK_FALSE(first == different);
    CHECK_FALSE(first < same);
    const bool halfplanes_are_ordered = (first < opposite) || (opposite < first);
    CHECK(halfplanes_are_ordered);

    std::set<Halfplane> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(same);
    ordered_set.insert(opposite);
    ordered_set.insert(different);
    CHECK(ordered_set.size() == 3);

    std::unordered_set<Halfplane> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(same);
    unordered_set.insert(opposite);
    unordered_set.insert(different);
    CHECK(unordered_set.size() == 3);
}

TEST_CASE("Halfplane distinguishes defining points, boundary points, and interior points") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    CHECK(diagonal.verticesContain(Point(0, 0)));
    CHECK(diagonal.verticesContain(Point(4, 4)));
}

TEST_CASE("Halfplane boundary containment extends to linear primitives on the boundary line") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Line = pgl::Line<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    CHECK(diagonal.boundaryContains(Line({-2, -2}, {6, 6})));
    CHECK(diagonal.boundaryContains(Segment({1, 1}, {3, 3})));
    CHECK(diagonal.boundaryContains(Ray({1, 1}, {2, 2})));
    CHECK(diagonal.boundaryContains(Rectangle({2, 2}, {2, 2})));
    CHECK(diagonal.boundaryContains(Triangle({0, 0}, {1, 1}, {2, 2})));

    CHECK_FALSE(diagonal.boundaryContains(Segment({1, 1}, {3, 2})));
    CHECK_FALSE(diagonal.boundaryContains(Ray({1, 1}, {2, 3})));
    CHECK_FALSE(diagonal.boundaryContains(Rectangle({1, 1}, {3, 2})));
    CHECK_FALSE(diagonal.boundaryContains(Triangle({0, 0}, {1, 1}, {2, 3})));
}

TEST_CASE("Tests halfplane pointInside and interiorContains") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane horizontal({4, 0}, {0, 0});
    const Halfplane vertical({3, -1}, {3, 6});
    const Halfplane diagonal({0, 0}, {4, 4});
    const Halfplane descending({4, 0}, {0, 4});

    CHECK(horizontal.interiorContains(horizontal.pointInside()));
    CHECK(vertical.interiorContains(vertical.pointInside()));
    CHECK(diagonal.interiorContains(diagonal.pointInside()));
    CHECK(descending.interiorContains(descending.pointInside()));
}

TEST_CASE("Halfplane predicates handle segments and oriented segments") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Segment inside({1, 1}, {3, 2});
    const Segment boundary({1, 0}, {3, 0});
    const Segment crossing({1, -1}, {3, 2});
    const Segment outside({1, -2}, {3, -1});
    const OrientedSegment oriented_crossing({3, 2}, {1, -1});

    CHECK(upper.contains(inside));
    CHECK(upper.interiorContains(inside));
    CHECK(upper.contains(boundary));
    CHECK_FALSE(upper.interiorContains(boundary));
    CHECK(upper.intersects(crossing));
    CHECK(upper.interiorsIntersect(crossing));
    CHECK(upper.intersects(oriented_crossing));
    CHECK(upper.interiorsIntersect(oriented_crossing));
    CHECK_FALSE(upper.contains(crossing));
    CHECK_FALSE(upper.intersects(outside));
    CHECK_FALSE(upper.separates(crossing));
    CHECK_FALSE(upper.crosses(crossing));
}

TEST_CASE("Halfplane predicates handle lines and oriented lines") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Line inside_parallel({0, 2}, {4, 2});
    const Line boundary({0, 0}, {4, 0});
    const Line outside_parallel({0, -2}, {4, -2});
    const Line crossing({0, -1}, {4, 3});
    const OrientedLine oriented_crossing({4, 3}, {0, -1});

    CHECK(upper.contains(inside_parallel));
    CHECK(upper.interiorContains(inside_parallel));
    CHECK(upper.contains(boundary));
    CHECK_FALSE(upper.interiorContains(boundary));
    CHECK(upper.intersects(crossing));
    CHECK(upper.interiorsIntersect(crossing));
    CHECK(upper.intersects(oriented_crossing));
    CHECK(upper.interiorsIntersect(oriented_crossing));
    CHECK_FALSE(upper.intersects(outside_parallel));
}

TEST_CASE("Halfplane predicates handle rays") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Ray inside({1, 1}, {3, 2});
    const Ray leaving({1, 1}, {3, 0});
    const Ray entering({1, -2}, {3, 1});
    const Ray outside_parallel({1, -1}, {3, -1});
    const Ray boundary({1, 0}, {3, 0});

    CHECK(upper.contains(inside));
    CHECK(upper.interiorContains(inside));
    CHECK_FALSE(upper.contains(leaving));
    CHECK(upper.intersects(leaving));
    CHECK(upper.interiorsIntersect(leaving));
    CHECK_FALSE(upper.contains(entering));
    CHECK(upper.intersects(entering));
    CHECK(upper.interiorsIntersect(entering));
    CHECK_FALSE(upper.intersects(outside_parallel));
    CHECK(upper.contains(boundary));
    CHECK_FALSE(upper.interiorContains(boundary));
}

TEST_CASE("Halfplane predicates handle rectangles") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle inside({1, 1}, {3, 3});
    const Rectangle touching({1, 0}, {3, 2});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle outside({1, -3}, {3, -1});

    CHECK(upper.contains(inside));
    CHECK(upper.interiorContains(inside));
    CHECK(upper.contains(touching));
    CHECK_FALSE(upper.interiorContains(touching));
    CHECK(upper.intersects(crossing));
    CHECK(upper.interiorsIntersect(crossing));
    CHECK_FALSE(upper.contains(crossing));
    CHECK_FALSE(upper.intersects(outside));
}

TEST_CASE("Halfplane per-axis scaling keeps the correct inside side under reflection") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    // Left of (0,0)->(1,0) is the upper half-plane (y >= 0).
    const Halfplane upper({0, 0}, {1, 0});
    REQUIRE(upper.contains(Point(5, 3)));
    REQUIRE_FALSE(upper.contains(Point(5, -3)));

    // A positive factor scales the geometry without flipping the side.
    const Halfplane taller = upper.scaledUpY(2);
    CHECK(taller.contains(Point(0, 1)));
    CHECK_FALSE(taller.contains(Point(0, -1)));

    // scaledUpX(-1) reflects across x=0; the upper half-plane maps to itself.
    const Halfplane reflectedX = upper.scaledUpX(-1);
    CHECK(reflectedX.contains(Point(5, 3)));
    CHECK_FALSE(reflectedX.contains(Point(5, -3)));

    // scaledUpY(-1) reflects across y=0; the upper half-plane becomes the lower.
    const Halfplane reflectedY = upper.scaledUpY(-1);
    CHECK_FALSE(reflectedY.contains(Point(5, 3)));
    CHECK(reflectedY.contains(Point(5, -3)));

    // In-place mutation matches the returning form.
    Halfplane h = upper;
    h.scaleUpY(-1);
    CHECK(h == reflectedY);
}

TEST_CASE("Halfplane interior intersection distinguishes true overlap from boundary-only contact") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Halfplane left({0, -1}, {0, 1});
    const Halfplane right({0, 1}, {0, -1});
    const Halfplane wider_left({1, -1}, {1, 1});
    const Halfplane same_left({0, 2}, {0, 4});
    const Halfplane stricter_left({-1, -1}, {-1, 1});
    const Halfplane rising({0, 0}, {2, 2});
    const Halfplane falling({0, 2}, {2, 0});

    CHECK(left.intersects(right));
    CHECK_FALSE(left.interiorsIntersect(right));
    CHECK(left.interiorsIntersect(wider_left));
    CHECK(left.interiorsIntersect(same_left));
    CHECK(left.interiorsIntersect(stricter_left));
    CHECK(rising.interiorsIntersect(falling));

    const Halfplane upper({0, 0}, {4, 0});
    const Triangle boundary_only({0, 0}, {2, 0}, {1, -1});
    const Triangle entering({0, 0}, {2, 0}, {1, 1});

    CHECK(upper.intersects(boundary_only));
    CHECK_FALSE(upper.interiorsIntersect(boundary_only));
    CHECK(upper.interiorsIntersect(entering));
}

TEST_CASE("Halfplane covers the non-Convex contract for topology predicates") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Halfplane upper({0, 0}, {4, 0});

    CHECK(upper.interiorsIntersect(Line({0, -1}, {4, 3})));
    CHECK(upper.interiorsIntersect(OrientedLine({4, 3}, {0, -1})));
    CHECK(upper.interiorsIntersect(Segment({1, -1}, {3, 2})));
    CHECK(upper.interiorsIntersect(OrientedSegment({3, 2}, {1, -1})));
    CHECK(upper.interiorsIntersect(Ray({1, -2}, {3, 1})));
    CHECK(upper.interiorsIntersect(Rectangle({1, -1}, {3, 2})));
    CHECK(upper.interiorsIntersect(Halfplane({1, -1}, {1, 1})));
    CHECK(upper.interiorsIntersect(Triangle({0, 0}, {2, 0}, {1, 1})));
    CHECK(upper.interiorsIntersect(Shape(Rectangle({1, -1}, {3, 2}))));

    CHECK_FALSE(upper.separates(Line({0, -1}, {4, 3})));
    CHECK_FALSE(upper.separates(OrientedLine({4, 3}, {0, -1})));
    CHECK_FALSE(upper.separates(Segment({1, -1}, {3, 2})));
    CHECK_FALSE(upper.separates(OrientedSegment({3, 2}, {1, -1})));
    CHECK_FALSE(upper.separates(Ray({1, -2}, {3, 1})));
    CHECK_FALSE(upper.separates(Rectangle({1, -1}, {3, 2})));
    CHECK_FALSE(upper.separates(Halfplane({1, -1}, {1, 1})));
    CHECK_FALSE(upper.separates(Triangle({0, 0}, {2, 0}, {1, 1})));
    CHECK_FALSE(upper.separates(Shape(Rectangle({1, -1}, {3, 2}))));

    CHECK_FALSE(upper.crosses(Line({0, -1}, {4, 3})));
    CHECK_FALSE(upper.crosses(OrientedLine({4, 3}, {0, -1})));
    CHECK_FALSE(upper.crosses(Segment({1, -1}, {3, 2})));
    CHECK_FALSE(upper.crosses(OrientedSegment({3, 2}, {1, -1})));
    CHECK_FALSE(upper.crosses(Ray({1, -2}, {3, 1})));
    CHECK_FALSE(upper.crosses(Rectangle({1, -1}, {3, 2})));
    CHECK_FALSE(upper.crosses(Halfplane({1, -1}, {1, 1})));
    CHECK_FALSE(upper.crosses(Triangle({0, 0}, {2, 0}, {1, 1})));
    CHECK_FALSE(upper.crosses(Shape(Rectangle({1, -1}, {3, 2}))));
}

TEST_CASE("Halfplane intersections clip lines and oriented lines into points, rays, or whole lines") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using Halfplane = pgl::Halfplane<Point>;
    using Line = pgl::Line<Point>;
    using RationalRay = pgl::Ray<RationalPoint>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Halfplane upper({0, 0}, {4, 0});

    const auto crossing = upper.intersection<Rational>(Line({0, -1}, {3, 1}));
    REQUIRE(crossing);
    REQUIRE(std::holds_alternative<RationalRay>(*crossing));
    const auto& crossing_ray = std::get<RationalRay>(*crossing);
    CHECK(crossing_ray.source() == RationalPoint(Rational(3, 2), Rational(0)));
    CHECK(crossing_ray.contains(RationalPoint(3, 1)));
    CHECK(upper.contains(crossing_ray.target()));

    const auto inside_parallel = upper.intersection(Line({0, 2}, {4, 2}));
    REQUIRE(inside_parallel);
    REQUIRE(std::holds_alternative<Line>(*inside_parallel));
    CHECK(std::get<Line>(*inside_parallel) == Line({0, 2}, {4, 2}));

    const auto boundary = upper.intersection(Line({0, 0}, {4, 0}));
    REQUIRE(boundary);
    REQUIRE(std::holds_alternative<Line>(*boundary));
    CHECK(std::get<Line>(*boundary) == Line({0, 0}, {4, 0}));

    CHECK_FALSE(upper.intersection(Line({0, -2}, {4, -2})));

    const auto oriented_crossing = upper.intersection<Rational>(OrientedLine({3, 1}, {0, -1}));
    REQUIRE(oriented_crossing);
    REQUIRE(std::holds_alternative<RationalRay>(*oriented_crossing));
    CHECK(std::get<RationalRay>(*oriented_crossing).source() == RationalPoint(Rational(3, 2), Rational(0)));

    const Halfplane degenerate({0, 0}, {0, 0});
    const auto degenerate_hit = degenerate.intersection(Line({-1, -1}, {1, 1}));
    REQUIRE(degenerate_hit);
    REQUIRE(std::holds_alternative<Point>(*degenerate_hit));
    CHECK(std::get<Point>(*degenerate_hit) == Point(0, 0));
}

TEST_CASE("Halfplane intersections clip segments and oriented segments") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using Halfplane = pgl::Halfplane<Point>;
    using Segment = pgl::Segment<Point>;
    using RationalSegment = pgl::Segment<RationalPoint>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Halfplane upper({0, 0}, {4, 0});

    const auto crossing = upper.intersection<Rational>(Segment({0, -1}, {3, 1}));
    REQUIRE(crossing);
    REQUIRE(std::holds_alternative<RationalSegment>(*crossing));
    CHECK(std::get<RationalSegment>(*crossing) == RationalSegment(RationalPoint(Rational(3, 2), Rational(0)),
                                                                  RationalPoint(Rational(3), Rational(1))));

    const auto inside = upper.intersection(Segment({1, 1}, {3, 2}));
    REQUIRE(inside);
    REQUIRE(std::holds_alternative<Segment>(*inside));
    CHECK(std::get<Segment>(*inside) == Segment({1, 1}, {3, 2}));

    const auto boundary = upper.intersection(Segment({1, 0}, {3, 0}));
    REQUIRE(boundary);
    REQUIRE(std::holds_alternative<Segment>(*boundary));
    CHECK(std::get<Segment>(*boundary) == Segment({1, 0}, {3, 0}));

    const auto touching = upper.intersection(Segment({0, -1}, {0, 0}));
    REQUIRE(touching);
    REQUIRE(std::holds_alternative<Point>(*touching));
    CHECK(std::get<Point>(*touching) == Point(0, 0));

    CHECK_FALSE(upper.intersection(Segment({1, -3}, {3, -1})));

    const auto oriented_crossing = upper.intersection<Rational>(OrientedSegment({3, 1}, {0, -1}));
    REQUIRE(oriented_crossing);
    REQUIRE(std::holds_alternative<RationalSegment>(*oriented_crossing));
    CHECK(std::get<RationalSegment>(*oriented_crossing) == RationalSegment(RationalPoint(Rational(3, 2), Rational(0)),
                                                                           RationalPoint(Rational(3), Rational(1))));
}

TEST_CASE("Halfplane intersections clip rays into points, segments, or rays") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;
    using Segment = pgl::Segment<Point>;
    using RationalRay = pgl::Ray<RationalPoint>;

    const Halfplane upper({0, 0}, {4, 0});

    const auto inside = upper.intersection(Ray({1, 1}, {3, 2}));
    REQUIRE(inside);
    REQUIRE(std::holds_alternative<Ray>(*inside));
    CHECK(std::get<Ray>(*inside) == Ray({1, 1}, {3, 2}));

    const auto leaving = upper.intersection(Ray({1, 1}, {3, -1}));
    REQUIRE(leaving);
    REQUIRE(std::holds_alternative<Segment>(*leaving));
    CHECK(std::get<Segment>(*leaving) == Segment({1, 1}, {2, 0}));

    const auto entering = upper.intersection<Rational>(Ray({0, -1}, {3, 1}));
    REQUIRE(entering);
    REQUIRE(std::holds_alternative<RationalRay>(*entering));
    CHECK(std::get<RationalRay>(*entering) == RationalRay(RationalPoint(Rational(3, 2), Rational(0)),
                                                          RationalPoint(Rational(3), Rational(1))));

    const auto boundary = upper.intersection(Ray({1, 0}, {3, 0}));
    REQUIRE(boundary);
    REQUIRE(std::holds_alternative<Ray>(*boundary));
    CHECK(std::get<Ray>(*boundary) == Ray({1, 0}, {3, 0}));

    const auto touching = upper.intersection(Ray({0, 0}, {1, -1}));
    REQUIRE(touching);
    REQUIRE(std::holds_alternative<Point>(*touching));
    CHECK(std::get<Point>(*touching) == Point(0, 0));

    CHECK_FALSE(upper.intersection(Ray({1, -2}, {3, -3})));
}

TEST_CASE("Halfplane contains and interiorContains another half-plane") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    // Left of (0,0)->(4,0) is the upper half-plane (interior y > 0, boundary y = 0).
    const Halfplane upper({0, 0}, {4, 0});

    // A half-plane contains another half-plane only when the latter degenerates
    // to a point (source == target); that point must lie in the closed half-plane.
    CHECK(upper.contains(Halfplane({2, 3}, {2, 3})));         // degenerate point, strictly inside
    CHECK(upper.contains(Halfplane({2, 0}, {2, 0})));         // degenerate point, on the boundary
    CHECK_FALSE(upper.contains(Halfplane({2, -3}, {2, -3}))); // degenerate point, outside
    CHECK_FALSE(upper.contains(upper));                       // a proper half-plane is never contained

    // interiorContains additionally requires the degenerate point to be strictly inside.
    CHECK(upper.interiorContains(Halfplane({2, 3}, {2, 3})));
    CHECK_FALSE(upper.interiorContains(Halfplane({2, 0}, {2, 0})));  // on the boundary
    CHECK_FALSE(upper.interiorContains(upper));
}

TEST_CASE("Halfplane measures squared distance as zero inside or distance to its boundary line") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rational = pgl::Rational<int64_t>;

    // Left of (0,0)->(4,0) is the upper half-plane (interior y > 0, boundary y = 0).
    const Halfplane upper({0, 0}, {4, 0});

    SUBCASE("shapes meeting the half-plane are at distance zero") {
        CHECK(upper.squaredDistance(Point(5, 5)) == 0);                        // interior point
        CHECK(upper.squaredDistance(Point(5, 0)) == 0);                        // on the boundary
        CHECK(upper.squaredDistance(pgl::Segment<Point>({1, -2}, {1, 3})) == 0); // crosses boundary
        CHECK(upper.squaredDistance(pgl::Line<Point>({0, 1}, {1, 2})) == 0);   // non-parallel line always enters
        CHECK(upper.squaredDistance(pgl::Ray<Point>({2, -1}, {2, 5})) == 0);   // ray enters the half-plane
    }

    SUBCASE("shapes outside fall back to the boundary line distance") {
        CHECK(upper.squaredDistance(Point(5, -5)) == 25);
        CHECK(upper.squaredDistance(pgl::Segment<Point>({2, -3}, {8, -7})) == 9);   // nearest endpoint (2,-3)
        CHECK(upper.squaredDistance(pgl::OrientedSegment<Point>({2, -3}, {8, -7})) == 9);
        CHECK(upper.squaredDistance(pgl::Line<Point>({0, -4}, {1, -4})) == 16);     // parallel line y=-4
        CHECK(upper.squaredDistance(pgl::OrientedLine<Point>({1, -4}, {0, -4})) == 16);
        CHECK(upper.squaredDistance(pgl::Ray<Point>({2, -3}, {2, -9})) == 9);       // points further away

        // Each equals the distance to the boundary line asLine().
        CHECK(upper.squaredDistance(Point(5, -5)) == upper.asLine().squaredDistance(Point(5, -5)));
    }

    SUBCASE("the relation is symmetric and honors ResultNumber") {
        const Point outside(5, -5);
        CHECK(outside.squaredDistance(upper) == upper.squaredDistance(outside));

        // A point whose squared distance to the boundary is fractional.
        const Halfplane diagonal({0, 0}, {2, 2});            // boundary y = x
        const Point q(3, 0);                                 // exact squared distance 9/2
        CHECK(diagonal.squaredDistance(q) == 4);             // integer default truncates
        CHECK(diagonal.squaredDistance<double>(q) == doctest::Approx(4.5));
        CHECK(diagonal.squaredDistance<Rational>(q) == Rational(9, 2));
    }
}
