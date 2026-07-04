#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>
#include <vector>

TEST_CASE("Convex boundaryContains Rectangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    // Unit square (0,0)-(4,0)-(4,4)-(0,4)
    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    // Degenerate rectangle that collapses to a boundary segment.
    const Rectangle line_rect(Point(0, 0), Point(4, 0));
    CHECK_MESSAGE(sq.boundaryContains(line_rect), sq, " boundaryContains degenerate rect on edge");

    // Proper interior rectangle is not on the boundary.
    const Rectangle inner(Point(1, 1), Point(2, 2));
    CHECK_FALSE_MESSAGE(sq.boundaryContains(inner), sq, " boundaryContains interior rect");
}

TEST_CASE("Convex contains and interiorContains Rectangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    const Rectangle inner({1, 1}, {3, 3});
    const Rectangle on_edge({0, 0}, {4, 2});
    const Rectangle outside({5, 5}, {7, 7});
    const Rectangle crossing({3, 3}, {6, 6});

    CHECK_MESSAGE(sq.contains(inner), sq, " contains inner rect");
    CHECK_MESSAGE(sq.interiorContains(inner), sq, " interiorContains inner rect");
    CHECK_MESSAGE(sq.contains(on_edge), sq, " contains edge-touching rect");
    CHECK_FALSE_MESSAGE(sq.interiorContains(on_edge), sq, " interiorContains edge-touching rect");
    CHECK_FALSE_MESSAGE(sq.contains(outside), sq, " contains outside rect");
    CHECK_FALSE_MESSAGE(sq.contains(crossing), sq, " contains crossing rect");
}

TEST_CASE("Convex and Rectangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping: both intersect and interiors intersect") {
        const Rectangle r({2, 2}, {6, 6});
        CHECK_MESSAGE(sq.intersects(r), sq, " intersects ", r);
        CHECK_MESSAGE(r.intersects(sq), r, " intersects ", sq);
        CHECK_MESSAGE(sq.interiorsIntersect(r), sq, " interiorsIntersect ", r);
        CHECK_MESSAGE(r.interiorsIntersect(sq), r, " interiorsIntersect ", sq);
    }

    SUBCASE("disjoint: neither intersects") {
        const Rectangle r({10, 10}, {12, 12});
        CHECK_FALSE_MESSAGE(sq.intersects(r), sq, " intersects disjoint rect");
        CHECK_FALSE_MESSAGE(r.intersects(sq), "disjoint rect intersects ", sq);
        CHECK_FALSE_MESSAGE(sq.interiorsIntersect(r), sq, " interiorsIntersect disjoint");
    }

    SUBCASE("edge-adjacent: intersects but interiors do not") {
        const Rectangle adj({4, 0}, {6, 4});
        CHECK_MESSAGE(sq.intersects(adj), sq, " intersects adjacent rect");
        CHECK_FALSE_MESSAGE(sq.interiorsIntersect(adj), sq, " interiorsIntersect adjacent rect");
    }
}

TEST_CASE("Convex separates and crosses Rectangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    // A horizontal rectangle that pokes out both sides of the square.
    const Rectangle band({-1, 1}, {5, 3});
    CHECK_MESSAGE(sq.separates(band), sq, " separates band rect");

    // A polygon entirely inside is not separated.
    const Rectangle inner({1, 1}, {3, 3});
    CHECK_FALSE_MESSAGE(sq.separates(inner), sq, " separates inner rect");

    // A polygon entirely outside is not separated.
    const Rectangle outside({10, 10}, {12, 12});
    CHECK_FALSE_MESSAGE(sq.separates(outside), sq, " separates outside rect");
}

TEST_CASE("Rectangle separates and crosses Convex") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    // A horizontal convex strip that passes through the rectangle.
    const Convex strip(std::vector<Point>{{-1, 1}, {5, 1}, {5, 2}, {-1, 2}});

    CHECK_MESSAGE(rect.separates(strip), rect, " separates band convex");
    CHECK_MESSAGE(rect.crosses(strip), rect, " crosses band convex");
    CHECK_MESSAGE(strip.separates(rect), strip, " separates ", rect);
    CHECK_MESSAGE(strip.crosses(rect), strip, " crosses ", rect);

    // Inner convex is not separated.
    const Convex inner(std::vector<Point>{{1, 1}, {3, 1}, {3, 2}, {1, 2}});
    CHECK_FALSE_MESSAGE(rect.separates(inner), rect, " separates inner convex");
}

TEST_CASE("Convex intersection with Rectangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping area: intersection is a Convex") {
        const Rectangle r({2, 2}, {6, 6});
        const auto result = sq.intersection(r);
        REQUIRE_MESSAGE(result, "sq ∩ overlapping rect should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Convex>(*result),
                      "area overlap clips to a Convex");
    }

    SUBCASE("edge-adjacent: intersection is a Segment") {
        const Rectangle r({4, 0}, {6, 4});  // shares right edge x=4 with the square
        const auto result = sq.intersection(r);
        REQUIRE_MESSAGE(result, "sq ∩ adjacent rect should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Segment>(*result),
                      "shared-edge intersection is a Segment");
    }

    SUBCASE("disjoint: no intersection") {
        const Rectangle r({10, 10}, {12, 12});
        CHECK_FALSE_MESSAGE(sq.intersection(r), "sq ∩ disjoint rect should be empty");
    }
}

TEST_CASE("Rectangle and Convex squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Rectangle r({8, 0}, {12, 4});

    // Farthest vertex on either side is at squared distance 64 (opposite corners).
    CHECK(sq.squaredHausdorffDistance(r) == 64);
    CHECK(r.squaredHausdorffDistance(sq) == 64);
}
