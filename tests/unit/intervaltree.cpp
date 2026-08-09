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

}  // namespace

TEST_CASE("IntervalTree indexes closed x and y projections") {
    const Segment first(Point(-10, 0), Point(2, 10));
    const Segment second(Point(2, 15), Point(7, 15));
    const Segment third(Point(20, -3), Point(20, 5));
    const std::vector<Segment> shapes{first, second, third};

    pgl::IntervalTree<Segment> empty;
    CHECK(empty.empty());
    CHECK(empty.countIntersecting(Rect(0, 0, 1, 1)) == 0);
    CHECK(empty.reportContainedIn(Rect(0, 0, 1, 1)).empty());
    CHECK(empty.emptyIntersecting(Rect(0, 0, 1, 1)));

    pgl::IntervalTree xTree(shapes);
    pgl::IntervalTree<Segment, pgl::ProjectionAxis::y> yTree(shapes);
    static_assert(std::is_same_v<decltype(xTree)::ShapeType, Segment>);

    const Rect xQuery(2, -100, 10, 100);
    CHECK(xTree.countIntersecting(xQuery) == 2);  // first touches at x = 2
    CHECK(xTree.countContainedIn(xQuery) == 1);
    CHECK_FALSE(xTree.emptyIntersecting(xQuery));
    CHECK_FALSE(xTree.emptyContainedIn(xQuery));

    const Rect yQuery(0, 5, 0, 15);
    CHECK(yTree.countIntersecting(yQuery) == 3);  // third touches at y = 5
    CHECK(yTree.countContainedIn(yQuery) == 1);

    const auto found = xTree.reportIntersecting(xQuery);
    CHECK(found.size() == 2);
    CHECK(std::find(found.begin(), found.end(), first) != found.end());
    CHECK(std::find(found.begin(), found.end(), second) != found.end());

    const auto contained = xTree.reportContainedIn(xQuery);
    CHECK(contained == std::vector<Segment>{second});
    std::size_t containedVisited = 0;
    xTree.visitContainedIn(xQuery, [&](const Segment&) { ++containedVisited; });
    CHECK(containedVisited == 1);
}

TEST_CASE("IntervalTree queries are projections rather than 2D predicates") {
    const Segment elevated(Point(1, 100), Point(9, 100));
    pgl::IntervalTree<Segment> tree(std::vector{elevated});

    const Point pointOnProjection(5, 0);
    CHECK_FALSE(elevated.intersects(pointOnProjection));
    CHECK(tree.countIntersecting(pointOnProjection) == 1);

    const Rect flatWindow(0, 0, 10, 0);
    CHECK_FALSE(flatWindow.contains(elevated));
    CHECK(tree.countContainedIn(flatWindow) == 1);
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
    CHECK(tree.countIntersecting(Rect(3, -1, 3, 10)) == 2);

    std::size_t visited = 0;
    CHECK(tree.visitIntersecting(Rect(0, -1, 10, 10), [&](const Segment&) {
        ++visited;
        return true;
    }));
    CHECK(visited == 1);

    CHECK(tree.erase(duplicate));
    CHECK(tree.size() == 2);
    CHECK(tree.has(duplicate));
    CHECK(tree.countIntersecting(Rect(3, -1, 3, 10)) == 1);
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

            CHECK(xTree.countIntersecting(query) == xIntersections);
            CHECK(yTree.countIntersecting(query) == yIntersections);
            CHECK(xTree.countContainedIn(query) == xContained);
            CHECK(yTree.countContainedIn(query) == yContained);
            CHECK(xTree.emptyIntersecting(query) == (xIntersections == 0));
            CHECK(yTree.emptyContainedIn(query) == (yContained == 0));
        }
    }
}
