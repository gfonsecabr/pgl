#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using Rect = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;

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

std::vector<Point> makePoints(int n, std::uint64_t seed) {
    Rng rng{seed};
    std::vector<Point> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v.emplace_back(rng.range(0, 1000), rng.range(0, 1000));
    }
    return v;
}

std::vector<Triangle> makeTriangles(int n, std::uint64_t seed) {
    Rng rng{seed};
    std::vector<Triangle> v;
    v.reserve(static_cast<std::size_t>(n));
    while (static_cast<int>(v.size()) < n) {
        const int x = rng.range(0, 1000);
        const int y = rng.range(0, 1000);
        const Point a(x + rng.range(-40, 40), y + rng.range(-40, 40));
        const Point b(x + rng.range(-40, 40), y + rng.range(-40, 40));
        const Point c(x + rng.range(-40, 40), y + rng.range(-40, 40));
        if (pgl::orientationSign(a, b, c) == 0) {
            continue;  // skip degenerate triangles (UB in pgl)
        }
        v.emplace_back(a, b, c);
    }
    return v;
}

// Weight = twice the triangle area (always positive, exact integer).
struct TwiceArea {
    int64_t operator()(const Triangle& t) const {
        const auto a = t.twiceArea();
        return a < 0 ? -static_cast<int64_t>(a) : static_cast<int64_t>(a);
    }
};

template <class S, class Q>
std::size_t bruteCountIntersecting(const std::vector<S>& v, const Q& q) {
    return static_cast<std::size_t>(std::count_if(v.begin(), v.end(),
        [&](const S& s) { return s.intersects(q); }));
}

template <class S, class Q>
std::size_t bruteCountContained(const std::vector<S>& v, const Q& q) {
    return static_cast<std::size_t>(std::count_if(v.begin(), v.end(),
        [&](const S& s) { return q.contains(s); }));
}

