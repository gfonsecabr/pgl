#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "pgl.hpp"

// The Minkowski erosion `A ⊖ B = {x : x ⊕ B ⊆ A}`, the morphological dual of the
// sum: where `A ⊕ B` sweeps `B` over `A` and keeps everything it touches, the
// erosion keeps the placements of `B` that stay inside. This file covers the
// pairs whose receiver is convex, which is where the erosion is one clamp per
// constraint and exact on the lattice; `region_minkowskierosion.cpp` covers the
// receivers that need the boolean engine.

using Point = pgl::Point<int>;
using EPoint = pgl::EPoint;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using PolygonShape = pgl::Polygon<Point>;
using PolylineShape = pgl::Polyline<Point>;
using Chain = pgl::MonotoneChain<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
using Ray = pgl::Ray<Point>;
using Halfplane = pgl::Halfplane<Point>;
using Intersection = pgl::HalfplaneIntersection<Point>;
using Disk = pgl::Disk<Point>;
using AnyShape = pgl::Shape<Point>;

// -----------------------------------------------------------------------------
// Fixtures.

static PolygonShape lShape() {
    return PolygonShape({Point(0, 0), Point(4, 0), Point(4, 1), Point(1, 1), Point(1, 4),
                         Point(0, 4)});
}

static Region annulus() {
    return Region(PolygonShape({Point(0, 0), Point(8, 0), Point(8, 8), Point(0, 8)}),
                  std::vector<PolygonShape>{
                      PolygonShape({Point(2, 2), Point(6, 2), Point(6, 6), Point(2, 6)})});
}

// -----------------------------------------------------------------------------
// The oracle: the definition, read as a query over the lattice.
//
// `x ∈ A ⊖ B` exactly when every point of `B` translated to `x` lies in `A`, and
// for a *convex* receiver the operand's vertices settle it — a convex shape
// containing them contains their hull, which is everything a support function
// sees of the operand anyway.

template <class Result, class A, class B>
static void checkDefinition(const Result& erosion, const A& a, const B& b, int lo, int hi) {
    std::size_t agreed = 0;
    for (int x = lo; x <= hi; ++x) {
        for (int y = lo; y <= hi; ++y) {
            bool fits = true;
            for (const auto& vertex : b.vertices()) {
                if (!a.contains(Point(vertex.x() + x, vertex.y() + y))) {
                    fits = false;
                    break;
                }
            }
            const bool inside = erosion.contains(Point(x, y));
            if (inside != fits) {
                CHECK_MESSAGE(inside == fits, "erosion disagrees at (" << x << "," << y << ")");
                return;
            }
            ++agreed;
        }
    }
    CHECK(agreed > 0);
}

// -----------------------------------------------------------------------------
// The pairs the erosion is defined for are the pairs the sum is defined for.
//
// Both live in a template so that the requires-expressions are dependent: a
// non-dependent one is a hard error rather than `false` under g++.

template <class A, class B>
inline constexpr bool summable = requires(const A& a, const B& b) { a.minkowskiSum(b); };

template <class A, class B>
inline constexpr bool erodable = requires(const A& a, const B& b) { a.minkowskiErosion(b); };

using EveryShape =
    std::tuple<pgl::EmptyShape<Point>, Point, Segment, OrientedSegment, Line, OrientedLine, Ray,
               Halfplane, RectangleShape, Triangle, Disk, Convex, Chain, PolylineShape,
               PolygonShape, Intersection, Region, RegionSet, AnyShape>;

static constexpr std::size_t shapeCount = std::tuple_size_v<EveryShape>;

template <class A, std::size_t... Index>
static constexpr bool rowAgrees(std::index_sequence<Index...>) {
    return ((summable<A, std::tuple_element_t<Index, EveryShape>> ==
             erodable<A, std::tuple_element_t<Index, EveryShape>>) &&
            ...);
}

template <std::size_t... Index>
static constexpr bool everyPairAgrees(std::index_sequence<Index...>) {
    return (rowAgrees<std::tuple_element_t<Index, EveryShape>>(
                std::make_index_sequence<shapeCount>{}) &&
            ...);
}

static_assert(everyPairAgrees(std::make_index_sequence<shapeCount>{}),
              "minkowskiErosion must be defined for exactly the pairs minkowskiSum is");

