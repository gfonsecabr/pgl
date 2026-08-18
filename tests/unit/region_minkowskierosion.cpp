#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "pgl.hpp"

// The region-valued Minkowski erosion: `A ⊖ B = {x : x ⊕ B ⊆ A}` for a receiver
// that is not convex, where the identity the convex receivers use --
// `A ⊖ B = ⋂ᵢ (Hᵢ ⊖ B)` over the half-planes `A` is the intersection of -- says
// nothing at all. Those receivers go through the complement instead:
//
//     A ⊖ B  =  A ∖ (U ⊕ (-B)),      U = closure(W° ∖ A) for a window W ⊇ A ∪ (A ⊕ B),
//
// and two things separate the result from a sum's. It is a `PolygonSet` and not
// one `PolygonWithHoles`, because an erosion disconnects what it shrinks; and it
// is regularized, `closure((A ⊖ B)°)`, so the thin material an erosion produces
// readily is dropped rather than represented.

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
using EPolygonShape = pgl::Polygon<EPoint>;

// -----------------------------------------------------------------------------
// Fixtures, all rectilinear with integer vertices, which is what lets the oracle
// below compare cell for cell.

static PolygonShape box(int x0, int y0, int x1, int y1) {
    return PolygonShape({Point(x0, y0), Point(x1, y0), Point(x1, y1), Point(x0, y1)});
}

// A U opening upward: [0,6]² with the notch (2,4)×(2,6] cut out.
static PolygonShape uShape() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 6), Point(4, 6), Point(4, 2),
                         Point(2, 2), Point(2, 6), Point(0, 6)});
}

// An L.
static PolygonShape lShape() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 2), Point(2, 2), Point(2, 6),
                         Point(0, 6)});
}

// Two 6×6 blocks joined by a corridor two units tall: the fixture whose erosion
// by anything taller than the corridor is two components.
static PolygonShape dumbbell() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 2), Point(10, 2), Point(10, 0),
                         Point(16, 0), Point(16, 6), Point(10, 6), Point(10, 4), Point(6, 4),
                         Point(6, 6), Point(0, 6)});
}

// An L three units on a side, small enough to fit inside a block of the
// dumbbell: the operand whose concavity the erosion has to read.
static PolygonShape smallL() {
    return PolygonShape({Point(0, 0), Point(3, 0), Point(3, 1), Point(1, 1), Point(1, 3),
                         Point(0, 3)});
}

// The square annulus [0,8]² ∖ (2,6)², as a region.
static Region annulus() {
    return Region(box(0, 0, 8, 8), std::vector<PolygonShape>{box(2, 2, 6, 6)});
}

// The region whose hole shares two edges with the outer ring, so that the two
// shared stretches are slits: region material with no area beside it.
static Region slitRegion() {
    return Region(box(0, 0, 8, 8), std::vector<PolygonShape>{box(0, 0, 4, 4)});
}

// Two disjoint blocks, as a set.
static RegionSet twoBlocks() {
    return RegionSet(std::vector<Region>{Region(box(0, 0, 5, 5)), Region(box(8, 0, 14, 6))});
}

// -----------------------------------------------------------------------------
// The oracle: the erosion, computed over the lattice cells.
//
// A rectilinear shape with integer vertices is the union of the closed unit cells
// its interior meets, and the regularized erosion of one such shape by another is
// again one, so the two can be compared cell for cell -- exactly, with no
// tolerance and nothing borrowed from the construction under test.
//
// The test itself is the definition: `cell(c) ⊆ A ⊖ B` exactly when
// `cell(c) ⊕ B ⊆ A`, and for a `B` that is itself a union of cells,
// `cell(c) ⊕ cell(d)` is the 2×2 block of cells at `c + d`.

using Cell = std::pair<int, int>;

template <class ShapeT>
static std::set<Cell> cellsOf(const ShapeT& shape, int lo, int hi) {
    std::set<Cell> cells;
    const auto half = [](int doubled) { return pgl::ERational(doubled, 2); };
    for (int x = lo; x < hi; ++x) {
        for (int y = lo; y < hi; ++y) {
            if (shape.contains(EPoint(half(2 * x + 1), half(2 * y + 1)))) {
                cells.emplace(x, y);
            }
        }
    }
    return cells;
}

