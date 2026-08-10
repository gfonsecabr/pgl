#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
using Ray = pgl::Ray<Point>;
using Halfplane = pgl::Halfplane<Point>;
using RectangleShape = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using Disk = pgl::Disk<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Chain = pgl::MonotoneChain<Point>;
using PolylineShape = pgl::Polyline<Point>;
using Intersection = pgl::HalfplaneIntersection<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// `A.separates(B)` asks whether `B ∖ A` is disconnected, in both directions.
// Three things make a region different from every operand that came before:
//
//  - it is cut by removals that cannot cut a simply connected shape. One
//    crosscut turns an annulus into a disk, but two disconnect it — and a
//    segment run from one hole to another cuts a region while cutting no
//    polygon at all.
//
//  - it does the cutting with more than its outer ring. A hole is a piece of
//    complement in the middle of the shape, so removing an annulus from a
//    rectangle that covers it leaves the hole interior stranded — and removing
//    it from a half-plane does too, which no simple polygon can manage.
//
//  - it is nonetheless always connected itself, however its rings meet: its
//    complement is a disjoint union of simply connected open sets and so
//    encloses no part of it. A band hole splits the *domain* in two, but the
//    slits along the outer ring keep the region in one piece.
//
// The main fixture is the 8x8 square with a 4x4 hole in the middle:
//
//     (0,8)               (8,8)
//        +-----------------+
//        |                 |
//        |    +-------+    |     hole = [2,6] x [2,6]
//        |    |       |    |
//        |    +-------+    |
//        |                 |
//        +-----------------+
//     (0,0)               (8,0)

static PolygonShape box(int x0, int y0, int x1, int y1) {
    return PolygonShape({x0, y0, x1, y0, x1, y1, x0, y1});
}

static Region annulus() {
    return Region(box(0, 0, 8, 8), std::vector{box(2, 2, 6, 6)});
}

// Two holes meeting along the segment x = 4, 1 <= y <= 6, which belongs to the
// region while nothing beside it does: a slit.
static Region slit() {
    return Region(box(0, 0, 8, 8), std::vector{box(1, 1, 4, 6), box(4, 1, 7, 6)});
}

// Two holes meeting at the single point (4,4), where the region is pinched but
// not cut: the two lobes still join around the outside.
static Region pointTouch() {
    return Region(box(0, 0, 8, 8), std::vector{box(1, 1, 4, 4), box(4, 4, 7, 7)});
}

// Two wedge holes eat the left and right of the square and meet at its centre,
// pinching the region at (4,4) — but the wedges' outer sides lie on the outer
// ring, so those two edges stay in the region and join the lobes as well.
static Region bowtie() {
    const PolygonShape left({0, 0, 4, 4, 0, 8});
    const PolygonShape right({8, 0, 8, 8, 4, 4});
    return Region(box(0, 0, 8, 8), std::vector{left, right});
}

// The band hole spans the square, so the domain falls into two slabs — but the
// hole's left and right edges lie on the outer ring, and those slits hold the
// region together.
static Region bandSplit() {
    return Region(box(0, 0, 8, 8), std::vector{box(0, 3, 8, 5)});
}