// A few of those pairs spelled out, so that a change to the concept shows up as
// more than one collapsed fold.
static_assert(erodable<RectangleShape, RectangleShape>);
static_assert(erodable<Triangle, PolygonShape>);      // through the convex overload
static_assert(erodable<PolygonShape, Triangle>);      // through the region overload
static_assert(erodable<Halfplane, RegionSet>);
static_assert(erodable<Point, Disk>);
static_assert(!erodable<RectangleShape, Disk>);       // the sum has no answer either
static_assert(!erodable<Line, PolygonShape>);
static_assert(!erodable<Disk, RectangleShape>);

// -----------------------------------------------------------------------------

TEST_CASE_TEMPLATE("Eroding by a point is the translation by its negation", TPoint,
                   pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<int64_t>>) {
    using Number = typename TPoint::NumberType;
    const auto n = [](int value) { return static_cast<Number>(value); };
    const TPoint t(n(3), n(-5));

    const auto sameAsTranslation = [&t](const auto& shape) {
        const auto eroded = shape.minkowskiErosion(t);
        static_assert(std::is_same_v<std::remove_cvref_t<decltype(eroded)>,
                                     std::remove_cvref_t<decltype(shape)>>,
                      "eroding by a point must return the operand's own type");
        auto translated = shape;
        translated -= t;
        CHECK(eroded == translated);
        // The erosion by a point and the sum with its negation are the same set.
        CHECK(eroded == shape.minkowskiSum(-t));
    };

    sameAsTranslation(TPoint(n(1), n(2)));
    sameAsTranslation(pgl::Segment<TPoint>(TPoint(n(0), n(0)), TPoint(n(2), n(4))));
    sameAsTranslation(pgl::OrientedSegment<TPoint>(TPoint(n(2), n(4)), TPoint(n(0), n(0))));
    sameAsTranslation(pgl::Line<TPoint>(TPoint(n(0), n(0)), TPoint(n(2), n(4))));
    sameAsTranslation(pgl::OrientedLine<TPoint>(TPoint(n(0), n(0)), TPoint(n(2), n(4))));
    sameAsTranslation(pgl::Ray<TPoint>(TPoint(n(0), n(0)), TPoint(n(2), n(4))));
    sameAsTranslation(pgl::Halfplane<TPoint>(TPoint(n(0), n(0)), TPoint(n(2), n(4))));
    sameAsTranslation(pgl::Rectangle<TPoint>(TPoint(n(0), n(0)), TPoint(n(3), n(2))));
    sameAsTranslation(pgl::Triangle<TPoint>(TPoint(n(0), n(0)), TPoint(n(4), n(0)), TPoint(n(0), n(3))));
    sameAsTranslation(pgl::Convex<TPoint>(std::vector<TPoint>{
        TPoint(n(0), n(0)), TPoint(n(4), n(0)), TPoint(n(4), n(3)), TPoint(n(0), n(3))}));
    sameAsTranslation(pgl::MonotoneChain<TPoint>(
        std::vector<TPoint>{TPoint(n(0), n(0)), TPoint(n(2), n(3)), TPoint(n(4), n(1))}));
    sameAsTranslation(pgl::Polyline<TPoint>(
        std::vector<TPoint>{TPoint(n(0), n(0)), TPoint(n(2), n(3)), TPoint(n(4), n(1))}));
    sameAsTranslation(pgl::Polygon<TPoint>(std::vector<TPoint>{
        TPoint(n(0), n(0)), TPoint(n(4), n(0)), TPoint(n(4), n(3)), TPoint(n(0), n(3))}));
    sameAsTranslation(pgl::HalfplaneIntersection<TPoint>(
        pgl::Rectangle<TPoint>(TPoint(n(0), n(0)), TPoint(n(4), n(4)))));
    sameAsTranslation(pgl::PolygonWithHoles<TPoint>(pgl::Polygon<TPoint>(std::vector<TPoint>{
        TPoint(n(0), n(0)), TPoint(n(4), n(0)), TPoint(n(4), n(3)), TPoint(n(0), n(3))})));
    sameAsTranslation(pgl::PolygonSet<TPoint>(
        pgl::PolygonWithHoles<TPoint>(pgl::Polygon<TPoint>(std::vector<TPoint>{
            TPoint(n(0), n(0)), TPoint(n(4), n(0)), TPoint(n(4), n(3)), TPoint(n(0), n(3))}))));
    sameAsTranslation(pgl::Disk<TPoint>(TPoint(n(1), n(1)), n(3)));
}

