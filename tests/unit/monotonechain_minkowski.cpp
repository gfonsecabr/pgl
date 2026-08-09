#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

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
using Chain = pgl::MonotoneChain<Point>;
using Region = pgl::PolygonWithHoles<Point>;

using EPoint = pgl::EPoint;
using EPolygon = pgl::EPolygon;

// The Minkowski sum of a `MonotoneChain` with a bounded convex shape. It is the
// same point set `Polyline::minkowskiSum` computes for the same chain, and it is
// a **single polygon** rather than a set of regions, because the chain's own
// invariant rules out everything that would need one:
//
//   Fix a vertical line `x = c` and look at the pairs landing on it,
//   `S = {(a,b) ∈ A×B : aₓ + bₓ = c}`. The chain is sorted, so it is
//   parametrized with `aₓ` non-decreasing and the parameters with a non-empty
//   fibre form an interval; `B` is convex, so each fibre is a segment moving
//   continuously. `S` is connected, hence so is its image under `a_y + b_y`.
//
// So `A ⊕ B` meets every vertical line in a single interval: it is the region
// between two x-monotone chains, one piece, never holed. What the construction
// then is, is a sweep — one convex merge per chain edge, and the pieces' lower
// and upper boundaries merged into the sum's two — rather than an arrangement of
// all the piece sums with a constrained triangulation over it.
//
// The tests below check that claim from the outside: against the definition of
// the sum over a probe grid, and against `Polyline`'s answer for the same chain,
// which is the same point set computed by a completely different engine.
//
// Two operands are not on that contract. A `Segment` and an `OrientedSegment`
// have no area of their own, so consecutive pieces of the sum can meet at a point
// rather than overlap and the sum can pinch shut, which no polygon may do; they
// keep the region-valued, regularized contract a `Polyline` has for them.

// -----------------------------------------------------------------------------
// In a template so the requires-expressions are dependent: a non-dependent one is
// a hard error rather than `false` under g++.

template <class A, class B>
inline constexpr bool summable = requires(const A& a, const B& b) { a.minkowskiSum(b); };

// -----------------------------------------------------------------------------
// Fixtures. Every one of them is sorted, which is all a chain asks.

// A single segment: the one chain that is convex.
static Chain segmentChain() {
    return Chain({Point(0, 0), Point(4, 0)});
}

// A peak. Its two edges' sums overlap, and their upper boundaries meet flat.
static Chain peakChain() {
    return Chain({Point(0, 0), Point(1, 1), Point(2, 0)});
}

// A valley. Its two edges' sums cross *above* the notch, at a point that is not
// on the lattice — the case that makes `ResultNumber` matter.
static Chain valleyChain() {
    return Chain({Point(0, 3), Point(1, 0), Point(2, 3)});
}

// A saw: four edges, alternating, whose sums all overlap under a wide operand.
static Chain sawChain() {
    return Chain({Point(0, 0), Point(1, 3), Point(2, 0), Point(3, 3), Point(4, 0)});
}

// A staircase, whose vertical edges are what make the sum's boundary run
// vertically in the middle: a piece as tall as such an edge overshoots its
// neighbour at the x where it ends, and the boundary steps across.
static Chain stairChain() {
    return Chain({Point(0, 0), Point(0, 3), Point(2, 3), Point(2, 6), Point(4, 6)});
}

// A chain with one vertical edge taller than most operands.
static Chain cliffChain() {
    return Chain({Point(0, 0), Point(1, 0), Point(1, 5), Point(3, 4)});
}

// -----------------------------------------------------------------------------
// Oracles, neither of which knows anything about the construction under test.

// The definition of the sum read as a query: `p ∈ A ⊕ B` exactly when `A` meets
// the reflected copy of `B` placed at `p`, since `p − B` is `(−B) + p` and a 180°
// rotation about the origin *is* the negation.
template <class ShapeB>
static bool inSumByDefinition(const Chain& a, const ShapeB& b, const Point& p) {
    auto placed = b.rotated90(2);
    placed += p;
    return a.intersects(placed);
}

