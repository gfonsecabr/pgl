#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

using EPoint = pgl::EPoint;
using EPolygon = pgl::EPolygon;
using ERegion = pgl::EPolygonWithHoles;

// The Minkowski sum of a *non-convex* operand, the gap `PolygonWithHoles` was
// proposed to close: `doc/raw/shape_methods.md` said outright that such a sum
// "can be a region with holes ... none of those is representable today".
//
// The construction is the identity `A ⊕ B = ⋃ᵢⱼ (Aᵢ ⊕ Bⱼ)` over a convex
// decomposition of each operand -- the triangles of its triangulated domain,
// plus whatever it holds that has no area beside it -- with the existing linear
// convex merge doing every piece sum and one regularized union assembling them.
//
// Two things separate it from the shape-valued `minkowskiSum`: the result is a
// *set* of regions (the sum of two connected shapes is connected, so this is one
// region unless its boundary pinches shut, which no single region may do), and
// it is *regularized*, `closure((A ⊕ B)°)`.

// -----------------------------------------------------------------------------
// Fixtures.

static PolygonShape box(int x0, int y0, int x1, int y1) {
    return PolygonShape({Point(x0, y0), Point(x1, y0), Point(x1, y1), Point(x0, y1)});
}

// A U opening upward: the square [0,6]² with the notch (2,4)×(2,6] cut out.
static PolygonShape uShape() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 6), Point(4, 6), Point(4, 2), Point(2, 2),
                    Point(2, 6), Point(0, 6)});
}

// A C: the square annulus [0,8]² ∖ (2,6)², cut open through the right wall over
// y ∈ [3,5]. Simply connected, and the one fixture whose sum grows a hole out of
// nothing once the cut is plugged.
static PolygonShape cShape() {
    return PolygonShape({Point(0, 0), Point(8, 0), Point(8, 3), Point(6, 3), Point(6, 2), Point(2, 2),
                    Point(2, 6), Point(6, 6), Point(6, 5), Point(8, 5), Point(8, 8), Point(0, 8)});
}

// An L.
static PolygonShape lShape() {
    return PolygonShape({Point(0, 0), Point(6, 0), Point(6, 2), Point(2, 2), Point(2, 6), Point(0, 6)});
}

// The square annulus, as a region.
static Region annulus() {
    return Region(box(0, 0, 8, 8), std::vector<PolygonShape>{box(2, 2, 6, 6)});
}

// The L-shape written as a region whose hole shares two edges with the outer
// ring: `[0,8]² ∖ (0,4)²`. The two shared stretches are slits -- region material
// with no area beside it -- and they sweep out area in a sum like anything else.
static Region slitRegion() {
    return Region(box(0, 0, 8, 8), std::vector<PolygonShape>{box(0, 0, 4, 4)});
}

// -----------------------------------------------------------------------------
// Oracles.

