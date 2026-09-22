#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "pgl.hpp"

using Coord = pgl::Point<int>;
using LabeledCoord = pgl::Point<int, std::string>;

using PolylineShape = pgl::Polyline<Coord>;
using ChainShape = pgl::MonotoneChain<Coord>;
using ConvexShape = pgl::Convex<Coord>;
using PolygonShape = pgl::Polygon<Coord>;
using RegionShape = pgl::PolygonWithHoles<Coord>;
using SetShape = pgl::PolygonSet<Coord>;

namespace {

// Whether `part` is a subsequence of `whole`, compared with labels.
template <class Point>
bool isSubsequence(const std::vector<Point>& part, const std::vector<Point>& whole) {
    std::size_t at = 0;
    for (const Point& p : whole) {
        if (at < part.size() && p == part[at] && p.label() == part[at].label()) {
            ++at;
        }
    }
    return at == part.size();
}

template <class Point>
bool isSubsequence(const std::vector<Point>& part, const std::vector<Point>& whole)
    requires(!pgl::detail::has_label_v<typename Point::LabelType>)
{
    std::size_t at = 0;
    for (const Point& p : whole) {
        if (at < part.size() && p == part[at]) {
            ++at;
        }
    }
    return at == part.size();
}

// The ring's vertices as a set, since a region may re-sort its rings.
template <class Point>
bool subsetOf(const std::vector<Point>& part, const std::vector<Point>& whole) {
    for (const Point& p : part) {
        if (std::find(whole.begin(), whole.end(), p) == whole.end()) {
            return false;
        }
    }
    return true;
}

// The squared Hausdorff distance against the squared tolerance: exactly when
// it is exact, allowing for its rounding when it is computed in double.
bool withinTolerance(double squaredHausdorff, double squaredTolerance) {
    return squaredHausdorff <= squaredTolerance * (1 + 1e-9) + 1e-9;
}

bool withinTolerance(const pgl::ERational& squaredHausdorff, int squaredTolerance) {
    return squaredHausdorff <= pgl::ERational(squaredTolerance);
}

std::vector<Coord> randomPoints(std::mt19937& random, int count, int range) {
    std::uniform_int_distribution<int> coordinate(0, range);
    std::vector<Coord> points;
    for (int i = 0; i < count; ++i) {
        points.emplace_back(coordinate(random), coordinate(random));
    }
    return points;
}

PolygonShape randomSimplePolygon(std::mt19937& random, int count, int range) {
    PolygonShape polygon(randomPoints(random, count, range));
    polygon.untangle();
    return polygon;
}

}  // namespace

TEST_CASE("polyline simplification keeps the endpoints and drops what the tolerance allows") {
    const PolylineShape line({0, 0, 1, 0, 2, 1, 3, 0, 4, 0, 10, 0});

    CHECK(line.simplified(0) == PolylineShape({0, 0, 1, 0, 2, 1, 3, 0, 10, 0}));
    CHECK(line.simplified(1) == PolylineShape({0, 0, 10, 0}));
    CHECK(line.simplified(pgl::ERational(1, 2)) == PolylineShape({0, 0, 2, 1, 3, 0, 10, 0}));
    CHECK(line.simplified(0.99) == PolylineShape({0, 0, 2, 1, 10, 0}));
    CHECK(line.simplified(-1) == line);

    PolylineShape inPlace = line;
    inPlace.simplify(1);
    CHECK(inPlace == PolylineShape({0, 0, 10, 0}));
}

TEST_CASE("the tolerance is compared exactly") {
    // (1,1) lies at squared distance exactly 1 from the segment (0,0)-(2,0).
    const PolylineShape line({0, 0, 1, 1, 2, 0});
    CHECK(line.simplified(1) == PolylineShape({0, 0, 2, 0}));
    CHECK(line.simplified(pgl::ERational(999999, 1000000)) == line);

    // (1,1) against the segment (0,0)-(3,1): squared distance 2/5 exactly.
    const PolylineShape slanted({0, 0, 1, 1, 3, 1});
    CHECK(slanted.simplified(pgl::ERational(2, 5)) == PolylineShape({0, 0, 3, 1}));
    CHECK(slanted.simplified(pgl::Rational<int64_t>(399, 1000)) == slanted);
}

TEST_CASE("a vertex beyond the end of its shortcut is measured to the endpoint") {
    // (5,0) lies on the line through (0,0) and (4,0), but past the segment's
    // end, at squared distance 1 from it.
    const PolylineShape line({0, 0, 5, 0, 4, 0});
    CHECK(line.simplified(1) == PolylineShape({0, 0, 4, 0}));
    CHECK(line.simplified(pgl::ERational(1, 2)) == line);
}