// Probes every integer point of a box comfortably holding the sum. The result
// type is exact on purpose: two pieces can meet off the lattice, and an oracle
// checking the true point set has to ask for it.
template <class ShapeB>
static void checkAgainstDefinition(const Chain& a, const ShapeB& b, int lo, int hi) {
    const EPolygon sum = a.template minkowskiSum<pgl::ERational>(b);
    for (int x = lo; x <= hi; ++x) {
        for (int y = lo; y <= hi; ++y) {
            const Point p(x, y);
            INFO("probe " << p);
            CHECK(sum.contains(EPoint(p)) == inSumByDefinition(a, b, p));
        }
    }
}

// The other oracle: the same chain, summed the way a `Polyline` is. That answer
// is regularized and this one is not, so they agree as point sets whenever the
// sum has area everywhere — which it does whenever the operand has some.
template <class ShapeB>
static void checkAgainstPolyline(const Chain& a, const ShapeB& b) {
    const EPolygon mine = a.template minkowskiSum<pgl::ERational>(b);
    const auto theirs = a.asPolyline().template minkowskiSum<pgl::ERational>(b);
    const auto difference =
        pgl::PolygonWithHoles<EPoint>(mine).template symmetricDifference<pgl::ERational>(
            theirs);
    INFO("chain " << a << " operand " << b << " gave " << mine << " against " << theirs);
    CHECK(difference.isEmpty());
}

// -----------------------------------------------------------------------------

TEST_CASE("minkowskiSum: the sum of a monotone chain with a convex shape is one polygon") {
    // A chain that is a segment is convex, so this is the plain convex merge
    // written as a polygon: the unit square dragged along four units of x.
    CHECK(segmentChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1))) ==
          PolygonShape({Point(0, 0), Point(5, 0), Point(5, 1), Point(0, 1)}));

    // The peak. Below, the two pieces' lower boundaries cross under the apex at
    // (3/2, 1/2); above, they meet flat and the boundary runs straight across.
    CHECK(peakChain().minkowskiSum<pgl::ERational>(RectangleShape(Point(0, 0), Point(1, 1))) ==
          EPolygon({EPoint(0, 0), EPoint(1, 0), EPoint(pgl::ERational(3, 2), pgl::ERational(1, 2)),
                    EPoint(2, 0), EPoint(3, 0), EPoint(3, 1), EPoint(2, 2), EPoint(1, 2),
                    EPoint(0, 1)}));

    // A wide operand swallows a saw whole: the lower boundary is flat because
    // some tooth's foot is always within reach, and the upper one is flat between
    // the outermost teeth for the same reason.
    CHECK(sawChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(4, 1))) ==
          PolygonShape({Point(0, 0), Point(8, 0), Point(8, 1), Point(7, 4), Point(1, 4),
                        Point(0, 1)}));

    // A triangle operand, where the two boundaries are built from different edges
    // of it: the sum's upper boundary runs straight from (1,2) to (3,0) because
    // the two pieces' upper edges there are collinear, and the redundant vertex
    // between them is dropped.
    CHECK(peakChain().minkowskiSum<pgl::ERational>(Triangle(Point(0, 0), Point(1, 0), Point(0, 1))) ==
          EPolygon({EPoint(0, 0), EPoint(1, 0), EPoint(pgl::ERational(3, 2), pgl::ERational(1, 2)),
                    EPoint(2, 0), EPoint(3, 0), EPoint(1, 2), EPoint(0, 1)}));
}

