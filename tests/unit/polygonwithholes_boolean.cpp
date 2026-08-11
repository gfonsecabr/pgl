#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

using EPoint = pgl::EPoint;
using EPolygon = pgl::EPolygon;
using ERegion = pgl::EPolygonWithHoles;

// The three boolean operations that join the difference: union `A ∪ B`,
// intersection `A ∩ B` and symmetric difference `A △ B`. All four are one
// engine run with one line changed -- the arrangement of both boundaries cuts
// the plane into cells on which membership in each operand is constant, and a
// per-cell test says which cells survive -- so what these tests are really for
// is the *contract* of each operation, and the ways a result can need a region.
//
// Each of the three needs `PolygonWithHoles` for a reason of its own:
//
//   * a union creates a hole out of nothing when the operands wrap round
//     between them, as a `U` united with the bar that caps it;
//   * an intersection keeps the holes of a holed operand. That is not a
//     contradiction of `Polygon::intersection` returning plain polygons: the
//     argument that no component of `A ∩ B` carries a hole assumes both
//     operands have a *connected complement*, and a region with holes is the
//     one shape in the library that does not have one;
//   * a symmetric difference is the union of two differences and inherits
//     holes from both.
//
// What is computed is the regularized result in every case, so material with no
// area -- a slit, a boundary stretch the operands share, an isolated contact
// point -- never reaches the answer.

// -----------------------------------------------------------------------------
// Fixtures, all on the even lattice so that the exhaustive oracle below can
// probe unit cells at integer (odd, odd) points.

static PolygonShape square(int lo, int hi) {
    return PolygonShape({Point(lo, lo), Point(hi, lo), Point(hi, hi), Point(lo, hi)});
}

static PolygonShape box(int x0, int y0, int x1, int y1) {
    return PolygonShape({Point(x0, y0), Point(x1, y0), Point(x1, y1), Point(x0, y1)});
}

namespace fixtures {

// A plain square.
static Region plain() { return Region(square(0, 12)); }

// A square with a hole in the middle.
static Region annulus() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(4, 4, 8, 8)});
}

// A hole spanning the square, so the region's *domain* is two slabs joined only
// by the slits along the left and right edges.
static Region band() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(0, 4, 12, 8)});
}

// A hole sharing a stretch of the outer ring: the region is an L, and the shared
// stretch is a slit, which no regularized answer may keep.
static Region notched() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(6, 6, 12, 12)});
}

// Two holes meeting at a single point, pinching the region shut there.
static Region pinched() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(0, 0, 6, 6), box(6, 6, 12, 12)});
}

// A C-shaped hole: its cavity is material, reachable only through the gap.
static Region cavity() {
    return Region(square(0, 12),
                  std::vector<PolygonShape>{PolygonShape({Point(2, 2), Point(10, 2), Point(10, 10),
                                                Point(2, 10), Point(2, 8), Point(8, 8),
                                                Point(8, 4), Point(2, 4)})});
}

// An L-shaped outer ring with a hole in the long arm.
static Region ell() {
    return Region(PolygonShape({Point(0, 0), Point(12, 0), Point(12, 6), Point(6, 6), Point(6, 12),
                           Point(0, 12)}),
                  std::vector<PolygonShape>{box(8, 2, 10, 4)});
}

static std::vector<std::pair<std::string, Region>> all() {
    return {{"plain", plain()}, {"annulus", annulus()}, {"band", band()},
            {"notched", notched()}, {"pinched", pinched()}, {"cavity", cavity()},
            {"ell", ell()}};
}

}  // namespace fixtures

// The regularization of a region, `closure(A°)`, computed by increment 10's
// difference against an operand that removes nothing. It is what every one of
// these operations returns when both operands are the same shape, and it
// differs from the region itself exactly on the fixtures carrying a slit.
static RegionSet regularized(const Region& region) {
    return region.difference<int>(PolygonShape{});
}

static int twiceAreaOf(const RegionSet& pieces) {
    int total = 0;
    for (const Region& piece : pieces) {
        total += piece.twiceArea();
    }
    return total;
}

// -----------------------------------------------------------------------------
// Union

TEST_CASE("union: a U capped by a bar encloses a hole neither operand has") {
    const PolygonShape u({Point(0, 0), Point(9, 0), Point(9, 9), Point(6, 9), Point(6, 3), Point(3, 3),
                     Point(3, 9), Point(0, 9)});
    const auto pieces = u.unionWith<int>(box(0, 9, 9, 12));

    REQUIRE(pieces.componentCount() == 1);
    REQUIRE(pieces.component(0).holeCount() == 1);
    CHECK(pieces.component(0).hole(0) == box(3, 3, 6, 9));
    CHECK(pieces.component(0).outer() == box(0, 0, 9, 12));
    CHECK(pieces.component(0).twiceArea() == 2 * (63 + 27));
    CHECK(pieces.component(0).isValid());
}

