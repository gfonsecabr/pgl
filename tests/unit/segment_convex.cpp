#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

TEST_CASE("Segment and triangle as convex predicates tests") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex triangle({{0, 0}, {6, 0}, {0, 6}});

    SUBCASE("edge-to-edge cut") {
        const Segment cut({-1, 2}, {5, 2});
        const OrientedSegment oriented_cut({5, 2}, {-1, 2});
        const Point interior_hit(2, 2);

        CHECK(cut.intersects(triangle));
        CHECK(triangle.intersects(cut));
        CHECK(oriented_cut.intersects(triangle));

        CHECK_FALSE(cut.contains(triangle));
        CHECK_FALSE(triangle.contains(cut));
        CHECK_FALSE(triangle.boundaryContains(cut));

        CHECK(cut.contains(interior_hit));
        CHECK(cut.interiorContains(interior_hit));
        CHECK(triangle.contains(interior_hit));
        CHECK(triangle.interiorContains(interior_hit));

        CHECK(cut.separates(triangle));
        CHECK(triangle.separates(cut));
        CHECK(oriented_cut.separates(triangle));
        CHECK(cut.crosses(triangle));
        CHECK(triangle.crosses(cut));
        CHECK(oriented_cut.crosses(triangle));

        const auto clipped = triangle.intersection(cut);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 2), Point(4, 2)));
    }

    SUBCASE("vertex-to-opposite-edge cut") {
        const Segment cut({0, 0}, {3, 3});

        CHECK(cut.intersects(triangle));
        CHECK(triangle.intersects(cut));
        CHECK(cut.separates(triangle));
        CHECK_FALSE(triangle.separates(cut));
        CHECK_FALSE(cut.crosses(triangle));
        CHECK_FALSE(triangle.crosses(cut));
    }

    SUBCASE("single-vertex touch") {
        const Segment touch({-1, -1}, {0, 0});
        const Point shared_vertex(0, 0);

        CHECK(touch.intersects(triangle));
        CHECK(triangle.intersects(touch));

        CHECK(touch.contains(shared_vertex));
        CHECK(touch.boundaryContains(shared_vertex));
        CHECK(triangle.contains(shared_vertex));
        CHECK_FALSE(triangle.interiorContains(shared_vertex));
        CHECK(triangle.boundaryContains(shared_vertex));

        CHECK_FALSE(touch.separates(triangle));
        CHECK_FALSE(triangle.separates(touch));
        CHECK_FALSE(touch.crosses(triangle));
        CHECK_FALSE(triangle.crosses(touch));
    }

    SUBCASE("segment starts strictly inside triangle") {
        const Segment starts_inside({1, 1}, {5, 1});

        CHECK(starts_inside.intersects(triangle));
        CHECK(triangle.intersects(starts_inside));

        CHECK_FALSE(starts_inside.separates(triangle));
        CHECK_FALSE(triangle.separates(starts_inside));
        CHECK_FALSE(starts_inside.crosses(triangle));
        CHECK_FALSE(triangle.crosses(starts_inside));
    }

    SUBCASE("segment lies on a triangle edge") {
        const Segment along_edge({0, 0}, {6, 0});
        const Point edge_midpoint(3, 0);

        CHECK(along_edge.intersects(triangle));
        CHECK(triangle.intersects(along_edge));

        CHECK(along_edge.contains(edge_midpoint));
        CHECK(along_edge.interiorContains(edge_midpoint));
        CHECK(triangle.boundaryContains(edge_midpoint));
        CHECK_FALSE(triangle.interiorContains(edge_midpoint));

        CHECK_FALSE(along_edge.separates(triangle));
        CHECK_FALSE(triangle.separates(along_edge));
        CHECK_FALSE(along_edge.crosses(triangle));
        CHECK_FALSE(triangle.crosses(along_edge));

        const auto overlap = triangle.intersection(along_edge);
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Segment>(*overlap));
        CHECK(std::get<Segment>(*overlap) == along_edge);
    }

    SUBCASE("strictly interior segment is contained but does not cut") {
        const Segment inner({1, 1}, {2, 2});

        CHECK(inner.intersects(triangle));
        CHECK(triangle.intersects(inner));
        CHECK(triangle.contains(inner));
        CHECK_FALSE(inner.contains(triangle));
        CHECK_FALSE(triangle.boundaryContains(inner));
        CHECK_FALSE(inner.separates(triangle));
        CHECK_FALSE(triangle.separates(inner));
        CHECK_FALSE(inner.crosses(triangle));
        CHECK_FALSE(triangle.crosses(inner));
    }

    SUBCASE("boundary subsegment is contained by the boundary only") {
        const Segment boundary_subsegment({1, 0}, {3, 0});
        const Point boundary_midpoint(2, 0);

        CHECK(boundary_subsegment.intersects(triangle));
        CHECK(triangle.intersects(boundary_subsegment));
        CHECK(triangle.contains(boundary_subsegment));
        CHECK(triangle.boundaryContains(boundary_subsegment));
        CHECK_FALSE(triangle.interiorContains(boundary_midpoint));
        CHECK(boundary_subsegment.contains(boundary_midpoint));
        CHECK_FALSE(boundary_subsegment.separates(triangle));
        CHECK_FALSE(triangle.separates(boundary_subsegment));
    }
}

