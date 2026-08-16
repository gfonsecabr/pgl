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

    // Equality is mutual containment, and it stays so here: the region has to
    // reach the set one corner at a time, no component holding the whole of it.
    CHECK(withDiamondHole.contains(fourCorners));
    CHECK(fourCorners.contains(withDiamondHole));
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

TEST_CASE("samePointSet compares unbounded shapes and their regions") {
    const pgl::Line<Point> line({0, 0}, {2, 1});
    const pgl::OrientedLine<Point> reversedLine({4, 2}, {-2, -1});
    CHECK(line.samePointSet(reversedLine));

    const pgl::Ray<Point> ray({0, 0}, {2, 1});
    CHECK(ray.samePointSet(pgl::Ray<Point>({0, 0}, {4, 2})));
    CHECK_FALSE(ray.samePointSet(pgl::Ray<Point>({0, 0}, {-2, -1})));
    CHECK_FALSE(ray.samePointSet(line));

    const pgl::Halfplane<Point> halfplane({0, 0}, {2, 1});
    CHECK(halfplane.samePointSet(pgl::Halfplane<Point>({-2, -1}, {4, 2})));
    CHECK_FALSE(halfplane.samePointSet(pgl::Halfplane<Point>({4, 2}, {-2, -1})));
    CHECK_FALSE(halfplane.samePointSet(halfplane.opposite()));
    CHECK_FALSE(halfplane.samePointSet(line));

    // A region of half-planes reaches every one of those states.
    const pgl::HalfplaneIntersection<Point> region(halfplane);
    CHECK(halfplane.samePointSet(region));
    CHECK_FALSE(halfplane.opposite().samePointSet(region));
    CHECK_FALSE(pgl::HalfplaneIntersection<Point>{}.samePointSet(region));

    pgl::HalfplaneIntersection<Point> collapsed(halfplane);
    collapsed.insert(halfplane.opposite());
    REQUIRE(collapsed.isLine());
    CHECK(line.samePointSet(collapsed));
    CHECK(reversedLine.samePointSet(collapsed));

    pgl::HalfplaneIntersection<Point> clipped(collapsed);
    clipped.insert(pgl::Halfplane<Point>({0, 0}, {1, -2}));
    REQUIRE(clipped.isRay());
    CHECK(ray.samePointSet(clipped));
    CHECK_FALSE(pgl::Ray<Point>({0, 0}, {-2, -1}).samePointSet(clipped));
}

TEST_CASE("samePointSet compares a half-plane intersection with polygonal rings") {
    const pgl::Rectangle<Point> box({0, 0}, {4, 4});
    const pgl::HalfplaneIntersection<Point> region(box);
    CHECK(region.samePointSet(box));
    CHECK(box.samePointSet(region));
    CHECK(region.samePointSet(PolygonShape({0, 0, 2, 0, 4, 0, 4, 4, 0, 4})));
    CHECK(region.samePointSet(
        pgl::Convex<Point>({Point(0, 0), Point(4, 0), Point(4, 4), Point(0, 4)})));
    CHECK_FALSE(region.samePointSet(pgl::Rectangle<Point>({0, 0}, {4, 5})));
    CHECK_FALSE(region.samePointSet(pgl::Triangle<Point>({0, 0}, {4, 0}, {0, 4})));

    // A reflex vertex breaks the counterclockwise run of edge directions the
    // stored constraints have, so the two never line up.
    CHECK_FALSE(region.samePointSet(PolygonShape({0, 0, 4, 0, 4, 4, 2, 2, 0, 4})));

    const pgl::Triangle<Point> triangle({0, 0}, {4, 0}, {0, 4});
    const pgl::HalfplaneIntersection<Point> wedge(triangle);
    CHECK(wedge.samePointSet(triangle));
    CHECK(wedge.samePointSet(PolygonShape({0, 0, 4, 0, 0, 4})));
    CHECK_FALSE(wedge.samePointSet(region));
}