// The definition of the sum, read as a query: `p ∈ A ⊕ B` exactly when `A` meets
// the reflected copy of `B` placed at `p`, since `p − B` is `(−B) + p` and a
// 180° rotation about the origin *is* the negation. This uses nothing from the
// construction under test -- only `rotated90`, translation and `intersects`,
// all of them separately tested.
//
// It settles every point, boundary included, provided at least one operand is
// the closure of its own interior and has area: `X ⊕ B` is then regular closed,
// so the regularization the construction applies drops nothing.
template <class ShapeA, class ShapeB>
static bool inSumByDefinition(const ShapeA& a, const ShapeB& b, const Point& p) {
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

// Probes every integer point of a box comfortably holding the sum.
//
// The result type is exact on purpose. Two slanted operands can put a vertex of
// the sum at a crossing with no integral coordinate -- `uShape() ⊕
// Triangle((-2,-1),(2,0),(0,2))` has its notch tip at `(16/5, 34/5)` -- and the
// default `ResultNumber` is the operands' own, which truncates it. That is the
// documented contract rather than a defect, but an oracle checking the true
// point set has to ask for the true point set.
template <class ShapeA, class ShapeB>
static void checkAgainstDefinition(const ShapeA& a, const ShapeB& b, int lo, int hi) {
    const auto sum = a.template minkowskiSum<pgl::ERational>(b);
    for (int x = lo; x <= hi; ++x) {
        for (int y = lo; y <= hi; ++y) {
            const Point p(x, y);
            INFO("probe " << p);
            CHECK(inResult(sum, p) == inSumByDefinition(a, b, p));
        }
    }
    // The pieces tile the sum: their interiors are pairwise disjoint, so the
    // areas add up rather than double-counting.
    for (std::size_t i = 0; i < sum.size(); ++i) {
        REQUIRE(sum[i].isValid());
        for (std::size_t j = i + 1; j < sum.size(); ++j) {
            REQUIRE_FALSE(sum[i].interiorsIntersect(sum[j]));
        }
    }
}

// The shear `(x,y) ↦ (x, y + kx)`, a unimodular *linear* map. The Minkowski sum
// commutes with every linear map, so this carries the exact rectilinear answers
// onto slanted edges and off-lattice crossings.
template <pgl::PointConcept PointT>
static PointT sheared(const PointT& p, int k) {
    using Number = typename PointT::NumberType;
    return PointT(p.x(), p.y() + Number(k) * p.x());
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

// The image of a whole answer under the shear, for comparing against the answer
// computed from the sheared operands.
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

TEST_CASE("minkowskiSum: a plugged cut grows a hole out of nothing") {
    const PolygonShape c = cShape();
    REQUIRE(c.twiceArea() == 88);

    // The cut through the right wall is two units wide, so a 2×2 square exactly
    // closes it and strands the cavity. Neither operand has a hole; the sum
    // does, and no `vector<Polygon>` could say which ring is which.
    const auto plugged = c.minkowskiSum(RectangleShape(Point(0, 0), Point(2, 2)));
    REQUIRE(plugged.size() == 1);
    CHECK(plugged[0].outer() == box(0, 0, 10, 10));
    REQUIRE(plugged[0].holeCount() == 1);
    CHECK(plugged[0].hole(0) == box(4, 4, 6, 6));
    CHECK(plugged[0].twiceArea() == 2 * (100 - 4));

    // One unit less and the cut survives, so the sum is simply connected.
    const auto open = c.minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(open.size() == 1);
    CHECK(open[0].holeCount() == 0);
}

TEST_CASE("minkowskiSum: a notch fills in without leaving a hole") {
    // The U's notch is two units wide and open at the top, so a summand that
    // spans it sweeps the arm across the whole notch rather than capping it.
    const auto sum = uShape().minkowskiSum(RectangleShape(Point(0, 0), Point(2, 1)));
    REQUIRE(sum.size() == 1);
    CHECK(sum[0].outer() == box(0, 0, 8, 7));
    CHECK(sum[0].holeCount() == 0);
}

TEST_CASE("minkowskiSum: a region operand keeps its hole, shrunken") {
    const Region a = annulus();

    // Sliding the annulus right by one erodes the cavity from the left only.
    const auto slid = a.minkowskiSum(RectangleShape(Point(0, 0), Point(1, 0)));
    REQUIRE(slid.size() == 1);
    CHECK(slid[0].outer() == box(0, 0, 9, 8));
    REQUIRE(slid[0].holeCount() == 1);
    CHECK(slid[0].hole(0) == box(3, 2, 6, 6));

    // A 2×2 square erodes it from every side at once.
    const auto grown = a.minkowskiSum(RectangleShape(Point(0, 0), Point(2, 2)));
    REQUIRE(grown.size() == 1);
    CHECK(grown[0].outer() == box(0, 0, 10, 10));
    REQUIRE(grown[0].holeCount() == 1);
    CHECK(grown[0].hole(0) == box(4, 4, 6, 6));

    // A summand as wide as the cavity closes it entirely.
    const auto closed = a.minkowskiSum(RectangleShape(Point(0, 0), Point(4, 4)));
    REQUIRE(closed.size() == 1);
    CHECK(closed[0].holeCount() == 0);
    CHECK(closed[0].outer() == box(0, 0, 12, 12));
}

TEST_CASE("minkowskiSum: a slit sweeps out area like anything else") {
    // `[0,8]² ∖ (0,4)²` holds the two stretches its hole shares with the outer
    // ring. They carry no area, so the triangulated domain does not reach them,
    // and a decomposition that stopped at the triangles would lose the material
    // they sweep out.
    const Region slitted = slitRegion();
    REQUIRE(slitted.isValid());
    REQUIRE(slitted.twiceArea() == 2 * (64 - 16));

    // The two slits sum to `[0,1]×[0,5]` and `[0,5]×[0,1]`, which run down two
    // sides of the notch the material leaves. That turns the notch into a
    // genuine hole: without the slits the answer would be a plain L, and with
    // them the uncovered part is the open square `(1,4)²`, walled in on all four
    // sides. A decomposition that stopped at the triangulated domain would get
    // both the area and the topology wrong.
    const auto sum = slitted.minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(sum.size() == 1);
    CHECK(sum[0].outer() == box(0, 0, 9, 9));
    REQUIRE(sum[0].holeCount() == 1);
    CHECK(sum[0].hole(0) == box(1, 1, 4, 4));

    CHECK(inResult(sum, Point(0, 4)));
    CHECK(inResult(sum, Point(1, 1)));
    // Two units in is past the reach of the summand, so it stays hole.
    CHECK_FALSE(inResult(sum, Point(2, 2)));
}

TEST_CASE("minkowskiSum: a segment operand, the thinnest one that still sweeps") {
    // A `Segment` has no area, and the sum of one with either receiver has some:
    // every piece of the receiver's decomposition is swept into a band. That is
    // enough to grow a hole -- the C's cut is two units tall, so dragging it two
    // units upward seals the cut and strands the cavity behind it.
    const PolygonShape c = cShape();
    const Segment up(Point(0, 0), Point(0, 2));

    const auto plugged = c.minkowskiSum(up);
    REQUIRE(plugged.size() == 1);
    CHECK(plugged[0].outer() == box(0, 0, 8, 10));
    REQUIRE(plugged[0].holeCount() == 1);
    // What is left of the annulus' cavity `(2,6)²` after the sweep erodes it
    // from below: the cut closes at the same moment the cavity stops shrinking.
    CHECK(plugged[0].hole(0) == box(2, 4, 6, 6));

    // The same point set spelled as a flat rectangle or a flat polygon answers
    // alike -- the segment is only the tightest type for it.
    CHECK(c.minkowskiSum(RectangleShape(Point(0, 0), Point(0, 2))) == plugged);
    CHECK(c.minkowskiSum(PolygonShape({Point(0, 0), Point(0, 2)})) == plugged);

    // An orientation is not part of a point set, so both directions agree with
    // the unoriented segment.
    CHECK(c.minkowskiSum(OrientedSegment(Point(0, 0), Point(0, 2))) == plugged);
    CHECK(c.minkowskiSum(OrientedSegment(Point(0, 2), Point(0, 0))) == plugged);

    // And it is the pair that decides, not the receiver: a segment written on
    // the left forwards, as every other bounded operand does.
    CHECK(up.minkowskiSum(c) == plugged);
    CHECK(OrientedSegment(Point(0, 0), Point(0, 2)).minkowskiSum(c) == plugged);
    CHECK(up.minkowskiSum<pgl::ERational>(c) == c.minkowskiSum<pgl::ERational>(up));

    // A region receiver behaves as it does with any other summand: the outer
    // ring grows by the sweep and the hole is eroded from the one side.
    const auto slid = annulus().minkowskiSum(Segment(Point(0, 0), Point(0, 3)));
    REQUIRE(slid.size() == 1);
    CHECK(slid[0].outer() == box(0, 0, 8, 11));
    REQUIRE(slid[0].holeCount() == 1);
    CHECK(slid[0].hole(0) == box(2, 5, 6, 6));

    // A summand that has collapsed to a point sweeps nothing, and the receiver
    // has area of its own, so the answer is the receiver moved there.
    const auto translated = c.minkowskiSum(Segment(Point(3, 1), Point(3, 1)));
    REQUIRE(translated.size() == 1);
    PolygonShape moved = c;
    moved += Point(3, 1);
    CHECK(translated[0].outer() == moved);
    CHECK(annulus().minkowskiSum(Segment(Point(0, 0), Point(0, 0)))[0] == annulus());

    // The empty shape still absorbs, from either side.
    CHECK(PolygonShape().minkowskiSum(up).empty());
    CHECK(up.minkowskiSum(PolygonShape()).empty());
}

TEST_CASE("minkowskiSum: a slit swept along its own direction is regularized away") {
    // The sharpest form of the regularization contract, and it needs nothing
    // exotic to reach: a rectilinear region and an axis-parallel segment.
    //
    // `[0,8]² ∖ (0,4)²` carries slits along the bottom (`(0,0)`--`(4,0)`) and
    // along the left wall (`(0,0)`--`(0,4)`). Swept two units to the right, the
    // bottom slit stays a segment -- part of `A ⊕ B` that no region may keep --
    // while the left one sweeps `[0,2]×[0,4]` and survives.
    const Region slitted = slitRegion();
    const auto swept = slitted.minkowskiSum(Segment(Point(0, 0), Point(2, 0)));
    REQUIRE(swept.size() == 1);
    CHECK(swept[0].holeCount() == 0);
    // The L's body swept right, plus the left slit's band, with the gap between
    // them left open: the bottom slit is what would have filled it.
    CHECK(swept[0].outer() == PolygonShape({Point(0, 0), Point(2, 0), Point(2, 4), Point(4, 4),
                                            Point(4, 0), Point(10, 0), Point(10, 8), Point(0, 8)}));
    CHECK(inResult(swept, Point(1, 2)));
    CHECK_FALSE(inResult(swept, Point(3, 2)));
    // The point set does hold it; `closure((A ⊕ B)°)` is what does not.
    CHECK(inSumByDefinition(slitted, Segment(Point(0, 0), Point(2, 0)), Point(3, 0)));

    // Turned across the slits instead, both of them sweep out area.
    const auto up = slitted.minkowskiSum(Segment(Point(0, 0), Point(0, 2)));
    REQUIRE(up.size() == 1);
    CHECK(up[0].outer() == PolygonShape({Point(0, 0), Point(8, 0), Point(8, 10), Point(0, 10),
                                         Point(0, 4), Point(4, 4), Point(4, 2), Point(0, 2)}));
}

TEST_CASE("minkowskiSum: agrees with the definition over a probe grid") {
    // The strong oracle: `p ∈ A ⊕ B ⟺ A ∩ (p − B) ≠ ∅`, answered by the
    // library's own `intersects` on a reflected, translated copy of `B`.
    const std::vector<RectangleShape> rectangles{
        RectangleShape(Point(0, 0), Point(2, 1)), RectangleShape(Point(0, 0), Point(2, 2)),
        RectangleShape(Point(-1, -1), Point(1, 1)), RectangleShape(Point(0, 0), Point(1, 3))};
    const std::vector<Triangle> triangles{Triangle(Point(0, 0), Point(3, 0), Point(0, 3)),
                                          Triangle(Point(0, 0), Point(2, 0), Point(1, 3)),
                                          Triangle(Point(-2, -1), Point(2, 0), Point(0, 2))};

    for (const PolygonShape& a : {uShape(), cShape(), lShape()}) {
        for (const RectangleShape& b : rectangles) {
            checkAgainstDefinition(a, b, -4, 13);
        }
        for (const Triangle& b : triangles) {
            checkAgainstDefinition(a, b, -4, 13);
        }
    }
    checkAgainstDefinition(lShape(), lShape(), -2, 14);
    checkAgainstDefinition(uShape(), lShape(), -2, 14);
    checkAgainstDefinition(uShape(), cShape(), -2, 16);

    // A `Segment` summand carries no area into the sum, and the receiver is
    // still the closure of its own interior, so the oracle settles it the same
    // way. The last one is slanted, which puts the piece boundaries in general
    // position rather than on the receiver's own directions.
    const std::vector<Segment> segments{Segment(Point(0, 0), Point(0, 2)),
                                        Segment(Point(0, 0), Point(3, 0)),
                                        Segment(Point(-1, -1), Point(2, 1))};
    for (const PolygonShape& a : {uShape(), cShape(), lShape()}) {
        for (const Segment& b : segments) {
            checkAgainstDefinition(a, b, -4, 13);
            checkAgainstDefinition(a, OrientedSegment(b.max(), b.min()), -4, 13);
        }
    }

    // A region operand: the holes need no special handling -- they are simply
    // where the decomposition has no piece.
    for (const RectangleShape& b : rectangles) {
        checkAgainstDefinition(annulus(), b, -4, 13);
        // A slitted operand is not the closure of its own interior, but the
        // summand is, which is all the oracle needs for the sum to be regular.
        checkAgainstDefinition(slitRegion(), b, -4, 13);
    }
    for (const Triangle& b : triangles) {
        checkAgainstDefinition(annulus(), b, -4, 13);
        checkAgainstDefinition(slitRegion(), b, -4, 13);
    }
    for (const Segment& b : segments) {
        // The annulus is the closure of its own interior; `slitRegion()` is not,
        // and against a summand with no area either the sum genuinely is not
        // regular closed -- that pair is the test case above, not this oracle.
        checkAgainstDefinition(annulus(), b, -4, 13);
    }
    checkAgainstDefinition(annulus(), lShape(), -4, 17);
    checkAgainstDefinition(slitRegion(), annulus(), -4, 19);
}

TEST_CASE("minkowskiSum: commutes, and translates with its operands") {
    const PolygonShape a = uShape();
    const PolygonShape b = lShape();

    CHECK(a.minkowskiSum(b) == b.minkowskiSum(a));
    CHECK(cShape().minkowskiSum(a) == a.minkowskiSum(cShape()));

    // `(A + t) ⊕ B = (A ⊕ B) + t`, which also exercises the canonicalization
    // every returned ring goes through.
    const Point t(-5, 3);
    PolygonShape shifted = a;
    shifted += t;
    const auto direct = shifted.minkowskiSum(b);
    auto moved = a.minkowskiSum(b);
    REQUIRE(direct.size() == moved.size());
    for (auto& piece : moved) {
        piece += t;
    }
    std::sort(moved.begin(), moved.end());
    CHECK(direct == moved);
}

TEST_CASE("minkowskiSum: a convex receiver answers as the convex merge does") {
    // Grounding the whole construction in the tested shape-valued sum: where the
    // operands are convex, the region-valued answer must be the single convex
    // shape the linear merge produces.
    const std::vector<PolygonShape> convexPolygons{
        box(0, 0, 4, 3), PolygonShape({Point(0, 0), Point(5, 0), Point(2, 4)}),
        PolygonShape({Point(0, 0), Point(4, 0), Point(5, 3), Point(3, 5), Point(0, 4)})};
    const std::vector<Convex> summands{
        Convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(1, 2)}),
        Convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)}),
        Convex(std::vector<Point>{Point(-1, -1), Point(1, -1), Point(1, 1), Point(-1, 1)})};

    for (const PolygonShape& polygon : convexPolygons) {
        const Convex hull(polygon.vertices());
        for (const Convex& summand : summands) {
            const auto sum = polygon.minkowskiSum(summand);
            REQUIRE(sum.size() == 1);
            CHECK(sum[0].holeCount() == 0);
            CHECK(sum[0].outer() == hull.minkowskiSum(summand).asPolygon());
        }
    }
}