TEST_CASE("PolygonWithHoles separates: a region is connected") {
    // Nothing removed is nothing severed, whatever the rings do. Checked
    // through a shape that misses the region entirely, which leaves it whole.
    const Segment away(Point(20, 20), Point(30, 30));
    for (const Region& region : {annulus(), slit(), pointTouch(), bowtie(), bandSplit()}) {
        REQUIRE(region.isValid());
        CHECK_FALSE(away.separates(region));
        CHECK_FALSE(region.separates(away));
        CHECK_FALSE(region.crosses(away));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: cutting the annulus") {
    const Region region = annulus();

    SUBCASE("one crosscut leaves a disk, two disconnect it") {
        const Segment left(Point(0, 4), Point(2, 4));
        const Segment right(Point(6, 4), Point(8, 4));
        CHECK_FALSE(left.separates(region));
        CHECK_FALSE(right.separates(region));
        // Both at once — as one polyline through the hole — do cut.
        const PolylineShape across({Point(0, 4), Point(2, 4), Point(6, 4), Point(8, 4)});
        CHECK(across.separates(region));
    }

    SUBCASE("a chord of the material that misses both rings does not cut") {
        const Segment chord(Point(1, 3), Point(1, 5));
        CHECK_FALSE(chord.separates(region));
    }

    SUBCASE("the region cuts a segment that crosses the material twice") {
        const Segment through(Point(-1, 4), Point(9, 4));
        CHECK(region.separates(through));  // the piece in the hole is stranded
        CHECK(region.crosses(through));    // and the segment cuts the annulus
        CHECK(through.crosses(region));
    }

    SUBCASE("a segment lying in the hole is left alone") {
        const Segment inside(Point(3, 4), Point(5, 4));
        CHECK_FALSE(region.separates(inside));
        CHECK_FALSE(region.intersects(inside));
    }

    SUBCASE("a segment along the hole rim is swallowed, not cut") {
        const Segment rim(Point(2, 2), Point(6, 2));
        CHECK(region.contains(rim));
        CHECK_FALSE(region.separates(rim));
    }

    SUBCASE("the oriented segment answers as its unoriented twin") {
        const Segment through(Point(-1, 4), Point(9, 4));
        const OrientedSegment oriented(Point(9, 4), Point(-1, 4));
        CHECK(region.separates(oriented) == region.separates(through));
        CHECK(oriented.separates(region) == through.separates(region));
    }
}

TEST_CASE("PolygonWithHoles vs Point: no point ever cuts a region") {
    // A pinch is never a cut point, however tightly the rings close on it.
    // Counting the components of `A ∖ {p}` by duality turns them into the
    // first cohomology of the complement plus p; every complement component is
    // a Jordan domain, so that is a wedge of simply connected spaces and thus
    // trivial. Concretely: whatever rings meet at p also run away from it, and
    // their boundaries belong to the region, so there is always a way around.
    CHECK_FALSE(Point(4, 4).separates(bowtie()));      // the two wedge sides
    CHECK_FALSE(Point(4, 4).separates(pointTouch()));  // around both holes
    CHECK_FALSE(Point(4, 3).separates(slit()));        // along the slit
    CHECK_FALSE(Point(0, 0).separates(annulus()));

    // Nor is a point ever cut itself.
    CHECK_FALSE(annulus().separates(Point(1, 1)));
    CHECK_FALSE(annulus().crosses(Point(1, 1)));
}

TEST_CASE("PolygonWithHoles vs Segment: the slit and the band") {
    SUBCASE("cutting the slit disconnects the region") {
        // The slit x = 4, 1 <= y <= 6 is the only material joining the two
        // halves apart from the outer margin; cutting both severs the region.
        const Region region = slit();
        const PolylineShape cut({Point(4, 0), Point(4, 8)});
        CHECK(cut.separates(region));
        // Cutting the slit alone still leaves the way around the margin.
        const Segment partial(Point(4, 2), Point(4, 5));
        CHECK_FALSE(partial.separates(region));
    }

    SUBCASE("the band leaves the region joined along its two slits") {
        const Region region = bandSplit();
        // Removing one slit still leaves the other.
        const Segment leftSlit(Point(0, 3), Point(0, 5));
        CHECK_FALSE(leftSlit.separates(region));
        // Removing both cuts the region into its two slabs.
        const PolylineShape bothSlits({Point(0, 3), Point(0, 5), Point(8, 5), Point(8, 3)});
        CHECK(bothSlits.separates(region));
    }
}

TEST_CASE("PolygonWithHoles vs Line, OrientedLine and Ray") {
    const Region region = annulus();

    SUBCASE("a line through the hole cuts the annulus and is cut by it") {
        const Line line(Point(0, 4), Point(1, 4));
        CHECK(line.separates(region));
        CHECK(region.separates(line));
        CHECK(region.crosses(line));
        CHECK(line.crosses(region));
        const OrientedLine oriented(Point(1, 4), Point(0, 4));
        CHECK(oriented.separates(region) == line.separates(region));
        CHECK(region.separates(oriented) == region.separates(line));
    }

    SUBCASE("a line clear of the region cuts nothing") {
        const Line line(Point(0, 20), Point(1, 20));
        CHECK_FALSE(line.separates(region));
        CHECK_FALSE(region.separates(line));
    }

    SUBCASE("a ray stopping inside the hole leaves the annulus whole") {
        // The ray enters through the left wall and ends in the hole, so it is
        // one crosscut: the annulus becomes a disk but stays connected.
        const Ray ray(Point(4, 4), Point(-1, 4));
        CHECK_FALSE(ray.separates(region));
        // The region still severs the ray: its far tail and the part inside
        // the hole are separate pieces.
        CHECK(region.separates(ray));
    }
}

TEST_CASE("PolygonWithHoles vs Halfplane and HalfplaneIntersection") {
    const Region region = annulus();

    SUBCASE("a half-plane never disconnects a polygon but a region does") {
        // Removing the annulus from the half-plane strands the hole interior.
        const Halfplane upper(Point(0, -4), Point(1, -4));
        CHECK(region.separates(upper));
        // A closed half-plane leaves a convex remainder of a convex region and
        // cannot cut the annulus: what is left is one connected piece.
        CHECK_FALSE(upper.separates(region));
        CHECK_FALSE(region.crosses(upper));
    }

    SUBCASE("a half-plane that reaches into the hole still leaves a path") {
        const Halfplane low(Point(0, 4), Point(1, 4));  // y >= 4
        CHECK_FALSE(low.separates(region));
    }

    SUBCASE("a bounded intersection answers as the convex polygon it is") {
        Intersection strip(Halfplane(Point(0, 3), Point(1, 3)));
        strip.insert(Halfplane(Point(1, 5), Point(0, 5)));
        strip.insert(Halfplane(Point(-2, 1), Point(-2, 0)));
        strip.insert(Halfplane(Point(10, 0), Point(10, 1)));
        REQUIRE(strip.isBounded());
        // The strip covers the annulus from wall to wall, so it cuts it.
        CHECK(strip.separates(region));
        CHECK(region.separates(strip));
        CHECK(region.crosses(strip));
    }

    SUBCASE("an unbounded intersection is clipped without changing the answer") {
        Intersection slab(Halfplane(Point(0, 3), Point(1, 3)));
        slab.insert(Halfplane(Point(1, 5), Point(0, 5)));
        REQUIRE_FALSE(slab.isBounded());
        CHECK(slab.separates(region));
        CHECK(region.separates(slab));
    }

    SUBCASE("the empty intersection removes nothing") {
        Intersection empty(Halfplane(Point(0, 0), Point(1, 0)));
        empty.insert(Halfplane(Point(1, -1), Point(0, -1)));
        REQUIRE(empty.empty());
        CHECK_FALSE(empty.separates(region));
    }
}

TEST_CASE("PolygonWithHoles vs the area shapes") {
    const Region region = annulus();

    SUBCASE("a rectangle spanning the region cuts it, and is cut back") {
        const RectangleShape band(Point(-1, 3), Point(9, 5));
        CHECK(band.separates(region));
        CHECK(region.separates(band));
        CHECK(region.crosses(band));
        CHECK(band.crosses(region));
    }

    SUBCASE("a shape enclosing the region strands the hole interior") {
        // Every edge of this rectangle is clear of the region, so its boundary
        // never touches it — and the remainder still comes apart, into the
        // frame around the outer ring and the swallowed hole. No simple
        // polygon can do that to a rectangle.
        const RectangleShape over(Point(-1, -1), Point(9, 9));
        CHECK(region.separates(over));
        CHECK_FALSE(over.separates(region));  // the annulus stays connected
        CHECK_FALSE(region.crosses(over));

        // A rectangle inside the outer ring instead loses everything but the
        // hole, which is one piece.
        const RectangleShape inside(Point(1, 1), Point(7, 7));
        CHECK_FALSE(region.separates(inside));
    }

    SUBCASE("a triangle biting one side leaves the annulus whole") {
        const Triangle bite(Point(-1, 3), Point(-1, 5), Point(1, 4));
        CHECK_FALSE(bite.separates(region));
        // What is left of the triangle is the part outside the outer ring:
        // one piece, since its tip stops short of the hole.
        CHECK_FALSE(region.separates(bite));
    }

    SUBCASE("a convex hull spanning the material cuts it") {
        const Convex wedge(std::vector{Point(-1, 3), Point(9, 3), Point(9, 5), Point(-1, 5)});
        CHECK(wedge.separates(region));
    }

    SUBCASE("a reflex polygon around the hole does not cut the annulus") {
        // A U shape open to the right: it takes one bite, so the material is
        // still connected around the far side.
        const PolygonShape uShape({1, 1, 7, 1, 7, 3, 3, 3, 3, 5, 7, 5, 7, 7, 1, 7});
        CHECK_FALSE(uShape.separates(region));
    }

    SUBCASE("a degenerate area operand answers as it does for a polygon") {
        // Matching Polygon::separates, which declines a degenerate operand.
        const RectangleShape flat(Point(1, 4), Point(7, 4));
        CHECK(flat.isDegenerate());
        CHECK(region.separates(flat) == region.outer().separates(flat));
        CHECK(flat.separates(region) == flat.separates(region.outer()));
    }
}

TEST_CASE("PolygonWithHoles vs PolygonWithHoles") {
    const Region region = annulus();

    SUBCASE("a region that swallows another strands its frame and its hole") {
        // What is left of the bigger region is the frame outside the outer
        // ring and the ring-shaped piece of the annulus's hole that its own
        // hole does not cover: two components.
        const Region other(box(-1, -1, 9, 9), std::vector{box(3, 3, 5, 5)});
        CHECK(region.separates(other));
        // The other region covers the annulus, so nothing of it is left.
        CHECK_FALSE(other.separates(region));
    }

    SUBCASE("two annuli crossing at right angles cut each other") {
        const Region turned(box(3, -3, 5, 11), std::vector{box(3, 2, 5, 6)});
        CHECK(turned.separates(region));
        CHECK(region.separates(turned));
        CHECK(region.crosses(turned));
    }

    SUBCASE("a region without holes answers exactly as its outer polygon") {
        const Region plain(box(-1, 3, 9, 5));
        CHECK(plain.separates(region) == plain.outer().separates(region));
        CHECK(region.separates(plain) == region.separates(plain.outer()));
    }
}

TEST_CASE("PolygonWithHoles vs MonotoneChain and Polyline") {
    const Region region = annulus();

    SUBCASE("a chain from wall to wall through the hole cuts the region") {
        const Chain chain({Point(-1, 4), Point(9, 4)});
        CHECK(chain.separates(region));
        CHECK(region.separates(chain));
        CHECK(region.crosses(chain));
    }

    SUBCASE("a chain stopping in the hole is one crosscut") {
        const Chain chain({Point(-1, 4), Point(4, 4)});
        CHECK_FALSE(chain.separates(region));
    }

    SUBCASE("a polyline closing a loop inside the material strands its inside") {
        // A square loop drawn in the material: what it encloses is cut off.
        const PolylineShape loop({Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1), Point(0, 0)});
        CHECK(loop.separates(region));
    }

    SUBCASE("a one-vertex chain removes a point, so it cuts nothing") {
        const Chain single({Point(3, 4)});
        CHECK_FALSE(single.separates(region));
        const Chain atPinch({Point(4, 4)});
        CHECK_FALSE(atPinch.separates(bowtie()));
    }
}

