#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Line = pgl::Line<Point>;
using Halfplane = pgl::Halfplane<Point>;
using RectangleShape = pgl::Rectangle<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using Intersection = pgl::HalfplaneIntersection<Point>;
using ERational = pgl::ERational;

// The half-plane intersection is the last operand of phase 1, and the only one
// that is convex and closed but not necessarily bounded. That splits the work:
//
//  - the three containment relations want a bounded operand, since the region is
//    bounded. A bounded one with area is a convex polygon and goes to the area
//    path; a degenerate one is a point, a segment, a ray or a line, and goes to
//    the overload for that carrier.
//  - intersects keeps the unbounded case by the ring-contact argument the other
//    operands use: an operand that misses ∂A entirely lies wholly in A° — which
//    a bounded region rules out for an unbounded shape — or wholly outside A.
//  - interiorsIntersect clips the operand to the region's bounding box first,
//    which changes no answer (A° is an open subset of that box) and leaves a
//    convex polygon behind.
//
// The fixture is the 10x10 square with a 4x4 hole in the middle, as in the other
// PolygonWithHoles pair tests.

static Region annulus() {
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({3, 3, 7, 3, 7, 7, 3, 7});
    return Region(outer, std::vector{hole});
}

// A half-plane contains everything to the left of the directed line, so these
// spell out the four axis-aligned ones without leaving the reader to work out
// the orientation.
static Halfplane rightOf(int x) { return Halfplane(Point(x, 1), Point(x, 0)); }
static Halfplane leftOf(int x) { return Halfplane(Point(x, 0), Point(x, 1)); }
static Halfplane above(int y) { return Halfplane(Point(0, y), Point(1, y)); }
static Halfplane below(int y) { return Halfplane(Point(1, y), Point(0, y)); }

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: the two extreme regions") {
    const Region region = annulus();
    REQUIRE(region.isValid());

    SUBCASE("the empty region is contained and meets nothing") {
        Intersection empty(rightOf(5));
        empty.insert(leftOf(3));
        REQUIRE(empty.empty());
        CHECK(region.contains(empty));
        CHECK(region.interiorContains(empty));
        CHECK(region.boundaryContains(empty));
        CHECK(!region.intersects(empty));
        CHECK(!region.interiorsIntersect(empty));
    }

    SUBCASE("the whole plane meets everything and is contained by nothing") {
        const Intersection plane;
        REQUIRE(plane.isPlane());
        CHECK(!region.contains(plane));
        CHECK(!region.interiorContains(plane));
        CHECK(!region.boundaryContains(plane));
        CHECK(region.intersects(plane));
        CHECK(region.interiorsIntersect(plane));
    }
}

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: unbounded operands") {
    const Region region = annulus();

    SUBCASE("a half-plane cutting across the region") {
        const Intersection h(rightOf(5));
        CHECK(!region.contains(h));  // unbounded
        CHECK(!region.interiorContains(h));
        CHECK(!region.boundaryContains(h));
        CHECK(region.intersects(h));
        CHECK(region.interiorsIntersect(h));
    }

    SUBCASE("a half-plane meeting the region only along the outer ring") {
        const Intersection h(rightOf(10));
        CHECK(region.intersects(h));           // the right edge belongs to both
        CHECK(!region.interiorsIntersect(h));  // but no area is shared
        CHECK(region.squaredDistance<ERational>(h) == ERational(0));
    }

    SUBCASE("a half-plane clear of the region") {
        const Intersection h(rightOf(12));
        CHECK(!region.intersects(h));
        CHECK(!region.interiorsIntersect(h));
        // The region measures a disjoint operand from its own ring edges, and
        // each edge hands the query to HalfplaneIntersection, which builds its
        // boundary edges in the requested coordinate type — so a floating-point
        // request has to come out the same as an exact one. It did not, until
        // Point stopped telling -0.0 and +0.0 apart: a vertex of this operand
        // lands on x = -0.0.
        CHECK(region.squaredDistance<ERational>(h) == ERational(4));
        CHECK(region.distanceL1<ERational>(h) == ERational(2));
        CHECK(region.distanceLInf<ERational>(h) == ERational(2));
        CHECK(region.squaredDistance<double>(h) == doctest::Approx(4.0));
        CHECK(region.distanceL1<double>(h) == doctest::Approx(2.0));
        CHECK(region.distanceLInf<double>(h) == doctest::Approx(2.0));
    }

    SUBCASE("a quadrant reaching into the region") {
        Intersection quadrant(rightOf(8));
        quadrant.insert(above(8));
        REQUIRE(!quadrant.isBounded());
        CHECK(!region.contains(quadrant));
        CHECK(region.intersects(quadrant));
        CHECK(region.interiorsIntersect(quadrant));
    }

    SUBCASE("a slab that only crosses the hole and the material around it") {
        Intersection slab(rightOf(4));
        slab.insert(leftOf(6));
        REQUIRE(!slab.isBounded());
        CHECK(!region.contains(slab));
        CHECK(region.intersects(slab));
        CHECK(region.interiorsIntersect(slab));
    }
}

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: bounded operands") {
    const Region region = annulus();

    SUBCASE("a box in the material between the rings") {
        const Intersection box(RectangleShape(Point(1, 1), Point(2, 9)));
        REQUIRE(box.isBounded());
        CHECK(region.contains(box));
        CHECK(region.interiorContains(box));
        CHECK(!region.boundaryContains(box));
        CHECK(region.intersects(box));
        CHECK(region.interiorsIntersect(box));
    }

    SUBCASE("a box touching the outer ring leaves the region interior") {
        const Intersection box(RectangleShape(Point(0, 0), Point(2, 2)));
        CHECK(region.contains(box));
        CHECK(!region.interiorContains(box));
        CHECK(region.interiorsIntersect(box));
    }

    SUBCASE("a box enclosing the hole is not contained") {
        const Intersection box(RectangleShape(Point(2, 2), Point(8, 8)));
        CHECK(!region.contains(box));
        CHECK(!region.interiorContains(box));
        CHECK(region.intersects(box));
        CHECK(region.interiorsIntersect(box));
    }

    SUBCASE("a box strictly inside the hole misses the region") {
        const Intersection box(RectangleShape(Point(4, 4), Point(6, 6)));
        CHECK(!region.contains(box));
        CHECK(!region.intersects(box));
        CHECK(!region.interiorsIntersect(box));
        CHECK(region.squaredDistance<ERational>(box) == ERational(1));
    }

    SUBCASE("a box spanning the hole exactly meets the region on its rim") {
        const Intersection box(RectangleShape(Point(3, 3), Point(7, 7)));
        CHECK(!region.contains(box));
        CHECK(region.intersects(box));
        CHECK(!region.interiorsIntersect(box));
    }

    SUBCASE("a triangle cut out by three half-planes") {
        Intersection triangle(above(1));
        triangle.insert(rightOf(1));
        triangle.insert(Halfplane(Point(3, 1), Point(1, 3)));  // x + y <= 4
        REQUIRE(triangle.isBounded());
        CHECK(region.contains(triangle));
        CHECK(region.interiorContains(triangle));
        CHECK(region.interiorsIntersect(triangle));
    }

    SUBCASE("the same triangle answers as its convex-polygon view") {
        Intersection triangle(above(1));
        triangle.insert(rightOf(1));
        triangle.insert(Halfplane(Point(3, 1), Point(1, 3)));
        const auto convex = triangle.asConvex<ERational>();
        CHECK(region.contains(triangle) == region.contains(convex));
        CHECK(region.interiorContains(triangle) == region.interiorContains(convex));
        CHECK(region.intersects(triangle) == region.intersects(convex));
        CHECK(region.interiorsIntersect(triangle) == region.interiorsIntersect(convex));
    }
}

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: degenerate operands") {
    const Region region = annulus();

    SUBCASE("a point carrier") {
        const Intersection inMaterial(Point(1, 1));
        REQUIRE(inMaterial.isDegenerate());
        REQUIRE(inMaterial.isPoint());
        CHECK(region.contains(inMaterial));
        CHECK(region.interiorContains(inMaterial));
        CHECK(!region.boundaryContains(inMaterial));
        CHECK(region.intersects(inMaterial));
        CHECK(!region.interiorsIntersect(inMaterial));

        const Intersection inHole(Point(5, 5));
        CHECK(!region.contains(inHole));
        CHECK(!region.intersects(inHole));

        const Intersection onRing(Point(3, 3));
        CHECK(region.contains(onRing));
        CHECK(!region.interiorContains(onRing));
        CHECK(region.boundaryContains(onRing));
    }

    SUBCASE("a segment carrier") {
        const Intersection onOuter(Segment(Point(0, 1), Point(0, 9)));
        REQUIRE(onOuter.isDegenerate());
        CHECK(region.contains(onOuter));
        CHECK(!region.interiorContains(onOuter));
        CHECK(region.boundaryContains(onOuter));
        CHECK(region.intersects(onOuter));
        CHECK(!region.interiorsIntersect(onOuter));

        const Intersection throughTheHole(Segment(Point(1, 5), Point(9, 5)));
        CHECK(!region.contains(throughTheHole));
        CHECK(!region.boundaryContains(throughTheHole));
        CHECK(region.intersects(throughTheHole));
    }

    SUBCASE("a line carrier is unbounded, so nothing contains it") {
        const Intersection line(Line(Point(0, 5), Point(1, 5)));
        REQUIRE(line.isDegenerate());
        REQUIRE(!line.isBounded());
        CHECK(!region.contains(line));
        CHECK(!region.interiorContains(line));
        CHECK(!region.boundaryContains(line));
        CHECK(region.intersects(line));
        CHECK(!region.interiorsIntersect(line));  // a line has no interior of its own

        const Intersection clear(Line(Point(0, 20), Point(1, 20)));
        CHECK(!region.intersects(clear));
    }

    SUBCASE("a ray carrier") {
        Intersection ray(Line(Point(5, 0), Point(5, 1)));
        ray.insert(above(0));
        REQUIRE(ray.isDegenerate());
        REQUIRE(ray.isRay());
        CHECK(!region.contains(ray));
        CHECK(region.intersects(ray));
        CHECK(!region.interiorsIntersect(ray));
    }
}

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: a pinched region") {
    // The 6x6 square cut across by a band hole sharing an edge stretch with the
    // outer ring on both sides. The left slit belongs to the region and has no
    // area beside it.
    const PolygonShape outer({0, 0, 6, 0, 6, 6, 0, 6});
    const PolygonShape hole({0, 2, 6, 2, 6, 4, 0, 4});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    Intersection slabOverTheBand(above(2));
    slabOverTheBand.insert(below(4));
    slabOverTheBand.insert(leftOf(0));  // x <= 0, so only the slit is left
    REQUIRE(!slabOverTheBand.empty());
    CHECK(region.intersects(slabOverTheBand));
    CHECK(!region.interiorsIntersect(slabOverTheBand));

    // Reaching past the band into a slab is a different matter.
    Intersection reachingIn(above(1));
    reachingIn.insert(below(4));
    reachingIn.insert(leftOf(2));
    CHECK(region.intersects(reachingIn));
    CHECK(region.interiorsIntersect(reachingIn));
}

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: the symmetric predicates") {
    const Region region = annulus();
    const Intersection box(RectangleShape(Point(2, 2), Point(8, 8)));
    const Intersection inHole(RectangleShape(Point(4, 4), Point(6, 6)));
    const Intersection halfplane(rightOf(5));

    CHECK(box.intersects(region) == region.intersects(box));
    CHECK(box.interiorsIntersect(region) == region.interiorsIntersect(box));
    CHECK(inHole.intersects(region) == region.intersects(inHole));
    CHECK(inHole.interiorsIntersect(region) == region.interiorsIntersect(inHole));
    CHECK(halfplane.intersects(region) == region.intersects(halfplane));
    CHECK(halfplane.interiorsIntersect(region) == region.interiorsIntersect(halfplane));

    CHECK(inHole.squaredDistance<ERational>(region) == region.squaredDistance<ERational>(inHole));
    CHECK(inHole.distanceL1<ERational>(region) == region.distanceL1<ERational>(inHole));
    CHECK(inHole.distanceLInf<ERational>(region) == region.distanceLInf<ERational>(inHole));

    // A region without holes is its outer polygon, and answers exactly as it does.
    const PolygonShape poly({0, 0, 10, 0, 10, 10, 0, 10});
    const Region solid(poly);
    for (const Intersection& h : {box, inHole, halfplane}) {
        CHECK(solid.intersects(h) == poly.intersects(h));
        CHECK(solid.interiorsIntersect(h) == poly.interiorsIntersect(h));
        CHECK(solid.squaredDistance<ERational>(h) == poly.squaredDistance<ERational>(h));
    }
}

