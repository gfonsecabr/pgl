#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <list>
#include <set>
#include <vector>

namespace {

// Small deterministic generator so the test data is identical across compilers
// (std::uniform_int_distribution is not portable between standard libraries).
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

template <class PointType>
std::vector<PointType> makePoints(int n, std::uint64_t seed, int lo, int hi) {
    Rng rng{seed};
    std::vector<PointType> v;
    for (int i = 0; i < n; ++i)
        v.emplace_back(rng.range(lo, hi), rng.range(lo, hi));
    return v;
}

template <class PointType>
using Key = std::array<PointType, 3>;

template <class TriangleType>
auto keyOf(const TriangleType& t) {
    return Key<typename TriangleType::PointType>{t[0], t[1], t[2]};
}

// Every triangle as a key, failing the test if one repeats.
template <class TriangleType>
auto keysOf(const std::vector<TriangleType>& triangles) {
    std::set<Key<typename TriangleType::PointType>> keys;
    for (const auto& t : triangles) {
        const bool fresh = keys.insert(keyOf(t)).second;
        CHECK(fresh);
    }
    return keys;
}

// The definition spelled out: non-degenerate, and no other point of the set in
// the closed triangle.
template <class PointType>
std::set<Key<PointType>> bruteEmptyTriangles(std::vector<PointType> points) {
    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());
    std::set<Key<PointType>> out;
    const std::size_t n = points.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j)
            for (std::size_t k = j + 1; k < n; ++k) {
                if (pgl::orientationSign(points[i], points[j], points[k]) == 0)
                    continue;
                const pgl::Triangle<PointType> t(points[i], points[j], points[k]);
                bool empty = true;
                for (std::size_t s = 0; s < n && empty; ++s)
                    if (s != i && s != j && s != k && t.contains(points[s]))
                        empty = false;
                if (empty)
                    out.insert(keyOf(t));
            }
    return out;
}

template <class PointType>
bool hasVertex(const Key<PointType>& key, const PointType& p) {
    return key[0] == p || key[1] == p || key[2] == p;
}

template <class PointType>
std::set<Key<PointType>> bruteAtVertex(std::vector<PointType> points, const PointType& p) {
    points.push_back(p);
    std::set<Key<PointType>> out;
    for (const auto& key : bruteEmptyTriangles(points))
        if (hasVertex(key, p))
            out.insert(key);
    return out;
}

template <class PointType>
std::set<Key<PointType>> bruteAtEdge(std::vector<PointType> points, const PointType& a,
                                     const PointType& b) {
    points.push_back(a);
    points.push_back(b);
    std::set<Key<PointType>> out;
    for (const auto& key : bruteEmptyTriangles(points))
        if (hasVertex(key, a) && hasVertex(key, b))
            out.insert(key);
    return out;
}

template <class PointType>
void checkAgainstBruteForce(const std::vector<PointType>& points,
                            const std::vector<PointType>& queries) {
    CHECK(keysOf(pgl::findEmptyTriangles(points)) == bruteEmptyTriangles(points));

    for (const auto& p : queries)
        CHECK(keysOf(pgl::findEmptyTriangles(points, p)) == bruteAtVertex(points, p));

    for (std::size_t i = 0; i < queries.size(); ++i)
        for (std::size_t j = i + 1; j < queries.size(); ++j) {
            const auto& a = queries[i];
            const auto& b = queries[j];
            if (a == b)
                continue;
            const auto expected = bruteAtEdge(points, a, b);
            CHECK(keysOf(pgl::findEmptyTriangles(points, pgl::Segment(a, b))) == expected);
            CHECK(keysOf(pgl::findEmptyTriangles(points, pgl::OrientedSegment(b, a))) ==
                  expected);
        }
}

}  // namespace

TEST_CASE_TEMPLATE("empty triangles agree with brute force on random points", Number, int,
                   long long, pgl::Rational<int>, pgl::ERational) {
    using PointType = pgl::Point<Number>;
    for (std::uint64_t seed = 1; seed <= 40; ++seed) {
        CAPTURE(seed);
        const auto points = makePoints<PointType>(14, seed, -1000, 1000);
        auto queries = points;
        queries.resize(6);
        queries.push_back(PointType(3, -7));  // Not a point of the set.
        checkAgainstBruteForce(points, queries);
    }
}

TEST_CASE("empty triangles agree with brute force on grids full of collinear points") {
    using PointType = pgl::Point<int>;
    for (std::uint64_t seed = 1; seed <= 150; ++seed) {
        CAPTURE(seed);
        const auto points = makePoints<PointType>(12 + static_cast<int>(seed % 8), seed, 0, 4);
        auto queries = points;
        queries.resize(5);
        queries.push_back(PointType(2, 2));
        queries.push_back(PointType(-1, 2));
        queries.push_back(PointType(5, 5));
        checkAgainstBruteForce(points, queries);
    }
}

