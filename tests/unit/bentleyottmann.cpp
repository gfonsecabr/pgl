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
    auto bf = pgl::detail::bruteForceCrossings(segs);
    auto bo = pgl::findCrossings(segs);

    CHECK(bf.size() == bo.size());
    std::sort(bf.begin(),bf.end());
    std::sort(bo.begin(),bo.end());
    CHECK(bf == bo);
}

TEST_CASE_TEMPLATE("Find intersections among segments", Point, pgl::Point<int>) {
    auto segs = randomSegments<Point>(15,10);
    auto bf = pgl::detail::bruteForceIntersections(segs);
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
    const auto bruteCrossings = sorted(pgl::detail::bruteForceCrossings(segs));
    CHECK(crossings.size() == 4);
    CHECK(crossings == bruteCrossings);

    const auto intersections = sorted(pgl::findIntersections(segs));
    const auto bruteIntersections = sorted(pgl::detail::bruteForceIntersections(segs));
    CHECK(intersections.size() == 10);
    CHECK(intersections == bruteIntersections);

    // The xy sweep never used the height expression and was correct throughout,
    // so it pins the expected answer independently.
    CHECK(sorted(pgl::detail::xyCrossings(segs)) == bruteCrossings);
    CHECK(sorted(pgl::detail::xyIntersections(segs)) == bruteIntersections);
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
    CHECK(pgl::findCrossings(shifted).size() == pgl::detail::bruteForceCrossings(shifted).size());
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

        const auto crossings = sorted(pgl::detail::bruteForceCrossings(segs));
        const auto intersections = sorted(pgl::detail::bruteForceIntersections(segs));
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

// Regression: a zero-length segment's two ends are one point, and the pass
// pairing segments that share an endpoint listed it there twice, so a single
// point came back as intersecting itself and detectIntersections answered true
// for inputs where no two segments meet.
TEST_CASE("A zero-length segment does not intersect itself") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const std::vector<Segment> lone = {Segment(Point(1, 3), Point(1, 3))};
    CHECK(pgl::findIntersections(lone).empty());
    CHECK_FALSE(pgl::detectIntersections(lone));

    const std::vector<Segment> apart = {Segment(Point(1, 3), Point(1, 3)),
                                        Segment(Point(4, 0), Point(9, 2))};
    CHECK(pgl::findIntersections(apart).empty());
    CHECK_FALSE(pgl::detectIntersections(apart));

    // A point on a segment still meets it, at an end or inside it.
    const std::vector<Segment> touching = {Segment(Point(4, 0), Point(9, 2)),
                                           Segment(Point(4, 0), Point(4, 0)),
                                           Segment(Point(0, 0), Point(10, 10)),
                                           Segment(Point(5, 5), Point(5, 5)),
                                           Segment(Point(10, 10), Point(10, 10))};
    CHECK(pgl::findIntersections(touching).size() == 3);
    CHECK(pgl::detectIntersections(touching));
    CHECK(pgl::findCrossings(touching).empty());
}

// A segment given k times is k segments of the input, as it is to the
// brute-force scan: a pair for every two of its copies, and every pair it makes
// with another segment made by each copy -- each coming back as itself.
TEST_CASE("Repeated segments are reported once per copy, labels included") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point, std::string>;

    const std::vector<Segment> segs = {
        Segment(Point(0, 0), Point(10, 10), "a1"),
        Segment(Point(0, 10), Point(10, 0), "b"),
        Segment(Point(0, 0), Point(10, 10), "a2"),
        Segment(Point(20, 0), Point(30, 0), "far"),
        Segment(Point(0, 0), Point(10, 10), "a3"),
    };
    const auto labels = [](const auto &pairs) {
        std::multiset<std::string> out;
        for (const auto &pair : pairs) {
            out.insert(pair[0].label() < pair[1].label()
                           ? pair[0].label() + "|" + pair[1].label()
                           : pair[1].label() + "|" + pair[0].label());
        }
        return out;
    };

    const std::multiset<std::string> crossings = {"a1|b", "a2|b", "a3|b"};
    CHECK(labels(pgl::findCrossings(segs)) == crossings);
    CHECK(labels(pgl::detail::xyCrossings(segs)) == crossings);

    const std::multiset<std::string> intersections = {"a1|a2", "a1|a3", "a2|a3",
                                                      "a1|b", "a2|b", "a3|b"};
    CHECK(labels(pgl::findIntersections(segs)) == intersections);
    CHECK(labels(pgl::detail::xyIntersections(segs)) == intersections);
}

