#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <functional>
#include <sstream>
#include <unordered_set>
#include <vector>

using Point = pgl::Point<int>;
using Polygon = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// A 10x10 square with a 2x2 square hole at (2,2)-(4,4): area 100 - 4 = 96.
static Polygon outerSquare() { return Polygon({0, 0, 10, 0, 10, 10, 0, 10}); }
static Polygon smallHole() { return Polygon({2, 2, 4, 2, 4, 4, 2, 4}); }
static Polygon otherHole() { return Polygon({6, 6, 8, 6, 8, 8, 6, 8}); }

TEST_CASE("PolygonWithHoles construction and ring access") {
    SUBCASE("default construction is the empty region") {
        const Region region;
        CHECK(region.isEmpty());
        CHECK(!region.hasHoles());
        CHECK(region.holeCount() == 0);
        CHECK(region.vertexCount() == 0);
        CHECK(region.twiceArea() == 0);
        CHECK(region.isUndefined());
    }

    SUBCASE("outer boundary only") {
        const Region region(outerSquare());
        CHECK(!region.isEmpty());
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
        const Polygon degeneratePoint({3, 3, 3, 3, 3, 3});
        const Polygon degenerateSegment({3, 3, 5, 3, 7, 3});
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
        region.addHole(Polygon({1, 1, 1, 1, 1, 1}));
        CHECK(region.holeCount() == 2);
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
        const Polygon centred({4, 4, 6, 4, 6, 6, 4, 6});
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
        const Polygon left({2, 2, 4, 2, 4, 4, 2, 4});
        const Polygon right({4, 4, 6, 4, 6, 6, 4, 6});
        CHECK(Region(outerSquare(), std::vector{left, right}).isValid());
    }

    SUBCASE("a hole touching the outer boundary is valid") {
        const Polygon touching({0, 2, 2, 2, 2, 4, 0, 4});
        CHECK(Region(outerSquare(), std::vector{touching}).isValid());
    }

    // The contract is interior disjointness and nothing more: ring boundaries
    // may meet however they like, including along whole stretches of edge. The
    // region pinches shut where they do, which the predicates account for.
    SUBCASE("a hole sharing a whole edge with the outer boundary is valid") {
        const Polygon slit({0, 0, 4, 0, 4, 4, 0, 4});
        CHECK(Region(outerSquare(), std::vector{slit}).isValid());
    }

    SUBCASE("holes sharing a whole edge are valid") {
        const Polygon left({2, 2, 5, 2, 5, 8, 2, 8});
        const Polygon right({5, 2, 8, 2, 8, 8, 5, 8});
        CHECK(Region(outerSquare(), std::vector{left, right}).isValid());
    }

    SUBCASE("a hole spanning the square from edge to edge is valid") {
        // Splits the region in two and leaves a whisker on either side.
        const Polygon band({0, 4, 10, 4, 10, 6, 0, 6});
        CHECK(Region(outerSquare(), std::vector{band}).isValid());
    }

    SUBCASE("overlapping holes are invalid") {
        const Polygon left({2, 2, 5, 2, 5, 5, 2, 5});
        const Polygon right({4, 4, 7, 4, 7, 7, 4, 7});
        CHECK(!Region(outerSquare(), std::vector{left, right}).isValid());
    }

    SUBCASE("a nested hole is invalid") {
        const Polygon big({2, 2, 8, 2, 8, 8, 2, 8});
        const Polygon inner({3, 3, 5, 3, 5, 5, 3, 5});
        CHECK(!Region(outerSquare(), std::vector{big, inner}).isValid());
    }

    SUBCASE("a hole escaping the outer boundary is invalid") {
        const Polygon escaping({8, 8, 12, 8, 12, 12, 8, 12});
        CHECK(!Region(outerSquare(), std::vector{escaping}).isValid());
    }

    SUBCASE("a hole wholly outside the outer boundary is invalid") {
        const Polygon outside({20, 20, 22, 20, 22, 22, 20, 22});
        CHECK(!Region(outerSquare(), std::vector{outside}).isValid());
    }

    SUBCASE("a self-intersecting ring is invalid") {
        const Polygon bowtie({0, 0, 10, 10, 10, 0, 0, 10});
        CHECK(!Region(bowtie).isValid());
    }

    SUBCASE("the empty region is valid and carries no hole") {
        CHECK(Region().isValid());
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
        CHECK(region.interiorContains(region.pointInside<Q>()));
    }

    SUBCASE("a hole covering the ear of the lex-min vertex") {
        // Polygon's O(n) route starts at the lexicographically smallest vertex
        // and would look for a point in the triangle it spans with its
        // neighbours; here a hole occupies it, so the witness has to come from
        // elsewhere.
        const Polygon hole({0, 1, 8, 1, 8, 9, 0, 9});
        const Region region(Polygon({0, 0, 10, 0, 10, 10, 0, 10}), std::vector{hole});
        REQUIRE(region.isValid());
        CHECK(region.interiorContains(region.pointInside<Q>()));
    }

    SUBCASE("a region split in two by a hole") {
        const Polygon band({0, 4, 10, 4, 10, 6, 0, 6});
        const Region region(Polygon({0, 0, 10, 0, 10, 10, 0, 10}), std::vector{band});
        REQUIRE(region.isValid());
        const auto witness = region.pointInside<Q>();
        CHECK(region.interiorContains(witness));
    }

    SUBCASE("a slit region: the witness avoids the whiskers") {
        const Polygon hole({0, 0, 4, 0, 4, 4, 0, 4});
        const Region region(Polygon({0, 0, 8, 0, 8, 8, 0, 8}), std::vector{hole});
        const auto witness = region.pointInside<Q>();
        CHECK(region.contains(witness));
        CHECK(region.interiorContains(witness));
        CHECK_FALSE(region.boundaryContains(witness));
    }

    SUBCASE("pointInsideInteriorContainedIn") {
        const Region region(outerSquare(), std::vector{smallHole()});
        CHECK(region.pointInsideInteriorContainedIn(outerSquare()));
        CHECK_FALSE(region.pointInsideInteriorContainedIn(smallHole()));
        CHECK_FALSE(region.pointInsideInteriorContainedIn(Polygon({20, 20, 22, 20, 22, 22, 20, 22})));
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
