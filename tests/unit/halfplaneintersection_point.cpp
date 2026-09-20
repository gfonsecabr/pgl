#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <optional>
#include <stdexcept>
#include <type_traits>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;

namespace {
const Halfplane yGE0(0, 0, 1, 0);   // y >= 0
const Halfplane xLE1(1, 0, 1, 1);   // x <= 1
const Halfplane yLE1(1, 1, 0, 1);   // y <= 1
const Halfplane xGE0(0, 1, 0, 0);   // x >= 0

Region unitSquare() {
    return Region({yGE0, xLE1, yLE1, xGE0});
}
Region quadrant() {
    return Region({yGE0, xGE0});
}
Region strip() {
    return Region({yGE0, yLE1});
}
Region emptyRegion() {
    Region k({yGE0});
    k.insert(Halfplane(1, -1, 0, -1));  // y <= -1
    return k;
}
}  // namespace

TEST_CASE("Point containment on a bounded region") {
    const Region k = unitSquare();
    CHECK(k.contains(Point(0, 0)));
    CHECK(k.contains(Point(1, 1)));
    CHECK(k.contains(Point(1, 0)));
    CHECK(!k.contains(Point(2, 0)));
    CHECK(!k.contains(Point(0, -1)));
    CHECK(!k.contains(Point(-1, 2)));

    CHECK(k.boundaryContains(Point(0, 0)));
    CHECK(k.boundaryContains(Point(1, 0)));
    CHECK(!k.boundaryContains(Point(2, 0)));

    CHECK(!k.interiorContains(Point(0, 0)));
    CHECK(!k.interiorContains(Point(1, 0)));
    CHECK(!k.interiorContains(Point(2, 2)));

    // A 2x2 square has a strictly interior lattice point.
    Region big({yGE0, xGE0, Halfplane(2, 0, 2, 1), Halfplane(1, 2, 0, 2)});
    CHECK(big.interiorContains(Point(1, 1)));
    CHECK(!big.boundaryContains(Point(1, 1)));
}

TEST_CASE("Point containment on unbounded regions") {
    const Region wedge = quadrant();
    CHECK(wedge.contains(Point(1000000, 1)));
    CHECK(wedge.interiorContains(Point(5, 5)));
    CHECK(wedge.boundaryContains(Point(5, 0)));
    CHECK(wedge.boundaryContains(Point(0, 0)));
    CHECK(!wedge.contains(Point(-1, 5)));

    const Region band = strip();
    CHECK(band.contains(Point(-1000, 0)));
    CHECK(band.interiorContains(Point(0, 0)) == false);
    CHECK(band.boundaryContains(Point(123, 1)));
    CHECK(!band.contains(Point(0, 2)));

    const Region half(yGE0);
    CHECK(half.contains(Point(3, 0)));
    CHECK(half.interiorContains(Point(3, 1)));
    CHECK(half.boundaryContains(Point(3, 0)));
    CHECK(!half.contains(Point(3, -1)));
}

TEST_CASE("Point containment on the plane and the empty region") {
    const Region plane;
    CHECK(plane.contains(Point(0, 0)));
    CHECK(plane.interiorContains(Point(0, 0)));
    CHECK(!plane.boundaryContains(Point(0, 0)));  // the plane has no boundary

    const Region none = emptyRegion();
    CHECK(!none.contains(Point(0, 0)));
    CHECK(!none.interiorContains(Point(0, 0)));
    CHECK(!none.boundaryContains(Point(0, 0)));
}

TEST_CASE("Point containment on degenerate regions") {
    const Region line({yGE0, Halfplane(1, 0, 0, 0)});  // the x axis
    CHECK(line.contains(Point(-9, 0)));
    CHECK(line.boundaryContains(Point(-9, 0)));  // a degenerate region is its own boundary
    CHECK(!line.interiorContains(Point(-9, 0)));
    CHECK(!line.contains(Point(0, 1)));
}

TEST_CASE("Point predicates: intersects, interiorsIntersect, separates, crosses") {
    const Region k = unitSquare();
    CHECK(k.intersects(Point(1, 1)));
    CHECK(!k.intersects(Point(2, 2)));

    // A point's interior is the point itself.
    const Region big({yGE0, xGE0, Halfplane(2, 0, 2, 1), Halfplane(1, 2, 0, 2)});
    CHECK(big.interiorsIntersect(Point(1, 1)));
    CHECK(!big.interiorsIntersect(Point(0, 0)));

    CHECK(!k.separates(Point(0, 0)));
    CHECK(!k.crosses(Point(0, 0)));
}

TEST_CASE("Point intersection construction") {
    const Region k = unitSquare();
    const auto hit = k.intersection<int>(Point(1, 1));
    REQUIRE(hit.has_value());
    CHECK(*hit == Point(1, 1));
    CHECK(!k.intersection<int>(Point(3, 3)).has_value());
}

