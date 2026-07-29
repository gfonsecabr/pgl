#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <string>
#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
using Ray = pgl::Ray<Point>;
using Halfplane = pgl::Halfplane<Point>;
using Rectangle = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using Disk = pgl::Disk<Point>;
using Polygon = pgl::Polygon<Point>;
using Chain = pgl::MonotoneChain<Point>;
using Polyline = pgl::Polyline<Point>;
using Intersection = pgl::HalfplaneIntersection<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// The reverse direction of the three containment predicates: `B.contains(A)`,
// `B.boundaryContains(A)` and `B.interiorContains(A)` for a region `A` and a
// lower-ranked `B`. The symmetric predicates and the distances already answer
// from both sides, and `separates` was settled in its own increment; these are
// what was left.
//
// Two rewritings carry all of it, and both are about how little the holes
// matter:
//
//  - `B ⊇ A ⟺ B ⊇ outer`, because the region holds the whole outer ring
//    however its rings meet, and every shape here is closed with a connected
//    complement — except a `Polyline`, which can close a loop. The all-slit
//    region below is exactly where the two readings part company.
//
//  - `∂B ⊇ A` needs the region to have no area, and a region with no area is
//    exactly the union of its ring edges. No shape's boundary is ever asked
//    about the outer polygon, which may well have area of its own.
//
// The fixtures live on an even lattice so that the unit grid refines the
// arrangement of every rectilinear shape used against them, which is what makes
// the exhaustive oracle below exact rather than a sampling.

static Polygon box(int x0, int y0, int x1, int y1) {
    return Polygon({x0, y0, x1, y0, x1, y1, x0, y1});
}

// The 16x16 square with an 8x8 hole in the middle.
static Region annulus() {
    return Region(box(0, 0, 16, 16), std::vector{box(4, 4, 12, 12)});
}

// Two holes meeting along x = 8, 2 <= y <= 12, which belongs to the region
// while nothing beside it does: a slit.
static Region slit() {
    return Region(box(0, 0, 16, 16), std::vector{box(2, 2, 8, 12), box(8, 2, 14, 12)});
}

// Two holes meeting at the single point (8,8): a pinch that is not a cut.
static Region pointTouch() {
    return Region(box(0, 0, 16, 16), std::vector{box(2, 2, 8, 8), box(8, 8, 14, 14)});
}

// A band hole all the way across, leaving two slabs joined by the slits its
// left and right edges leave on the outer ring.
static Region bandSplit() {
    return Region(box(0, 0, 16, 16), std::vector{box(0, 6, 16, 10)});
}

// A hole in the corner, sharing two whole stretches of the outer ring.
static Region cornerHole() {
    return Region(box(0, 0, 16, 16), std::vector{box(0, 0, 6, 6)});
}

// The hole covers the outer polygon exactly, so the region *is* the ring: a
// closed curve with no area anywhere, and the one fixture where the outer
// polygon answers differently from the region itself.
static Region ringOnly() {
    return Region(box(0, 0, 16, 16), std::vector{box(0, 0, 16, 16)});
}

static Region noHoles() {
    return Region(box(0, 0, 16, 16));
}

static std::vector<Region> rectilinearRegions() {
    return {annulus(), slit(), pointTouch(), bandSplit(), cornerHole(), ringOnly(), noHoles()};
}

// ---------------------------------------------------------------------------
// The exhaustive oracle.
//
// Both operands are rectilinear with even coordinates, so membership in either
// one is constant on every cell of the unit grid: the integer points of the
// region's bounding box are one representative per cell — (even,even) a
// vertex, (odd,even) and (even,odd) an edge midpoint, (odd,odd) a face centre
// — and every cell that meets the region has one. So `A ⊆ B` holds exactly
// when every representative in `A` is in `B`, and likewise against `∂B` and
// `B∖∂B`. Nothing of the shipped implementation is involved: the answers come
// from the region's own point location and the shape's own point predicates,
// both settled in earlier increments.

