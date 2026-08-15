#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <tuple>
#include <vector>

namespace {

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

template <class First, class Second>
void checkSymmetric(const First& first, const Second& second) {
    CHECK(first.samePointSet(second) == second.samePointSet(first));
}

template <class First, class Tuple>
void checkAgainstAll(const First& first, const Tuple& shapes) {
    std::apply(
        [&first](const auto&... second) { (checkSymmetric(first, second), ...); },
        shapes);
}

template <class Number>
auto sampleShapes() {
    using SamplePoint = pgl::Point<Number>;
    using SamplePolygon = pgl::Polygon<SamplePoint>;
    using SampleRegion = pgl::PolygonWithHoles<SamplePoint>;
    const SamplePolygon square({Number(0), Number(0), Number(2), Number(0),
                                Number(2), Number(2), Number(0), Number(2)});
    const SampleRegion region(square);
    return std::tuple{
        pgl::EmptyShape<SamplePoint>{},
        SamplePoint(0, 0),
        pgl::Segment<SamplePoint>({0, 0}, {1, 0}),
        pgl::OrientedSegment<SamplePoint>({1, 0}, {0, 0}),
        pgl::Line<SamplePoint>({0, 0}, {1, 0}),
        pgl::OrientedLine<SamplePoint>({1, 0}, {0, 0}),
        pgl::Ray<SamplePoint>({0, 0}, {1, 0}),
        pgl::Halfplane<SamplePoint>({0, 0}, {1, 0}),
        pgl::Rectangle<SamplePoint>({0, 0}, {2, 2}),
        pgl::Triangle<SamplePoint>({0, 0}, {2, 0}, {0, 2}),
        pgl::Disk<SamplePoint>(Number(0), Number(0), Number(1)),
        pgl::Convex<SamplePoint>(
            {SamplePoint(0, 0), SamplePoint(2, 0), SamplePoint(0, 2)}),
        pgl::MonotoneChain<SamplePoint>({SamplePoint(0, 0), SamplePoint(1, 0)}),
        pgl::Polyline<SamplePoint>({SamplePoint(0, 0), SamplePoint(1, 0)}),
        square,
        pgl::HalfplaneIntersection<SamplePoint>(
            pgl::Rectangle<SamplePoint>({0, 0}, {2, 2})),
        region,
        pgl::PolygonSet<SamplePoint>(region),
        pgl::Shape<SamplePoint>(square)};
}

}  // namespace

TEST_CASE("samePointSet is defined and symmetric for every pair of shape kinds") {
    const auto shapes = sampleShapes<int>();
    const auto otherNumberShapes = sampleShapes<long long>();

    std::apply(
        [&shapes](const auto&... first) { (checkAgainstAll(first, shapes), ...); },
        shapes);
    std::apply(
        [&otherNumberShapes](const auto&... first) {
            (checkAgainstAll(first, otherNumberShapes), ...);
        },
        shapes);
}

TEST_CASE("samePointSet compares different shape and number types") {
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;

    const Point point(2, 3);
    const pgl::Segment<RationalPoint> collapsed(
        RationalPoint(Rational(2), Rational(3)),
        RationalPoint(Rational(2), Rational(3)));
    CHECK(point.samePointSet(collapsed));
    CHECK(collapsed.samePointSet(point));

    const pgl::Line<RationalPoint> line(
        RationalPoint(Rational(0), Rational(0)),
        RationalPoint(Rational(1), Rational(0)));
    CHECK_FALSE(point.samePointSet(line));
    CHECK_FALSE(line.samePointSet(point));

    const pgl::Line<RationalPoint> undefinedLine(
        RationalPoint(Rational(2), Rational(3)),
        RationalPoint(Rational(2), Rational(3)));
    CHECK_FALSE(point.samePointSet(undefinedLine));
    CHECK_FALSE(undefinedLine.samePointSet(point));

    const pgl::Shape<Point> wrappedInt(point);
    const pgl::Shape<RationalPoint> wrappedRational(collapsed);
    CHECK(wrappedInt.samePointSet(wrappedRational));
    CHECK(wrappedRational.samePointSet(wrappedInt));
}

TEST_CASE("samePointSet ignores orientation and redundant linear vertices") {
    const pgl::Segment<Point> segment({0, 0}, {4, 0});
    const pgl::OrientedSegment<Point> reversed({4, 0}, {0, 0});
    CHECK(segment.samePointSet(reversed));

    const pgl::Line<Point> line({0, 0}, {1, 0});
    const pgl::OrientedLine<Point> reversedLine({3, 0}, {-2, 0});
    CHECK(line.samePointSet(reversedLine));

    const pgl::MonotoneChain<Point> chain(
        {Point(0, 0), Point(1, 0), Point(3, 0), Point(4, 0)});
    const pgl::Polyline<Point> polyline(
        {Point(0, 0), Point(2, 0), Point(4, 0)});
    CHECK(segment.samePointSet(chain));
    CHECK(chain.samePointSet(polyline));

    const pgl::Triangle<Point> first({0, 0}, {1, 0}, {4, 0});
    const pgl::Triangle<Point> second({0, 0}, {3, 0}, {4, 0});
    CHECK(first != second);
    CHECK(first.samePointSet(second));
}

