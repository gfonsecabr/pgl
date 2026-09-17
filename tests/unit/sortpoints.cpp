#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using Point = pgl::Point<int>;
using PointR = pgl::Point<pgl::Rational<int>>;

namespace {

// Small deterministic generator so the test data is identical across compilers
// (std::uniform_int_distribution is not portable between standard libraries).
struct Rng {
    std::uint64_t state = 0x9e3779b97f4a7c15ULL;
    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }
    int range(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

std::vector<Point> makePoints(int n, std::uint64_t seed, int lo, int hi) {
    Rng rng{seed};
    std::vector<Point> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v.emplace_back(rng.range(lo, hi), rng.range(lo, hi));
    }
    return v;
}

// The documented order spelled out directly: the angular half measured from the
// reference direction, then counterclockwise within the half, then farther
// first. Written to check sortAround's output, not to produce it.
bool specLess(const Point& reference, const Point& p, const Point& a, const Point& b) {
    const auto half = [&](const Point& q) {
        const auto side = pgl::orientationSign(p, reference, q);
        if (side > 0) return 0;
        if (side < 0) return 1;
        return pgl::dotSign(reference - p, q - p) >= 0 ? 0 : 1;
    };
    if (half(a) != half(b)) return half(a) < half(b);
    const auto turn = pgl::orientationSign(p, a, b);
    if (turn > 0) return true;
    if (turn < 0) return false;
    return p.squaredDistance<int>(a) > p.squaredDistance<int>(b);
}

// Whether an already sorted vector respects the documented order: the points
// equal to the center sit at the end, and the rest are sorted under the spec.
bool isSortedAround(const std::vector<Point>& points, const Point& p) {
    const auto last = std::find(points.begin(), points.end(), p);
    if (!std::all_of(last, points.end(), [&p](const Point& q) { return q == p; })) {
        return false;
    }
    if (last - points.begin() < 2) {
        return true;
    }
    const Point reference = *std::min_element(points.begin(), last);
    for (auto it = points.begin() + 1; it != last; ++it) {
        if (specLess(reference, p, *it, *(it - 1))) {
            return false;
        }
    }
    return true;
}

bool samePoints(std::vector<Point> a, std::vector<Point> b) {
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    return a == b;
}

}  // namespace