TEST_CASE("minkowskiSum: agrees with the definition over a probe grid") {
    const std::vector<RectangleShape> rectangles{
        RectangleShape(Point(0, 0), Point(2, 1)), RectangleShape(Point(0, 0), Point(2, 2)),
        RectangleShape(Point(-1, -1), Point(1, 1)), RectangleShape(Point(0, 0), Point(1, 3))};
    const std::vector<Triangle> triangles{Triangle(Point(0, 0), Point(3, 0), Point(0, 3)),
                                          Triangle(Point(0, 0), Point(2, 0), Point(1, 3)),
                                          Triangle(Point(-2, -1), Point(2, 0), Point(0, 2))};
    const std::vector<Convex> convexes{
        Convex(std::vector<Point>{Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)}),
        Convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)})};

    for (const Chain& a : {segmentChain(), peakChain(), valleyChain(), sawChain(), stairChain(),
                           cliffChain()}) {
        for (const RectangleShape& b : rectangles) {
            checkAgainstDefinition(a, b, -5, 12);
        }
        for (const Triangle& b : triangles) {
            checkAgainstDefinition(a, b, -5, 12);
        }
        for (const Convex& b : convexes) {
            checkAgainstDefinition(a, b, -5, 12);
        }
    }

    // A chain of one vertex is a point, and the sum is the operand translated.
    const Chain lone({Point(2, 1)});
    checkAgainstDefinition(lone, rectangles[1], -3, 8);
    checkAgainstDefinition(lone, triangles[2], -3, 8);
}

TEST_CASE("minkowskiSum: agrees with the same chain summed as a polyline") {
    // The other engine: the arrangement of all the piece sums, with a constrained
    // triangulation over it. Same point set, computed without any of this file's
    // reasoning about monotonicity.
    const RectangleShape rectangle(Point(0, 0), Point(2, 1));
    const Triangle triangle(Point(-2, -1), Point(2, 0), Point(0, 2));
    const Convex convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)});

    for (const Chain& a : {segmentChain(), peakChain(), valleyChain(), sawChain(), stairChain(),
                           cliffChain()}) {
        checkAgainstPolyline(a, rectangle);
        checkAgainstPolyline(a, triangle);
        checkAgainstPolyline(a, convex);
    }
}

TEST_CASE("minkowskiSum: a vertical chain edge steps the boundary") {
    // The piece over a vertical edge is as tall as that edge where it ends, so it
    // overshoots the piece beside it there and the boundary runs vertically for a
    // stretch. Nothing about that is exceptional — it is what the sum looks like —
    // but it is the one shape a boundary read as a function of x cannot have, so
    // it is worth pinning down on its own.
    const Chain step({Point(0, 0), Point(0, 4)});
    CHECK(step.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1))) ==
          PolygonShape({Point(0, 0), Point(1, 0), Point(1, 5), Point(0, 5)}));

    // A vertical edge of 5 against an operand only 1 wide: the sum keeps the
    // vertical wall the edge sweeps, in the middle of the boundary rather than at
    // its ends.
    const auto sum = cliffChain().minkowskiSum(RectangleShape(Point(0, 0), Point(1, 1)));
    CHECK(sum.contains(Point(2, 5)));
    CHECK(sum.contains(Point(2, 1)));
    CHECK_FALSE(sum.contains(Point(3, 6)));
    checkAgainstDefinition(cliffChain(), RectangleShape(Point(0, 0), Point(1, 1)), -3, 10);
    checkAgainstPolyline(cliffChain(), RectangleShape(Point(0, 0), Point(1, 1)));
}

TEST_CASE("minkowskiSum: an operand of no width sweeps the chain vertically") {
    // A vertical segment has no span for the sweep to run along, and needs none:
    // the sum's fibre over x is the chain's own fibre widened by the operand's, so
    // the answer is the chain traced out through one end of the operand and back
    // through the other.
    const auto sum = peakChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(0, 2)));
    CHECK(sum == PolygonShape({Point(0, 0), Point(1, 1), Point(2, 0), Point(2, 2), Point(1, 3),
                               Point(0, 2)}));
    checkAgainstDefinition(peakChain(), RectangleShape(Point(0, 0), Point(0, 2)), -3, 8);

    // An operand that has collapsed to a point has no area at all, and this sum is
    // not regularized: the answer is the point set, which is the translated chain,
    // reported as the degenerate polygon that traces it out and back.
    const auto flat = peakChain().minkowskiSum(RectangleShape(Point(3, 3), Point(3, 3)));
    CHECK(flat.isDegenerate());
    CHECK(flat.area() == 0);
    for (const Point& vertex : {Point(3, 3), Point(4, 4), Point(5, 3)}) {
        CHECK(flat.contains(vertex));
    }
}