TEST_CASE("samePointSet compares polygonal regions geometrically") {
    const PolygonShape square({0, 0, 4, 0, 4, 4, 0, 4});
    const PolygonShape subdividedSquare({0, 0, 2, 0, 4, 0, 4, 4, 0, 4});
    REQUIRE(square != subdividedSquare);
    CHECK(square.samePointSet(subdividedSquare));
    CHECK(square.samePointSet(pgl::Rectangle<Point>({0, 0}, {4, 4})));
    CHECK(square.samePointSet(
        pgl::Convex<Point>({Point(0, 0), Point(2, 0), Point(4, 0),
                            Point(4, 4), Point(0, 4)})));

    const PolygonShape hole({1, 1, 3, 1, 3, 3, 1, 3});
    const PolygonShape subdividedHole({1, 1, 2, 1, 3, 1, 3, 3, 1, 3});
    const Region first(square, std::vector{hole});
    const Region second(subdividedSquare, std::vector{subdividedHole});
    REQUIRE(first != second);
    CHECK(first.samePointSet(second));

    const pgl::PolygonSet<Point> firstSet(first);
    const pgl::PolygonSet<Point> secondSet(second);
    REQUIRE(firstSet != secondSet);
    CHECK(firstSet.samePointSet(secondSet));

    // Subdivision changes Polygon's size-first ordering, so equivalent holes
    // and components need geometric matching rather than positional matching.
    const PolygonShape otherHole({6, 1, 8, 1, 8, 3, 7, 3, 6, 3});
    const PolygonShape plainOtherHole({6, 1, 8, 1, 8, 3, 6, 3});
    const PolygonShape moreSubdividedHole(
        {1, 1, 2, 1, 3, 1, 3, 2, 3, 3, 1, 3});
    const Region reorderedFirst(
        PolygonShape({0, 0, 10, 0, 10, 5, 0, 5}),
        std::vector{hole, otherHole});
    const Region reorderedSecond(
        PolygonShape({0, 0, 5, 0, 10, 0, 10, 5, 0, 5}),
        std::vector{moreSubdividedHole, plainOtherHole});
    CHECK(reorderedFirst.samePointSet(reorderedSecond));

    const Region left(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2}));
    const Region subdividedLeft(
        PolygonShape({0, 0, 1, 0, 2, 0, 2, 1, 2, 2, 0, 2}));
    const Region subdividedRight(
        PolygonShape({5, 0, 7, 0, 7, 1, 7, 2, 5, 2}));
    const Region right(PolygonShape({5, 0, 7, 0, 7, 2, 5, 2}));
    const pgl::PolygonSet<Point> reorderedFirstSet(
        std::vector{left, subdividedRight});
    const pgl::PolygonSet<Point> reorderedSecondSet(
        std::vector{subdividedLeft, right});
    CHECK(reorderedFirstSet.samePointSet(reorderedSecondSet));
}

TEST_CASE("samePointSet handles empty and lower-dimensional intersections") {
    const pgl::EmptyShape<Point> empty;
    CHECK(empty.samePointSet(PolygonShape{}));
    CHECK(empty.samePointSet(pgl::Rectangle<Point>{}));
    CHECK(empty.samePointSet(pgl::PolygonSet<Point>{}));
    CHECK_FALSE(empty.samePointSet(Point(0, 0)));

    const pgl::Segment<Point> segment({0, 0}, {4, 0});
    const pgl::HalfplaneIntersection<Point> fromSegment(segment);
    const pgl::HalfplaneIntersection<Point> fromRectangle(
        pgl::Rectangle<Point>({0, 0}, {4, 0}));
    CHECK(fromSegment.samePointSet(segment));
    CHECK(fromSegment.samePointSet(fromRectangle));
}

TEST_CASE("samePointSet matches a diamond hole with its four corner components") {
    const PolygonShape square({0, 0, 2, 0, 2, 2, 0, 2});
    const PolygonShape diamond({0, 1, 1, 2, 2, 1, 1, 0});
    const Region withDiamondHole(square, std::vector{diamond});

    const pgl::PolygonSet<Point> fourCorners(std::vector{
        Region(PolygonShape({0, 0, 1, 0, 0, 1})),
        Region(PolygonShape({0, 1, 1, 2, 0, 2})),
        Region(PolygonShape({1, 0, 2, 0, 2, 1})),
        Region(PolygonShape({1, 2, 2, 1, 2, 2}))});

    CHECK(withDiamondHole.samePointSet(fourCorners));
    CHECK(fourCorners.samePointSet(withDiamondHole));
}

TEST_CASE("samePointSet preserves a seam where a hole touches its outer boundary") {
    const PolygonShape square({0, 0, 3, 0, 3, 3, 0, 3});
    const PolygonShape touchingHole({1, 1, 3, 1, 3, 2, 1, 2});
    const Region withRetainedSeam(square, std::vector{touchingHole});

    // This has the same area, but omits the vertical seam (3,1)--(3,2) that
    // belongs to both boundaries and is therefore retained by the holed region.
    const Region withoutSeam(
        PolygonShape({0, 0, 3, 0, 3, 1, 1, 1, 1, 2, 3, 2, 3, 3, 0, 3}));
    const pgl::PolygonSet<Point> regularizedNotch(withoutSeam);

    CHECK_FALSE(withRetainedSeam.samePointSet(regularizedNotch));
    CHECK_FALSE(regularizedNotch.samePointSet(withRetainedSeam));
}