TEST_CASE("Sweep agrees with brute force when segments repeat or have no length") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    auto sorted = [](auto v) { std::sort(v.begin(), v.end()); return v; };

    std::size_t points = 0, repeats = 0;
    for (unsigned seed = 0; seed < 600; ++seed) {
        std::mt19937 rgen(seed);
        const int span = 3 + static_cast<int>(seed % 6);
        std::uniform_int_distribution<int> coord(-span, span), percent(0, 99);
        std::vector<Segment> segs;
        const std::size_t n = 4 + seed % 20;
        while (segs.size() < n) {
            if (!segs.empty() && percent(rgen) < 25) {
                std::uniform_int_distribution<std::size_t> pick(0, segs.size() - 1);
                segs.push_back(segs[pick(rgen)]);
                ++repeats;
                continue;
            }
            const Point p(coord(rgen), coord(rgen));
            const Point q = percent(rgen) < 30 ? p : Point(coord(rgen), coord(rgen));
            points += p == q;
            segs.emplace_back(p, q);
        }

        // As multisets: a pair repeated by the brute-force scan is owed as
        // many times by the sweeps.
        const auto crossings = sorted(pgl::detail::bruteForceCrossings(segs));
        const auto intersections = sorted(pgl::detail::bruteForceIntersections(segs));
        REQUIRE(sorted(pgl::findCrossings(segs)) == crossings);
        REQUIRE(sorted(pgl::findIntersections(segs)) == intersections);
        REQUIRE(pgl::detectCrossings(segs) == !crossings.empty());
        REQUIRE(pgl::detectIntersections(segs) == !intersections.empty());
        REQUIRE(sorted(pgl::detail::xyCrossings(segs)) == crossings);
        REQUIRE(sorted(pgl::detail::xyIntersections(segs)) == intersections);
    }
    CHECK(points > 500);
    CHECK(repeats > 500);
}

// findIntersections and findCrossings pick between the Bentley-Ottmann sweep
// and a scan over bounding boxes. Every method must report the same pairs in
// the same order, so which one ran is never observable; forcing each in turn
// is also what keeps the sweep itself covered, now that the public functions
// mostly take the scan.
namespace methods {

using Method = pgl::detail::SegmentPairMethod;
using Rational = pgl::Rational<pgl::BigInt>;

// The degenerate stress inputs, with a copy of a few segments and a few
// zero-length ones added, mapped through `coordinate` to another number type.
template <class Number, class Coordinate>
std::vector<pgl::Segment<pgl::Point<Number>>> degenerate(unsigned seed, Coordinate coordinate) {
    std::mt19937 rgen(seed);
    std::vector<stress::Segment> base;
    switch (seed % 3) {
    case 0: base = stress::pencils(rgen, 3, 7); break;
    case 1: base = stress::grid(rgen, 6, 12 + static_cast<int>(seed % 25)); break;
    default: base = stress::collinear(rgen, 5, 5); break;
    }
    if (base.size() > 3) {
        base.push_back(base[seed % base.size()]);
        base.push_back(base[(seed / 3) % base.size()]);
        base.emplace_back(base[1].min(), base[1].min());
        base.emplace_back(base[2].max(), base[2].max());
    }
    std::vector<pgl::Segment<pgl::Point<Number>>> out;
    for (const auto &s : base) {
        out.emplace_back(pgl::Point<Number>(coordinate(s.min().x()), coordinate(s.min().y())),
                         pgl::Point<Number>(coordinate(s.max().x()), coordinate(s.max().y())));
    }
    return out;
}

using Relation = pgl::detail::SegmentPairRelation;

template <class Segments>
auto bruteForce(const Segments &segs, Relation relation) {
    switch (relation) {
    case Relation::intersects: return pgl::detail::bruteForceIntersections(segs);
    case Relation::crosses: return pgl::detail::bruteForceCrossings(segs);
    case Relation::interiorsIntersect: break;
    }
    return pgl::detail::bruteForceInteriorIntersections(segs);
}

template <class Segments>
void checkMethodsAgree(const Segments &segs) {
    auto sorted = [](auto v) { std::sort(v.begin(), v.end()); return v; };
    for (const Relation relation :
         {Relation::intersects, Relation::crosses, Relation::interiorsIntersect}) {
        CAPTURE(static_cast<int>(relation));
        const auto sweep = pgl::detail::findSegmentPairs<Rational>(segs, relation, Method::sweep);
        REQUIRE(pgl::detail::findSegmentPairs<Rational>(segs, relation, Method::scan) == sweep);
        REQUIRE(pgl::detail::findSegmentPairs<Rational>(segs, relation, Method::automatic) == sweep);
        const auto brute = bruteForce(segs, relation);
        for (const Method method : {Method::sweep, Method::scan, Method::automatic}) {
            REQUIRE(pgl::detail::detectSegmentPair<Rational>(segs, relation, method) == !brute.empty());
        }
        using Unlabelled = pgl::Segment<typename Segments::value_type::PointType>;
        std::vector<std::array<Unlabelled, 2>> unlabelled;
        for (const auto &pair : sweep) {
            unlabelled.push_back({Unlabelled(pair[0]), Unlabelled(pair[1])});
        }
        REQUIRE(sorted(unlabelled) == sorted(brute));
    }
    REQUIRE(pgl::findInteriorIntersections(segs) ==
            pgl::detail::findSegmentPairs<Rational>(segs, Relation::interiorsIntersect));
    REQUIRE(pgl::detectInteriorIntersections(segs) ==
            !pgl::detail::bruteForceInteriorIntersections(segs).empty());
}

}  // namespace methods

