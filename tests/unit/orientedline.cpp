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


TEST_CASE_TEMPLATE("OrientedLine preserves source and target order while exposing min and max", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using OrientedLine = pgl::OrientedLine<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const OrientedLine degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.source() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.target() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.source() == Point(Number{}, Number{}));
        CHECK(degenerate.target() == Point(Number{}, Number{}));
    }

    const OrientedLine line(make_point(static_cast<Number>(4), static_cast<Number>(3), "a"),
                            make_point(static_cast<Number>(2), static_cast<Number>(1), "b"));

    CHECK(line.source().x() == Number(4));
    CHECK(line.source().y() == Number(3));
    CHECK(line.target().x() == Number(2));
    CHECK(line.target().y() == Number(1));
    CHECK(line.min().x() == Number(2));
    CHECK(line.min().y() == Number(1));
    CHECK(line.max().x() == Number(4));
    CHECK(line.max().y() == Number(3));

    Number coordinate_sum{};
    for (const auto& point : line) {
        coordinate_sum += point.x() + point.y();
    }
    CHECK(coordinate_sum == Number(10));

    if constexpr (requires { line.source().label(); }) {
        CHECK(line.source().label() == "a");
        CHECK(line.target().label() == "b");
        CHECK(line.min().label() == "b");
        CHECK(line.max().label() == "a");
    }
}

TEST_CASE("OrientedLine streams, flips, scales, translates, and converts to Line") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine line(4, 3, 2, 1);

    std::ostringstream stream;
    stream << line;
    CHECK(stream.str() == "-(4,3)--(2,1)->");

    const auto opposite = line.opposite();
    const auto translated = line + Point(1, 2);
    const auto shifted = translated - Point(1, 1);
    const auto scaled = 2 * line;
    const Line unoriented = static_cast<Line>(line);

    CHECK(opposite.source() == Point(2, 1));
    CHECK(opposite.target() == Point(4, 3));
    CHECK(translated.source() == Point(5, 5));
    CHECK(translated.target() == Point(3, 3));
    CHECK(shifted.source() == Point(4, 4));
    CHECK(shifted.target() == Point(2, 2));
    CHECK(scaled.source() == Point(8, 6));
    CHECK(scaled.target() == Point(4, 2));
    CHECK(unoriented == Line(Point(2, 1), Point(4, 3)));
}

TEST_CASE("OrientedLine converts between labeled and unlabeled defining points") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainLine = pgl::OrientedLine<PlainPoint>;
    using LabelLine = pgl::OrientedLine<LabelPoint>;

    const LabelLine labeled(LabelPoint(4, 3, "a"), LabelPoint(2, 1, "b"));
    const PlainLine plain_source(4, 3, 2, 1);

    const PlainLine plain_from_labeled = labeled;
    const LabelLine labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.source() == PlainPoint(4, 3));
    CHECK(plain_from_labeled.target() == PlainPoint(2, 1));
    CHECK(labeled_from_plain.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_from_plain.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_from_plain.source().label().empty());
    CHECK(labeled_from_plain.target().label().empty());

    PlainLine plain_assigned;
    plain_assigned = labeled;
    CHECK(plain_assigned.source() == PlainPoint(4, 3));
    CHECK(plain_assigned.target() == PlainPoint(2, 1));

    LabelLine labeled_assigned;
    labeled_assigned = plain_source;
    CHECK(labeled_assigned.source() == LabelPoint(4, 3, ""));
    CHECK(labeled_assigned.target() == LabelPoint(2, 1, ""));
    CHECK(labeled_assigned.source().label().empty());
    CHECK(labeled_assigned.target().label().empty());
}

TEST_CASE("OrientedLine builds geometric and orientation-dependent half-planes") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine line({4, 4}, {0, 0});

    CHECK(line.halfplaneAbove().contains(Point(1, 0)));
    CHECK_FALSE(line.halfplaneAbove().contains(Point(0, 1)));

    CHECK(line.halfplaneBelow().contains(Point(0, 1)));
    CHECK_FALSE(line.halfplaneBelow().contains(Point(1, 0)));

    CHECK(line.leftHalfplane().contains(Point(1, 0)));
    CHECK_FALSE(line.leftHalfplane().contains(Point(0, 1)));

    CHECK(line.rightHalfplane().contains(Point(0, 1)));
    CHECK_FALSE(line.rightHalfplane().contains(Point(1, 0)));
}

TEST_CASE("OrientedLine equality, ordering, and hashing depend on the represented oriented geometric line") {
    using OrientedLine = pgl::OrientedLine<pgl::Point<int>>;

    const OrientedLine first({1, 2}, {3, 4});
    const OrientedLine same({2, 3}, {5, 6});
    const OrientedLine opposite({3, 4}, {1, 2});
    const OrientedLine different({1, 2}, {1, 5});

    CHECK(first == same);
    CHECK_FALSE(first == opposite);
    CHECK_FALSE(first == different);
    CHECK_FALSE(first < same);
    const bool oriented_lines_are_ordered = (first < opposite) || (opposite < first);
    CHECK(oriented_lines_are_ordered);

    std::set<OrientedLine> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(same);
    ordered_set.insert(opposite);
    ordered_set.insert(different);
    CHECK(ordered_set.size() == 3);

    std::unordered_set<OrientedLine> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(same);
    unordered_set.insert(opposite);
    unordered_set.insert(different);
    CHECK(unordered_set.size() == 3);
}