TEST_CASE("a closed polyline stays closed") {
    const PolylineShape loop({0, 0, 10, 0, 10, 1, 0, 1, 0, 0});
    const PolylineShape result = loop.simplified(4);
    REQUIRE(result.size() >= 2);
    CHECK(result.isClosed());
    CHECK(withinTolerance(result.squaredHausdorffDistance(loop), 4));
}

TEST_CASE("labels come through on the kept vertices and on the shape") {
    std::vector<LabeledCoord> points{{0, 0, "a"}, {1, 0, "b"}, {2, 5, "c"}, {3, 0, "d"}, {4, 0, "e"}};
    pgl::Polyline<LabeledCoord, int> line(points);
    line.label() = 7;
    const auto result = line.simplified(1);
    REQUIRE(result.size() == 3);
    CHECK(result[0].label() == "a");
    CHECK(result[1].label() == "c");
    CHECK(result[2].label() == "e");
    CHECK(result.label() == 7);
}

TEST_CASE("monotone chain simplification stays monotone") {
    const ChainShape chain({0, 0, 1, 1, 2, 0, 3, 1, 4, 0, 5, 3});
    CHECK(chain.simplified(1) == ChainShape({0, 0, 4, 0, 5, 3}));
    CHECK(chain.simplified(0) == chain);

    std::mt19937 random(11);
    for (int round = 0; round < 50; ++round) {
        const ChainShape sample(randomPoints(random, 20, 100));
        for (const int tolerance : {0, 4, 50, 400}) {
            const ChainShape result = sample.simplified(tolerance);
            CHECK(isSubsequence(result.vertices(), sample.vertices()));
            CHECK(result[0] == sample[0]);
            CHECK(result[result.size() - 1] == sample[sample.size() - 1]);
            CHECK(withinTolerance(result.squaredHausdorffDistance(sample), tolerance));
        }
    }
}

TEST_CASE("convex simplification keeps a convex subsequence") {
    const ConvexShape box({0, 0, 10, 0, 10, 1, 5, 2, 0, 1});
    CHECK(box.simplified(1) == ConvexShape({0, 0, 10, 1, 5, 2}, pgl::trusted));
    CHECK(box.simplified(4) == ConvexShape({0, 0, 10, 1}, pgl::trusted));
    CHECK(box.simplified(0) == box);

    std::mt19937 random(12);
    for (int round = 0; round < 50; ++round) {
        const ConvexShape hull(randomPoints(random, 30, 200));
        for (const int tolerance : {0, 9, 100, 2500}) {
            const ConvexShape result = hull.simplified(tolerance);
            CHECK(isSubsequence(result.vertices(), hull.vertices()));
            CHECK(result[0] == hull[0]);
            CHECK(ConvexShape(result.vertices()).size() == result.size());
            CHECK(withinTolerance(result.squaredHausdorffDistance(hull), tolerance));
        }
    }
}

TEST_CASE("polygon simplification drops collinear vertices at tolerance zero") {
    const PolygonShape square({0, 0, 2, 0, 4, 0, 4, 4, 2, 4, 0, 4, 0, 2});
    CHECK(square.simplified(0) == PolygonShape({0, 0, 4, 0, 4, 4, 0, 4}));
}

TEST_CASE("polygon simplification fills a notch within the tolerance") {
    const PolygonShape u({0, 0, 6, 0, 6, 6, 4, 6, 4, 2, 2, 2, 2, 6, 0, 6});
    CHECK(u.simplified(0) == u);
    CHECK(u.simplified(100) == PolygonShape({0, 0, 6, 0, 6, 6, 0, 6}));
    CHECK(u.simplified(4) == PolygonShape({0, 0, 6, 0, 6, 6, 4, 2, 0, 6}));
    // A tolerance wider than the polygon still leaves a simple polygon.
    const PolygonShape wide = u.simplified(1000000);
    CHECK(wide.isSimple());
    CHECK(wide.size() >= 3);
}

TEST_CASE("polygon simplification never lets two edges meet") {
    // Two long teeth whose tips a shortcut would join across the gap.
    const PolygonShape comb({0, 0, 20, 0, 20, 2, 2, 2, 2, 3, 20, 3, 20, 5, 0, 5});
    for (const int tolerance : {1, 4, 9, 25, 100}) {
        const PolygonShape result = comb.simplified(tolerance);
        CHECK(result.isSimple());
        CHECK(isSubsequence(result.vertices(), comb.vertices()));
        CHECK(withinTolerance(result.squaredHausdorffDistance(comb), tolerance));
    }
}

TEST_CASE("random polygons simplify to simple polygons within the tolerance") {
    std::mt19937 random(13);
    for (int round = 0; round < 60; ++round) {
        const PolygonShape polygon = randomSimplePolygon(random, 24, 100);
        REQUIRE(polygon.isSimple());
        for (const int tolerance : {0, 4, 25, 100, 900}) {
            const PolygonShape result = polygon.simplified(tolerance);
            CHECK(result.isSimple());
            CHECK(result[0] == polygon[0]);
            CHECK(subsetOf(result.vertices(), polygon.vertices()));
            CHECK(withinTolerance(result.squaredHausdorffDistance(polygon), tolerance));
        }
    }
}