TEST_CASE("minkowskiSum: exact for integer coordinates, and off the lattice on request") {
    // The construction divides nowhere: every vertex of every piece is a sum of
    // two input vertices, and the sweep decides everything with integer
    // determinants. So a sum whose boundary has no crossing is exact in `int`.
    const Chain a = stairChain();
    const RectangleShape b(Point(0, 0), Point(2, 1));
    const auto plain = a.minkowskiSum(b);
    const auto exact = a.minkowskiSum<pgl::ERational>(b);
    REQUIRE(plain.size() == exact.size());
    for (std::size_t i = 0; i < plain.size(); ++i) {
        CHECK(EPoint(plain[i]) == exact[i]);
    }

    // Where two pieces genuinely cross, the vertex need not be on the lattice, and
    // it is the caller's `ResultNumber` that decides what happens to it. Both arms
    // of the valley are integral and their sums still meet at (3/2, 5/2).
    const auto notch = valleyChain().minkowskiSum<pgl::ERational>(RectangleShape(Point(0, 0), Point(1, 1)));
    bool found = false;
    for (const EPoint& vertex : notch) {
        found = found || vertex == EPoint(pgl::ERational(3, 2), pgl::ERational(5, 2));
    }
    CHECK(found);
    // Asked for an integral result, that one vertex truncates, exactly as the
    // boolean operations and the region-valued sums truncate theirs. Every other
    // vertex of the answer is a sum of two input vertices and survives intact.
    const auto truncated = valleyChain().minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
    REQUIRE(truncated.size() == notch.size());
    for (std::size_t i = 0; i < truncated.size(); ++i) {
        const bool isTheCrossing = notch[i] == EPoint(pgl::ERational(3, 2), pgl::ERational(5, 2));
        CHECK((EPoint(truncated[i]) == notch[i]) == !isTheCrossing);
    }
    CHECK(truncated.contains(Point(1, 2)));
}

TEST_CASE("minkowskiSum: commutes, and translates with its operands") {
    const Chain a = sawChain();
    const RectangleShape rectangle(Point(0, 0), Point(2, 2));
    const Triangle triangle(Point(0, 0), Point(2, 0), Point(0, 2));
    const Convex convex(std::vector<Point>{Point(0, 0), Point(3, 1), Point(1, 3)});

    // Written on the left, a convex operand forwards to the chain: which operand
    // comes first never decides the answer.
    CHECK(rectangle.minkowskiSum(a) == a.minkowskiSum(rectangle));
    CHECK(triangle.minkowskiSum(a) == a.minkowskiSum(triangle));
    CHECK(convex.minkowskiSum(a) == a.minkowskiSum(convex));

    // `(A + t) ⊕ B = (A ⊕ B) + t`, from either side.
    const Point shift(3, -2);
    auto translated = a.minkowskiSum(rectangle);
    translated += shift;
    CHECK((a + shift).minkowskiSum(rectangle) == translated);
    auto moved = rectangle;
    moved += shift;
    CHECK(a.minkowskiSum(moved) == translated);
}

TEST_CASE("minkowskiSum: a summand with no area keeps the region-valued contract") {
    // A segment sweeps a chain edge into a parallelogram unless the two are
    // parallel, and consecutive pieces then meet along a segment rather than
    // overlapping. So the sum can pinch shut, which a polygon may not do, and
    // these two operands answer with regions like a `Polyline`'s.
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum<int>(
                                     std::declval<const Segment&>())),
                                 std::vector<Region>>);

    const auto sum = peakChain().minkowskiSum(Segment(Point(0, 0), Point(0, 2)));
    REQUIRE(sum.size() == 1);
    CHECK(sum.front().holes().empty());
    // The same answer the chain gives as a polyline, which is where this pair's
    // contract comes from.
    CHECK(sum == peakChain().asPolyline().minkowskiSum(Segment(Point(0, 0), Point(0, 2))));

    // An orientation is not part of a point set.
    CHECK(peakChain().minkowskiSum(OrientedSegment(Point(0, 2), Point(0, 0))) == sum);

    // Regularized, so a chain summed along its own direction keeps nothing.
    CHECK(segmentChain().minkowskiSum(Segment(Point(0, 0), Point(2, 0))).empty());

    // And a chain whose parts merely touch after the flat sweep is dropped comes
    // back as more than one region — the answer no polygon could have given.
    const Chain bent({Point(0, 0), Point(2, 2), Point(4, 0)});
    const auto touching = bent.minkowskiSum(Segment(Point(0, 0), Point(1, 1)));
    CHECK(touching == bent.asPolyline().minkowskiSum(Segment(Point(0, 0), Point(1, 1))));
}