TEST_CASE("OrientedLine distinguishes geometric containment from stored orientation") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diagonal({0, 0}, {4, 4});
    const OrientedLine same_orientation({1, 1}, {3, 3});
    const OrientedLine opposite_orientation({3, 3}, {1, 1});
    const OrientedLine crossing({0, 4}, {4, 0});
    const OrientedLine parallel({0, 1}, {4, 5});
    const Point on_diagonal(2, 2);
    const Point off_diagonal(2, 3);
    const Line supporting_line({2, 2}, {6, 6});
    const Segment subsegment({1, 1}, {3, 3});
    const OrientedSegment oriented_subsegment({3, 3}, {1, 1});

    CHECK(diagonal.contains(Point(2, 2)));
    CHECK(diagonal.interiorContains(Point(2, 2)));
    CHECK_FALSE(diagonal.boundaryContains(Point(2, 2)));

    CHECK(diagonal.contains(supporting_line));
    CHECK(diagonal.contains(same_orientation));
    CHECK(diagonal.contains(opposite_orientation));
    CHECK(diagonal.contains(subsegment));
    CHECK(diagonal.contains(oriented_subsegment));
    CHECK(diagonal.collinear(supporting_line));
    CHECK(diagonal.collinear(opposite_orientation));
    CHECK(diagonal.collinear(oriented_subsegment));

    CHECK(diagonal.orientation(Point(0, 1)) == std::partial_ordering::greater);
    CHECK(diagonal.orientation(Point(1, 0)) == std::partial_ordering::less);
    CHECK(diagonal.orientation(Point(2, 2)) == std::partial_ordering::equivalent);

    CHECK(diagonal.parallel(parallel));
    CHECK(diagonal.parallel(Line({0, 2}, {4, 6})));
    CHECK_FALSE(diagonal.parallel(crossing));

    CHECK(diagonal.intersects(on_diagonal));
    CHECK_FALSE(diagonal.intersects(off_diagonal));
    CHECK(diagonal.intersects(same_orientation));
    CHECK(diagonal.intersects(opposite_orientation));
    CHECK(diagonal.intersects(crossing));
    CHECK_FALSE(diagonal.intersects(parallel));
    CHECK(diagonal.interiorsIntersect(crossing));
    CHECK(diagonal.crosses(crossing));
    CHECK_FALSE(diagonal.crosses(opposite_orientation));

    CHECK_FALSE(diagonal.crosses(pgl::Point<int>(2, 2)));
    CHECK(diagonal.crosses(pgl::Segment<pgl::Point<int>>({0, 4}, {4, 0})));
    CHECK(diagonal.crosses(pgl::Ray<pgl::Point<int>>({0, 4}, {4, 0})));
    CHECK_FALSE(diagonal.crosses(pgl::Halfplane<pgl::Point<int>>({0, 0}, {4, 4})));
}

TEST_CASE("OrientedLine intersection and distances support exact rational results") {
    using Rational = pgl::Rational<int64_t>;
    using IntLine = pgl::OrientedLine<pgl::Point<int>>;
    using RationalPoint = pgl::Point<Rational>;

    const IntLine rising({0, 0}, {4, 4});
    const IntLine falling({6, 0}, {0, 3});
    const IntLine parallel({0, 1}, {4, 5});
    const pgl::Point<int> on_rising(3, 3);
    const pgl::Point<int> off_rising(3, 2);

    const auto point_intersection = rising.intersection<Rational>(on_rising);
    REQUIRE(point_intersection);
    CHECK(*point_intersection == RationalPoint(Rational(3), Rational(3)));

    CHECK_FALSE(rising.intersection<Rational>(off_rising).has_value());

    const auto intersection = rising.intersection<Rational>(falling);
    REQUIRE(intersection);
    REQUIRE(std::holds_alternative<RationalPoint>(*intersection));
    CHECK(std::get<RationalPoint>(*intersection) == RationalPoint(Rational(2), Rational(2)));

    const auto same_line_intersection = rising.intersection<Rational>(IntLine({1, 1}, {3, 3}));
    REQUIRE(same_line_intersection);
    REQUIRE(std::holds_alternative<pgl::Line<pgl::Point<Rational>>>(*same_line_intersection));

    CHECK(rising.squaredDistance(pgl::Point<int>(2, 0)) == doctest::Approx(2.0));
    CHECK(rising.squaredDistance(parallel) == doctest::Approx(0.5));
}

TEST_CASE("OrientedLine evaluates coordinates like its supporting line") {
    using OrientedLine = pgl::OrientedLine<pgl::Point<int>>;
    using Rational = pgl::Rational<int64_t>;

    const OrientedLine diagonal({4, 4}, {0, 0});
    const OrientedLine vertical({2, 3}, {2, -1});
    const OrientedLine horizontal({4, 1}, {-2, 1});

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
}

TEST_CASE("OrientedLine covers the non-Convex contract through Line delegation") {
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

    const OrientedLine vertical({2, 2}, {2, -2});

    CHECK(vertical.interiorsIntersect(Point(2, 0)));  // on the line (its interior is the whole line)
    CHECK(vertical.interiorsIntersect(Segment({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(OrientedSegment({0, 0}, {4, 0})));
    CHECK(vertical.interiorsIntersect(Line({0, 0}, {4, 0})));
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

TEST_CASE("OrientedLine interiorContains another oriented line") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine axis({0, 0}, {4, 0});

    // interiorContains delegates to contains and ignores orientation: it holds
    // exactly when the underlying line matches.
    CHECK(axis.interiorContains(axis));
    CHECK(axis.interiorContains(OrientedLine({-3, 0}, {7, 0})));       // same line, same direction
    CHECK(axis.interiorContains(OrientedLine({7, 0}, {-3, 0})));       // same line, opposite direction
    CHECK_FALSE(axis.interiorContains(OrientedLine({0, 1}, {4, 1})));  // parallel but distinct
    CHECK_FALSE(axis.interiorContains(OrientedLine({0, 0}, {0, 4})));  // crossing but distinct
}
