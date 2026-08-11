#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using Point = pgl::Point<int>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using PolygonShape = pgl::Polygon<Point>;
using PolylineShape = pgl::Polyline<Point>;
using HalfplaneShape = pgl::Halfplane<Point>;
using RegionIntersection = pgl::HalfplaneIntersection<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

// The *literal* intersection of two region-valued operands: `A ∩ B` itself,
// not the `closure(A° ∩ B°)` that `regularizedIntersection` answers with. The
// two run the same cell engine and differ only in what they do with the cells
// that carry no area: the regularized one drops them, and this one reports them
// as the pieces they are.
//
// So the contract has three kinds of piece, and every test below is about one
// of them or about how they fit together:
//
//   * an **area** piece, a `PolygonWithHoles` -- a region rather than a polygon
//     because a component of `A ∩ B` keeps a hole of a holed operand;
//   * a **strand**, a `Polyline` -- a stretch of boundary the two operands share
//     with no area on either side of it, which closes up into a ring whenever
//     an operand's hole is met exactly;
//   * an **isolated point**, where the two boundaries only touch.
//
// Whatever the shape of the answer, its area is the regularized answer's and
// its point set is `A ∩ B`; the two oracles at the bottom check exactly that
// across the whole operand grid.

// -----------------------------------------------------------------------------
// Fixtures, on the even lattice so the oracle can probe integer points.

static PolygonShape square(int lo, int hi) {
    return PolygonShape({Point(lo, lo), Point(hi, lo), Point(hi, hi), Point(lo, hi)});
}

static PolygonShape box(int x0, int y0, int x1, int y1) {
    return PolygonShape({Point(x0, y0), Point(x1, y0), Point(x1, y1), Point(x0, y1)});
}

namespace fixtures {

static Region plain() { return Region(square(0, 12)); }

static Region annulus() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(4, 4, 8, 8)});
}

// A hole spanning the square: the region is two slabs joined only by the slits
// along the left and right edges.
static Region band() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(0, 4, 12, 8)});
}

// A hole sharing a stretch of the outer ring, so the region is an L with a slit.
static Region notched() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(6, 6, 12, 12)});
}

// Two holes meeting at a single point, pinching the region shut there.
static Region pinched() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(0, 0, 6, 6), box(6, 6, 12, 12)});
}

static std::vector<std::pair<std::string, Region>> all() {
    return {{"plain", plain()},
            {"annulus", annulus()},
            {"band", band()},
            {"notched", notched()},
            {"pinched", pinched()}};
}

}  // namespace fixtures

// -----------------------------------------------------------------------------
// Reading an answer

template <class Pieces>
static int areaPieceCount(const Pieces& pieces) {
    int count = 0;
    for (const auto& piece : pieces) {
        count += std::holds_alternative<Region>(piece) ? 1 : 0;
    }
    return count;
}

template <class Pieces>
static int strandCount(const Pieces& pieces) {
    int count = 0;
    for (const auto& piece : pieces) {
        count += std::holds_alternative<PolylineShape>(piece) ? 1 : 0;
    }
    return count;
}

template <class Pieces>
static int pointCount(const Pieces& pieces) {
    int count = 0;
    for (const auto& piece : pieces) {
        count += std::holds_alternative<Point>(piece) ? 1 : 0;
    }
    return count;
}

template <class Pieces>
static int twiceAreaOf(const Pieces& pieces) {
    int total = 0;
    for (const auto& piece : pieces) {
        if (const auto* area = std::get_if<Region>(&piece)) {
            total += area->twiceArea();
        }
    }
    return total;
}

template <class Pieces>
static bool covers(const Pieces& pieces, const Point& probe) {
    for (const auto& piece : pieces) {
        if (std::visit([&probe](const auto& shape) { return shape.contains(probe); }, piece)) {
            return true;
        }
    }
    return false;
}

static int twiceAreaOf(const RegionSet& pieces) {
    int total = 0;
    for (const Region& piece : pieces) {
        total += piece.twiceArea();
    }
    return total;
}

// -----------------------------------------------------------------------------
// The area pieces