template <class Result, class A, class B>
static void checkCells(const Result& erosion, const A& a, const B& b, int lo, int hi) {
    const std::set<Cell> cellsA = cellsOf(a, lo, hi);
    const std::set<Cell> cellsB = cellsOf(b, lo, hi);
    REQUIRE_FALSE(cellsA.empty());
    REQUIRE_FALSE(cellsB.empty());  // an operand with no area needs the other oracle

    std::set<Cell> expected;
    for (int x = lo; x < hi; ++x) {
        for (int y = lo; y < hi; ++y) {
            bool fits = true;
            for (const auto& [dx, dy] : cellsB) {
                for (int i = 0; i <= 1 && fits; ++i) {
                    for (int j = 0; j <= 1 && fits; ++j) {
                        fits = cellsA.count(Cell(x + dx + i, y + dy + j)) != 0;
                    }
                }
                if (!fits) {
                    break;
                }
            }
            if (fits) {
                expected.emplace(x, y);
            }
        }
    }
    CHECK(cellsOf(erosion, lo, hi) == expected);
}

// The oracle for an operand the cells cannot express -- one with no area, or one
// that is not rectilinear. Every placement the erosion reports really does fit,
// which is the half of the contract a construction can get wrong silently; the
// exact answers are checked by hand at the call sites.
template <class Result, class A, class B>
static void checkSound(const Result& erosion, const A& a, const B& b, int lo, int hi) {
    std::size_t inside = 0;
    for (int x = 2 * lo; x <= 2 * hi; ++x) {
        for (int y = 2 * lo; y <= 2 * hi; ++y) {
            const EPoint probe(pgl::ERational(x, 2), pgl::ERational(y, 2));
            if (!erosion.contains(probe)) {
                continue;
            }
            ++inside;
            const auto placed = b.minkowskiSum(probe);
            if (!a.contains(placed)) {
                CHECK_MESSAGE(a.contains(placed),
                              "the erosion admits a placement that does not fit at (" << x << "/2,"
                                                                                      << y << "/2)");
                return;
            }
        }
    }
    CHECK(inside > 0);
}

// -----------------------------------------------------------------------------

TEST_CASE("A polygon erodes to the set of placements that fit") {
    const PolygonShape u = uShape();

    // The plainest case, by hand: the U shrinks on its outer boundary and the
    // notch grows into it.
    const auto unit = u.minkowskiErosion(RectangleShape(0, 0, 1, 1));
    REQUIRE(unit.componentCount() == 1);
    CHECK(unit.component(0).holes().empty());
    CHECK(unit.component(0).outer() ==
          EPolygonShape({EPoint(0, 0), EPoint(5, 0), EPoint(5, 5), EPoint(4, 5), EPoint(4, 1),
                         EPoint(1, 1), EPoint(1, 5), EPoint(0, 5)}));
    checkCells(unit, u, RectangleShape(0, 0, 1, 1), -4, 10);

    // And against the oracle over a range of operands with area.
    const auto sweep = [](const auto& a, int lo, int hi) {
        checkCells(a.minkowskiErosion(RectangleShape(0, 0, 1, 1)), a, RectangleShape(0, 0, 1, 1),
                   lo, hi);
        checkCells(a.minkowskiErosion(RectangleShape(0, 0, 2, 2)), a, RectangleShape(0, 0, 2, 2),
                   lo, hi);
        checkCells(a.minkowskiErosion(RectangleShape(1, 2, 4, 3)), a, RectangleShape(1, 2, 4, 3),
                   lo, hi);
        checkCells(a.minkowskiErosion(box(0, 0, 3, 1)), a, box(0, 0, 3, 1), lo, hi);
        checkCells(a.minkowskiErosion(lShape()), a, lShape(), lo, hi);
        checkCells(a.minkowskiErosion(Region(lShape())), a, Region(lShape()), lo, hi);
        checkCells(a.minkowskiErosion(annulus()), a, annulus(), lo, hi);
        checkCells(a.minkowskiErosion(twoBlocks()), a, twoBlocks(), lo, hi);
        checkCells(a.minkowskiErosion(Convex(std::vector<Point>{Point(0, 0), Point(2, 0),
                                                               Point(2, 2), Point(0, 2)})),
                   a, RectangleShape(0, 0, 2, 2), lo, hi);
    };

    sweep(uShape(), -8, 12);
    sweep(lShape(), -8, 12);
    sweep(dumbbell(), -8, 20);
    sweep(annulus(), -8, 12);
    sweep(slitRegion(), -8, 12);
    sweep(twoBlocks(), -8, 18);
    sweep(box(0, 0, 9, 7), -8, 14);  // a convex receiver, which takes the linear path
    sweep(Region(box(0, 0, 9, 7)), -8, 14);
}

