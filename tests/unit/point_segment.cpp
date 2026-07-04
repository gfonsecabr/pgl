#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Segments with a common endpoint") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    Point center(3,-1);
    auto x = GENERATE(-3,-2,-1,1,2,3);
    auto y = GENERATE(-3,-2,-1,1,2,3);
    Segment s(center, center+Point{x,y});

    // std::cout << s << " " << s.midpoint() << std::endl;
    CHECK_MESSAGE(s.contains(center), s, " contains ", center);
    CHECK_MESSAGE(s.boundaryContains(center), s, " boundaryContains ", center);
    CHECK_FALSE_MESSAGE(s.interiorContains(center), s, " interiorContains ", center);
    if(x%2 == 0 && y%2 == 0) {
        CHECK_MESSAGE(s.interiorContains(s.midpoint()), s, " interiorContains ", s.midpoint());
    }
    CHECK_FALSE_MESSAGE(s.interiorContains(s.midpoint()+Point(2,0)), s, " interiorContains ", s.midpoint()+Point(2,0));

    pgl::Segment<pgl::Point<double>> sd(pgl::Point<double>(center), pgl::Point<double>(center+Point{x,y}));

    CHECK_MESSAGE(sd.interiorContains(sd.midpoint()), sd, " interiorContains ", sd.midpoint());
}

TEST_CASE("Scaling segments") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    auto x = GENERATE(-3,-2,-1,0,1,2,3);
    auto y = GENERATE(-3,-2,-1,1,2,3);
    Point p(x,y);
    Segment s({},p);
    auto mul = GENERATE(2,3,4,5);

    CHECK_MESSAGE((mul*s).contains(p), mul*s, " contains ", p);
    CHECK_MESSAGE((mul*s).interiorContains(p), mul*s, " interiorContains ", p);
}

TEST_CASE("Point contains a Segment only when it degenerates to that point") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Point p(2, 3);
    const Segment degenerate({2, 3}, {2, 3});
    const Segment s({2, 3}, {4, 5});

    CHECK_MESSAGE(p.contains(degenerate), p, " contains ", degenerate);
    CHECK_MESSAGE(p.interiorContains(degenerate), p, " interiorContains ", degenerate);
    CHECK_FALSE_MESSAGE(p.boundaryContains(degenerate), p, " boundaryContains ", degenerate);
    CHECK_FALSE_MESSAGE(p.contains(s), p, " contains ", s);
}

TEST_CASE("Point and Segment intersection predicates") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment s({0, 0}, {4, 0});
    const Point mid(2, 0);       // strict interior
    const Point end(0, 0);       // endpoint, on the boundary
    const Point off(2, 2);       // off the segment

    SUBCASE("a point on the segment intersects it, both directions") {
        CHECK_MESSAGE(s.intersects(mid), s, " intersects ", mid);
        CHECK_MESSAGE(mid.intersects(s), mid, " intersects ", s);
        CHECK_MESSAGE(s.intersects(end), s, " intersects ", end);
        CHECK_MESSAGE(end.intersects(s), end, " intersects ", s);
    }

    SUBCASE("a point off the segment does not intersect it") {
        CHECK_FALSE_MESSAGE(s.intersects(off), s, " intersects ", off);
        CHECK_FALSE_MESSAGE(off.intersects(s), off, " intersects ", s);
    }

    SUBCASE("interiors meet only at a strictly interior point") {
        // A point's interior is the point itself, so this is interior containment.
        CHECK_MESSAGE(s.interiorsIntersect(mid), s, " interiorsIntersect ", mid);
        CHECK_MESSAGE(mid.interiorsIntersect(s), mid, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.interiorsIntersect(end), s, " interiorsIntersect ", end);
        CHECK_FALSE_MESSAGE(s.interiorsIntersect(off), s, " interiorsIntersect ", off);
    }
}

TEST_CASE("Point separates a Segment, but never the reverse, and they never cross") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment s({0, 0}, {4, 0});
    const Point mid(2, 0);   // strict interior splits the segment in two
    const Point end(0, 0);   // endpoint leaves one piece
    const Point off(2, 2);

    // A point cuts a segment iff it lies strictly inside it.
    CHECK_MESSAGE(mid.separates(s), mid, " separates ", s);
    CHECK_FALSE_MESSAGE(end.separates(s), end, " separates ", s);
    CHECK_FALSE_MESSAGE(off.separates(s), off, " separates ", s);

    // Removing a (1D) segment from a (0D) point can never leave two pieces.
    CHECK_FALSE_MESSAGE(s.separates(mid), s, " separates ", mid);

    // crosses needs mutual separation, which never holds here.
    CHECK_FALSE_MESSAGE(mid.crosses(s), mid, " crosses ", mid);
    CHECK_FALSE_MESSAGE(s.crosses(mid), s, " crosses ", mid);
}

TEST_CASE("Point and Segment intersection construction") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment s({0, 0}, {4, 0});

    SUBCASE("a point on the segment yields that point, both directions") {
        const Point mid(2, 0);
        const auto fromSeg = s.intersection(mid);
        const auto fromPt = mid.intersection(s);

        REQUIRE(fromSeg.has_value());
        CHECK(*fromSeg == mid);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == mid);
    }

    SUBCASE("a point off the segment yields nothing") {
        const Point off(2, 2);
        CHECK_FALSE(s.intersection(off).has_value());
        CHECK_FALSE(off.intersection(s).has_value());
    }
}

TEST_CASE("Rational points") {
    using Point = pgl::Point<pgl::Rational<int64_t>>;
    using Segment = pgl::Segment<Point>;

    auto x = GENERATE(-3,-2,-1,0,1,2,3);
    auto y = GENERATE(-3,-2,-1,1,2,3);
    auto x2 = GENERATE(-3,-2,-1,0,1,2,3);
    auto y2 = GENERATE(-3,-2,-1,1,2,3);
    Point p(x,y), p2(x2,y2);
    Segment s(p, p2);
    if(p != p2) {
        CHECK_MESSAGE(s.interiorContains(s.midpoint()), s, " interiorContains ", s.midpoint());

        auto ratio = GENERATE(1,2,3,4);
        Point q = pgl::Rational<int64_t>(ratio)*p/5 + pgl::Rational<int64_t>(5-ratio)*p2/5;

        CHECK_MESSAGE(s.interiorContains(q), s, " interiorContains ", q);
    }
}

TEST_CASE("Point and Segment squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Segment s({0, 0}, {4, 0});
    const Point p(1, 3);

    // The segment's farthest endpoint from p is (4,0) at squared distance 18,
    // which dominates the point-side term (squaredDistance(p, s) == 9).
    CHECK(s.squaredHausdorffDistance(p) == 18);
    CHECK(p.squaredHausdorffDistance(s) == 18);
}