template <class Shape>
static void checkAgainstGrid(const std::string& name, const Shape& shape, const Region& region) {
    INFO("shape: " << name);
    bool inside = true;
    bool onBoundary = true;
    bool strictlyInside = true;
    const Rectangle bounds = region.bbox();
    for (int x = bounds.min().x(); x <= bounds.max().x(); ++x) {
        for (int y = bounds.min().y(); y <= bounds.max().y(); ++y) {
            const Point p(x, y);
            if (!region.contains(p)) {
                continue;
            }
            inside = inside && shape.contains(p);
            onBoundary = onBoundary && shape.boundaryContains(p);
            strictlyInside = strictlyInside && shape.interiorContains(p);
        }
    }
    CHECK(shape.contains(region) == inside);
    CHECK(shape.boundaryContains(region) == onBoundary);
    CHECK(shape.interiorContains(region) == strictlyInside);
}

TEST_CASE("PolygonWithHoles reverse containment: rectilinear grid oracle") {
    for (const Region& region : rectilinearRegions()) {
        REQUIRE(region.isValid());

        // Points: inside the material, in a hole, on a ring, outside.
        for (const Point& p : {Point(2, 2), Point(8, 8), Point(0, 0), Point(20, 20)}) {
            checkAgainstGrid("point", p, region);
        }

        // Axis-aligned 1D shapes, each in a position that meets a ring.
        for (const Segment& s : {Segment(Point(0, 0), Point(16, 0)),
                                 Segment(Point(0, 8), Point(16, 8)),
                                 Segment(Point(-4, 4), Point(20, 4)),
                                 Segment(Point(8, 2), Point(8, 12))}) {
            checkAgainstGrid("segment", s, region);
            checkAgainstGrid("orientedsegment", OrientedSegment(s.max(), s.min()), region);
            checkAgainstGrid("chain", Chain({s.min(), s.max()}), region);
        }
        for (const Line& l : {Line(Point(0, 0), Point(16, 0)), Line(Point(8, 0), Point(8, 16))}) {
            checkAgainstGrid("line", l, region);
            checkAgainstGrid("orientedline", OrientedLine(l[1], l[0]), region);
        }
        for (const Ray& r : {Ray(Point(-8, 0), Point(16, 0)), Ray(Point(8, 0), Point(8, 16))}) {
            checkAgainstGrid("ray", r, region);
        }

        // Half-planes and their intersections, from covering to disjoint.
        for (int cut : {-2, 0, 6, 16, 20}) {
            const Halfplane h(Point(cut, 0), Point(cut, 4));  // x <= cut
            checkAgainstGrid("halfplane", h, region);
            Intersection slab(h);
            slab.insert(Halfplane(Point(0, 16), Point(0, 0)));  // x >= 0
            checkAgainstGrid("halfplaneintersection", slab, region);
        }

        // Areas: covering, cutting, and sitting inside a hole.
        for (const Rectangle& r : {Rectangle(Point(-2, -2), Point(18, 18)),
                                   Rectangle(Point(0, 0), Point(16, 16)),
                                   Rectangle(Point(0, 0), Point(8, 16)),
                                   Rectangle(Point(4, 4), Point(12, 12)),
                                   Rectangle(Point(20, 20), Point(24, 24))}) {
            checkAgainstGrid("rectangle", r, region);
            checkAgainstGrid("convex", Convex({r.min(), Point(r.max().x(), r.min().y()), r.max(),
                                               Point(r.min().x(), r.max().y())}),
                             region);
            checkAgainstGrid("polygon", box(r.min().x(), r.min().y(), r.max().x(), r.max().y()),
                             region);
            checkAgainstGrid("intersection-box", Intersection(r), region);
        }

        // A rectilinear L and a rectilinear U, the non-convex containers.
        checkAgainstGrid("L", Polygon({-2, -2, 18, -2, 18, 8, 8, 8, 8, 18, -2, 18}), region);
        checkAgainstGrid("U", Polygon({-2, -2, 18, -2, 18, 18, 12, 18, 12, 4, 4, 4, 4, 18, -2, 18}),
                         region);

        // Degenerate triangles are the only rectilinear ones.
        checkAgainstGrid("flat triangle", Triangle(Point(0, 0), Point(8, 0), Point(16, 0)), region);

        // Chains and polylines, including the loop that traces the outer ring.
        checkAgainstGrid("staircase", Chain({Point(0, 0), Point(8, 0), Point(8, 16), Point(16, 16)}),
                         region);
        checkAgainstGrid("ring loop",
                         Polyline({Point(0, 0), Point(16, 0), Point(16, 16), Point(0, 16),
                                   Point(0, 0)}),
                         region);
        checkAgainstGrid("open ring",
                         Polyline({Point(16, 0), Point(16, 16), Point(0, 16), Point(0, 0)}), region);
        checkAgainstGrid("comb", Polyline({Point(0, 0), Point(0, 16), Point(8, 16), Point(8, 0),
                                           Point(16, 0), Point(16, 16)}),
                         region);
    }
}

