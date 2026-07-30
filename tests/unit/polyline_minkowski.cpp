#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Rectangle = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using Polygon = pgl::Polygon<Point>;
using Polyline = pgl::Polyline<Point>;
using Region = pgl::PolygonWithHoles<Point>;

using EPoint = pgl::EPoint;
using EPolyline = pgl::EPolyline;

// The Minkowski sum of a `Polyline` with a shape that has area: dragging that
// shape along the chain. The receiver contributes no area of its own, and the
// answer still needs a region with holes -- a chain that comes back on itself
// closes the swept material over a cavity neither operand has.
//
// Same construction as the region-valued sums of `polygonwithholes_minkowski.cpp`
// (the identity `A ⊕ B = ⋃ᵢⱼ (Aᵢ ⊕ Bⱼ)` over a convex decomposition of each
// operand, the linear convex merge on every piece pair, one regularized union to
// assemble), with the chain's decomposition being its **edges**: a polyline is
// its own boundary, so there is no triangle anywhere in it. Its vertex set is not
// the decomposition -- the hull of the vertices is the answer only for a convex
// operand, and a chain that bends is not one.
//
// The sum is regularized, `closure((A ⊕ B)°)`, which bites harder here than it
// does on an area receiver: the sum of a chain with a shape that has *no* area
// keeps nothing at all, so a point summand -- a translation of the chain -- comes
// back empty rather than as a flat region.

// -----------------------------------------------------------------------------
// Both live in a template so that the requires-expressions are dependent: a
// non-dependent one is a hard error rather than `false` under g++.

template <class A, class B>
inline constexpr bool summable = requires(const A& a, const B& b) { a.minkowskiSum(b); };

template <class A, class B>
inline constexpr bool addable = requires(const A& a, const B& b) { a + b; };

// -----------------------------------------------------------------------------
// Fixtures.

static Polygon box(int x0, int y0, int x1, int y1) {
    return Polygon({Point(x0, y0), Point(x1, y0), Point(x1, y1), Point(x0, y1)});
}

// An L bent at the origin's far corner: two edges, one turn.
static Polyline lChain() {
    return Polyline({Point(0, 0), Point(6, 0), Point(6, 6)});
}

// A U, open upward: the chain a polygon's notch would trace.
static Polyline uChain() {
    return Polyline({Point(0, 6), Point(0, 0), Point(6, 0), Point(6, 6)});
}

// A V, whose two arms meet off the axes so their sweeps cross at an angle.
static Polyline vChain() {
    return Polyline({Point(0, 0), Point(4, 6), Point(8, 0)});
}

// A U opening upward, as an area operand: the square [0,6]² with the notch
// (2,4)×(2,6] cut out. The same fixture `polygonwithholes_minkowski.cpp` uses.
static Polygon uShapePolygon() {
    return Polygon({Point(0, 0), Point(6, 0), Point(6, 6), Point(4, 6), Point(4, 2), Point(2, 2),
                    Point(2, 6), Point(0, 6)});
}

// An L, as an area operand.
static Polygon lShapePolygon() {
    return Polygon({Point(0, 0), Point(6, 0), Point(6, 2), Point(2, 2), Point(2, 6), Point(0, 6)});
}

// The boundary of the square [0,8]², traced once and closed: the first vertex
// repeats as the last, which a polyline is free to do (it is then not simple).
static Polyline closedChain() {
    return Polyline({Point(0, 0), Point(8, 0), Point(8, 8), Point(0, 8), Point(0, 0)});
}

// The same square boundary left open by @p gap units at the bottom of its left
// wall. A summand reaching across the gap closes the sweep anyway.
static Polyline openChain(int gap) {
    return Polyline({Point(0, gap), Point(0, 8), Point(8, 8), Point(8, 0), Point(0, 0)});
}

// -----------------------------------------------------------------------------
// Oracles. Both are the ones `polygonwithholes_minkowski.cpp` uses, and neither
// knows anything about the construction under test.

