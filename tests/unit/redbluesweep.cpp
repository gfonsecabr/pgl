#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pgl.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace {

using Point = pgl::Point<int>;
// Not `Polygon`: doctest.h drags in <windows.h> on MSVC, where that name is a
// GDI function.
using PolygonShape = pgl::Polygon<Point>;
using Segment = pgl::Segment<Point>;

std::vector<Segment> edgesOf(const PolygonShape &polygon) {
    std::vector<Segment> edges;
    for (const auto &e : polygon.edgesView()) {
        edges.emplace_back(e.min(), e.max());
    }
    return edges;
}

// What the sweep is answering, by testing every red edge against every blue one.
struct BruteForce {
    bool crosses = false;
    bool meets = false;
};

BruteForce bruteForce(const PolygonShape &red, const PolygonShape &blue) {
    BruteForce found;
    for (const Segment &one : edgesOf(red)) {
        for (const Segment &other : edgesOf(blue)) {
            found.crosses = found.crosses || one.crosses(other);
            found.meets = found.meets || one.intersects(other);
        }
    }
    return found;
}

// Star-shaped about a centre: sorting the sampled directions by angle makes the
// ring simple however jagged the radii are, which is the sweep's precondition.
PolygonShape star(std::mt19937 &rgen, int n, int span, Point centre) {
    std::uniform_int_distribution<int> radius(1, span);
    std::vector<Point> ring;
    for (int i = 0; i < n; ++i) {
        const double angle = 2 * std::numbers::pi * i / n;
        const int r = radius(rgen);
        const Point v((int)std::llround(r * std::cos(angle)),
                      (int)std::llround(r * std::sin(angle)));
        if (v != Point(0, 0)) {
            ring.push_back(centre + v);
        }
    }
    ring.erase(std::unique(ring.begin(), ring.end()), ring.end());
    if (ring.size() >= 3 && ring.front() == ring.back()) {
        ring.pop_back();
    }
    if (ring.size() < 3) {
        ring = {centre, centre + Point(1, 0), centre + Point(0, 1)};
    }
    return PolygonShape(ring);
}

}  // namespace

TEST_CASE("The three verdicts on boundaries that plainly do each") {
    const PolygonShape square({{0, 0}, {10, 0}, {10, 10}, {0, 10}});

    // Clear of it.
    CHECK(pgl::redBlueSweep(square.edgesView(),
                            PolygonShape({{20, 20}, {24, 20}, {24, 24}}).edgesView()) ==
          pgl::BoundaryContact::Disjoint);
    // Strictly inside it: the boundaries are still disjoint.
    CHECK(pgl::redBlueSweep(square.edgesView(),
                            PolygonShape({{3, 3}, {6, 3}, {6, 6}, {3, 6}}).edgesView()) ==
          pgl::BoundaryContact::Disjoint);
    // Through two of its sides.
    CHECK(pgl::redBlueSweep(square.edgesView(),
                            PolygonShape({{-2, 5}, {12, 4}, {12, 6}}).edgesView()) ==
          pgl::BoundaryContact::Crossing);
    // Sharing one vertex and nothing else.
    CHECK(pgl::redBlueSweep(square.edgesView(),
                            PolygonShape({{10, 10}, {14, 10}, {14, 14}}).edgesView()) ==
          pgl::BoundaryContact::Touching);
    // Resting along part of one side.
    CHECK(pgl::redBlueSweep(square.edgesView(),
                            PolygonShape({{2, 10}, {8, 10}, {8, 14}, {2, 14}}).edgesView()) ==
          pgl::BoundaryContact::Touching);

    // The relation is symmetric in the two colours.
    for (const PolygonShape &other :
         {PolygonShape({{20, 20}, {24, 20}, {24, 24}}),
          PolygonShape({{-2, 5}, {12, 4}, {12, 6}}),
          PolygonShape({{10, 10}, {14, 10}, {14, 14}})}) {
        CHECK(pgl::redBlueSweep(square.edgesView(), other.edgesView()) ==
              pgl::redBlueSweep(other.edgesView(), square.edgesView()));
    }
}

// The sweep's status structure is addressed by node: an edge's Left event
// records where it went and its Right event goes straight there. Getting that
// wrong loses an adjacency, and a lost adjacency is a missed pair -- which shows
// up only on inputs dense enough for the miss to matter. Small coordinates make
// shared endpoints, collinear overlaps and grazing contacts the common case.
TEST_CASE("Sweep agrees with brute force over overlapping random boundaries") {
    std::mt19937 rgen(1);
    std::uniform_int_distribution<int> offset(-14, 14);

    int disjoint = 0, touching = 0, crossing = 0;
    for (int trial = 0; trial < 1500; ++trial) {
        const int span = 4 + trial % 9;
        const PolygonShape red = star(rgen, 5 + trial % 11, span, Point(offset(rgen), offset(rgen)));
        const PolygonShape blue = star(rgen, 5 + trial % 13, span, Point(offset(rgen), offset(rgen)));
        const BruteForce found = bruteForce(red, blue);
        const auto verdict = pgl::redBlueSweep(red.edgesView(), blue.edgesView());

        switch (verdict) {
        case pgl::BoundaryContact::Disjoint:
            // A complete claim: nothing of one colour touches anything of the other.
            ++disjoint;
            REQUIRE_FALSE(found.meets);
            break;
        case pgl::BoundaryContact::Crossing:
            // A verified claim: some concrete pair really does cross.
            ++crossing;
            REQUIRE(found.crosses);
            break;
        case pgl::BoundaryContact::Touching:
            // They meet; the sweep is allowed not to have found the crossing,
            // but not to have invented the contact.
            ++touching;
            REQUIRE(found.meets);
            break;
        }
    }
    // Guards the generator: all three verdicts have to be reached, or the
    // checks above are asserting nothing about two of them.
    CHECK(disjoint > 200);
    CHECK(touching > 20);
    CHECK(crossing > 200);
}