TEST_CASE("floating-point polygons simplify too") {
    const pgl::Polygon<pgl::Point<double>> u({0, 0, 6, 0, 6, 6, 4, 6, 4, 2, 2, 2, 2, 6, 0, 6});
    CHECK(u.simplified(100) == pgl::Polygon<pgl::Point<double>>({0, 0, 6, 0, 6, 6, 0, 6}));
    CHECK(u.simplified(0.5).size() == 8);
}

TEST_CASE("a hole the outer boundary would sweep past stays inside") {
    // The bump above y = 100 is within the tolerance of the top edge, but the
    // hole sits inside it: dropping the bump would leave the hole outside.
    const RegionShape region(PolygonShape({0, 0, 100, 0, 100, 100, 50, 110, 0, 100}),
                             std::vector{PolygonShape({45, 101, 55, 101, 50, 105})});
    REQUIRE(region.isValid());
    const RegionShape result = region.simplified(200);
    CHECK(result.isValid());
    CHECK(result.holes().size() == 1);
    CHECK(withinTolerance(result.squaredHausdorffDistance(region), 200));

    // Without the hole the bump goes.
    const PolygonShape outer = region.outer().simplified(200);
    CHECK(outer == PolygonShape({0, 0, 100, 0, 100, 100, 0, 100}));
}

TEST_CASE("a hole touching the outer boundary keeps touching it only there") {
    const RegionShape region(PolygonShape({0, 0, 20, 0, 20, 20, 10, 21, 0, 20}),
                             std::vector{PolygonShape({5, 0, 15, 0, 10, 5})});
    REQUIRE(region.isValid());
    for (const int tolerance : {0, 1, 4, 100}) {
        const RegionShape result = region.simplified(tolerance);
        CHECK(result.isValid());
        CHECK(result.holes().size() == 1);
        CHECK(withinTolerance(result.squaredHausdorffDistance(region), tolerance));
    }
}

TEST_CASE("random regions simplify to valid regions within the tolerance") {
    std::mt19937 random(14);
    std::uniform_int_distribution<int> coordinate(0, 60);
    for (int round = 0; round < 25; ++round) {
        std::vector<pgl::Triangle<Coord>> triangles;
        for (int t = 0; t < 7; ++t) {
            triangles.emplace_back(Coord(coordinate(random), coordinate(random)),
                                   Coord(coordinate(random), coordinate(random)),
                                   Coord(coordinate(random), coordinate(random)));
        }
        const auto set = pgl::regularizedUnionOf<pgl::EPoint>(triangles);
        REQUIRE(set.isValid());
        for (const int tolerance : {0, 4, 36, 400}) {
            const auto result = set.simplified(tolerance);
            CHECK(result.isValid());
            CHECK(result.componentCount() == set.componentCount());
            CHECK(withinTolerance(result.squaredHausdorffDistance(set), tolerance));
            for (const auto& component : set) {
                const auto simplifiedComponent = component.simplified(tolerance);
                CHECK(simplifiedComponent.isValid());
                CHECK(simplifiedComponent.holes().size() == component.holes().size());
                CHECK(withinTolerance(simplifiedComponent.squaredHausdorffDistance(component), tolerance));
            }
        }
    }
}

TEST_CASE("two regions a shortcut would merge stay apart") {
    // Two squares one unit apart: any shortcut across the gap would touch the
    // other component.
    const SetShape set(std::vector{RegionShape(PolygonShape({0, 0, 10, 0, 10, 10, 5, 11, 0, 10})),
                                   RegionShape(PolygonShape({0, 12, 10, 12, 10, 20, 0, 20}))});
    REQUIRE(set.isValid());
    for (const int tolerance : {1, 4, 100}) {
        const SetShape result = set.simplified(tolerance);
        CHECK(result.isValid());
        CHECK(result.componentCount() == 2);
        CHECK(withinTolerance(result.squaredHausdorffDistance(set), tolerance));
    }
}

TEST_CASE("Shape forwards simplification to the alternatives that have it") {
    const PolygonShape u({0, 0, 6, 0, 6, 6, 4, 6, 4, 2, 2, 2, 2, 6, 0, 6});
    pgl::Shape<Coord> shape(u);
    CHECK(PolygonShape(shape.simplified(100)) == u.simplified(100));
    shape.simplify(100);
    CHECK(PolygonShape(shape) == u.simplified(100));

    const pgl::Shape<Coord> triangle(pgl::Triangle<Coord>(Coord(0, 0), Coord(4, 0), Coord(0, 4)));
    CHECK_THROWS_AS((void)triangle.simplified(1), pgl::unsupported_operation);
}