TEST_CASE("Two rectangles erode to a rectangle") {
    const RectangleShape big(0, 0, 10, 10);

    // The minima and the maxima subtract, one interval at a time.
    const auto eroded = big.minkowskiErosion(RectangleShape(0, 0, 3, 2));
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(eroded)>, RectangleShape>,
                  "two rectangles must erode to a rectangle");
    CHECK(eroded == RectangleShape(0, 0, 7, 8));
    checkDefinition(eroded, big, RectangleShape(0, 0, 3, 2), -4, 14);

    // An operand placed away from the origin drags the answer with it: the two
    // corners subtract independently.
    CHECK(big.minkowskiErosion(RectangleShape(2, 3, 5, 7)) == RectangleShape(-2, -3, 5, 3));

    // A side of the operand longer than the receiver's leaves nothing, and the
    // empty rectangle is how a rectangle says so.
    CHECK(big.minkowskiErosion(RectangleShape(0, 0, 11, 2)).empty());
    CHECK(big.minkowskiErosion(RectangleShape(0, 0, 2, 11)).empty());
    CHECK(RectangleShape().minkowskiErosion(RectangleShape(0, 0, 1, 1)).empty());

    // Exactly as wide: the erosion is a segment, which a rectangle holds.
    const auto sliver = big.minkowskiErosion(RectangleShape(0, 0, 10, 4));
    CHECK(sliver == RectangleShape(0, 0, 0, 6));
    CHECK(sliver.isDegenerate());

    // The sum's identity, backwards: for two rectangles the erosion recovers the
    // receiver exactly.
    const RectangleShape small(1, 2, 4, 3);
    CHECK(big.minkowskiSum(small).minkowskiErosion(small) == big);
}

TEST_CASE("A half-plane erodes to a half-plane, moved in by the support point") {
    const Halfplane up(Point(0, 0), Point(1, 0));  // y >= 0

    const auto eroded = up.minkowskiErosion(RectangleShape(2, 3, 5, 7));
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(eroded)>, Halfplane>,
                  "a half-plane eroded by a bounded shape must stay a half-plane");
    CHECK(eroded == Halfplane(Point(0, -3), Point(1, -3)));  // y >= -3
    CHECK(eroded.contains(Point(100, -3)));
    CHECK_FALSE(eroded.contains(Point(0, -4)));

    // The sum and the erosion move the boundary the same distance, in opposite
    // directions.
    CHECK(up.minkowskiSum(RectangleShape(2, 3, 5, 7)) == Halfplane(Point(0, 3), Point(1, 3)));

    // Concavity, holes and disconnection are all invisible: only the support
    // point survives.
    CHECK(up.minkowskiErosion(lShape()) == Halfplane(Point(0, 0), Point(1, 0)));
    CHECK(up.minkowskiErosion(annulus()) == Halfplane(Point(0, 0), Point(1, 0)));
    CHECK(up.minkowskiErosion(RegionSet(annulus())) == Halfplane(Point(0, 0), Point(1, 0)));
    CHECK(up.minkowskiErosion(PolylineShape({Point(0, 4), Point(3, 9)})) ==
          Halfplane(Point(0, -4), Point(1, -4)));
    CHECK(up.minkowskiErosion(Chain({Point(0, 2), Point(3, 5)})) ==
          Halfplane(Point(0, -2), Point(1, -2)));

    // A slanted boundary, where the support point is a corner rather than a
    // side: `y >= x` erodes to `y >= x + 2`, the corner (2,0) of the operand
    // being the one that leaves first.
    const Halfplane slanted(Point(0, 0), Point(1, 1));
    CHECK(slanted.minkowskiErosion(RectangleShape(0, 0, 2, 2)) ==
          Halfplane(Point(-2, 0), Point(-1, 1)));
    CHECK(slanted.minkowskiSum(RectangleShape(0, 0, 2, 2)) ==
          Halfplane(Point(2, 0), Point(3, 1)));
}