TEST_CASE("minkowskiSum: shear invariance carries the answers off the axes") {
    // The sum commutes with every linear map, and an integer shear is a
    // bijection of the lattice, so the sheared answer must be *equal as a set of
    // regions* to the answer for the sheared operands.
    const std::vector<std::pair<PolygonShape, PolygonShape>> pairs{
        {uShape(), lShape()},
        {cShape(), box(0, 0, 2, 2)},
        {lShape(), PolygonShape({Point(0, 0), Point(3, 0), Point(0, 3)})}};

    // Exact throughout: a sheared crossing need not be integral, and truncation
    // does not commute with the shear.
    for (int k : {1, -2}) {
        for (const auto& [a, b] : pairs) {
            CHECK(sheared(a, k).template minkowskiSum<pgl::ERational>(sheared(b, k)) ==
                  sheared(a.template minkowskiSum<pgl::ERational>(b), k));
        }

        // The same for a region operand, whose slits the shear carries along.
        const Region slitted = slitRegion();
        const PolygonShape unit = box(0, 0, 1, 1);
        CHECK(sheared(slitted, k).template minkowskiSum<pgl::ERational>(sheared(unit, k)) ==
              sheared(slitted.template minkowskiSum<pgl::ERational>(unit), k));
    }
}

TEST_CASE("minkowskiSum: degenerate operands") {
    const PolygonShape u = uShape();

    // A summand collapsed to a single point is a translation, and the answer is
    // the regularized operand moved there.
    const auto translated = u.minkowskiSum(RectangleShape(Point(3, -1), Point(3, -1)));
    REQUIRE(translated.size() == 1);
    CHECK(translated[0].holeCount() == 0);
    PolygonShape expected = u;
    expected += Point(3, -1);
    CHECK(translated[0].outer() == expected);

    // A summand with no area but some length still sweeps out a region: the U
    // dragged three units upward keeps its notch, three units shallower.
    const auto swept = u.minkowskiSum(RectangleShape(Point(0, 0), Point(0, 3)));
    REQUIRE(swept.size() == 1);
    CHECK(swept[0].outer() == PolygonShape({Point(0, 0), Point(6, 0), Point(6, 9), Point(4, 9),
                                       Point(4, 5), Point(2, 5), Point(2, 9), Point(0, 9)}));

    // Two operands that are both flat sum to something flat, which regularizes
    // away -- unless they point in different directions, in which case the sum
    // is a genuine parallelogram.
    const PolygonShape flat({Point(0, 0), Point(4, 0)});
    REQUIRE(flat.isDegenerate());
    CHECK(flat.minkowskiSum(PolygonShape({Point(0, 0), Point(2, 0)})).empty());
    const auto parallelogram = flat.minkowskiSum(PolygonShape({Point(0, 0), Point(0, 3)}));
    REQUIRE(parallelogram.size() == 1);
    CHECK(parallelogram[0].outer() == box(0, 0, 4, 3));

    // A flat operand against one with area is still the honest sweep.
    const auto sweptC = cShape().minkowskiSum(flat);
    REQUIRE(sweptC.size() == 1);
    CHECK(sweptC[0].holeCount() == 0);
    for (int x = -2; x <= 14; ++x) {
        for (int y = -2; y <= 10; ++y) {
            CHECK(inResult(sweptC, Point(x, y)) == inSumByDefinition(cShape(), flat, Point(x, y)));
        }
    }

    // The empty shapes absorb, whichever type carries them and whichever side
    // they are on.
    CHECK(u.minkowskiSum(PolygonShape()).empty());
    CHECK(PolygonShape().minkowskiSum(u).empty());
    CHECK(u.minkowskiSum(Convex()).empty());
    CHECK(u.minkowskiSum(Region()).empty());
    CHECK(Region().minkowskiSum(u).empty());
}