TEST_CASE("Mixed coordinate types") {
    const Region k = unitSquare();
    using Q = pgl::Rational<int>;
    CHECK(k.contains(pgl::Point<Q>(Q(1, 2), Q(1, 2))));
    CHECK(k.interiorContains(pgl::Point<Q>(Q(1, 2), Q(1, 2))));
    CHECK(k.boundaryContains(pgl::Point<Q>(Q(1, 2), Q(0))));
    CHECK(!k.contains(pgl::Point<Q>(Q(3, 2), Q(1, 2))));
    CHECK(k.contains(pgl::Point<double>(0.5, 0.5)));
}

TEST_CASE("Symmetric predicates forward from lower-ranked shapes") {
    const Region k = unitSquare();
    // shapeRank routes a.intersects(b) to the higher-ranked shape's overload.
    CHECK(yGE0.intersects(k));
    CHECK(pgl::Segment<Point>(Point(-1, 0), Point(2, 0)).intersects(k));
    CHECK(pgl::Line<Point>(Point(-1, 0), Point(2, 0)).intersects(k));
}

TEST_CASE("Hausdorff distance to a point") {
    using Exact = pgl::ERational;
    const Region k = unitSquare();

    // The farthest point of the square from (3,4) is the opposite corner; the
    // point's own distance to the square is smaller, so it never decides.
    CHECK(k.squaredHausdorffDistance(Point(3, 4)) == Exact(25));
    CHECK(k.hausdorffDistanceL1(Point(3, 4)) == Exact(7));
    CHECK(k.hausdorffDistanceLInf(Point(3, 4)) == Exact(4));

    // A corner of the region measures to the corner across the diagonal.
    CHECK(k.squaredHausdorffDistance(Point(0, 0)) == Exact(2));
    CHECK(k.hausdorffDistanceL1(Point(0, 0)) == Exact(2));
    CHECK(k.hausdorffDistanceLInf(Point(0, 0)) == Exact(1));

    // The point answers the same, through its forwarder to the higher-ranked
    // region.
    CHECK(Point(3, 4).squaredHausdorffDistance<Exact>(k) == Exact(25));
    CHECK(Point(3, 4).hausdorffDistanceL1<Exact>(k) == Exact(7));
    CHECK(Point(3, 4).hausdorffDistanceLInf<Exact>(k) == Exact(4));

    // A degenerate region stands for its point set.
    const Region dot(Point(2, 2));
    CHECK(dot.squaredHausdorffDistance(Point(5, 6)) == Exact(25));
    CHECK(dot.hausdorffDistanceL1(Point(5, 6)) == Exact(7));
    CHECK(dot.hausdorffDistanceLInf(Point(5, 6)) == Exact(4));
}

TEST_CASE("The region's Hausdorff distance is exact on its own vertices") {
    using Exact = pgl::ERational;
    // y >= 0, 2y <= x, 3x + 2y <= 7: the vertices (0,0), (7/3,0) and (7/4,7/8)
    // are not lattice points.
    const Region k({Halfplane(Point(0, 0), Point(1, 0)), Halfplane(Point(2, 1), Point(0, 0)),
                    Halfplane(Point(3, -1), Point(1, 2))});
    REQUIRE(k.isBounded());

    CHECK(k.squaredHausdorffDistance(Point(0, 0)) == Exact(49, 9));
    CHECK(k.hausdorffDistanceL1(Point(0, 0)) == Exact(21, 8));
    CHECK(k.hausdorffDistanceLInf(Point(0, 0)) == Exact(7, 3));

    // The default result type is exact; another is honoured as asked, and an
    // integral one truncates.
    static_assert(std::is_same_v<decltype(k.hausdorffDistanceL1(Point(0, 0))), Exact>);
    CHECK(k.hausdorffDistanceL1<double>(Point(0, 0)) == doctest::Approx(21.0 / 8.0));
    CHECK(k.hausdorffDistanceL1<int>(Point(0, 0)) == 2);
}

TEST_CASE("A Hausdorff distance needs a bounded region") {
    // The distance from an unbounded set is infinite, which no result type
    // represents.
    CHECK_THROWS_AS((void)quadrant().squaredHausdorffDistance(Point(0, 0)), std::logic_error);
    CHECK_THROWS_AS((void)strip().hausdorffDistanceL1(Point(0, 0)), std::logic_error);
    CHECK_THROWS_AS((void)Region().hausdorffDistanceLInf(Point(0, 0)), std::logic_error);
    CHECK_THROWS_AS((void)Point(0, 0).hausdorffDistanceL1<pgl::ERational>(quadrant()), std::logic_error);
}
