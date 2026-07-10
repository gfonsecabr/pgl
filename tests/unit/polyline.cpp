#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <compare>
#include <concepts>
#include <sstream>
#include <unordered_set>
#include <vector>

// Polyline keeps the vertices in the given traversal order; the constructor
// only canonicalizes the direction, reversing the sequence when the reversed
// sequence is lexicographically smaller.
TEST_CASE("Polyline construction preserves the traversal order") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;

    SUBCASE("vertices are not sorted") {
        const Polyline zig({0, 0, 5, 5, 1, 0});
        REQUIRE(zig.size() == 3);
        CHECK(zig[0] == Point(0, 0));
        CHECK(zig[1] == Point(5, 5));
        CHECK(zig[2] == Point(1, 0));
    }

    SUBCASE("input is reversed when the last vertex is lexicographically smaller") {
        const Polyline given({Point(4, 0), Point(2, 2), Point(0, 0)});
        CHECK(given[0] == Point(0, 0));
        CHECK(given[2] == Point(4, 0));
    }

    SUBCASE("a polyline equals its reversal") {
        const Polyline forward({Point(0, 0), Point(2, 2), Point(4, 0)});
        const Polyline backward({Point(4, 0), Point(2, 2), Point(0, 0)});
        CHECK(forward == backward);
        CHECK((forward <=> backward) == std::strong_ordering::equal);
    }

    SUBCASE("equal extremes break the tie by the full sequence") {
        const Polyline a({Point(0, 0), Point(1, 3), Point(2, 0), Point(1, 5), Point(0, 0)});
        const Polyline b({Point(0, 0), Point(1, 5), Point(2, 0), Point(1, 3), Point(0, 0)});
        CHECK(a == b);
        CHECK(a[1] == Point(1, 3));  // the lexicographically smaller direction is stored
    }

    SUBCASE("repeated vertices are kept") {
        const Polyline repeat({Point(0, 0), Point(1, 1), Point(1, 1), Point(2, 0)});
        CHECK(repeat.size() == 4);
    }

    SUBCASE("flat coordinate list constructor") {
        const Polyline poly({0, 0, 5, 5, 1, 0});
        REQUIRE(poly.size() == 3);
        CHECK(poly[1] == Point(5, 5));
    }

    SUBCASE("trusted input is stored as given") {
        const std::vector<Point> pts{Point(0, 0), Point(5, 5), Point(1, 0)};
        const Polyline poly(pts, true);
        CHECK(poly == Polyline(pts));
    }

    SUBCASE("class template argument deduction") {
        const std::vector<Point> pts{Point(3, 1), Point(0, 2)};
        const pgl::Polyline fromRange(pts);
        CHECK(fromRange.size() == 2);
        const pgl::Polyline fromCoords({0, 0, 1, 1});
        CHECK(fromCoords.size() == 2);
        static_assert(std::same_as<decltype(fromCoords), const pgl::Polyline<pgl::Point<int>>>);
    }

    SUBCASE("empty polyline") {
        const Polyline none;
        CHECK(none.size() == 0);
        CHECK(none.empty());
        CHECK(none.isDegenerate());
    }
}

TEST_CASE("Polyline vertex access") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    const Polyline poly({0, 0, 5, 5, 1, 0, 6, 2});

    SUBCASE("get reduces the index modulo the vertex count") {
        CHECK(poly.get(0) == poly[0]);
        CHECK(poly.get(4) == poly[0]);
        CHECK(poly.get(-1) == poly[3]);
    }

    SUBCASE("index locates vertices linearly") {
        CHECK(poly.index(poly[2]) == 2);
        CHECK(poly.index(Point(7, 7)) == -1);
    }

    SUBCASE("iteration visits the vertices in order") {
        std::vector<Point> seen(poly.begin(), poly.end());
        REQUIRE(seen.size() == poly.size());
        for (std::size_t i = 0; i < seen.size(); ++i) {
            CHECK(seen[i] == poly[i]);
        }
        CHECK(poly.vertices() == seen);
    }

    SUBCASE("edges connect consecutive vertices without closing") {
        const auto edges = poly.edges();
        REQUIRE(edges.size() == poly.size() - 1);
        CHECK(edges[0] == pgl::Segment(poly[0], poly[1]));
        CHECK(edges[2] == pgl::Segment(poly[2], poly[3]));
        const auto lazy = poly.edgesView();
        CHECK(std::equal(lazy.begin(), lazy.end(), edges.begin(), edges.end()));
        const auto oriented = poly.orientedEdges();
        REQUIRE(oriented.size() == edges.size());
        CHECK(oriented[1].source() == poly[1]);
        CHECK(oriented[1].target() == poly[2]);
    }
}