TEST_CASE("union: overlapping shapes fuse into one piece") {
    const auto pieces = square(0, 10).unionWith<int>(box(5, 5, 15, 15));

    REQUIRE(pieces.componentCount() == 1);
    CHECK(pieces.component(0).holeCount() == 0);
    CHECK(pieces.component(0).twiceArea() == 2 * (100 + 100 - 25));
}

TEST_CASE("union: shapes sharing a stretch of boundary fuse; a point does not") {
    const auto abutting = square(0, 10).unionWith<int>(box(10, 0, 20, 10));
    REQUIRE(abutting.componentCount() == 1);
    CHECK(abutting.component(0).outer() == box(0, 0, 20, 10));

    // Meeting at a single corner, the union pinches shut there. A region may not
    // have a self-touching outer ring, so it comes back as two pieces.
    const auto corner = square(0, 10).unionWith<int>(box(10, 10, 20, 20));
    REQUIRE(corner.componentCount() == 2);
    CHECK(twiceAreaOf(corner) == 2 * 200);
}

TEST_CASE("union: disjoint shapes come back side by side") {
    const auto pieces = square(0, 10).unionWith<int>(box(20, 20, 30, 30));

    REQUIRE(pieces.componentCount() == 2);
    CHECK(pieces.component(0).outer() == square(0, 10));
    CHECK(pieces.component(1).outer() == box(20, 20, 30, 30));
}

TEST_CASE("union: filling a hole") {
    const Region annulus = fixtures::annulus();

    // Exactly plugged: the hole is gone and the square comes back whole.
    const auto plugged = annulus.unionWith<int>(box(4, 4, 8, 8));
    REQUIRE(plugged.componentCount() == 1);
    CHECK(plugged.component(0).holeCount() == 0);
    CHECK(plugged.component(0).outer() == square(0, 12));

    // Half plugged: what is left of the hole is still a hole.
    const auto half = annulus.unionWith<int>(box(4, 4, 8, 6));
    REQUIRE(half.componentCount() == 1);
    REQUIRE(half.component(0).holeCount() == 1);
    CHECK(half.component(0).hole(0) == box(4, 6, 8, 8));

    // A plug reaching out of the region: the outer ring grows, and what is left
    // of the hole is a U -- still one hole, since its two arms are joined below
    // the plug and the region closes over both of them.
    const auto through = annulus.unionWith<int>(box(5, 5, 7, 20));
    REQUIRE(through.componentCount() == 1);
    REQUIRE(through.component(0).holeCount() == 1);
    CHECK(through.component(0).hole(0) == PolygonShape({Point(4, 4), Point(8, 4), Point(8, 8), Point(7, 8),
                                         Point(7, 5), Point(5, 5), Point(5, 8), Point(4, 8)}));
    // 128 for the region, plus 30 for the plug, less the 8 they share.
    CHECK(through.component(0).twiceArea() == 2 * 150);
}

TEST_CASE("union: a region with a slit loses it, since a slit has no area") {
    // `notched` is an L whose hole shares the top and right edges of the outer
    // ring; those shared stretches are slits, in the region but with no area
    // beside them. The regularized union may not keep them.
    const auto pieces = fixtures::notched().unionWith<int>(PolygonShape{});

    REQUIRE(pieces.componentCount() == 1);
    CHECK(pieces.component(0).holeCount() == 0);
    CHECK(pieces.component(0).outer() == PolygonShape({Point(0, 0), Point(12, 0), Point(12, 6), Point(6, 6),
                                        Point(6, 12), Point(0, 12)}));
    CHECK(pieces.component(0).twiceArea() == fixtures::notched().twiceArea());
}

// -----------------------------------------------------------------------------
// Intersection
//
// This is the operation `Polygon` does not offer in region form, and the tests
// below are what says why: every one of them keeps a hole that a
// `std::vector<Polygon>` result would have to lose.

TEST_CASE("intersection: a region met by a shape covering it is the region itself") {
    for (const auto& [fixture, region] : fixtures::all()) {
        INFO(fixture);
        const auto pieces = region.intersection<int>(box(-5, -5, 20, 20));
        CHECK(pieces == regularized(region));
        CHECK(twiceAreaOf(pieces) == region.twiceArea());
    }
}

TEST_CASE("intersection: a hole survives being met") {
    // Cut through, the hole opens: what is left of it is a bite out of the
    // boundary rather than a hole.
    const auto half = fixtures::annulus().intersection<int>(box(0, 0, 12, 6));
    REQUIRE(half.componentCount() == 1);
    CHECK(half.component(0).holeCount() == 0);
    CHECK(half.component(0).twiceArea() == 2 * (72 - 8));

    // Kept whole, the hole stays a hole.
    const auto around = fixtures::annulus().intersection<int>(box(1, 1, 11, 11));
    REQUIRE(around.componentCount() == 1);
    REQUIRE(around.component(0).holeCount() == 1);
    CHECK(around.component(0).hole(0) == box(4, 4, 8, 8));
    CHECK(around.component(0).outer() == box(1, 1, 11, 11));
}

