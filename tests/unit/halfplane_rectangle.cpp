#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

TEST_CASE("Halfplane containment of Rectangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    // upper = closed half-plane y >= 0, boundary y = 0
    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle inside({1, 1}, {3, 3});
    const Rectangle touching({1, 0}, {3, 2});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle outside({1, -3}, {3, -1});

    CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
    CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
    CHECK_MESSAGE(upper.contains(touching), upper, " contains ", touching);
    CHECK_FALSE_MESSAGE(upper.interiorContains(touching), upper, " interiorContains ", touching);
    CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains ", crossing);
    CHECK_FALSE_MESSAGE(upper.contains(outside), upper, " contains ", outside);
}

TEST_CASE("Halfplane boundaryContains Rectangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    // A degenerate rectangle that collapses to a line segment on the boundary.
    CHECK_MESSAGE(diagonal.boundaryContains(Rectangle({2, 2}, {2, 2})), diagonal,
                  " boundaryContains degenerate point rect on boundary");
    // A proper rectangle cannot lie entirely on a 1D boundary.
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(Rectangle({1, 1}, {3, 2})), diagonal,
                        " boundaryContains off-boundary rectangle");
}

TEST_CASE("Halfplane and Rectangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle inside({1, 1}, {3, 3});
    const Rectangle touching({1, 0}, {3, 2});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle outside({1, -3}, {3, -1});
    // Halfplane whose boundary is above the rectangle.
    const Halfplane outside_upper({0, 5}, {4, 5});
    // Halfplane tangent along the top edge.
    const Halfplane tangent_upper({0, 3}, {4, 3});

    SUBCASE("rectangle inside: both intersect, interiors intersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("crossing rectangle: both intersect, interiors intersect") {
        CHECK_MESSAGE(upper.intersects(crossing), upper, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(upper), crossing, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(crossing), upper, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(upper), crossing, " interiorsIntersect ", upper);
    }

    SUBCASE("outside rectangle: no intersection") {
        CHECK_FALSE_MESSAGE(upper.intersects(outside), upper, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(upper), outside, " intersects ", upper);
    }

    SUBCASE("halfplane above rectangle: no intersection") {
        const Rectangle low({0, 0}, {4, 3});
        CHECK_FALSE_MESSAGE(outside_upper.intersects(low), outside_upper, " intersects ", low);
    }

    SUBCASE("tangent along top: intersects but interiors do not") {
        const Rectangle low({0, 0}, {4, 3});
        CHECK_MESSAGE(tangent_upper.intersects(low), tangent_upper, " intersects ", low);
        CHECK_FALSE_MESSAGE(tangent_upper.interiorsIntersect(low), tangent_upper, " interiorsIntersect ", low);
    }
}

TEST_CASE("Halfplane never separates or crosses Rectangle, and Rectangle never separates Halfplane") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle inside({1, 1}, {3, 2});

    // A half-plane is convex: removing it from a rectangle leaves a convex region.
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);
    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);

    // A finite rectangle cannot cut across an infinite half-plane.
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}

