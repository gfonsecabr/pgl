#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <functional>
#include <sstream>
#include <unordered_set>
#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// A 10x10 square with a 2x2 square hole at (2,2)-(4,4): area 100 - 4 = 96.
static PolygonShape outerSquare() { return PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}); }
static PolygonShape smallHole() { return PolygonShape({2, 2, 4, 2, 4, 4, 2, 4}); }
static PolygonShape otherHole() { return PolygonShape({6, 6, 8, 6, 8, 8, 6, 8}); }

TEST_CASE("PolygonWithHoles construction and ring access") {
    SUBCASE("default construction is the empty region") {
        const Region region;
        CHECK(region.empty());
        CHECK(!region.hasHoles());
        CHECK(region.holeCount() == 0);
        CHECK(region.vertexCount() == 0);
        CHECK(region.twiceArea() == 0);
        CHECK(region.isUndefined());
    }

    SUBCASE("outer boundary only") {
        const Region region(outerSquare());
        CHECK(!region.empty());
        CHECK(!region.hasHoles());
        CHECK(region.outer() == outerSquare());
        CHECK(region.vertexCount() == 4);
        CHECK(region.twiceArea() == 200);
    }

    SUBCASE("outer boundary with holes") {
        const Region region(outerSquare(), std::vector{smallHole(), otherHole()});
        CHECK(region.hasHoles());
        CHECK(region.holeCount() == 2);
        CHECK(region.vertexCount() == 12);
        CHECK(region.twiceArea() == 200 - 8 - 8);
    }

    SUBCASE("zero-area holes are dropped") {
        // A collapsed ring (a point), a ring spanning only a segment, and a
        // proper hole. Only the proper hole removes anything from the region.
        const PolygonShape degeneratePoint({3, 3, 3, 3, 3, 3});
        const PolygonShape degenerateSegment({3, 3, 5, 3, 7, 3});
        const Region region(outerSquare(),
                            std::vector{degeneratePoint, degenerateSegment, smallHole()});
        CHECK(region.holeCount() == 1);
        CHECK(region.hole(0) == smallHole());
        CHECK(region.twiceArea() == 192);
    }

    SUBCASE("addHole keeps canonical order and drops degenerate rings") {
        Region region(outerSquare());
        region.addHole(otherHole());
        region.addHole(smallHole());
        REQUIRE(region.holeCount() == 2);
        CHECK(region.hole(0) < region.hole(1));
        region.addHole(PolygonShape({1, 1, 1, 1, 1, 1}));
        CHECK(region.holeCount() == 2);
    }

    SUBCASE("eraseHole by index takes the canonical order") {
        Region region(outerSquare(), std::vector{otherHole(), smallHole()});
        REQUIRE(region.hole(0) == smallHole());
        region.eraseHole(0);
        CHECK(region.holeCount() == 1);
        CHECK(region.hole(0) == otherHole());
        CHECK(region.twiceArea() == 200 - 8);
        region.eraseHole(0);
        CHECK(!region.hasHoles());
        CHECK(region.twiceArea() == 200);
    }

    SUBCASE("eraseHole by polygon reports whether it erased") {
        Region region(outerSquare(), std::vector{smallHole(), otherHole()});
        CHECK(region.eraseHole(otherHole()));
        CHECK(region.holeCount() == 1);
        CHECK(region.hole(0) == smallHole());

        // Erasing it again, and erasing a polygon that is no hole of this
        // region, both fail and leave the region alone.
        CHECK(!region.eraseHole(otherHole()));
        CHECK(!region.eraseHole(PolygonShape({1, 1, 2, 1, 2, 2, 1, 2})));
        CHECK(region.holeCount() == 1);
        CHECK(region.twiceArea() == 192);
    }

    SUBCASE("erasing every hole gives back the hole-free region") {
        const Region hollow(outerSquare(), std::vector{smallHole(), otherHole()});
        Region region = hollow;
        CHECK(region.eraseHole(smallHole()));
        region.eraseHole(0);
        CHECK(region == Region(outerSquare()));
        CHECK(region.isValid());

        // add then erase round-trips, memoized hash included.
        region.addHole(smallHole());
        region.addHole(otherHole());
        CHECK(region == hollow);
        CHECK(std::hash<Region>{}(region) == std::hash<Region>{}(hollow));
        CHECK(region.eraseHole(smallHole()));
        CHECK(region.eraseHole(otherHole()));
        CHECK(region == Region(outerSquare()));
        CHECK(std::hash<Region>{}(region) == std::hash<Region>{}(Region(outerSquare())));
    }

    SUBCASE("a region stays valid after an erase") {
        // Two holes that touch along an edge: erasing one leaves the other.
        const PolygonShape left({2, 2, 5, 2, 5, 5, 2, 5});
        const PolygonShape right({5, 2, 8, 2, 8, 5, 5, 5});
        Region region(outerSquare(), std::vector{left, right});
        REQUIRE(region.isValid());
        CHECK(region.eraseHole(left));
        CHECK(region.isValid());
        CHECK(region.holeCount() == 1);
        CHECK(region.hole(0) == right);
    }
}