TEST_CASE("PolygonWithHoles vs Disk") {
    const Region region = annulus();

    SUBCASE("a disk swallowing the hole strands its interior") {
        // What is left of the disk is the part beyond the outer ring and the
        // hole interior in the middle: two pieces.
        const Disk over(Point(4, 4), 5);
        CHECK(region.separates(over));
        // The inscribed disk stays inside the outer ring, so the hole is all
        // that is left of it -- one piece.
        const Disk inscribed(Point(4, 4), 4);
        CHECK_FALSE(region.separates(inscribed));
    }

    SUBCASE("the disk cuts back when it eats the middle of every wall") {
        const Disk inscribed(Point(4, 4), 4);
        CHECK(inscribed.separates(region));  // the four corners are left
        CHECK_FALSE(region.crosses(inscribed));

        const Disk over(Point(4, 4), 5);
        CHECK(over.separates(region));
        CHECK(region.crosses(over));
        CHECK(over.crosses(region));
    }

    SUBCASE("one bite leaves the annulus whole") {
        // The disk spans the left wall, touching both rings, but the material
        // still runs around the far side of the hole.
        const Disk bite(Point(1, 4), 1);
        CHECK_FALSE(bite.separates(region));
        CHECK_FALSE(region.separates(bite));  // the disk lies in the material
    }

    SUBCASE("a disk inside a hole is one piece of the complement") {
        const Disk inHole(Point(4, 4), 1);
        CHECK_FALSE(region.separates(inHole));
        CHECK_FALSE(inHole.separates(region));
    }

    SUBCASE("a slit cuts a disk although it carries no area") {
        // The two holes meet along x = 4, and that segment is region material
        // with nothing beside it. A disk inside the holes meets the region in
        // that segment alone, which splits it into two half-disks.
        const Region region2 = slit();
        const Disk onSlit(Point(4, 4), 1);
        CHECK(region2.separates(onSlit));
        CHECK_FALSE(onSlit.separates(region2));  // the frame still joins it up
        CHECK_FALSE(region2.crosses(onSlit));
    }

    SUBCASE("a pinch point cuts a disk into two quadrants") {
        // The holes meet at (4,4) only, so a disk around it keeps just that
        // point of the region -- and the two hole interiors it covers meet
        // nowhere else.
        const Region region2 = pointTouch();
        CHECK(region2.separates(Disk(Point(4, 4), 1)));
        CHECK_FALSE(region2.separates(Disk(Point(3, 3), 1)));  // inside one hole
    }

    SUBCASE("a slit holds the region together where the material is cut") {
        // The hole reaches the left wall, so the region is two slabs, a column
        // of material on the right, and the slit the hole's left edge shares
        // with the outer ring. The disk severs the column, and the slit -- which
        // carries no area and so no triangle of the domain -- is what keeps the
        // slabs joined.
        const Region curtain(box(0, 0, 8, 8), std::vector{box(0, 2, 7, 6)});
        REQUIRE(curtain.isValid());
        const Disk cut(Point(8, 4), 1);
        CHECK_FALSE(cut.separates(curtain));
        // Take the slit away as well -- a band across everything -- and the
        // same two slabs do come apart.
        CHECK(RectangleShape(Point(-1, 3), Point(9, 5)).separates(curtain));
    }

    SUBCASE("the band's two slits both have to go") {
        const Region band = bandSplit();
        // A disk over the left slit alone leaves the right one carrying the
        // region.
        CHECK_FALSE(Disk(Point(0, 4), 1).separates(band));
        // One big enough to swallow both also eats through the slabs.
        CHECK(Disk(Point(4, 4), 5).separates(band));
    }

    SUBCASE("a degenerate disk cuts nothing, either way") {
        const Disk radiusZero(Point(4, 4), 0);
        CHECK_FALSE(region.separates(radiusZero));
        CHECK_FALSE(radiusZero.separates(region));
        const Disk undefined(Point(0, 0), Point(1, 1), Point(2, 2));
        REQUIRE(undefined.isUndefined());
        CHECK_FALSE(region.separates(undefined));
        CHECK_FALSE(undefined.separates(region));
    }

    SUBCASE("a region without holes answers exactly as its outer polygon") {
        const Region plain(box(0, 0, 8, 8));
        const Disk disk(Point(0, 4), 3);
        CHECK(plain.separates(disk) == plain.outer().separates(disk));
        CHECK(disk.separates(plain) == disk.separates(plain.outer()));
    }
}