TEST_CASE("sortAround orders points counterclockwise from the smallest") {
    const Point center(0, 0);
    std::vector<Point> points{{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
    pgl::sortAround(points, center);

    // The lexicographically smallest point is (-1,0), so the order starts there
    // and turns counterclockwise: down, right, up.
    CHECK(points == std::vector<Point>{{-1, 0}, {0, -1}, {1, 0}, {0, 1}});
}

TEST_CASE("sortAround puts the farther of two points in the same direction first") {
    const Point center(0, 0);
    std::vector<Point> points{{1, 0}, {3, 0}, {2, 0}, {0, 5}};
    pgl::sortAround(points, center);

    // (0,5) is the smallest point, so it leads; the +x direction follows with
    // its three points ordered from farthest to nearest.
    CHECK(points == std::vector<Point>{{0, 5}, {3, 0}, {2, 0}, {1, 0}});
}

TEST_CASE("sortAround starts at the whole block sharing the reference direction") {
    const Point center(0, 0);
    // The smallest point (0,2) is not the farthest one in its own direction, so
    // the block leads with (0,6) rather than with the reference itself.
    std::vector<Point> points{{0, 2}, {0, 6}, {0, 4}, {1, -1}};
    pgl::sortAround(points, center);

    CHECK(points == std::vector<Point>{{0, 6}, {0, 4}, {0, 2}, {1, -1}});
    CHECK(isSortedAround(points, center));
}

TEST_CASE("sortAround leaves points equal to the center at the end") {
    const Point center(2, 3);
    std::vector<Point> points{{2, 3}, {4, 3}, {2, 3}, {2, 5}, {0, 3}};
    pgl::sortAround(points, center);

    REQUIRE(points.size() == 5);
    CHECK(points[3] == center);
    CHECK(points[4] == center);

    std::vector<Point> rest(points.begin(), points.begin() + 3);
    std::vector<Point> alone{{4, 3}, {2, 5}, {0, 3}};
    pgl::sortAround(alone, center);
    CHECK(rest == alone);
}

TEST_CASE("sortAround handles inputs too small to reorder") {
    const Point center(0, 0);
    std::vector<Point> none;
    pgl::sortAround(none, center);
    CHECK(none.empty());

    std::vector<Point> one{{3, 4}};
    pgl::sortAround(one, center);
    CHECK(one == std::vector<Point>{{3, 4}});

    std::vector<Point> centers{center, center, center};
    pgl::sortAround(centers, center);
    CHECK(centers == std::vector<Point>{center, center, center});
}

TEST_CASE("sortAround permutes its input into the documented order") {
    for (int n : {2, 3, 7, 40, 200}) {
        for (std::uint64_t seed = 1; seed <= 8; ++seed) {
            // A coarse grid on purpose: it produces duplicates, points collinear
            // with the center, and points equal to it.
            const std::vector<Point> input = makePoints(n, seed, -4, 4);
            for (const Point& center : {Point(0, 0), Point(-4, 2), Point(3, -1)}) {
                std::vector<Point> sorted = input;
                pgl::sortAround(sorted, center);
                CHECK(samePoints(sorted, input));
                CHECK(isSortedAround(sorted, center));
            }
        }
    }
}

TEST_CASE("sortAround stays exact when the points outrun the center's type") {
    using R = pgl::Rational<int>;
    const Point center(-2, 1);
    // Both points lie on the +x ray from the center, closer together than the
    // center's own integer coordinates can express.
    std::vector<PointR> points{{R(4, 3), R(1)}, {R(5, 3), R(1)}, {R(-3), R(1)}};
    pgl::sortAround(points, center);

    CHECK(points == std::vector<PointR>{{R(-3), R(1)}, {R(5, 3), R(1)}, {R(4, 3), R(1)}});
}

TEST_CASE("sortAround traces a simple star-shaped ring around an interior center") {
    using ConvexShape = pgl::Convex<Point>;
    using PolygonShape = pgl::Polygon<Point>;

    // The guarantee holds when the center is strictly inside the hull of the
    // points, which is what keeps every gap between consecutive directions
    // under half a turn. The coarse grid puts points on rays through the center
    // and three to a line often enough that both show up here.
    const auto kernelHolds = [](const std::vector<Point>& ring, const Point& c) {
        for (std::size_t i = 0; i < ring.size(); ++i) {
            if (pgl::orientationSign(ring[i], ring[(i + 1) % ring.size()], c) < 0) {
                return false;
            }
        }
        return true;
    };

    int tested = 0;
    for (int n : {5, 9, 30}) {
        for (std::uint64_t seed = 1; seed <= 12; ++seed) {
            std::vector<Point> points = makePoints(n, seed, -4, 4);
            std::sort(points.begin(), points.end());
            points.erase(std::unique(points.begin(), points.end()), points.end());
            for (const Point& center : {Point(0, 0), Point(1, -1), Point(-2, 2)}) {
                std::vector<Point> ring = points;
                ring.erase(std::remove(ring.begin(), ring.end(), center), ring.end());
                if (ring.size() < 3 || !ConvexShape(ring).interiorContains(center)) {
                    continue;  // outside the hull no order can answer; see the docs
                }
                pgl::sortAround(ring, center);
                CAPTURE(n);
                CAPTURE(seed);
                CAPTURE(center);
                CHECK(PolygonShape(ring, pgl::trusted).isSimple());
                CHECK(kernelHolds(ring, center));
                ++tested;
            }
        }
    }
    REQUIRE(tested > 20);  // the filter must not empty the test out
}

TEST_CASE("sortAround closes along the supporting line for a center on the hull") {
    using ConvexShape = pgl::Convex<Point>;
    using PolygonShape = pgl::Polygon<Point>;

    // On the hull boundary the largest gap is a half turn exactly and the ring
    // closes along that edge's line, so a third point on the line between the
    // two the closing edge joins breaks simplicity. Which of the two collinear
    // blocks that is depends on the side the remaining points fall on, so the
    // same stack answers both ways -- which is why the documented precondition
    // says strictly inside.
    const Point center(2, 0);
    const std::vector<Point> stackedAbove{{1, 2}, {2, -1}, {2, 1}, {2, 2}};
    const std::vector<Point> stackedBelow{{1, 2}, {2, -1}, {2, -2}, {2, 1}};
    for (const auto& points : {stackedAbove, stackedBelow}) {
        const ConvexShape hull(points);
        REQUIRE(hull.contains(center));
        REQUIRE_FALSE(hull.interiorContains(center));
    }

    std::vector<Point> ring = stackedAbove;
    pgl::sortAround(ring, center);
    CHECK(ring == std::vector<Point>{{1, 2}, {2, -1}, {2, 2}, {2, 1}});
    CHECK_FALSE(PolygonShape(ring, pgl::trusted).isSimple());

    ring = stackedBelow;
    pgl::sortAround(ring, center);
    CHECK(ring == std::vector<Point>{{1, 2}, {2, -2}, {2, -1}, {2, 1}});
    CHECK(PolygonShape(ring, pgl::trusted).isSimple());
}

TEST_CASE("hilbertSort permutes its input") {
    for (int n : {0, 1, 2, 5, 64, 300}) {
        const std::vector<Point> input = makePoints(n, 3, -50, 50);
        std::vector<Point> sorted = input;
        pgl::hilbertSort(sorted);
        CHECK(samePoints(sorted, input));
    }
}

TEST_CASE("hilbertSort keeps neighbors close together") {
    const std::vector<Point> input = makePoints(512, 5, 0, 255);

    std::vector<Point> sorted = input;
    pgl::hilbertSort(sorted);

    const auto travel = [](const std::vector<Point>& v) {
        long long total = 0;
        for (std::size_t i = 1; i < v.size(); ++i) {
            total += v[i - 1].squaredDistance<long long>(v[i]);
        }
        return total;
    };
    CHECK(travel(sorted) < travel(input) / 10);
}