TEST_CASE("A convex receiver erodes to the region its own constraints leave") {
    const Triangle tri(Point(0, 0), Point(2, 0), Point(0, 3));

    // The one worked example: 3x + 2y <= 6 moves in by the operand's height, and
    // the answer leaves the lattice even though every constraint stays on it.
    const auto eroded = tri.minkowskiErosion(Segment(Point(0, 0), Point(0, 1)));
    static_assert(std::is_same_v<typename std::remove_cvref_t<decltype(eroded)>::NumberType, int>,
                  "an erosion's constraints stay on the operands' lattice");
    REQUIRE(eroded.isBounded());
    CHECK(eroded.asConvex<pgl::ERational>() ==
          pgl::Convex<EPoint>(std::vector<EPoint>{EPoint(0, 0), EPoint(pgl::ERational(4, 3), 0),
                                                  EPoint(0, 2)}));
    checkDefinition(eroded, tri, Segment(Point(0, 0), Point(0, 1)), -4, 6);

    // The whole cross product of bounded convex receivers and operands, against
    // the definition.
    const auto sweep = [](const auto& a) {
        checkDefinition(a.minkowskiErosion(Segment(Point(0, 0), Point(2, 1))), a,
                        Segment(Point(0, 0), Point(2, 1)), -12, 14);
        checkDefinition(a.minkowskiErosion(OrientedSegment(Point(2, 1), Point(0, 0))), a,
                        OrientedSegment(Point(2, 1), Point(0, 0)), -12, 14);
        checkDefinition(a.minkowskiErosion(RectangleShape(-1, 0, 2, 3)), a,
                        RectangleShape(-1, 0, 2, 3), -12, 14);
        checkDefinition(a.minkowskiErosion(Triangle(Point(0, 0), Point(3, 1), Point(1, 3))), a,
                        Triangle(Point(0, 0), Point(3, 1), Point(1, 3)), -12, 14);
        const Convex hex(std::vector<Point>{Point(0, 0), Point(3, 0), Point(4, 2), Point(3, 4),
                                            Point(1, 4), Point(-1, 2)});
        checkDefinition(a.minkowskiErosion(hex), a, hex, -12, 14);
        // Non-convex operands reach the same overload set and are read through
        // their hull.
        checkDefinition(a.minkowskiErosion(lShape()), a, lShape(), -12, 14);
        checkDefinition(a.minkowskiErosion(annulus()), a, annulus(), -12, 14);
        checkDefinition(a.minkowskiErosion(PolylineShape({Point(0, 0), Point(3, 2), Point(1, 4)})),
                        a, PolylineShape({Point(0, 0), Point(3, 2), Point(1, 4)}), -12, 14);
        checkDefinition(a.minkowskiErosion(Chain({Point(0, 0), Point(2, 3), Point(4, 1)})), a,
                        Chain({Point(0, 0), Point(2, 3), Point(4, 1)}), -12, 14);
        checkDefinition(a.minkowskiErosion(RegionSet(annulus())), a, RegionSet(annulus()), -12, 14);
    };

    sweep(RectangleShape(0, 0, 9, 8));
    sweep(Triangle(Point(0, 0), Point(11, 0), Point(0, 9)));
    sweep(Convex(std::vector<Point>{Point(0, 0), Point(10, 0), Point(12, 6), Point(6, 11),
                                    Point(-1, 5)}));
    sweep(Segment(Point(0, 0), Point(8, 4)));
    sweep(OrientedSegment(Point(8, 4), Point(0, 0)));
    sweep(Point(3, 3));

    // A HalfplaneIntersection receiver takes the convex operands its own sum
    // takes, and no others: nothing forwards a non-convex one to it.
    const Intersection region(RectangleShape(0, 0, 9, 9));
    checkDefinition(region.minkowskiErosion(RectangleShape(-1, 0, 2, 3)), region,
                    RectangleShape(-1, 0, 2, 3), -12, 14);
    checkDefinition(region.minkowskiErosion(Triangle(Point(0, 0), Point(3, 1), Point(1, 3))),
                    region, Triangle(Point(0, 0), Point(3, 1), Point(1, 3)), -12, 14);
    checkDefinition(region.minkowskiErosion(Segment(Point(0, 0), Point(2, 1))), region,
                    Segment(Point(0, 0), Point(2, 1)), -12, 14);
}

