#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Inserting segments into rectangles") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;

    Rectangle r(1,2,3,4);
    CHECK(r.min() == Point(1, 2));
    CHECK(r.max() == Point(3, 4));

    r.insert(Segment(2, 1, 2, 3));
    CHECK(r.min() == Point(1, 1));
    CHECK(r.max() == Point(3, 4));

    const std::set<Segment> ordered_segments{{4, 1, 1, 5}, {3, 2, 2, 4}};
    r.insert(ordered_segments);
    CHECK(r.min() == Point(1, 1));
    CHECK(r.max() == Point(4, 5));

    const std::vector<Segment> unordered_segments{{3, 2, 8, -1}, {4, 6, 2, 5}};
    r.insert(unordered_segments);
    CHECK(r.min() == Point(1, -1));
    CHECK(r.max() == Point(8, 6));
}

TEST_CASE("Construct rectangles from containers of segments") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;

    {
        const std::set<Segment> ordered_segments{{4, 1, 1, 5}, {3, 2, 2, 4}};
        Rectangle r(ordered_segments);
        CHECK(r.min() == Point(1, 1));
        CHECK(r.max() == Point(4, 5));
    }

    {
        const std::vector<Segment> unordered_segments{{4, 1, 1, 5}, {3, 2, 2, 4}, {3, 2, 8, -1}, {4, 6, 2, 5}};
        Rectangle r(unordered_segments);
        CHECK(r.min() == Point(1, -1));
        CHECK(r.max() == Point(8, 6));
    }
}

TEST_CASE("Convex polygon bounding box") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Convex = pgl::Convex<Point>;

    SUBCASE("triangle") {
        const Convex t({{0, 0}, {6, 0}, {0, 6}});
        CHECK(t.bbox() == Rectangle(0, 0, 6, 6));
    }

    SUBCASE("axis-aligned square") {
        const Convex s({{0, 0}, {4, 0}, {4, 4}, {0, 4}});
        CHECK(s.bbox() == Rectangle(0, 0, 4, 4));
    }

    SUBCASE("extreme y vertices fall mid-chain, not at the stored ends") {
        // Regression: the leftmost vertex is neither the lowest nor the highest,
        // so the y-extremes sit at interior offsets of the stored vertex list.
        // A rotation-blind peak search picked the wrong vertex here.
        const Convex c({{7, 15}, {20, 8}, {25, 6}, {23, 30}});
        CHECK(c.bbox() == Rectangle(7, 6, 25, 30));
    }

    SUBCASE("top and bottom vertices both off the x-extremes") {
        const Convex c({{0, 5}, {5, 0}, {10, 5}, {5, 12}});
        CHECK(c.bbox() == Rectangle(0, 0, 10, 12));
    }

    SUBCASE("translated convex keeps an exact box") {
        Convex c({{7, 15}, {20, 8}, {25, 6}, {23, 30}});
        c += Point(100, -40);
        CHECK(c.bbox() == Rectangle(107, -34, 125, -10));
        c -= Point(100, -40);
        CHECK(c.bbox() == Rectangle(7, 6, 25, 30));
    }

    SUBCASE("degenerate convex shapes") {
        const Convex single({{3, 4}});
        CHECK(single.bbox() == Rectangle(3, 4, 3, 4));

        const Convex vertical({{2, 1}, {2, 9}});
        CHECK(vertical.bbox() == Rectangle(2, 1, 2, 9));

        const Convex horizontal({{-3, 5}, {8, 5}});
        CHECK(horizontal.bbox() == Rectangle(-3, 5, 8, 5));

        const Convex diagonal({{1, 1}, {7, 4}});
        CHECK(diagonal.bbox() == Rectangle(1, 1, 7, 4));
    }
}

TEST_CASE("Convex bbox matches a linear scan over random polygons") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Convex = pgl::Convex<Point>;

    std::mt19937 rng(20240528);
    std::uniform_int_distribution<int> coord(-50, 50);

    for (int iter = 0; iter < 4000; ++iter) {
        std::vector<Point> pts;
        const int k = std::uniform_int_distribution<int>(1, 9)(rng);
        for (int i = 0; i < k; ++i) {
            pts.emplace_back(coord(rng), coord(rng));
        }
        Convex c(pts);
        if (c.size() == 0) {
            continue;
        }

        // Linear-scan reference over the (possibly translated) vertices.
        int min_x = c[0].x(), max_x = c[0].x(), min_y = c[0].y(), max_y = c[0].y();
        for (std::size_t i = 1; i < c.size(); ++i) {
            min_x = std::min(min_x, c[i].x());
            max_x = std::max(max_x, c[i].x());
            min_y = std::min(min_y, c[i].y());
            max_y = std::max(max_y, c[i].y());
        }
        const Rectangle expected(min_x, min_y, max_x, max_y);

        REQUIRE(c.bbox() == expected);

        // Also exercise the translated path: a rigid shift must shift the box.
        Convex shifted = c;
        shifted += Point(13, -27);
        REQUIRE(shifted.bbox() == expected + Point(13, -27));
    }
}