TEST_CASE("minkowskiSum: every operand type, on both receivers") {
    const PolygonShape u = uShape();
    const Region a = annulus();
    const RectangleShape rectangle(Point(0, 0), Point(2, 2));
    const Triangle triangle(Point(0, 0), Point(2, 0), Point(0, 2));
    const Convex convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)});
    const PolygonShape polygon = box(0, 0, 2, 2);

    // A rectangle, a convex square and the polygon that spells it are the same
    // point set, so they must sum alike whichever type carries them.
    CHECK(u.minkowskiSum(rectangle) == u.minkowskiSum(convex));
    CHECK(u.minkowskiSum(rectangle) == u.minkowskiSum(polygon));
    CHECK(a.minkowskiSum(rectangle) == a.minkowskiSum(convex));
    CHECK(a.minkowskiSum(rectangle) == a.minkowskiSum(polygon));
    CHECK_FALSE(u.minkowskiSum(triangle).empty());

    // A slanted segment and the four other types that spell the same two-point
    // set: the tightest one is only a spelling.
    const Segment segment(Point(0, 0), Point(2, 2));
    const Convex flatConvex(std::vector<Point>{Point(0, 0), Point(2, 2)});
    for (const auto& receiver : {Region(u), a}) {
        CHECK(receiver.minkowskiSum(segment) ==
              receiver.minkowskiSum(OrientedSegment(Point(2, 2), Point(0, 0))));
        CHECK(receiver.minkowskiSum(segment) == receiver.minkowskiSum(flatConvex));
        CHECK(receiver.minkowskiSum(segment) ==
              receiver.minkowskiSum(PolygonShape({Point(0, 0), Point(2, 2)})));
        CHECK_FALSE(receiver.minkowskiSum(segment).empty());
    }

    // Region against region, and a region against the polygon that spells it.
    CHECK(u.minkowskiSum(a) == a.minkowskiSum(u));
    CHECK(Region(u).minkowskiSum(polygon) == u.minkowskiSum(polygon));
    const auto both = a.minkowskiSum(annulus());
    REQUIRE(both.size() == 1);
    CHECK(both[0].outer() == box(0, 0, 16, 16));
    // Each cavity is eroded by the other annulus, which reaches everywhere: the
    // sum of two annuli that overlap over their full width has no hole left.
    CHECK(both[0].holeCount() == 0);
}