TEST_CASE("Polyline isDegenerate is all-vertices-equal") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;

    CHECK(Polyline().isDegenerate());
    CHECK(Polyline({Point(1, 2)}).isDegenerate());
    CHECK(Polyline({Point(1, 2), Point(1, 2), Point(1, 2)}).isDegenerate());
    CHECK(!Polyline({Point(1, 2), Point(3, 4)}).isDegenerate());
    CHECK(!Polyline({Point(1, 2), Point(1, 2), Point(3, 4)}).isDegenerate());
}

TEST_CASE("Polyline isSimple") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;

    SUBCASE("simple shapes") {
        CHECK(Polyline().isSimple());
        CHECK(Polyline({Point(1, 1)}).isSimple());
        CHECK(Polyline({0, 0, 2, 2}).isSimple());
        CHECK(Polyline({0, 0, 2, 2, 4, 0}).isSimple());
        CHECK(Polyline({0, 0, 2, 0, 2, 2, 0, 2}).isSimple());  // open square
    }

    SUBCASE("a crossing bow-tie is not simple") {
        CHECK(!Polyline({0, 0, 2, 2, 2, 0, 0, 2}).isSimple());
    }

    SUBCASE("a closed polyline is not simple (first and last edges meet)") {
        CHECK(!Polyline({0, 0, 2, 0, 2, 2, 0, 2, 0, 0}).isSimple());
    }

    SUBCASE("revisiting a vertex is not simple") {
        CHECK(!Polyline({0, 0, 2, 0, 2, 2, 1, 0, 0, 2}).isSimple());  // (1,0) lies on the first edge
    }

    SUBCASE("a collinear back-track spike is not simple") {
        CHECK(!Polyline({0, 0, 4, 0, 2, 0}).isSimple());
    }

    SUBCASE("a repeated consecutive vertex is not simple") {
        CHECK(!Polyline({0, 0, 2, 2, 2, 2, 4, 0}).isSimple());
    }

    SUBCASE("large polylines exercise the Bentley-Ottmann sweep") {
        std::vector<Point> stair;
        for (int i = 0; i <= 6; ++i) {
            stair.push_back(Point(i, i));
            stair.push_back(Point(i + 1, i));
        }
        const Polyline staircase(stair);
        REQUIRE(staircase.size() == 14);  // 13 edges > 8: sweep path
        CHECK(staircase.isSimple());

        auto closed = stair;
        closed.push_back(stair.front());
        CHECK(!Polyline(closed).isSimple());

        auto revisit = stair;
        revisit.push_back(Point(3, 3));  // an existing interior vertex
        CHECK(!Polyline(revisit).isSimple());
    }

    SUBCASE("floating-point polylines use the brute-force test") {
        const pgl::Polyline zig({0.0, 0.0, 2.0, 2.0, 4.0, 0.0});
        CHECK(zig.isSimple());
        const pgl::Polyline bow({0.0, 0.0, 2.0, 2.0, 2.0, 0.0, 0.0, 2.0});
        CHECK(!bow.isSimple());
    }
}

TEST_CASE("Polyline lengths") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    const Polyline poly({0, 0, 3, 4, 3, 8});  // edge lengths 5 and 4

    CHECK(poly.length() == doctest::Approx(9.0));
    CHECK(poly.length<float>() == doctest::Approx(9.0f));
    CHECK(poly.lengthL1() == 11);
    CHECK(poly.lengthLInf() == 8);

    // A self-overlapping polyline counts every traversal.
    const Polyline back({0, 0, 4, 0, 2, 0});
    CHECK(back.lengthL1() == 6);
}