TEST_CASE("A convex receiver erodes by the operand's convex hull") {
    // A support function sees no further than the hull, so these are the same
    // set — which is what lets a convex receiver keep the pairs whose *sum*
    // belongs to the operand.
    const RectangleShape box(0, 0, 12, 12);
    const auto direct = box.minkowskiErosion(lShape());
    const auto hulled = box.minkowskiErosion(lShape().convexHull());
    CHECK(direct == hulled);
    CHECK(box.minkowskiErosion(annulus()) == box.minkowskiErosion(annulus().convexHull()));
    CHECK(box.minkowskiErosion(RegionSet(annulus())) ==
          box.minkowskiErosion(annulus().convexHull()));
}

TEST_CASE("Eroding a sum by its own summand recovers a convex receiver") {
    // `(A ⊕ B) ⊖ B = A` for convex operands, the adjunction that makes the two
    // operations duals. It fails for a non-convex `A`, which is the whole reason
    // the region-valued erosion exists.
    const auto recovers = [](const auto& a, const auto& b) {
        const auto grown = a.minkowskiSum(b);
        const auto back = grown.minkowskiErosion(b);
        REQUIRE(back.isBounded());
        CHECK(back.template asConvex<pgl::ERational>() == pgl::Convex<EPoint>(a.vertices()));
    };
    const RectangleShape box(0, 0, 7, 5);
    const Triangle tri(Point(0, 0), Point(6, 1), Point(2, 5));
    const Convex hex(std::vector<Point>{Point(0, 0), Point(4, 0), Point(6, 3), Point(3, 6),
                                        Point(-1, 3)});
    recovers(box, tri);
    recovers(tri, box);
    recovers(hex, Segment(Point(0, 0), Point(3, 2)));
    recovers(hex, tri);
    recovers(tri, hex);

    // And the two one-sided halves of the adjunction, which hold for every pair:
    // an opening shrinks, a closing grows.
    const auto opened = box.minkowskiErosion(tri).asConvex<pgl::ERational>().minkowskiSum(tri);
    CHECK(pgl::PolygonWithHoles<EPoint>(pgl::Polygon<EPoint>(box.asPolygon()))
              .contains(pgl::Polygon<EPoint>(opened.asPolygon())));
}

TEST_CASE("An unbounded receiver erodes as a convex polyhedron") {
    const Line xAxis(Point(0, 0), Point(1, 0));
    const Ray east(Point(0, 0), Point(1, 0));
    const Halfplane up(Point(0, 0), Point(1, 0));

    // A line survives only a parallel operand, and comes back the same line.
    const auto sameLine = xAxis.minkowskiErosion(Segment(Point(0, 0), Point(5, 0)));
    REQUIRE(sameLine.getIfLine().has_value());
    CHECK(*sameLine.getIfLine() == Line(Point(0, 0), Point(1, 0)));
    CHECK(xAxis.minkowskiErosion(xAxis).getIfLine().has_value());
    CHECK(xAxis.minkowskiErosion(Segment(Point(0, 0), Point(0, 1))).empty());
    CHECK(xAxis.minkowskiErosion(Triangle(Point(0, 0), Point(3, 0), Point(0, 2))).empty());

    // A ray keeps its direction and loses its head start.
    const auto shorter = east.minkowskiErosion(Segment(Point(0, 0), Point(2, 0)));
    REQUIRE(shorter.getIfRay<int>().has_value());
    CHECK(*shorter.getIfRay<int>() == Ray(Point(0, 0), Point(1, 0)));
    CHECK(east.minkowskiErosion(Segment(Point(0, 0), Point(0, 1))).empty());
    // A ray eroded by a ray pointing the same way is the whole line's half: the
    // placements of one inside the other.
    CHECK(east.minkowskiErosion(east).getIfRay<int>().has_value());
    CHECK(east.minkowskiErosion(Ray(Point(0, 0), Point(-1, 0))).empty());

    // Two half-planes: parallel and facing the same way, the erosion is a
    // half-plane; anything else is empty.
    const auto stillUp = up.minkowskiErosion(Halfplane(Point(5, 3), Point(7, 3)));
    REQUIRE(stillUp.getIfHalfplane().has_value());
    CHECK(stillUp.getIfHalfplane()->contains(Point(0, 0)));
    CHECK(up.minkowskiErosion(Halfplane(Point(0, 9), Point(-1, 9))).empty());
    // A line or a ray parallel to the boundary is bounded in the one direction
    // the half-plane constrains, so it fits wherever the half-plane holds it:
    // the erosion is the half-plane itself.
    REQUIRE(up.minkowskiErosion(xAxis).getIfHalfplane().has_value());
    CHECK(*up.minkowskiErosion(xAxis).getIfHalfplane() == up);
    REQUIRE(up.minkowskiErosion(east).getIfHalfplane().has_value());
    CHECK(*up.minkowskiErosion(east).getIfHalfplane() == up);
    // Not parallel, and nothing fits.
    CHECK(up.minkowskiErosion(Line(Point(0, 0), Point(1, 1))).empty());
    CHECK(up.minkowskiErosion(Ray(Point(0, 0), Point(0, -1))).empty());

    // A bounded region of constraints, eroded exactly in its own coordinates.
    const Intersection box(RectangleShape(0, 0, 10, 10));
    const auto shrunk = box.minkowskiErosion(RectangleShape(0, 0, 2, 3));
    static_assert(std::is_same_v<typename std::remove_cvref_t<decltype(shrunk)>::NumberType, int>,
                  "a HalfplaneIntersection receiver erodes without dividing");
    CHECK(shrunk.asConvex<pgl::ERational>() ==
          pgl::Convex<EPoint>(pgl::Rectangle<EPoint>(EPoint(0, 0), EPoint(8, 7))));

    // An unbounded region of constraints: the quadrant eroded by a box is the
    // quadrant moved in.
    Intersection quadrant;
    quadrant.insert(Halfplane(Point(0, 0), Point(1, 0)));
    quadrant.insert(Halfplane(Point(0, 0), Point(0, -1)));
    const auto movedIn = quadrant.minkowskiErosion(RectangleShape(0, 0, 2, 3));
    CHECK_FALSE(movedIn.isBounded());
    CHECK(movedIn.contains(Point(0, 0)));
    CHECK_FALSE(movedIn.contains(Point(-1, 0)));
}