TEST_CASE("PolygonWithHoles value semantics do not depend on hole order") {
    const Region ab(outerSquare(), std::vector{smallHole(), otherHole()});
    const Region ba(outerSquare(), std::vector{otherHole(), smallHole()});

    CHECK(ab == ba);
    CHECK((ab <=> ba) == std::strong_ordering::equal);
    CHECK(std::hash<Region>{}(ab) == std::hash<Region>{}(ba));

    const Region fewer(outerSquare(), std::vector{smallHole()});
    CHECK(ab != fewer);
    CHECK((ab <=> fewer) != std::strong_ordering::equal);

    SUBCASE("usable in unordered containers") {
        std::unordered_set<Region> set;
        set.insert(ab);
        set.insert(ba);
        set.insert(fewer);
        CHECK(set.size() == 2);
    }

    SUBCASE("memoized hash survives a translation") {
        Region moved = ab;
        const std::size_t before = std::hash<Region>{}(moved);
        moved += Point(3, 4);
        const std::size_t after = std::hash<Region>{}(moved);
        CHECK(before != after);
        CHECK(after == std::hash<Region>{}(Region(ab) += Point(3, 4)));
    }
}

TEST_CASE("PolygonWithHoles measures") {
    const Region region(outerSquare(), std::vector{smallHole()});

    SUBCASE("area subtracts the holes") {
        CHECK(region.twiceArea() == 192);
        CHECK(region.area<double>() == doctest::Approx(96.0));
        CHECK(!region.isDegenerate());
    }

    SUBCASE("centroid is area-weighted with holes negative") {
        // (100 * 5 - 4 * 3) / 96 in both coordinates, by symmetry about y = x.
        using Rational = pgl::Rational<long long>;
        const auto centre = region.centroid<Rational>();
        CHECK(centre.x() == Rational(488, 96));
        CHECK(centre.y() == Rational(488, 96));
    }

    SUBCASE("a hole at the centre leaves the centroid put") {
        const PolygonShape centred({4, 4, 6, 4, 6, 6, 4, 6});
        const Region symmetric(outerSquare(), std::vector{centred});
        const auto centre = symmetric.centroid<double>();
        CHECK(centre.x() == doctest::Approx(5.0));
        CHECK(centre.y() == doctest::Approx(5.0));
    }

    SUBCASE("vertices centroid covers every ring") {
        const auto centre = region.verticesCentroid<double>();
        // Four outer corners averaging (5,5) and four hole corners averaging (3,3).
        CHECK(centre.x() == doctest::Approx(4.0));
        CHECK(centre.y() == doctest::Approx(4.0));
    }

    SUBCASE("bbox and diameter come from the outer ring") {
        CHECK(region.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(10, 10)));
        CHECK(region.diameter().squaredLength() == 200);
    }

    SUBCASE("convexHull comes from the outer ring, ignoring the hole") {
        CHECK(region.convexHull() == region.outer().convexHull());
    }
}

TEST_CASE("PolygonWithHoles ring traversal") {
    const Region region(outerSquare(), std::vector{smallHole()});

    SUBCASE("vertices and edges cover every ring") {
        CHECK(region.vertices().size() == 8);
        CHECK(region.edges().size() == 8);
        CHECK(region.orientedEdges().size() == 8);
    }

    SUBCASE("hole rings are reversed so the region stays on the left") {
        const auto oriented = region.orientedEdges();
        // The outer ring keeps Polygon's counterclockwise storage order.
        CHECK(oriented[0].source() == region.outer()[0]);
        CHECK(oriented[0].target() == region.outer()[1]);
        // The hole ring is emitted clockwise, i.e. against its stored order.
        const auto& hole = region.hole(0);
        CHECK(oriented[4].source() == hole[1]);
        CHECK(oriented[4].target() == hole[0]);
    }

    SUBCASE("holes are iterable directly") {
        std::size_t seen = 0;
        for (const auto& hole : region) {
            CHECK(hole == smallHole());
            ++seen;
        }
        CHECK(seen == 1);
    }
}