TEST_CASE("minkowskiSum: the pairs a chain accepts") {
    // A `Point` is a translation, and stays the single-shape sum every shape has.
    static_assert(pgl::MinkowskiSummableConcept<Chain, Point>);
    CHECK(peakChain().minkowskiSum(Point(1, 1)) ==
          Chain({Point(1, 1), Point(2, 2), Point(3, 1)}));
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum(
                                     std::declval<const Point&>())),
                                 Chain>);

    // A chain is not convex, so no other pair uses the generic one-shape sum.
    // The three convex operands return a polygon; a non-convex polygon returns
    // one region, while thin operands and non-regular regions may need several.
    static_assert(!pgl::MinkowskiSummableConcept<Chain, RectangleShape>);
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum<int>(
                                     std::declval<const RectangleShape&>())),
                                 PolygonShape>);
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum<int>(
                                     std::declval<const Triangle&>())),
                                 PolygonShape>);
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum<int>(
                                     std::declval<const Convex&>())),
                                 PolygonShape>);
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum<int>(
                                     std::declval<const OrientedSegment&>())),
                                 std::vector<Region>>);

    // The non-convex operands, whose own concavity strands cavities whatever the
    // chain does: they own the pair by rank and answer with regions, from either
    // side.
    static_assert(summable<Chain, PolygonShape>);
    static_assert(summable<PolygonShape, Chain>);
    static_assert(summable<Chain, Region>);
    static_assert(summable<Region, Chain>);
    static_assert(std::is_same_v<decltype(std::declval<const Chain&>().minkowskiSum<int>(
                                     std::declval<const PolygonShape&>())),
                                 Region>);

    // Two chains are the pair left out, exactly as two polylines are: neither
    // outranks the other, so neither owns it. `asPolyline()` and the operand's
    // edges are the way through.
    static_assert(!summable<Chain, Chain>);
    static_assert(!summable<Chain, PolylineShape>);
    static_assert(!summable<PolylineShape, Chain>);

    // Unbounded and curved operands are refused as they are everywhere.
    static_assert(!summable<Chain, pgl::Halfplane<Point>>);
    static_assert(!summable<Chain, pgl::Disk<Point>>);
    static_assert(!summable<Chain, pgl::Line<Point>>);
}

TEST_CASE("minkowskiSum: a non-convex operand answers as it does for a polyline") {
    // The chain contributes its edges, the operand its triangulation, and the
    // answer is the union of the piece sums — the same construction, and the same
    // answer, that the operand gives for the chain read as a polyline.
    const PolygonShape u({Point(0, 0), Point(6, 0), Point(6, 6), Point(4, 6), Point(4, 2),
                          Point(2, 2), Point(2, 6), Point(0, 6)});
    const Chain a = peakChain();
    CHECK(a.minkowskiSum(u) == a.asPolyline().minkowskiSum(u));
    CHECK(u.minkowskiSum(a) == a.minkowskiSum(u));

    const Region annulus(PolygonShape({Point(0, 0), Point(8, 0), Point(8, 8), Point(0, 8)}),
                         std::vector<PolygonShape>{
                             PolygonShape({Point(2, 2), Point(6, 2), Point(6, 6), Point(2, 6)})});
    CHECK(a.minkowskiSum(annulus) == a.asPolyline().minkowskiSum(annulus));
    CHECK(annulus.minkowskiSum(a) == a.minkowskiSum(annulus));
}
