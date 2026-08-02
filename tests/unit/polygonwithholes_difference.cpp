#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <string>
#include <vector>

using Point = pgl::Point<int>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

using EPoint = pgl::EPoint;
using EPolygon = pgl::EPolygon;
using ERegion = pgl::EPolygonWithHoles;

// The regularized set difference `A ∖ B` of two closed polygonal shapes, the
// first boolean operation in the library and the first construction whose
// result genuinely needs a region: removing a polygon from the middle of
// another one leaves a hole, and no other shape can say so.
//
// `A ∩ B` is the case that does *not* need one. Both operands are closed with a
// connected complement, so a Jordan curve in the intersection bounds a disk in
// each of them and hence in the intersection: it can carry no hole, and
// `Polygon::intersection` loses nothing by returning plain polygons.
//
// What is computed is `closure(A° ∖ B)`. The regularization is what makes the
// answer a set of regions: a stretch of `∂A` that `B` touches without covering,
// and a slit of `A` (which has no area at all), have nowhere to go in a
// `vector<PolygonWithHoles>` and are dropped.

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

// A square with a hole in the middle: the shape the operation exists for.
static Region annulus() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(4, 4, 8, 8)});
}

// A hole spanning the square, so the region's *domain* is two slabs joined only
// by the slits along the left and right edges.
static Region band() {
    return Region(square(0, 12), std::vector<PolygonShape>{box(0, 4, 12, 8)});
}

// A hole sharing a stretch of the outer ring: the region is an L, and the shared
// stretch carries no boundary of the difference at all.
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

// -----------------------------------------------------------------------------
// Named cases

TEST_CASE("Polygon difference: a polygon removed from the middle leaves a hole") {
    const PolygonShape outer = square(0, 10);
    const PolygonShape inner = box(3, 3, 7, 7);
    const auto pieces = outer.difference(inner);

    REQUIRE(pieces.size() == 1);
    CHECK(pieces[0].outer() == outer);
    REQUIRE(pieces[0].holeCount() == 1);
    CHECK(pieces[0].hole(0) == inner);
    CHECK(pieces[0].twiceArea() == 200 - 32);
    CHECK(pieces[0].isValid());
}

TEST_CASE("Polygon difference: a bite from the side leaves no hole") {
    const auto pieces = square(0, 10).difference(box(3, -1, 7, 4));

    REQUIRE(pieces.size() == 1);
    CHECK(pieces[0].holeCount() == 0);
    CHECK(pieces[0].outer() == PolygonShape({Point(0, 0), Point(3, 0), Point(3, 4), Point(7, 4),
                                        Point(7, 0), Point(10, 0), Point(10, 10), Point(0, 10)}));
}

TEST_CASE("Polygon difference: a band splits the polygon in two") {
    const auto pieces = square(0, 10).difference(box(-1, 4, 11, 6));

    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0] == Region(box(0, 0, 10, 4)));
    CHECK(pieces[1] == Region(box(0, 6, 10, 10)));
}

TEST_CASE("Polygon difference: degenerate outcomes") {
    const PolygonShape unit = square(0, 10);

    SUBCASE("a remover that misses gives the polygon back") {
        const auto pieces = unit.difference(box(50, 50, 60, 60));
        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Region(unit));
    }
    SUBCASE("a remover abutting from outside removes nothing") {
        const auto pieces = unit.difference(box(10, 2, 14, 8));
        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Region(unit));
    }
    SUBCASE("a remover meeting at one corner removes nothing") {
        const auto pieces = unit.difference(PolygonShape({Point(10, 10), Point(14, 10), Point(14, 14)}));
        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Region(unit));
    }
    SUBCASE("removing the polygon itself leaves nothing") {
        CHECK(unit.difference(unit).empty());
    }
    SUBCASE("a remover swallowing the polygon leaves nothing") {
        CHECK(unit.difference(box(-1, -1, 11, 11)).empty());
    }
    SUBCASE("a polygon with no area has nothing to lose") {
        CHECK(PolygonShape({Point(0, 0), Point(4, 0), Point(2, 0)}).difference(unit).empty());
    }
    SUBCASE("a remover with no area removes nothing") {
        const auto pieces = unit.difference(PolygonShape({Point(2, 2), Point(8, 2)}));
        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Region(unit));
        CHECK(unit.difference(PolygonShape{})[0] == Region(unit));
    }
}