// The definition of the sum read as a query: `p ∈ A ⊕ B` exactly when `A` meets
// the reflected copy of `B` placed at `p`, since `p − B` is `(−B) + p` and a 180°
// rotation about the origin *is* the negation.
//
// It settles boundary points too, provided the summand has area: `A ⊕ B` is then
// regular closed for any compact `A`, chain included, since it is a union of
// translates of `B`, each of them the closure of its own interior.
template <class ShapeB>
static bool inSumByDefinition(const Polyline& a, const ShapeB& b, const Point& p) {
    auto placed = b.rotated90(2);
    placed += p;
    return a.intersects(placed);
}

template <class Sum>
static bool inResult(const Sum& sum, const Point& p) {
    for (const auto& piece : sum) {
        if (piece.contains(p)) {
            return true;
        }
    }
    return false;
}

// Probes every integer point of a box comfortably holding the sum. The result
// type is exact on purpose: two arms of a chain can meet off the lattice (see the
// last test case), and an oracle checking the true point set has to ask for it.
template <class ShapeB>
static void checkAgainstDefinition(const Polyline& a, const ShapeB& b, int lo, int hi) {
    const auto sum = a.template minkowskiSum<pgl::ERational>(b);
    for (int x = lo; x <= hi; ++x) {
        for (int y = lo; y <= hi; ++y) {
            const Point p(x, y);
            INFO("probe " << p);
            CHECK(inResult(sum, p) == inSumByDefinition(a, b, p));
        }
    }
    // The pieces tile the sum: their interiors are pairwise disjoint.
    for (std::size_t i = 0; i < sum.size(); ++i) {
        REQUIRE(sum[i].isValid());
        for (std::size_t j = i + 1; j < sum.size(); ++j) {
            REQUIRE_FALSE(sum[i].interiorsIntersect(sum[j]));
        }
    }
}

// The shear `(x,y) ↦ (x, y + kx)`, a unimodular linear map. The sum commutes with
// every linear map, so this carries the rectilinear answers onto slanted edges.
template <pgl::PointConcept PointT>
static PointT sheared(const PointT& p, int k) {
    using Number = typename PointT::NumberType;
    return PointT(p.x(), p.y() + Number(k) * p.x());
}

template <pgl::PolylineConcept PolylineT>
static PolylineT sheared(const PolylineT& chain, int k) {
    std::vector<typename PolylineT::PointType> vertices;
    for (const auto& vertex : chain.vertices()) {
        vertices.push_back(sheared(vertex, k));
    }
    return PolylineT(std::move(vertices));
}

template <pgl::ConvexConcept ConvexT>
static ConvexT sheared(const ConvexT& convex, int k) {
    std::vector<typename ConvexT::PointType> vertices;
    for (const auto& vertex : convex) {
        vertices.push_back(sheared(vertex, k));
    }
    return ConvexT(std::move(vertices));
}

template <pgl::PolygonConcept PolygonT>
static PolygonT sheared(const PolygonT& polygon, int k) {
    std::vector<typename PolygonT::PointType> vertices;
    for (const auto& vertex : polygon.vertices()) {
        vertices.push_back(sheared(vertex, k));
    }
    return PolygonT(std::move(vertices));
}

template <pgl::PolygonWithHolesConcept RegionT>
static RegionT sheared(const RegionT& region, int k) {
    std::vector<pgl::Polygon<typename RegionT::PointType>> holes;
    for (const auto& hole : region.holes()) {
        holes.push_back(sheared(hole, k));
    }
    return RegionT(sheared(region.outer(), k), std::move(holes));
}

template <class RegionT>
static std::vector<RegionT> sheared(const std::vector<RegionT>& sum, int k) {
    std::vector<RegionT> mapped;
    for (const auto& piece : sum) {
        mapped.push_back(sheared(piece, k));
    }
    std::sort(mapped.begin(), mapped.end());
    return mapped;
}

// -----------------------------------------------------------------------------

