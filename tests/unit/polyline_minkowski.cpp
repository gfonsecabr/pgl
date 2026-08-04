#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using PolygonShape = pgl::Polygon<Point>;
using PolylineShape = pgl::Polyline<Point>;
using Region = pgl::PolygonWithHoles<Point>;

using EPoint = pgl::EPoint;
using EPolyline = pgl::EPolyline;

// The Minkowski sum of a `Polyline` with another bounded shape: dragging that
// shape along the chain. The receiver contributes no area of its own, and the
// answer still needs a region with holes -- a chain that comes back on itself
// closes the swept material over a cavity neither operand has. Most of the
// operands bring area; a `Segment` brings none and sweeps some out anyway, since
// an edge of the chain and the segment span a parallelogram unless the two are
// parallel.
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
// does on an area receiver: whatever the sweep leaves flat is dropped, so a point
// summand -- a translation of the chain -- comes back empty rather than as a flat
// region, and a segment summand drops the sweep of every chain edge parallel to
// it, which can leave the answer disconnected.

// -----------------------------------------------------------------------------
// Both live in a template so that the requires-expressions are dependent: a
// non-dependent one is a hard error rather than `false` under g++.

template <class A, class B>
inline constexpr bool summable = requires(const A& a, const B& b) { a.minkowskiSum(b); };

template <class A, class B>
inline constexpr bool addable = requires(const A& a, const B& b) { a + b; };

// -----------------------------------------------------------------------------
// Fixtures.

static PolygonShape box(int x0, int y0, int x1, int y1) {
    return PolygonShape({Point(x0, y0), Point(x1, y0), Point(x1, y1), Point(x0, y1)});
}

// An L bent at the origin's far corner: two edges, one turn.
static PolylineShape lChain() {
    return PolylineShape({Point(0, 0), Point(6, 0), Point(6, 6)});
}

// A U, open upward: the chain a polygon's notch would trace.
static PolylineShape uChain() {
    return PolylineShape({Point(0, 6), Point(0, 0), Point(6, 0), Point(6, 6)});
}

// A V, whose two arms meet off the axes so their sweeps cross at an angle.
static PolylineShape vChain() {
    return PolylineShape({Point(0, 0), Point(4, 6), Point(8, 0)});
}

// A U opening upward, as an area operand: the square [0,6]² with the notch
// (2,4)×(2,6] cut out. The same fixture `polygonwithholes_minkowski.cpp` uses.
static PolygonShape uShapePolygon() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 6), Point(4, 6), Point(4, 2), Point(2, 2),
                    Point(2, 6), Point(0, 6)});
}

// An L, as an area operand.
static PolygonShape lShapePolygon() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 2), Point(2, 2), Point(2, 6), Point(0, 6)});
}

// The square annulus, as a region: the one operand with a hole of its own.
static Region annulusRegion() {
    return Region(box(0, 0, 8, 8), std::vector<PolygonShape>{box(2, 2, 6, 6)});
}

// `[0,8]² ∖ (0,4)²` written as a region whose hole shares two edges with the outer
// ring. Those two shared stretches are slits -- region material with no area
// beside it -- and they sweep out area along a chain like anything else.
static Region slitRegion() {
    return Region(box(0, 0, 8, 8), std::vector<PolygonShape>{box(0, 0, 4, 4)});
}

// The same point set as `slitRegion()` minus its slits: the closure of its
// interior, which has exactly the triangles the region's domain has and none of
// the slits. What the two sums differ by is what the slits sweep.
static PolygonShape slitFreeL() {
    return PolygonShape({Point(4, 0), Point(8, 0), Point(8, 8), Point(0, 8), Point(0, 4), Point(4, 4)});
}

// The boundary of the square [0,8]², traced once and closed: the first vertex
// repeats as the last, which a polyline is free to do (it is then not simple).
static PolylineShape closedChain() {
    return PolylineShape({Point(0, 0), Point(8, 0), Point(8, 8), Point(0, 8), Point(0, 0)});
}

