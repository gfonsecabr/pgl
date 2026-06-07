#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>

#include "pgl.hpp"


TEST_CASE_TEMPLATE("Line orders its defining points and preserves exact defining labels when possible", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Line = pgl::Line<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Line degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.min() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.max() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.min() == Point(Number{}, Number{}));
        CHECK(degenerate.max() == Point(Number{}, Number{}));
    }

    const Line line(make_point(static_cast<Number>(4), static_cast<Number>(3), "a"),
                    make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));

    CHECK(line[0].x() == Number(2));
    CHECK(line[0].y() == Number(1));
    CHECK(line[1].x() == Number(4));
    CHECK(line[1].y() == Number(3));

    Number coordinate_sum{};
    for (const auto& point : line) {
        coordinate_sum += point.x() + point.y();
    }
    CHECK(coordinate_sum == Number(10));

    if constexpr (requires { line.min().label(); }) {
        CHECK(line.min().label() == "b");
        CHECK(line.max().label() == "a");
    }
}

TEST_CASE("Line streams, translates, and scales while preserving geometry") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;

    const Line line(4, 3, 2, 1);

    std::ostringstream stream;
    stream << line;
    CHECK(stream.str() == "-(2,1)--(4,3)-");

    const auto translated = line + Point(1, 2);
    const auto shifted = translated - Point(1, 1);
    const auto scaled = 2 * line;

    CHECK(translated.min() == Point(3, 3));
    CHECK(translated.max() == Point(5, 5));
    CHECK(shifted.min() == Point(2, 2));
    CHECK(shifted.max() == Point(4, 4));
    CHECK(scaled.min() == Point(4, 2));
    CHECK(scaled.max() == Point(8, 6));
}

TEST_CASE("Line converts between labeled and unlabeled defining points") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainLine = pgl::Line<PlainPoint>;
    using LabelLine = pgl::Line<LabelPoint>;

    const LabelLine labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainLine plain_source(4, 3, 2, 1);

    const PlainLine plain_from_labeled = labeled;
    const LabelLine labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.min() == PlainPoint(2, 1));
    CHECK(plain_from_labeled.max() == PlainPoint(4, 3));
    CHECK(labeled_from_plain.min() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.max() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.min().label().empty());
    CHECK(labeled_from_plain.max().label().empty());

    PlainLine plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.min() == PlainPoint(2, 1));
    CHECK(plain_assigned.max() == PlainPoint(4, 3));

    LabelLine labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.min() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.max() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.min().label().empty());
    CHECK(labeled_assigned.max().label().empty());
}

TEST_CASE("Line builds geometric above and below half-planes") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;

    const Line diagonal({4, 4}, {0, 0});
    const auto above = diagonal.halfplaneAbove();
    const auto below = diagonal.halfplaneBelow();

    CHECK(above.contains(Point(1, 0)));
    CHECK(above.boundaryContains(Point(2, 2)));
    CHECK_FALSE(above.contains(Point(0, 1)));

    CHECK(below.contains(Point(0, 1)));
    CHECK_FALSE(below.contains(Point(1, 0)));

    const Line vertical({2, 4}, {2, -1});
    CHECK(vertical.halfplaneAbove().contains(Point(3, 0)));
    CHECK_FALSE(vertical.halfplaneAbove().contains(Point(1, 0)));
}

TEST_CASE("Line equality, ordering, and hashing depend on the represented geometric line") {
    using Line = pgl::Line<pgl::Point<int>>;

    const Line first({1, 2}, {3, 4});
    const Line second({2, 3}, {5, 6});
    const Line different({1, 2}, {1, 5});

    CHECK(first == second);
    CHECK_FALSE(first == different);
    CHECK_FALSE(first < second);
    const bool lines_are_ordered = (first < different) || (different < first);
    CHECK(lines_are_ordered);

    std::set<Line> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(second);
    ordered_set.insert(different);
    CHECK(ordered_set.size() == 2);

    std::unordered_set<Line> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(second);
    unordered_set.insert(different);
    CHECK(unordered_set.size() == 2);
}

TEST_CASE("Line distinguishes containment, collinearity, parallelism, and intersections") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Line diagonal({0, 0}, {4, 4});
    const Line same_line({1, 1}, {3, 3});
    const Line crossing({0, 4}, {4, 0});
    const Line parallel({0, 1}, {4, 5});
    const Segment subsegment({1, 1}, {3, 3});
    const OrientedSegment oriented_subsegment({3, 3}, {1, 1});

    CHECK(diagonal.contains(Point(2, 2)));
    CHECK(diagonal.interiorContains(Point(2, 2)));
    CHECK_FALSE(diagonal.boundaryContains(Point(2, 2)));

    CHECK(diagonal.contains(same_line));
    CHECK(diagonal.contains(subsegment));
    CHECK(diagonal.contains(oriented_subsegment));
    CHECK(diagonal.collinear(subsegment));
    CHECK(diagonal.collinear(oriented_subsegment));
    CHECK(diagonal.collinear(same_line));

    CHECK(diagonal.parallel(parallel));
    CHECK_FALSE(diagonal.parallel(crossing));

    CHECK(diagonal.intersects(same_line));
    CHECK(diagonal.intersects(crossing));
    CHECK_FALSE(diagonal.intersects(parallel));
    CHECK(diagonal.intersects(Point(2, 2)));
    CHECK_FALSE(diagonal.intersects(Point(2, 3)));
    CHECK(diagonal.interiorsIntersect(crossing));
    CHECK(diagonal.crosses(crossing));
}

