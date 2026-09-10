#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>
#include <random>

#include "pgl.hpp"

template<class Point>
std::vector<pgl::Segment<Point>> randomSegments(size_t n1, size_t n2, size_t seed = 1) {
    std::mt19937 rgen(seed); // Seed
    std::set<pgl::Segment<Point>> ret_set;
    static std::uniform_int_distribution<int> base(-15,15);
    static std::uniform_int_distribution<int> len(0,20);

    for (size_t i = 0; i < n1; i++) {
        Point p(base(rgen),base(rgen));
        Point q(p.x()+ len(rgen), p.y() + len(rgen));
        if (p != q)
            ret_set.emplace(p,q);
    }

    static std::uniform_int_distribution<int> base2(-15,15);
    static std::uniform_int_distribution<int> len2(-6,6);
    static std::uniform_int_distribution<int> k2(0,2);
    Point center(base2(rgen),base2(rgen));
    for (size_t i = 0; i < n2; i++) {
        Point v(len2(rgen), len2(rgen));
        Point p1 = center + v;
        Point p2 = center - v*k2(rgen);
        if (p1 != p2)
            ret_set.emplace(p1,p2);
    }

    return std::vector<pgl::Segment<Point>>(ret_set.begin(),ret_set.end());
}

// FIXME: Breaks for points with labels and rational coordinates?
TEST_CASE_TEMPLATE("Find crossings among segments", Point, pgl::Point<int>) {
    auto segs = randomSegments<Point>(15,10);
    auto bf = pgl::bruteForceCrossings(segs);
    auto bo = pgl::findCrossings(segs);

    CHECK(bf.size() == bo.size());
    std::sort(bf.begin(),bf.end());
    std::sort(bo.begin(),bo.end());
    CHECK(bf == bo);
}

TEST_CASE_TEMPLATE("Find intersections among segments", Point, pgl::Point<int>) {
    auto segs = randomSegments<Point>(15,10);
    auto bf = pgl::bruteForceIntersections(segs);
    auto bo = pgl::findIntersections(segs);

    CHECK(bf.size() == bo.size());
    std::sort(bf.begin(),bf.end());
    std::sort(bo.begin(),bo.end());
    CHECK(bf == bo);
}

// Regression: a convex polygon is always simple, but isSimple()'s sweep path
// (n > 8) used to report a spurious self-intersection when the polygon has a
// unique leftmost vertex whose two incident edges share that left endpoint.
TEST_CASE("Convex polygon with a unique leftmost vertex is simple") {
    using Point = pgl::Point<int>;
    std::vector<Point> vertices = {
        {0,4},{4,0},{6,0},{8,1},{9,4},{8,7},{6,8},{4,8},{1,6},
    };
    pgl::Polygon<Point> poly(vertices);
    CHECK(poly.isSimple());
}

// Segment labels are metadata: ignored by equality/ordering/hashing, but they
// must survive the trip through the sweep line and reappear on the segments in
// the returned pairs.
TEST_CASE("Find{Crossings,Intersections} preserve segment labels in the result") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point, std::string>;

    // Two crossing diagonals plus a disjoint segment.
    std::vector<Segment> segs;
    segs.emplace_back(Point(0, 0), Point(10, 10), "up");
    segs.emplace_back(Point(0, 10), Point(10, 0), "down");
    segs.emplace_back(Point(20, 0), Point(30, 10), "away");

    // Endpoints -> expected label. The map ignores labels in its ordering, so a
    // returned segment looks up the label its endpoints were created with,
    // regardless of which order the algorithm reports the pair in.
    std::map<Segment, std::string> expected;
    for (const auto &s : segs)
        expected[s] = s.label();

    auto crossings = pgl::findCrossings(segs);
    REQUIRE(crossings.size() == 1);
    for (const auto &pair : crossings) {
        for (const auto &s : pair) {
            REQUIRE(expected.count(s) == 1);
            CHECK(s.label() == expected.at(s));
        }
    }

    auto intersections = pgl::findIntersections(segs);
    REQUIRE(intersections.size() == 1);
    for (const auto &pair : intersections) {
        for (const auto &s : pair) {
            REQUIRE(expected.count(s) == 1);
            CHECK(s.label() == expected.at(s));
        }
    }
}

TEST_CASE_TEMPLATE("Detect crossings and intersections among segments", Point, pgl::Point<int>) {
    std::vector<pgl::Segment<Point>> segs;
    for(int i = 1; i < 5; i++) {
        segs.emplace_back(i,i*i,i+1,(i+1)*(i+1));
    }

    bool cross = pgl::detectCrossings(segs);
    CHECK_FALSE(cross);
    bool isec = pgl::detectIntersections(segs);
    CHECK(isec);
    segs.emplace_back(0,0,11,20);
    bool cross2 = pgl::detectCrossings(segs);
    CHECK(cross2);
}

