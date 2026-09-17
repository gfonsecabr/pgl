#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using Triangle = pgl::Triangle<Point>;
using ERational = pgl::Rational<long long>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
// Upper half-plane y >= 0, unbounded.
Region upperHalf() {
    return Region({Halfplane(0, 0, 1, 0)});
}
}  // namespace

TEST_CASE("Region contains a triangle") {
    const Region k = box6();
    CHECK(k.contains(Triangle(Point(1, 1), Point(4, 1), Point(1, 4))));
    CHECK(k.interiorContains(Triangle(Point(1, 1), Point(4, 1), Point(1, 4))));
    CHECK(k.contains(Triangle(Point(0, 0), Point(6, 0), Point(0, 6))));   // on boundary
    CHECK(!k.interiorContains(Triangle(Point(0, 0), Point(6, 0), Point(0, 6))));
    CHECK(!k.contains(Triangle(Point(4, 4), Point(9, 4), Point(4, 9))));
}

TEST_CASE("A triangle contains a region") {
    const Triangle big(Point(-1, -1), Point(20, -1), Point(-1, 20));
    CHECK(big.contains(box6()));
    CHECK(!big.contains(upperHalf()));   // unbounded region escapes any triangle
}

TEST_CASE("Region intersects a triangle") {
    const Region k = box6();
    CHECK(k.intersects(Triangle(Point(4, 4), Point(9, 4), Point(4, 9))));
    CHECK(k.interiorsIntersect(Triangle(Point(4, 4), Point(9, 4), Point(4, 9))));
    CHECK(!k.intersects(Triangle(Point(8, 8), Point(10, 8), Point(8, 10))));
}

TEST_CASE("Intersection with a triangle stays a region") {
    const Region k = box6();
    const Region clip = k.intersection<int>(Triangle(Point(0, 0), Point(4, 0), Point(0, 4)));
    CHECK(clip.isBounded());
    CHECK(clip.twiceArea<long long>() == 16);   // right triangle legs 4
}

TEST_CASE("Separation with a triangle") {
    // A large thin triangle slicing across the box separates it.
    const Region k = box6();
    const Triangle knife(Point(-2, 2), Point(8, 3), Point(-2, 4));
    CHECK(knife.separates(k));
    CHECK(k.separates(knife));
    CHECK(k.crosses(knife));
    const Triangle tiny(Point(2, 2), Point(3, 2), Point(2, 3));
    CHECK(!tiny.separates(k));
}

TEST_CASE("Distance to a triangle") {
    const Region k = box6();
    const Triangle t(Point(9, 0), Point(12, 0), Point(9, 3));
    CHECK(k.squaredDistance<double>(t) == doctest::Approx(9.0));
    CHECK(k.squaredDistance<ERational>(t) == ERational(9));
    CHECK(k.squaredDistance<double>(Triangle(Point(2, 2), Point(4, 2), Point(2, 4))) == doctest::Approx(0.0));
}

TEST_CASE("Region meets a triangle in a convex region") {
    using PolygonShape = pgl::Polygon<Point>;
    using Piece = pgl::PolygonWithHoles<Point>;

    SUBCASE("a bounded region clips the triangle to their overlap") {
        // x >= 3 is the only one of the triangle's constraints that bites.
        const Triangle wedge(Point(3, -3), Point(9, 3), Point(3, 9));
        const auto met = box6().regularizedIntersection<int>(wedge);
        static_assert(std::is_same_v<decltype(met), const pgl::PolygonSet<Point>>);
        REQUIRE(met.componentCount() == 1);
        CHECK(met.twiceArea() == 2 * 18);
        CHECK(met.component(0) == Piece(PolygonShape({3, 0, 6, 0, 6, 6, 3, 6})));
        CHECK(met == wedge.regularizedIntersection<int>(box6()));
    }

    SUBCASE("an unbounded region is bounded by the triangle") {
        const Triangle straddling(Point(0, -2), Point(4, -2), Point(2, 2));
        const auto met = upperHalf().regularizedIntersection<int>(straddling);
        REQUIRE(met.componentCount() == 1);
        CHECK(met.twiceArea() == 4);
        CHECK(met.component(0) == Piece(PolygonShape({1, 0, 3, 0, 2, 2})));
        CHECK(met == straddling.regularizedIntersection<int>(upperHalf()));
    }

    SUBCASE("touching, missing and area-free operands give the empty set") {
        CHECK(upperHalf()
                  .regularizedIntersection<int>(Triangle(Point(0, -2), Point(4, -2), Point(2, 0)))
                  .empty());
        CHECK(upperHalf()
                  .regularizedIntersection<int>(Triangle(Point(0, -4), Point(4, -4), Point(2, -1)))
                  .empty());
        const Triangle flat(Point(1, 1), Point(2, 2), Point(3, 3));
        CHECK(box6().regularizedIntersection<int>(flat).empty());
        CHECK(flat.regularizedIntersection<int>(box6()).empty());
        // The empty region -- two contradictory constraints -- meets nothing,
        // while the whole plane, which is what a constraint-free region is,
        // answers the other operand.
        Region none(Halfplane(0, 0, 1, 0));
        none.insert(Halfplane(1, -1, 0, -1));
        REQUIRE(none.empty());
        const Triangle small(Point(0, 0), Point(2, 0), Point(0, 2));
        CHECK(none.regularizedIntersection<int>(small).empty());
        CHECK(Region().regularizedIntersection<int>(small).componentCount() == 1);
    }
}