// A half-plane intersection operand supports the region-valued
// `regularizedIntersection`,
// the one that keeps holes -- so this pair answers with regions rather than with
// the component vector `HalfplaneIntersection::intersection(Polygon)` returns,
// and it answers the same in either order.

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: regularized intersection keeps holes") {
    const Region region = annulus();
    using EPoint = pgl::Point<ERational>;
    using ERegion = pgl::PolygonWithHoles<EPoint>;

    SUBCASE("a region covered by the operand comes back whole, hole and all") {
        const Intersection covering(RectangleShape(Point(-5, -5), Point(15, 15)));
        const auto pieces = region.regularizedIntersection<ERational>(covering);
        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0) == ERegion(region));
        CHECK(covering.regularizedIntersection<ERational>(region) == pieces);
    }

    SUBCASE("an unbounded operand is clipped to the region") {
        // {x >= 5} keeps the right half of the annulus, whose ring is cut open
        // by the hole into a single outer ring with a notch.
        const Intersection right(rightOf(5));
        REQUIRE(!right.isBounded());
        const auto pieces = region.regularizedIntersection<ERational>(right);
        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0).holes().empty());
        CHECK(pieces.component(0).area<ERational>() == ERational(50 - 8));
        CHECK(right.regularizedIntersection<ERational>(region) == pieces);
    }

    SUBCASE("the operand can come apart into several pieces") {
        // The horizontal slab 4 <= y <= 6 crosses the hole, so what is left of
        // the annulus in it is the two bars beside the hole.
        Intersection slab(above(4));
        slab.insert(below(6));
        const auto pieces = region.regularizedIntersection<ERational>(slab);
        REQUIRE(pieces.componentCount() == 2);
        CHECK(pieces.component(0).area<ERational>() == ERational(6));
        CHECK(pieces.component(1).area<ERational>() == ERational(6));
        CHECK(slab.regularizedIntersection<ERational>(region) == pieces);
    }

    SUBCASE("the crossings are exact whatever the coordinates look like") {
        // The line through (0,1) and (3,0) meets the outer ring at thirds; the
        // arrangement is built over rationals, so the area is exact.
        const Intersection tilted(Halfplane(Point(0, 1), Point(3, 0)));
        const auto pieces = region.regularizedIntersection<ERational>(tilted);
        REQUIRE(pieces.componentCount() == 1);
        REQUIRE(pieces.component(0).holes().size() == 1);
        // The whole annulus but the corner triangle (0,0)-(3,0)-(0,1).
        CHECK(pieces.component(0).area<ERational>() == ERational(100 - 16) - ERational(3, 2));
    }

    SUBCASE("nothing with area gives no piece at all") {
        Intersection empty(rightOf(5));
        empty.insert(leftOf(3));
        REQUIRE(empty.empty());
        CHECK(region.regularizedIntersection<ERational>(empty).empty());

        Intersection line(above(5));
        line.insert(below(5));
        REQUIRE(line.isDegenerate());
        CHECK(region.regularizedIntersection<ERational>(line).empty());
        CHECK(line.regularizedIntersection<ERational>(region).empty());

        // Missing the region entirely is empty too.
        CHECK(region.regularizedIntersection<ERational>(Intersection(rightOf(20))).empty());
    }
}