// Regression: six segments from a CGSHOP2022 instance, over coordinates large
// enough that the status order's height expression -- degree three in the
// coordinates -- overflows 64 bits. With `long long` coordinates left
// unpromoted the expression wrapped, the wrapped coefficient was then widened
// again for the exact comparison, and the two arithmetics stopped agreeing:
// heightSign returned the wrong sign, the status tree ordered two segments
// against their actual heights, a crossing went unreported, and find() later
// missed a segment the tree still held, so processRIGHT erased end().
//
// The six share four endpoints, which is what puts the near-degenerate pairs in
// front of the comparator. They are checked against brute force at every
// coordinate width because only the widest overflowed: the same input over
// `int` never reached the bad path.
TEST_CASE_TEMPLATE("Sweep agrees with brute force on large shared-endpoint coordinates",
                   Number, int, long, long long) {
    using Point = pgl::Point<Number>;
    using Segment = pgl::Segment<Point>;

    const std::vector<Segment> segs = {
        Segment(Point(8001103, 20643050), Point(7890830, 20858656)),
        Segment(Point(8001103, 20643050), Point(8913516, 20211134)),
        Segment(Point(8538875, 20320635), Point(8443546, 20293985)),
        Segment(Point(8538875, 20320635), Point(7890830, 20858656)),
        Segment(Point(8913516, 20211134), Point(7890830, 20858656)),
        Segment(Point(7524275, 20403185), Point(8307188, 21091424)),
    };

    auto sorted = [](auto v) { std::sort(v.begin(), v.end()); return v; };

    const auto crossings = sorted(pgl::findCrossings(segs));
    const auto bruteCrossings = sorted(pgl::bruteForceCrossings(segs));
    CHECK(crossings.size() == 4);
    CHECK(crossings == bruteCrossings);

    const auto intersections = sorted(pgl::findIntersections(segs));
    const auto bruteIntersections = sorted(pgl::bruteForceIntersections(segs));
    CHECK(intersections.size() == 10);
    CHECK(intersections == bruteIntersections);

    // The xy sweep never used the height expression and was correct throughout,
    // so it pins the expected answer independently.
    CHECK(sorted(pgl::xyCrossings(segs)) == bruteCrossings);
    CHECK(sorted(pgl::xyIntersections(segs)) == bruteIntersections);
}

// The same six translated so that every coordinate is small. An exact integer
// translation changes no orientation predicate and no event order, so the sweep
// owes the same answer -- and while the height expression was overflowing it
// gave one here and not above, which is what identified the magnitude rather
// than the combinatorics as the trigger.
TEST_CASE("Sweep is invariant under an exact integer translation") {
    using Point = pgl::Point<long long>;
    using Segment = pgl::Segment<Point>;

    const std::vector<Point> ends = {
        {8001103, 20643050}, {7890830, 20858656}, {8001103, 20643050},
        {8913516, 20211134}, {8538875, 20320635}, {8443546, 20293985},
        {8538875, 20320635}, {7890830, 20858656}, {8913516, 20211134},
        {7890830, 20858656}, {7524275, 20403185}, {8307188, 21091424},
    };

    std::vector<Segment> here, shifted;
    for (std::size_t i = 0; i + 1 < ends.size(); i += 2) {
        const Point p = ends[i], q = ends[i + 1];
        here.emplace_back(p, q);
        shifted.emplace_back(Point(p.x() - 7524275, p.y() - 20211134),
                             Point(q.x() - 7524275, q.y() - 20211134));
    }

    CHECK(pgl::findCrossings(here).size() == pgl::findCrossings(shifted).size());
    CHECK(pgl::findIntersections(here).size() == pgl::findIntersections(shifted).size());
    CHECK(pgl::findCrossings(shifted).size() == pgl::bruteForceCrossings(shifted).size());
}