TEST_CASE("intersection: the holes of both operands are holes of the result") {
    const Region a(square(0, 12), std::vector<PolygonShape>{box(2, 2, 5, 5)});
    const Region b(square(0, 12), std::vector<PolygonShape>{box(7, 7, 10, 10)});
    const auto pieces = a.intersection<int>(b);

    REQUIRE(pieces.componentCount() == 1);
    REQUIRE(pieces.component(0).holeCount() == 2);
    CHECK(pieces.component(0).hole(0) == box(2, 2, 5, 5));
    CHECK(pieces.component(0).hole(1) == box(7, 7, 10, 10));
    CHECK(pieces.component(0).twiceArea() == 2 * (144 - 9 - 9));
}

TEST_CASE("intersection: touching without overlapping is empty, being regularized") {
    CHECK(Region(square(0, 10)).intersection<int>(box(10, 0, 20, 10)).empty());
    CHECK(Region(square(0, 10)).intersection<int>(box(10, 10, 20, 20)).empty());
    CHECK(Region(square(0, 10)).intersection<int>(box(20, 20, 30, 30)).empty());

    // A shape lying entirely in a hole meets the region only along the rim.
    CHECK(fixtures::annulus().intersection<int>(box(5, 5, 7, 7)).empty());
    CHECK(fixtures::annulus().intersection<int>(box(4, 4, 8, 8)).empty());
}

TEST_CASE("intersection: a cut can split the result in two") {
    const auto pieces = fixtures::annulus().intersection<int>(box(0, 4, 12, 8));

    REQUIRE(pieces.componentCount() == 2);
    CHECK(twiceAreaOf(pieces) == 2 * (48 - 16));
    CHECK(pieces.component(0).holeCount() == 0);
    CHECK(pieces.component(1).holeCount() == 0);
}

TEST_CASE("intersection: exact where Polygon::intersection truncates") {
    // The two boundaries cross at (5, 5) and (5, 0) -- integral points -- so an
    // integral result type loses nothing here. The arrangement is built over
    // rationals whatever the result type is, so nothing is truncated on the way.
    const Region region(square(0, 10));
    const auto pieces = region.intersection<int>(PolygonShape({Point(0, 0), Point(10, 10), Point(0, 10)}));

    REQUIRE(pieces.componentCount() == 1);
    CHECK(pieces.component(0).outer() == PolygonShape({Point(0, 0), Point(10, 10), Point(0, 10)}));
    CHECK(pieces.component(0).twiceArea() == 100);
}

// -----------------------------------------------------------------------------
// Symmetric difference

TEST_CASE("symmetric difference: the part exactly one operand covers") {
    const auto pieces = square(0, 10).symmetricDifference<int>(box(5, 5, 15, 15));

    // The two L-shaped remainders meet only at (5, 10) and (10, 5), so they are
    // two pieces rather than one.
    REQUIRE(pieces.componentCount() == 2);
    CHECK(twiceAreaOf(pieces) == 2 * (100 - 25 + 100 - 25));
}

TEST_CASE("symmetric difference: a shape against itself is empty") {
    for (const auto& [fixture, region] : fixtures::all()) {
        CHECK_MESSAGE(region.symmetricDifference<int>(region).empty(), fixture);
    }
    CHECK(square(0, 10).symmetricDifference<int>(square(0, 10)).empty());
}

TEST_CASE("symmetric difference: a region against its own outer polygon is its holes") {
    const auto pieces = fixtures::annulus().symmetricDifference<int>(square(0, 12));

    REQUIRE(pieces.componentCount() == 1);
    CHECK(pieces.component(0).outer() == box(4, 4, 8, 8));
    CHECK(pieces.component(0).holeCount() == 0);
}

TEST_CASE("symmetric difference: it can carry a hole of its own") {
    // The bigger square minus the smaller one is an annulus; the smaller one
    // contributes nothing, being covered by both.
    const auto pieces = square(0, 12).symmetricDifference<int>(box(4, 4, 8, 8));

    REQUIRE(pieces.componentCount() == 1);
    REQUIRE(pieces.component(0).holeCount() == 1);
    CHECK(pieces.component(0).hole(0) == box(4, 4, 8, 8));
}

TEST_CASE("symmetric difference: a region against its own hole is the square whole") {
    // The two operands share only the hole's rim, which has no area, so the
    // regularized answer joins across it: the annulus and the plug come back as
    // one filled square rather than two pieces meeting along the rim.
    const auto pieces = fixtures::annulus().symmetricDifference<int>(box(4, 4, 8, 8));

    REQUIRE(pieces.componentCount() == 1);
    CHECK(pieces.component(0).outer() == square(0, 12));
    CHECK(pieces.component(0).holeCount() == 0);
    CHECK(pieces.component(0).twiceArea() == 2 * 144);
}