TEST_CASE("Every segment pair method reports the same pairs in the same order") {
    for (unsigned seed = 0; seed < 240; ++seed) {
        CAPTURE(seed);
        methods::checkMethodsAgree(methods::degenerate<int>(seed, [](int c) { return c; }));
    }
}

TEST_CASE("Segment pair methods agree over fractional and wide coordinates") {
    for (unsigned seed = 0; seed < 60; ++seed) {
        CAPTURE(seed);
        // Thirds are never exact in double, so every box the scan builds is a
        // rounded one and every sign near zero goes to the exact fallback.
        methods::checkMethodsAgree(methods::degenerate<pgl::ERational>(
            seed, [](int c) { return pgl::ERational(c, 3) + pgl::ERational(1, 7); }));
        // Past 2^53 neighbouring integers share a double.
        methods::checkMethodsAgree(methods::degenerate<long long>(
            seed, [](int c) { return (1LL << 60) + c; }));
    }
}

TEST_CASE("Segment pair methods agree when a coordinate overflows double") {
    using Number = pgl::ERational;
    const pgl::BigInt huge = pgl::detail::pow2(1100);
    // Every coordinate converts to an infinite double; the boxes then span the
    // whole plane and the exact predicates decide everything.
    for (unsigned seed = 0; seed < 12; ++seed) {
        CAPTURE(seed);
        methods::checkMethodsAgree(methods::degenerate<Number>(
            seed, [&huge](int c) { return Number(huge) * Number(c + 20); }));
    }
}

TEST_CASE("Automatic choice stays correct where every bounding box overlaps") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    // Long parallel diagonals side by side: all boxes overlap, nothing meets.
    std::vector<Segment> parallel;
    for (int i = 0; i < 400; ++i) {
        parallel.emplace_back(Point(3 * i, 0), Point(3 * i + 100000, 100000));
    }
    CHECK(pgl::findIntersections(parallel).empty());
    CHECK(pgl::findCrossings(parallel).empty());
    // One more segment across all of them.
    parallel.emplace_back(Point(0, 50000), Point(200000, 50000));
    CHECK(pgl::findCrossings(parallel).size() == 400);
    methods::checkMethodsAgree(parallel);
}

TEST_CASE("A scan abandoned by its budget stops early") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    std::vector<Segment> grid;
    for (int i = 0; i < 64; ++i) {
        grid.emplace_back(Point(0, 2 * i + 1), Point(200, 2 * i + 1));
        grid.emplace_back(Point(2 * i + 1, 0), Point(2 * i + 1, 200));
    }
    const pgl::detail::SegmentPairScan<Segment> scan(grid, pgl::detail::SegmentPairRelation::intersects);
    std::size_t found = 0;
    const auto count = [&scan, &found](auto i, auto j) {
        const bool meets = scan.meets(i, j);
        found += meets;
        return meets;
    };
    std::size_t consulted = 0;
    // Along x the horizontal segments come first, each meeting all 64
    // verticals, so stopping at the first consultation leaves half the pairs.
    CHECK_FALSE(scan.scan(false, count, [&consulted](const auto &) { return ++consulted == 1; }));
    CHECK(consulted == 1);
    CHECK(found < 64 * 64);

    found = 0;
    CHECK(scan.scan(true, count, [](const auto &) { return false; }));
    CHECK(found == 64 * 64);
}