// ---------------------------------------------------------------------------
// Shear invariance.
//
// All three predicates are invariant under any affine bijection, so a
// unimodular integer map must not change an answer. `(x,y) -> (x, y + kx)`
// keeps x-coordinates, hence keeps a monotone chain monotone, and carries the
// rectilinear fixtures above onto slanted edges and off-lattice crossings —
// which is how the exact answers there become ground truth for geometry the
// grid oracle cannot reach.

static Point shear(const Point& p, int k) {
    return Point(p.x(), p.y() + k * p.x());
}

static Polygon shear(const Polygon& poly, int k) {
    std::vector<Point> vertices;
    for (const Point& p : poly) {
        vertices.push_back(shear(p, k));
    }
    return Polygon(vertices);
}

static Region shear(const Region& region, int k) {
    std::vector<Polygon> holes;
    for (const Polygon& hole : region.holes()) {
        holes.push_back(shear(hole, k));
    }
    return Region(shear(region.outer(), k), holes);
}

static Segment shear(const Segment& s, int k) {
    return Segment(shear(s.min(), k), shear(s.max(), k));
}

static Line shear(const Line& l, int k) {
    return Line(shear(l[0], k), shear(l[1], k));
}

static Ray shear(const Ray& r, int k) {
    return Ray(shear(r.source(), k), shear(r.target(), k));
}

static Halfplane shear(const Halfplane& h, int k) {
    return Halfplane(shear(h.source(), k), shear(h.target(), k));
}

static Triangle shear(const Triangle& t, int k) {
    return Triangle(shear(t.a(), k), shear(t.b(), k), shear(t.c(), k));
}

static Convex shear(const Convex& c, int k) {
    std::vector<Point> vertices;
    for (std::size_t i = 0; i < c.size(); ++i) {
        vertices.push_back(shear(c[i], k));
    }
    return Convex(vertices);
}

static Chain shear(const Chain& c, int k) {
    std::vector<Point> vertices;
    for (std::size_t i = 0; i < c.size(); ++i) {
        vertices.push_back(shear(c[i], k));
    }
    return Chain(vertices);
}

static Polyline shear(const Polyline& p, int k) {
    std::vector<Point> vertices;
    for (std::size_t i = 0; i < p.size(); ++i) {
        vertices.push_back(shear(p[i], k));
    }
    return Polyline(vertices);
}

template <class Shape>
static void checkShearInvariant(const std::string& name, const Shape& shape, const Region& region) {
    INFO("shape: " << name);
    for (int k : {1, 2, -3}) {
        const Shape mapped = shear(shape, k);
        const Region image = shear(region, k);
        CHECK(mapped.contains(image) == shape.contains(region));
        CHECK(mapped.boundaryContains(image) == shape.boundaryContains(region));
        CHECK(mapped.interiorContains(image) == shape.interiorContains(region));
    }
}