TEST_CASE("An erosion disconnects what a sum keeps together") {
    const PolygonShape bell = dumbbell();

    // The corridor is two units tall, so an operand three units tall cannot pass
    // through it and the answer is the two blocks.
    const auto split = bell.minkowskiErosion(RectangleShape(0, 0, 1, 3));
    CHECK(split.componentCount() == 2);
    checkCells(split, bell, RectangleShape(0, 0, 1, 3), -8, 20);

    // One unit shorter, and the corridor still carries it: one component.
    const auto joined = bell.minkowskiErosion(RectangleShape(0, 0, 1, 1));
    CHECK(joined.componentCount() == 1);
    checkCells(joined, bell, RectangleShape(0, 0, 1, 1), -8, 20);

    // Exactly as tall as the corridor is the case regularization decides: the
    // placements through the corridor are a segment, which is dropped, and the
    // two blocks come back apart.
    const auto exact = bell.minkowskiErosion(RectangleShape(0, 0, 1, 2));
    CHECK(exact.componentCount() == 2);
    checkCells(exact, bell, RectangleShape(0, 0, 1, 2), -8, 20);

    // The sum of the same pair is one region, which is the asymmetry the two
    // result types record.
    static_assert(std::is_same_v<decltype(bell.minkowskiSum(RectangleShape(0, 0, 1, 3))),
                                 pgl::PolygonWithHoles<EPoint>>,
                  "the sum of a pair with a body is one region");
    static_assert(std::is_same_v<decltype(bell.minkowskiErosion(RectangleShape(0, 0, 1, 3))),
                                 pgl::PolygonSet<EPoint>>,
                  "the erosion of the same pair is a set");
}

TEST_CASE("A region's holes and slits erode outward") {
    const Region ring = annulus();

    // The cavity (2,6)² grows by the operand in every direction, and the outer
    // ring shrinks: one region, still with one hole.
    const auto eroded = ring.minkowskiErosion(RectangleShape(0, 0, 1, 1));
    REQUIRE(eroded.componentCount() == 1);
    CHECK(eroded.component(0).holes().size() == 1);
    CHECK(eroded.component(0).outer() == EPolygonShape({EPoint(0, 0), EPoint(7, 0), EPoint(7, 7),
                                                        EPoint(0, 7)}));
    checkCells(eroded, ring, RectangleShape(0, 0, 1, 1), -6, 12);

    // A wide enough operand closes the ring's walls, which are two units thick,
    // and nothing is left.
    CHECK(ring.minkowskiErosion(RectangleShape(0, 0, 3, 3)).componentCount() == 0);

    // A slit has no area and stops nothing a hole would not, but the material
    // beside it is gone all the same.
    const auto slit = slitRegion().minkowskiErosion(RectangleShape(0, 0, 1, 1));
    checkCells(slit, slitRegion(), RectangleShape(0, 0, 1, 1), -6, 12);
}

TEST_CASE("A regularized erosion drops what has no area") {
    // A corridor exactly as wide as its operand erodes to a curve, which no set
    // of regions holds: the regularization returns nothing.
    const PolygonShape corridor = box(0, 0, 8, 2);
    CHECK(corridor.minkowskiErosion(RectangleShape(0, 0, 1, 2)).componentCount() == 0);
    CHECK(corridor.minkowskiErosion(RectangleShape(0, 0, 8, 1)).componentCount() == 0);
    // One unit narrower, and there is area again.
    CHECK(corridor.minkowskiErosion(RectangleShape(0, 0, 1, 1)).componentCount() == 1);

    // A receiver with no area erodes to nothing by anything at all, which is the
    // same rule read one dimension down.
    const PolylineShape chainish({Point(0, 0), Point(5, 0), Point(5, 5)});
    CHECK(chainish.minkowskiErosion(RectangleShape(0, 0, 1, 1)).componentCount() == 0);
    CHECK(chainish.minkowskiErosion(Segment(Point(0, 0), Point(1, 0))).componentCount() == 0);
    CHECK(chainish.minkowskiErosion(chainish).componentCount() == 0);
    const Chain chain({Point(0, 0), Point(2, 1), Point(4, 0)});
    CHECK(chain.minkowskiErosion(RectangleShape(0, 0, 1, 1)).componentCount() == 0);
    CHECK(chain.minkowskiErosion(Triangle(Point(0, 0), Point(1, 0), Point(0, 1))).componentCount() ==
          0);
    CHECK(chain.minkowskiErosion(Segment(Point(0, 0), Point(1, 0))).componentCount() == 0);
    CHECK(chain.minkowskiErosion(chain).componentCount() == 0);
    // A collapsed polygon has no area either.
    CHECK(PolygonShape({Point(0, 0), Point(4, 0), Point(4, 0), Point(0, 0)})
              .minkowskiErosion(RectangleShape(0, 0, 1, 1))
              .componentCount() == 0);
}