TEST_CASE("Polygon difference: a hole may touch the outer ring it is cut from") {
    // The diamond meets the left edge at (0,5) and nowhere else, so the boundary
    // of the difference is a single closed walk through that point twice. It has
    // to come back split into the two rings that touch there.
    const PolygonShape diamond({Point(0, 5), Point(3, 2), Point(6, 5), Point(3, 8)});
    const auto pieces = square(0, 10).difference(diamond);

    REQUIRE(pieces.size() == 1);
    CHECK(pieces[0].outer() == square(0, 10));
    REQUIRE(pieces[0].holeCount() == 1);
    CHECK(pieces[0].hole(0) == diamond);
    CHECK(pieces[0].isValid());
    CHECK(pieces[0].twiceArea() == 200 - 36);
}

TEST_CASE("Polygon difference: pieces meeting at a point come back separately") {
    // The diamond touches all four edge midpoints, leaving four corner triangles
    // that meet in pairs at those points. A region may not have a self-touching
    // outer ring, so each is its own piece.
    const auto pieces =
        square(0, 10).difference(PolygonShape({Point(0, 5), Point(5, 0), Point(10, 5), Point(5, 10)}));

    REQUIRE(pieces.size() == 4);
    for (const Region& piece : pieces) {
        CHECK(piece.holeCount() == 0);
        CHECK(piece.twiceArea() == 25);
        CHECK(piece.isValid());
    }
}

TEST_CASE("PolygonWithHoles difference: a region loses material around its holes") {
    const Region annulus = fixtures::annulus();

    SUBCASE("a corner bite keeps the hole") {
        const auto pieces = annulus.difference(box(-1, -1, 2, 2));
        REQUIRE(pieces.size() == 1);
        REQUIRE(pieces[0].holeCount() == 1);
        CHECK(pieces[0].hole(0) == box(4, 4, 8, 8));
        CHECK(pieces[0].twiceArea() == annulus.twiceArea() - 8);
    }
    SUBCASE("a band through the hole splits the region and absorbs the hole") {
        const auto pieces = annulus.difference(box(-1, 5, 13, 7));
        REQUIRE(pieces.size() == 2);
        for (const Region& piece : pieces) {
            CHECK(piece.holeCount() == 0);
            CHECK(piece.isValid());
        }
        CHECK(pieces[0].twiceArea() + pieces[1].twiceArea() == 288 - 32 - 32);
    }
}

TEST_CASE("PolygonWithHoles difference: a slit has no area and never survives") {
    // The hole shares the left edge of the outer ring, so the region is an L
    // whose boundary runs along the shared stretch once, not twice.
    const Region notched = fixtures::notched();
    const auto pieces = notched.difference(box(50, 50, 60, 60));

    REQUIRE(pieces.size() == 1);
    CHECK(pieces[0].holeCount() == 0);
    CHECK(pieces[0].twiceArea() == notched.twiceArea());
    CHECK(pieces[0].outer() == PolygonShape({Point(0, 0), Point(12, 0), Point(12, 6), Point(6, 6),
                                        Point(6, 12), Point(0, 12)}));
}

TEST_CASE("PolygonWithHoles difference: a pinched region comes back in pieces") {
    const auto pieces = fixtures::pinched().difference(box(50, 50, 60, 60));

    REQUIRE(pieces.size() == 2);
    for (const Region& piece : pieces) {
        CHECK(piece.holeCount() == 0);
        CHECK(piece.isValid());
    }
    CHECK(pieces[0].twiceArea() + pieces[1].twiceArea() == fixtures::pinched().twiceArea());
}

TEST_CASE("difference: an island stranded in a lake is a piece of its own") {
    // Plugging the gap of the C-shaped hole cuts its cavity off from the rest of
    // the region. The result is the outer square with the whole C-plus-plug as a
    // hole, and the cavity as a second, nested-looking piece.
    const auto pieces = fixtures::cavity().difference(box(2, 4, 4, 8));

    REQUIRE(pieces.size() == 2);
    const Region* island = pieces[0].twiceArea() < pieces[1].twiceArea() ? &pieces[0] : &pieces[1];
    const Region* shell = island == &pieces[0] ? &pieces[1] : &pieces[0];

    CHECK(island->holeCount() == 0);
    CHECK(island->outer() == box(4, 4, 8, 8));
    REQUIRE(shell->holeCount() == 1);
    CHECK(shell->hole(0) == box(2, 2, 10, 10));
    CHECK(shell->contains(shell->hole(0)[0]));
    // They are two regions, not one nested in the other: a flat list is all the
    // library says, and their interiors are disjoint.
    CHECK_FALSE(island->interiorsIntersect(*shell));
}