// -----------------------------------------------------------------------------
// Every operand type, on both receivers.

TEST_CASE("boolean operations accept every bounded shape with area") {
    const Region region = fixtures::annulus();
    const PolygonShape polygon = box(2, 2, 10, 10);
    const Convex convex(std::vector<Point>{Point(2, 2), Point(10, 2), Point(10, 10),
                                           Point(2, 10)});
    const Triangle triangle(Point(2, 2), Point(10, 2), Point(10, 10));
    const RectangleShape rectangle(Point(2, 2), Point(10, 10));

    // A convex operand and the polygon spelling of it must answer alike.
    CHECK(region.unionWith<int>(convex) == region.unionWith<int>(polygon));
    CHECK(region.unionWith<int>(rectangle) == region.unionWith<int>(polygon));
    CHECK(region.intersection<int>(convex) == region.intersection<int>(polygon));
    CHECK(region.intersection<int>(rectangle) == region.intersection<int>(polygon));
    CHECK(region.symmetricDifference<int>(convex) == region.symmetricDifference<int>(polygon));
    CHECK(region.symmetricDifference<int>(rectangle) == region.symmetricDifference<int>(polygon));

    CHECK(polygon.unionWith<int>(convex) == polygon.unionWith<int>(rectangle));
    CHECK(polygon.symmetricDifference<int>(convex) == polygon.symmetricDifference<int>(rectangle));

    // The triangle is half of it, so it is a different answer, not the same one.
    CHECK(!region.intersection<int>(triangle).empty());
    CHECK(region.intersection<int>(triangle) != region.intersection<int>(convex));
    CHECK(!polygon.unionWith<int>(triangle).empty());
    CHECK(!polygon.symmetricDifference<int>(triangle).empty());

    // Region operands, on both receivers.
    CHECK(polygon.unionWith<int>(region) == Region(polygon).unionWith<int>(region));
    CHECK(polygon.symmetricDifference<int>(region) == Region(polygon).symmetricDifference<int>(region));
}

TEST_CASE("boolean operations: a lower-ranked receiver forwards to the region side") {
    // The symmetric three are implemented once per unordered pair, on the
    // higher-ranked of its two operands. A shape of lower `shapeRank` -- a
    // convex polygon, a triangle, a rectangle -- takes the pair by forwarding
    // to the other operand, exactly as `intersection` does, so that writing it
    // in either order is the same call. (`difference` is not symmetric and
    // forwards nowhere; every one of its ordered pairs is stated on its own
    // receiver, which the grid test below covers.)
    const Region region = fixtures::annulus();
    const PolygonShape polygon = box(2, 2, 10, 10);
    const Convex convex(std::vector<Point>{Point(2, 2), Point(10, 2), Point(10, 10),
                                           Point(2, 10)});
    const Triangle triangle(Point(2, 2), Point(10, 2), Point(10, 10));
    const RectangleShape rectangle(Point(2, 2), Point(10, 10));

    CHECK(convex.unionWith<int>(polygon) == polygon.unionWith<int>(convex));
    CHECK(triangle.unionWith<int>(polygon) == polygon.unionWith<int>(triangle));
    CHECK(rectangle.unionWith<int>(polygon) == polygon.unionWith<int>(rectangle));
    CHECK(convex.unionWith<int>(region) == region.unionWith<int>(convex));
    CHECK(triangle.unionWith<int>(region) == region.unionWith<int>(triangle));
    CHECK(rectangle.unionWith<int>(region) == region.unionWith<int>(rectangle));

    CHECK(convex.symmetricDifference<int>(polygon) == polygon.symmetricDifference<int>(convex));
    CHECK(triangle.symmetricDifference<int>(polygon) == polygon.symmetricDifference<int>(triangle));
    CHECK(rectangle.symmetricDifference<int>(region) == region.symmetricDifference<int>(rectangle));
    CHECK(triangle.symmetricDifference<int>(region) == region.symmetricDifference<int>(triangle));

    // `intersection` is the one whose two receivers answer differently:
    // `Polygon::intersection` returns components, `PolygonWithHoles` returns
    // regions. The region side owns the mixed pair, so a polygon receiver
    // forwards to it and gets regions back rather than components.
    CHECK(polygon.intersection<int>(region) == region.intersection<int>(polygon));
    CHECK(triangle.intersection<int>(region) == region.intersection<int>(triangle));
    static_assert(std::is_same_v<decltype(polygon.intersection<int>(region)),
                                 decltype(region.intersection<int>(polygon))>);

    // The requested result type travels through the forwarder untouched.
    CHECK(triangle.unionWith<pgl::ERational>(region) == region.unionWith<pgl::ERational>(triangle));
    static_assert(std::is_same_v<decltype(triangle.unionWith<pgl::ERational>(region)),
                                 pgl::PolygonSet<EPoint>>);

    // The three convex regions own their pairs among themselves, for the
    // symmetric difference exactly as for the union: each pair on the
    // higher-ranked operand, with the lower-ranked one forwarding to it.
    CHECK(rectangle.symmetricDifference<int>(triangle) ==
          triangle.symmetricDifference<int>(rectangle));
    CHECK(rectangle.symmetricDifference<int>(convex) == convex.symmetricDifference<int>(rectangle));
    CHECK(triangle.symmetricDifference<int>(convex) == convex.symmetricDifference<int>(triangle));
    CHECK(rectangle.symmetricDifference<int>(rectangle).empty());
    CHECK(triangle.symmetricDifference<int>(triangle).empty());
    CHECK(convex.symmetricDifference<int>(convex).empty());
    // The convex spelling and the polygon spelling of one shape answer alike.
    CHECK(rectangle.symmetricDifference<int>(triangle) ==
          rectangle.asPolygon().symmetricDifference<int>(triangle));
}

