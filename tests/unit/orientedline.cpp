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

    CHECK(diagonal.yAtX<int>(2) == 2);
    CHECK(diagonal.xAtY<int>(3) == 3);
    CHECK(diagonal.yAtX<Rational>(1).value() == Rational(1));
    CHECK(diagonal.xAtY<Rational>(1).value() == Rational(1));

    CHECK(vertical.yAtX<int>(2) == -1);
    CHECK_FALSE(vertical.yAtX<int>(1).has_value());
    CHECK(vertical.xAtY<int>(0) == 2);

    CHECK(horizontal.xAtY<int>(1) == -2);
    CHECK_FALSE(horizontal.xAtY<int>(0).has_value());
    CHECK(horizontal.yAtX<int>(3) == 1);
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

// integralLine only exists for rational coordinates; detecting that needs a
// dependent context, so the requires-expression lives in a concept.
template <class Line>
concept HasIntegralLine = requires(const Line& line) { line.integralLine(); };

TEST_CASE("OrientedLine integralLine returns an equal line over integer coordinates") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using IntegerLine = pgl::OrientedLine<pgl::Point<int64_t>>;

    // Line equality is geometric, so the defining points are compared directly
    // wherever the exact representative is the point of the check.
    const auto definedBy = [](const auto& line, const pgl::Point<int64_t>& source,
                              const pgl::Point<int64_t>& target) {
        return line.source() == source && line.target() == target;
    };

    // y == x, given by two half-integer points: the lattice point closest to the
    // origin is the origin itself, and the next one along the orientation is (1,1).
    const OrientedLine diagonal({Rational(1, 2), Rational(1, 2)}, {Rational(3, 2), Rational(3, 2)});
    const auto integral = diagonal.integralLine();
    REQUIRE(integral.has_value());
    CHECK(std::is_same_v<std::remove_cvref_t<decltype(*integral)>, IntegerLine>);
    CHECK(definedBy(*integral, {0, 0}, {1, 1}));
    CHECK(OrientedLine(*integral) == diagonal);

    // y == x + 1, from points with a different denominator. Here (0,1) and
    // (-1,0) are equally close to the origin, and the tie goes to the
    // lexicographically smaller one.
    const OrientedLine shifted({Rational(1, 3), Rational(4, 3)}, {Rational(4, 3), Rational(7, 3)});
    REQUIRE(shifted.integralLine().has_value());
    CHECK(definedBy(*shifted.integralLine(), {-1, 0}, {0, 1}));
    CHECK(OrientedLine(*shifted.integralLine()) == shifted);

    // The representative depends only on the line, not on the points defining it.
    const OrientedLine same({Rational(-11, 2), Rational(-11, 2)}, {Rational(101, 2), Rational(101, 2)});
    CHECK(definedBy(*same.integralLine(), {0, 0}, {1, 1}));

    // Orientation is part of the answer: the opposite line reports the opposite,
    // from the same source, since that source depends only on the point set.
    const auto opposite = diagonal.opposite().integralLine();
    REQUIRE(opposite.has_value());
    CHECK(OrientedLine(*opposite) == diagonal.opposite());
    CHECK(definedBy(*opposite, {0, 0}, {-1, -1}));
    CHECK_FALSE(OrientedLine(*opposite) == diagonal);
    CHECK(shifted.opposite().integralLine()->source() == shifted.integralLine()->source());

    // Axis-parallel lines keep their direction too.
    const OrientedLine leftwards({Rational(7, 2), Rational(3)}, {Rational(1, 2), Rational(3)});
    REQUIRE(leftwards.integralLine().has_value());
    CHECK(definedBy(*leftwards.integralLine(), {0, 3}, {-1, 3}));
    const OrientedLine downwards({Rational(5), Rational(9, 4)}, {Rational(5), Rational(1, 4)});
    REQUIRE(downwards.integralLine().has_value());
    CHECK(definedBy(*downwards.integralLine(), {5, 0}, {5, -1}));

    // A line already over integers comes back as it stands, up to the choice of
    // defining points.
    const OrientedLine whole({Rational(3), Rational(4)}, {Rational(-1), Rational(9)});
    REQUIRE(whole.integralLine().has_value());
    CHECK(OrientedLine(*whole.integralLine()) == whole);
    CHECK(definedBy(*whole.integralLine(), {3, 4}, {-1, 9}));
}