TEST_CASE("A bounded receiver eroded by an unbounded operand is empty") {
    const Line xAxis(Point(0, 0), Point(1, 0));
    const Ray east(Point(0, 0), Point(1, 0));
    const Halfplane up(Point(0, 0), Point(1, 0));
    Intersection unbounded;
    unbounded.insert(Halfplane(Point(0, 0), Point(1, 0)));

    const auto nothingFits = [&](const auto& a) {
        CHECK(a.minkowskiErosion(xAxis).empty());
        CHECK(a.minkowskiErosion(east).empty());
        CHECK(a.minkowskiErosion(up).empty());
        CHECK(a.minkowskiErosion(unbounded).empty());
    };
    nothingFits(RectangleShape(0, 0, 10, 10));
    nothingFits(Triangle(Point(0, 0), Point(9, 0), Point(0, 9)));
    nothingFits(Convex(std::vector<Point>{Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)}));
    nothingFits(Segment(Point(0, 0), Point(5, 5)));
    nothingFits(Point(1, 1));
    nothingFits(Intersection(RectangleShape(0, 0, 4, 4)));

    // The concept admits a non-convex receiver against a half-plane operand, and
    // the answer is empty for the same reason. The hull the clamp reads instead
    // of the receiver cannot change that.
    CHECK(lShape().minkowskiErosion(up).empty());
    CHECK(annulus().minkowskiErosion(up).empty());
    CHECK(RegionSet(annulus()).minkowskiErosion(up).empty());
    CHECK(PolylineShape({Point(0, 0), Point(3, 3)}).minkowskiErosion(up).empty());
    CHECK(Chain({Point(0, 0), Point(3, 3)}).minkowskiErosion(up).empty());
}

