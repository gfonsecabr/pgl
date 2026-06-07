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