TEST_CASE("minkowskiSum: a bounded convex receiver forwards to the region side") {
    // Only `Polygon` and `PolygonWithHoles` carry the region-valued sum, so a
    // convex operand written on the left forwards to them, as `intersection`
    // does. The shape-valued sum is untouched by that: where
    // `MinkowskiSummableConcept` accepts the pair, it still answers with a
    // single shape.
    const PolygonShape u = uShape();
    const Region a = annulus();
    const RectangleShape rectangle(Point(0, 0), Point(2, 2));
    const Triangle triangle(Point(0, 0), Point(2, 0), Point(0, 2));
    const Convex convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)});

    CHECK(rectangle.minkowskiSum(u) == u.minkowskiSum(rectangle));
    CHECK(triangle.minkowskiSum(u) == u.minkowskiSum(triangle));
    CHECK(convex.minkowskiSum(u) == u.minkowskiSum(convex));
    CHECK(rectangle.minkowskiSum(a) == a.minkowskiSum(rectangle));
    CHECK(triangle.minkowskiSum(a) == a.minkowskiSum(triangle));
    CHECK(convex.minkowskiSum(a) == a.minkowskiSum(convex));

    CHECK(triangle.minkowskiSum<pgl::ERational>(u) == u.minkowskiSum<pgl::ERational>(triangle));

    // Two bounded convex operands still sum to one shape, not to a set of
    // regions, and a point operand is still a translation.
    static_assert(std::is_same_v<decltype(triangle.minkowskiSum(rectangle)), Convex>);
    static_assert(std::is_same_v<decltype(rectangle.minkowskiSum(rectangle)), RectangleShape>);
    static_assert(std::is_same_v<decltype(convex.minkowskiSum(triangle)), Convex>);
    static_assert(std::is_same_v<decltype(triangle.minkowskiSum(Point(1, 1))), Triangle>);
}