TEST_CASE("empty triangles of a full grid") {
    using PointType = pgl::Point<int>;
    std::vector<PointType> points;
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 4; ++y)
            points.emplace_back(x, y);
    checkAgainstBruteForce(points, {PointType(0, 0), PointType(1, 1), PointType(3, 1),
                                    PointType(2, 3), PointType(1, -1)});
}

TEST_CASE("empty triangles of points in convex position are every triple") {
    using PointType = pgl::Point<int>;
    const std::vector<PointType> points{{0, 0}, {4, 0}, {6, 3}, {4, 6}, {0, 6}, {-2, 3}};
    CHECK(pgl::findEmptyTriangles(points).size() == 20);
    CHECK(pgl::findEmptyTriangles(points, points[0]).size() == 10);
    CHECK(pgl::findEmptyTriangles(points, pgl::Segment(points[0], points[3])).size() == 4);
}

TEST_CASE("empty triangles of small and degenerate inputs") {
    using PointType = pgl::Point<int>;
    const std::vector<PointType> none;
    CHECK(pgl::findEmptyTriangles(none).empty());
    CHECK(pgl::findEmptyTriangles(none, PointType(1, 1)).empty());
    CHECK(pgl::findEmptyTriangles(none, pgl::Segment(PointType(0, 0), PointType(1, 0))).empty());

    const std::vector<PointType> collinear{{0, 0}, {1, 1}, {2, 2}, {3, 3}};
    CHECK(pgl::findEmptyTriangles(collinear).empty());
    CHECK(pgl::findEmptyTriangles(collinear, PointType(1, 1)).empty());

    // Coincident points count once.
    const std::vector<PointType> repeated{{0, 0}, {0, 0}, {4, 0}, {4, 0}, {0, 4}};
    CHECK(pgl::findEmptyTriangles(repeated).size() == 1);
    CHECK(pgl::findEmptyTriangles(repeated, PointType(0, 0)).size() == 1);
    CHECK(pgl::findEmptyTriangles(repeated, pgl::Segment(PointType(0, 0), PointType(4, 0)))
              .size() == 1);

    // A zero-length edge has no triangles; a point inside the edge blocks all.
    const std::vector<PointType> square{{0, 0}, {4, 0}, {4, 4}, {0, 4}};
    CHECK(pgl::findEmptyTriangles(square, pgl::Segment(PointType(0, 0), PointType(0, 0)))
              .empty());
    CHECK(pgl::findEmptyTriangles(square, pgl::Segment(PointType(0, 0), PointType(4, 4)))
              .size() == 2);
    auto blocked = square;
    blocked.emplace_back(2, 2);
    CHECK(pgl::findEmptyTriangles(blocked, pgl::Segment(PointType(0, 0), PointType(4, 4)))
              .empty());

    // A query vertex outside the set is treated as one of its points.
    CHECK(pgl::findEmptyTriangles(square, PointType(2, 2)).size() == 4);
}

TEST_CASE("empty triangles accept any container of points") {
    using PointType = pgl::Point<int>;
    const std::list<PointType> points{{0, 0}, {4, 0}, {0, 4}, {5, 5}};
    const std::array<PointType, 4> same{{{0, 0}, {4, 0}, {0, 4}, {5, 5}}};
    CHECK(keysOf(pgl::findEmptyTriangles(points)) == keysOf(pgl::findEmptyTriangles(same)));
    CHECK(pgl::findEmptyTriangles(points).size() == 4);
}

TEST_CASE("visiting empty triangles stops on true") {
    using PointType = pgl::Point<int>;
    const auto points = makePoints<PointType>(30, 7, -100, 100);

    int visits = 0;
    CHECK(pgl::visitEmptyTriangles(points, [&visits](const auto&) { return ++visits == 3; }));
    CHECK(visits == 3);

    visits = 0;
    CHECK(pgl::visitEmptyTriangles(points, points[0],
                                   [&visits](const auto&) { return ++visits == 2; }));
    CHECK(visits == 2);

    visits = 0;
    CHECK(pgl::visitEmptyTriangles(points, pgl::Segment(points[0], points[1]),
                                   [&visits](const auto&) { return ++visits == 1; }));
    CHECK(visits == 1);

    // A void visitor sees them all and the visit reports no stop.
    std::size_t count = 0;
    CHECK_FALSE(pgl::visitEmptyTriangles(points, [&count](const auto&) { ++count; }));
    CHECK(count == pgl::findEmptyTriangles(points).size());

    // A visitor that never stops sees them all too.
    count = 0;
    CHECK_FALSE(pgl::visitEmptyTriangles(points, [&count](const auto&) {
        ++count;
        return false;
    }));
    CHECK(count == pgl::findEmptyTriangles(points).size());
}

TEST_CASE("empty triangles keep the point labels") {
    using PointType = pgl::Point<int, int>;
    const std::vector<PointType> points{{0, 0, 1}, {4, 0, 2}, {0, 4, 3}};
    const auto triangles = pgl::findEmptyTriangles(points);
    REQUIRE(triangles.size() == 1);
    int sum = 0;
    for (int i = 0; i < 3; ++i)
        sum += triangles[0][static_cast<std::size_t>(i)].label();
    CHECK(sum == 6);
}