TEST_CASE("PolygonWithHoles separates: the empty shape") {
    const Region region = annulus();
    const pgl::EmptyShape<Point> nothing;
    CHECK_FALSE(region.separates(nothing));
    CHECK_FALSE(region.crosses(nothing));
}

TEST_CASE("PolygonWithHoles separates: exact rational coordinates") {
    using ERegion = pgl::PolygonWithHoles<pgl::Point<pgl::ERational>>;
    using EPolygon = pgl::Polygon<pgl::Point<pgl::ERational>>;
    using ESegment = pgl::Segment<pgl::Point<pgl::ERational>>;
    using EPoint = pgl::Point<pgl::ERational>;

    const EPolygon outer({0, 0, 8, 0, 8, 8, 0, 8});
    const EPolygon hole({2, 2, 6, 2, 6, 6, 2, 6});
    const ERegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    const ESegment through(EPoint(pgl::ERational(-1), pgl::ERational(4)),
                           EPoint(pgl::ERational(9), pgl::ERational(4)));
    CHECK(region.separates(through));  // the piece in the hole is stranded
    CHECK(through.separates(region));   // and the cut runs wall to wall

    // A half-integer cut runs the full height of the left wall, so it severs
    // the region -- and lies wholly inside it, so nothing of it survives.
    const ESegment half(EPoint(pgl::ERational(1, 2), pgl::ERational(0)),
                        EPoint(pgl::ERational(1, 2), pgl::ERational(8)));
    CHECK(half.separates(region));
    CHECK_FALSE(region.separates(half));
    CHECK(region.contains(half));

    // A disk over the hole and past the outer ring, in the same exact type:
    // the hole interior is stranded, and the four corners of the square
    // survive the removal.
    const pgl::Disk<EPoint> disk(EPoint(pgl::ERational(4), pgl::ERational(4)), pgl::ERational(5));
    CHECK(region.separates(disk));
    CHECK(disk.separates(region));
    CHECK(region.crosses(disk));
}