namespace {

// Two combs with their teeth pointing at each other and every tip on the one
// vertical line x = 50: the tips are vertical edges of both colours sharing an
// abscissa and interleaved in height, and each tooth's corners put events of
// different kinds at one point. A blue tooth in `touch` is lowered onto the red
// tooth below it, so their tips overlap along the line; one in `cross` is also
// pushed five units past the line, so that it crosses the red tip. The combs
// are scaled by `scale` and moved by `shift`, which the verdict cannot notice.
template <class Number>
std::pair<pgl::Polygon<pgl::Point<Number>>, pgl::Polygon<pgl::Point<Number>>>
facingCombs(int teeth, const std::vector<bool>& touch, const std::vector<bool>& cross,
            int scale, Point shift) {
    const int tip = 50;
    const int spine = 100;
    const auto at = [&](int x, int y) {
        return pgl::Point<Number>(Number(x * scale + shift.x()), Number(y * scale + shift.y()));
    };
    std::vector<pgl::Point<Number>> red{at(0, 0), at(1, 0)};
    for (int i = 0; i < teeth; ++i) {
        red.push_back(at(1, 40 * i + 2));
        red.push_back(at(tip, 40 * i + 2));
        red.push_back(at(tip, 40 * i + 18));
        red.push_back(at(1, 40 * i + 18));
    }
    red.push_back(at(1, 40 * teeth));
    red.push_back(at(0, 40 * teeth));

    std::vector<pgl::Point<Number>> blue{at(spine - 1, 0), at(spine, 0), at(spine, 40 * teeth),
                                         at(spine - 1, 40 * teeth)};
    for (int j = teeth - 1; j >= 0; --j) {
        const bool lowered = touch[j] || cross[j];
        const int lo = 40 * j + (lowered ? 10 : 22);
        const int hi = 40 * j + (lowered ? 30 : 38);
        const int reach = cross[j] ? tip - 5 : tip;
        blue.push_back(at(spine - 1, hi));
        blue.push_back(at(reach, hi));
        blue.push_back(at(reach, lo));
        blue.push_back(at(spine - 1, lo));
    }
    return {pgl::Polygon<pgl::Point<Number>>(red), pgl::Polygon<pgl::Point<Number>>(blue)};
}

}  // namespace

// The events are radix sorted from about a thousand on, which the small
// boundaries above never reach. 120 teeth a side keep some 1400 events past the
// box filter. Integer coordinates take the radix sort and double ones the
// comparison sort, so the two must give the same verdict, and the right one --
// in three coordinate ranges the radix sort treats differently: narrow enough
// that most passes are skipped, straddling zero, and wide enough that every
// byte varies.
TEST_CASE("Sweep over many events at a shared abscissa") {
    const int teeth = 120;
    std::mt19937 rgen(5);
    std::bernoulli_distribution sometimes(0.05);
    const std::pair<int, Point> placements[] = {
        {1, Point(0, 0)}, {1, Point(-50, -2400)}, {200000, Point(-10000000, -480000000)}};

    for (const auto& [scale, shift] : placements) {
        for (int mode = 0; mode < 3; ++mode) {
            std::vector<bool> touch(teeth, false);
            std::vector<bool> cross(teeth, false);
            for (int j = 0; j < teeth; ++j) {
                touch[j] = mode >= 1 && sometimes(rgen);
                cross[j] = mode == 2 && sometimes(rgen);
            }
            touch[teeth / 2] = mode >= 1;
            cross[teeth / 3] = mode == 2;

            const auto [red, blue] = facingCombs<int>(teeth, touch, cross, scale, shift);
            const auto verdict = pgl::redBlueSweep(red.edgesView(), blue.edgesView());
            const auto [redDouble, blueDouble] =
                facingCombs<double>(teeth, touch, cross, scale, shift);
            CHECK(pgl::redBlueSweep(redDouble.edgesView(), blueDouble.edgesView()) == verdict);

            const pgl::BoundaryContact expected[] = {pgl::BoundaryContact::Disjoint,
                                                     pgl::BoundaryContact::Touching,
                                                     pgl::BoundaryContact::Crossing};
            CHECK(verdict == expected[mode]);
        }
    }
}