// The smallest squared distance from any shape in v to the query q.
template <class ResultNumber, class S, class Q>
ResultNumber bruteNearestDistance(const std::vector<S>& v, const Q& q) {
    ResultNumber best = q.template squaredDistance<ResultNumber>(v[0]);
    for (std::size_t i = 1; i < v.size(); ++i) {
        const ResultNumber d = q.template squaredDistance<ResultNumber>(v[i]);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

// Point-Point distanceL1/distanceLInf take no ResultNumber template of their
// own (integer L1/LInf is always exact), unlike every other shape pair, so a
// generic caller must probe for the templated overload before falling back.
template <class ResultNumber, class Q, class Other>
ResultNumber testDistanceL1(const Q& q, const Other& other) {
    if constexpr (requires { q.template distanceL1<ResultNumber>(other); }) {
        return q.template distanceL1<ResultNumber>(other);
    } else {
        return static_cast<ResultNumber>(q.distanceL1(other));
    }
}

template <class ResultNumber, class Q, class Other>
ResultNumber testDistanceLInf(const Q& q, const Other& other) {
    if constexpr (requires { q.template distanceLInf<ResultNumber>(other); }) {
        return q.template distanceLInf<ResultNumber>(other);
    } else {
        return static_cast<ResultNumber>(q.distanceLInf(other));
    }
}

// Same as bruteNearestDistance, but under the L1 (Manhattan) metric.
template <class ResultNumber, class S, class Q>
ResultNumber bruteNearestDistanceL1(const std::vector<S>& v, const Q& q) {
    ResultNumber best = testDistanceL1<ResultNumber>(q, v[0]);
    for (std::size_t i = 1; i < v.size(); ++i) {
        const ResultNumber d = testDistanceL1<ResultNumber>(q, v[i]);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

// Same as bruteNearestDistance, but under the LInf (Chebyshev) metric.
template <class ResultNumber, class S, class Q>
ResultNumber bruteNearestDistanceLInf(const std::vector<S>& v, const Q& q) {
    ResultNumber best = testDistanceLInf<ResultNumber>(q, v[0]);
    for (std::size_t i = 1; i < v.size(); ++i) {
        const ResultNumber d = testDistanceLInf<ResultNumber>(q, v[i]);
        if (d < best) {
            best = d;
        }
    }
    return best;
}

// A spread of query rectangles: tiny, medium, large, all-covering, far-away.
std::vector<Rect> queryWindows() {
    std::vector<Rect> w;
    Rng rng{0xc0ffee};
    for (int i = 0; i < 60; ++i) {
        const int x = rng.range(-100, 1000);
        const int y = rng.range(-100, 1000);
        const int s = rng.range(5, 500);
        w.emplace_back(x, y, x + s, y + s);
    }
    w.emplace_back(-100, -100, 1200, 1200);  // covers everything
    w.emplace_back(5000, 5000, 6000, 6000);  // disjoint from everything
    return w;
}

}  // namespace

TEST_CASE("ShapeTree empty tree") {
    pgl::ShapeTree<Point> tree{std::vector<Point>{}};
    CHECK(tree.empty());
    CHECK(tree.size() == 0);
    CHECK(tree.countIntersecting(Rect(0, 0, 10, 10)) == 0);
    CHECK(tree.reportIntersecting(Rect(0, 0, 10, 10)).empty());
    CHECK(tree.emptyIntersecting(Rect(0, 0, 10, 10)));
    CHECK(tree.countContainedIn(Rect(0, 0, 10, 10)) == 0);
    CHECK(tree.emptyContainedIn(Rect(0, 0, 10, 10)));
}

TEST_CASE("ShapeTree single element") {
    const Triangle t({0, 0}, {10, 0}, {0, 10});
    pgl::ShapeTree<Triangle> tree{std::vector<Triangle>{t}};
    CHECK(tree.size() == 1);
    CHECK_FALSE(tree.empty());

    const Rect covering(-1, -1, 11, 11);
    CHECK(tree.countIntersecting(covering) == 1);
    CHECK(tree.countContainedIn(covering) == 1);
    CHECK_FALSE(tree.emptyIntersecting(covering));
    CHECK_FALSE(tree.emptyContainedIn(covering));

    const Rect crossing(5, -1, 11, 11);  // overlaps but does not contain
    CHECK(tree.countIntersecting(crossing) == 1);
    CHECK(tree.countContainedIn(crossing) == 0);
    CHECK(tree.emptyContainedIn(crossing));

    const Rect farShape(100, 100, 110, 110);
    CHECK(tree.countIntersecting(farShape) == 0);
    CHECK(tree.emptyIntersecting(farShape));
}

TEST_CASE("ShapeTree intersects family matches brute force") {
    const std::vector<Triangle> tris = makeTriangles(250, 7);
    const std::vector<Rect> windows = queryWindows();

    for (const std::size_t leafSize : {std::size_t{1}, std::size_t{3}, std::size_t{8}, std::size_t{20}}) {
        const pgl::ShapeTree<Triangle> tree(tris, leafSize);
        CHECK(tree.size() == tris.size());

        for (const Rect& q : windows) {
            const std::size_t expected = bruteCountIntersecting(tris, q);

            CHECK(tree.countIntersecting(q) == expected);
            CHECK(tree.emptyIntersecting(q) == (expected == 0));

            const auto report = tree.reportIntersecting(q);
            CHECK(report.size() == expected);

            std::size_t visited = 0;
            tree.visitIntersecting(q, [&](const Triangle&) { ++visited; });
            CHECK(visited == expected);

            // report must equal the brute-force matches as a multiset
            std::vector<Triangle> brute;
            for (const Triangle& t : tris) {
                if (t.intersects(q)) {
                    brute.push_back(t);
                }
            }
            std::vector<Triangle> sortedReport = report;
            std::sort(brute.begin(), brute.end());
            std::sort(sortedReport.begin(), sortedReport.end());
            CHECK(sortedReport == brute);
        }
    }
}

TEST_CASE("ShapeTree containedIn family matches brute force") {
    const std::vector<Triangle> tris = makeTriangles(250, 11);
    const std::vector<Rect> windows = queryWindows();
    const pgl::ShapeTree<Triangle> tree(tris, 4);

    for (const Rect& q : windows) {
        const std::size_t expected = bruteCountContained(tris, q);

        CHECK(tree.countContainedIn(q) == expected);
        CHECK(tree.emptyContainedIn(q) == (expected == 0));
        CHECK(tree.reportContainedIn(q).size() == expected);

        std::size_t visited = 0;
        tree.visitContainedIn(q, [&](const Triangle&) { ++visited; });
        CHECK(visited == expected);

        // Containment implies intersection, never the other way for these.
        CHECK(tree.countContainedIn(q) <= tree.countIntersecting(q));
    }
}

TEST_CASE("ShapeTree visit stops when the visitor returns true") {
    const std::vector<Triangle> tris = makeTriangles(250, 13);
    const std::vector<Rect> windows = queryWindows();
    const pgl::ShapeTree<Triangle> tree(tris, 4);

    for (const Rect& q : windows) {
        const std::size_t total = tree.countIntersecting(q);

        // Stopping after the first match visits exactly one shape and reports
        // the early stop; a void visitor never stops and visits them all.
        std::size_t visited = 0;
        const bool stopped = tree.visitIntersecting(q, [&](const Triangle&) {
            ++visited;
            return true;
        });
        CHECK(stopped == (total > 0));
        CHECK(visited == (total > 0 ? std::size_t{1} : std::size_t{0}));

        // A bool visitor that never asks to stop behaves like a void visitor.
        visited = 0;
        const bool stopped2 = tree.visitIntersecting(q, [&](const Triangle&) {
            ++visited;
            return false;
        });
        CHECK_FALSE(stopped2);
        CHECK(visited == total);

        // Same contract for containedIn: stop after a fixed number of matches.
        const std::size_t contained = tree.countContainedIn(q);
        const std::size_t limit = 2;
        visited = 0;
        const bool stopped3 = tree.visitContainedIn(q, [&](const Triangle&) {
            return ++visited >= limit;
        });
        CHECK(stopped3 == (contained >= limit));
        CHECK(visited == std::min(contained, limit));
    }
}

TEST_CASE("ShapeTree weighted sums match brute force") {
    const std::vector<Triangle> tris = makeTriangles(200, 5);
    const std::vector<Rect> windows = queryWindows();
    const TwiceArea weight;
    const pgl::ShapeTree<Triangle, TwiceArea> tree(tris, 3, weight);

    for (const Rect& q : windows) {
        int64_t expectedIntersect = 0;
        int64_t expectedContained = 0;
        for (const Triangle& t : tris) {
            if (t.intersects(q)) {
                expectedIntersect += weight(t);
            }
            if (q.contains(t)) {
                expectedContained += weight(t);
            }
        }
        CHECK(tree.sumIntersecting(q) == expectedIntersect);
        CHECK(tree.sumContainedIn(q) == expectedContained);
    }

    // The single-argument weight constructor uses the default leaf size.
    const pgl::ShapeTree<Triangle, TwiceArea> tree2(tris, weight);
    const Rect all(-100, -100, 1200, 1200);
    CHECK(tree2.sumIntersecting(all) == tree.sumIntersecting(all));
}

TEST_CASE("ShapeTree on points") {
    const std::vector<Point> points = makePoints(300, 3);
    const std::vector<Rect> windows = queryWindows();
    const pgl::ShapeTree<Point> tree(points, 5);

    for (const Rect& q : windows) {
        const std::size_t expected = bruteCountIntersecting(points, q);
        CHECK(tree.countIntersecting(q) == expected);
        // For points, "intersects a rectangle" and "contained in it" coincide.
        CHECK(tree.countContainedIn(q) == expected);
    }
}

TEST_CASE("ShapeTree deduces the stored shape type from the container") {
    const std::vector<Triangle> tris = makeTriangles(20, 7);

    // No template arguments: S is deduced as the container's value type.
    pgl::ShapeTree deduced(tris);
    static_assert(std::is_same_v<decltype(deduced)::ShapeType, Triangle>);

    // The leaf-size overload still deduces the default (empty) weight.
    pgl::ShapeTree withLeaf(tris, 3);
    static_assert(std::is_same_v<decltype(withLeaf)::ShapeType, Triangle>);

    CHECK(deduced.size() == tris.size());
    CHECK(withLeaf.size() == tris.size());
}

TEST_CASE("Deduced ShapeTree preserves segment labels") {
    using LabeledSegment = pgl::Segment<Point, std::string>;
    std::vector<LabeledSegment> segs;
    segs.emplace_back(Point(0, 0), Point(10, 0), "a");
    segs.emplace_back(Point(0, 5), Point(10, 5), "b");

    // CTAD must carry the label type through, not flatten to Segment<Point>.
    pgl::ShapeTree tree(segs);
    static_assert(std::is_same_v<decltype(tree)::ShapeType, LabeledSegment>);

    const Rect window(-1, -1, 11, 11);  // contains both segments
    auto found = tree.reportIntersecting(window);
    REQUIRE(found.size() == 2);
    std::vector<std::string> labels{found[0].label(), found[1].label()};
    std::sort(labels.begin(), labels.end());
    CHECK(labels == std::vector<std::string>{"a", "b"});
}

TEST_CASE("ShapeTree empty tree returns a default-constructed nearest neighbor") {
    pgl::ShapeTree<Point> tree{std::vector<Point>{}};
    CHECK(tree.nearestNeighbor(Point(0, 0)) == Point{});
}

TEST_CASE("ShapeTree nearestNeighbor on points matches brute force") {
    const std::vector<Point> points = makePoints(300, 17);

    // A spread of query points: inside, around, and far from the data.
    Rng rng{0xabcd};
    std::vector<Point> queries;
    for (int i = 0; i < 80; ++i) {
        queries.emplace_back(rng.range(-200, 1200), rng.range(-200, 1200));
    }

    for (const std::size_t leafSize : {std::size_t{1}, std::size_t{4}, std::size_t{16}}) {
        const pgl::ShapeTree<Point> tree(points, leafSize);
        for (const Point& q : queries) {
            const Point found = tree.nearestNeighbor(q);
            const int64_t expected = bruteNearestDistance<int64_t>(points, q);
            CHECK(q.squaredDistance<int64_t>(found) == expected);
        }
    }
}

TEST_CASE("ShapeTree nearestNeighbor on triangles matches brute force") {
    using Rational = pgl::Rational<int64_t>;
    const std::vector<Triangle> tris = makeTriangles(200, 19);
    const pgl::ShapeTree<Triangle> tree(tris, 4);

    Rng rng{0x1234};
    for (int i = 0; i < 60; ++i) {
        const Point q(rng.range(-200, 1200), rng.range(-200, 1200));
        const Triangle found = tree.nearestNeighbor<Rational>(q);
        const Rational expected = bruteNearestDistance<Rational>(tris, q);
        CHECK(q.squaredDistance<Rational>(found) == expected);
    }
}

TEST_CASE("ShapeTree empty tree returns a default-constructed L1/LInf nearest neighbor") {
    pgl::ShapeTree<Point> tree{std::vector<Point>{}};
    CHECK(tree.nearestNeighborL1(Point(0, 0)) == Point{});
    CHECK(tree.nearestNeighborLInf(Point(0, 0)) == Point{});
}

TEST_CASE("ShapeTree nearestNeighborL1 on points matches brute force") {
    const std::vector<Point> points = makePoints(300, 17);

    Rng rng{0xabcd};
    std::vector<Point> queries;
    for (int i = 0; i < 80; ++i) {
        queries.emplace_back(rng.range(-200, 1200), rng.range(-200, 1200));
    }

    for (const std::size_t leafSize : {std::size_t{1}, std::size_t{4}, std::size_t{16}}) {
        const pgl::ShapeTree<Point> tree(points, leafSize);
        for (const Point& q : queries) {
            const Point found = tree.nearestNeighborL1(q);
            const int64_t expected = bruteNearestDistanceL1<int64_t>(points, q);
            CHECK(testDistanceL1<int64_t>(q, found) == expected);
        }
    }
}

TEST_CASE("ShapeTree nearestNeighborLInf on points matches brute force") {
    const std::vector<Point> points = makePoints(300, 17);

    Rng rng{0xabcd};
    std::vector<Point> queries;
    for (int i = 0; i < 80; ++i) {
        queries.emplace_back(rng.range(-200, 1200), rng.range(-200, 1200));
    }

    for (const std::size_t leafSize : {std::size_t{1}, std::size_t{4}, std::size_t{16}}) {
        const pgl::ShapeTree<Point> tree(points, leafSize);
        for (const Point& q : queries) {
            const Point found = tree.nearestNeighborLInf(q);
            const int64_t expected = bruteNearestDistanceLInf<int64_t>(points, q);
            CHECK(testDistanceLInf<int64_t>(q, found) == expected);
        }
    }
}

TEST_CASE("ShapeTree nearestNeighborL1 on triangles matches brute force") {
    using Rational = pgl::Rational<int64_t>;
    const std::vector<Triangle> tris = makeTriangles(200, 19);
    const pgl::ShapeTree<Triangle> tree(tris, 4);

    Rng rng{0x1234};
    for (int i = 0; i < 60; ++i) {
        const Point q(rng.range(-200, 1200), rng.range(-200, 1200));
        const Triangle found = tree.nearestNeighborL1<Rational>(q);
        const Rational expected = bruteNearestDistanceL1<Rational>(tris, q);
        CHECK(q.distanceL1<Rational>(found) == expected);
    }
}

TEST_CASE("ShapeTree nearestNeighborLInf on triangles matches brute force") {
    using Rational = pgl::Rational<int64_t>;
    const std::vector<Triangle> tris = makeTriangles(200, 19);
    const pgl::ShapeTree<Triangle> tree(tris, 4);

    Rng rng{0x1234};
    for (int i = 0; i < 60; ++i) {
        const Point q(rng.range(-200, 1200), rng.range(-200, 1200));
        const Triangle found = tree.nearestNeighborLInf<Rational>(q);
        const Rational expected = bruteNearestDistanceLInf<Rational>(tris, q);
        CHECK(q.distanceLInf<Rational>(found) == expected);
    }
}

TEST_CASE("ShapeTree contains reports exact membership") {
    const std::vector<Triangle> tris = makeTriangles(200, 23);
    const pgl::ShapeTree<Triangle> tree(tris, 4);

    // Every stored shape is found.
    for (const Triangle& t : tris) {
        CHECK(tree.has(t));
    }

    // A shape placed far outside the data range is absent. This is membership,
    // not geometric containment: an equal shape must be stored.
    const Triangle absent({5000, 5000}, {5040, 5000}, {5000, 5040});
    CHECK_FALSE(tree.has(absent));

    // An empty tree contains nothing.
    pgl::ShapeTree<Triangle> emptyTree{std::vector<Triangle>{}};
    CHECK_FALSE(emptyTree.has(tris.front()));
}

TEST_CASE("ShapeTree erase removes shapes and keeps queries correct") {
    const std::vector<Triangle> tris = makeTriangles(300, 29);
    const std::vector<Rect> windows = queryWindows();

    for (const std::size_t leafSize : {std::size_t{1}, std::size_t{4}, std::size_t{12}}) {
        pgl::ShapeTree<Triangle> tree(tris, leafSize);
        std::vector<Triangle> ref = tris;

        // Erasing a shape that is not stored leaves the tree unchanged.
        const Triangle absent({5000, 5000}, {5040, 5000}, {5000, 5040});
        CHECK_FALSE(tree.erase(absent));
        CHECK(tree.size() == ref.size());

        // Erase shapes one at a time in a deterministic pseudo-random order,
        // checking the tree against brute force throughout.
        Rng rng{0xdada + leafSize};
        while (!ref.empty()) {
            const std::size_t k = static_cast<std::size_t>(
                rng.range(0, static_cast<int>(ref.size()) - 1));
            const Triangle victim = ref[k];

            CHECK(tree.erase(victim));
            ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(k));

            CHECK(tree.size() == ref.size());

            // Re-validate the spatial queries every so often (every shape would
            // be slow); always check on the last few removals.
            if (ref.size() % 50 == 0 || ref.size() < 4) {
                for (const Rect& q : windows) {
                    CHECK(tree.countIntersecting(q) == bruteCountIntersecting(ref, q));
                    CHECK(tree.countContainedIn(q) == bruteCountContained(ref, q));
                }
                for (const Triangle& t : ref) {
                    CHECK(tree.has(t));
                }
            }
        }

        CHECK(tree.empty());
        CHECK(tree.boundingBoxes().empty());
        CHECK(tree.countIntersecting(windows.front()) == 0);
    }
}

TEST_CASE("ShapeTree erase maintains weighted sums") {
    const std::vector<Triangle> tris = makeTriangles(200, 31);
    const std::vector<Rect> windows = queryWindows();
    const TwiceArea weight;
    pgl::ShapeTree<Triangle, TwiceArea> tree(tris, 4, weight);
    std::vector<Triangle> ref = tris;

    Rng rng{0xbeef};
    for (int step = 0; step < 150; ++step) {
        const std::size_t k = static_cast<std::size_t>(
            rng.range(0, static_cast<int>(ref.size()) - 1));
        CHECK(tree.erase(ref[k]));
        ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(k));

        if (step % 25 == 0) {
            for (const Rect& q : windows) {
                int64_t expectedIntersect = 0;
                int64_t expectedContained = 0;
                for (const Triangle& t : ref) {
                    if (t.intersects(q)) {
                        expectedIntersect += weight(t);
                    }
                    if (q.contains(t)) {
                        expectedContained += weight(t);
                    }
                }
                CHECK(tree.sumIntersecting(q) == expectedIntersect);
                CHECK(tree.sumContainedIn(q) == expectedContained);
            }
        }
    }
}

TEST_CASE("ShapeTree erase keeps the node array compact under interleaved insert/erase") {
    std::vector<Triangle> ref = makeTriangles(120, 37);
    pgl::ShapeTree<Triangle> tree(ref, 4);
    const std::vector<Rect> windows = queryWindows();

    // A generator of fresh triangles, distinct from the rebuild seed above.
    Rng gen{0x5151};
    auto freshTriangle = [&]() {
        for (;;) {
            const int x = gen.range(0, 1000);
            const int y = gen.range(0, 1000);
            const Point a(x + gen.range(-40, 40), y + gen.range(-40, 40));
            const Point b(x + gen.range(-40, 40), y + gen.range(-40, 40));
            const Point c(x + gen.range(-40, 40), y + gen.range(-40, 40));
            if (pgl::orientationSign(a, b, c) != 0) {
                return Triangle(a, b, c);
            }
        }
    };

    // The number of nodes equals the number of bounding boxes reported.
    std::size_t maxNodes = tree.boundingBoxes().size();

    Rng rng{0x2727};
    for (int iter = 0; iter < 4000; ++iter) {
        const std::size_t k = static_cast<std::size_t>(
            rng.range(0, static_cast<int>(ref.size()) - 1));
        CHECK(tree.erase(ref[k]));
        ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(k));

        const Triangle s = freshTriangle();
        tree.insert(s);
        ref.push_back(s);

        maxNodes = std::max(maxNodes, tree.boundingBoxes().size());
    }

    CHECK(tree.size() == ref.size());

    // The size held roughly constant, so the node count must stay bounded rather
    // than grow with the number of operations: detached slots are reclaimed.
    CHECK(maxNodes < 1000);

    // The tree is still correct after all the churn.
    for (const Rect& q : windows) {
        CHECK(tree.countIntersecting(q) == bruteCountIntersecting(ref, q));
    }
    for (const Triangle& t : ref) {
        CHECK(tree.has(t));
    }
}