TEST_CASE("intersection: a region met by a shape covering it comes back whole") {
    for (const auto& [fixture, region] : fixtures::all()) {
        INFO(fixture);
        const auto pieces = region.intersection<int>(box(-5, -5, 20, 20));
        CHECK(areaPieceCount(pieces) == region.difference<int>(PolygonShape{}).componentCount());
        CHECK(twiceAreaOf(pieces) == region.twiceArea());
    }
}

TEST_CASE("intersection: a hole of an operand is a hole of the result") {
    const auto pieces = fixtures::annulus().intersection<int>(box(1, 1, 11, 11));

    REQUIRE(pieces.size() == 1);
    const Region& piece = std::get<Region>(pieces.front());
    REQUIRE(piece.holeCount() == 1);
    CHECK(piece.hole(0) == box(4, 4, 8, 8));
    CHECK(piece.outer() == box(1, 1, 11, 11));
}

TEST_CASE("intersection: the holes of both operands are holes of the result") {
    const Region a(square(0, 12), std::vector<PolygonShape>{box(2, 2, 5, 5)});
    const Region b(square(0, 12), std::vector<PolygonShape>{box(7, 7, 10, 10)});
    const auto pieces = a.intersection<int>(b);

    REQUIRE(pieces.size() == 1);
    const Region& piece = std::get<Region>(pieces.front());
    REQUIRE(piece.holeCount() == 2);
    CHECK(piece.hole(0) == box(2, 2, 5, 5));
    CHECK(piece.hole(1) == box(7, 7, 10, 10));
    CHECK(piece.twiceArea() == 2 * (144 - 9 - 9));
}

TEST_CASE("intersection: a cut can split the area into two pieces") {
    const auto pieces = fixtures::annulus().intersection<int>(box(0, 4, 12, 8));

    CHECK(areaPieceCount(pieces) == 2);
    CHECK(twiceAreaOf(pieces) == 2 * (48 - 16));
}

TEST_CASE("intersection: no piece carries a vertex in the middle of a straight run") {
    // The cut crosses the outer ring at (0, 6) and (12, 6), which the
    // arrangement makes vertices of the answer's boundary even where it runs
    // straight through. They are dropped, exactly as in a regularized ring.
    const auto pieces = Region(square(0, 12)).intersection<int>(box(-2, 6, 14, 20));

    REQUIRE(pieces.size() == 1);
    CHECK(std::get<Region>(pieces.front()).outer() == box(0, 6, 12, 12));
}

// -----------------------------------------------------------------------------
// The strands, which are what regularization drops

TEST_CASE("intersection: a shape filling a hole exactly meets the region in its rim") {
    const auto pieces = fixtures::annulus().intersection<int>(box(4, 4, 8, 8));

    REQUIRE(pieces.size() == 1);
    CHECK(strandCount(pieces) == 1);
    // A strand that closes up repeats its first vertex last.
    const PolylineShape& rim = std::get<PolylineShape>(pieces.front());
    REQUIRE(rim.size() == 5);
    CHECK(rim[0] == rim[4]);
    CHECK(PolygonShape(std::vector<Point>(rim.begin(), rim.begin() + 4)) == box(4, 4, 8, 8));

    // The regularized answer has nothing at all to report here.
    CHECK(fixtures::annulus().regularizedIntersection<int>(box(4, 4, 8, 8)).empty());
}

TEST_CASE("intersection: operands sharing a stretch of boundary meet along it") {
    const auto pieces = Region(square(0, 12)).intersection<int>(box(12, 2, 20, 8));

    REQUIRE(pieces.size() == 1);
    CHECK(std::get<PolylineShape>(pieces.front()) ==
          PolylineShape(std::vector<Point>{Point(12, 2), Point(12, 8)}));
    CHECK(twiceAreaOf(pieces) == 0);
}

TEST_CASE("intersection: operands touching at a corner meet in that point") {
    const auto pieces = Region(square(0, 12)).intersection<int>(box(12, 12, 20, 20));

    REQUIRE(pieces.size() == 1);
    CHECK(std::get<Point>(pieces.front()) == Point(12, 12));
    CHECK(pointCount(pieces) == 1);
}

