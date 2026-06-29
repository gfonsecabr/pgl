#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// Neither shape contains the other: a 1D ray cannot hold a 2D polygon, and a
// polygon can only "contain" a degenerate (point-like) ray.
TEST_CASE("Ray and Polygon never contain each other") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Ray through({5, -5}, {5, 0});  // crosses the interior

    CHECK_FALSE(square.contains(through));
    CHECK_FALSE(through.contains(square));
}

// intersects / interiorsIntersect, both directions (the Ray side forwards to the
// Polygon implementation by symmetry).
TEST_CASE("Ray and Polygon intersection predicates") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a ray driving through the interior meets it everywhere") {
        const Ray through({5, -5}, {5, 0});
        CHECK(square.intersects(through));
        CHECK(through.intersects(square));
        CHECK(square.interiorsIntersect(through));
        CHECK(through.interiorsIntersect(square));
    }

    SUBCASE("a ray running along an edge grazes the boundary only") {
        const Ray edge({-5, 0}, {-4, 0});  // overlaps the bottom edge y = 0
        CHECK(square.intersects(edge));
        CHECK(edge.intersects(square));
        CHECK_FALSE(square.interiorsIntersect(edge));
        CHECK_FALSE(edge.interiorsIntersect(square));
    }

    SUBCASE("a ray pointing away misses the polygon") {
        const Ray away({-5, 5}, {-6, 5});
        CHECK_FALSE(square.intersects(away));
        CHECK_FALSE(away.intersects(square));
        CHECK_FALSE(square.interiorsIntersect(away));
        CHECK_FALSE(away.interiorsIntersect(square));
    }
}

// crosses requires mutual separation: a ray that is a full chord across cuts the
// polygon both ways; a ray that only enters (source inside) leaves a slit and
// does not cross.
TEST_CASE("Ray and Polygon cross only on a full transversal") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a ray crossing all the way through crosses both ways") {
        const Ray through({5, -5}, {5, 0});
        CHECK(square.crosses(through));
        CHECK(through.crosses(square));
    }

    SUBCASE("a ray launched from inside does not cross") {
        const Ray fromInside({5, 5}, {5, 6});
        CHECK(square.interiorsIntersect(fromInside));  // interiors do meet
        CHECK_FALSE(square.crosses(fromInside));        // but it is not a full cut
        CHECK_FALSE(fromInside.crosses(square));
    }
}

TEST_CASE("Polygon separates Ray") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("ray passing through with the source outside is cut in two") {
        // Source below, heading up: the polygon body interrupts the ray, leaving
        // a finite piece below and an infinite piece above.
        const Ray through({5, -5}, {5, 0});

        CHECK(square.separates(through));
    }

    SUBCASE("ray from inside heading out leaves a single piece") {
        const Ray leaving({5, 5}, {5, 6});

        CHECK(square.interiorContains(leaving.source()));
        CHECK_FALSE(square.separates(leaving));
    }

    SUBCASE("ray from a boundary point heading out leaves a single piece") {
        const Ray from_edge({5, 0}, {5, 6});

        CHECK(square.boundaryContains(from_edge.source()));
        CHECK_FALSE(square.separates(from_edge));
    }

    SUBCASE("ray from a corner into the interior leaves a single piece") {
        // Source at the corner vertex, heading into the interior: the ray's
        // inside part is one chord, so only one infinite outside piece remains.
        const Ray from_corner({0, 0}, {5, 8});

        CHECK(square.boundaryContains(from_corner.source()));
        CHECK_FALSE(square.separates(from_corner));
    }

    SUBCASE("ray missing the polygon entirely") {
        const Ray away({-5, 5}, {-6, 5});

        CHECK_FALSE(square.separates(away));
    }

    SUBCASE("ray pointing back through the polygon is cut") {
        // Source above, heading down: still crosses the whole body.
        const Ray down({5, 15}, {5, 14});

        CHECK(square.separates(down));
    }
}

