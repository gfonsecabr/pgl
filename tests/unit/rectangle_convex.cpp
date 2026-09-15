#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <random>
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
        const auto result = sq.intersection<int>(r);
        REQUIRE_MESSAGE(result, "sq ∩ overlapping rect should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Convex>(*result),
                      "area overlap clips to a Convex");
    }

    SUBCASE("edge-adjacent: intersection is a Segment") {
        const Rectangle r({4, 0}, {6, 4});  // shares right edge x=4 with the square
        const auto result = sq.intersection<int>(r);
        REQUIRE_MESSAGE(result, "sq ∩ adjacent rect should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Segment>(*result),
                      "shared-edge intersection is a Segment");
    }

    SUBCASE("disjoint: no intersection") {
        const Rectangle r({10, 10}, {12, 12});
        CHECK_FALSE_MESSAGE(sq.intersection<int>(r), "sq ∩ disjoint rect should be empty");
    }
}

TEST_CASE("Rectangle and Convex squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Rectangle r({8, 0}, {12, 4});

    // Farthest vertex on either side is at squared distance 64 (opposite corners).
    CHECK(sq.squaredHausdorffDistance<int>(r) == 64);
    CHECK(r.squaredHausdorffDistance<int>(sq) == 64);
}

TEST_CASE("Rectangle unites with Convex into a set of regions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect(Point(0, 0), Point(4, 4));
    const Convex offset(std::vector<Point>{{2, 2}, {6, 2}, {6, 6}, {2, 6}});

    SUBCASE("the union is the same set whichever operand receives it") {
        const auto fromRect = rect.regularizedUnion<int>(offset);
        const auto fromConvex = offset.regularizedUnion<int>(rect);
        static_assert(std::is_same_v<decltype(fromRect), const pgl::PolygonSet<Point>>);
        static_assert(std::is_same_v<decltype(fromConvex), const pgl::PolygonSet<Point>>);
        CHECK(fromRect == fromConvex);
        REQUIRE(fromRect.componentCount() == 1);
        CHECK(fromRect.twiceArea() == 2 * (16 + 16 - 4));
        CHECK(fromRect.component(0).outer().size() == 8);
    }

    SUBCASE("a covered rectangle leaves just the cover") {
        const Convex big(std::vector<Point>{{-1, -1}, {9, -1}, {9, 9}, {-1, 9}});
        const auto result = rect.regularizedUnion<int>(big);
        REQUIRE(result.componentCount() == 1);
        CHECK(result.component(0) == pgl::PolygonWithHoles<Point>(big.asPolygon()));
    }

    SUBCASE("disjoint operands stay two components either way round") {
        const Convex away(std::vector<Point>{{10, 10}, {13, 10}, {13, 13}, {10, 13}});
        CHECK(rect.regularizedUnion<int>(away).componentCount() == 2);
        CHECK(away.regularizedUnion<int>(rect) == rect.regularizedUnion<int>(away));
    }
}

namespace {

template <class Point>
pgl::Convex<Point> randomRectangleClipConvex(std::mt19937& generator) {
    using Number = typename Point::NumberType;
    std::vector<Point> points;
    const bool scattered = generator() % 3 == 0;
    const int count = scattered ? 1 + static_cast<int>(generator() % 5) : 3 + static_cast<int>(generator() % 60);
    const int radius = 4 + static_cast<int>(generator() % 40);
    for (int i = 0; i < count; ++i) {
        if (scattered) {
            points.emplace_back(Number(static_cast<int>(generator() % 9) - 4),
                                Number(static_cast<int>(generator() % 9) - 4));
        } else {
            const double angle = 2 * 3.141592653589793 * i / count;
            points.emplace_back(Number(static_cast<int>(std::lround(radius * std::cos(angle)))),
                                Number(static_cast<int>(std::lround(radius * std::sin(angle)))));
        }
    }
    return pgl::Convex<Point>(points);
}

}  // namespace

TEST_CASE_TEMPLATE("A rectangle clip equals the clip by the rectangle as a Convex",
                   Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::ERational>) {
    using Number = typename Point::NumberType;
    using ResultPoint = pgl::Point<pgl::ERational>;
    std::mt19937 generator(2026);
    for (int trial = 0; trial < 400; ++trial) {
        const pgl::Convex<Point> convex = randomRectangleClipConvex<Point>(generator);
        const auto coordinate = [&] { return static_cast<int>(generator() % 81) - 40; };
        int x0 = coordinate(), y0 = coordinate(), x1 = coordinate(), y1 = coordinate();
        if (generator() % 5 == 0) {
            x1 = x0;  // a segment or a point
        }
        const pgl::Rectangle<Point> box(Point(Number(std::min(x0, x1)), Number(std::min(y0, y1))),
                                        Point(Number(std::max(x0, x1)), Number(std::max(y0, y1))));
        const auto clipped = convex.template intersection<pgl::ERational>(box);
        // Clipped in exact coordinates, where the reference needs no rounding.
        const pgl::Rectangle<ResultPoint> exactBox{ResultPoint(box.min()), ResultPoint(box.max())};
        const auto expected = pgl::Convex<ResultPoint>(convex).template intersection<pgl::ERational>(exactBox.asConvex());
        CHECK_MESSAGE(clipped == expected, convex, " ", box);
        if (clipped && std::holds_alternative<pgl::Convex<ResultPoint>>(*clipped)) {
            CHECK(std::get<pgl::Convex<ResultPoint>>(*clipped).size() >= 3);
        }
    }
}