// The clip cuts the rectangle edges wherever the boundary line meets them, so
// the results are asked for in ERational and stay exact.
TEST_CASE("Rectangle intersection with Halfplane: clips to Convex, Segment or Point") {
    using Point = pgl::Point<int>;
    using RectangleShape = pgl::Rectangle<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using EPoint = pgl::Point<pgl::ERational>;
    using ESegment = pgl::Segment<EPoint>;
    using EConvex = pgl::Convex<EPoint>;

    const RectangleShape rect(Point(0, 0), Point(4, 4));

    SUBCASE("halfplane cuts through the rectangle: the overlapping area is kept") {
        // Halfplane y >= 2 keeps the top half of the rectangle.
        const Halfplane upper({0, 2}, {4, 2});
        const auto r = rect.intersection<pgl::ERational>(upper);
        REQUIRE_MESSAGE(r, "rect ∩ cutting halfplane should be non-empty");
        REQUIRE(std::holds_alternative<EConvex>(*r));
        CHECK_MESSAGE(std::get<EConvex>(*r) ==
                          EConvex({EPoint(0, 2), EPoint(4, 2), EPoint(4, 4), EPoint(0, 4)}),
                      "top half of the rectangle");
    }

    SUBCASE("halfplane cuts off a corner: the overlap is a triangle") {
        // Halfplane x + y >= 4 keeps the corner triangle at (4,4).
        const Halfplane corner({0, 4}, {4, 0});
        const auto r = rect.intersection<pgl::ERational>(corner);
        REQUIRE_MESSAGE(r, "rect ∩ corner-cutting halfplane should be non-empty");
        REQUIRE(std::holds_alternative<EConvex>(*r));
        CHECK_MESSAGE(std::get<EConvex>(*r) ==
                          EConvex({EPoint(4, 0), EPoint(4, 4), EPoint(0, 4)}),
                      "corner triangle above the diagonal");
    }

    SUBCASE("halfplane contains the whole rectangle: the rectangle is returned") {
        const Halfplane all({0, -1}, {4, -1});  // y >= -1, entire rect inside
        const auto r = rect.intersection<pgl::ERational>(all);
        REQUIRE_MESSAGE(r, "rect ∩ containing halfplane should be non-empty");
        REQUIRE(std::holds_alternative<EConvex>(*r));
        CHECK_MESSAGE(std::get<EConvex>(*r) == EConvex(rect.asConvex()),
                      "whole rectangle kept when it lies inside the halfplane");
    }

    SUBCASE("halfplane tangent along an edge: that edge is the overlap") {
        const Halfplane tangent({0, 4}, {4, 4});  // y >= 4, touches the top edge
        const auto r = rect.intersection<pgl::ERational>(tangent);
        REQUIRE_MESSAGE(r, "rect ∩ tangent halfplane should be non-empty");
        REQUIRE(std::holds_alternative<ESegment>(*r));
        CHECK_MESSAGE(std::get<ESegment>(*r) == ESegment(EPoint(0, 4), EPoint(4, 4)),
                      "shared top edge");
    }

    SUBCASE("halfplane touching a single corner: the overlap is that corner") {
        const Halfplane tip({0, 8}, {8, 0});  // x + y >= 8, touches (4,4) only
        const auto r = rect.intersection<pgl::ERational>(tip);
        REQUIRE_MESSAGE(r, "rect ∩ corner-touching halfplane should be non-empty");
        REQUIRE(std::holds_alternative<EPoint>(*r));
        CHECK_MESSAGE(std::get<EPoint>(*r) == EPoint(4, 4), "shared corner");
    }

    SUBCASE("halfplane misses the rectangle: empty") {
        const Halfplane above({0, 10}, {4, 10});  // y >= 10, rect max y = 4
        CHECK_FALSE_MESSAGE(rect.intersection<pgl::ERational>(above),
                            "rect ∩ missing halfplane is empty");
    }

    SUBCASE("halfplane on the left gives the same clip") {
        const Halfplane upper({0, 2}, {4, 2});
        CHECK_MESSAGE(upper.intersection<pgl::ERational>(rect) ==
                          rect.intersection<pgl::ERational>(upper),
                      "halfplane ∩ rect agrees with rect ∩ halfplane");
    }

    SUBCASE("crossings off the corners stay exact") {
        // Boundary through (0,1) and (4,4): it enters the rectangle at (0,1) and
        // leaves it at (4,4), keeping everything above the cut.
        const Halfplane cut({0, 1}, {4, 4});
        const auto r = rect.intersection<pgl::ERational>(cut);
        REQUIRE_MESSAGE(r, "rect ∩ cutting halfplane should be non-empty");
        REQUIRE(std::holds_alternative<EConvex>(*r));
        CHECK_MESSAGE(std::get<EConvex>(*r) ==
                          EConvex({EPoint(0, 1), EPoint(4, 4), EPoint(0, 4)}),
                      "exact clip against the slanted boundary");
    }

    SUBCASE("a fractional crossing is kept exactly") {
        // Boundary through (1,0) and (2,3): it enters the rectangle at (1,0) and
        // leaves it through the top edge at x = 7/3, off the integer grid.
        const Halfplane cut({1, 0}, {2, 3});
        const auto r = rect.intersection<pgl::ERational>(cut);
        const pgl::ERational seven_thirds(7, 3);
        REQUIRE_MESSAGE(r, "rect ∩ cutting halfplane should be non-empty");
        REQUIRE(std::holds_alternative<EConvex>(*r));
        CHECK_MESSAGE(std::get<EConvex>(*r) ==
                          EConvex({EPoint(0, 0), EPoint(1, 0), EPoint(seven_thirds, 4),
                                   EPoint(0, 4)}),
                      "clip keeps the left side of the slanted boundary");
    }
}