TEST_CASE("Ray separates Polygon") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a ray that is a chord across cuts the polygon") {
        const Ray chord({5, -5}, {5, 0});

        CHECK(chord.separates(square));
    }

    SUBCASE("a ray with its source strictly inside leaves a slit") {
        const Ray slit({5, 5}, {5, 6});

        CHECK(square.interiorContains(slit.source()));
        CHECK_FALSE(slit.separates(square));
    }

    SUBCASE("a ray missing the polygon does not cut it") {
        const Ray away({-5, 5}, {-6, 5});

        CHECK_FALSE(away.separates(square));
    }

    SUBCASE("source on an edge, crossing through, cuts the polygon") {
        // This is where the two directions disagree: the ray is a chord clear
        // across (so it cuts the polygon), but the part of the ray inside the
        // closed polygon is bounded, leaving only one infinite outside piece, so
        // the polygon does not cut the ray.
        const Ray from_edge({5, 0}, {5, 1});

        CHECK(from_edge.separates(square));
        CHECK_FALSE(square.separates(from_edge));
    }
}

TEST_CASE("Ray separates Polygon for non-convex shapes") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("ray through the solid base cuts the shape") {
        const Ray low({-1, 2}, {0, 2});

        CHECK(low.separates(notch));
    }

    SUBCASE("ray rising inside the notch gap does not cut the shape") {
        // Source inside the notch void, heading up out of the shape: never
        // crosses the body as a complete chord.
        const Ray up({5, 6}, {5, 7});

        CHECK_FALSE(up.separates(notch));
    }
}

// Non-convex cases for the ray overload, mirroring the segment ones. Every
// expected value was checked against an exact rational ground truth.
TEST_CASE("Polygon separates Ray for reflex/edge-collinear non-convex cases") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});
    const Polygon ell({0, 0, 6, 0, 6, 3, 3, 3, 3, 6, 0, 6});

    SUBCASE("ray down the notch wall pokes out only the bottom") {
        // Source on the wall (6,5); down through the wall and solid base, out
        // the bottom: a single outside piece, so it must NOT separate.
        const Ray down_wall({6, 5}, {6, 4});

        CHECK_FALSE(notch.separates(down_wall));
    }

    SUBCASE("ray from inside an L through the reflex vertex stays one piece") {
        // Source inside the arm, heading through the reflex vertex (3,3) and out
        // the far side: one outside piece beyond the exit, so it must NOT
        // separate.
        const Ray through_reflex({5, 1}, {3, 3});

        CHECK_FALSE(ell.separates(through_reflex));
    }
}

TEST_CASE("Ray separates Polygon matches Ray separates Convex") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon polygon({0, 0, 10, 0, 13, 7, 5, 12, -3, 7});
    const Convex convex({{0, 0}, {10, 0}, {13, 7}, {5, 12}, {-3, 7}});

    const Ray cut({5, -5}, {5, -4});
    const Ray slit({5, 6}, {5, 7});
    const Ray miss({-8, 6}, {-9, 6});

    CHECK(cut.separates(polygon) == cut.separates(convex));
    CHECK(slit.separates(polygon) == slit.separates(convex));
    CHECK(miss.separates(polygon) == miss.separates(convex));

    CHECK(cut.separates(polygon));
    CHECK_FALSE(slit.separates(polygon));
    CHECK_FALSE(miss.separates(polygon));
}

// A ray that only touches the polygon boundary (grazing a vertex) is still split
// into a finite piece and an infinite one, so it separates. Ground-truth verified.
TEST_CASE("Polygon separates a boundary-touching ray") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Convex pentagon with apex at (5, 12).
    const Polygon pentagon({0, 0, 10, 0, 13, 7, 5, 12, -3, 7});
    // Ray grazing only the apex vertex (5, 12): a finite piece before it and an
    // infinite one after, so it must separate.
    const Ray apex_graze({-3, 12}, {9, 12});

    CHECK(pentagon.separates(apex_graze));
}

// Polygon::intersection(Ray) is the supporting line clipped to the half-line
// starting at the source.
TEST_CASE("Polygon intersects Ray from the source outward") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Segment>;

    const Polygon u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});

    SUBCASE("a ray entering from the left cuts both prongs") {
        const auto pieces = u.intersection(Ray({-5, 5}, {-4, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a ray starting inside reaches only the pieces ahead of it") {
        // Source at (5, 5) is in the open notch (outside); aimed right, it hits
        // only the right prong.
        const auto pieces = u.intersection(Ray({5, 5}, {6, 5}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a ray pointing away from the polygon misses it") {
        CHECK(u.intersection(Ray({-5, 5}, {-6, 5})).empty());
    }

    SUBCASE("a ray whose source is strictly inside starts the chord at the source") {
        // Source (1,5) is inside the left prong; aimed right it exits the left
        // prong, crosses the open notch, then re-enters the right prong.
        const auto pieces = u.intersection(Ray({1, 5}, {2, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({1, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }
}