TEST_CASE("intersection: a strand hangs off an area piece as a piece of its own") {
    // One square of the set meets the box in an area, the other only along the
    // box's right edge; the two contacts meet at (4, 4), so the answer is a
    // region with a whisker growing out of one corner. A whisker fits in no
    // region, which is why it is a piece of its own -- as it is for two
    // polygons.
    const RegionSet set(std::vector<Region>{Region(box(2, 0, 6, 4)), Region(box(4, 4, 8, 10))});
    const auto pieces = set.intersection<int>(box(0, 0, 4, 10));

    CHECK(areaPieceCount(pieces) == 1);
    CHECK(strandCount(pieces) == 1);
    CHECK(pointCount(pieces) == 0);
    CHECK(twiceAreaOf(pieces) == 2 * 8);
    CHECK(std::get<Region>(pieces.back()).outer() == box(2, 0, 4, 4));
    CHECK(std::get<PolylineShape>(pieces.front()) ==
          PolylineShape(std::vector<Point>{Point(4, 4), Point(4, 10)}));
    CHECK(covers(pieces, Point(4, 7)));
}

TEST_CASE("intersection: a slit survives where regularization removes it") {
    // The band's two slabs are joined only by the slits up the left and right
    // edges. Meeting the band with its own outer square keeps them.
    const auto pieces = fixtures::band().intersection<int>(square(0, 12));

    CHECK(areaPieceCount(pieces) == 2);
    CHECK(strandCount(pieces) == 2);
    CHECK(covers(pieces, Point(0, 6)));
    CHECK(covers(pieces, Point(12, 6)));
    // The regularized answer keeps the two slabs and nothing else.
    const auto regular = fixtures::band().regularizedIntersection<int>(square(0, 12));
    CHECK(regular.componentCount() == 2);
    CHECK(twiceAreaOf(pieces) == twiceAreaOf(regular));
}

TEST_CASE("intersection: operands that miss each other answer with nothing") {
    CHECK(Region(square(0, 10)).intersection<int>(box(20, 20, 30, 30)).empty());
    CHECK(Region(square(0, 10)).intersection<int>(RegionSet{}).empty());
    CHECK(RegionSet{}.intersection<int>(Region(square(0, 10))).empty());
    // Lying strictly inside a hole is missing the region too.
    CHECK(fixtures::annulus().intersection<int>(box(5, 5, 7, 7)).empty());
}

// -----------------------------------------------------------------------------
// The operand grid

TEST_CASE("intersection: every operand with area, in both directions") {
    const Region region = fixtures::annulus();
    const RegionSet set(std::vector<Region>{Region(box(0, 0, 6, 6)), Region(box(8, 8, 14, 14))});

    const auto sameBothWays = [](const auto& a, const auto& b) {
        const auto forward = a.template intersection<int>(b);
        const auto backward = b.template intersection<int>(a);
        CHECK(std::is_same_v<decltype(forward), decltype(backward)>);
        CHECK(forward.size() == backward.size());
        CHECK(twiceAreaOf(forward) == twiceAreaOf(backward));
        // And the areas are the ones the regularized operation reports.
        CHECK(twiceAreaOf(forward) == twiceAreaOf(a.template regularizedIntersection<int>(b)));
    };

    sameBothWays(region, box(2, 2, 10, 10));
    sameBothWays(region, RectangleShape(Point(2, 2), Point(10, 10)));
    sameBothWays(region, Triangle(Point(0, 0), Point(12, 0), Point(0, 12)));
    sameBothWays(region, Convex(std::vector<Point>{Point(2, 2), Point(10, 2), Point(10, 10)}));
    sameBothWays(region, HalfplaneShape(Point(0, 6), Point(12, 6)));
    sameBothWays(region, RegionIntersection(RectangleShape(Point(2, 2), Point(10, 10))));
    sameBothWays(region, fixtures::notched());
    sameBothWays(region, set);

    sameBothWays(set, box(2, 2, 10, 10));
    sameBothWays(set, RectangleShape(Point(2, 2), Point(10, 10)));
    sameBothWays(set, Triangle(Point(0, 0), Point(12, 0), Point(0, 12)));
    sameBothWays(set, Convex(std::vector<Point>{Point(2, 2), Point(10, 2), Point(10, 10)}));
    sameBothWays(set, HalfplaneShape(Point(0, 6), Point(12, 6)));
    sameBothWays(set, RegionIntersection(RectangleShape(Point(2, 2), Point(10, 10))));
    sameBothWays(set, set);
}