TEST_CASE("A lower-dimensional erosion is typed, not flagged") {
    // A segment eroded by a parallel segment is the stretch of placements left.
    const Segment span(Point(0, 0), Point(10, 0));
    const auto sub = span.minkowskiErosion(Segment(Point(0, 0), Point(3, 0)));
    REQUIRE(sub.getIfSegment<int>().has_value());
    CHECK(*sub.getIfSegment<int>() == Segment(Point(0, 0), Point(7, 0)));
    CHECK(span.minkowskiErosion(Segment(Point(0, 0), Point(1, 1))).empty());
    CHECK(span.minkowskiErosion(span).getIfPoint<int>().has_value());

    // A point holds only a point.
    CHECK(Point(2, 3).minkowskiErosion(Segment(Point(0, 0), Point(1, 0))).empty());
    const auto stillPoint = Point(2, 3).minkowskiErosion(Segment(Point(1, 1), Point(1, 1)));
    REQUIRE(stillPoint.getIfPoint<int>().has_value());
    CHECK(*stillPoint.getIfPoint<int>() == Point(1, 2));

    // A square eroded by an equally large square is its own corner.
    const auto corner =
        RectangleShape(0, 0, 4, 4).minkowskiErosion(Triangle(Point(0, 0), Point(4, 0), Point(0, 4)));
    REQUIRE(corner.getIfPoint<int>().has_value());
    CHECK(*corner.getIfPoint<int>() == Point(0, 0));
}

TEST_CASE("Eroding by a shape that covers no point is the whole plane") {
    const RectangleShape box(0, 0, 10, 10);

    // The one answer bigger than the receiver, and the result types split over
    // whether they can hold it.
    CHECK(box.minkowskiErosion(pgl::EmptyShape<Point>{}).isPlane());
    CHECK(box.minkowskiErosion(Convex()).isPlane());
    CHECK(box.minkowskiErosion(PolygonShape()).isPlane());
    CHECK(box.minkowskiErosion(RegionSet()).isPlane());
    CHECK(box.minkowskiErosion(Intersection(Convex())).isPlane());
    CHECK(Triangle(Point(0, 0), Point(3, 0), Point(0, 3))
              .minkowskiErosion(pgl::EmptyShape<Point>{})
              .isPlane());
    CHECK(pgl::EmptyShape<Point>{}.minkowskiErosion(pgl::EmptyShape<Point>{}).isPlane());

    CHECK_THROWS_AS(static_cast<void>(box.minkowskiErosion(RectangleShape())), std::logic_error);
    CHECK_THROWS_AS(
        static_cast<void>(Halfplane(Point(0, 0), Point(1, 0)).minkowskiErosion(Convex())),
        std::logic_error);

    // Nothing fits inside the empty set, whatever it is asked to hold.
    CHECK(pgl::EmptyShape<Point>{}.minkowskiErosion(box).empty());
    CHECK(pgl::EmptyShape<Point>{}.minkowskiErosion(Point(1, 1)) == pgl::EmptyShape<Point>{});
    CHECK(Convex().minkowskiErosion(RectangleShape(0, 0, 1, 1)).empty());
    CHECK(Intersection(Convex()).minkowskiErosion(RectangleShape(0, 0, 1, 1)).empty());
}

TEST_CASE("A HalfplaneIntersection operand is the one erosion that divides") {
    // Its support point is a crossing of two of its own boundary lines, so the
    // constraint it translates leaves the lattice — and only then does the
    // result carry division_result_t coordinates.
    const RectangleShape box(0, 0, 10, 10);
    Intersection wedge;
    wedge.insert(Halfplane(Point(0, 0), Point(3, 1)));
    wedge.insert(Halfplane(Point(3, 1), Point(0, 3)));
    wedge.insert(Halfplane(Point(0, 3), Point(0, 0)));

    const auto eroded = box.minkowskiErosion(wedge);
    static_assert(std::is_same_v<typename std::remove_cvref_t<decltype(eroded)>::NumberType,
                                 pgl::division_result_t<int>>,
                  "a HalfplaneIntersection operand erodes over division_result_t");
    REQUIRE(eroded.isBounded());
    CHECK(eroded.area<pgl::ERational>() > 0);
    // Every placement the erosion admits really does fit.
    for (const auto& vertex : eroded.vertices<pgl::ERational>()) {
        CHECK(pgl::HalfplaneIntersection<EPoint>(box).contains(vertex));
    }
}