// The same square boundary left open by @p gap units at the bottom of its left
// wall. A summand reaching across the gap closes the sweep anyway.
static PolylineShape openChain(int gap) {
    return PolylineShape({Point(0, gap), Point(0, 8), Point(8, 8), Point(8, 0), Point(0, 0)});
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
static bool inSumByDefinition(const PolylineShape& a, const ShapeB& b, const Point& p) {
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
static void checkAgainstDefinition(const PolylineShape& a, const ShapeB& b, int lo, int hi) {
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
    const auto sum = lChain().minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(sum.size() == 1);
    CHECK(sum[0].outer() == PolygonShape({Point(0, 0), Point(7, 0), Point(7, 7), Point(6, 7),
                                     Point(6, 1), Point(0, 1)}));
    CHECK(sum[0].holeCount() == 0);
    CHECK(sum[0].twiceArea() == 2 * (7 + 6));

    // A summand thick enough to bridge the corner from the outside rounds nothing
    // off: the sweep of a corner is the summand's own corner, translated.
    const auto wide = lChain().minkowskiSum(RectangleShape(Point(-1, -1), Point(1, 1)));
    REQUIRE(wide.size() == 1);
    CHECK(wide[0].holeCount() == 0);
    CHECK(wide[0].outer() == PolygonShape({Point(-1, -1), Point(7, -1), Point(7, 7), Point(5, 7),
                                      Point(5, 1), Point(-1, 1)}));
}

TEST_CASE("minkowskiSum: a closed chain sweeps out a hole") {
    // The square boundary dragged by the unit square is a frame one unit thick,
    // and what it does not reach is a genuine hole -- the cavity the chain
    // encloses, eroded by the summand. Neither operand has a hole; no
    // `Polyline`, `Polygon` or `Convex` could hold this answer.
    const auto framed = closedChain().minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(framed.size() == 1);
    CHECK(framed[0].outer() == box(0, 0, 9, 9));
    REQUIRE(framed[0].holeCount() == 1);
    CHECK(framed[0].hole(0) == box(1, 1, 8, 8));
    CHECK(framed[0].twiceArea() == 2 * (81 - 49));

    // A bigger summand erodes the cavity further, from the far side of each wall.
    const auto thick = closedChain().minkowskiSum(RectangleShape(Point(0, 0), Point(4, 4)));
    REQUIRE(thick.size() == 1);
    CHECK(thick[0].outer() == box(0, 0, 12, 12));
    REQUIRE(thick[0].holeCount() == 1);
    CHECK(thick[0].hole(0) == box(4, 4, 8, 8));

    // A summand as wide as the square closes the cavity entirely.
    const auto filled = closedChain().minkowskiSum(RectangleShape(Point(0, 0), Point(8, 8)));
    REQUIRE(filled.size() == 1);
    CHECK(filled[0].outer() == box(0, 0, 16, 16));
    CHECK(filled[0].holeCount() == 0);

    // A triangle drags a corner along instead of a side, so the frame is thicker
    // on two sides than on the others -- and the cavity is still a hole.
    const auto slanted = closedChain().minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));
    REQUIRE(slanted.size() == 1);
    CHECK(slanted[0].outer() ==
          PolygonShape({Point(0, 0), Point(10, 0), Point(10, 8), Point(8, 10), Point(0, 10)}));
    REQUIRE(slanted[0].holeCount() == 1);
    CHECK(slanted[0].hole(0) == box(2, 2, 8, 8));
}

TEST_CASE("minkowskiSum: an open chain can still close its own sweep") {
    // The same square boundary, left open by one unit: the summand reaches across
    // the gap, the two ends of the sweep meet along a segment, and the cavity is
    // walled in exactly as it is for the closed chain.
    const auto closedOff = openChain(1).minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(closedOff.size() == 1);
    CHECK(closedOff[0].outer() == box(0, 0, 9, 9));
    REQUIRE(closedOff[0].holeCount() == 1);
    CHECK(closedOff[0].hole(0) == box(1, 1, 8, 8));

    // One unit wider and the summand no longer bridges it: the cavity opens to
    // the outside through the gap, so the sum is simply connected and its
    // boundary walks in and back out again.
    const auto open = openChain(2).minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(open.size() == 1);
    CHECK(open[0].holeCount() == 0);
    CHECK(open[0].outer() ==
          PolygonShape({Point(0, 0), Point(9, 0), Point(9, 9), Point(0, 9), Point(0, 2), Point(1, 2),
                   Point(1, 8), Point(8, 8), Point(8, 1), Point(0, 1)}));
    CHECK(open[0].twiceArea() == 62);
}