// Both operations, on one ordered pair, checked against the identities that tie
// them to the union rather than against a table of coordinates -- so the same
// checks apply to every pair of the grid whatever the shapes are.
template <class Left, class Right>
static void checkBooleanPair(const Left& left, const Right& right) {
    const auto difference = left.template difference<int>(right);
    const auto symmetric = left.template symmetricDifference<int>(right);
    static_assert(std::is_same_v<decltype(difference), const RegionSet>);
    static_assert(std::is_same_v<decltype(symmetric), const RegionSet>);

    // A △ B does not depend on the order, and is the union of the two
    // differences -- which is what makes it need both of them defined.
    CHECK(symmetric == right.template symmetricDifference<int>(left));
    CHECK(symmetric == difference.template unionWith<int>(right.template difference<int>(left)));

    // |A ∖ B| = |A| - |A ∩ B| and |A △ B| = |A ∪ B| - |A ∩ B|, with every area
    // taken on the regularization the operations return: `A ∪ A` is `A`
    // regularized, and the identities hold there and not on the operands as
    // written, a slit having area on neither side.
    const auto both = left.template unionWith<int>(right);
    const int leftArea = left.template unionWith<int>(left).twiceArea();
    const int rightArea = right.template unionWith<int>(right).twiceArea();
    const int shared = leftArea + rightArea - both.twiceArea();
    CHECK(difference.twiceArea() == leftArea - shared);
    CHECK(symmetric.twiceArea() == both.twiceArea() - shared);
}

template <class Left, class... Rights>
static void checkBooleanRow(const Left& left, const Rights&... rights) {
    (checkBooleanPair(left, rights), ...);
}

TEST_CASE("boolean operations: the difference and the symmetric difference cover the grid") {
    // All thirty-six ordered pairs of the six bounded shapes with area, which
    // is the grid `unionWith` covers and which the other two now match. A
    // difference is not symmetric, so none of these pairs can be reached by
    // forwarding: each is a definition of its own.
    // Every edge is axis-parallel or at 45 degrees with an even intercept, so
    // the boundaries cross only at lattice points and an `int` result is the
    // exact one -- which is what lets the identities be checked on the nose.
    const RectangleShape rectangle(Point(0, 0), Point(12, 12));
    const Triangle triangle(Point(0, 0), Point(12, 0), Point(12, 12));
    const Convex convex(std::vector<Point>{Point(6, 0), Point(12, 6), Point(6, 12), Point(0, 6)});
    const PolygonShape polygon = box(4, 0, 8, 12);
    const Region region = fixtures::annulus();
    const RegionSet set = box(0, 0, 5, 5).unionWith<int>(box(7, 7, 12, 12));
    REQUIRE(set.componentCount() == 2);

    const auto row = [&](const auto& left) {
        checkBooleanRow(left, rectangle, triangle, convex, polygon, region, set);
    };
    row(rectangle);
    row(triangle);
    row(convex);
    row(polygon);
    row(region);
    row(set);

    // A convex operand and the polygon spelling of it answer alike, on either
    // side of a difference.
    const PolygonShape spelled = triangle.asPolygon();
    CHECK(rectangle.difference<int>(triangle) == rectangle.difference<int>(spelled));
    CHECK(triangle.difference<int>(rectangle) == spelled.difference<int>(rectangle));

    // Removing the whole of the receiver leaves nothing, and removing nothing
    // leaves its regularization.
    CHECK(triangle.difference<int>(rectangle).empty());
    CHECK(convex.difference<int>(PolygonShape{}) == convex.unionWith<int>(convex));

    // A set operand goes into the one arrangement whole rather than being
    // folded over, which is the same answer removing its components one at a
    // time gives.
    CHECK(region.difference<int>(set) ==
          region.difference<int>(set.component(0)).difference<int>(set.component(1)));
}