TEST_CASE("difference: the remover's own hole leaves material behind") {
    const Region ring(box(2, 2, 8, 8), std::vector<PolygonShape>{box(4, 4, 6, 6)});
    const auto pieces = square(0, 10).difference(ring);

    REQUIRE(pieces.size() == 2);
    const Region* island = pieces[0].twiceArea() < pieces[1].twiceArea() ? &pieces[0] : &pieces[1];
    CHECK(island->outer() == box(4, 4, 6, 6));
    CHECK(island->holeCount() == 0);
}

TEST_CASE("difference: convex, triangle and rectangle operands") {
    const PolygonShape unit = square(0, 10);
    const RectangleShape rectangle(Point(3, 3), Point(7, 7));
    const auto viaRectangle = unit.difference(rectangle);
    const auto viaPolygon = unit.difference(box(3, 3, 7, 7));
    CHECK(viaRectangle == viaPolygon);

    const Triangle triangle(Point(0, 0), Point(10, 0), Point(0, 10));
    CHECK(unit.difference(triangle) == unit.difference(PolygonShape({Point(0, 0), Point(10, 0),
                                                                Point(0, 10)})));

    const Convex hull(std::vector<Point>{Point(2, 2), Point(8, 2), Point(8, 8), Point(2, 8)});
    CHECK(unit.difference(hull) == unit.difference(box(2, 2, 8, 8)));

    CHECK(fixtures::annulus().difference(rectangle) ==
          fixtures::annulus().difference(box(3, 3, 7, 7)));
}

TEST_CASE("difference: crossings off the lattice need an exact result type") {
    const PolygonShape a = square(0, 7);
    const PolygonShape b({Point(3, -1), Point(9, 3), Point(3, 8)});
    const auto pieces = a.difference<pgl::ERational>(b);

    // Three pieces: the slab left of x = 3, and the two corners the slanted
    // triangle leaves on the right.
    REQUIRE(pieces.size() == 3);
    pgl::ERational total(0);
    for (const auto& piece : pieces) {
        CHECK(piece.isValid());
        total += piece.twiceArea();
    }
    // The square's 98 minus the 453/10 the triangle takes out of it: clipping
    // the triangle to the square gives (3,0), (9/2,0), (7,5/3), (7,14/3),
    // (21/5,7), (3,7), whose shoelace is 453/10.
    CHECK(total == pgl::ERational(527, 10));

    bool sawFraction = false;
    for (const auto& piece : pieces) {
        for (const EPoint& vertex : piece.outer()) {
            if (vertex.x().denominator() != 1 || vertex.y().denominator() != 1) {
                sawFraction = true;
            }
        }
    }
    CHECK(sawFraction);
}

TEST_CASE("difference: exact instantiation over ERational operands") {
    const ERegion region(EPolygon({EPoint(0, 0), EPoint(12, 0), EPoint(12, 12), EPoint(0, 12)}),
                         std::vector<EPolygon>{EPolygon({EPoint(4, 4), EPoint(8, 4), EPoint(8, 8),
                                                         EPoint(4, 8)})});
    const auto pieces = region.difference<pgl::ERational>(
        EPolygon({EPoint(-1, 5), EPoint(13, 5), EPoint(13, 7), EPoint(-1, 7)}));
    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0].twiceArea() + pieces[1].twiceArea() == region.twiceArea() - 32);
}

// -----------------------------------------------------------------------------
// Exhaustive oracle: every fixture against every axis-aligned rectangle over the
// lattice. Both operands live on the even lattice, so membership in either is
// constant on each cell of the unit grid and the (odd, odd) points are one
// representative per cell. The difference is then decided cell by cell with no
// shipped code involved -- the region's own point location and the rectangle's
// own containment -- and the total area follows from the count.

// The cell representatives: one (odd, odd) point per unit cell of the box the
// fixtures live in. No fixture boundary passes through one, so each classifies
// its whole cell.
static std::vector<Point> cellRepresentatives() {
    std::vector<Point> probes;
    for (int x = 1; x < 12; x += 2) {
        for (int y = 1; y < 12; y += 2) {
            probes.emplace_back(x, y);
        }
    }
    return probes;
}

// Everything asked of one answer: it covers exactly the cells the oracle says,
// its total area is the cell count, its pieces are valid regions, and their
// interiors are disjoint. Returns the oracle's cell count so the caller can
// check it saw a non-trivial one.
static int checkAgainstCells(const std::string& name, const Region& region,
                             const PolygonShape& remover) {
    INFO(name << " minus " << remover);
    const auto pieces = region.difference(remover);

    int cells = 0;
    bool membershipAgrees = true;
    for (const Point& probe : cellRepresentatives()) {
        const bool expected = region.contains(probe) && !remover.contains(probe);
        bool found = false;
        for (const Region& piece : pieces) {
            found = found || piece.contains(probe);
        }
        membershipAgrees = membershipAgrees && (found == expected);
        cells += expected ? 1 : 0;
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
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        for (std::size_t j = i + 1; j < pieces.size(); ++j) {
            disjoint = disjoint && !pieces[i].interiorsIntersect(pieces[j]);
        }
    }
    CHECK(disjoint);
    return cells;
}

