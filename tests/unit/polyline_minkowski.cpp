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
using RegionSet = pgl::PolygonSet<Point>;

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

template <pgl::PolygonWithHolesConcept RegionT>
static bool inResult(const RegionT& sum, const Point& p) {
    return sum.contains(p);
}

template <pgl::PolygonSetConcept SetT>
static bool inResult(const SetT& sum, const Point& p) {
    return sum.contains(p);
}

template <class RegionT>
static bool inResult(const std::vector<RegionT>& sum, const Point& p) {
    for (const RegionT& piece : sum) {
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
    if constexpr (pgl::PolygonWithHolesConcept<decltype(sum)>) {
        REQUIRE(sum.isValid());
    } else {
        for (std::size_t i = 0; i < sum.componentCount(); ++i) {
            REQUIRE(sum.component(i).isValid());
            for (std::size_t j = i + 1; j < sum.componentCount(); ++j) {
                REQUIRE_FALSE(sum.component(i).interiorsIntersect(sum.component(j)));
            }
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

template <pgl::PolygonSetConcept SetT>
static SetT sheared(const SetT& sum, int k) {
    std::vector<typename SetT::ComponentType> mapped;
    for (const auto& piece : sum) {
        mapped.push_back(sheared(piece, k));
    }
    return SetT(std::move(mapped));
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
    const auto sum = lChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
    CHECK(sum.outer() == PolygonShape({Point(0, 0), Point(7, 0), Point(7, 7), Point(6, 7),
                                       Point(6, 1), Point(0, 1)}));
    CHECK(sum.holeCount() == 0);
    CHECK(sum.twiceArea() == 2 * (7 + 6));

    // A summand thick enough to bridge the corner from the outside rounds nothing
    // off: the sweep of a corner is the summand's own corner, translated.
    const auto wide = lChain().minkowskiSum<int>(RectangleShape(Point(-1, -1), Point(1, 1)));
    CHECK(wide.holeCount() == 0);
    CHECK(wide.outer() == PolygonShape({Point(-1, -1), Point(7, -1), Point(7, 7), Point(5, 7),
                                        Point(5, 1), Point(-1, 1)}));
}

TEST_CASE("minkowskiSum: a closed chain sweeps out a hole") {
    // The square boundary dragged by the unit square is a frame one unit thick,
    // and what it does not reach is a genuine hole -- the cavity the chain
    // encloses, eroded by the summand. Neither operand has a hole; no
    // `Polyline`, `Polygon` or `Convex` could hold this answer.
    const auto framed = closedChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
    CHECK(framed.outer() == box(0, 0, 9, 9));
    REQUIRE(framed.holeCount() == 1);
    CHECK(framed.hole(0) == box(1, 1, 8, 8));
    CHECK(framed.twiceArea() == 2 * (81 - 49));

    // A bigger summand erodes the cavity further, from the far side of each wall.
    const auto thick = closedChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(4, 4)));
    CHECK(thick.outer() == box(0, 0, 12, 12));
    REQUIRE(thick.holeCount() == 1);
    CHECK(thick.hole(0) == box(4, 4, 8, 8));

    // A summand as wide as the square closes the cavity entirely.
    const auto filled = closedChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(8, 8)));
    CHECK(filled.outer() == box(0, 0, 16, 16));
    CHECK(filled.holeCount() == 0);

    // A triangle drags a corner along instead of a side, so the frame is thicker
    // on two sides than on the others -- and the cavity is still a hole.
    const auto slanted = closedChain().minkowskiSum<int>(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));
    CHECK(slanted.outer() ==
          PolygonShape({Point(0, 0), Point(10, 0), Point(10, 8), Point(8, 10), Point(0, 10)}));
    REQUIRE(slanted.holeCount() == 1);
    CHECK(slanted.hole(0) == box(2, 2, 8, 8));
}