TEST_CASE("samePointSet separates a disk from every straight-edged shape") {
    const pgl::Disk<Point> disk({0, 0}, {4, 0}, {2, 2});
    CHECK(disk.samePointSet(pgl::Disk<Point>({2, 2}, {0, 0}, {4, 0})));
    CHECK_FALSE(disk.samePointSet(pgl::Disk<Point>(Point(2, 1), 5)));
    CHECK_FALSE(disk.samePointSet(
        pgl::Convex<Point>({Point(0, 0), Point(4, 0), Point(2, 2)})));
    CHECK_FALSE(disk.samePointSet(disk.bbox()));

    // A disk of zero radius is a point, and that is the one way it agrees.
    const pgl::Disk<Point> dot({3, 7}, {3, 7}, {3, 7});
    CHECK(dot.samePointSet(Point(3, 7)));
    CHECK(dot.samePointSet(pgl::Rectangle<Point>({3, 7}, {3, 7})));
    CHECK(dot.samePointSet(PolygonShape({3, 7, 3, 7, 3, 7})));
    CHECK_FALSE(dot.samePointSet(Point(3, 8)));
}

TEST_CASE("samePointSet compares chains and polylines by their point sets") {
    const pgl::MonotoneChain<Point> chain({Point(0, 0), Point(2, 2), Point(4, 0)});
    const pgl::Polyline<Point> polyline({Point(0, 0), Point(2, 2), Point(4, 0)});
    CHECK(chain.samePointSet(polyline));

    // A polyline has no canonical form: it may run either way and may retrace.
    CHECK(polyline.samePointSet(
        pgl::Polyline<Point>({Point(4, 0), Point(2, 2), Point(0, 0)})));
    CHECK(polyline.samePointSet(
        pgl::Polyline<Point>({Point(0, 0), Point(2, 2), Point(4, 0), Point(2, 2)})));
    CHECK_FALSE(polyline.samePointSet(
        pgl::Polyline<Point>({Point(0, 0), Point(2, 2), Point(4, 0), Point(5, 3)})));

    const pgl::MonotoneChain<Point> subdivided(
        {Point(0, 0), Point(1, 1), Point(2, 2), Point(3, 1), Point(4, 0)});
    CHECK(subdivided.samePointSet(chain));
    CHECK(subdivided.samePointSet(polyline));
    CHECK_FALSE(subdivided.samePointSet(
        pgl::MonotoneChain<Point>({Point(0, 0), Point(2, 2), Point(4, 1)})));
}

TEST_CASE("samePointSet never asks a shape for its area") {
    // Twice the area of this square is 2^33, which wraps to zero in the int
    // coordinate type. The point sets are decided by the vertices, which do
    // fit, and are unaffected -- as is isDegenerate, which reads the vertices
    // rather than the wrapped area.
    const PolygonShape square({0, 0, 65536, 0, 65536, 65536, 0, 65536});
    REQUIRE(square.twiceArea() == 0);      // the area wraps
    REQUIRE_FALSE(square.isDegenerate());  // the predicate does not follow it
    const PolygonShape subdivided(
        {0, 0, 65536, 0, 65536, 65536, 32768, 65536, 0, 65536});
    const PolygonShape shorter({0, 0, 65536, 0, 65536, 65535, 0, 65536});

    CHECK(square.samePointSet(subdivided));
    CHECK(square.samePointSet(pgl::Rectangle<Point>({0, 0}, {65536, 65536})));
    CHECK_FALSE(square.samePointSet(shorter));
    CHECK_FALSE(square.isPoint());
    CHECK_FALSE(square.isSegment());
}