TEST_CASE("boolean operations: exact instantiation over ERational operands") {
    const ERegion region(EPolygon({EPoint(0, 0), EPoint(12, 0), EPoint(12, 12), EPoint(0, 12)}),
                         std::vector<EPolygon>{EPolygon({EPoint(4, 4), EPoint(8, 4), EPoint(8, 8),
                                                         EPoint(4, 8)})});
    const EPolygon slab({EPoint(-1, 5), EPoint(13, 5), EPoint(13, 7), EPoint(-1, 7)});

    // The slab reaches out of the region on both sides and cuts its hole in
    // two, so the union gains area outside and keeps two holes inside:
    // 128 + 28 - 16 = 140 in area, with the hole left as [4,8]x[4,5] and
    // [4,8]x[7,8].
    const auto united = region.unionWith<pgl::ERational>(slab);
    REQUIRE(united.componentCount() == 1);
    CHECK(united.component(0).holeCount() == 2);
    CHECK(united.component(0).twiceArea() == pgl::ERational(280));

    // The intersection is the slab clipped to the region: two 4x2 blocks, one
    // either side of the hole.
    const auto met = region.intersection<pgl::ERational>(slab);
    REQUIRE(met.componentCount() == 2);
    pgl::ERational total(0);
    for (const auto& piece : met) {
        total += piece.twiceArea();
    }
    CHECK(total == pgl::ERational(32));
}

// -----------------------------------------------------------------------------
// Exhaustive oracle: every fixture against every axis-aligned rectangle over the
// lattice, for all three operations. Both operands live on the even lattice, so
// membership in either is constant on each cell of the unit grid and the
// (odd, odd) points are one representative per cell. Each answer is then decided
// cell by cell with no shipped code involved -- the region's own point location
// and the rectangle's own containment -- and the total area follows from the
// count.

enum class Op { Union, Intersection, SymmetricDifference };

static const char* name(Op op) {
    switch (op) {
        case Op::Union:
            return " union ";
        case Op::Intersection:
            return " meet ";
        default:
            return " symdiff ";
    }
}

static RegionSet apply(Op op, const Region& a, const PolygonShape& b) {
    switch (op) {
        case Op::Union:
            return a.unionWith<int>(b);
        case Op::Intersection:
            return a.intersection<int>(b);
        default:
            return a.symmetricDifference<int>(b);
    }
}

static bool expected(Op op, bool inA, bool inB) {
    switch (op) {
        case Op::Union:
            return inA || inB;
        case Op::Intersection:
            return inA && inB;
        default:
            return inA != inB;
    }
}

// One (odd, odd) point per unit cell, over a box holding both operands: the
// fixtures live in [0, 12] and the rectangles below in [-2, 14], and a union
// reaches everything either of them covers.
static std::vector<Point> cellRepresentatives() {
    std::vector<Point> probes;
    for (int x = -3; x <= 15; x += 2) {
        for (int y = -3; y <= 15; y += 2) {
            probes.emplace_back(x, y);
        }
    }
    return probes;
}

// Everything asked of one answer: it covers exactly the cells the oracle says,
// its total area is the cell count, its pieces are valid regions, and their
// interiors are disjoint. Returns the oracle's cell count.
static int checkAgainstCells(Op op, const std::string& fixture, const Region& region,
                             const PolygonShape& other) {
    INFO(fixture << name(op) << other);
    const auto pieces = apply(op, region, other);

    int cells = 0;
    bool membershipAgrees = true;
    for (const Point& probe : cellRepresentatives()) {
        const bool want = expected(op, region.contains(probe), other.contains(probe));
        bool found = false;
        for (const Region& piece : pieces) {
            found = found || piece.contains(probe);
        }
        membershipAgrees = membershipAgrees && (found == want);
        cells += want ? 1 : 0;
    }
    CHECK(membershipAgrees);

    int twiceArea = 0;
    bool allValid = true;
    for (const Region& piece : pieces) {
        allValid = allValid && piece.isValid();
        twiceArea += piece.twiceArea();
    }
    CHECK(allValid);
    CHECK(twiceArea == 8 * cells);

    bool disjoint = true;
    for (std::size_t i = 0; i < pieces.componentCount(); ++i) {
        for (std::size_t j = i + 1; j < pieces.componentCount(); ++j) {
            disjoint = disjoint && !pieces.component(i).interiorsIntersect(pieces.component(j));
        }
    }
    CHECK(disjoint);
    return cells;
}

TEST_CASE("boolean operations: exhaustive against a unit-cell oracle (rectilinear)") {
    int nonEmpty = 0;
    for (const Op op : {Op::Union, Op::Intersection, Op::SymmetricDifference}) {
        for (const auto& [fixture, region] : fixtures::all()) {
            for (int x0 = -2; x0 <= 10; x0 += 4) {
                for (int x1 = x0 + 2; x1 <= 14; x1 += 4) {
                    for (int y0 = -2; y0 <= 10; y0 += 4) {
                        for (int y1 = y0 + 2; y1 <= 14; y1 += 4) {
                            nonEmpty +=
                                checkAgainstCells(op, fixture, region, box(x0, y0, x1, y1)) > 0 ? 1
                                                                                                : 0;
                        }
                    }
                }
            }
        }
    }
    CHECK(nonEmpty > 1500);
}