TEST_CASE("minkowskiSum: an open chain can still close its own sweep") {
    // The same square boundary, left open by one unit: the summand reaches across
    // the gap, the two ends of the sweep meet along a segment, and the cavity is
    // walled in exactly as it is for the closed chain.
    const auto closedOff = openChain(1).minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
    CHECK(closedOff.outer() == box(0, 0, 9, 9));
    REQUIRE(closedOff.holeCount() == 1);
    CHECK(closedOff.hole(0) == box(1, 1, 8, 8));

    // One unit wider and the summand no longer bridges it: the cavity opens to
    // the outside through the gap, so the sum is simply connected and its
    // boundary walks in and back out again.
    const auto open = openChain(2).minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
    CHECK(open.holeCount() == 0);
    CHECK(open.outer() ==
          PolygonShape({Point(0, 0), Point(9, 0), Point(9, 9), Point(0, 9), Point(0, 2), Point(1, 2),
                   Point(1, 8), Point(8, 8), Point(8, 1), Point(0, 1)}));
    CHECK(open.twiceArea() == 62);
}

TEST_CASE("minkowskiSum: the sweep of a chain is the union of the sweeps of its parts") {
    // `(A₁ ∪ A₂) ⊕ B = (A₁ ⊕ B) ∪ (A₂ ⊕ B)`, checked on the split of a chain at
    // an interior vertex. It says the edge decomposition is not merely one that
    // works but the only one that matters: any coarser or finer split of the same
    // chain has to give the same answer.
    const PolylineShape v = vChain();
    const RectangleShape b(Point(0, 0), Point(2, 1));

    const auto whole = v.minkowskiSum<int>(b);
    const auto left = PolylineShape({Point(0, 0), Point(4, 6)}).minkowskiSum<int>(b);
    const auto right = PolylineShape({Point(4, 6), Point(8, 0)}).minkowskiSum<int>(b);
    const auto joined = left.regularizedUnion<int>(right);
    REQUIRE(joined.componentCount() == 1);
    CHECK(joined.component(0) == whole);

    // Splitting an edge in the middle of its length changes nothing either: the
    // extra vertex adds a piece to the decomposition that covers no new point.
    CHECK(PolylineShape({Point(0, 0), Point(2, 3), Point(4, 6), Point(8, 0)}).minkowskiSum<int>(b) == whole);
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
        const auto sum = PolylineShape({edge.min(), edge.max()}).minkowskiSum<int>(summand);
        CHECK(sum.holeCount() == 0);
        CHECK(sum.outer() == edge.minkowskiSum(summand).asPolygon());
    }

    // A chain whose vertices are collinear covers exactly that segment, however
    // many edges it takes to retrace it, so it must sum alike.
    const Triangle t(Point(0, 0), Point(2, 0), Point(0, 2));
    const auto straight = PolylineShape({Point(0, 0), Point(2, 0), Point(5, 0)}).minkowskiSum<int>(t);
    CHECK(straight.outer() == Segment(Point(0, 0), Point(5, 0)).minkowskiSum(t).asPolygon());
}