TEST_CASE("An operand with no area still erodes") {
    const PolygonShape bell = dumbbell();

    // A segment shrinks the receiver along its own direction only, and the
    // answer is exact.
    const auto pushed = bell.minkowskiErosion(Segment(Point(0, 0), Point(4, 0)));
    REQUIRE(pushed.componentCount() == 1);
    CHECK(pushed.component(0).outer() ==
          EPolygonShape({EPoint(0, 0), EPoint(2, 0), EPoint(2, 2), EPoint(10, 2), EPoint(10, 0),
                         EPoint(12, 0), EPoint(12, 6), EPoint(10, 6), EPoint(10, 4), EPoint(2, 4),
                         EPoint(2, 6), EPoint(0, 6)}));
    checkSound(pushed, bell, Segment(Point(0, 0), Point(4, 0)), -4, 18);

    // The same operand written both ways, and a chain of two segments.
    CHECK(bell.minkowskiErosion(OrientedSegment(Point(4, 0), Point(0, 0))) == pushed);
    const PolylineShape bent({Point(0, 0), Point(2, 0), Point(2, 2)});
    const auto byChain = bell.minkowskiErosion(bent);
    checkSound(byChain, bell, bent, -4, 18);
    CHECK(byChain.componentCount() == 2);
    const Chain slope({Point(0, 0), Point(2, 2)});
    const auto bySlope = bell.minkowskiErosion(slope);
    checkSound(bySlope, bell, slope, -4, 18);
    CHECK(bySlope.componentCount() == 2);

    // A segment taller than the corridor cuts the receiver in two, exactly as a
    // rectangle does: an operand with no area erodes no less than one with some.
    const auto cut = bell.minkowskiErosion(Segment(Point(0, 0), Point(0, 3)));
    CHECK(cut.componentCount() == 2);
    checkSound(cut, bell, Segment(Point(0, 0), Point(0, 3)), -4, 18);
}

TEST_CASE("A non-rectilinear operand erodes soundly") {
    // The cell oracle cannot express a triangle, so the soundness half is checked
    // over a half-integer grid and the shape of the answer by hand.
    const PolygonShape u = uShape();
    const Triangle tri(Point(0, 0), Point(2, 0), Point(0, 2));
    const auto eroded = u.minkowskiErosion(tri);
    checkSound(eroded, u, tri, -4, 10);
    REQUIRE(eroded.componentCount() == 1);
    // Both arms of the U are exactly as wide as the operand, so the placements
    // inside them are a segment and the regularization drops them: what is left
    // with area is the corner where the bottom band meets the left arm, and the
    // hypotenuse of the operand is what bounds it.
    CHECK(eroded.component(0).outer() ==
          EPolygonShape({EPoint(0, 0), EPoint(2, 0), EPoint(0, 2)}));

    // Eroding by a convex operand and by its own convex hull agree, since the
    // hull of a convex shape is itself; eroding by a *non-convex* one does not,
    // which is exactly what the complement construction is for.
    const PolygonShape bell = dumbbell();
    const auto byEll = bell.minkowskiErosion(smallL());
    const auto byHull = bell.minkowskiErosion(smallL().convexHull());
    checkSound(byEll, bell, smallL(), -4, 18);
    checkSound(byHull, bell, smallL().convexHull(), -4, 18);
    CHECK(byEll != byHull);  // the operand's notch is read, not hulled away
    // The hull is the larger operand, so it erodes further.
    for (const auto& component : byHull) {
        CHECK(byEll.contains(component));
    }
}

TEST_CASE("Eroding a sum by its own summand contains the receiver") {
    // The closing `(A ⊕ B) ⊖ B ⊇ A`, one of the two halves of the adjunction
    // that holds for every pair, convex or not.
    const auto closingContains = [](const auto& a, const auto& b) {
        const auto closed = a.minkowskiSum(b).minkowskiErosion(b);
        CHECK(closed.contains(pgl::Polygon<EPoint>(a)));
    };
    closingContains(uShape(), RectangleShape(0, 0, 2, 1));
    closingContains(lShape(), RectangleShape(0, 0, 1, 3));
    closingContains(dumbbell(), Segment(Point(0, 0), Point(2, 1)));

    // And the opening `(A ⊖ B) ⊕ B ⊆ A`, which is the same statement read the
    // other way.
    const PolygonShape u = uShape();
    const RectangleShape unit(0, 0, 1, 1);
    const auto opened = u.minkowskiErosion(unit);
    REQUIRE(opened.componentCount() == 1);
    const auto grown = opened.component(0).minkowskiSum<pgl::ERational>(unit);
    CHECK(pgl::PolygonWithHoles<EPoint>(pgl::Polygon<EPoint>(u)).contains(grown));
}