TEST_CASE("Line intersection and distances support exact rational results") {
    using Rational = pgl::Rational<int64_t>;
    using IntLine = pgl::Line<pgl::Point<int>>;
    using RationalPoint = pgl::Point<Rational>;

    const IntLine rising({0, 0}, {4, 4});
    const IntLine falling({0, 3}, {6, 0});
    const IntLine parallel({0, 1}, {4, 5});

    const auto intersection = rising.intersection<Rational>(falling);
    REQUIRE(intersection);
    REQUIRE(std::holds_alternative<RationalPoint>(*intersection));
    CHECK(std::get<RationalPoint>(*intersection) == RationalPoint(Rational(2), Rational(2)));

    const auto same_line_intersection = rising.intersection<Rational>(IntLine({1, 1}, {3, 3}));
    REQUIRE(same_line_intersection);
    REQUIRE(std::holds_alternative<pgl::Line<pgl::Point<Rational>>>(*same_line_intersection));

    CHECK(rising.intersection(pgl::Point<int>(2, 2)) == pgl::Point<int>(2, 2));
    CHECK_FALSE(rising.intersection(pgl::Point<int>(2, 3)));

    CHECK(rising.squaredDistance(pgl::Point<int>(2, 0)) == doctest::Approx(2.0));
    CHECK(rising.squaredDistance(parallel) == doctest::Approx(0.5));
}

TEST_CASE("Line evaluates coordinates at fixed x and y when the query is well-defined") {
    using Line = pgl::Line<pgl::Point<int>>;
    using Rational = pgl::Rational<int64_t>;

    const Line diagonal({0, 0}, {4, 4});
    const Line vertical({2, -1}, {2, 3});
    const Line horizontal({-2, 1}, {4, 1});
    const Line degenerate({3, 5}, {3, 5});

    CHECK(diagonal.yAtX(2) == 2);
    CHECK(diagonal.xAtY(3) == 3);
    CHECK(diagonal.yAtX<Rational>(1).value() == Rational(1));
    CHECK(diagonal.xAtY<Rational>(1).value() == Rational(1));

    CHECK(vertical.yAtX(2) == -1);
    CHECK_FALSE(vertical.yAtX(1).has_value());
    CHECK(vertical.xAtY(0) == 2);

    CHECK(horizontal.xAtY(1) == -2);
    CHECK_FALSE(horizontal.xAtY(0).has_value());
    CHECK(horizontal.yAtX(3) == 1);

    CHECK(degenerate.yAtX(3) == 5);
    CHECK_FALSE(degenerate.yAtX(2).has_value());
    CHECK(degenerate.xAtY(5) == 3);
    CHECK_FALSE(degenerate.xAtY(4).has_value());
}

TEST_CASE("Line separates and crosses 1D targets using the topological definition") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Line vertical({2, -2}, {2, 2});
    const Line horizontal({0, 0}, {4, 0});
    const Segment segment({0, 0}, {4, 0});
    const Segment collinear_segment({1, 0}, {3, 0});
    const Segment endpoint_touching_segment({2, 0}, {2, 3});
    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle box({1, -1}, {3, 1});
    const Ray ray({0, 0}, {4, 0});
    const Ray source_touching_ray({2, 0}, {2, 3});
    const Triangle triangle({1, -1}, {3, -1}, {2, 2});

    CHECK_FALSE(vertical.crosses(Point(2, 0)));
    CHECK(vertical.separates(horizontal));
    CHECK(vertical.separates(segment));
    CHECK_FALSE(vertical.separates(endpoint_touching_segment));
    CHECK(vertical.separates(ray));
    CHECK_FALSE(vertical.separates(source_touching_ray));
    CHECK(vertical.crosses(horizontal));
    CHECK(vertical.crosses(segment));
    CHECK(vertical.crosses(ray));
    CHECK_FALSE(vertical.crosses(upper));
    CHECK(vertical.crosses(box));
    CHECK(vertical.crosses(triangle));

    CHECK_FALSE(horizontal.separates(collinear_segment));
    CHECK_FALSE(horizontal.crosses(collinear_segment));
}

TEST_CASE("Line covers the non-Convex contract for interior, separation, and crossing predicates") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Shape = pgl::Shape<Point>;

    const Line vertical({2, -2}, {2, 2});

    CHECK(vertical.interiorsIntersect(Point(2, 0)));
    CHECK(vertical.interiorsIntersect(Segment({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(OrientedSegment({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(OrientedLine({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(Ray({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(Halfplane({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(Rectangle({1, -1}, {3, 1})));
    CHECK(vertical.interiorsIntersect(Triangle({1, -1}, {3, -1}, {2, 2})));

    CHECK_FALSE(vertical.separates(Point(2, 0)));
    CHECK(vertical.separates(Segment({0, 0}, {4, 0})));
    CHECK(vertical.separates(OrientedLine({0, 0}, {4, 0})));
    CHECK_FALSE(vertical.separates(Halfplane({0, 0}, {4, 0})));
    CHECK(vertical.separates(Triangle({1, -1}, {3, -1}, {2, 2})));

    CHECK_FALSE(vertical.crosses(Point(2, 0)));
    CHECK(vertical.crosses(Shape(Segment({0, 0}, {4, 0}))));
    CHECK(vertical.crosses(Ray({0, 0}, {4, 0})));
    CHECK_FALSE(vertical.crosses(Halfplane({0, 0}, {4, 0})));
    CHECK(vertical.crosses(Rectangle({1, -1}, {3, 1})));
}