TEST_CASE("Segment and pentagon predicates tests") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;

    Convex convex1({{0, 0}, {8, 8}, {16, 8}, {24, 0}, {12, -8}});
    Convex convex2({{0, 0}, {0, 8}, {16, 8}, {16, 0}, {12, -8}});
    convex1 += Point(36, 24);
    convex2 -= Point(36, 24);

    for(const auto& convex : {convex1, convex2}) {
        std::vector<Segment> edges = convex.edges();
        for(auto & edge: edges) {
            CHECK(edge.intersects(convex));
            CHECK(convex.intersects(edge));
            CHECK(convex.contains(edge));
            CHECK(convex.boundaryContains(edge));
            CHECK_FALSE(edge.contains(convex));
            CHECK_FALSE(edge.separates(convex));
            CHECK_FALSE(convex.separates(edge));
            CHECK_FALSE(edge.crosses(convex));
            CHECK_FALSE(convex.crosses(edge));
            CHECK_FALSE(convex.interiorsIntersect(edge));

            const auto miniEdge = (edge - edge.midpoint()) / 2 + edge.midpoint();

            CHECK(miniEdge.intersects(convex));
            CHECK(convex.intersects(miniEdge));
            CHECK(convex.contains(miniEdge));
            CHECK(convex.boundaryContains(miniEdge));
            CHECK_FALSE(miniEdge.contains(convex));
            CHECK_FALSE(miniEdge.separates(convex));
            CHECK_FALSE(convex.separates(miniEdge));
            CHECK_FALSE(miniEdge.crosses(convex));
            CHECK_FALSE(convex.crosses(miniEdge));
            CHECK_FALSE(convex.interiorsIntersect(miniEdge));

            const auto maxiEdge = (edge - edge.midpoint()) * 2 + edge.midpoint();

            CHECK(maxiEdge.intersects(convex));
            CHECK(convex.intersects(maxiEdge));
            CHECK_FALSE(convex.contains(maxiEdge));
            CHECK_FALSE(convex.boundaryContains(maxiEdge));
            CHECK_FALSE(maxiEdge.contains(convex));
            CHECK_FALSE(maxiEdge.separates(convex));
            // maxiEdge runs along the edge and overhangs both vertices, so
            // removing the convex polygon leaves a stub on each side.
            CHECK(convex.separates(maxiEdge));
            CHECK_FALSE(maxiEdge.crosses(convex));
            CHECK_FALSE(convex.crosses(maxiEdge));
            CHECK_FALSE(convex.interiorsIntersect(maxiEdge));

            const auto extension = maxiEdge - maxiEdge[0] + edge.midpoint();
            CHECK(extension.intersects(convex));
            CHECK(convex.intersects(extension));
            CHECK_FALSE(convex.contains(extension));
            CHECK_FALSE(convex.boundaryContains(extension));
            CHECK_FALSE(extension.contains(convex));
            CHECK_FALSE(extension.separates(convex));
            CHECK_FALSE(convex.separates(extension));
            CHECK_FALSE(extension.crosses(convex));
            CHECK_FALSE(convex.crosses(extension));
            CHECK_FALSE(convex.interiorsIntersect(extension));
        }

        std::vector<Segment> diagonals;
        for(size_t i = 0; i < convex.size(); ++i) {
            diagonals.emplace_back(convex[i], convex.get(i + 2));
        }

        for(auto & diagonal: diagonals) {
            CHECK(diagonal.intersects(convex));
            CHECK(convex.intersects(diagonal));
            CHECK(convex.contains(diagonal));
            CHECK(diagonal.separates(convex));
            CHECK(convex.interiorsIntersect(diagonal));
            CHECK_FALSE(convex.interiorContains(diagonal));
            CHECK_FALSE(convex.boundaryContains(diagonal));
            CHECK_FALSE(diagonal.contains(convex));
            CHECK_FALSE(convex.separates(diagonal));
            CHECK_FALSE(diagonal.crosses(convex));
            CHECK_FALSE(convex.crosses(diagonal));

            const auto miniDiagonal = (diagonal - diagonal.midpoint()) / 2 + diagonal.midpoint();

            CHECK(miniDiagonal.intersects(convex));
            CHECK(convex.intersects(miniDiagonal));
            CHECK(convex.contains(miniDiagonal));
            CHECK_FALSE(miniDiagonal.separates(convex));
            CHECK(convex.interiorsIntersect(miniDiagonal));
            CHECK(convex.interiorContains(miniDiagonal));
            CHECK_FALSE(convex.boundaryContains(miniDiagonal));
            CHECK_FALSE(miniDiagonal.contains(convex));
            CHECK_FALSE(convex.separates(miniDiagonal));
            CHECK_FALSE(miniDiagonal.crosses(convex));
            CHECK_FALSE(convex.crosses(miniDiagonal));

            const auto maxiDiagonal = (diagonal - diagonal.midpoint()) * 2 + diagonal.midpoint();

            CHECK(maxiDiagonal.intersects(convex));
            CHECK(convex.intersects(maxiDiagonal));
            CHECK(maxiDiagonal.separates(convex));
            CHECK_FALSE(convex.contains(maxiDiagonal));
            CHECK(convex.interiorsIntersect(maxiDiagonal));
            CHECK_FALSE(convex.interiorContains(maxiDiagonal));
            CHECK_FALSE(convex.boundaryContains(maxiDiagonal));
            CHECK_FALSE(maxiDiagonal.contains(convex));
            CHECK(convex.separates(maxiDiagonal));
            CHECK(maxiDiagonal.crosses(convex));
            CHECK(convex.crosses(maxiDiagonal));
        }

        std::vector<Segment> chords;
        for(size_t i = 0; i < edges.size(); ++i) {
            chords.emplace_back(edges[i].midpoint(), convex.get(i + 2));
        }

        for(auto & chord: chords) {
            CHECK(chord.intersects(convex));
            CHECK(convex.intersects(chord));
            CHECK(convex.contains(chord));
            CHECK(chord.separates(convex));
            CHECK(convex.interiorsIntersect(chord));
            CHECK_FALSE(convex.interiorContains(chord));
            CHECK_FALSE(convex.boundaryContains(chord));
            CHECK_FALSE(chord.contains(convex));
            CHECK_FALSE(convex.separates(chord));
            CHECK_FALSE(chord.crosses(convex));
            CHECK_FALSE(convex.crosses(chord));

            const auto miniChord = (chord - chord.midpoint()) / 2 + chord.midpoint();

            CHECK(miniChord.intersects(convex));
            CHECK(convex.intersects(miniChord));
            CHECK(convex.contains(miniChord));
            CHECK_FALSE(miniChord.separates(convex));
            CHECK(convex.interiorsIntersect(miniChord));
            CHECK(convex.interiorContains(miniChord));
            CHECK_FALSE(convex.boundaryContains(miniChord));
            CHECK_FALSE(miniChord.contains(convex));
            CHECK_FALSE(convex.separates(miniChord));
            CHECK_FALSE(miniChord.crosses(convex));
            CHECK_FALSE(convex.crosses(miniChord));

            const auto maxiChord = (chord - chord.midpoint()) * 2 + chord.midpoint();

            CHECK(maxiChord.intersects(convex));
            CHECK(convex.intersects(maxiChord));
            CHECK(maxiChord.separates(convex));
            CHECK_FALSE(convex.contains(maxiChord));
            CHECK(convex.interiorsIntersect(maxiChord));
            CHECK_FALSE(convex.interiorContains(maxiChord));
            CHECK_FALSE(convex.boundaryContains(maxiChord));
            CHECK_FALSE(maxiChord.contains(convex));
            CHECK(convex.separates(maxiChord));
            CHECK(maxiChord.crosses(convex));
            CHECK(convex.crosses(maxiChord));
        }

        std::vector<Segment> chords2;
        for(size_t i = 0; i < edges.size(); ++i) {
            chords2.emplace_back(edges[i].midpoint(), edges[(i + 2) % edges.size()].midpoint());
        }

        for(auto & chord: chords2) {
            CHECK(chord.intersects(convex));
            CHECK(convex.intersects(chord));
            CHECK(convex.contains(chord));
            CHECK(chord.separates(convex));
            CHECK(convex.interiorsIntersect(chord));
            CHECK_FALSE(convex.interiorContains(chord));
            CHECK_FALSE(convex.boundaryContains(chord));
            CHECK_FALSE(chord.contains(convex));
            CHECK_FALSE(convex.separates(chord));
            CHECK_FALSE(chord.crosses(convex));
            CHECK_FALSE(convex.crosses(chord));

            const auto miniChord = (chord - chord.midpoint()) / 2 + chord.midpoint();

            CHECK(miniChord.intersects(convex));
            CHECK(convex.intersects(miniChord));
            CHECK(convex.contains(miniChord));
            CHECK_FALSE(miniChord.separates(convex));
            CHECK(convex.interiorsIntersect(miniChord));
            CHECK(convex.interiorContains(miniChord));
            CHECK_FALSE(convex.boundaryContains(miniChord));
            CHECK_FALSE(miniChord.contains(convex));
            CHECK_FALSE(convex.separates(miniChord));
            CHECK_FALSE(miniChord.crosses(convex));
            CHECK_FALSE(convex.crosses(miniChord));

            const auto maxiChord = (chord - chord.midpoint()) * 2 + chord.midpoint();

            CHECK(maxiChord.intersects(convex));
            CHECK(convex.intersects(maxiChord));
            CHECK(maxiChord.separates(convex));
            CHECK_FALSE(convex.contains(maxiChord));
            CHECK(convex.interiorsIntersect(maxiChord));
            CHECK_FALSE(convex.interiorContains(maxiChord));
            CHECK_FALSE(convex.boundaryContains(maxiChord));
            CHECK_FALSE(maxiChord.contains(convex));
            CHECK(convex.separates(maxiChord));
            CHECK(maxiChord.crosses(convex));
            CHECK(convex.crosses(maxiChord));
        }
    }
}