TEST_CASE("minkowskiSum: the sweep of a chain is the union of the sweeps of its parts") {
    // `(A₁ ∪ A₂) ⊕ B = (A₁ ⊕ B) ∪ (A₂ ⊕ B)`, checked on the split of a chain at
    // an interior vertex. It says the edge decomposition is not merely one that
    // works but the only one that matters: any coarser or finer split of the same
    // chain has to give the same answer.
    const PolylineShape v = vChain();
    const RectangleShape b(Point(0, 0), Point(2, 1));

    const auto whole = v.minkowskiSum(b);
    const auto left = PolylineShape({Point(0, 0), Point(4, 6)}).minkowskiSum(b);
    const auto right = PolylineShape({Point(4, 6), Point(8, 0)}).minkowskiSum(b);
    REQUIRE(whole.size() == 1);
    REQUIRE(left.size() == 1);
    REQUIRE(right.size() == 1);
    CHECK(left[0].unionWith(right[0]) == whole);

    // Splitting an edge in the middle of its length changes nothing either: the
    // extra vertex adds a piece to the decomposition that covers no new point.
    CHECK(PolylineShape({Point(0, 0), Point(2, 3), Point(4, 6), Point(8, 0)}).minkowskiSum(b) == whole);
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
        const auto sum = PolylineShape({edge.min(), edge.max()}).minkowskiSum(summand);
        REQUIRE(sum.size() == 1);
        CHECK(sum[0].holeCount() == 0);
        CHECK(sum[0].outer() == edge.minkowskiSum(summand).asPolygon());
    }

    // A chain whose vertices are collinear covers exactly that segment, however
    // many edges it takes to retrace it, so it must sum alike.
    const Triangle t(Point(0, 0), Point(2, 0), Point(0, 2));
    const auto straight = PolylineShape({Point(0, 0), Point(2, 0), Point(5, 0)}).minkowskiSum(t);
    REQUIRE(straight.size() == 1);
    CHECK(straight[0].outer() == Segment(Point(0, 0), Point(5, 0)).minkowskiSum(t).asPolygon());
}

TEST_CASE("minkowskiSum: a polygon summand, whose own concavity also strands cavities") {
    // The one non-convex operand a chain takes. A polygon that spells the same
    // point set as a convex operand has to sum alike, whichever type carries it.
    const PolylineShape chain = lChain();
    CHECK(chain.minkowskiSum(box(0, 0, 1, 1)) ==
          chain.minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1))));
    CHECK(chain.minkowskiSum(PolygonShape({Point(0, 0), Point(2, 0), Point(0, 2)})) ==
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
    const PolylineShape vertical({Point(0, 0), Point(0, 3)});
    CHECK(uShapePolygon().minkowskiSum(vertical) ==
          uShapePolygon().minkowskiSum(RectangleShape(Point(0, 0), Point(0, 3))));

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
    const PolygonShape flat({Point(0, 0), Point(4, 0)});
    REQUIRE(flat.isDegenerate());
    CHECK(PolylineShape({Point(0, 0), Point(3, 0)}).minkowskiSum(flat).empty());
    const auto crossed = vertical.minkowskiSum(flat);
    REQUIRE(crossed.size() == 1);
    CHECK(crossed[0].outer() == box(0, 0, 4, 3));

    // The empty polygon absorbs from either side.
    CHECK(chain.minkowskiSum(PolygonShape()).empty());
    CHECK(PolygonShape().minkowskiSum(chain).empty());
}