TEST_CASE("minkowskiSum: a polygon summand, whose own concavity also strands cavities") {
    // The one non-convex operand a chain takes. A polygon that spells the same
    // point set as a convex operand has to sum alike, whichever type carries it.
    const PolylineShape chain = lChain();
    CHECK(chain.minkowskiSum<int>(box(0, 0, 1, 1)) ==
          chain.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1))));
    CHECK(chain.minkowskiSum<int>(PolygonShape({Point(0, 0), Point(2, 0), Point(0, 2)})) ==
          chain.minkowskiSum<int>(Triangle(Point(0, 0), Point(2, 0), Point(0, 2))));

    // Written the other way round it is the same call: `Polygon` carries the
    // mirror overload, so neither spelling is the privileged one.
    CHECK(box(0, 0, 1, 1).minkowskiSum<int>(chain) == chain.minkowskiSum<int>(box(0, 0, 1, 1)));
    CHECK(uShapePolygon().minkowskiSum<int>(chain) == chain.minkowskiSum<int>(uShapePolygon()));
    CHECK(uShapePolygon().minkowskiSum<pgl::ERational>(chain) ==
          chain.minkowskiSum<pgl::ERational>(uShapePolygon()));

    // A chain with no bend is a segment, so a polygon swept along one must give
    // what the polygon receiver already gives for the degenerate rectangle that
    // covers the same segment -- code that predates the chain operand entirely.
    const PolylineShape vertical({Point(0, 0), Point(0, 3)});
    CHECK(uShapePolygon().minkowskiSum<int>(vertical) ==
          uShapePolygon().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(0, 3))));

    // Both operands non-convex: the chain's turns and the polygon's notch each
    // reach around the other, and the answer still has one hole -- what the U
    // sweeping the square's boundary cannot reach at the centre.
    const auto both = closedChain().minkowskiSum<int>(uShapePolygon());
    CHECK(both.outer() == box(0, 0, 14, 14));
    REQUIRE(both.holeCount() == 1);
    CHECK(both.hole(0) == box(6, 6, 8, 8));
    CHECK(both.twiceArea() == 2 * (196 - 4));

    // A polygon with no area is a segment again: parallel to a straight chain the
    // sum keeps nothing, across it the sweep is a parallelogram.
    const PolygonShape flat({Point(0, 0), Point(4, 0)});
    REQUIRE(flat.isDegenerate());
    CHECK(PolylineShape({Point(0, 0), Point(3, 0)}).minkowskiSum<int>(flat).empty());
    const auto crossed = vertical.minkowskiSum<int>(flat);
    CHECK(crossed.outer() == box(0, 0, 4, 3));

    // Degeneracy does not widen this overload's result type. The same vertical
    // segment spelling that makes the square-boundary sweep split below keeps
    // the first component in canonical order here.
    const auto first = closedChain().minkowskiSum<int>(PolygonShape({Point(0, 0), Point(0, 3)}));
    CHECK(first.outer() == box(0, 0, 8, 3));
    CHECK(first.holeCount() == 0);

    // The empty polygon absorbs from either side.
    CHECK(chain.minkowskiSum<int>(PolygonShape()).empty());
    CHECK(PolygonShape().minkowskiSum<int>(chain).empty());
}