TEST_CASE("minkowskiSum: dragging a shape along a chain sweeps out a region") {
    // The L dragged by the unit square: a one-unit band along each arm, joined at
    // the corner, and nothing else.
    const auto sum = lChain().minkowskiSum(Rectangle(Point(0, 0), Point(1, 1)));
    REQUIRE(sum.size() == 1);
    CHECK(sum[0].outer() == Polygon({Point(0, 0), Point(7, 0), Point(7, 7), Point(6, 7),
                                     Point(6, 1), Point(0, 1)}));
    CHECK(sum[0].holeCount() == 0);
    CHECK(sum[0].twiceArea() == 2 * (7 + 6));

    // A summand thick enough to bridge the corner from the outside rounds nothing
    // off: the sweep of a corner is the summand's own corner, translated.
    const auto wide = lChain().minkowskiSum(Rectangle(Point(-1, -1), Point(1, 1)));
    REQUIRE(wide.size() == 1);
    CHECK(wide[0].holeCount() == 0);
    CHECK(wide[0].outer() == Polygon({Point(-1, -1), Point(7, -1), Point(7, 7), Point(5, 7),
                                      Point(5, 1), Point(-1, 1)}));
}

TEST_CASE("minkowskiSum: a closed chain sweeps out a hole") {
    // The square boundary dragged by the unit square is a frame one unit thick,
    // and what it does not reach is a genuine hole -- the cavity the chain
    // encloses, eroded by the summand. Neither operand has a hole; no
    // `Polyline`, `Polygon` or `Convex` could hold this answer.
    const auto framed = closedChain().minkowskiSum(Rectangle(Point(0, 0), Point(1, 1)));
    REQUIRE(framed.size() == 1);
    CHECK(framed[0].outer() == box(0, 0, 9, 9));
    REQUIRE(framed[0].holeCount() == 1);
    CHECK(framed[0].hole(0) == box(1, 1, 8, 8));
    CHECK(framed[0].twiceArea() == 2 * (81 - 49));

    // A bigger summand erodes the cavity further, from the far side of each wall.
    const auto thick = closedChain().minkowskiSum(Rectangle(Point(0, 0), Point(4, 4)));
    REQUIRE(thick.size() == 1);
    CHECK(thick[0].outer() == box(0, 0, 12, 12));
    REQUIRE(thick[0].holeCount() == 1);
    CHECK(thick[0].hole(0) == box(4, 4, 8, 8));

    // A summand as wide as the square closes the cavity entirely.
    const auto filled = closedChain().minkowskiSum(Rectangle(Point(0, 0), Point(8, 8)));
    REQUIRE(filled.size() == 1);
    CHECK(filled[0].outer() == box(0, 0, 16, 16));
    CHECK(filled[0].holeCount() == 0);

    // A triangle drags a corner along instead of a side, so the frame is thicker
    // on two sides than on the others -- and the cavity is still a hole.
    const auto slanted = closedChain().minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));
    REQUIRE(slanted.size() == 1);
    CHECK(slanted[0].outer() ==
          Polygon({Point(0, 0), Point(10, 0), Point(10, 8), Point(8, 10), Point(0, 10)}));
    REQUIRE(slanted[0].holeCount() == 1);
    CHECK(slanted[0].hole(0) == box(2, 2, 8, 8));
}