TEST_CASE("The two curved pairs") {
    const Disk big(Point(0, 0), 5);
    const Disk small(Point(4, 1), 2);

    // Centres subtract and so do radii, exactly for centre-and-radius disks.
    const auto eroded = big.minkowskiErosion<pgl::ERational>(small);
    REQUIRE(eroded.has_value());
    CHECK(eroded->center<pgl::ERational>() == EPoint(-4, -1));
    CHECK(eroded->radius<pgl::ERational>() == 3);
    // And it is the inverse of the sum, as it is for rectangles.
    CHECK(big.minkowskiSum<pgl::ERational>(small).minkowskiErosion<pgl::ERational>(small)->center<pgl::ERational>() ==
          EPoint(0, 0));

    // The wider operand fits in nothing: `std::nullopt` is how a shape with no
    // empty state says empty.
    CHECK_FALSE(small.minkowskiErosion(big).has_value());
    // Equal radii leave the centre difference alone.
    const auto point = big.minkowskiErosion<pgl::ERational>(Disk(Point(1, 1), 5));
    REQUIRE(point.has_value());
    CHECK(point->radius<pgl::ERational>() == 0);

    // A half-plane slides in by the radius along its own normal, where the sum
    // slides it out.
    const Halfplane up(Point(0, 0), Point(1, 0));
    const auto moved = up.minkowskiErosion(big);
    CHECK(moved.contains(pgl::Point<double>(0, 5)));
    CHECK_FALSE(moved.contains(pgl::Point<double>(0, 4.9)));
    CHECK(up.minkowskiSum(big).contains(pgl::Point<double>(0, -5)));

    // A disk holds no translate of a half-plane, by the two types alone.
    const auto nothing = big.minkowskiErosion(up);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(nothing)>,
                                 pgl::EmptyShape<pgl::Point<double>>>,
                  "a disk eroded by a half-plane is the empty shape");

    // A disk that covers a single point erodes a point receiver to a point.
    const auto pinned = Point(4, 5).minkowskiErosion(Disk(Point(1, 1), 0));
    REQUIRE(pinned.getIfPoint<int>().has_value());
    CHECK(*pinned.getIfPoint<int>() == Point(3, 4));
    CHECK(Point(4, 5).minkowskiErosion(big).empty());
}

TEST_CASE("A runtime Shape erodes when the pair of alternatives has an answer") {
    const AnyShape box(RectangleShape(0, 0, 10, 10));
    const AnyShape tri(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));

    const AnyShape eroded = box.minkowskiErosion(tri);
    // The erosion needs no division for this pair, so the wrapper holds it where
    // the polyhedral *sum* of the same pair would not fit.
    REQUIRE(eroded.isHalfplaneIntersection());
    CHECK(eroded.getIfHalfplaneIntersection()->asConvex<pgl::ERational>() ==
          pgl::Convex<EPoint>(pgl::Rectangle<EPoint>(EPoint(0, 0), EPoint(8, 8))));

    // A translation keeps the stored alternative, as it does for the sum.
    const AnyShape translated = box.minkowskiErosion(AnyShape(Point(1, 2)));
    REQUIRE(translated.isRectangle());
    CHECK(*translated.getIfRectangle() == RectangleShape(-1, -2, 9, 8));

    // A mixed spelling, and a concrete operand.
    CHECK(box.minkowskiErosion(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)))
              .getIfHalfplaneIntersection()
              ->isBounded());
    CHECK(RectangleShape(0, 0, 10, 10)
              .minkowskiErosion(tri)
              .getIfHalfplaneIntersection()
              ->isBounded());

    // The pairs with no single-shape answer throw, exactly as the sum's do.
    CHECK_THROWS_AS(static_cast<void>(AnyShape(Disk(Point(0, 0), 5))
                                          .minkowskiErosion(AnyShape(Disk(Point(0, 0), 1)))),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(AnyShape(lShape()).minkowskiErosion(
                        AnyShape(RectangleShape(0, 0, 1, 1)))),
                    std::logic_error);
}

TEST_CASE("A convex receiver's erosion agrees with the region engine") {
    // The same shape, once as a convex receiver and once as a Polygon, which
    // reaches the region-valued overload. The two constructions are different
    // and the answer is not.
    const RectangleShape box(0, 0, 10, 10);
    const Triangle tri(Point(0, 0), Point(3, 0), Point(0, 2));

    const auto convexAnswer = box.minkowskiErosion(tri);
    const auto regionAnswer = PolygonShape(box.asPolygon()).minkowskiErosion(tri);
    REQUIRE(regionAnswer.componentCount() == 1);
    CHECK(regionAnswer.component(0).outer() ==
          pgl::Polygon<EPoint>(convexAnswer.asConvex<pgl::ERational>().asPolygon()));
}