TEST_CASE("intersection: an unbounded operand collapsing onto a line still reports the contact") {
    // The clip of this half-plane intersection against the region's box has no
    // interior at all: it is the segment the two constraints leave. The
    // regularized answer is empty and this one is the contact itself.
    RegionIntersection slab;
    slab.insert(HalfplaneShape(Point(0, 6), Point(12, 6)));
    slab.insert(HalfplaneShape(Point(12, 6), Point(0, 6)));
    REQUIRE(slab.isDegenerate());

    const auto pieces = Region(square(0, 12)).intersection<int>(slab);
    CHECK(strandCount(pieces) == 1);
    CHECK(covers(pieces, Point(6, 6)));
    CHECK(twiceAreaOf(pieces) == 0);
    CHECK(Region(square(0, 12)).regularizedIntersection<int>(slab).empty());
}

TEST_CASE("intersection: exact over an integral result type") {
    // The crossings are integral here, and the arrangement is rational whatever
    // the result type is, so nothing is truncated on the way out.
    const Region region(square(0, 10));
    const auto pieces = region.intersection<int>(PolygonShape({Point(0, 0), Point(10, 10), Point(0, 10)}));

    REQUIRE(pieces.size() == 1);
    CHECK(std::get<Region>(pieces.front()).outer() ==
          PolygonShape({Point(0, 0), Point(10, 10), Point(0, 10)}));
}

TEST_CASE("intersection: instantiates over exact rational operands") {
    const pgl::EPolygonWithHoles region(
        pgl::EPolygon({pgl::EPoint(0, 0), pgl::EPoint(6, 0), pgl::EPoint(6, 6), pgl::EPoint(0, 6)}),
        std::vector<pgl::EPolygon>{pgl::EPolygon(
            {pgl::EPoint(2, 2), pgl::EPoint(4, 2), pgl::EPoint(4, 4), pgl::EPoint(2, 4)})});
    const auto pieces = region.intersection<pgl::ERational>(
        pgl::ERectangle(pgl::EPoint(1, 1), pgl::EPoint(5, 5)));

    REQUIRE(pieces.size() == 1);
    const auto& piece = std::get<pgl::EPolygonWithHoles>(pieces.front());
    CHECK(piece.holeCount() == 1);
    CHECK(piece.twiceArea() == pgl::ERational(2 * (16 - 4)));
}

// -----------------------------------------------------------------------------
// Oracles

TEST_CASE("intersection: the pieces cover exactly A n B") {
    const RegionSet set(std::vector<Region>{Region(box(0, 0, 6, 6)), Region(box(6, 6, 14, 14))});
    const std::vector<std::pair<std::string, Region>> regions = fixtures::all();

    for (const auto& [fixture, region] : regions) {
        for (const auto& [otherName, other] : regions) {
            INFO(fixture << " n " << otherName);
            const auto pieces = region.intersection<int>(other);
            for (int x = -1; x <= 13; ++x) {
                for (int y = -1; y <= 13; ++y) {
                    const Point probe(x, y);
                    CHECK(covers(pieces, probe) == (region.contains(probe) && other.contains(probe)));
                }
            }
        }
        INFO(fixture << " n set");
        const auto against = region.intersection<int>(set);
        for (int x = -1; x <= 15; ++x) {
            for (int y = -1; y <= 15; ++y) {
                const Point probe(x, y);
                CHECK(covers(against, probe) == (region.contains(probe) && set.contains(probe)));
            }
        }
    }
}

TEST_CASE("intersection: the area pieces are the regularized answer") {
    const std::vector<std::pair<std::string, Region>> regions = fixtures::all();
    for (const auto& [fixture, region] : regions) {
        for (const auto& [otherName, other] : regions) {
            INFO(fixture << " n " << otherName);
            const auto pieces = region.intersection<int>(other);
            const auto regular = region.regularizedIntersection<int>(other);
            CHECK(areaPieceCount(pieces) == static_cast<int>(regular.componentCount()));
            CHECK(twiceAreaOf(pieces) == twiceAreaOf(regular));
            for (std::size_t i = 0, seen = 0; i < pieces.size(); ++i) {
                if (const auto* area = std::get_if<Region>(&pieces[i])) {
                    CHECK(*area == regular.component(seen++));
                }
            }
        }
    }
}