TEST_CASE("Every receiver takes every bounded operand") {
    // One call per (receiver, operand) kind, to hold the overload sets open: the
    // erosion is defined wherever the sum is, and the sum's own operand families
    // are what these mirror.
    const auto everyOperand = [](const auto& a) {
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(RectangleShape(0, 0, 1, 1))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(Triangle(Point(0, 0), Point(1, 0),
                                                                    Point(0, 1)))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(
            Convex(std::vector<Point>{Point(0, 0), Point(1, 0), Point(1, 1)}))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(Segment(Point(0, 0), Point(1, 1)))));
        CHECK_NOTHROW(
            static_cast<void>(a.minkowskiErosion(OrientedSegment(Point(1, 1), Point(0, 0)))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(box(0, 0, 2, 1))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(Region(box(0, 0, 2, 1)))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(RegionSet(Region(box(0, 0, 2, 1))))));
        CHECK_NOTHROW(static_cast<void>(
            a.minkowskiErosion(PolylineShape({Point(0, 0), Point(1, 0), Point(1, 1)}))));
        CHECK_NOTHROW(
            static_cast<void>(a.minkowskiErosion(Chain({Point(0, 0), Point(1, 1)}))));
        // And the pairs the single-shape dispatcher owns.
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(Point(1, 1))));
        CHECK_NOTHROW(static_cast<void>(a.minkowskiErosion(pgl::EmptyShape<Point>{})));
    };
    everyOperand(uShape());
    everyOperand(annulus());
    everyOperand(twoBlocks());
    everyOperand(PolylineShape({Point(0, 0), Point(5, 0), Point(5, 5)}));
    everyOperand(Chain({Point(0, 0), Point(2, 1), Point(4, 0)}));
}

TEST_CASE("Eroding by a shape that covers no point has no set to return") {
    const PolygonShape u = uShape();

    // The whole plane is the answer, and a PolygonSet cannot hold it.
    CHECK_THROWS_AS(static_cast<void>(u.minkowskiErosion(PolygonShape())), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(u.minkowskiErosion(RegionSet())), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(u.minkowskiErosion(RectangleShape())), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(u.minkowskiErosion(Convex())), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(annulus().minkowskiErosion(PolygonShape())),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(twoBlocks().minkowskiErosion(PolygonShape())),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(PolylineShape({Point(0, 0), Point(1, 1)})
                                          .minkowskiErosion(PolygonShape())),
                    std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(Chain({Point(0, 0), Point(1, 1)})
                                          .minkowskiErosion(PolygonShape())),
                    std::logic_error);

    // The single-shape overload holds it, and returns it.
    CHECK(u.minkowskiErosion(pgl::EmptyShape<Point>{}).isPlane());

    // An empty receiver erodes to the empty set, which a PolygonSet does hold.
    CHECK(PolygonShape().minkowskiErosion(RectangleShape(0, 0, 1, 1)).componentCount() == 0);
    CHECK(RegionSet().minkowskiErosion(RectangleShape(0, 0, 1, 1)).componentCount() == 0);
}

TEST_CASE("The coordinate type is the caller's") {
    const PolygonShape u = uShape();

    // The default is the exact rational every region-valued construction
    // defaults to.
    static_assert(std::is_same_v<decltype(u.minkowskiErosion(RectangleShape(0, 0, 1, 1))),
                                 pgl::PolygonSet<EPoint>>,
                  "the erosion defaults to division_result_t coordinates");
    // And an explicit request is honoured, integral or floating.
    const auto integral = u.minkowskiErosion<int>(RectangleShape(0, 0, 1, 1));
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(integral)>, RegionSet>,
                  "an explicit ResultNumber is the result's coordinate type");
    REQUIRE(integral.componentCount() == 1);
    CHECK(integral.component(0).outer() ==
          PolygonShape({Point(0, 0), Point(5, 0), Point(5, 5), Point(4, 5), Point(4, 1),
                        Point(1, 1), Point(1, 5), Point(0, 5)}));
    const auto floating = u.minkowskiErosion<double>(RectangleShape(0, 0, 1, 1));
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(floating)>,
                                 pgl::PolygonSet<pgl::Point<double>>>,
                  "an explicit ResultNumber is the result's coordinate type");
    CHECK(floating.componentCount() == 1);
}