TEST_CASE("Interior intersections are crossings and collinear overlaps with length") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point, std::string>;
    const std::vector<Segment> segs = {
        Segment(Point(0, 0), Point(10, 0), "base"),
        Segment(Point(5, 0), Point(15, 0), "overlap"),     // collinear, shares [5,10]
        Segment(Point(10, 0), Point(20, 0), "abutting"),   // collinear, meets base at a point
        Segment(Point(3, -3), Point(3, 3), "cross"),       // crosses base
        Segment(Point(7, 0), Point(7, 5), "tee"),          // endpoint inside base and overlap
        Segment(Point(0, 0), Point(-4, 6), "corner"),      // shares base's endpoint
        Segment(Point(2, 0), Point(2, 0), "point"),        // zero length, inside base
        Segment(Point(0, 0), Point(10, 0), "copy"),        // base again
        Segment(Point(30, 0), Point(30, 8), "up"),         // vertical
        Segment(Point(30, 4), Point(30, 12), "higher"),    // overlaps up
        Segment(Point(30, 12), Point(30, 20), "stacked"),  // meets higher at a point
    };
    std::multiset<std::string> labels;
    for (const auto &pair : pgl::findInteriorIntersections(segs)) {
        labels.insert(pair[0].label() < pair[1].label() ? pair[0].label() + "|" + pair[1].label()
                                                          : pair[1].label() + "|" + pair[0].label());
    }
    const std::multiset<std::string> expected = {
        "base|copy", "base|overlap", "base|cross", "copy|overlap", "copy|cross",
        "abutting|overlap", "higher|up"};
    CHECK(labels == expected);
    CHECK(pgl::detectInteriorIntersections(segs));
    methods::checkMethodsAgree(segs);

    const std::vector<Segment> touching = {segs[0], segs[2], segs[4], segs[5], segs[6]};
    CHECK(pgl::findInteriorIntersections(touching).empty());
    CHECK_FALSE(pgl::detectInteriorIntersections(touching));
}

// Degenerate families on which the sweep used to spend far more than its bound:
// a run of segments through one point tested every pair of the run although a
// long collinear block made nearly all of them overlaps, a vertical segment
// walked every segment through its endpoint to find none it crossed, and a
// detection finished a whole run after the answer was known. They are checked
// here for the answers, at a size where any mistake in the pairs listed
// outright shows.
namespace families {

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;

// Nested collinear segments, a few of them sharing an endpoint or only
// overlapping, crossed at distinct points by parallel transversals, one of
// them through the bundle's common endpoint.
std::vector<Segment> bundleWithTransversals(int m) {
    std::vector<Segment> segs;
    const int width = 2 * m + 2;
    for (int i = 1; i <= m; ++i) {
        segs.emplace_back(Point(-width - i, 0), Point(width + i, 0));
    }
    segs.emplace_back(Point(-width - 1, 0), Point(3, 0));
    segs.emplace_back(Point(1, 0), Point(width + m + 5, 0));
    for (int j = 0; j < m; ++j) {
        const int c = 2 * j - m;
        segs.emplace_back(Point(c - 1, -1), Point(c + 1, 1));
    }
    segs.emplace_back(Point(width, -2), Point(width + 2, 2));
    segs.emplace_back(Point(3, -3), Point(3, 3));
    return segs;
}

// Collinear horizontals with verticals standing on them, hanging from them,
// crossing them, reduced to a point on them, and overlapping one another.
std::vector<Segment> verticalsOnBundle(int m) {
    std::vector<Segment> segs;
    const int length = 4 * m + 10;
    for (int i = 0; i < m; ++i) {
        segs.emplace_back(Point(-i, 0), Point(length + i, 0));
    }
    for (int j = 0; j < m; ++j) {
        const int x = 4 * j + 1;
        switch (j % 5) {
        case 0: segs.emplace_back(Point(x, 0), Point(x, 1)); break;
        case 1: segs.emplace_back(Point(x, -1), Point(x, 0)); break;
        case 2: segs.emplace_back(Point(x, -1), Point(x, 1)); break;
        case 3: segs.emplace_back(Point(x, 0), Point(x, 0)); break;
        default:
            segs.emplace_back(Point(x, 0), Point(x, 2));
            segs.emplace_back(Point(x, 1), Point(x, 3));
            segs.emplace_back(Point(x, -2), Point(x, 0));
            break;
        }
    }
    // A vertical through the bundle's left end, and one where it ends.
    segs.emplace_back(Point(-(m - 1), -1), Point(-(m - 1), 1));
    segs.emplace_back(Point(length, 0), Point(length, 4));
    return segs;
}

// Segments through the origin, several of them on each line, kept apart until
// the origin by blockers that end there.
std::vector<Segment> starWithBlockers(int m) {
    std::vector<Segment> segs;
    for (int i = 1; i <= m; ++i) {
        segs.emplace_back(Point(-2, -4 * i), Point(2, 4 * i));
        if (i % 3 == 0) {
            segs.emplace_back(Point(-1, -2 * i), Point(3, 6 * i));
            segs.emplace_back(Point(-4, -8 * i), Point(1, 2 * i));
        }
    }
    for (int i = 1; i < m; ++i) {
        segs.emplace_back(Point(-4, -8 * i - 4), Point(0, 0));
    }
    segs.emplace_back(Point(-3, 0), Point(3, 0));
    return segs;
}

template <class Number, class Coordinate>
std::vector<pgl::Segment<pgl::Point<Number>>> mapped(const std::vector<Segment> &segs,
                                                   const Coordinate &coordinate) {
    std::vector<pgl::Segment<pgl::Point<Number>>> out;
    for (const Segment &s : segs) {
        out.emplace_back(pgl::Point<Number>(coordinate(s.min().x()), coordinate(s.min().y())),
                         pgl::Point<Number>(coordinate(s.max().x()), coordinate(s.max().y())));
    }
    return out;
}

void checkFamily(const std::vector<Segment> &segs) {
    methods::checkMethodsAgree(segs);
    methods::checkMethodsAgree(mapped<pgl::ERational>(
        segs, [](int c) { return pgl::ERational(c, 3) + pgl::ERational(1, 7); }));
    methods::checkMethodsAgree(mapped<long long>(segs, [](int c) { return (1LL << 40) + c; }));
}

}  // namespace families