TEST_CASE("ShapeTree rebuild after erases restores a queryable tree") {
    std::vector<Triangle> ref = makeTriangles(200, 41);
    pgl::ShapeTree<Triangle> tree(ref, 4);
    const std::vector<Rect> windows = queryWindows();

    // Remove a third of the shapes, degrading the structure.
    Rng rng{0x9090};
    for (int i = 0; i < 66; ++i) {
        const std::size_t k = static_cast<std::size_t>(
            rng.range(0, static_cast<int>(ref.size()) - 1));
        CHECK(tree.erase(ref[k]));
        ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(k));
    }

    tree.rebuild();
    CHECK(tree.size() == ref.size());

    // The rebuilt root box encloses every remaining shape.
    const auto boxes = tree.boundingBoxes();
    REQUIRE_FALSE(boxes.empty());
    for (const Triangle& t : ref) {
        CHECK(boxes.front().contains(t.bbox()));
    }

    for (const Rect& q : windows) {
        CHECK(tree.countIntersecting(q) == bruteCountIntersecting(ref, q));
        CHECK(tree.countContainedIn(q) == bruteCountContained(ref, q));
    }
}

TEST_CASE("ShapeTree boundingBoxes is empty exactly when the tree is empty") {
    pgl::ShapeTree<Triangle> emptyTree{std::vector<Triangle>{}};
    CHECK(emptyTree.boundingBoxes().empty());

    const std::vector<Triangle> tris = makeTriangles(30, 43);
    const pgl::ShapeTree<Triangle> tree(tris, 3);
    const auto boxes = tree.boundingBoxes();
    CHECK_FALSE(boxes.empty());
    // The first box is the root's: it encloses every shape.
    for (const Triangle& t : tris) {
        CHECK(boxes.front().contains(t.bbox()));
    }
}

TEST_CASE("ShapeTree draws node boxes to a canvas") {
    const std::vector<Triangle> tris = makeTriangles(40, 2);
    const pgl::ShapeTree<Triangle> tree(tris, 3);

    pgl::Canvas canvas;
    canvas << tree;
    const std::string svg = canvas.toSVG();
    // At least one node box should have been emitted as an SVG rectangle.
    CHECK(svg.find("<rect") != std::string::npos);
}

TEST_CASE("ShapeTree insert leaves no trace when bbox() throws") {
    using Shape = pgl::Shape<Point>;
    pgl::ShapeTree<Shape> tree{std::vector<Shape>{}};

    const Shape unbounded = pgl::Line<Point>(Point(0, 0), Point(1, 1));
    CHECK_THROWS_AS(tree.insert(unbounded), std::logic_error);

    // A failed insert must not leave a phantom element in storage.
    CHECK(tree.size() == 0);
    CHECK(tree.shapes().empty());
    CHECK(tree.begin() == tree.end());

    // The tree must remain fully usable afterwards.
    const Shape p = Point(2, 3);
    tree.insert(p);
    CHECK(tree.size() == 1);
    CHECK(tree.shapes().front() == p);
}
