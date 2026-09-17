#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using Convex = pgl::Convex<Point>;
using ERational = pgl::Rational<long long>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
Region vslab() {
    return Region({Halfplane(0, 1, 0, 0), Halfplane(3, 0, 3, 1)});
}
Convex square(int lo, int hi) {
    return Convex(std::vector<Point>{{lo, lo}, {hi, lo}, {hi, hi}, {lo, hi}});
}
}  // namespace

TEST_CASE("Region contains a convex polygon") {
    const Region k = box6();
    CHECK(k.contains(square(1, 3)));
    CHECK(k.interiorContains(square(1, 3)));
    CHECK(k.contains(square(0, 6)));
    CHECK(!k.interiorContains(square(0, 6)));
    CHECK(!k.contains(square(4, 8)));
}

TEST_CASE("A convex polygon contains a region") {
    CHECK(square(-1, 7).contains(box6()));
    CHECK(square(-1, 7).interiorContains(box6()));
    CHECK(!square(-1, 7).contains(vslab()));
    CHECK(square(0, 6).contains(box6()));
    CHECK(!square(0, 6).interiorContains(box6()));
}

TEST_CASE("Region intersects a convex polygon") {
    const Region k = box6();
    CHECK(k.intersects(square(3, 9)));
    CHECK(k.interiorsIntersect(square(3, 9)));
    CHECK(k.intersects(square(6, 9)));          // corner touch
    CHECK(!k.interiorsIntersect(square(6, 9)));
    CHECK(!k.intersects(square(7, 9)));
}

TEST_CASE("Intersection with a convex polygon stays a region") {
    const Region k = box6();
    const Region clip = k.intersection<int>(square(2, 5));
    CHECK(clip.isBounded());
    CHECK(clip.twiceArea<long long>() == 18);   // 3 x 3
    CHECK(k.intersection<int>(square(10, 12)).empty());
}

TEST_CASE("Separation and crossing with a convex polygon") {
    const Region k = box6();
    const Convex band(std::vector<Point>{{-1, 2}, {7, 2}, {7, 4}, {-1, 4}});
    CHECK(band.separates(k));
    CHECK(k.separates(band));
    CHECK(k.crosses(band));
    CHECK(!square(2, 4).separates(k));
}

TEST_CASE("An unbounded slab is cut by a spanning convex polygon") {
    const Region s = vslab();
    const Convex spanning(std::vector<Point>{{-1, 8}, {4, 8}, {4, 10}, {-1, 10}});
    CHECK(spanning.separates(s));
    const Convex narrow(std::vector<Point>{{0, 8}, {1, 8}, {1, 10}, {0, 10}});
    CHECK(!narrow.separates(s));
}

TEST_CASE("Distance to a convex polygon") {
    const Region k = box6();
    // A square three units to the right of the box, overlapping it in y.
    const Convex right(std::vector<Point>{{9, 2}, {11, 2}, {11, 4}, {9, 4}});
    CHECK(k.squaredDistance<double>(right) == doctest::Approx(9.0));
    CHECK(k.squaredDistance<ERational>(right) == ERational(9));
    CHECK(k.squaredDistance<double>(square(2, 4)) == doctest::Approx(0.0));
}

TEST_CASE("Region meets a convex polygon in a convex region") {
    using PolygonShape = pgl::Polygon<Point>;
    using Piece = pgl::PolygonWithHoles<Point>;

    // |x - 2| + |y - 2| <= 3.
    const Convex diamond(std::vector<Point>{{2, -1}, {5, 2}, {2, 5}, {-1, 2}});

    SUBCASE("a bounded region clips the convex polygon to their overlap") {
        const auto met = box6().regularizedIntersection<int>(square(2, 9));
        static_assert(std::is_same_v<decltype(met), const pgl::PolygonSet<Point>>);
        REQUIRE(met.componentCount() == 1);
        CHECK(met.twiceArea() == 2 * 16);
        CHECK(met.component(0) == Piece(PolygonShape({2, 2, 6, 2, 6, 6, 2, 6})));
        CHECK(met == square(2, 9).regularizedIntersection<int>(box6()));
    }

    SUBCASE("an unbounded slab takes both tips off the diamond") {
        const auto met = vslab().regularizedIntersection<int>(diamond);
        REQUIRE(met.componentCount() == 1);
        CHECK(met.twiceArea() == 26);
        CHECK(met.component(0) == Piece(PolygonShape({0, 1, 2, -1, 3, 0, 3, 4, 2, 5, 0, 3})));
        CHECK(met == diamond.regularizedIntersection<int>(vslab()));
    }

    SUBCASE("touching, missing and area-free operands give the empty set") {
        CHECK(box6().regularizedIntersection<int>(square(6, 9)).empty());
        CHECK(box6().regularizedIntersection<int>(square(9, 11)).empty());
        const Convex collapsed(std::vector<Point>{{1, 1}, {3, 3}});
        CHECK(box6().regularizedIntersection<int>(collapsed).empty());
        CHECK(collapsed.regularizedIntersection<int>(box6()).empty());
        // The empty region -- two contradictory constraints -- meets nothing,
        // while the whole plane, which is what a constraint-free region is,
        // answers the other operand.
        Region none(Halfplane(0, 0, 1, 0));
        none.insert(Halfplane(1, -1, 0, -1));
        REQUIRE(none.empty());
        CHECK(none.regularizedIntersection<int>(square(0, 2)).empty());
        CHECK(Region().regularizedIntersection<int>(square(0, 2)).componentCount() == 1);
    }
}