// Regression guards for the orientation-based Convex::interiorsIntersect(Segment)
// (the cyclicMaxOrPositive support-triangle method) and for the exact
// Convex::pointInside<ResultNumber>(), which previously truncated through an
// untyped inner triangle.pointInside() call.
TEST_CASE("Convex interiorsIntersect(Segment) adversarial cases") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;

    SUBCASE("octagon") {
        const Convex oct({{4, 0}, {8, 0}, {12, 4}, {12, 8}, {8, 12}, {4, 12}, {0, 8}, {0, 4}});

        CHECK(oct.interiorsIntersect(Segment({0, 6}, {12, 6})));   // chord across center
        CHECK(oct.interiorsIntersect(Segment({4, 0}, {8, 12})));   // vertex-to-vertex diagonal
        CHECK(oct.interiorsIntersect(Segment({6, 0}, {6, 2})));    // endpoint on edge, stabs in
        CHECK_FALSE(oct.interiorsIntersect(Segment({4, 0}, {8, -4})));   // touches one vertex only
        CHECK_FALSE(oct.interiorsIntersect(Segment({-4, 6}, {-1, 6})));  // outside; line crosses interior
        CHECK_FALSE(oct.interiorsIntersect(Segment({8, 0}, {12, 4})));   // lies along a slant edge
        CHECK_FALSE(oct.interiorsIntersect(Segment({5, 0}, {7, 0})));    // sub-segment of an edge
    }

    SUBCASE("axis-aligned square with vertical and horizontal edges") {
        const Convex sq({{0, 0}, {10, 0}, {10, 10}, {0, 10}});

        CHECK(sq.interiorsIntersect(Segment({5, -2}, {5, 12})));     // vertical chord
        CHECK(sq.interiorsIntersect(Segment({-2, -2}, {5, 5})));     // through a corner into interior
        CHECK(sq.interiorsIntersect(Segment({-1, 2}, {2, -1})));     // clips the corner triangle
        CHECK_FALSE(sq.interiorsIntersect(Segment({-5, 0}, {15, 0})));  // collinear, extends past bottom edge
        CHECK_FALSE(sq.interiorsIntersect(Segment({-1, 1}, {1, -1})));  // touches the corner vertex only

        // boundaryContains on vertical/horizontal sub-edges.
        CHECK(sq.boundaryContains(Segment({0, 2}, {0, 8})));    // left vertical sub-edge
        CHECK(sq.boundaryContains(Segment({10, 3}, {10, 9})));  // right vertical sub-edge
        CHECK(sq.boundaryContains(Segment({2, 0}, {7, 0})));    // bottom sub-edge
        CHECK_FALSE(sq.boundaryContains(Segment({0, 0}, {10, 10})));  // interior diagonal
    }

    SUBCASE("pointInside<ResultNumber> stays interior (no integer truncation)") {
        using Rational = pgl::Rational<int64_t>;

        const Convex tiny({{0, 0}, {1, 0}, {0, 1}});
        CHECK(tiny.interiorContains(tiny.pointInside<Rational>()));

        Convex translated({{0, 0}, {3, 0}, {3, 3}, {0, 3}});
        translated += Point(5, 5);
        CHECK(translated.interiorContains(translated.pointInside<Rational>()));
    }
}
