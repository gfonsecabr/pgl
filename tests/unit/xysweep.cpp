#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <set>
#include <vector>

#include "pgl.hpp"

// The sweep only takes over above pgl::detail::xySweepMinSegments edges, so the
// polygons and polylines exercising it here are deliberately larger than that.
static constexpr std::size_t sweepSize = pgl::detail::xySweepMinSegments + 1;

template <class Point>
static std::vector<pgl::Segment<Point>> randomSegments(std::size_t count, unsigned seed) {
    std::mt19937 rgen(seed);
    std::uniform_int_distribution<int> base(-12, 12);
    std::uniform_int_distribution<int> len(-6, 6);

    std::vector<pgl::Segment<Point>> ret;
    while (ret.size() < count) {
        Point p(base(rgen), base(rgen));
        Point q(p.x() + len(rgen), p.y() + len(rgen));
        if (p != q) {
            ret.emplace_back(p, q);
        }
    }
    return ret;
}

// A closed curve sampled densely enough that no two non-consecutive edges meet.
template <class Point>
static std::vector<Point> flowerVertices(std::size_t count) {
    std::vector<Point> ret;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(count);
        const double r = 100.0 + 30.0 * std::sin(5.0 * t) + 10.0 * std::cos(11.0 * t);
        ret.emplace_back(r * std::cos(t), r * std::sin(t));
    }
    return ret;
}

// The quadratic definition of simplicity, as an oracle independent of the path
// under test: edges meet only where consecutive edges share an endpoint.
template <class Point>
static bool simpleByDefinition(const std::vector<pgl::Segment<Point>>& edges, bool ring) {
    const std::size_t m = edges.size();
    for (std::size_t i = 0; i < m; ++i) {
        if (edges[i].isDegenerate()) {
            return false;  // a repeated vertex is never simple
        }
        for (std::size_t j = i + 1; j < m; ++j) {
            const bool adjacent = (j == i + 1) || (ring && i == 0 && j == m - 1);
            if (adjacent) {
                if (edges[i].interiorsIntersect(edges[j])) {
                    return false;
                }
            } else if (edges[i].intersects(edges[j])) {
                return false;
            }
        }
    }
    return true;
}

template <class Shape>
static std::vector<pgl::Segment<typename Shape::PointType>> edgesOf(const Shape& shape, bool ring) {
    using Point = typename Shape::PointType;
    std::vector<pgl::Segment<Point>> ret;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(shape.size());
    const std::ptrdiff_t last = ring ? n : n - 1;
    for (std::ptrdiff_t i = 0; i < last; ++i) {
        ret.emplace_back(shape.get(i), shape.get(i + 1));
    }
    return ret;
}

template <class T>
static void sortPairs(T& pairs) {
    std::sort(pairs.begin(), pairs.end());
}

// ---------------------------------------------------------------- xy sweeps

TEST_CASE_TEMPLATE("xyCrossings agrees with bruteForceCrossings", Point,
                   pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<int>>) {
    for (unsigned seed = 1; seed <= 20; ++seed) {
        auto segs = randomSegments<Point>(40, seed);
        auto swept = pgl::xyCrossings(segs);
        auto brute = pgl::bruteForceCrossings(segs);
        sortPairs(swept);
        sortPairs(brute);
        CHECK(swept == brute);
    }
}

TEST_CASE_TEMPLATE("xyIntersections agrees with bruteForceIntersections", Point,
                   pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<int>>) {
    for (unsigned seed = 1; seed <= 20; ++seed) {
        auto segs = randomSegments<Point>(40, seed);
        auto swept = pgl::xyIntersections(segs);
        auto brute = pgl::bruteForceIntersections(segs);
        sortPairs(swept);
        sortPairs(brute);
        CHECK(swept == brute);
    }
}