TEST_CASE("PolygonWithHoles vs HalfplaneIntersection: the difference removes it") {
    // A union or a symmetric difference with an unbounded operand is unbounded
    // and fits in no set of regions. A difference is not: `A ∖ B` sits inside
    // `A`, so it stays bounded however far `B` reaches, and that is what the
    // overloads here are. The latitude is one-sided -- it is the *receiver* that
    // has to be bounded -- so there is no `intersection.difference(region)` to
    // check against, and each answer is checked against the intersection it
    // complements instead.
    const Region region = annulus();

    SUBCASE("what the difference removes is what the intersection keeps") {
        // The two partition the region, since B and its complement do.
        for (const Halfplane& h : {rightOf(5), above(4), leftOf(3), below(6),
                                   Halfplane(Point(0, 1), Point(3, 0))}) {
            const auto removed = region.difference<ERational>(h);
            const auto kept = region.regularizedIntersection<ERational>(h);
            CHECK(removed.twiceArea() + kept.twiceArea() == ERational(2 * (100 - 16)));
            // and removing the opposite half-plane is keeping this one.
            CHECK(region.difference<ERational>(h.opposite()).twiceArea() == kept.twiceArea());
        }
    }

    SUBCASE("an unbounded operand is clipped, and can leave several pieces") {
        // The horizontal slab 4 <= y <= 6 crosses the hole; removing it cuts the
        // annulus into the part below it and the part above it.
        Intersection slab(above(4));
        slab.insert(below(6));
        REQUIRE(!slab.isBounded());
        const auto pieces = region.difference<ERational>(slab);
        REQUIRE(pieces.componentCount() == 2);
        CHECK(pieces.twiceArea() == ERational(2 * (100 - 16 - 12)));
    }

    SUBCASE("a bounded operand in the middle opens a hole") {
        // Removing a rectangle strictly inside the square leaves a region with
        // a hole, exactly as removing the polygon spelling of it does.
        const Intersection middle(RectangleShape(Point(2, 2), Point(8, 8)));
        const auto pieces = region.difference<ERational>(middle);
        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0).holes().size() == 1);
        CHECK(pieces.twiceArea() == ERational(2 * (100 - 36)));
        CHECK(pieces == region.difference<ERational>(RectangleShape(Point(2, 2), Point(8, 8))));
    }

    SUBCASE("an operand covering the region leaves nothing") {
        const Intersection covering(RectangleShape(Point(-5, -5), Point(15, 15)));
        CHECK(region.difference<ERational>(covering).empty());
        CHECK(region.difference<ERational>(Intersection{}).empty());  // the whole plane
    }

    SUBCASE("nothing with area removes nothing, and gives the regularization") {
        Intersection empty(rightOf(5));
        empty.insert(leftOf(3));
        REQUIRE(empty.empty());
        CHECK(region.difference<ERational>(empty) == region.regularized<ERational>());

        Intersection line(above(5));
        line.insert(below(5));
        REQUIRE(line.isDegenerate());
        CHECK(region.difference<ERational>(line) == region.regularized<ERational>());

        // Missing the region entirely removes nothing either.
        CHECK(region.difference<ERational>(Intersection(rightOf(20))) ==
              region.regularized<ERational>());
        CHECK(region.difference<ERational>(rightOf(20)) == region.regularized<ERational>());
    }

    SUBCASE("every bounded receiver takes both spellings") {
        const RectangleShape rectangle(Point(0, 0), Point(10, 10));
        const pgl::Triangle<Point> triangle(Point(0, 0), Point(10, 0), Point(10, 10));
        const pgl::Convex<Point> convex(
            std::vector<Point>{Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)});
        const PolygonShape polygon({0, 0, 10, 0, 10, 10, 0, 10});
        const pgl::PolygonSet<Point> set(region);
        const Halfplane top = above(5);
        const Intersection topIntersection(top);

        // A half-plane is the one-constraint half-plane intersection, on every
        // one of them.
        CHECK(rectangle.difference<ERational>(top) ==
              rectangle.difference<ERational>(topIntersection));
        CHECK(triangle.difference<ERational>(top) ==
              triangle.difference<ERational>(topIntersection));
        CHECK(convex.difference<ERational>(top) == convex.difference<ERational>(topIntersection));
        CHECK(polygon.difference<ERational>(top) == polygon.difference<ERational>(topIntersection));
        CHECK(region.difference<ERational>(top) == region.difference<ERational>(topIntersection));
        CHECK(set.difference<ERational>(top) == set.difference<ERational>(topIntersection));

        // The convex spellings of one square all answer alike, and the set
        // answers like the region it holds.
        CHECK(rectangle.difference<ERational>(top) == convex.difference<ERational>(top));
        CHECK(rectangle.difference<ERational>(top) == polygon.difference<ERational>(top));
        CHECK(set.difference<ERational>(top) == region.difference<ERational>(top));
        CHECK(triangle.difference<ERational>(top).twiceArea() == ERational(75));
    }

    SUBCASE("an integral result is exact where the cut is on the lattice") {
        // The engine builds the arrangement over rationals whatever the result
        // type is, so an `int` request that lands on the lattice is not an
        // approximation of the answer but the answer.
        const auto exact = region.difference<ERational>(above(5));
        const auto integral = region.difference<int>(above(5));
        CHECK(integral.twiceArea() == 2 * (50 - 8));
        CHECK(exact.twiceArea() == ERational(2 * (50 - 8)));
        CHECK(pgl::PolygonSet<pgl::Point<ERational>>(integral) == exact);
    }
}
