#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

TEST_CASE("Halfplane containment of OrientedSegment") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // upper = closed half-plane y >= 0 (left side of (0,0)->(4,0))
    const Halfplane upper({0, 0}, {4, 0});
    const OrientedSegment inside({3, 2}, {1, 1});     // strictly in interior
    const OrientedSegment boundary({3, 0}, {1, 0});   // on the boundary line y=0
    const OrientedSegment crossing({3, 2}, {1, -1}); // crosses into y < 0

    SUBCASE("Halfplane contains and interiorContains a wholly interior OrientedSegment") {
        CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
        CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
    }

    SUBCASE("Halfplane contains a boundary OrientedSegment but does not interiorContain it") {
        CHECK_MESSAGE(upper.contains(boundary), upper, " contains ", boundary);
        CHECK_FALSE_MESSAGE(upper.interiorContains(boundary), upper, " interiorContains ", boundary);
    }

    SUBCASE("Halfplane boundaryContains an OrientedSegment on the boundary line") {
        CHECK_MESSAGE(upper.boundaryContains(boundary), upper, " boundaryContains ", boundary);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(inside), upper, " boundaryContains ", inside);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(crossing), upper, " boundaryContains ", crossing);
    }

    SUBCASE("Halfplane does not contain an OrientedSegment that crosses the boundary") {
        CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains ", crossing);
    }
}

TEST_CASE("OrientedSegment containment of Halfplane") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const OrientedSegment inside({3, 2}, {1, 1});

    // A finite OrientedSegment can never contain an infinite Halfplane.
    CHECK_FALSE_MESSAGE(inside.contains(upper), inside, " contains ", upper);
    CHECK_FALSE_MESSAGE(inside.boundaryContains(upper), inside, " boundaryContains ", upper);
    CHECK_FALSE_MESSAGE(inside.interiorContains(upper), inside, " interiorContains ", upper);
}

TEST_CASE("OrientedSegment and Halfplane intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const OrientedSegment inside({3, 2}, {1, 1});
    const OrientedSegment boundary({3, 0}, {1, 0});
    const OrientedSegment crossing({3, 2}, {1, -1});
    const OrientedSegment outside({3, -2}, {1, -1});

    SUBCASE("inside: both intersect and both interiors intersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("boundary: intersects but interiors do not") {
        CHECK_MESSAGE(upper.intersects(boundary), upper, " intersects ", boundary);
        CHECK_FALSE_MESSAGE(upper.interiorsIntersect(boundary), upper, " interiorsIntersect ", boundary);
        CHECK_FALSE_MESSAGE(boundary.interiorsIntersect(upper), boundary, " interiorsIntersect ", upper);
    }

    SUBCASE("crossing: intersects and interiors intersect") {
        CHECK_MESSAGE(upper.intersects(crossing), upper, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(upper), crossing, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(crossing), upper, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(upper), crossing, " interiorsIntersect ", upper);
    }

    SUBCASE("outside: no intersection") {
        CHECK_FALSE_MESSAGE(upper.intersects(outside), upper, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(upper), outside, " intersects ", upper);
    }
}

TEST_CASE("OrientedSegment and Halfplane separates and crosses are always false") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const OrientedSegment inside({3, 2}, {1, 1});
    const OrientedSegment crossing({3, 2}, {1, -1});

    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(inside.separates(upper), inside, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);

    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);
    CHECK_FALSE_MESSAGE(inside.crosses(upper), inside, " crosses ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}

TEST_CASE("Halfplane intersection clips an OrientedSegment") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalSegment = pgl::Segment<RationalPoint>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});

    SUBCASE("crossing OS yields a clipped rational segment") {
        const OrientedSegment cross_os({3, 1}, {0, -1});
        const auto r = upper.intersection<Rational>(cross_os);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalSegment>(*r));
        CHECK_MESSAGE(std::get<RationalSegment>(*r) ==
                          RationalSegment(RationalPoint(Rational(3, 2), Rational(0)),
                                         RationalPoint(Rational(3), Rational(1))),
                      "upper ∩ cross_os");
    }

    SUBCASE("fully inside OS is returned unchanged") {
        const OrientedSegment os_in({3, 2}, {1, 1});
        const auto r = upper.intersection(os_in);
        using Seg = pgl::Segment<Point>;
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Seg>(*r));
        CHECK_MESSAGE(std::get<Seg>(*r) == Seg({1, 1}, {3, 2}), "upper ∩ os_in");
    }

    SUBCASE("fully outside OS yields empty") {
        const OrientedSegment os_out({1, -3}, {3, -1});
        CHECK_FALSE_MESSAGE(upper.intersection(os_out), "upper ∩ os_out should be empty");
    }
}