TEST_CASE("Polyline equality, hashing, and translation") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;

    const Polyline base({0, 0, 5, 5, 1, 0});
    Polyline moved = base;
    moved += Point(2, 3);
    CHECK(moved != base);
    CHECK(moved[0] == Point(2, 3));
    moved -= Point(2, 3);
    CHECK(moved == base);

    SUBCASE("a translated copy equals an independently built polyline") {
        Polyline shifted = base;
        shifted += Point(1, 1);
        const Polyline direct({1, 1, 6, 6, 2, 1});
        CHECK(shifted == direct);
        CHECK(std::hash<Polyline>{}(shifted) == std::hash<Polyline>{}(direct));
    }

    SUBCASE("hash is usable in unordered containers") {
        std::unordered_set<Polyline> set;
        set.insert(base);
        set.insert(Polyline({1, 0, 5, 5, 0, 0}));  // reversal of base
        CHECK(set.size() == 1);
        set.insert(Polyline({0, 0, 5, 5}));
        CHECK(set.size() == 2);
    }

    SUBCASE("free translation operators") {
        const auto sum = base + Point(1, 2);
        CHECK(sum[0] == Point(1, 2));
        const auto diff = sum - Point(1, 2);
        CHECK(diff == base);
    }
}

TEST_CASE("Polyline transformations preserve the traversal order") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    const Polyline base({0, 0, 5, 5, 1, 0});

    SUBCASE("four quarter rotations compose to the identity") {
        Polyline rotated = base;
        rotated.rotate90(4);
        CHECK(rotated == base);
        CHECK(base.rotated90().rotated90().rotated90().rotated90() == base);
    }

    SUBCASE("rotation maps vertices rigidly") {
        const Polyline rotated = base.rotated90();
        CHECK(rotated == Polyline({Point(0, 0), Point(-5, 5), Point(0, 1)}));
    }

    SUBCASE("scaling by a negative factor keeps the canonical direction") {
        const auto scaled = base * -2;
        CHECK(scaled == Polyline({Point(0, 0), Point(-10, -10), Point(-2, 0)}));
        CHECK(scaled / -2 == base);
    }

    SUBCASE("axis scalings") {
        CHECK(base.scaledUpX(2) == Polyline({0, 0, 10, 5, 2, 0}));
        CHECK(base.scaledUpY(3) == Polyline({0, 0, 5, 15, 1, 0}));
        CHECK(Polyline({0, 0, 10, 5, 2, 0}).scaledDownX(2) == base);
        Polyline inPlace = base;
        inPlace.scaleUpX(2);
        inPlace.scaleDownX(2);
        CHECK(inPlace == base);
    }

    SUBCASE("affine transformation maps the vertex sequence") {
        const pgl::Transformation<int> reflect(-1, 0, 0, 1, 0, 0);  // mirror in x
        const auto mirrored = reflect * base;
        CHECK(mirrored == Polyline({Point(0, 0), Point(-5, 5), Point(-1, 0)}));
    }
}

TEST_CASE("Polyline bounding box and diameter") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    Polyline poly({0, 0, 5, 5, 1, -2});

    CHECK(poly.bbox() == pgl::Rectangle(Point(0, -2), Point(5, 5)));
    poly += Point(1, 1);
    CHECK(poly.bbox() == pgl::Rectangle(Point(1, -1), Point(6, 6)));
    const auto fbox = poly.fbox();
    CHECK(fbox.min().x() == doctest::Approx(1.0));

    const Polyline segLike({0, 0, 3, 4});
    CHECK(segLike.diameter() == pgl::Segment(Point(0, 0), Point(3, 4)));
}

TEST_CASE("Polyline stream output") {
    using Point = pgl::Point<int>;
    const pgl::Polyline<Point> poly({0, 0, 5, 5, 1, 0});
    std::ostringstream stream;
    stream << poly;
    CHECK(stream.str() == "Polyline[(0,0),(5,5),(1,0)]");
}

TEST_CASE("Polyline type conversion") {
    using Point = pgl::Point<int>;
    const pgl::Polyline<Point> poly({0, 0, 5, 5, 1, 0});
    const pgl::Polyline<pgl::Point<double>> asDouble(poly);
    REQUIRE(asDouble.size() == 3);
    CHECK(asDouble[1] == pgl::Point(5.0, 5.0));
    const pgl::EPolyline exact(poly);
    CHECK(exact.size() == 3);
    CHECK(exact.contains(pgl::EPoint(pgl::ERational(1), pgl::ERational(1))));
}
