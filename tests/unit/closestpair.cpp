#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "pgl.hpp"

namespace {

using IntPoint = pgl::Point<int>;
using IntSegment = pgl::Segment<IntPoint>;

static_assert(std::is_same_v<decltype(pgl::closestPair(
                                 std::declval<const std::vector<IntPoint>&>())),
                             IntSegment>);
static_assert(std::is_same_v<decltype(pgl::closestPair(
                                 std::declval<const std::vector<pgl::Point<double>>&>())),
                             pgl::Segment<pgl::Point<double>>>);
// The input point type is kept, so labelled points come back labelled.
static_assert(std::is_same_v<decltype(pgl::closestPair(
                                 std::declval<const std::vector<pgl::Point<int, std::string>>&>())),
                             pgl::Segment<pgl::Point<int, std::string>>>);
// The brute-force cutoff is picked per coordinate family, on the type the
// arithmetic happens in rather than the stored coordinate — so an int point,
// whose squared distances are computed in int64_t, is charged as a primitive.
template <class Point>
inline constexpr std::size_t thresholdFor =
    pgl::detail::closestPairBruteForceThreshold<pgl::detail::closest_pair_coordinate_t<Point>>;

static_assert(thresholdFor<IntPoint> == 6);
static_assert(thresholdFor<pgl::Point<double>> == 6);
static_assert(thresholdFor<pgl::Point<pgl::Rational<int>>> == 12);
static_assert(thresholdFor<pgl::Point<pgl::BigInt>> == 4);
static_assert(thresholdFor<pgl::Point<pgl::ERational>> == 4);

// Deterministic, portable generator: std::uniform_int_distribution is not
// stable across standard libraries.
struct Rng {
    std::uint64_t state;
    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }
    int range(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

// Reference implementation: every pair, same comparisons.
template <class Point>
auto bruteForceClosestPair(const std::vector<Point>& points) {
    using Coordinate = pgl::detail::promoted_number_t<typename Point::NumberType>;
    std::size_t bestI = 0, bestJ = 1;
    auto best = points[0].template squaredDistance<Coordinate>(points[1]);
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            const auto squared = points[i].template squaredDistance<Coordinate>(points[j]);
            if (squared < best) {
                best = squared;
                bestI = i;
                bestJ = j;
            }
        }
    }
    return std::pair{pgl::Segment<Point>(points[bestI], points[bestJ]), best};
}

// The closest pair need not be unique, so compare the achieved distance and
// check that the returned endpoints really are input points.
template <class Point>
void checkAgainstBruteForce(const std::vector<Point>& points) {
    using Coordinate = pgl::detail::promoted_number_t<typename Point::NumberType>;
    const auto expected = bruteForceClosestPair(points);
    const auto found = pgl::closestPair(points);

    CHECK(found.min().template squaredDistance<Coordinate>(found.max()) == expected.second);
    CHECK(std::find(points.begin(), points.end(), found.min()) != points.end());
    CHECK(std::find(points.begin(), points.end(), found.max()) != points.end());
}

// Runs the recursion at an explicit cutoff, bypassing the tuned default, and
// checks each run against the expected squared distance. The cutoff is a
// template parameter, so the thresholds to try are a compile-time list.
template <std::size_t... Thresholds, class Point, class Distance>
void checkEveryThreshold(const std::vector<Point>& points, const Distance& expected) {
    const auto check = [&](const auto& found) {
        CHECK(found.min().template squaredDistance<Distance>(found.max()) == expected);
    };
    (check(pgl::detail::closestPairDriver<Thresholds>(points)), ...);
}

}  // namespace

TEST_CASE("closestPair on two points returns them") {
    const std::vector<IntPoint> points{{7, -3}, {-2, 5}};
    CHECK(pgl::closestPair(points) == IntSegment({7, -3}, {-2, 5}));
}

TEST_CASE("closestPair returns the endpoints in segment order") {
    const std::vector<IntPoint> points{{0, 0}, {100, 0}, {51, 1}, {50, 0}};
    const auto found = pgl::closestPair(points);

    CHECK(found == IntSegment({50, 0}, {51, 1}));
    CHECK(found.min() == IntPoint(50, 0));
    CHECK(found.max() == IntPoint(51, 1));
}

TEST_CASE("closestPair finds a coincident pair at distance zero") {
    const std::vector<IntPoint> points{{0, 0}, {4, 9}, {-6, 2}, {4, 9}, {30, 30}};
    const auto found = pgl::closestPair(points);

    CHECK(found == IntSegment({4, 9}, {4, 9}));
    CHECK(found.min().squaredDistance(found.max()) == 0);
}

TEST_CASE("closestPair handles points sharing an abscissa") {
    // Every point is on the split line, so the recursion resolves the answer
    // entirely inside the strip.
    std::vector<IntPoint> points;
    for (int y = 0; y < 32; ++y) {
        points.emplace_back(5, 3 * y);
    }
    points.emplace_back(5, 3 * 31 + 1);

    checkAgainstBruteForce(points);
    CHECK(pgl::closestPair(points) == IntSegment({5, 93}, {5, 94}));
}

TEST_CASE("closestPair handles collinear points on a diagonal") {
    std::vector<IntPoint> points;
    for (int i = 0; i < 40; ++i) {
        points.emplace_back(2 * i, 2 * i);
    }
    points.emplace_back(41, 41);

    checkAgainstBruteForce(points);
    CHECK(pgl::closestPair(points) == IntSegment({40, 40}, {41, 41}));
}