// Regression: the sweep named the alternatives of the variant returned by
// Segment::intersection as Point<Rational, NoLabel>, but a point label rides
// along with the point type, so for labelled points that alternative was not in
// the variant at all and the whole algorithm failed to instantiate.
TEST_CASE("Find{Crossings,Intersections} accept labelled points and keep the labels") {
    using Point = pgl::Point<int, std::string>;
    using Segment = pgl::Segment<Point>;

    std::vector<Segment> segs;
    segs.emplace_back(Point(0, 0, "a"), Point(10, 10, "a"));
    segs.emplace_back(Point(0, 10, "b"), Point(10, 0, "b"));

    const auto crossings = pgl::findCrossings(segs);
    REQUIRE(crossings.size() == 1);
    for (const auto &s : crossings.front()) {
        CHECK(s.min().label() == s.max().label());
        CHECK_FALSE(s.min().label().empty());
    }

    const auto intersections = pgl::findIntersections(segs);
    REQUIRE(intersections.size() == 1);
    for (const auto &s : intersections.front()) {
        CHECK_FALSE(s.min().label().empty());
    }
}

// The status tree keeps the segments crossing at one point as a run, and turns
// that run over in place when the sweep passes the point. Everything about that
// is decided by how many segments meet, whether any of them are collinear, and
// what sits just past the run's two ends -- so the inputs that exercise it are
// the degenerate ones, and they have to be generated rather than written out.
namespace stress {

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;

// A tiny coordinate grid makes concurrent crossings, collinear overlaps and
// shared endpoints the common case rather than the exception.
std::vector<Segment> grid(std::mt19937 &rgen, int span, int n) {
    std::uniform_int_distribution<int> coord(-span, span);
    std::set<Segment> uniq;
    while ((int)uniq.size() < n) {
        Point p(coord(rgen), coord(rgen)), q(coord(rgen), coord(rgen));
        if (p != q) {
            uniq.emplace(p, q);
        }
    }
    return {uniq.begin(), uniq.end()};
}

// Several segments through each of a few common points: runs many segments long.
std::vector<Segment> pencils(std::mt19937 &rgen, int centers, int perCenter) {
    std::uniform_int_distribution<int> coord(-12, 12);
    std::uniform_int_distribution<int> offset(-8, 8);
    std::set<Segment> uniq;
    for (int i = 0; i < centers; ++i) {
        const Point o(coord(rgen), coord(rgen));
        for (int j = 0; j < perCenter; ++j) {
            const Point v(offset(rgen), offset(rgen));
            if (v != Point(0, 0)) {
                uniq.emplace(o - v, o + v);
            }
        }
    }
    return {uniq.begin(), uniq.end()};
}

// Segments drawn from a handful of lines, so collinear overlapping pairs are
// everywhere and a run holds several of them at once.
std::vector<Segment> collinear(std::mt19937 &rgen, int lines, int perLine) {
    std::uniform_int_distribution<int> slope(-2, 2);
    std::uniform_int_distribution<int> shift(-6, 6);
    std::uniform_int_distribution<int> at(-9, 9);
    std::set<Segment> uniq;
    for (int i = 0; i < lines; ++i) {
        const int m = slope(rgen), b = shift(rgen);
        for (int j = 0; j < perLine; ++j) {
            const int x1 = at(rgen), x2 = at(rgen);
            if (x1 != x2) {
                uniq.emplace(Point(x1, m * x1 + b), Point(x2, m * x2 + b));
            }
        }
    }
    return {uniq.begin(), uniq.end()};
}

}  // namespace stress

TEST_CASE("Sweep agrees with brute force over degenerate random inputs") {
    auto sorted = [](auto v) { std::sort(v.begin(), v.end()); return v; };

    std::size_t crossingsSeen = 0, intersectionsSeen = 0;
    for (unsigned seed = 0; seed < 400; ++seed) {
        std::mt19937 rgen(seed);
        std::vector<stress::Segment> segs;
        switch (seed % 4) {
        case 0: segs = stress::pencils(rgen, 3, 7); break;
        case 1: segs = stress::grid(rgen, 6, 12 + (int)(seed % 25)); break;
        case 2: segs = stress::grid(rgen, 14, 12 + (int)(seed % 25)); break;
        default: segs = stress::collinear(rgen, 5, 5); break;
        }

        const auto crossings = sorted(pgl::bruteForceCrossings(segs));
        const auto intersections = sorted(pgl::bruteForceIntersections(segs));
        crossingsSeen += crossings.size();
        intersectionsSeen += intersections.size();

        REQUIRE(sorted(pgl::findCrossings(segs)) == crossings);
        REQUIRE(sorted(pgl::findIntersections(segs)) == intersections);
        REQUIRE(pgl::detectCrossings(segs) == !crossings.empty());
        REQUIRE(pgl::detectIntersections(segs) == !intersections.empty());
    }
    // Guards the generators: an input set that stopped producing degeneracies
    // would still pass every check above.
    CHECK(crossingsSeen > 20000);
    CHECK(intersectionsSeen > 25000);
}