TEST_CASE("minkowskiSum: a region summand keeps its hole and sweeps its slits") {
    const Region a = annulusRegion();
    const PolylineShape unitChain({Point(0, 0), Point(1, 0)});

    // A chain of one horizontal unit covers exactly what the degenerate rectangle
    // `(0,0)--(1,0)` covers, so the answer must be the one the region receiver
    // already gives for that rectangle -- code that predates the chain operand.
    // The cavity is eroded from the left only, as it is there.
    const auto slid = a.minkowskiSum(unitChain);
    REQUIRE(slid.size() == 1);
    CHECK(slid[0].outer() == box(0, 0, 9, 8));
    REQUIRE(slid[0].holeCount() == 1);
    CHECK(slid[0].hole(0) == box(3, 2, 6, 6));
    CHECK(slid == a.minkowskiSum(RectangleShape(Point(0, 0), Point(1, 0))));

    // Either order, and the same in an exact result type.
    CHECK(unitChain.minkowskiSum(a) == slid);
    CHECK(a.minkowskiSum<pgl::ERational>(unitChain) ==
          unitChain.minkowskiSum<pgl::ERational>(a));

    // A closed unit chain erodes the cavity from all four sides at once.
    const auto framed =
        a.minkowskiSum(PolylineShape({Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1), Point(0, 0)}));
    REQUIRE(framed.size() == 1);
    CHECK(framed[0].outer() == box(0, 0, 9, 9));
    REQUIRE(framed[0].holeCount() == 1);
    CHECK(framed[0].hole(0) == box(3, 3, 6, 6));

    // A chain long enough to reach across the cavity closes it entirely.
    const auto closed = a.minkowskiSum(PolylineShape({Point(0, 0), Point(4, 0), Point(4, 4)}));
    REQUIRE(closed.size() == 1);
    CHECK(closed[0].holeCount() == 0);

    // The slits. Dragged along a diagonal chain, the two stretches the hole shares
    // with the outer ring sweep out two parallelograms at the notch's corner, and
    // those turn the notch into a genuine hole. The slit-free polygon covering the
    // same area has neither the extra material nor the hole, so this is what a
    // decomposition stopping at the triangulated domain would get wrong.
    const PolylineShape diagonal({Point(0, 0), Point(1, 1)});
    const auto swept = slitRegion().minkowskiSum(diagonal);
    REQUIRE(swept.size() == 1);
    REQUIRE(swept[0].holeCount() == 1);
    CHECK(swept[0].hole(0) == box(1, 1, 4, 4));
    CHECK(swept[0].twiceArea() == 142);

    const auto withoutSlits = slitFreeL().minkowskiSum(diagonal);
    REQUIRE(withoutSlits.size() == 1);
    CHECK(withoutSlits[0].holeCount() == 0);
    CHECK(withoutSlits[0].twiceArea() == 128);
    // The two points the slits alone reach.
    for (const Point& p : {Point(1, 0), Point(0, 1)}) {
        INFO("slit-swept point " << p);
        CHECK(swept[0].contains(p));
        CHECK_FALSE(withoutSlits[0].contains(p));
    }

    // A slit whose direction the chain shares sweeps out nothing, and the
    // regularization drops it: the vertical slit `(0,0)--(0,4)` dragged along a
    // vertical chain is a segment, a genuine part of the point set that no region
    // may keep. So (0,2) is in `A ⊕ B` and not in the answer -- the same contract
    // that makes a point summand come back empty.
    const auto vertical = slitRegion().minkowskiSum(PolylineShape({Point(0, 0), Point(0, 1)}));
    REQUIRE(vertical.size() == 1);
    CHECK(vertical[0].holeCount() == 0);
    CHECK(vertical[0].twiceArea() == 120);
    CHECK(inSumByDefinition(PolylineShape({Point(0, 0), Point(0, 1)}), slitRegion(), Point(0, 2)));
    CHECK_FALSE(inResult(vertical, Point(0, 2)));
    // The horizontal slit is across that chain, so its own sweep survives.
    CHECK(inResult(vertical, Point(2, 0)));

    // The empty region absorbs from either side.
    CHECK(unitChain.minkowskiSum(Region()).empty());
    CHECK(Region().minkowskiSum(unitChain).empty());
}