// -----------------------------------------------------------------------------
// Inclusion-exclusion. |A ∪ B| + |A ∩ B| = |A| + |B| and
// |A △ B| = |A ∪ B| - |A ∩ B| hold for the regularized operations, since what
// regularization drops has no area. This checks the three answers against each
// other and against the operands with no point location anywhere in it.

TEST_CASE("boolean operations: inclusion-exclusion ties the three answers together") {
    for (const auto& [fixture, region] : fixtures::all()) {
        for (const auto& [otherName, other] : fixtures::all()) {
            for (int dx = -6; dx <= 6; dx += 3) {
                for (int dy = -6; dy <= 6; dy += 3) {
                    const Region shifted = other + Point(dx, dy);
                    INFO(fixture << " against " << otherName << " shifted by " << dx << ", " << dy);

                    const int united = twiceAreaOf(region.unionWith<int>(shifted));
                    const int met = twiceAreaOf(region.intersection<int>(shifted));
                    const int apart = twiceAreaOf(region.symmetricDifference<int>(shifted));

                    CHECK(united + met == region.twiceArea() + shifted.twiceArea());
                    CHECK(apart == united - met);
                    CHECK(twiceAreaOf(region.difference<int>(shifted)) == region.twiceArea() - met);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// The three new operations against the tested difference of increment 10.
//
// Piece-level equality is not available here and should not be expected:
// `A △ B` is the union of the two differences as a point set, but its *pieces*
// can fuse where the differences abut -- two boxes sharing an edge have
// `A ∖ B = A` and `B ∖ A = B`, which the symmetric difference returns as one
// piece. The areas are what carry over, and they pin all three answers.

TEST_CASE("boolean operations: rewritten through the difference") {
    for (const auto& [fixture, region] : fixtures::all()) {
        for (int dx = -6; dx <= 6; dx += 4) {
            for (int dy = -6; dy <= 6; dy += 4) {
                const Region other(box(2 + dx, 2 + dy, 10 + dx, 10 + dy));
                INFO(fixture << " against " << other.outer());

                const int less = twiceAreaOf(region.difference<int>(other));
                const int lessTheOtherWay = twiceAreaOf(other.difference<int>(region));

                // A ∩ B = A ∖ (A ∖ B), so |A ∩ B| = |A| - |A ∖ B|.
                CHECK(twiceAreaOf(region.intersection<int>(other)) == region.twiceArea() - less);
                // A △ B = (A ∖ B) ∪ (B ∖ A), and the two are disjoint.
                CHECK(twiceAreaOf(region.symmetricDifference<int>(other)) == less + lessTheOtherWay);
                // A ∪ B = (A ∖ B) ∪ B.
                CHECK(twiceAreaOf(region.unionWith<int>(other)) == less + other.twiceArea());
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Commutativity and idempotence.

TEST_CASE("boolean operations: the symmetric ones do not depend on the order") {
    for (const auto& [fixture, region] : fixtures::all()) {
        for (const auto& [otherName, other] : fixtures::all()) {
            for (int dx = -6; dx <= 6; dx += 4) {
                const Region shifted = other + Point(dx, dx / 2);
                INFO(fixture << " against " << otherName << " shifted by " << dx);
                CHECK(region.unionWith<int>(shifted) == shifted.unionWith<int>(region));
                CHECK(region.intersection<int>(shifted) == shifted.intersection<int>(region));
                CHECK(region.symmetricDifference<int>(shifted) == shifted.symmetricDifference<int>(region));
            }
        }
    }
}

TEST_CASE("boolean operations: a shape against itself gives its regularization") {
    for (const auto& [fixture, region] : fixtures::all()) {
        INFO(fixture);
        const auto self = regularized(region);
        CHECK(region.unionWith<int>(region) == self);
        CHECK(region.intersection<int>(region) == self);
        CHECK(region.symmetricDifference<int>(region).empty());

        // Regularization never changes the area, only the point set: what it
        // drops has none. It can change the *number* of pieces, though, and on
        // two of these fixtures it does -- `band` and `pinched` are connected
        // only through material with no area, so `closure(A°)` comes apart.
        CHECK(twiceAreaOf(self) == region.twiceArea());
        CHECK(self.componentCount() == (fixture == "band" || fixture == "pinched" ? 2u : 1u));
    }
}

// -----------------------------------------------------------------------------
// Shear invariance. Every one of these operations commutes with an affine
// bijection, and (x, y) -> (x, y + kx) maps the lattice onto itself, so the
// exact rectilinear answers above become ground truth for slanted edges and
// off-axis crossings.

static Point shear(const Point& p, int k) {
    return Point(p.x(), p.y() + k * p.x());
}

static PolygonShape shear(const PolygonShape& polygon, int k) {
    std::vector<Point> vertices;
    for (const Point& p : polygon) {
        vertices.push_back(shear(p, k));
    }
    return PolygonShape(vertices);
}

static Region shear(const Region& region, int k) {
    std::vector<PolygonShape> holes;
    for (const PolygonShape& hole : region.holes()) {
        holes.push_back(shear(hole, k));
    }
    return Region(shear(region.outer(), k), holes);
}

TEST_CASE("boolean operations: shear invariance carries the answers off the axes") {
    for (const int k : {1, -2}) {
        for (const Op op : {Op::Union, Op::Intersection, Op::SymmetricDifference}) {
            for (const auto& [fixture, region] : fixtures::all()) {
                for (int x0 = -2; x0 <= 10; x0 += 4) {
                    for (int y0 = -2; y0 <= 10; y0 += 4) {
                        const PolygonShape other = box(x0, y0, x0 + 6, y0 + 6);
                        INFO(fixture << name(op) << other << " sheared by " << k);
                        std::vector<Region> want;
                        for (const Region& piece : apply(op, region, other)) {
                            want.push_back(shear(piece, k));
                        }
                        CHECK(apply(op, shear(region, k), shear(other, k)) == RegionSet(want));
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// General position. Off the lattice the boundaries cross at rational points, so
// the result type has to be exact; the oracle is then any probe avoiding both
// boundaries, since the boundary of any of these results is contained in their
// union.

TEST_CASE("boolean operations: probe oracle in general position") {
    const std::vector<EPolygon> shapes{
        EPolygon({EPoint(0, 0), EPoint(11, 0), EPoint(11, 11), EPoint(0, 11)}),
        EPolygon({EPoint(1, 0), EPoint(11, 4), EPoint(6, 11), EPoint(0, 6)}),
        EPolygon({EPoint(0, 0), EPoint(11, 3), EPoint(4, 5), EPoint(11, 8), EPoint(0, 11)}),
    };
    const std::vector<ERegion> targets{
        ERegion(shapes[0], std::vector<EPolygon>{EPolygon({EPoint(3, 3), EPoint(7, 2),
                                                           EPoint(8, 7), EPoint(2, 8)})}),
        ERegion(shapes[1]),
        ERegion(shapes[2], std::vector<EPolygon>{EPolygon({EPoint(1, 1), EPoint(3, 1),
                                                           EPoint(3, 9), EPoint(1, 9)})}),
    };

    int checks = 0;
    for (const ERegion& target : targets) {
        for (const EPolygon& shape : shapes) {
            for (int dx = -3; dx <= 3; dx += 3) {
                for (int dy = -3; dy <= 3; dy += 3) {
                    const EPolygon shifted = shape + EPoint(dx, dy);
                    INFO(target.outer() << " against " << shifted);

                    const auto united = target.unionWith<pgl::ERational>(shifted);
                    const auto met = target.intersection<pgl::ERational>(shifted);
                    const auto apart = target.symmetricDifference<pgl::ERational>(shifted);

                    pgl::ERational unitedArea(0), metArea(0), apartArea(0);
                    bool allValid = true;
                    for (const auto& piece : united) {
                        allValid = allValid && piece.isValid();
                        unitedArea += piece.twiceArea();
                    }
                    for (const auto& piece : met) {
                        allValid = allValid && piece.isValid();
                        metArea += piece.twiceArea();
                    }
                    for (const auto& piece : apart) {
                        allValid = allValid && piece.isValid();
                        apartArea += piece.twiceArea();
                    }
                    CHECK(allValid);
                    CHECK(unitedArea + metArea == target.twiceArea() + shifted.twiceArea());
                    CHECK(apartArea == unitedArea - metArea);

                    bool agrees = true;
                    for (int x = -4; x <= 15; ++x) {
                        for (int y = -4; y <= 15; ++y) {
                            const EPoint probe(pgl::ERational(2 * x + 1, 2),
                                               pgl::ERational(2 * y + 1, 2));
                            if (target.boundaryContains(probe) || shifted.boundaryContains(probe)) {
                                continue;
                            }
                            const bool inA = target.contains(probe);
                            const bool inB = shifted.contains(probe);
                            bool inUnion = false, inMeet = false, inApart = false;
                            for (const auto& piece : united) {
                                inUnion = inUnion || piece.contains(probe);
                            }
                            for (const auto& piece : met) {
                                inMeet = inMeet || piece.contains(probe);
                            }
                            for (const auto& piece : apart) {
                                inApart = inApart || piece.contains(probe);
                            }
                            agrees = agrees && inUnion == (inA || inB) && inMeet == (inA && inB) &&
                                     inApart == (inA != inB);
                            checks += 3;
                        }
                    }
                    CHECK(agrees);
                }
            }
        }
    }
    CHECK(checks > 20000);
}