TEST_CASE("xy sweeps handle degenerate configurations") {
    using Point = pgl::Point<int>;
    using SegmentType = pgl::Segment<Point>;

    SUBCASE("empty and single-segment input") {
        std::vector<SegmentType> none;
        CHECK(pgl::xyCrossings(none).empty());
        CHECK(pgl::xyIntersections(none).empty());

        std::vector<SegmentType> one{{0, 0, 4, 4}};
        CHECK(pgl::xyCrossings(one).empty());
        CHECK(pgl::xyIntersections(one).empty());
    }

    SUBCASE("boxes touching in a single abscissa") {
        // Two segments meeting end to end: the boxes share only the line x = 4,
        // which the sweep must still compare.
        std::vector<SegmentType> segs{{0, 0, 4, 4}, {4, 4, 8, 0}};
        CHECK(pgl::xyCrossings(segs).empty());
        CHECK(pgl::xyIntersections(segs).size() == 1);
    }

    SUBCASE("vertical segments") {
        // A degenerate (zero-width) box has its opening and closing event at the
        // same abscissa; the vertical pair still has to be reported.
        std::vector<SegmentType> segs{{2, 0, 2, 8}, {2, 4, 2, 12}, {0, 6, 4, 6}};
        auto swept = pgl::xyIntersections(segs);
        auto brute = pgl::bruteForceIntersections(segs);
        sortPairs(swept);
        sortPairs(brute);
        CHECK(swept == brute);
        CHECK(swept.size() == 3);
    }

    SUBCASE("collinear overlap and duplicated segments") {
        std::vector<SegmentType> segs{{0, 0, 6, 0}, {2, 0, 8, 0}, {0, 0, 6, 0}};
        auto swept = pgl::xyIntersections(segs);
        auto brute = pgl::bruteForceIntersections(segs);
        sortPairs(swept);
        sortPairs(brute);
        CHECK(swept == brute);
        CHECK(swept.size() == 3);  // every pair overlaps, duplicates included
        CHECK(pgl::xyCrossings(segs).empty());  // overlapping is not crossing
    }

    SUBCASE("many segments sharing one endpoint") {
        std::vector<SegmentType> segs;
        for (int k = -5; k <= 5; ++k) {
            segs.emplace_back(0, 0, 10, k);
        }
        auto swept = pgl::xyIntersections(segs);
        auto brute = pgl::bruteForceIntersections(segs);
        sortPairs(swept);
        sortPairs(brute);
        CHECK(swept == brute);
    }
}

TEST_CASE("xy sweeps scale past the pairwise scan") {
    using Point = pgl::Point<double>;
    auto segs = randomSegments<Point>(600, 7);
    auto swept = pgl::xyIntersections(segs);
    auto brute = pgl::bruteForceIntersections(segs);
    sortPairs(swept);
    sortPairs(brute);
    CHECK(swept == brute);

    auto sweptCrossings = pgl::xyCrossings(segs);
    auto bruteCrossings = pgl::bruteForceCrossings(segs);
    sortPairs(sweptCrossings);
    sortPairs(bruteCrossings);
    CHECK(sweptCrossings == bruteCrossings);
}

// ------------------------------------------------- isSimple on the sweep path