TEST_CASE("minkowskiSum: a region summand keeps its hole and sweeps its slits") {
    const Region a = annulusRegion();
    const PolylineShape unitChain({Point(0, 0), Point(1, 0)});

    // A chain of one horizontal unit covers exactly what the degenerate rectangle
    // `(0,0)--(1,0)` covers, so the answer must be the one the region receiver
    // already gives for that rectangle -- code that predates the chain operand.
    // The cavity is eroded from the left only, as it is there.
    const auto slid = a.minkowskiSum<int>(unitChain);
    CHECK(slid.outer() == box(0, 0, 9, 8));
    REQUIRE(slid.holeCount() == 1);
    CHECK(slid.hole(0) == box(3, 2, 6, 6));
    CHECK(slid == a.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 0))));

    // Either order, and the same in an exact result type.
    CHECK(unitChain.minkowskiSum<int>(a) == slid);
    CHECK(a.minkowskiSum<pgl::ERational>(unitChain) ==
          unitChain.minkowskiSum<pgl::ERational>(a));

    // A closed unit chain erodes the cavity from all four sides at once.
    const auto framed =
        a.minkowskiSum<int>(PolylineShape({Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1), Point(0, 0)}));
    CHECK(framed.outer() == box(0, 0, 9, 9));
    REQUIRE(framed.holeCount() == 1);
    CHECK(framed.hole(0) == box(3, 3, 6, 6));

    // A chain long enough to reach across the cavity closes it entirely.
    const auto closed = a.minkowskiSum<int>(PolylineShape({Point(0, 0), Point(4, 0), Point(4, 4)}));
    CHECK(closed.holeCount() == 0);

    // The slits. Dragged along a diagonal chain, the two stretches the hole shares
    // with the outer ring sweep out two parallelograms at the notch's corner, and
    // those turn the notch into a genuine hole. The slit-free polygon covering the
    // same area has neither the extra material nor the hole, so this is what a
    // decomposition stopping at the triangulated domain would get wrong.
    const PolylineShape diagonal({Point(0, 0), Point(1, 1)});
    const auto swept = slitRegion().minkowskiSum<int>(diagonal);
    REQUIRE(swept.holeCount() == 1);
    CHECK(swept.hole(0) == box(1, 1, 4, 4));
    CHECK(swept.twiceArea() == 142);

    const auto withoutSlits = slitFreeL().minkowskiSum<int>(diagonal);
    CHECK(withoutSlits.holeCount() == 0);
    CHECK(withoutSlits.twiceArea() == 128);
    // The two points the slits alone reach.
    for (const Point& p : {Point(1, 0), Point(0, 1)}) {
        INFO("slit-swept point " << p);
        CHECK(swept.contains(p));
        CHECK_FALSE(withoutSlits.contains(p));
    }

    // A slit whose direction the chain shares sweeps out nothing, and the
    // regularization drops it: the vertical slit `(0,0)--(0,4)` dragged along a
    // vertical chain is a segment, a genuine part of the point set that no region
    // may keep. So (0,2) is in `A ⊕ B` and not in the answer -- the same contract
    // that makes a point summand come back empty.
    const auto vertical = slitRegion().minkowskiSum<int>(PolylineShape({Point(0, 0), Point(0, 1)}));
    CHECK(vertical.holeCount() == 0);
    CHECK(vertical.twiceArea() == 120);
    CHECK(inSumByDefinition(PolylineShape({Point(0, 0), Point(0, 1)}), slitRegion(), Point(0, 2)));
    CHECK_FALSE(inResult(vertical, Point(0, 2)));
    // The horizontal slit is across that chain, so its own sweep survives.
    CHECK(inResult(vertical, Point(2, 0)));

    // The empty region absorbs from either side.
    CHECK(unitChain.minkowskiSum<int>(Region()).empty());
    CHECK(Region().minkowskiSum<int>(unitChain).empty());
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
    CHECK(rectangle.minkowskiSum<int>(a) == a.minkowskiSum<int>(rectangle));
    CHECK(triangle.minkowskiSum<int>(a) == a.minkowskiSum<int>(triangle));
    CHECK(convex.minkowskiSum<int>(a) == a.minkowskiSum<int>(convex));
    CHECK(triangle.minkowskiSum<pgl::ERational>(a) == a.minkowskiSum<pgl::ERational>(triangle));

    // A rectangle, the convex square and the same square as a `Convex` are one
    // point set, so they sum alike whichever type carries them.
    CHECK(a.minkowskiSum<int>(rectangle) == a.minkowskiSum<int>(convex));

    // `(A + t) ⊕ B = (A ⊕ B) + t`, which also exercises the canonicalization
    // every returned ring goes through.
    const Point t(-5, 3);
    PolylineShape shifted = a;
    shifted += t;
    const auto direct = shifted.minkowskiSum<int>(triangle);
    auto moved = a.minkowskiSum<int>(triangle);
    moved += t;
    CHECK(direct == moved);

    // A polyline traversed backwards is the same point set, and compares equal;
    // its sum must agree too.
    const PolylineShape reversed({Point(6, 6), Point(6, 0), Point(0, 0), Point(0, 6)});
    REQUIRE(reversed == a);
    CHECK(reversed.minkowskiSum<int>(triangle) == a.minkowskiSum<int>(triangle));
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

    const auto swept = square.minkowskiSum<int>(slant);
    REQUIRE(swept.componentCount() == 1);
    CHECK(swept.component(0).outer() == PolygonShape({Point(0, 0), Point(8, 0), Point(10, 1), Point(10, 9),
                                            Point(2, 9), Point(0, 8)}));
    REQUIRE(swept.component(0).holeCount() == 1);
    // What the band leaves uncovered: `[0,8]² ∩ ([0,8]² + (2,1))`.
    CHECK(swept.component(0).hole(0) == box(2, 1, 8, 8));

    // Orientation is not part of a point set, and the same two points spelled as
    // a flat rectangle, convex or polygon answer alike -- through the
    // region-valued overload, which those types reach even collapsed, and which
    // is contracted only because this particular sum does not split.
    CHECK(square.minkowskiSum<int>(OrientedSegment(Point(2, 1), Point(0, 0))) == swept);
    CHECK(square.minkowskiSum<int>(Convex(std::vector<Point>{Point(0, 0), Point(2, 1)})) ==
          swept.component(0));
    CHECK(square.minkowskiSum<int>(PolygonShape({Point(0, 0), Point(2, 1)})) == swept.component(0));

    // Written on the left, a segment forwards to the chain, as every other
    // bounded operand does.
    CHECK(slant.minkowskiSum<int>(square) == swept);
    CHECK(OrientedSegment(Point(0, 0), Point(2, 1)).minkowskiSum<int>(square) == swept);
    CHECK(slant.minkowskiSum<pgl::ERational>(square) ==
          square.minkowskiSum<pgl::ERational>(slant));

    // The regularization is easiest to trip over here, and it takes nothing
    // exotic: an axis-parallel segment along the square's own edges leaves the
    // two edges it is parallel to sweeping nothing at all, so what survives is
    // the two bands the other pair sweeps -- and they do not touch. The sum of
    // two connected shapes is connected; `closure((A ⊕ B)°)` need not be.
    const auto split = square.minkowskiSum<int>(Segment(Point(0, 0), Point(0, 3)));
    REQUIRE(split.componentCount() == 2);
    CHECK(split.component(0).outer() == box(0, 0, 8, 3));
    CHECK(split.component(1).outer() == box(0, 8, 8, 11));
    CHECK(split.component(0).holeCount() == 0);
    CHECK(split.component(1).holeCount() == 0);
    // The vertical walls' sweep is part of the point set and not of the answer.
    CHECK(inSumByDefinition(square, Segment(Point(0, 0), Point(0, 3)), Point(0, 5)));
    CHECK_FALSE(inResult(split, Point(0, 5)));

    // An L bent at a right angle keeps only the arm across the segment.
    CHECK(lChain().minkowskiSum<int>(Segment(Point(0, 0), Point(0, 2))).component(0).outer() ==
          box(0, 0, 6, 2));

    // A summand collapsed to a point leaves nothing at all: a chain has no area
    // to translate, so this is empty rather than the moved chain.
    CHECK(square.minkowskiSum<int>(Segment(Point(3, 3), Point(3, 3))).empty());

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
    CHECK(v.minkowskiSum<int>(RectangleShape(Point(3, 3), Point(3, 3))).empty());
    CHECK(v.minkowskiSum<int>(Triangle(Point(1, 1), Point(1, 1), Point(1, 1))).empty());

    // A flat summand parallel to a straight chain is the same story; across it,
    // the sweep is a genuine parallelogram.
    const PolylineShape straight({Point(0, 0), Point(4, 0)});
    CHECK(straight.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(2, 0))).empty());
    const auto crossed = straight.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(0, 3)));
    CHECK(crossed.outer() == box(0, 0, 4, 3));

    // A chain of one vertex is that point, whose sum is the summand translated
    // there -- the one decomposition that is a lone point rather than an edge.
    const auto placed =
        PolylineShape({Point(2, 3)}).minkowskiSum<int>(Triangle(Point(0, 0), Point(2, 0), Point(0, 2)));
    CHECK(placed.outer() == PolygonShape({Point(2, 3), Point(4, 3), Point(2, 5)}));
    CHECK(placed.holeCount() == 0);

    // A degenerate chain with several coincident vertices is the same point.
    CHECK(PolylineShape({Point(2, 3), Point(2, 3), Point(2, 3)})
              .minkowskiSum<int>(Triangle(Point(0, 0), Point(2, 0), Point(0, 2))) == placed);

    // A zero-length edge inside a chain contributes nothing but is not an error.
    const auto repeated = PolylineShape({Point(0, 0), Point(0, 0), Point(4, 0)})
                              .minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
    CHECK(repeated.outer() == box(0, 0, 5, 1));

    // The empty operands absorb, whichever side they are on.
    CHECK(PolylineShape().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(2, 2))).empty());
    CHECK(v.minkowskiSum<int>(Convex()).empty());
    CHECK(Convex().minkowskiSum<int>(v).empty());
}