TEST_CASE("PolygonWithHoles reverse containment: shear invariance") {
    for (const Region& region : rectilinearRegions()) {
        checkShearInvariant("point", Point(0, 0), region);
        checkShearInvariant("point inside", Point(2, 2), region);
        checkShearInvariant("segment", Segment(Point(0, 0), Point(16, 0)), region);
        checkShearInvariant("line", Line(Point(0, 0), Point(16, 0)), region);
        checkShearInvariant("ray", Ray(Point(-8, 0), Point(16, 0)), region);
        checkShearInvariant("halfplane", Halfplane(Point(0, 16), Point(0, 0)), region);
        checkShearInvariant("triangle", Triangle(Point(-8, -8), Point(40, -8), Point(-8, 40)),
                            region);
        checkShearInvariant("small triangle", Triangle(Point(4, 4), Point(12, 4), Point(4, 12)),
                            region);
        checkShearInvariant("convex", Convex({Point(0, 0), Point(16, 0), Point(16, 16),
                                              Point(0, 16)}),
                            region);
        checkShearInvariant("polygon L", Polygon({-2, -2, 18, -2, 18, 8, 8, 8, 8, 18, -2, 18}),
                            region);
        checkShearInvariant("big polygon", box(-2, -2, 18, 18), region);
        checkShearInvariant("chain", Chain({Point(0, 0), Point(8, 0), Point(8, 16), Point(16, 16)}),
                            region);
        checkShearInvariant("ring loop",
                            Polyline({Point(0, 0), Point(16, 0), Point(16, 16), Point(0, 16),
                                      Point(0, 0)}),
                            region);
    }
}

// ---------------------------------------------------------------------------
// Convex containers, off the lattice.
//
// A convex shape holds the region exactly when it holds every vertex of the
// outer ring: those vertices are in the region, and their convex hull covers
// the outer polygon, which covers the region. The same argument runs with the
// open interior, which is convex too. That settles the disk and the slanted
// triangles the grid oracle has to sit out.

template <class ConvexShape>
static void checkConvexContainer(const std::string& name, const ConvexShape& shape,
                                 const Region& region) {
    INFO("shape: " << name);
    bool inside = true;
    bool strictlyInside = true;
    for (const Point& v : region.outer()) {
        inside = inside && shape.contains(v);
        strictlyInside = strictlyInside && shape.interiorContains(v);
    }
    CHECK(shape.contains(region) == inside);
    CHECK(shape.interiorContains(region) == strictlyInside);
}

TEST_CASE("PolygonWithHoles reverse containment: convex containers") {
    for (const Region& region : rectilinearRegions()) {
        for (int radius : {2, 8, 11, 12, 40}) {
            for (const Point& centre : {Point(8, 8), Point(0, 0), Point(3, 11), Point(40, 40)}) {
                checkConvexContainer("disk", Disk(centre, radius), region);
            }
        }
        for (const Triangle& t : {Triangle(Point(-8, -8), Point(40, -8), Point(-8, 40)),
                                  Triangle(Point(-1, -1), Point(33, -1), Point(-1, 33)),
                                  Triangle(Point(0, 0), Point(16, 0), Point(0, 16)),
                                  Triangle(Point(5, 5), Point(11, 5), Point(8, 11))}) {
            checkConvexContainer("triangle", t, region);
        }
        for (const Convex& c : {Convex({Point(-1, 8), Point(8, -1), Point(17, 8), Point(8, 17)}),
                                Convex({Point(-8, 8), Point(8, -8), Point(24, 8), Point(8, 24)}),
                                Convex({Point(1, 1), Point(15, 1), Point(8, 15)})}) {
            checkConvexContainer("convex", c, region);
        }
    }
}

// ---------------------------------------------------------------------------
// The Polyline exception, and what tells it apart from every other shape.