TEST_CASE("minkowskiSum: exact over rational coordinates") {
    const ERegion a(EPolygon({EPoint(0, 0), EPoint(8, 0), EPoint(8, 8), EPoint(0, 8)}),
                    std::vector<EPolygon>{
                        EPolygon({EPoint(2, 2), EPoint(6, 2), EPoint(6, 6), EPoint(2, 6)})});
    const EPolygon b({EPoint(0, 0), EPoint(1, 0), EPoint(1, 1), EPoint(0, 1)});

    const auto sum = a.minkowskiSum(b);
    REQUIRE(sum.size() == 1);
    CHECK(sum[0].outer() == EPolygon({EPoint(0, 0), EPoint(9, 0), EPoint(9, 9), EPoint(0, 9)}));
    REQUIRE(sum[0].holeCount() == 1);
    CHECK(sum[0].hole(0) == EPolygon({EPoint(3, 3), EPoint(6, 3), EPoint(6, 6), EPoint(3, 6)}));

    // A slanted summand puts the piece boundaries in general position; the
    // arrangement is built over exact rationals either way, so an answer whose
    // vertices happen to be integral comes back integral.
    const EPolygon slanted({EPoint(0, 0), EPoint(2, 1), EPoint(1, 2)});
    const auto skew = a.minkowskiSum(slanted);
    REQUIRE(skew.size() == 1);
    CHECK(skew[0].holeCount() == 1);
    CHECK(skew[0].twiceArea() > 0);
}