TEST_CASE("minkowskiSum: agrees with the definition over a probe grid") {
    // The strong oracle: `p ∈ A ⊕ B ⟺ A ∩ (p − B) ≠ ∅`, answered by the library's
    // own `intersects` on a reflected, translated copy of `B`.
    const std::vector<RectangleShape> rectangles{
        RectangleShape(Point(0, 0), Point(2, 1)), RectangleShape(Point(0, 0), Point(2, 2)),
        RectangleShape(Point(-1, -1), Point(1, 1)), RectangleShape(Point(0, 0), Point(1, 3))};
    const std::vector<Triangle> triangles{Triangle(Point(0, 0), Point(3, 0), Point(0, 3)),
                                          Triangle(Point(0, 0), Point(2, 0), Point(1, 3)),
                                          Triangle(Point(-2, -1), Point(2, 0), Point(0, 2))};
    const std::vector<Convex> convexes{
        Convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)}),
        Convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)})};

    for (const PolylineShape& a : {lChain(), uChain(), vChain()}) {
        for (const RectangleShape& b : rectangles) {
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
    for (const PolylineShape& a : {closedChain(), openChain(1), openChain(2)}) {
        for (const RectangleShape& b : rectangles) {
            checkAgainstDefinition(a, b, -4, 14);
        }
        checkAgainstDefinition(a, triangles[0], -4, 14);
        checkAgainstDefinition(a, convexes[1], -4, 14);
    }

    // A chain that crosses itself, and one that doubles back along an edge it has
    // already traced: both are point sets like any other, and the decomposition
    // never assumes a simple chain.
    const PolylineShape crossing({Point(0, 0), Point(6, 6), Point(6, 0), Point(0, 6)});
    const PolylineShape doubled({Point(0, 0), Point(6, 0), Point(3, 0), Point(3, 5)});
    for (const RectangleShape& b : rectangles) {
        checkAgainstDefinition(crossing, b, -4, 12);
        checkAgainstDefinition(doubled, b, -4, 12);
    }
    checkAgainstDefinition(crossing, triangles[2], -4, 12);
    checkAgainstDefinition(doubled, triangles[2], -4, 12);

    // The polygon summand, where both operands may be concave: the oracle is the
    // only check that settles the interaction of the two concavities without
    // reasoning about which piece sum covers what.
    for (const PolygonShape& b : {box(0, 0, 2, 2), uShapePolygon(), lShapePolygon()}) {
        for (const PolylineShape& a : {lChain(), uChain(), vChain()}) {
            checkAgainstDefinition(a, b, -4, 16);
        }
        checkAgainstDefinition(closedChain(), b, -4, 18);
        checkAgainstDefinition(crossing, b, -4, 16);
    }
    checkAgainstDefinition(openChain(1), uShapePolygon(), -4, 18);
    checkAgainstDefinition(doubled, lShapePolygon(), -4, 16);

    // The region summand. The annulus is the closure of its own interior, so the
    // sum is regular closed whatever the chain is and the oracle settles every
    // point of it, cavity included.
    for (const PolylineShape& a : {lChain(), vChain(), closedChain(), crossing}) {
        checkAgainstDefinition(a, annulusRegion(), -4, 18);
    }

    // The slitted region needs the chain chosen with care, and the reason is the
    // contract rather than the engine: a slit dragged along an edge of its own
    // direction sweeps out a segment, which is in `A ⊕ B` and which no region may
    // keep, so the regularized answer is smaller than the point set the oracle
    // computes. Give every chain edge a direction neither slit has and the two
    // agree again -- both slits then sweep out area, and nothing is dropped.
    for (const PolylineShape& a : {vChain(), PolylineShape({Point(0, 0), Point(1, 1)}),
                              PolylineShape({Point(0, 0), Point(3, 2), Point(6, -1)})}) {
        checkAgainstDefinition(a, slitRegion(), -4, 16);
    }
}

TEST_CASE("minkowskiSum: commutes, and translates with its operands") {
    const PolylineShape a = uChain();
    const RectangleShape rectangle(Point(0, 0), Point(2, 2));
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
    PolylineShape shifted = a;
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
    const PolylineShape reversed({Point(6, 6), Point(6, 0), Point(0, 0), Point(0, 6)});
    REQUIRE(reversed == a);
    CHECK(reversed.minkowskiSum(triangle) == a.minkowskiSum(triangle));
}

TEST_CASE("minkowskiSum: shear invariance carries the answers off the axes") {
    // Exact throughout: a sheared crossing need not be integral, and truncation
    // does not commute with the shear.
    for (int k : {1, -2}) {
        for (const PolylineShape& a : {lChain(), closedChain(), vChain()}) {
            const PolygonShape unit = box(0, 0, 1, 1);
            const Convex square(unit.vertices());
            CHECK(sheared(a, k).template minkowskiSum<pgl::ERational>(sheared(square, k)) ==
                  sheared(a.template minkowskiSum<pgl::ERational>(square), k));
        }

        // The same for the polygon summand, whose notch the shear slants too.
        const PolygonShape u = uShapePolygon();
        CHECK(sheared(vChain(), k).template minkowskiSum<pgl::ERational>(sheared(u, k)) ==
              sheared(vChain().template minkowskiSum<pgl::ERational>(u), k));
    }
}

TEST_CASE("minkowskiSum: a segment summand, where neither operand has area") {
    // The one operand a chain takes that brings no area of its own, and the sum
    // still has some: an edge of the chain and the segment span a parallelogram
    // unless the two are parallel. So the closed chain still comes back as a
    // region with a hole -- the same shape of answer a rectangle summand gives,
    // sheared into a band.
    const PolylineShape square = closedChain();
    const Segment slant(Point(0, 0), Point(2, 1));

    const auto swept = square.minkowskiSum(slant);
    REQUIRE(swept.size() == 1);
    CHECK(swept[0].outer() == PolygonShape({Point(0, 0), Point(8, 0), Point(10, 1), Point(10, 9),
                                            Point(2, 9), Point(0, 8)}));
    REQUIRE(swept[0].holeCount() == 1);
    // What the band leaves uncovered: `[0,8]² ∩ ([0,8]² + (2,1))`.
    CHECK(swept[0].hole(0) == box(2, 1, 8, 8));

    // Orientation is not part of a point set, and the same two points spelled as
    // a flat rectangle, convex or polygon answer alike.
    CHECK(square.minkowskiSum(OrientedSegment(Point(2, 1), Point(0, 0))) == swept);
    CHECK(square.minkowskiSum(Convex(std::vector<Point>{Point(0, 0), Point(2, 1)})) == swept);
    CHECK(square.minkowskiSum(PolygonShape({Point(0, 0), Point(2, 1)})) == swept);

    // Written on the left, a segment forwards to the chain, as every other
    // bounded operand does.
    CHECK(slant.minkowskiSum(square) == swept);
    CHECK(OrientedSegment(Point(0, 0), Point(2, 1)).minkowskiSum(square) == swept);
    CHECK(slant.minkowskiSum<pgl::ERational>(square) ==
          square.minkowskiSum<pgl::ERational>(slant));

    // The regularization is easiest to trip over here, and it takes nothing
    // exotic: an axis-parallel segment along the square's own edges leaves the
    // two edges it is parallel to sweeping nothing at all, so what survives is
    // the two bands the other pair sweeps -- and they do not touch. The sum of
    // two connected shapes is connected; `closure((A ⊕ B)°)` need not be.
    const auto split = square.minkowskiSum(Segment(Point(0, 0), Point(0, 3)));
    REQUIRE(split.size() == 2);
    CHECK(split[0].outer() == box(0, 0, 8, 3));
    CHECK(split[1].outer() == box(0, 8, 8, 11));
    CHECK(split[0].holeCount() == 0);
    CHECK(split[1].holeCount() == 0);
    // The vertical walls' sweep is part of the point set and not of the answer.
    CHECK(inSumByDefinition(square, Segment(Point(0, 0), Point(0, 3)), Point(0, 5)));
    CHECK_FALSE(inResult(split, Point(0, 5)));

    // An L bent at a right angle keeps only the arm across the segment.
    CHECK(lChain().minkowskiSum(Segment(Point(0, 0), Point(0, 2)))[0].outer() == box(0, 0, 6, 2));

    // A summand collapsed to a point leaves nothing at all: a chain has no area
    // to translate, so this is empty rather than the moved chain.
    CHECK(square.minkowskiSum(Segment(Point(3, 3), Point(3, 3))).empty());

    // The definition settles every probe as long as no edge of the chain runs
    // parallel to the segment -- where one does, the sum is genuinely not the
    // closure of its own interior and the case above is what states the contract.
    for (const Segment& b : {Segment(Point(0, 0), Point(2, 1)), Segment(Point(-1, 1), Point(2, -1)),
                             Segment(Point(0, 0), Point(1, 3))}) {
        checkAgainstDefinition(vChain(), b, -4, 13);
        checkAgainstDefinition(closedChain(), b, -4, 13);
    }
}

TEST_CASE("minkowskiSum: degenerate operands") {
    const PolylineShape v = vChain();

    // A chain has no area, so a summand with none either leaves nothing for the
    // regularization to keep. A point summand is the sharpest case: the sum is
    // the chain translated, a perfectly good `Polyline` and not a region at all.
    CHECK(v.minkowskiSum(RectangleShape(Point(3, 3), Point(3, 3))).empty());
    CHECK(v.minkowskiSum(Triangle(Point(1, 1), Point(1, 1), Point(1, 1))).empty());

    // A flat summand parallel to a straight chain is the same story; across it,
    // the sweep is a genuine parallelogram.
    const PolylineShape straight({Point(0, 0), Point(4, 0)});
    CHECK(straight.minkowskiSum(RectangleShape(Point(0, 0), Point(2, 0))).empty());
    const auto crossed = straight.minkowskiSum(RectangleShape(Point(0, 0), Point(0, 3)));
    REQUIRE(crossed.size() == 1);
    CHECK(crossed[0].outer() == box(0, 0, 4, 3));

    // A chain of one vertex is that point, whose sum is the summand translated
    // there -- the one decomposition that is a lone point rather than an edge.
    const auto placed =
        PolylineShape({Point(2, 3)}).minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));
    REQUIRE(placed.size() == 1);
    CHECK(placed[0].outer() == PolygonShape({Point(2, 3), Point(4, 3), Point(2, 5)}));
    CHECK(placed[0].holeCount() == 0);

    // A degenerate chain with several coincident vertices is the same point.
    CHECK(PolylineShape({Point(2, 3), Point(2, 3), Point(2, 3)})
              .minkowskiSum(Triangle(Point(0, 0), Point(2, 0), Point(0, 2))) == placed);

    // A zero-length edge inside a chain contributes nothing but is not an error.
    const auto repeated = PolylineShape({Point(0, 0), Point(0, 0), Point(4, 0)})
                              .minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(repeated.size() == 1);
    CHECK(repeated[0].outer() == box(0, 0, 5, 1));

    // The empty operands absorb, whichever side they are on.
    CHECK(PolylineShape().minkowskiSum(RectangleShape(Point(0, 0), Point(2, 2))).empty());
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
    const PolylineShape v = vChain();
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
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const RectangleShape&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const Triangle&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const Convex&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const PolygonShape&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolygonShape&>().minkowskiSum(
                                    std::declval<const PolylineShape&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const Region&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const Region&>().minkowskiSum(
                                    std::declval<const PolylineShape&>())),
                                std::vector<Region>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>()
                                              .minkowskiSum<pgl::ERational>(
                                                  std::declval<const Convex&>())),
                                std::vector<pgl::PolygonWithHoles<EPoint>>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolygonShape&>()
                                              .minkowskiSum<pgl::ERational>(
                                                  std::declval<const PolylineShape&>())),
                                std::vector<pgl::PolygonWithHoles<EPoint>>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const Point&>())),
                                PolylineShape>);
    static_assert(!pgl::MinkowskiSummableConcept<PolylineShape, PolygonShape>);

    // A `Segment` and an `OrientedSegment` join them: neither brings area, and
    // the band each edge of the chain sweeps along one has some. Both spellings
    // work, since the two segment types forward to the chain.
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const Segment&>())),
                                std::vector<Region>>);
    static_assert(summable<PolylineShape, OrientedSegment>);
    static_assert(summable<Segment, PolylineShape>);
    static_assert(summable<OrientedSegment, PolylineShape>);

    // What is left out is a second chain, and a monotone chain, which
    // `asPolyline()` converts when its sum is wanted. `operator+` stays out of
    // the region-valued case entirely.
    static_assert(!summable<PolylineShape, PolylineShape>);
    static_assert(!summable<PolylineShape, pgl::MonotoneChain<Point>>);
    static_assert(!summable<pgl::MonotoneChain<Point>, PolylineShape>);
    static_assert(!addable<PolylineShape, RectangleShape>);
    static_assert(addable<PolylineShape, Point>);
}