TEST_CASE("PolygonWithHoles reverse containment: the polyline holds a ring no polygon does") {
    const Region ring = ringOnly();
    REQUIRE(ring.isValid());
    REQUIRE(ring.twiceArea() == 0);
    REQUIRE(ring.hasHoles());

    const Polyline loop({Point(0, 0), Point(16, 0), Point(16, 16), Point(0, 16), Point(0, 0)});

    SUBCASE("the loop holds the region but not the polygon it bounds") {
        CHECK(loop.contains(ring));
        CHECK_FALSE(loop.contains(ring.outer()));
        // Its two extreme vertices coincide at (0,0), which is in the region,
        // so the relative interior falls short of it.
        CHECK_FALSE(loop.interiorContains(ring));
        CHECK(loop.boundaryContains(Point(0, 0)));
    }

    SUBCASE("a loop started mid-edge leaves both ends outside the region's corner") {
        const Polyline shifted({Point(8, 0), Point(16, 0), Point(16, 16), Point(0, 16), Point(0, 0),
                                Point(8, 0)});
        CHECK(shifted.contains(ring));
        // The extremes are both (8,0), on the ring, so again not interior.
        CHECK_FALSE(shifted.interiorContains(ring));
    }

    SUBCASE("a loop with a tail has its extremes off the ring") {
        const Polyline tailed({Point(8, -4), Point(8, 0), Point(16, 0), Point(16, 16), Point(0, 16),
                               Point(0, 0), Point(8, 0), Point(8, -4)});
        CHECK(tailed.contains(ring));
        CHECK(tailed.interiorContains(ring));
    }

    SUBCASE("an open ring misses the piece it does not close") {
        const Polyline open({Point(16, 0), Point(16, 16), Point(0, 16), Point(0, 0)});
        CHECK_FALSE(open.contains(ring));
    }

    SUBCASE("the polyline never holds a region with area") {
        const Polyline outline({Point(0, 0), Point(16, 0), Point(16, 16), Point(0, 16), Point(0, 0),
                                Point(4, 4), Point(12, 4), Point(12, 12), Point(4, 12),
                                Point(4, 4)});
        // Every ring edge of the annulus is on this polyline, and it still does
        // not hold the material between them.
        CHECK_FALSE(outline.contains(annulus()));
        CHECK(outline.contains(ring));
    }

    SUBCASE("every other shape answers the ring by its outer polygon") {
        const Rectangle r(Point(0, 0), Point(16, 16));
        CHECK(r.contains(ring));
        CHECK(r.contains(ring.outer()));
        CHECK(r.boundaryContains(ring));
        CHECK_FALSE(r.interiorContains(ring));
        CHECK_FALSE(r.boundaryContains(ring.outer()));
    }
}

// ---------------------------------------------------------------------------
// Hand-written cases, one family at a time.

TEST_CASE("PolygonWithHoles reverse containment: a hole never changes the answer") {
    const Region region = annulus();
    const Region plain = noHoles();

    SUBCASE("a container covering the outer ring covers the region, hole or not") {
        const Rectangle big(Point(-1, -1), Point(17, 17));
        CHECK(big.contains(region));
        CHECK(big.contains(plain));
        CHECK(big.interiorContains(region));
        CHECK(big.interiorContains(plain));
    }

    SUBCASE("a container filling the hole exactly holds none of the region") {
        const Rectangle inner(Point(4, 4), Point(12, 12));
        CHECK_FALSE(inner.contains(region));
        // It does hold the hole rim, which belongs to the region -- but the
        // material around it is what it misses.
        CHECK(inner.contains(Segment(Point(4, 4), Point(12, 4))));
    }

    SUBCASE("a container that stops short of the outer ring fails") {
        const Rectangle almost(Point(0, 0), Point(15, 16));
        CHECK_FALSE(almost.contains(region));
        CHECK(almost.contains(Point(15, 16)));
    }

    SUBCASE("the boundary of a container never holds a region with area") {
        const Rectangle exact(Point(0, 0), Point(16, 16));
        CHECK(exact.contains(region));
        CHECK_FALSE(exact.boundaryContains(region));
        CHECK_FALSE(exact.interiorContains(region));
    }
}