TEST_CASE("minkowskiSum: an open chain can still close its own sweep") {
    // The same square boundary, left open by one unit: the summand reaches across
    // the gap, the two ends of the sweep meet along a segment, and the cavity is
    // walled in exactly as it is for the closed chain.
    const auto closedOff = openChain(1).minkowskiSum(Rectangle(Point(0, 0), Point(1, 1)));
    REQUIRE(closedOff.size() == 1);
    CHECK(closedOff[0].outer() == box(0, 0, 9, 9));
    REQUIRE(closedOff[0].holeCount() == 1);
    CHECK(closedOff[0].hole(0) == box(1, 1, 8, 8));

    // One unit wider and the summand no longer bridges it: the cavity opens to
    // the outside through the gap, so the sum is simply connected and its
    // boundary walks in and back out again.
    const auto open = openChain(2).minkowskiSum(Rectangle(Point(0, 0), Point(1, 1)));
    REQUIRE(open.size() == 1);
    CHECK(open[0].holeCount() == 0);
    CHECK(open[0].outer() ==
          Polygon({Point(0, 0), Point(9, 0), Point(9, 9), Point(0, 9), Point(0, 2), Point(1, 2),
                   Point(1, 8), Point(8, 8), Point(8, 1), Point(0, 1)}));
    CHECK(open[0].twiceArea() == 62);
}

TEST_CASE("minkowskiSum: the sweep of a chain is the union of the sweeps of its parts") {
    // `(A₁ ∪ A₂) ⊕ B = (A₁ ⊕ B) ∪ (A₂ ⊕ B)`, checked on the split of a chain at
    // an interior vertex. It says the edge decomposition is not merely one that
    // works but the only one that matters: any coarser or finer split of the same
    // chain has to give the same answer.
    const Polyline v = vChain();
    const Rectangle b(Point(0, 0), Point(2, 1));

    const auto whole = v.minkowskiSum(b);
    const auto left = Polyline({Point(0, 0), Point(4, 6)}).minkowskiSum(b);
    const auto right = Polyline({Point(4, 6), Point(8, 0)}).minkowskiSum(b);
    REQUIRE(whole.size() == 1);
    REQUIRE(left.size() == 1);
    REQUIRE(right.size() == 1);
    CHECK(left[0].unionWith(right[0]) == whole);

    // Splitting an edge in the middle of its length changes nothing either: the
    // extra vertex adds a piece to the decomposition that covers no new point.
    CHECK(Polyline({Point(0, 0), Point(2, 3), Point(4, 6), Point(8, 0)}).minkowskiSum(b) == whole);
}

TEST_CASE("minkowskiSum: a single-edge chain answers as the convex merge does") {
    // Grounding the construction in the tested shape-valued sum: a chain of one
    // edge is a segment, whose sum with a bounded convex shape is the single
    // `Convex` the linear merge produces.
    const Segment edge(Point(1, 1), Point(5, 3));
    const std::vector<Convex> summands{
        Convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(1, 2)}),
        Convex(std::vector<Point>{Point(-1, -1), Point(1, -1), Point(1, 1), Point(-1, 1)}),
        Convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)})};

    for (const Convex& summand : summands) {
        const auto sum = Polyline({edge.min(), edge.max()}).minkowskiSum(summand);
        REQUIRE(sum.size() == 1);
        CHECK(sum[0].holeCount() == 0);
        CHECK(sum[0].outer() == edge.minkowskiSum(summand).asPolygon());
    }

    // A chain whose vertices are collinear covers exactly that segment, however
    // many edges it takes to retrace it, so it must sum alike.
    const Triangle t(Point(0, 0), Point(2, 0), Point(0, 2));
    const auto straight = Polyline({Point(0, 0), Point(2, 0), Point(5, 0)}).minkowskiSum(t);
    REQUIRE(straight.size() == 1);
    CHECK(straight[0].outer() == Segment(Point(0, 0), Point(5, 0)).minkowskiSum(t).asPolygon());
}