TEST_CASE("minkowskiSum: a non-integral crossing needs an exact result type") {
    // Two slanted boundaries can meet off the lattice: the notch of `U ⊕ T`
    // closes at (16/5, 34/5), where the sum of the U's left arm with T runs into
    // the sum of its right arm with T. That vertex is a genuine feature of the
    // answer, not an artefact of the method -- no decomposition puts it anywhere
    // else -- so an integral result type has nowhere to put it and truncates,
    // exactly as `difference` and `unionWith` document.
    const PolygonShape u = uShape();
    const Triangle t(Point(-2, -1), Point(2, 0), Point(0, 2));

    const auto exact = u.minkowskiSum<pgl::ERational>(t);
    REQUIRE(exact.size() == 1);
    const auto& ring = exact[0].outer().vertices();
    const pgl::Point<pgl::ERational> tip(pgl::ERational(16, 5), pgl::ERational(34, 5));
    CHECK(std::find(ring.begin(), ring.end(), tip) != ring.end());
    CHECK(exact[0].contains(pgl::Point<pgl::ERational>(3, 7)));

    // The default result type is the operands' own, so the same call in `int`
    // rounds the tip inward and reports (3,7) as outside.
    const auto truncated = u.minkowskiSum(t);
    REQUIRE(truncated.size() == 1);
    CHECK_FALSE(truncated[0].contains(Point(3, 7)));
}