TEST_CASE("PolygonWithHoles isValid") {
    SUBCASE("a hole strictly inside is valid") {
        CHECK(Region(outerSquare(), std::vector{smallHole()}).isValid());
    }

    SUBCASE("disjoint holes are valid") {
        CHECK(Region(outerSquare(), std::vector{smallHole(), otherHole()}).isValid());
    }

    SUBCASE("holes touching each other at a point are valid") {
        const PolygonShape left({2, 2, 4, 2, 4, 4, 2, 4});
        const PolygonShape right({4, 4, 6, 4, 6, 6, 4, 6});
        CHECK(Region(outerSquare(), std::vector{left, right}).isValid());
    }

    SUBCASE("a hole touching the outer boundary is valid") {
        const PolygonShape touching({0, 2, 2, 2, 2, 4, 0, 4});
        CHECK(Region(outerSquare(), std::vector{touching}).isValid());
    }

    // The contract is interior disjointness and nothing more: ring boundaries
    // may meet however they like, including along whole stretches of edge. The
    // region pinches shut where they do, which the predicates account for.
    SUBCASE("a hole sharing a whole edge with the outer boundary is valid") {
        const PolygonShape slit({0, 0, 4, 0, 4, 4, 0, 4});
        CHECK(Region(outerSquare(), std::vector{slit}).isValid());
    }

    SUBCASE("holes sharing a whole edge are valid") {
        const PolygonShape left({2, 2, 5, 2, 5, 8, 2, 8});
        const PolygonShape right({5, 2, 8, 2, 8, 8, 5, 8});
        CHECK(Region(outerSquare(), std::vector{left, right}).isValid());
    }

    SUBCASE("a hole spanning the square from edge to edge is valid") {
        // Splits the region in two and leaves a whisker on either side.
        const PolygonShape band({0, 4, 10, 4, 10, 6, 0, 6});
        CHECK(Region(outerSquare(), std::vector{band}).isValid());
    }

    SUBCASE("overlapping holes are invalid") {
        const PolygonShape left({2, 2, 5, 2, 5, 5, 2, 5});
        const PolygonShape right({4, 4, 7, 4, 7, 7, 4, 7});
        CHECK(!Region(outerSquare(), std::vector{left, right}).isValid());
    }

    SUBCASE("a nested hole is invalid") {
        const PolygonShape big({2, 2, 8, 2, 8, 8, 2, 8});
        const PolygonShape inner({3, 3, 5, 3, 5, 5, 3, 5});
        CHECK(!Region(outerSquare(), std::vector{big, inner}).isValid());
    }

    SUBCASE("a hole escaping the outer boundary is invalid") {
        const PolygonShape escaping({8, 8, 12, 8, 12, 12, 8, 12});
        CHECK(!Region(outerSquare(), std::vector{escaping}).isValid());
    }

    SUBCASE("a hole wholly outside the outer boundary is invalid") {
        const PolygonShape outside({20, 20, 22, 20, 22, 22, 20, 22});
        CHECK(!Region(outerSquare(), std::vector{outside}).isValid());
    }

    SUBCASE("a self-intersecting ring is invalid") {
        const PolygonShape bowtie({0, 0, 10, 10, 10, 0, 0, 10});
        CHECK(!Region(bowtie).isValid());
    }

    SUBCASE("the empty region is valid and carries no hole") {
        CHECK(Region().isValid());
    }
}