TEST_CASE("minkowskiSum: a polygon summand, whose own concavity also strands cavities") {
    // The one non-convex operand a chain takes. A polygon that spells the same
    // point set as a convex operand has to sum alike, whichever type carries it.
    const Polyline chain = lChain();
    CHECK(chain.minkowskiSum(box(0, 0, 1, 1)) ==
          chain.minkowskiSum(Rectangle(Point(0, 0), Point(1, 1))));
    CHECK(chain.minkowskiSum(Polygon({Point(0, 0), Point(2, 0), Point(0, 2)})) ==
          chain.minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2))));

    // Written the other way round it is the same call: `Polygon` carries the
    // mirror overload, so neither spelling is the privileged one.
    CHECK(box(0, 0, 1, 1).minkowskiSum(chain) == chain.minkowskiSum(box(0, 0, 1, 1)));
    CHECK(uShapePolygon().minkowskiSum(chain) == chain.minkowskiSum(uShapePolygon()));
    CHECK(uShapePolygon().minkowskiSum<pgl::ERational>(chain) ==
          chain.minkowskiSum<pgl::ERational>(uShapePolygon()));

    // A chain with no bend is a segment, so a polygon swept along one must give
    // what the polygon receiver already gives for the degenerate rectangle that
    // covers the same segment -- code that predates the chain operand entirely.
    const Polyline vertical({Point(0, 0), Point(0, 3)});
    CHECK(uShapePolygon().minkowskiSum(vertical) ==
          uShapePolygon().minkowskiSum(Rectangle(Point(0, 0), Point(0, 3))));

    // Both operands non-convex: the chain's turns and the polygon's notch each
    // reach around the other, and the answer still has one hole -- what the U
    // sweeping the square's boundary cannot reach at the centre.
    const auto both = closedChain().minkowskiSum(uShapePolygon());
    REQUIRE(both.size() == 1);
    CHECK(both[0].outer() == box(0, 0, 14, 14));
    REQUIRE(both[0].holeCount() == 1);
    CHECK(both[0].hole(0) == box(6, 6, 8, 8));
    CHECK(both[0].twiceArea() == 2 * (196 - 4));

    // A polygon with no area is a segment again: parallel to a straight chain the
    // sum keeps nothing, across it the sweep is a parallelogram.
    const Polygon flat({Point(0, 0), Point(4, 0)});
    REQUIRE(flat.isDegenerate());
    CHECK(Polyline({Point(0, 0), Point(3, 0)}).minkowskiSum(flat).empty());
    const auto crossed = vertical.minkowskiSum(flat);
    REQUIRE(crossed.size() == 1);
    CHECK(crossed[0].outer() == box(0, 0, 4, 3));

    // The empty polygon absorbs from either side.
    CHECK(chain.minkowskiSum(Polygon()).empty());
    CHECK(Polygon().minkowskiSum(chain).empty());
}

TEST_CASE("minkowskiSum: agrees with the definition over a probe grid") {
    // The strong oracle: `p ∈ A ⊕ B ⟺ A ∩ (p − B) ≠ ∅`, answered by the library's
    // own `intersects` on a reflected, translated copy of `B`.
    const std::vector<Rectangle> rectangles{
        Rectangle(Point(0, 0), Point(2, 1)), Rectangle(Point(0, 0), Point(2, 2)),
        Rectangle(Point(-1, -1), Point(1, 1)), Rectangle(Point(0, 0), Point(1, 3))};
    const std::vector<Triangle> triangles{Triangle(Point(0, 0), Point(3, 0), Point(0, 3)),
                                          Triangle(Point(0, 0), Point(2, 0), Point(1, 3)),
                                          Triangle(Point(-2, -1), Point(2, 0), Point(0, 2))};
    const std::vector<Convex> convexes{
        Convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)}),
        Convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)})};

    for (const Polyline& a : {lChain(), uChain(), vChain()}) {
        for (const Rectangle& b : rectangles) {
            checkAgainstDefinition(a, b, -4, 13);
        }
        for (const Triangle& b : triangles) {
            checkAgainstDefinition(a, b, -4, 13);
        }
        for (const Convex& b : convexes) {
            checkAgainstDefinition(a, b, -4, 13);
        }
    }

    // The chains whose sweep has a hole: the oracle settles the cavity as readily
    // as the material, since it never asks how the answer was assembled.
    for (const Polyline& a : {closedChain(), openChain(1), openChain(2)}) {
        for (const Rectangle& b : rectangles) {
            checkAgainstDefinition(a, b, -4, 14);
        }
        checkAgainstDefinition(a, triangles[0], -4, 14);
        checkAgainstDefinition(a, convexes[1], -4, 14);
    }

    // A chain that crosses itself, and one that doubles back along an edge it has
    // already traced: both are point sets like any other, and the decomposition
    // never assumes a simple chain.
    const Polyline crossing({Point(0, 0), Point(6, 6), Point(6, 0), Point(0, 6)});
    const Polyline doubled({Point(0, 0), Point(6, 0), Point(3, 0), Point(3, 5)});
    for (const Rectangle& b : rectangles) {
        checkAgainstDefinition(crossing, b, -4, 12);
        checkAgainstDefinition(doubled, b, -4, 12);
    }
    checkAgainstDefinition(crossing, triangles[2], -4, 12);
    checkAgainstDefinition(doubled, triangles[2], -4, 12);

    // The polygon summand, where both operands may be concave: the oracle is the
    // only check that settles the interaction of the two concavities without
    // reasoning about which piece sum covers what.
    for (const Polygon& b : {box(0, 0, 2, 2), uShapePolygon(), lShapePolygon()}) {
        for (const Polyline& a : {lChain(), uChain(), vChain()}) {
            checkAgainstDefinition(a, b, -4, 16);
        }
        checkAgainstDefinition(closedChain(), b, -4, 18);
        checkAgainstDefinition(crossing, b, -4, 16);
    }
    checkAgainstDefinition(openChain(1), uShapePolygon(), -4, 18);
    checkAgainstDefinition(doubled, lShapePolygon(), -4, 16);
}