TEST_CASE("closestPair matches brute force on random integer points") {
    Rng rng{2024};
    for (int trial = 0; trial < 40; ++trial) {
        const int count = rng.range(2, 60);
        std::vector<IntPoint> points;
        for (int i = 0; i < count; ++i) {
            // A small coordinate range makes ties and duplicates common.
            points.emplace_back(rng.range(-25, 25), rng.range(-25, 25));
        }
        checkAgainstBruteForce(points);
    }
}

TEST_CASE("closestPair matches brute force on random double points") {
    Rng rng{99};
    for (int trial = 0; trial < 20; ++trial) {
        const int count = rng.range(2, 80);
        std::vector<pgl::Point<double>> points;
        for (int i = 0; i < count; ++i) {
            points.emplace_back(rng.range(-1000, 1000) / 8.0, rng.range(-1000, 1000) / 8.0);
        }
        checkAgainstBruteForce(points);
    }
}

TEST_CASE("closestPair matches brute force on random rational points") {
    using RationalPoint = pgl::Point<pgl::Rational<>>;
    Rng rng{7};
    for (int trial = 0; trial < 10; ++trial) {
        const int count = rng.range(2, 40);
        std::vector<RationalPoint> points;
        for (int i = 0; i < count; ++i) {
            points.emplace_back(pgl::Rational<>(rng.range(-40, 40), 3),
                                pgl::Rational<>(rng.range(-40, 40), 7));
        }
        checkAgainstBruteForce(points);
    }
}

TEST_CASE("the brute-force cutoff never changes the answer") {
    // The cutoff is a speed knob: any value from the minimum 3 up to one that
    // swallows the whole input has to give the same distance as every other.
    Rng rng{31337};
    for (int trial = 0; trial < 15; ++trial) {
        const int count = rng.range(2, 150);
        std::vector<IntPoint> points;
        for (int i = 0; i < count; ++i) {
            points.emplace_back(rng.range(-60, 60), rng.range(-60, 60));
        }
        const auto expected = bruteForceClosestPair(points).second;
        checkEveryThreshold<3, 4, 7, 24, 64, 4096>(points, expected);
    }
}

TEST_CASE("the recursion leaves its range in abscissa order") {
    // The strip is found by walking outwards from the split index, which is only
    // valid while the range is still sorted by abscissa. So no level may reorder
    // it — all the reordering belongs in the scratch buffer. The merging
    // formulation of this algorithm leaves each range sorted by ordinate
    // instead, and adopting any part of it here would silently break the walk.
    Rng rng{4242};
    std::vector<IntPoint> points;
    for (int i = 0; i < 400; ++i) {
        points.emplace_back(rng.range(-40, 40), rng.range(-40, 40));
    }
    std::sort(points.begin(), points.end());
    const std::vector<IntPoint> before = points;

    std::vector<IntPoint> scratch(points.size());
    pgl::detail::ClosestPairCandidate<IntPoint> best{
        points[0], points[1], points[0].squaredDistance<std::int64_t>(points[1])};
    pgl::detail::closestPairRecursive<6>(points.data(), points.size(), scratch.data(), best);

    CHECK(points == before);
    CHECK(std::is_sorted(points.begin(), points.end()));
    // And it did find the answer while leaving the range alone.
    CHECK(best.squaredDistance == bruteForceClosestPair(before).second);
}

TEST_CASE("closestPair handles arbitrary-precision integer coordinates") {
    using BigPoint = pgl::Point<pgl::BigInt>;
    const pgl::BigInt base = pgl::BigInt(1000000000000000000LL) * pgl::BigInt(1000);
    std::vector<BigPoint> points;
    for (int i = 0; i < 40; ++i) {
        points.emplace_back(base * pgl::BigInt(i + 1), base * pgl::BigInt(3 * i + 1));
    }
    points.emplace_back(base * pgl::BigInt(40) + pgl::BigInt(2), base * pgl::BigInt(118));

    const auto found = pgl::closestPair(points);
    CHECK(found.min().squaredDistance(found.max()) == pgl::BigInt(4));
}

TEST_CASE("closestPair keeps the labels of the points it returns") {
    using LabelledPoint = pgl::Point<int, std::string>;
    const std::vector<LabelledPoint> points{{0, 0, "origin"},
                                            {60, 0, "east"},
                                            {61, 2, "east neighbor"},
                                            {0, 40, "north"}};
    const auto found = pgl::closestPair(points);

    CHECK(found.min().label() == "east");
    CHECK(found.max().label() == "east neighbor");
}

TEST_CASE("closestPair accepts a large exact-coordinate input") {
    // Coordinates well past the range where squaring an int32 difference fits.
    using BigPoint = pgl::Point<pgl::ERational>;
    const pgl::ERational base(pgl::BigInt(1000000000000000000LL) * pgl::BigInt(1000));
    std::vector<BigPoint> points;
    for (int i = 0; i < 20; ++i) {
        points.emplace_back(base * pgl::ERational(i + 1), base * pgl::ERational(2 * i + 1));
    }
    points.emplace_back(base * pgl::ERational(20), base * pgl::ERational(39) + pgl::ERational(1));

    const auto found = pgl::closestPair(points);
    CHECK(found.min().squaredDistance(found.max()) == pgl::ERational(1));
}