TEST_CASE("minkowskiSum: the boundary decomposition agrees with the convex one") {
    // A convex operand lets the receiver's *boundary* be decomposed into
    // x-monotone runs, on the identity `A ⊕ B = (A + q₀) ∪ (∂A ⊕ B)`, instead of
    // its area being triangulated. Both decompositions are exact and answer the
    // same question, so they must give the same regions; what makes this worth a
    // test of its own is that they reach them through entirely different code --
    // one through the chain sweep and no triangulation at all.
    //
    // The receivers are chosen so that the boundary decomposition is what
    // actually runs, and the REQUIRE below is what says so: it is the same
    // predicate the dispatcher consults, and a change that quietly stopped the
    // decomposition from firing would fail here rather than pass vacuously.
    const PolygonShape comb({Point(0, 0), Point(12, 0), Point(12, 2), Point(10, 2), Point(10, 9),
                             Point(8, 9), Point(8, 2), Point(6, 2), Point(6, 9), Point(4, 9),
                             Point(4, 2), Point(2, 2), Point(2, 9), Point(0, 9)});
    const PolygonShape staircase({Point(0, 0), Point(9, 0), Point(9, 9), Point(7, 9), Point(7, 6),
                                  Point(5, 6), Point(5, 4), Point(3, 4), Point(3, 2),
                                  Point(0, 2)});
    const Region holed(box(0, 0, 24, 24), std::vector<PolygonShape>{box(8, 8, 16, 16)});

    const std::vector<Triangle> summands{Triangle(Point(0, 0), Point(3, 0), Point(0, 3)),
                                         Triangle(Point(-1, -2), Point(2, 0), Point(0, 2))};

    const auto agrees = [&summands](const auto& receiver) {
        for (const Triangle& summand : summands) {
            const auto operand = pgl::detail::minkowskiAsConvex(summand);
            auto runs = pgl::detail::minkowskiBoundaryRuns(receiver);
            REQUIRE(pgl::detail::minkowskiBoundaryPays(receiver, operand, runs));

            auto boundary = receiver.template minkowskiSum<pgl::ERational>(summand);
            auto convex = pgl::detail::decomposedMinkowskiSum<EPoint>(receiver, summand);
            std::sort(boundary.begin(), boundary.end());
            std::sort(convex.begin(), convex.end());
            CHECK(boundary == convex);
        }
    };
    agrees(comb);
    agrees(staircase);
    agrees(holed);

    // A slit is the one boundary the decomposition has to decline: two of the
    // region's rings cover the same stretch, so the piece it would build has a
    // boundary that overlaps itself, which the coverage classifier reads as
    // outside. The answer still has to come back right, from the other path.
    const Region slit = slitRegion();
    REQUIRE_FALSE(pgl::detail::minkowskiHasSimpleBoundary(slit));
    auto slitSum = slit.minkowskiSum<pgl::ERational>(summands[0]);
    auto slitRef = pgl::detail::decomposedMinkowskiSum<EPoint>(slit, summands[0]);
    std::sort(slitSum.begin(), slitSum.end());
    std::sort(slitRef.begin(), slitRef.end());
    CHECK(slitSum == slitRef);
}

TEST_CASE("minkowskiSum: monotone runs cover a walk exactly once") {
    // The runs are what the boundary decomposition sums, so their union has to be
    // the walk itself: every edge in exactly one run, and each run strictly
    // increasing lexicographically, which is what `MonotoneChain` requires.
    const auto runsOf = [](const std::vector<Point>& walk) {
        return pgl::detail::minkowskiMonotoneRuns(walk);
    };

    // Already monotone: one run, untouched.
    CHECK(runsOf({Point(0, 0), Point(1, 1), Point(2, 0), Point(3, 5)}).size() == 1);
    // Monotone the other way: one run, reversed so that it increases.
    const auto backwards = runsOf({Point(3, 5), Point(2, 0), Point(1, 1), Point(0, 0)});
    REQUIRE(backwards.size() == 1);
    CHECK(backwards[0].front() == Point(0, 0));
    // A turn in x, and a vertical stretch that reverses: each ends a run.
    CHECK(runsOf({Point(0, 0), Point(4, 1), Point(2, 2)}).size() == 2);
    CHECK(runsOf({Point(0, 0), Point(0, 4), Point(0, 2)}).size() == 2);
    // A repeated vertex spans no edge, so it ends the run and starts the next at
    // the second of the two -- the split costs a piece and loses nothing, which is
    // what matters, since a chain may not repeat a vertex at all.
    CHECK(runsOf({Point(0, 0), Point(1, 1), Point(1, 1), Point(2, 2)}).size() == 2);
    // A walk that is one point over and over spans no edge at all, and the caller
    // is what turns that into the summand translated there.
    CHECK(runsOf({Point(2, 3), Point(2, 3)}).empty());

    // Every edge of a zigzag is its own run, and the runs still cover it.
    const std::vector<Point> zigzag{Point(0, 0), Point(3, 1), Point(1, 2),
                                    Point(4, 3), Point(2, 4)};
    const auto runs = runsOf(zigzag);
    CHECK(runs.size() == zigzag.size() - 1);
    std::size_t edges = 0;
    for (const auto& run : runs) {
        REQUIRE(run.size() >= 2);
        for (std::size_t i = 0; i + 1 < run.size(); ++i) {
            CHECK(run[i] < run[i + 1]);  // strictly increasing, as a chain must be
        }
        edges += run.size() - 1;
    }
    CHECK(edges == zigzag.size() - 1);
}