TEST_CASE("minkowskiSum: commutes, and translates with its operands") {
    const Polyline a = uChain();
    const Rectangle rectangle(Point(0, 0), Point(2, 2));
    const Triangle triangle(Point(0, 0), Point(2, 0), Point(0, 2));
    const Convex convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)});

    // Only the polyline can hold the answer, so a convex operand written first
    // forwards to it -- the same rule the region-valued sums follow.
    CHECK(rectangle.minkowskiSum(a) == a.minkowskiSum(rectangle));
    CHECK(triangle.minkowskiSum(a) == a.minkowskiSum(triangle));
    CHECK(convex.minkowskiSum(a) == a.minkowskiSum(convex));
    CHECK(triangle.minkowskiSum<pgl::ERational>(a) == a.minkowskiSum<pgl::ERational>(triangle));

    // A rectangle, the convex square and the same square as a `Convex` are one
    // point set, so they sum alike whichever type carries them.
    CHECK(a.minkowskiSum(rectangle) == a.minkowskiSum(convex));

    // `(A + t) ⊕ B = (A ⊕ B) + t`, which also exercises the canonicalization
    // every returned ring goes through.
    const Point t(-5, 3);
    Polyline shifted = a;
    shifted += t;
    const auto direct = shifted.minkowskiSum(triangle);
    auto moved = a.minkowskiSum(triangle);
    REQUIRE(direct.size() == moved.size());
    for (auto& piece : moved) {
        piece += t;
    }
    std::sort(moved.begin(), moved.end());
    CHECK(direct == moved);

    // A polyline traversed backwards is the same point set, and compares equal;
    // its sum must agree too.
    const Polyline reversed({Point(6, 6), Point(6, 0), Point(0, 0), Point(0, 6)});
    REQUIRE(reversed == a);
    CHECK(reversed.minkowskiSum(triangle) == a.minkowskiSum(triangle));
}

TEST_CASE("minkowskiSum: shear invariance carries the answers off the axes") {
    // Exact throughout: a sheared crossing need not be integral, and truncation
    // does not commute with the shear.
    for (int k : {1, -2}) {
        for (const Polyline& a : {lChain(), closedChain(), vChain()}) {
            const Polygon unit = box(0, 0, 1, 1);
            const Convex square(unit.vertices());
            CHECK(sheared(a, k).template minkowskiSum<pgl::ERational>(sheared(square, k)) ==
                  sheared(a.template minkowskiSum<pgl::ERational>(square), k));
        }

        // The same for the polygon summand, whose notch the shear slants too.
        const Polygon u = uShapePolygon();
        CHECK(sheared(vChain(), k).template minkowskiSum<pgl::ERational>(sheared(u, k)) ==
              sheared(vChain().template minkowskiSum<pgl::ERational>(u), k));
    }
}