TEST_CASE("PolygonWithHoles reverse containment: degenerate regions") {
    SUBCASE("a region that is one point") {
        const Region dot(Polygon({4, 4, 4, 4, 4, 4}));
        REQUIRE(dot.isPoint());
        CHECK(Point(4, 4).contains(dot));
        CHECK(Point(4, 4).interiorContains(dot));
        CHECK_FALSE(Point(5, 4).contains(dot));
        CHECK(Segment(Point(0, 4), Point(8, 4)).contains(dot));
        CHECK(Segment(Point(0, 4), Point(8, 4)).interiorContains(dot));
        CHECK_FALSE(Segment(Point(4, 4), Point(8, 4)).interiorContains(dot));
        CHECK(Segment(Point(4, 4), Point(8, 4)).boundaryContains(dot));
        CHECK(Rectangle(Point(0, 0), Point(8, 8)).interiorContains(dot));
        CHECK_FALSE(Rectangle(Point(0, 0), Point(8, 8)).boundaryContains(dot));
        CHECK(Rectangle(Point(4, 4), Point(8, 8)).boundaryContains(dot));
    }

    SUBCASE("a region that is one segment") {
        const Region bar(Polygon({2, 4, 10, 4, 6, 4}));
        REQUIRE(bar.isSegment());
        CHECK(Segment(Point(0, 4), Point(16, 4)).contains(bar));
        CHECK(Segment(Point(0, 4), Point(16, 4)).interiorContains(bar));
        CHECK(Line(Point(0, 4), Point(1, 4)).contains(bar));
        CHECK_FALSE(Segment(Point(0, 4), Point(8, 4)).contains(bar));
        CHECK(Rectangle(Point(0, 0), Point(16, 4)).boundaryContains(bar));
        CHECK_FALSE(Rectangle(Point(0, 0), Point(16, 8)).boundaryContains(bar));
        CHECK(Rectangle(Point(0, 0), Point(16, 8)).interiorContains(bar));
    }

    SUBCASE("the empty region is inside everything") {
        const Region empty;
        REQUIRE(empty.isEmpty());
        CHECK(Point(0, 0).contains(empty));
        CHECK(Point(0, 0).boundaryContains(empty));
        CHECK(Point(0, 0).interiorContains(empty));
        CHECK(Rectangle(Point(0, 0), Point(1, 1)).contains(empty));
        CHECK(Rectangle(Point(0, 0), Point(1, 1)).boundaryContains(empty));
        CHECK(Rectangle(Point(0, 0), Point(1, 1)).interiorContains(empty));
        CHECK(Polyline({Point(0, 0), Point(1, 1)}).contains(empty));
        CHECK(Line(Point(0, 0), Point(1, 1)).contains(empty));
    }
}

TEST_CASE("PolygonWithHoles reverse containment: boundaries hold slit material") {
    // The hole shares the whole bottom edge of the outer ring, so that edge is
    // covered twice and carries no material beside it -- but it is still in the
    // region, and the rectangle's boundary still holds it.
    const Region region = cornerHole();

    SUBCASE("only a region with no area can sit on a boundary") {
        CHECK_FALSE(Rectangle(Point(0, 0), Point(16, 16)).boundaryContains(region));
    }

    SUBCASE("the slit alone is a region a boundary does hold") {
        const Region strip(box(0, 0, 6, 6), std::vector{box(0, 0, 6, 6)});
        REQUIRE(strip.isValid());
        CHECK(Rectangle(Point(0, 0), Point(6, 6)).boundaryContains(strip));
        CHECK_FALSE(Rectangle(Point(0, 0), Point(8, 6)).boundaryContains(strip));
        CHECK(Polyline({Point(0, 0), Point(6, 0), Point(6, 6), Point(0, 6), Point(0, 0)})
                  .contains(strip));
    }
}