TEST_CASE("samePointSet matches sets that cut the same points differently") {
    const PolygonShape square({0, 0, 2, 0, 2, 2, 0, 2});
    const PolygonShape diamond({0, 1, 1, 2, 2, 1, 1, 0});
    const pgl::PolygonSet<Point> pinched(Region(square, std::vector{diamond}));
    const pgl::PolygonSet<Point> fourCorners(std::vector{
        Region(PolygonShape({0, 0, 1, 0, 0, 1})),
        Region(PolygonShape({0, 1, 1, 2, 0, 2})),
        Region(PolygonShape({1, 0, 2, 0, 2, 1})),
        Region(PolygonShape({1, 2, 2, 1, 2, 2}))});

    REQUIRE(pinched.componentCount() == 1);
    REQUIRE(fourCorners.componentCount() == 4);
    CHECK(pinched.samePointSet(fourCorners));
    CHECK(fourCorners.samePointSet(pinched));

    const pgl::PolygonSet<Point> threeCorners(std::vector{
        Region(PolygonShape({0, 0, 1, 0, 0, 1})),
        Region(PolygonShape({0, 1, 1, 2, 0, 2})),
        Region(PolygonShape({1, 0, 2, 0, 2, 1}))});
    CHECK_FALSE(pinched.samePointSet(threeCorners));
    CHECK_FALSE(fourCorners.samePointSet(threeCorners));
}

TEST_CASE("samePointSet rejects shape kinds that cannot cover the same set") {
    const pgl::Rectangle<Point> box({0, 0}, {4, 4});
    const pgl::Triangle<Point> triangle({0, 0}, {4, 0}, {0, 4});
    // Four corners against three.
    CHECK_FALSE(box.samePointSet(triangle));

    const PolygonShape square({0, 0, 4, 0, 4, 4, 0, 4});
    const PolygonShape hole({1, 1, 3, 1, 3, 3, 1, 3});
    const Region holed(square, std::vector{hole});
    CHECK_FALSE(box.samePointSet(holed));
    CHECK_FALSE(square.samePointSet(holed));
    CHECK(square.samePointSet(Region(square)));

    // A shape with area against one without.
    const pgl::Segment<Point> segment({0, 0}, {4, 0});
    CHECK_FALSE(box.samePointSet(segment));
    CHECK_FALSE(triangle.samePointSet(segment));
    CHECK(pgl::Rectangle<Point>({0, 0}, {4, 0}).samePointSet(segment));
    CHECK(pgl::Triangle<Point>({0, 0}, {2, 0}, {4, 0}).samePointSet(segment));
}

TEST_CASE("samePointSet matches a large-coordinate region with its singleton set") {
    using WidePoint = pgl::Point<int64_t>;
    using WidePolygon = pgl::Polygon<WidePoint>;
    using WideRegion = pgl::PolygonWithHoles<WidePoint>;

    const WidePolygon outer({
        0, 824682617,
        34229828, 692767501,
        31140325, 337108145,
        387653274, 346657649,
        546911990, 280411710,
        402851929, 356082192,
        357028779, 512847829,
        282828455, 734764933,
        371533788, 583311160,
        413253598, 529870160,
        674443975, 130963417,
        676696868, 74240175,
        634520437, 0,
        1073741824, 421770903,
        1008785991, 577648623,
        909074044, 469175870,
        874792982, 745836095,
        687762209, 780157145,
        351608009, 805933556,
        115213807, 875794801});
    const WidePolygon hole({
        401212068, 587116547,
        538187922, 386368944,
        702126055, 119441798,
        444516562, 740014126});
    const WideRegion region(outer, std::vector{hole});
    const pgl::PolygonWithHoles<pgl::EPoint> exactRegion(region);
    const pgl::PolygonSet<pgl::EPoint> singleton(exactRegion);

    CHECK(singleton.samePointSet(region));
    CHECK(region.samePointSet(singleton));

    const pgl::Shape<pgl::EPoint> wrappedSingleton(singleton);
    const pgl::Shape<WidePoint> wrappedRegion(region);
    CHECK(wrappedSingleton.samePointSet(wrappedRegion));
    CHECK(wrappedRegion.samePointSet(wrappedSingleton));
}
