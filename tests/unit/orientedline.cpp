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

    CHECK(line.halfplaneAbove().contains(Point(0, 1)));
    CHECK_FALSE(line.halfplaneAbove().contains(Point(1, 0)));

    CHECK(line.halfplaneBelow().contains(Point(1, 0)));
    CHECK_FALSE(line.halfplaneBelow().contains(Point(0, 1)));

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
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diagonal({0, 0}, {4, 4});
    const OrientedLine same_orientation({1, 1}, {3, 3});
    const OrientedLine opposite_orientation({3, 3}, {1, 1});
    const OrientedLine crossing({0, 4}, {4, 0});
    const OrientedLine parallel({0, 1}, {4, 5});
    const Line supporting_line({2, 2}, {6, 6});
    const Segment subsegment({1, 1}, {3, 3});

    CHECK(diagonal.contains(supporting_line));
    CHECK(diagonal.contains(same_orientation));
    CHECK(diagonal.contains(opposite_orientation));
    CHECK(diagonal.contains(subsegment));
    CHECK(diagonal.collinear(supporting_line));
    CHECK(diagonal.collinear(opposite_orientation));

    CHECK(diagonal.orientation(Point(0, 1)) == std::partial_ordering::greater);
    CHECK(diagonal.orientation(Point(1, 0)) == std::partial_ordering::less);
    CHECK(diagonal.orientation(Point(2, 2)) == std::partial_ordering::equivalent);

    CHECK(diagonal.parallel(parallel));
    CHECK(diagonal.parallel(Line({0, 2}, {4, 6})));
    CHECK_FALSE(diagonal.parallel(crossing));

    CHECK(diagonal.intersects(same_orientation));
    CHECK(diagonal.intersects(opposite_orientation));
    CHECK(diagonal.intersects(crossing));
    CHECK_FALSE(diagonal.intersects(parallel));
    CHECK(diagonal.interiorsIntersect(crossing));
    CHECK(diagonal.crosses(crossing));
    CHECK_FALSE(diagonal.crosses(opposite_orientation));
}

TEST_CASE("OrientedLine intersection and distances support exact rational results") {
    using Rational = pgl::Rational<int64_t>;
    using IntLine = pgl::OrientedLine<pgl::Point<int>>;
    using RationalPoint = pgl::Point<Rational>;

    const IntLine rising({0, 0}, {4, 4});
    const IntLine falling({6, 0}, {0, 3});
    const IntLine parallel({0, 1}, {4, 5});

    const auto intersection = rising.intersection<Rational>(falling);
    REQUIRE(intersection);
    REQUIRE(std::holds_alternative<RationalPoint>(*intersection));
    CHECK(std::get<RationalPoint>(*intersection) == RationalPoint(Rational(2), Rational(2)));

    const auto same_line_intersection = rising.intersection<Rational>(IntLine({1, 1}, {3, 3}));
    REQUIRE(same_line_intersection);
    REQUIRE(std::holds_alternative<pgl::Line<pgl::Point<Rational>>>(*same_line_intersection));

    // Euclidean squared distance is generally fractional, so request a
    // floating-point ResultNumber; the integer default would truncate.
    CHECK(rising.squaredDistance<double>(pgl::Point<int>(2, 0)) == doctest::Approx(2.0));
    CHECK(rising.squaredDistance<double>(parallel) == doctest::Approx(0.5));
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

TEST_CASE("OrientedLine self-pair: separates another OrientedLine") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine vertical({2, 2}, {2, -2});
    CHECK(vertical.separates(OrientedLine({0, 0}, {4, 0})));
}

TEST_CASE("OrientedLine asOrientedSegmentFor returns a segment that meets the rectangle the same way") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({0, 0}, {10, 10});

    // The returned segment must relate to the rectangle exactly like the line, and
    // (when they meet) keep both endpoints strictly outside the closed rectangle.
    const auto closedContains = [&](const Point& p) {
        return p.x() >= 0 && p.x() <= 10 && p.y() >= 0 && p.y() <= 10;
    };
    const auto sameWay = [&](const OrientedLine& line) {
        const OrientedSegment seg = line.asOrientedSegmentFor(rect);
        CHECK(seg.intersects(rect) == line.intersects(rect));
        CHECK(seg.crosses(rect) == line.crosses(rect));
        CHECK(seg.separates(rect) == line.separates(rect));
        if (line.intersects(rect)) {
            CHECK_FALSE(closedContains(seg.source()));
            CHECK_FALSE(closedContains(seg.target()));
        }
        return seg;
    };

    // Horizontal: extended one past each vertical edge, orientation preserved.
    CHECK(sameWay(OrientedLine({-2, 1}, {6, 1})) == OrientedSegment({-1, 1}, {11, 1}));
    CHECK(sameWay(OrientedLine({6, 1}, {-2, 1})) == OrientedSegment({11, 1}, {-1, 1}));

    // Vertical: extended one past each horizontal edge.
    CHECK(sameWay(OrientedLine({1, -2}, {1, 6})) == OrientedSegment({1, -1}, {1, 11}));
    CHECK(sameWay(OrientedLine({1, 6}, {1, -2})) == OrientedSegment({1, 11}, {1, -1}));

    // Diagonal chord through the interior (both orientations).
    CHECK(sameWay(OrientedLine({0, 0}, {10, 10})) == OrientedSegment({-10, -10}, {20, 20}));
    CHECK(sameWay(OrientedLine({0, 10}, {10, 0})) == OrientedSegment({-10, 20}, {20, -10}));
    sameWay(OrientedLine({1, 2}, {3, 4}));   // shallow-ish, off-origin
    sameWay(OrientedLine({3, 4}, {1, 2}));   // reversed
    sameWay(OrientedLine({4, -5}, {6, 30}));  // steep

    // A line that enters at a corner and continues into the interior still crosses.
    const OrientedLine cornerIn({0, 0}, {3, 1});
    CHECK(cornerIn.crosses(rect));
    CHECK(sameWay(cornerIn).crosses(rect));

    // Single-vertex graze: touches only the corner (0,0), so it neither crosses nor
    // separates; the segment must do the same while still containing the corner.
    const OrientedLine graze({-2, 2}, {2, -2});
    CHECK(graze.intersects(rect));
    CHECK_FALSE(graze.crosses(rect));
    CHECK_FALSE(graze.separates(rect));
    const OrientedSegment grazeSeg = sameWay(graze);
    CHECK(grazeSeg.contains(Point(0, 0)));

    // Disjoint line: the original defining points are returned unchanged.
    const OrientedLine disjoint({20, 0}, {24, 4});
    REQUIRE_FALSE(rect.intersects(disjoint));
    CHECK(disjoint.asOrientedSegmentFor(rect) == OrientedSegment({20, 0}, {24, 4}));
}

TEST_CASE("OrientedLine asOrientedSegmentFor supports exact rational coordinates") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({Rational(0), Rational(0)}, {Rational(10), Rational(10)});
    const OrientedLine line({Rational(1), Rational(2)}, {Rational(3), Rational(5)});
    const auto seg = line.asOrientedSegmentFor(rect);

    CHECK(seg.crosses(rect) == line.crosses(rect));
    CHECK(seg.separates(rect) == line.separates(rect));
    CHECK(line.crosses(rect));
    CHECK(seg.crosses(rect));
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