TEST_CASE("OrientedLine integralLine reports the lines that have no integer representation") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // A line meets the integer grid only when, written as a*x + b*y == c with
    // coprime integers a and b, c is an integer as well. These three miss it.
    const OrientedLine vertical({Rational(1, 2), Rational(0)}, {Rational(1, 2), Rational(1)});
    CHECK_FALSE(vertical.integralLine().has_value());          // x == 1/2
    const OrientedLine antidiagonal({Rational(1, 2), Rational(0)}, {Rational(0), Rational(1, 2)});
    CHECK_FALSE(antidiagonal.integralLine().has_value());      // x + y == 1/2
    const OrientedLine sixth({Rational(1, 2), Rational(1, 3)}, {Rational(5, 2), Rational(7, 3)});
    CHECK_FALSE(sixth.integralLine().has_value());             // y == x - 1/6

    // A degenerate line is undefined: it has no direction, so no line to return.
    const OrientedLine degenerate({Rational(1, 2), Rational(1, 2)}, {Rational(1, 2), Rational(1, 2)});
    CHECK_FALSE(degenerate.integralLine().has_value());

    // The method only exists where the coordinates are fractions to begin with.
    CHECK_FALSE(HasIntegralLine<pgl::OrientedLine<pgl::Point<int>>>);
    CHECK(HasIntegralLine<OrientedLine>);
}

TEST_CASE("OrientedLine integralLine reports coordinates that do not fit the result type") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // x == 1000000, whose closest lattice point needs more than an int8_t.
    const OrientedLine distant({Rational(1000000), Rational(1, 2)}, {Rational(1000000), Rational(3, 2)});
    CHECK_FALSE(distant.integralLine<int8_t>().has_value());
    REQUIRE(distant.integralLine<int64_t>().has_value());
    CHECK(OrientedLine(*distant.integralLine<int64_t>()) == distant);

    // Exact coordinates that overflow every fixed-width type still convert when
    // an arbitrary-precision result is requested.
    using Exact = pgl::Point<pgl::ERational>;
    const pgl::BigInt beyond = pgl::BigInt(int64_t(1) << 62) * pgl::BigInt(64);
    const pgl::OrientedLine<Exact> wide({pgl::ERational(beyond), pgl::ERational(0)},
                                        {pgl::ERational(beyond), pgl::ERational(1)});
    CHECK_FALSE(wide.integralLine<int64_t>().has_value());
    const auto exact = wide.integralLine<pgl::BigInt>();
    REQUIRE(exact.has_value());
    CHECK(exact->source() == pgl::Point<pgl::BigInt>(beyond, pgl::BigInt(0)));
    CHECK(pgl::OrientedLine<Exact>(*exact) == wide);
}

TEST_CASE("OrientedLine integralLine agrees across its two arithmetic paths") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // The same line, y == x + 1 with direction (1,1), reached through
    // coordinates on either side of the width that decides whether the
    // computation runs in a pgl::int128 or in BigInt. The answer cannot depend
    // on which arithmetic served it.
    for (const int64_t denominator : {int64_t(1), int64_t(4095), int64_t(4096),
                                      int64_t(4097), int64_t(1000003)}) {
        const Point start(Rational(1, denominator), Rational(1, denominator) + Rational(1));
        const OrientedLine line(start, Point(start.x() + Rational(1), start.y() + Rational(1)));
        const auto integral = line.integralLine();
        REQUIRE(integral.has_value());
        CHECK(integral->source() == pgl::Point<int64_t>(-1, 0));
        CHECK(integral->target() == pgl::Point<int64_t>(0, 1));
        CHECK(OrientedLine(*integral) == line);
    }

    // Wide coordinates that no fixed-width intermediate could carry: parts of
    // this size are exactly what the BigInt path exists for. This is the line
    // 7x == 3y, whose lattice points are the multiples of (3,7).
    const int64_t wide = int64_t(1) << 40;
    const Point origin(Rational(3, wide), Rational(7, wide));
    const OrientedLine steep(origin, Point(origin.x() + Rational(3), origin.y() + Rational(7)));
    const auto integral = steep.integralLine();
    REQUIRE(integral.has_value());
    CHECK(OrientedLine(*integral) == steep);
    CHECK(integral->source() == pgl::Point<int64_t>(0, 0));
    CHECK(integral->target() == pgl::Point<int64_t>(3, 7));
}

TEST_CASE("OrientedLine integralLine carries the line label") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational, std::string>;
    using OrientedLine = pgl::OrientedLine<Point, std::string>;

    const OrientedLine labeled({Rational(1, 2), Rational(1, 2)}, {Rational(3, 2), Rational(3, 2)}, "edge");
    const auto integral = labeled.integralLine<int>();
    REQUIRE(integral.has_value());
    CHECK(integral->label() == "edge");
    CHECK(integral->source() == pgl::Point<int, std::string>(0, 0));
    CHECK(integral->target() == pgl::Point<int, std::string>(1, 1));
}