TEST_CASE("minkowskiSum: exact over rational coordinates") {
    const EPolyline chain({EPoint(0, 0), EPoint(8, 0), EPoint(8, 8), EPoint(0, 8), EPoint(0, 0)});
    const pgl::Rectangle<EPoint> unit(EPoint(0, 0), EPoint(1, 1));

    const auto sum = chain.minkowskiSum<pgl::ERational>(unit);
    CHECK(sum.outer() ==
          pgl::Polygon<EPoint>({EPoint(0, 0), EPoint(9, 0), EPoint(9, 9), EPoint(0, 9)}));
    REQUIRE(sum.holeCount() == 1);
    CHECK(sum.hole(0) ==
          pgl::Polygon<EPoint>({EPoint(1, 1), EPoint(8, 1), EPoint(8, 8), EPoint(1, 8)}));

    // Half-integral vertices are as exact as integral ones, and the answer's
    // coordinates are just the sums of the operands'.
    const EPolyline half({EPoint(pgl::ERational(1, 2), 0), EPoint(pgl::ERational(1, 2), 4)});
    const auto swept = half.minkowskiSum<pgl::ERational>(unit);
    CHECK(swept.outer() == pgl::Polygon<EPoint>({EPoint(pgl::ERational(1, 2), 0),
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
    const auto& ring = exact.outer().vertices();
    const pgl::Point<pgl::ERational> tip(pgl::ERational(11, 2), pgl::ERational(15, 4));
    CHECK(std::find(ring.begin(), ring.end(), tip) != ring.end());
    CHECK(exact.twiceArea() == pgl::ERational(393, 4));

    // Requesting `int` explicitly rounds that vertex to (5,3) and reports as
    // covered a point the definition says is not: the truncated notch is
    // shallower than the true one.
    const auto truncated = v.minkowskiSum<int>(t);
    CHECK(truncated.twiceArea() == 102);
    const pgl::Point<pgl::ERational> outside(pgl::ERational(26, 5), pgl::ERational(31, 10));
    CHECK_FALSE(exact.contains(outside));
    CHECK(truncated.contains(outside));
}

TEST_CASE("minkowskiSum: two chains, with no area between them") {
    // Both operands decompose into their edges, and each pair of edges spans a
    // parallelogram unless the two are parallel. A segment operand is the
    // one-edge case of this, and answers the same way.
    const PolylineShape l = lChain();
    const Segment slant(Point(0, 0), Point(2, 1));
    CHECK(l.minkowskiSum<int>(PolylineShape({Point(0, 0), Point(2, 1)})) ==
          l.minkowskiSum<int>(slant));

    // A chain summed with a chain sweeps each edge of one along the whole of the
    // other, so the two spellings of the pair agree.
    const PolylineShape v = vChain();
    CHECK(l.minkowskiSum<int>(v) == v.minkowskiSum<int>(l));

    // The union identity holds over the operand's edges as it does over the
    // receiver's: `A ⊕ (B₁ ∪ B₂) = (A ⊕ B₁) ∪ (A ⊕ B₂)`.
    const auto whole = l.minkowskiSum<int>(v);
    const auto left = l.minkowskiSum<int>(PolylineShape({Point(0, 0), Point(4, 6)}));
    const auto right = l.minkowskiSum<int>(PolylineShape({Point(4, 6), Point(8, 0)}));
    CHECK(left.regularizedUnion<int>(right) == whole);

    // Nothing here is a body, so nothing is contracted to stay in one piece: two
    // parallel chains sweep out nothing at all.
    CHECK(PolylineShape({Point(0, 0), Point(4, 0)})
              .minkowskiSum<int>(PolylineShape({Point(0, 0), Point(2, 0)}))
              .empty());

    // A monotone chain is a polyline that happens to be sorted, and sums as one
    // from either side — the chain's forwarder reaches the polyline's overload.
    const pgl::MonotoneChain<Point> sorted({Point(0, 0), Point(4, 6), Point(8, 0)});
    CHECK(l.minkowskiSum<int>(sorted) == l.minkowskiSum<int>(sorted.asPolyline()));
    CHECK(sorted.minkowskiSum<int>(l) == l.minkowskiSum<int>(sorted));

    SUBCASE("against the definition, over a probe grid") {
        for (const PolylineShape& b : {vChain(), closedChain(),
                                       PolylineShape({Point(0, 0), Point(1, 3)})}) {
            checkAgainstDefinition(lChain(), b, -6, 18);
        }
    }
}

TEST_CASE("minkowskiSum: the pairs a chain accepts") {
    // The receiver has no area, so the operands are exactly the shapes that have
    // some -- and having some is what makes the sum a single region rather than a
    // set of them. `MinkowskiSummableConcept` still rejects every one of those
    // pairs -- it gates the sums that fit in a single shape -- and a `Point`
    // operand still goes through it, giving back a translated `Polyline`.
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const RectangleShape&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const Triangle&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const Convex&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const PolygonShape&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const PolygonShape&>().minkowskiSum<int>(
                                    std::declval<const PolylineShape&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const Region&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const Region&>().minkowskiSum<int>(
                                    std::declval<const PolylineShape&>())),
                                Region>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>()
                                              .minkowskiSum<pgl::ERational>(
                                                  std::declval<const Convex&>())),
                                pgl::PolygonWithHoles<EPoint>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolygonShape&>()
                                              .minkowskiSum<pgl::ERational>(
                                                  std::declval<const PolylineShape&>())),
                                pgl::PolygonWithHoles<EPoint>>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum(
                                    std::declval<const Point&>())),
                                PolylineShape>);
    static_assert(!pgl::MinkowskiSummableConcept<PolylineShape, PolygonShape>);

    // A `Segment` and an `OrientedSegment` join them: neither brings area, and
    // the band each edge of the chain sweeps along one has some. Both spellings
    // work, since the two segment types forward to the chain. This is the pair
    // with no body on either side, and the only one of the set that stays
    // set-valued: the bands an edge parallel to the summand fails to sweep can
    // leave the answer in pieces for operands that are in no way degenerate.
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const Segment&>())),
                                RegionSet>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const OrientedSegment&>())),
                                RegionSet>);
    static_assert(summable<PolylineShape, OrientedSegment>);
    static_assert(summable<Segment, PolylineShape>);
    static_assert(summable<OrientedSegment, PolylineShape>);

    // A second chain is an operand too, and so is a monotone chain: both sides
    // contribute their edges, and neither brings a body, so the pair keeps the
    // set-valued contract the segment operands have. `operator+` stays out of
    // the region-valued case entirely.
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const PolylineShape&>())),
                                RegionSet>);
    static_assert(std::is_same_v<decltype(std::declval<const PolylineShape&>().minkowskiSum<int>(
                                    std::declval<const pgl::MonotoneChain<Point>&>())),
                                RegionSet>);
    static_assert(summable<pgl::MonotoneChain<Point>, PolylineShape>);
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
            const auto perEdge = pgl::detail::decomposedMinkowskiSum<EPoint>(chain, summand);
            REQUIRE(perEdge.componentCount() == 1);
            CHECK(boundary == perEdge.component(0));
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
    const auto zigzagRef = pgl::detail::decomposedMinkowskiSum<EPoint>(zigzag, summands[0]);
    REQUIRE(zigzagRef.componentCount() == 1);
    CHECK(zigzagSum == zigzagRef.component(0));
}