TEST_CASE("minkowskiSum: a chain's boundary decomposition agrees with its edges") {
    // A polyline is its own boundary, so against a convex operand it need not be
    // taken one edge at a time: consecutive edges that keep going the same way in
    // x form a monotone run, whose whole sum is one polygon from the chain sweep
    // and needs no arrangement. The answer must not depend on which of the two
    // decompositions ran, and the REQUIRE is what keeps this from passing
    // vacuously if the run decomposition ever stopped firing here.
    const PolylineShape staircase({Point(0, 0), Point(2, 3), Point(4, 1), Point(6, 4), Point(8, 2),
                                   Point(10, 5), Point(12, 3), Point(14, 6)});
    const PolylineShape monotone({Point(0, 0), Point(2, 5), Point(4, 1), Point(6, 7), Point(9, 2),
                                  Point(13, 8), Point(18, 3), Point(24, 9), Point(31, 4)});

    const std::vector<Triangle> summands{Triangle(Point(0, 0), Point(4, 0), Point(0, 4)),
                                         Triangle(Point(-1, -2), Point(3, 0), Point(0, 3))};

    for (const PolylineShape& chain : {staircase, monotone}) {
        for (const Triangle& summand : summands) {
            const auto operand = pgl::detail::minkowskiAsConvex(summand);
            auto runs = pgl::detail::minkowskiBoundaryRuns(chain);
            REQUIRE(runs.size() < chain.size() - 1);  // fewer runs than edges, or why bother
            REQUIRE(pgl::detail::minkowskiBoundaryPays(chain, operand, runs));

            auto boundary = chain.minkowskiSum<pgl::ERational>(summand);
            auto perEdge = pgl::detail::decomposedMinkowskiSum<EPoint>(chain, summand);
            std::sort(boundary.begin(), boundary.end());
            std::sort(perEdge.begin(), perEdge.end());
            CHECK(boundary == perEdge);
        }
    }

    // A chain that turns back at every vertex has one run per edge, so the two
    // decompositions produce the very same pieces and the cheaper one is taken.
    // The answer is the same either way; what is being pinned is the choice.
    const PolylineShape zigzag({Point(0, 0), Point(5, 1), Point(1, 2), Point(6, 3), Point(2, 4)});
    const auto operand = pgl::detail::minkowskiAsConvex(summands[0]);
    const auto zigzagRuns = pgl::detail::minkowskiBoundaryRuns(zigzag);
    CHECK(zigzagRuns.size() == zigzag.size() - 1);
    CHECK_FALSE(pgl::detail::minkowskiBoundaryPays(zigzag, operand, zigzagRuns));
    auto zigzagSum = zigzag.minkowskiSum<pgl::ERational>(summands[0]);
    auto zigzagRef = pgl::detail::decomposedMinkowskiSum<EPoint>(zigzag, summands[0]);
    std::sort(zigzagSum.begin(), zigzagSum.end());
    std::sort(zigzagRef.begin(), zigzagRef.end());
    CHECK(zigzagSum == zigzagRef);
}