TEST_CASE("difference: exhaustive against a unit-cell oracle (rectilinear)") {
    int nonEmpty = 0;
    for (const auto& [name, region] : fixtures::all()) {
        for (int x0 = -2; x0 <= 10; x0 += 4) {
            for (int x1 = x0 + 2; x1 <= 14; x1 += 4) {
                for (int y0 = -2; y0 <= 10; y0 += 4) {
                    for (int y1 = y0 + 2; y1 <= 14; y1 += 4) {
                        nonEmpty += checkAgainstCells(name, region, box(x0, y0, x1, y1)) > 0 ? 1 : 0;
                    }
                }
            }
        }
    }
    CHECK(nonEmpty > 500);
}

// -----------------------------------------------------------------------------
// Shear invariance. The difference commutes with any affine bijection, and
// (x, y) -> (x, y + kx) maps the lattice onto itself, so the exact rectilinear
// answers above become ground truth for slanted edges and off-axis crossings.

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

TEST_CASE("difference: shear invariance carries the answers off the axes") {
    for (const int k : {1, -2}) {
        for (const auto& [name, region] : fixtures::all()) {
            for (int x0 = -2; x0 <= 10; x0 += 4) {
                for (int y0 = -2; y0 <= 10; y0 += 4) {
                    const PolygonShape remover = box(x0, y0, x0 + 6, y0 + 6);
                    INFO(name << " sheared by " << k << " minus " << remover);
                    std::vector<Region> expected;
                    for (const Region& piece : region.difference(remover)) {
                        expected.push_back(shear(piece, k));
                    }
                    std::sort(expected.begin(), expected.end());
                    CHECK(shear(region, k).difference(shear(remover, k)) == expected);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// General position. Off the lattice the boundaries cross at rational points, so
// the result type has to be exact; the oracle is then any probe that avoids both
// boundaries, since the boundary of the difference is contained in their union.

TEST_CASE("difference: probe oracle in general position") {
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
        for (const EPolygon& remover : shapes) {
            for (int dx = -3; dx <= 3; dx += 3) {
                for (int dy = -3; dy <= 3; dy += 3) {
                    const EPolygon shifted = remover + EPoint(dx, dy);
                    INFO(target.outer() << " minus " << shifted);
                    const auto pieces = target.difference<pgl::ERational>(shifted);

                    pgl::ERational total(0);
                    bool allValid = true;
                    for (const auto& piece : pieces) {
                        allValid = allValid && piece.isValid();
                        total += piece.twiceArea();
                    }
                    CHECK(allValid);
                    CHECK(total <= target.twiceArea());

                    bool agrees = true;
                    for (int x = -1; x <= 12; ++x) {
                        for (int y = -1; y <= 12; ++y) {
                            const EPoint probe(pgl::ERational(2 * x + 1, 2),
                                               pgl::ERational(2 * y + 1, 2));
                            if (target.boundaryContains(probe) || shifted.boundaryContains(probe)) {
                                continue;
                            }
                            const bool expected = target.contains(probe) && !shifted.contains(probe);
                            bool found = false;
                            for (const auto& piece : pieces) {
                                found = found || piece.contains(probe);
                            }
                            agrees = agrees && (found == expected);
                            ++checks;
                        }
                    }
                    CHECK(agrees);
                }
            }
        }
    }
    CHECK(checks > 5000);
}

// -----------------------------------------------------------------------------
// The pieces really are a decomposition: they tile the difference, and putting
// each of them back removes nothing more.

TEST_CASE("difference: removing a piece from the difference leaves the rest") {
    for (const auto& [name, region] : fixtures::all()) {
        const PolygonShape remover = box(3, 3, 9, 9);
        const auto pieces = region.difference(remover);
        for (const Region& piece : pieces) {
            CHECK_MESSAGE(piece.difference(piece).empty(), name);
            for (const Region& other : pieces) {
                if (!(other == piece)) {
                    const auto rest = piece.difference(other);
                    REQUIRE_MESSAGE(rest.size() == 1, name);
                    CHECK_MESSAGE(rest[0] == piece, name);
                }
            }
        }
    }
}