// A slit is a stretch of boundary the rings cover twice: region material with no
// area on either side of it. Rings meeting at an isolated point are not one — the
// interior still reaches the point from every side.
TEST_CASE("PolygonWithHoles isRegular") {
    SUBCASE("a region without holes is regular") {
        CHECK(Region(outerSquare()).isRegular());
    }

    SUBCASE("holes strictly inside leave the region regular") {
        CHECK(Region(outerSquare(), std::vector{smallHole()}).isRegular());
        CHECK(Region(outerSquare(), std::vector{smallHole(), otherHole()}).isRegular());
    }

    SUBCASE("holes touching at a point leave the region regular") {
        const PolygonShape left({2, 2, 4, 2, 4, 4, 2, 4});
        const PolygonShape right({4, 4, 6, 4, 6, 6, 4, 6});
        CHECK(Region(outerSquare(), std::vector{left, right}).isRegular());
    }

    SUBCASE("a hole touching the outer boundary at a point leaves the region regular") {
        const PolygonShape touching({0, 5, 3, 3, 3, 7});
        const Region region(outerSquare(), std::vector{touching});
        REQUIRE(region.isValid());
        CHECK(region.isRegular());
    }

    SUBCASE("a hole sharing a whole edge with the outer boundary is a slit") {
        const PolygonShape slit({0, 0, 4, 0, 4, 4, 0, 4});
        CHECK(!Region(outerSquare(), std::vector{slit}).isRegular());
    }

    SUBCASE("a hole sharing part of an edge with the outer boundary is a slit") {
        const PolygonShape slit({0, 2, 4, 2, 4, 6, 0, 6});
        CHECK(!Region(outerSquare(), std::vector{slit}).isRegular());
    }

    SUBCASE("holes sharing a whole edge are a slit") {
        const PolygonShape left({2, 2, 5, 2, 5, 8, 2, 8});
        const PolygonShape right({5, 2, 8, 2, 8, 8, 5, 8});
        CHECK(!Region(outerSquare(), std::vector{left, right}).isRegular());
    }

    SUBCASE("a hole spanning the square from edge to edge is a slit on either side") {
        const PolygonShape band({0, 4, 10, 4, 10, 6, 0, 6});
        CHECK(!Region(outerSquare(), std::vector{band}).isRegular());
    }

    SUBCASE("the empty region is regular") {
        CHECK(Region().isRegular());
    }
}

TEST_CASE("PolygonWithHoles regularized") {
    SUBCASE("a regular region comes back unchanged") {
        const Region region(outerSquare(), std::vector{smallHole(), otherHole()});
        const auto pieces = region.regularized<int>();
        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0) == region);
    }

    SUBCASE("a hole open to the outer boundary becomes a notch") {
        const PolygonShape slit({0, 2, 4, 2, 4, 6, 0, 6});
        const Region region(outerSquare(), std::vector{slit});
        const auto pieces = region.regularized<int>();
        REQUIRE(pieces.componentCount() == 1);
        CHECK(!pieces.component(0).hasHoles());
        CHECK(pieces.component(0).isRegular());
        CHECK(pieces.component(0).twiceArea() == region.twiceArea());
    }

    SUBCASE("holes sharing an edge merge into one") {
        const PolygonShape left({2, 2, 5, 2, 5, 8, 2, 8});
        const PolygonShape right({5, 2, 8, 2, 8, 8, 5, 8});
        const Region region(outerSquare(), std::vector{left, right});
        const auto pieces = region.regularized<int>();
        REQUIRE(pieces.componentCount() == 1);
        REQUIRE(pieces.component(0).holeCount() == 1);
        CHECK(pieces.component(0).hole(0) == PolygonShape({2, 2, 8, 2, 8, 8, 2, 8}));
        CHECK(pieces.component(0).isRegular());
        CHECK(pieces.component(0).twiceArea() == region.twiceArea());
    }

    SUBCASE("dropping the slits can take the region apart") {
        const PolygonShape band({0, 4, 10, 4, 10, 6, 0, 6});
        const Region region(outerSquare(), std::vector{band});
        const auto pieces = region.regularized<int>();
        REQUIRE(pieces.componentCount() == 2);
        CHECK(pieces.component(0) == Region(PolygonShape({0, 0, 10, 0, 10, 4, 0, 4})));
        CHECK(pieces.component(1) == Region(PolygonShape({0, 6, 10, 6, 10, 10, 0, 10})));
        CHECK(pieces.component(0).twiceArea() + pieces.component(1).twiceArea() == region.twiceArea());
    }

    SUBCASE("the empty region has no pieces") {
        CHECK(Region().regularized<int>().empty());
    }

    SUBCASE("the result type follows the requested number type") {
        const PolygonShape slit({0, 2, 4, 2, 4, 6, 0, 6});
        const Region region(outerSquare(), std::vector{slit});
        const auto pieces = region.regularized<double>();
        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0).area<double>() == doctest::Approx(region.area<double>()));
        static_assert(
            std::is_same_v<decltype(pieces.component(0)),
                           const pgl::PolygonWithHoles<pgl::Point<double>>&>);
    }
}