TEST_CASE("Sweep agrees with brute force on a collinear bundle crossed by transversals") {
    for (const int m : {1, 2, 3, 7, 16}) {
        CAPTURE(m);
        families::checkFamily(families::bundleWithTransversals(m));
    }
    // Every transversal crosses the whole bundle at one point.
    CHECK(pgl::detail::bruteForceCrossings(families::bundleWithTransversals(16)).size() > 16 * 16);
}

TEST_CASE("Sweep agrees with brute force on verticals touching a collinear bundle") {
    for (const int m : {1, 2, 5, 11, 20}) {
        CAPTURE(m);
        families::checkFamily(families::verticalsOnBundle(m));
    }
}

TEST_CASE("Sweep agrees with brute force on a star through a point with blockers") {
    for (const int m : {2, 3, 6, 13}) {
        CAPTURE(m);
        families::checkFamily(families::starWithBlockers(m));
    }
}

TEST_CASE("A nested vertical zigzag is not simple") {
    for (const int size : {12, 40, 101}) {
        CAPTURE(size);
        std::vector<pgl::Point<int>> zigzag;
        for (int i = 0; i < size / 2; ++i) {
            zigzag.emplace_back(0, i);
            zigzag.emplace_back(0, size - i);
        }
        CHECK_FALSE(pgl::Polyline<pgl::Point<int>>(zigzag).isSimple());
        std::vector<pgl::Point<pgl::ERational>> exact;
        for (const auto &p : zigzag) {
            exact.emplace_back(pgl::ERational(p.x()), pgl::ERational(p.y()));
        }
        CHECK_FALSE(pgl::Polyline<pgl::Point<pgl::ERational>>(exact).isSimple());
        zigzag.emplace_back(1, size / 2);
        zigzag.push_back(zigzag.front());
        CHECK_FALSE(pgl::Polyline<pgl::Point<int>>(zigzag).isSimple());
    }
    // A simple comb whose teeth share abscissas with one another.
    std::vector<pgl::Point<int>> comb;
    for (int i = 0; i < 20; ++i) {
        comb.emplace_back(4 * i, 0);
        comb.emplace_back(4 * i, 5);
        comb.emplace_back(4 * i + 2, 5);
        comb.emplace_back(4 * i + 2, 0);
    }
    CHECK(pgl::Polyline<pgl::Point<int>>(comb).isSimple());
    comb.emplace_back(78, -1);
    comb.emplace_back(0, -1);
    comb.push_back(comb.front());
    CHECK(pgl::Polyline<pgl::Point<int>>(comb).isSimple());
    // Its closing edge run up along the first tooth instead.
    comb.back() = pgl::Point<int>(0, 1);
    CHECK_FALSE(pgl::Polyline<pgl::Point<int>>(comb).isSimple());
}