TEST_CASE("Polygon::isSimple takes the sweep path for large floating-point rings") {
    using Point = pgl::Point<double>;
    using PolygonShape = pgl::Polygon<Point>;

    SUBCASE("a dense closed curve is simple") {
        PolygonShape poly(flowerVertices<Point>(sweepSize));
        REQUIRE(poly.size() > pgl::detail::xySweepMinSegments);
        CHECK(poly.isSimple());
        CHECK(simpleByDefinition(edgesOf(poly, true), true));
    }

    SUBCASE("one swapped vertex pair breaks it") {
        auto vertices = flowerVertices<Point>(sweepSize);
        std::swap(vertices[3], vertices[sweepSize / 2]);
        PolygonShape poly(vertices);
        CHECK(!poly.isSimple());
        CHECK(!simpleByDefinition(edgesOf(poly, true), true));
    }

    SUBCASE("a repeated vertex breaks it") {
        auto vertices = flowerVertices<Point>(sweepSize);
        vertices[7] = vertices[8];
        PolygonShape poly(vertices);
        CHECK(!poly.isSimple());
    }

    SUBCASE("a spike touching the interior of a far edge breaks it") {
        // A flat ring subdivided into 142 vertices: the bottom side runs along
        // the even abscissas, so a vertex dropped onto an odd one lands in the
        // interior of a bottom edge rather than on one of its endpoints.
        std::vector<Point> vertices;
        for (int i = 0; i <= 70; ++i) {
            vertices.emplace_back(2.0 * i, 0.0);
        }
        for (int i = 70; i >= 0; --i) {
            vertices.emplace_back(2.0 * i, 10.0);
        }
        PolygonShape flat(vertices);
        REQUIRE(flat.size() > pgl::detail::xySweepMinSegments);
        REQUIRE(flat.isSimple());  // collinear vertices along a side are fine

        vertices[100] = Point(41.0, 0.0);  // touches, without crossing, an edge
        PolygonShape touching(vertices);
        CHECK(!touching.isSimple());
        CHECK(!simpleByDefinition(edgesOf(touching, true), true));
    }

    SUBCASE("the sweep and the pairwise scan agree over random rings") {
        std::mt19937 rgen(99);
        std::uniform_real_distribution<double> radius(60.0, 140.0);
        for (int iter = 0; iter < 25; ++iter) {
            std::vector<double> angles;
            for (std::size_t i = 0; i < sweepSize; ++i) {
                angles.push_back(2.0 * std::numbers::pi * static_cast<double>(i) /
                                 static_cast<double>(sweepSize));
            }
            std::vector<Point> vertices;
            for (double t : angles) {
                const double r = radius(rgen);
                vertices.emplace_back(std::round(r * std::cos(t)), std::round(r * std::sin(t)));
            }
            PolygonShape poly(vertices);
            if (poly.size() < 3) {
                continue;
            }
            CHECK(poly.isSimple() == simpleByDefinition(edgesOf(poly, true), true));
        }
    }
}

TEST_CASE("Polyline::isSimple takes the sweep path for large floating-point chains") {
    using Point = pgl::Point<double>;
    using PolylineShape = pgl::Polyline<Point>;

    SUBCASE("an open staircase is simple") {
        std::vector<Point> vertices;
        for (std::size_t i = 0; i < sweepSize + 1; ++i) {
            vertices.emplace_back(static_cast<double>(i), static_cast<double>(i % 2));
        }
        PolylineShape chain(vertices);
        REQUIRE(chain.size() > pgl::detail::xySweepMinSegments + 1);
        CHECK(chain.isSimple());
        CHECK(simpleByDefinition(edgesOf(chain, false), false));
    }

    SUBCASE("closing the chain makes it non-simple") {
        auto vertices = flowerVertices<Point>(sweepSize);
        vertices.push_back(vertices.front());  // first and last edges are not adjacent
        PolylineShape chain(vertices);
        CHECK(!chain.isSimple());
        CHECK(!simpleByDefinition(edgesOf(chain, false), false));
    }

    SUBCASE("a revisited vertex breaks it") {
        std::vector<Point> vertices;
        for (std::size_t i = 0; i < sweepSize + 1; ++i) {
            vertices.emplace_back(static_cast<double>(i), static_cast<double>(i % 2));
        }
        vertices.back() = vertices[4];
        PolylineShape chain(vertices);
        CHECK(!chain.isSimple());
        CHECK(!simpleByDefinition(edgesOf(chain, false), false));
    }
}

// The exact coordinate types keep the exact sweep line, unaffected by the new
// path: the same rings answer the same way there.
TEST_CASE("Exact coordinates keep answering as before at sweep-path sizes") {
    using Point = pgl::Point<int>;
    using PolygonShape = pgl::Polygon<Point>;

    std::vector<Point> vertices;
    for (std::size_t i = 0; i < sweepSize; ++i) {
        const double t = 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(sweepSize);
        vertices.emplace_back(static_cast<int>(std::round(1000.0 * std::cos(t))),
                              static_cast<int>(std::round(1000.0 * std::sin(t))));
    }
    PolygonShape poly(vertices);
    CHECK(poly.isSimple());

    std::swap(vertices[2], vertices[sweepSize / 2]);
    PolygonShape tangled(vertices);
    CHECK(!tangled.isSimple());
}
