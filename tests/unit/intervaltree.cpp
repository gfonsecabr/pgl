#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using Rect = pgl::Rectangle<Point>;
using Segment = pgl::Segment<Point>;

namespace {

struct Rng {
    std::uint64_t state;

    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }

    int range(int low, int high) {
        return low + static_cast<int>(next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

template <pgl::ProjectionAxis Axis, class Shape>
auto interval(const Shape& shape) {
    const auto box = shape.bbox();
    if constexpr (Axis == pgl::ProjectionAxis::x) {
        return std::pair{box.min().x(), box.max().x()};
    } else {
        return std::pair{box.min().y(), box.max().y()};
    }
}

template <pgl::ProjectionAxis Axis>
std::size_t bruteIntersecting(const std::vector<Segment>& shapes, const Rect& query) {
    const auto [queryLow, queryHigh] = interval<Axis>(query);
    return static_cast<std::size_t>(std::count_if(shapes.begin(), shapes.end(),
        [&](const Segment& shape) {
            const auto [low, high] = interval<Axis>(shape);
            return !(high < queryLow) && !(queryHigh < low);
        }));
}

template <pgl::ProjectionAxis Axis>
std::size_t bruteContained(const std::vector<Segment>& shapes, const Rect& query) {
    const auto [queryLow, queryHigh] = interval<Axis>(query);
    return static_cast<std::size_t>(std::count_if(shapes.begin(), shapes.end(),
        [&](const Segment& shape) {
            const auto [low, high] = interval<Axis>(shape);
            return !(low < queryLow) && !(queryHigh < low) && !(queryHigh < high);
        }));
}

Segment randomSegment(Rng& rng) {
    return Segment(Point(rng.range(-50, 50), rng.range(-50, 50)),
                   Point(rng.range(-50, 50), rng.range(-50, 50)));
}

// A coordinate that counts its comparisons, so a test can bound how much of the
// tree an operation touches.
std::size_t comparisons = 0;

struct CountedNumber {
    int value = 0;

    friend bool operator<(const CountedNumber& a, const CountedNumber& b) {
        ++comparisons;
        return a.value < b.value;
    }
};

// A one-dimensional shape whose bounding box has the closed interval
// [low, high] as both of its projections.
struct Stick {
    int low = 0;
    int high = 0;
    int id = 0;

    struct Corner {
        CountedNumber coordinate;
        [[nodiscard]] CountedNumber x() const { return coordinate; }
        [[nodiscard]] CountedNumber y() const { return coordinate; }
    };

    struct Box {
        Corner lower;
        Corner upper;
        [[nodiscard]] Corner min() const { return lower; }
        [[nodiscard]] Corner max() const { return upper; }
    };

    [[nodiscard]] Box bbox() const {
        return Box{Corner{CountedNumber{low}}, Corner{CountedNumber{high}}};
    }

    bool operator==(const Stick&) const = default;
};

std::size_t bruteSticksIntersecting(const std::vector<Stick>& sticks, int low, int high) {
    return static_cast<std::size_t>(std::count_if(sticks.begin(), sticks.end(),
        [&](const Stick& s) { return s.low <= high && low <= s.high; }));
}

std::size_t floorLog2(std::size_t n) {
    std::size_t result = 0;
    while (n > 1) {
        n /= 2;
        ++result;
    }
    return result;
}

}  // namespace

TEST_CASE("IntervalTree indexes closed x and y projections") {
    const Segment first(Point(-10, 0), Point(2, 10));
    const Segment second(Point(2, 15), Point(7, 15));
    const Segment third(Point(20, -3), Point(20, 5));
    const std::vector<Segment> shapes{first, second, third};

    pgl::IntervalTree<Segment> empty;
    CHECK(empty.empty());
    CHECK(empty.countProjectionsIntersecting(Rect(0, 0, 1, 1)) == 0);
    CHECK(empty.reportProjectionsContainedIn(Rect(0, 0, 1, 1)).empty());
    CHECK(empty.emptyProjectionsIntersecting(Rect(0, 0, 1, 1)));

    pgl::IntervalTree xTree(shapes);
    pgl::IntervalTree<Segment, pgl::ProjectionAxis::y> yTree(shapes);
    static_assert(std::is_same_v<decltype(xTree)::ShapeType, Segment>);

    const Rect xQuery(2, -100, 10, 100);
    CHECK(xTree.countProjectionsIntersecting(xQuery) == 2);  // first touches at x = 2
    CHECK(xTree.countProjectionsContainedIn(xQuery) == 1);
    CHECK_FALSE(xTree.emptyProjectionsIntersecting(xQuery));
    CHECK_FALSE(xTree.emptyProjectionsContainedIn(xQuery));

    const Rect yQuery(0, 5, 0, 15);
    CHECK(yTree.countProjectionsIntersecting(yQuery) == 3);  // third touches at y = 5
    CHECK(yTree.countProjectionsContainedIn(yQuery) == 1);

    const auto found = xTree.reportProjectionsIntersecting(xQuery);
    CHECK(found.size() == 2);
    CHECK(std::find(found.begin(), found.end(), first) != found.end());
    CHECK(std::find(found.begin(), found.end(), second) != found.end());

    const auto contained = xTree.reportProjectionsContainedIn(xQuery);
    CHECK(contained == std::vector<Segment>{second});
    std::size_t containedVisited = 0;
    xTree.visitProjectionsContainedIn(xQuery, [&](const Segment&) { ++containedVisited; });
    CHECK(containedVisited == 1);
}

TEST_CASE("IntervalTree queries are projections rather than 2D predicates") {
    const Segment elevated(Point(1, 100), Point(9, 100));
    pgl::IntervalTree<Segment> tree(std::vector{elevated});

    const Point pointOnProjection(5, 0);
    CHECK_FALSE(elevated.intersects(pointOnProjection));
    CHECK(tree.countProjectionsIntersecting(pointOnProjection) == 1);
    CHECK(tree.countIntersecting(pointOnProjection) == 0);
    CHECK(tree.emptyIntersecting(pointOnProjection));

    const Rect flatWindow(0, 0, 10, 0);
    CHECK_FALSE(flatWindow.contains(elevated));
    CHECK(tree.countProjectionsContainedIn(flatWindow) == 1);
    CHECK(tree.countContainedIn(flatWindow) == 0);
    CHECK(tree.emptyContainedIn(flatWindow));
}

TEST_CASE("IntervalTree exact query family matches ShapeTree") {
    const Segment contained(Point(1, 1), Point(9, 1));
    const Segment crossing(Point(-5, 5), Point(5, 5));
    const Segment projectedOnly(Point(1, 50), Point(9, 50));
    const Segment outside(Point(20, 0), Point(30, 0));
    const std::vector<Segment> shapes{contained, crossing, projectedOnly, outside};
    const Rect query(0, 0, 10, 10);

    const pgl::ShapeTree<Segment> shapeTree(shapes);
    const pgl::IntervalTree<Segment> xTree(shapes);
    const pgl::IntervalTree<Segment, pgl::ProjectionAxis::y> yTree(shapes);
    const auto expectedIntersecting = shapeTree.reportIntersecting(query);
    const auto expectedContained = shapeTree.reportContainedIn(query);

    const auto checkTree = [&](const auto& tree) {
        CHECK(tree.countIntersecting(query) == shapeTree.countIntersecting(query));
        CHECK(tree.countContainedIn(query) == shapeTree.countContainedIn(query));
        CHECK(tree.emptyIntersecting(query) == shapeTree.emptyIntersecting(query));
        CHECK(tree.emptyContainedIn(query) == shapeTree.emptyContainedIn(query));

        const auto actualIntersecting = tree.reportIntersecting(query);
        const auto actualContained = tree.reportContainedIn(query);
        CHECK(actualIntersecting.size() == expectedIntersecting.size());
        CHECK(actualContained.size() == expectedContained.size());
        CHECK(std::is_permutation(actualIntersecting.begin(), actualIntersecting.end(),
                                  expectedIntersecting.begin(), expectedIntersecting.end()));
        CHECK(std::is_permutation(actualContained.begin(), actualContained.end(),
                                  expectedContained.begin(), expectedContained.end()));

        std::size_t visited = 0;
        tree.visitIntersecting(query, [&](const Segment&) { ++visited; });
        CHECK(visited == expectedIntersecting.size());
    };
    // Both axes must give the same exact result; only their pruning differs.
    checkTree(xTree);
    checkTree(yTree);
}

TEST_CASE("IntervalTree supports duplicate intervals, visitors, and erasure") {
    const Segment duplicate(Point(3, 1), Point(3, 4));
    const Segment other(Point(4, 0), Point(8, 0));
    pgl::IntervalTree<Segment> tree;
    tree.insert(duplicate);
    tree.insert(duplicate);
    tree.insert(other);

    CHECK(tree.size() == 3);
    CHECK(tree.has(duplicate));
    CHECK(tree.countProjectionsIntersecting(Rect(3, -1, 3, 10)) == 2);

    std::size_t visited = 0;
    CHECK(tree.visitProjectionsIntersecting(Rect(0, -1, 10, 10), [&](const Segment&) {
        ++visited;
        return true;
    }));
    CHECK(visited == 1);

    CHECK(tree.erase(duplicate));
    CHECK(tree.size() == 2);
    CHECK(tree.has(duplicate));
    CHECK(tree.countProjectionsIntersecting(Rect(3, -1, 3, 10)) == 1);
    CHECK(tree.erase(duplicate));
    CHECK_FALSE(tree.has(duplicate));
    CHECK_FALSE(tree.erase(duplicate));
    CHECK(tree.size() == 1);
}

TEST_CASE("IntervalTree matches brute force through randomized inserts and erases") {
    Rng rng{0x2c1b3c6d5e4f7081ULL};
    std::vector<Segment> reference;
    pgl::IntervalTree<Segment> xTree;
    pgl::IntervalTree<Segment, pgl::ProjectionAxis::y> yTree;

    for (int step = 0; step < 500; ++step) {
        if (reference.empty() || rng.range(0, 99) < 65) {
            const Segment shape = randomSegment(rng);
            reference.push_back(shape);
            xTree.insert(shape);
            yTree.insert(shape);
        } else {
            const std::size_t index = static_cast<std::size_t>(rng.range(0, static_cast<int>(reference.size() - 1)));
            const Segment shape = reference[index];
            CHECK(xTree.erase(shape));
            CHECK(yTree.erase(shape));
            reference[index] = reference.back();
            reference.pop_back();
        }

        for (int queryNumber = 0; queryNumber < 3; ++queryNumber) {
            const int x1 = rng.range(-60, 60);
            const int x2 = rng.range(-60, 60);
            const int y1 = rng.range(-60, 60);
            const int y2 = rng.range(-60, 60);
            const Rect query(x1, y1, x2, y2);

            const std::size_t xIntersections = bruteIntersecting<pgl::ProjectionAxis::x>(reference, query);
            const std::size_t yIntersections = bruteIntersecting<pgl::ProjectionAxis::y>(reference, query);
            const std::size_t xContained = bruteContained<pgl::ProjectionAxis::x>(reference, query);
            const std::size_t yContained = bruteContained<pgl::ProjectionAxis::y>(reference, query);

            CHECK(xTree.countProjectionsIntersecting(query) == xIntersections);
            CHECK(yTree.countProjectionsIntersecting(query) == yIntersections);
            CHECK(xTree.countProjectionsContainedIn(query) == xContained);
            CHECK(yTree.countProjectionsContainedIn(query) == yContained);
            CHECK(xTree.emptyProjectionsIntersecting(query) == (xIntersections == 0));
            CHECK(yTree.emptyProjectionsContainedIn(query) == (yContained == 0));
        }
    }
}

TEST_CASE("IntervalTree queries stay exact through a long run of removals") {
    Rng rng{0x9e3779b97f4a7c15ULL};
    std::vector<Segment> reference;
    pgl::IntervalTree<Segment> tree;

    for (int i = 0; i < 150; ++i) {
        const Segment shape = randomSegment(rng);
        reference.push_back(shape);
        tree.insert(shape);
    }

    // Removing nearly everything one shape at a time rebalances the tree
    // through every removal case, and each state is queried.
    while (reference.size() > 4) {
        const std::size_t index =
            static_cast<std::size_t>(rng.range(0, static_cast<int>(reference.size() - 1)));
        const Segment shape = reference[index];
        CHECK(tree.erase(shape));
        reference[index] = reference.back();
        reference.pop_back();
        CHECK(tree.size() == reference.size());

        const Rect query(rng.range(-60, 60), rng.range(-60, 60),
                         rng.range(-60, 60), rng.range(-60, 60));

        CHECK(tree.countProjectionsIntersecting(query) ==
              bruteIntersecting<pgl::ProjectionAxis::x>(reference, query));
        CHECK(tree.reportProjectionsIntersecting(query).size() ==
              bruteIntersecting<pgl::ProjectionAxis::x>(reference, query));
        CHECK(tree.countProjectionsContainedIn(query) ==
              bruteContained<pgl::ProjectionAxis::x>(reference, query));

        const std::size_t meeting = static_cast<std::size_t>(
            std::count_if(reference.begin(), reference.end(),
                          [&](const Segment& s) { return s.intersects(query); }));
        CHECK(tree.countIntersecting(query) == meeting);
        CHECK(tree.reportIntersecting(query).size() == meeting);
        CHECK(tree.emptyIntersecting(query) == (meeting == 0));

        const std::size_t inside = static_cast<std::size_t>(
            std::count_if(reference.begin(), reference.end(),
                          [&](const Segment& s) { return query.contains(s); }));
        CHECK(tree.countContainedIn(query) == inside);
        CHECK(tree.reportContainedIn(query).size() == inside);
    }

    // Iteration and shapes() stay compact and hold exactly the surviving
    // shapes.
    std::vector<Segment> stored(tree.begin(), tree.end());
    CHECK(stored.size() == reference.size());
    CHECK(tree.shapes().size() == reference.size());
    std::sort(stored.begin(), stored.end());
    std::vector<Segment> expected = reference;
    std::sort(expected.begin(), expected.end());
    CHECK(stored == expected);
    for (const Segment& shape : reference) {
        CHECK(tree.has(shape));
    }

    // Emptying the tree leaves it usable.
    for (const Segment& shape : reference) {
        CHECK(tree.erase(shape));
    }
    CHECK(tree.empty());
    CHECK(tree.size() == 0);
    const Rect everything(-100, -100, 100, 100);
    CHECK(tree.countProjectionsIntersecting(everything) == 0);
    CHECK(tree.emptyIntersecting(everything));

    const Segment revived(Point(1, 2), Point(3, 4));
    tree.insert(revived);
    CHECK(tree.size() == 1);
    CHECK(tree.has(revived));
    CHECK(tree.countIntersecting(everything) == 1);
    CHECK(tree.shapes() == std::vector<Segment>{revived});
}

TEST_CASE("IntervalTree survives removals among equal intervals and duplicate shapes") {
    // A tiny coordinate range makes most shapes share their projected interval
    // and many of them be equal, which is where a removal relabels nodes whose
    // order is decided by the node ID.
    Rng rng{0x243f6a8885a308d3ULL};
    std::vector<Segment> reference;
    pgl::IntervalTree<Segment, pgl::ProjectionAxis::y> tree;

    for (int step = 0; step < 400; ++step) {
        if (reference.empty() || rng.range(0, 99) < 55) {
            const Segment shape(Point(rng.range(0, 3), rng.range(0, 3)),
                                Point(rng.range(0, 3), rng.range(0, 3)));
            reference.push_back(shape);
            tree.insert(shape);
        } else {
            const std::size_t index =
                static_cast<std::size_t>(rng.range(0, static_cast<int>(reference.size() - 1)));
            const Segment shape = reference[index];
            CHECK(tree.erase(shape));
            reference[index] = reference.back();
            reference.pop_back();
        }

        CHECK(tree.size() == reference.size());
        for (const Segment& shape : reference) {
            CHECK(tree.has(shape));
        }

        const Rect query(rng.range(0, 3), rng.range(0, 3), rng.range(0, 3), rng.range(0, 3));
        CHECK(tree.countProjectionsIntersecting(query) ==
              bruteIntersecting<pgl::ProjectionAxis::y>(reference, query));
        CHECK(tree.countProjectionsContainedIn(query) ==
              bruteContained<pgl::ProjectionAxis::y>(reference, query));
        CHECK(tree.reportProjectionsIntersecting(query).size() ==
              bruteIntersecting<pgl::ProjectionAxis::y>(reference, query));
    }

    // Every stored shape can be removed, one occurrence at a time.
    while (!reference.empty()) {
        CHECK(tree.erase(reference.back()));
        reference.pop_back();
        CHECK(tree.size() == reference.size());
    }
    CHECK(tree.empty());
}

TEST_CASE("IntervalTree queries do not pay for removed intervals") {
    // Long intervals are inserted among short ones and then removed, the way
    // the xy sweep closes boxes. A query meeting only the removed intervals
    // must stay logarithmic: pruning follows the stored intervals alone.
    const int m = 3000;
    const int ceiling = 1000000;
    std::vector<Stick> live;
    std::vector<Stick> removed;
    pgl::IntervalTree<Stick> tree;
    for (int i = 0; i < m; ++i) {
        removed.push_back({10 * i, ceiling, 2 * i});
        live.push_back({10 * i + 5, 10 * i + 6, 2 * i + 1});
        tree.insert(removed.back());
        tree.insert(live.back());
    }
    Rng rng{0x5bd1e9955bd1e995ULL};
    for (std::size_t i = removed.size(); i > 1; --i) {
        std::swap(removed[i - 1],
                  removed[static_cast<std::size_t>(rng.range(0, static_cast<int>(i - 1)))]);
    }

    const std::size_t logBound = 64 * (floorLog2(2 * static_cast<std::size_t>(m)) + 1);
    for (std::size_t i = 0; i < removed.size(); ++i) {
        CHECK(tree.erase(removed[i]));
        if (i % 97 != 0 && i + 1 != removed.size()) {
            continue;
        }
        std::vector<Stick> stored = live;
        stored.insert(stored.end(), removed.begin() + static_cast<std::ptrdiff_t>(i + 1),
                      removed.end());
        CHECK(tree.size() == stored.size());

        // Above every short interval, below the ceiling.
        const int low = 10 * m + 100;
        comparisons = 0;
        const std::size_t count = tree.countProjectionsIntersecting(
            Stick{low, low + 1, -1});
        CHECK(count == bruteSticksIntersecting(stored, low, low + 1));
        if (i + 1 == removed.size()) {
            CHECK(count == 0);
            CHECK(comparisons <= logBound);
        }

        const int a = rng.range(0, 10 * m);
        const int b = a + rng.range(0, 40);
        CHECK(tree.countProjectionsIntersecting(Stick{a, b, -1}) ==
              bruteSticksIntersecting(stored, a, b));
        CHECK(tree.reportProjectionsIntersecting(Stick{a, b, -1}).size() ==
              bruteSticksIntersecting(stored, a, b));
    }

    // With the long intervals gone, every narrow query reports at most a few
    // shapes and so touches a logarithmic part of the tree.
    for (int q = 0; q < 50; ++q) {
        const int a = rng.range(-20, 10 * m + 20);
        comparisons = 0;
        std::size_t reported = 0;
        (void)tree.visitProjectionsIntersecting(Stick{a, a + 3, -1},
                                                [&](const Stick&) { ++reported; });
        CHECK(reported == bruteSticksIntersecting(live, a, a + 3));
        CHECK(comparisons <= logBound * (reported + 1));
    }
}

TEST_CASE("IntervalTree removal does not pay for removed equal intervals") {
    // Many shapes share one projected interval and nearly all are removed;
    // looking up that interval must then cost only the shapes still sharing it.
    const int m = 3000;
    pgl::IntervalTree<Stick, pgl::ProjectionAxis::y> tree;
    std::vector<Stick> others;
    for (int i = 0; i < m; ++i) {
        tree.insert(Stick{0, 10, i});
    }
    for (int i = 0; i <= m; ++i) {
        others.push_back({100 + 3 * i, 101 + 3 * i, m + i});
        tree.insert(others.back());
    }
    for (int i = 0; i + 1 < m; ++i) {
        CHECK(tree.erase(Stick{0, 10, i}));
    }
    CHECK(tree.size() == static_cast<std::size_t>(m + 2));

    const std::size_t logBound = 64 * (floorLog2(static_cast<std::size_t>(2 * m)) + 1);
    const Stick kept{0, 10, m - 1};
    const Stick absent{0, 10, -1};
    for (int round = 0; round < 20; ++round) {
        comparisons = 0;
        CHECK_FALSE(tree.erase(absent));
        CHECK_FALSE(tree.has(absent));
        CHECK(tree.has(kept));
        CHECK(comparisons <= 3 * logBound);

        comparisons = 0;
        CHECK(tree.erase(kept));
        tree.insert(kept);
        CHECK(comparisons <= 3 * logBound);
    }
    CHECK(tree.countProjectionsIntersecting(Stick{0, 10, -1}) == 1);
    for (const Stick& other : others) {
        CHECK(tree.has(other));
    }
    CHECK(tree.size() == static_cast<std::size_t>(m + 2));
}
