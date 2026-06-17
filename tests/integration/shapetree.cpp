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
    long operator()(const Triangle& t) const {
        const auto a = t.twiceArea();
        return a < 0 ? -static_cast<long>(a) : static_cast<long>(a);
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

    const Rect far(100, 100, 110, 110);
    CHECK(tree.countIntersecting(far) == 0);
    CHECK(tree.emptyIntersecting(far));
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

TEST_CASE("ShapeTree weighted sums match brute force") {
    const std::vector<Triangle> tris = makeTriangles(200, 5);
    const std::vector<Rect> windows = queryWindows();
    const TwiceArea weight;
    const pgl::ShapeTree<Triangle, TwiceArea> tree(tris, 3, weight);

    for (const Rect& q : windows) {
        long expectedIntersect = 0;
        long expectedContained = 0;
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

TEST_CASE("ShapeTree draws node boxes to a canvas") {
    const std::vector<Triangle> tris = makeTriangles(40, 2);
    const pgl::ShapeTree<Triangle> tree(tris, 3);

    pgl::Canvas canvas;
    canvas << tree;
    const std::string svg = canvas.toSVG();
    // At least one node box should have been emitted as an SVG rectangle.
    CHECK(svg.find("<rect") != std::string::npos);
}