TEST_CASE("PolygonWithHoles reverse containment: unbounded containers") {
    const Region region = annulus();

    SUBCASE("a half-plane holds the region exactly when it holds the outer ring") {
        CHECK(Halfplane(Point(0, 16), Point(0, 0)).contains(region));       // x >= 0
        CHECK_FALSE(Halfplane(Point(2, 16), Point(2, 0)).contains(region));  // x >= 2
        CHECK_FALSE(Halfplane(Point(0, 16), Point(0, 0)).interiorContains(region));
        CHECK(Halfplane(Point(-1, 16), Point(-1, 0)).interiorContains(region));
    }

    SUBCASE("a line or a ray holds only a collinear degenerate region") {
        const Region bar(Polygon({2, 4, 10, 4, 6, 4}));
        CHECK(Line(Point(0, 4), Point(1, 4)).contains(bar));
        CHECK(Ray(Point(0, 4), Point(1, 4)).contains(bar));
        CHECK_FALSE(Ray(Point(4, 4), Point(5, 4)).contains(bar));
        CHECK_FALSE(Line(Point(0, 4), Point(1, 4)).contains(region));
        CHECK_FALSE(Line(Point(0, 4), Point(1, 4)).boundaryContains(bar));  // a line has no boundary
    }

    SUBCASE("an unbounded half-plane intersection behaves as its constraints do") {
        Intersection quadrant(Halfplane(Point(0, 16), Point(0, 0)));
        quadrant.insert(Halfplane(Point(0, 0), Point(16, 0)));
        CHECK(quadrant.contains(region));
        CHECK_FALSE(quadrant.interiorContains(region));
        CHECK_FALSE(quadrant.boundaryContains(region));
    }
}

TEST_CASE("PolygonWithHoles reverse containment: the region as its own container") {
    // The same predicate in the other direction is already shipped; these check
    // the two do not contradict each other where both are defined.
    const Region outerRegion(box(0, 0, 16, 16), std::vector{box(6, 6, 10, 10)});
    const Region innerRegion(box(1, 1, 5, 15));
    CHECK(outerRegion.contains(innerRegion));
    CHECK(outerRegion.interiorContains(innerRegion));
    CHECK_FALSE(innerRegion.contains(outerRegion));

    const Rectangle wide(Point(-1, -1), Point(17, 17));
    CHECK(wide.contains(outerRegion));
    CHECK(outerRegion.contains(Rectangle(Point(1, 1), Point(5, 15))));
    CHECK_FALSE(outerRegion.contains(wide));
}

TEST_CASE("PolygonWithHoles reverse containment: exact rational coordinates") {
    using EPoint = pgl::Point<pgl::ERational>;
    using EPolygon = pgl::Polygon<EPoint>;
    using ERegion = pgl::PolygonWithHoles<EPoint>;
    using ERectangle = pgl::Rectangle<EPoint>;
    using EPolyline = pgl::Polyline<EPoint>;

    const auto at = [](int x, int y) { return EPoint(pgl::ERational(x), pgl::ERational(y)); };
    const auto ebox = [&](int x0, int y0, int x1, int y1) {
        return EPolygon({at(x0, y0), at(x1, y0), at(x1, y1), at(x0, y1)});
    };

    const ERegion region(ebox(0, 0, 4, 4), std::vector{ebox(1, 1, 3, 3)});
    REQUIRE(region.isValid());
    CHECK(ERectangle(at(0, 0), at(4, 4)).contains(region));
    CHECK_FALSE(ERectangle(at(0, 0), at(4, 4)).interiorContains(region));
    CHECK(ERectangle(EPoint(pgl::ERational(-1, 2), pgl::ERational(-1, 2)), at(5, 5))
              .interiorContains(region));
    CHECK_FALSE(ERectangle(at(0, 0), at(4, 4)).boundaryContains(region));

    const ERegion ring(ebox(0, 0, 4, 4), std::vector{ebox(0, 0, 4, 4)});
    CHECK(ERectangle(at(0, 0), at(4, 4)).boundaryContains(ring));
    CHECK(EPolyline({at(0, 0), at(4, 0), at(4, 4), at(0, 4), at(0, 0)}).contains(ring));
}

TEST_CASE("PolygonWithHoles reverse containment: mixed coordinate types") {
    using LongPoint = pgl::Point<long long>;
    const pgl::Rectangle<LongPoint> big(LongPoint(-1, -1), LongPoint(17, 17));
    const Region region = annulus();
    CHECK(big.contains(region));
    CHECK(big.interiorContains(region));
    CHECK_FALSE(big.boundaryContains(region));
}