TEST_CASE("PolygonWithHoles transformations") {
    const Region region(outerSquare(), std::vector{smallHole()});

    SUBCASE("translation moves every ring and preserves area") {
        const Region moved = region + Point(5, -3);
        CHECK(moved.twiceArea() == region.twiceArea());
        CHECK(moved.outer() == outerSquare() + Point(5, -3));
        CHECK(moved.hole(0) == smallHole() + Point(5, -3));
        CHECK((moved - Point(5, -3)) == region);
    }

    SUBCASE("translation is the Minkowski sum with a point, spelled either way") {
        // A point operand is the one summand that gives back a region rather
        // than the set of them the area operands return.
        const auto sum = region.minkowskiSum(Point(5, -3));
        static_assert(std::is_same_v<decltype(sum), const Region>);
        CHECK(sum == region + Point(5, -3));
        CHECK(sum == Point(5, -3).minkowskiSum(region));

        // The empty shape absorbs here as everywhere.
        CHECK(pgl::detail::is_empty_shape_v<decltype(region.minkowskiSum(pgl::EmptyShape<Point>{}))>);
    }

    SUBCASE("translation promotes the coordinate type instead of truncating") {
        // The library never downgrades a construction silently: a translation
        // the region's own integral coordinates cannot hold promotes the
        // result, exactly as `Polygon + Point` does.
        const pgl::EPoint half(pgl::ERational(1, 2), pgl::ERational(1, 2));
        const auto moved = region + half;
        static_assert(std::is_same_v<typename decltype(moved)::NumberType, pgl::ERational>);
        CHECK(moved.outer().vertices()[0] == pgl::EPoint(outerSquare().vertices()[0].x() + pgl::ERational(1, 2),
                                                         outerSquare().vertices()[0].y() + pgl::ERational(1, 2)));
        CHECK(moved.holeCount() == region.holeCount());
        CHECK(moved == region.minkowskiSum(half));
    }

    SUBCASE("scaling up scales the area quadratically") {
        const auto scaled = region * 3;
        CHECK(scaled.twiceArea() == region.twiceArea() * 9);
        CHECK(scaled.holeCount() == 1);
    }

    SUBCASE("a negative factor reflects and stays canonical") {
        const auto reflected = region * -1;
        CHECK(reflected.twiceArea() == region.twiceArea());
        CHECK(reflected.holeCount() == 1);
        CHECK(reflected.isValid());
        // Reflecting twice is the identity, holes included.
        CHECK((reflected * -1) == region);
    }

    SUBCASE("rotate90 preserves area and validity") {
        const Region turned = region.rotated90(1);
        CHECK(turned.twiceArea() == region.twiceArea());
        CHECK(turned.isValid());
        CHECK(turned.rotated90(3) == region);
    }

    SUBCASE("axis scaling stretches one axis and scales the area linearly") {
        const Region wide = region.scaledUpX(3);
        CHECK(wide.twiceArea() == region.twiceArea() * 3);
        CHECK(wide.holeCount() == 1);
        CHECK(wide.isValid());
        CHECK(wide.outer() == outerSquare().scaledUpX(3));
        CHECK(wide.hole(0) == smallHole().scaledUpX(3));
        CHECK(wide.scaledDownX(3) == region);

        const Region tall = region.scaledUpY(3);
        CHECK(tall.twiceArea() == region.twiceArea() * 3);
        CHECK(tall.scaledDownY(3) == region);
        CHECK(tall == region.rotated90(1).scaledUpX(3).rotated90(-1));

        // The in-place forms agree with the accessors.
        Region mutated = region;
        mutated.scaleUpX(3);
        CHECK(mutated == wide);
        mutated.scaleDownX(3);
        CHECK(mutated == region);
        mutated.scaleUpY(3);
        CHECK(mutated == tall);
        mutated.scaleDownY(3);
        CHECK(mutated == region);
    }

    SUBCASE("a negative axis factor reflects and stays canonical") {
        const Region flipped = region.scaledUpX(-1);
        CHECK(flipped.twiceArea() == region.twiceArea());
        CHECK(flipped.holeCount() == 1);
        CHECK(flipped.isValid());
        CHECK(flipped.scaledUpX(-1) == region);
    }

    SUBCASE("a zero axis factor collapses the region and drops its holes") {
        const Region flat = region.scaledUpX(0);
        CHECK(flat.twiceArea() == 0);
        CHECK(flat.holeCount() == 0);
        CHECK(flat.isDegenerate());
    }
}