TEST_CASE("minkowskiSum: degenerate operands") {
    const Polyline v = vChain();

    // A chain has no area, so a summand with none either leaves nothing for the
    // regularization to keep. A point summand is the sharpest case: the sum is
    // the chain translated, a perfectly good `Polyline` and not a region at all.
    CHECK(v.minkowskiSum(Rectangle(Point(3, 3), Point(3, 3))).empty());
    CHECK(v.minkowskiSum(Triangle(Point(1, 1), Point(1, 1), Point(1, 1))).empty());

    // A flat summand parallel to a straight chain is the same story; across it,
    // the sweep is a genuine parallelogram.
    const Polyline straight({Point(0, 0), Point(4, 0)});
    CHECK(straight.minkowskiSum(Rectangle(Point(0, 0), Point(2, 0))).empty());
    const auto crossed = straight.minkowskiSum(Rectangle(Point(0, 0), Point(0, 3)));
    REQUIRE(crossed.size() == 1);
    CHECK(crossed[0].outer() == box(0, 0, 4, 3));

    // A chain of one vertex is that point, whose sum is the summand translated
    // there -- the one decomposition that is a lone point rather than an edge.
    const auto placed =
        Polyline({Point(2, 3)}).minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));
    REQUIRE(placed.size() == 1);
    CHECK(placed[0].outer() == Polygon({Point(2, 3), Point(4, 3), Point(2, 5)}));
    CHECK(placed[0].holeCount() == 0);

    // A degenerate chain with several coincident vertices is the same point.
    CHECK(Polyline({Point(2, 3), Point(2, 3), Point(2, 3)})
              .minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2))) == placed);

    // A zero-length edge inside a chain contributes nothing but is not an error.
    const auto repeated = Polyline({Point(0, 0), Point(0, 0), Point(4, 0)})
                              .minkowskiSum(Rectangle(Point(0, 0), Point(1, 1)));
    REQUIRE(repeated.size() == 1);
    CHECK(repeated[0].outer() == box(0, 0, 5, 1));

    // The empty operands absorb, whichever side they are on.
    CHECK(Polyline().minkowskiSum(Rectangle(Point(0, 0), Point(2, 2))).empty());
    CHECK(v.minkowskiSum(Convex()).empty());
    CHECK(Convex().minkowskiSum(v).empty());
}

TEST_CASE("minkowskiSum: exact over rational coordinates") {
    const EPolyline chain({EPoint(0, 0), EPoint(8, 0), EPoint(8, 8), EPoint(0, 8), EPoint(0, 0)});
    const pgl::Rectangle<EPoint> unit(EPoint(0, 0), EPoint(1, 1));

    const auto sum = chain.minkowskiSum(unit);
    REQUIRE(sum.size() == 1);
    CHECK(sum[0].outer() ==
          pgl::Polygon<EPoint>({EPoint(0, 0), EPoint(9, 0), EPoint(9, 9), EPoint(0, 9)}));
    REQUIRE(sum[0].holeCount() == 1);
    CHECK(sum[0].hole(0) ==
          pgl::Polygon<EPoint>({EPoint(1, 1), EPoint(8, 1), EPoint(8, 8), EPoint(1, 8)}));

    // Half-integral vertices are as exact as integral ones, and the answer's
    // coordinates are just the sums of the operands'.
    const EPolyline half({EPoint(pgl::ERational(1, 2), 0), EPoint(pgl::ERational(1, 2), 4)});
    const auto swept = half.minkowskiSum(unit);
    REQUIRE(swept.size() == 1);
    CHECK(swept[0].outer() == pgl::Polygon<EPoint>({EPoint(pgl::ERational(1, 2), 0),
                                                    EPoint(pgl::ERational(3, 2), 0),
                                                    EPoint(pgl::ERational(3, 2), 5),
                                                    EPoint(pgl::ERational(1, 2), 5)}));
}

TEST_CASE("minkowskiSum: a non-integral crossing needs an exact result type") {
    // Where two arms of the chain sweep into each other, the boundary of the sum
    // has a vertex neither operand's lattice holds: the V's two arms, dragged
    // along a right triangle, meet at (11/2, 15/4). That vertex is a genuine
    // feature of the answer -- no decomposition puts it anywhere else -- so an
    // integral result type has nowhere to put it and truncates, exactly as the
    // region-valued sums and the boolean operations document.
    const Polyline v = vChain();
    const Triangle t(Point(0, 0), Point(3, 0), Point(0, 3));

    const auto exact = v.minkowskiSum<pgl::ERational>(t);
    REQUIRE(exact.size() == 1);
    const auto& ring = exact[0].outer().vertices();
    const pgl::Point<pgl::ERational> tip(pgl::ERational(11, 2), pgl::ERational(15, 4));
    CHECK(std::find(ring.begin(), ring.end(), tip) != ring.end());
    CHECK(exact[0].twiceArea() == pgl::ERational(393, 4));

    // The default result type is the operands' own, so the same call in `int`
    // rounds that vertex to (5,3) and reports as covered a point the definition
    // says is not: the truncated notch is shallower than the true one.
    const auto truncated = v.minkowskiSum(t);
    REQUIRE(truncated.size() == 1);
    CHECK(truncated[0].twiceArea() == 102);
    const pgl::Point<pgl::ERational> outside(pgl::ERational(26, 5), pgl::ERational(31, 10));
    CHECK_FALSE(exact[0].contains(outside));
    CHECK(truncated[0].contains(outside));
}

TEST_CASE("minkowskiSum: the pairs a chain accepts") {
    // The receiver has no area, so the operands are exactly the shapes that have
    // some. `MinkowskiSummableConcept` still rejects every one of those pairs --
    // it gates the sums that fit in a single shape -- and a `Point` operand still
    // goes through it, giving back a translated `Polyline`.
    static_assert(std::is_same_v<decltype(std::declval<const Polyline&>().minkowskiSum(
                                    std::declval<const Rectangle&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polyline&>().minkowskiSum(
                                    std::declval<const Triangle&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polyline&>().minkowskiSum(
                                    std::declval<const Convex&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polyline&>().minkowskiSum(
                                    std::declval<const Polygon&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polygon&>().minkowskiSum(
                                    std::declval<const Polyline&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polyline&>()
                                              .minkowskiSum<pgl::ERational>(
                                                  std::declval<const Convex&>())),
                                std::vector<pgl::PolygonWithHoles<EPoint>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polygon&>()
                                              .minkowskiSum<pgl::ERational>(
                                                  std::declval<const Polyline&>())),
                                std::vector<pgl::PolygonWithHoles<EPoint>>>);
    static_assert(std::is_same_v<decltype(std::declval<const Polyline&>().minkowskiSum(
                                    std::declval<const Point&>())),
                                Polyline>);
    static_assert(!pgl::MinkowskiSummableConcept<Polyline, Polygon>);

    // Two operands without area between them have no sum at all, in any type: a
    // second chain, a segment. A `PolygonWithHoles` summand *would* sum with a
    // chain -- the construction is the polygon one -- but no overload claims the
    // pair, so it is a compile error rather than an answer. `operator+` stays out
    // of the region-valued case entirely.
    static_assert(!summable<Polyline, Polyline>);
    static_assert(!summable<Polyline, Segment>);
    static_assert(!summable<Polyline, Region>);
    static_assert(!summable<Region, Polyline>);
    static_assert(!addable<Polyline, Rectangle>);
    static_assert(addable<Polyline, Point>);
}