TEST_CASE("PolygonWithHoles streaming") {
    const Region region(outerSquare(), std::vector{smallHole()});
    std::ostringstream out;
    out << region;
    CHECK(out.str() ==
          "PolygonWithHoles[Polygon[(0,0),(10,0),(10,10),(0,10)],Polygon[(2,2),(4,2),(4,4),(2,4)]]");
}

TEST_CASE("PolygonWithHoles pointInside") {
    using Q = pgl::Rational<long long>;

    SUBCASE("an annulus") {
        const Region region(outerSquare(), std::vector{smallHole()});
        const auto witness = region.pointInside<Q>();
        CHECK(region.interiorContains(witness));
        CHECK(witness == region.outer().pointInside<Q>());
    }

    SUBCASE("a hole covering the ear of the lex-min vertex") {
        // Polygon's O(n) route starts at the lexicographically smallest vertex
        // and would look for a point in the triangle it spans with its
        // neighbours; here a hole occupies it, so the witness has to come from
        // elsewhere.
        const PolygonShape hole({0, 1, 8, 1, 8, 9, 0, 9});
        const Region region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}), std::vector{hole});
        REQUIRE(region.isValid());
        CHECK(region.interiorContains(region.pointInside<Q>()));
    }

    SUBCASE("a region split in two by a hole") {
        const PolygonShape band({0, 4, 10, 4, 10, 6, 0, 6});
        const Region region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}), std::vector{band});
        REQUIRE(region.isValid());
        const auto witness = region.pointInside<Q>();
        CHECK(region.interiorContains(witness));
    }

    SUBCASE("a slit region: the witness avoids the whiskers") {
        const PolygonShape hole({0, 0, 4, 0, 4, 4, 0, 4});
        const Region region(PolygonShape({0, 0, 8, 0, 8, 8, 0, 8}), std::vector{hole});
        const auto witness = region.pointInside<Q>();
        CHECK(region.contains(witness));
        CHECK(region.interiorContains(witness));
        CHECK_FALSE(region.boundaryContains(witness));
    }

    SUBCASE("pointInsideInteriorContainedIn") {
        const Region region(outerSquare(), std::vector{smallHole()});
        CHECK(region.pointInsideInteriorContainedIn(outerSquare()));
        CHECK_FALSE(region.pointInsideInteriorContainedIn(smallHole()));
        CHECK_FALSE(region.pointInsideInteriorContainedIn(PolygonShape({20, 20, 22, 20, 22, 22, 20, 22})));
    }
}

TEST_CASE("PolygonWithHoles over exact and converted coordinate types") {
    SUBCASE("rational coordinates") {
        using RPoint = pgl::Point<pgl::Rational<long long>>;
        using RRegion = pgl::PolygonWithHoles<RPoint>;
        const pgl::Polygon<RPoint> outer({0, 0, 4, 0, 4, 4, 0, 4});
        const pgl::Polygon<RPoint> hole({1, 1, 2, 1, 2, 2, 1, 2});
        const RRegion region(outer, std::vector{hole});
        CHECK(region.twiceArea() == pgl::Rational<long long>(30));
        CHECK(region.isValid());
    }

    SUBCASE("the exact alias instantiates") {
        const pgl::EPolygon outer({0, 0, 10, 0, 10, 10, 0, 10});
        const pgl::EPolygon hole({2, 2, 4, 2, 4, 4, 2, 4});
        const pgl::EPolygonWithHoles region(outer, std::vector{hole});
        CHECK(region.twiceArea() == pgl::ERational(192));
        CHECK(region.isValid());
    }

    SUBCASE("converting the vertex type keeps the rings") {
        const Region source(outerSquare(), std::vector{smallHole()});
        const pgl::PolygonWithHoles<pgl::Point<long long>> converted(source);
        CHECK(converted.holeCount() == 1);
        CHECK(converted.twiceArea() == 192);
    }
}
