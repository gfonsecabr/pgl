#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <compare>
#include <concepts>
#include <sstream>
#include <unordered_set>
#include <variant>
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

// A flip removes one edge and adds one edge, keeping a single open path over
// the same vertices (a Hamiltonian-path move).
TEST_CASE("Polyline flip") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    using Segment = pgl::Segment<Point>;

    // Distinct, non-collinear vertices p0..p4 so every edge is unambiguous.
    const Point p0(0, 0), p1(1, 2), p2(2, 0), p3(3, 2), p4(4, 0);
    const Polyline base({p0, p1, p2, p3, p4});
    const Segment mid(p1, p2);  // interior edge e1, splits into A=[p0,p1], B=[p2,p3,p4]

    SUBCASE("the three interior reconnections") {
        // reverse the suffix B: add (p1, p4)
        CHECK(base.flippable(mid, Segment(p1, p4)));
        CHECK(base.flipped(mid, Segment(p1, p4)) == Polyline({p0, p1, p4, p3, p2}));
        // reverse the prefix A: add (p0, p2)
        CHECK(base.flippable(mid, Segment(p0, p2)));
        CHECK(base.flipped(mid, Segment(p0, p2)) == Polyline({p1, p0, p2, p3, p4}));
        // reverse both: add (p0, p4)
        CHECK(base.flippable(mid, Segment(p0, p4)));
        CHECK(base.flipped(mid, Segment(p0, p4)) == Polyline({p1, p0, p4, p3, p2}));
    }

    SUBCASE("flip is its own kind of undo") {
        // Reversing the suffix twice restores the original path.
        const auto once = base.flipped(mid, Segment(p1, p4));
        CHECK(once.flipped(Segment(p1, p4), mid) == base);
    }

    SUBCASE("non-flippable requests") {
        CHECK_FALSE(base.flippable(mid, mid));                    // re-adding the removed edge
        CHECK_FALSE(base.flippable(mid, Segment(p0, p3)));        // not a valid reconnection
        CHECK_FALSE(base.flippable(Segment(p0, p2), Segment(p1, p4)));  // oldEdge is not an edge
    }

    SUBCASE("in-place flip mutates and matches flipped") {
        Polyline poly = base;
        poly.flip(mid, Segment(p1, p4));
        CHECK(poly == base.flipped(mid, Segment(p1, p4)));
    }

    SUBCASE("a two-vertex polyline has no flip") {
        const Polyline two({p0, p4});
        CHECK_FALSE(two.flippable(Segment(p0, p4), Segment(p0, p4)));
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

namespace {

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
// closed square written as a polyline: first vertex equals the last
static const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});

TEST_CASE("Polyline intersects Polyline") {
    CHECK(zig.intersects(PLine({1, -1, 1, 3, 3, 3})));  // crosses the first edge
    CHECK(zig.intersects(PLine({2, 2, 2, 5})));         // touches the apex
    CHECK(!zig.intersects(PLine({0, 3, 4, 3})));
    CHECK(!zig.intersects(PLine({10, 10, 12, 12})));    // bbox cull path

    SUBCASE("self-intersecting operands") {
        const PLine bow({0, 0, 2, 2, 2, 0, 0, 2});
        CHECK(bow.intersects(zig));
        CHECK(bow.intersects(PLine({1, 1, 1, 4})));
    }

    SUBCASE("degenerate sizes") {
        CHECK(!PLine().intersects(zig));
        CHECK(!zig.intersects(PLine()));
        CHECK(zig.intersects(PLine({Point(1, 1)})));
        CHECK(PLine({Point(1, 1)}).intersects(zig));
        CHECK(!PLine({Point(1, 0)}).intersects(zig));
    }
}

TEST_CASE("Polyline contains Polyline") {
    CHECK(zig.contains(PLine({1, 1, 2, 2, 3, 1})));   // sub-path across the bend
    CHECK(zig.contains(PLine({1, 1, 2, 2, 1, 1})));   // back-tracking sub-path
    CHECK(zig.contains(zig));
    CHECK(!zig.contains(PLine({0, 0, 4, 0})));        // chord
    CHECK(zig.contains(PLine()));
    CHECK(zig.contains(PLine({Point(3, 1)})));

    SUBCASE("reversal equality interplay") {
        const PLine reversed({4, 0, 2, 2, 0, 0});
        CHECK(zig == reversed);
        CHECK(zig.contains(reversed));
    }
}

TEST_CASE("Polyline interiorContains Polyline") {
    CHECK(zig.interiorContains(PLine({1, 1, 2, 2, 3, 1})));
    CHECK(!zig.interiorContains(PLine({0, 0, 2, 2})));  // reaches the extreme
    CHECK(zig.interiorContains(PLine()));

    SUBCASE("boundaryContains only fits point-like polylines") {
        CHECK(zig.boundaryContains(PLine()));
        CHECK(zig.boundaryContains(PLine({Point(0, 0)})));
        CHECK(zig.boundaryContains(PLine({Point(4, 0), Point(4, 0)})));
        CHECK(!zig.boundaryContains(PLine({Point(2, 2)})));
        CHECK(!zig.boundaryContains(PLine({1, 1, 2, 2})));
    }
}

TEST_CASE("Polyline interiorsIntersect Polyline") {
    SUBCASE("crossing interiors") {
        CHECK(zig.interiorsIntersect(PLine({1, -1, 1, 3, 0, 3})));
    }

    SUBCASE("meeting only at both extremes is not interior") {
        const PLine sameEnds({0, 0, 2, -2, 4, 0});  // shares only (0,0) and (4,0) with zig
        CHECK(zig.intersects(sameEnds));
        CHECK(!zig.interiorsIntersect(sameEnds));
    }

    SUBCASE("an extreme of one inside the other counts for the other side only") {
        // (2,2) is zig's interior vertex and probe's extreme: the common point
        // is excluded as probe's extreme, so the interiors do not meet.
        const PLine probe({2, 2, 2, 5, 4, 5});
        CHECK(!zig.interiorsIntersect(probe));
    }

    SUBCASE("collinear overlap is interior") {
        CHECK(zig.interiorsIntersect(PLine({-1, -1, 1, 1, -1, 3})));
    }

    SUBCASE("interior vertex on interior vertex") {
        const PLine cross({2, 0, 2, 2, 2, 4});  // its interior vertex is zig's apex
        CHECK(zig.interiorsIntersect(cross));
    }

    SUBCASE("degenerate sizes have empty interiors") {
        CHECK(!zig.interiorsIntersect(PLine({Point(2, 2)})));
        CHECK(!PLine({Point(2, 2)}).interiorsIntersect(zig));
    }
}

TEST_CASE("Polyline separates and crosses Polyline") {
    SUBCASE("mutual transversal crossing") {
        const PLine a({0, 0, 4, 4});
        const PLine b({0, 4, 4, 0});
        CHECK(a.separates(b));
        CHECK(b.separates(a));
        CHECK(a.crosses(b));
    }

    SUBCASE("touching at a vertex does not separate") {
        const PLine a({0, 0, 2, 2});
        const PLine b({2, 2, 4, 0});
        CHECK(!a.separates(b));
        CHECK(!a.crosses(b));
    }

    SUBCASE("cutting an open chain in the middle separates it") {
        const PLine cut({1, -1, 1, 3});
        CHECK(cut.separates(zig));
        CHECK(zig.separates(cut));
        CHECK(zig.crosses(cut));
    }

    SUBCASE("set semantics: one crossing does not disconnect a closed polyline") {
        const PLine oneCut({1, -1, 1, 1});
        CHECK(!oneCut.separates(loop));
        const PLine twoCuts({1, -1, 1, 3});
        CHECK(twoCuts.separates(loop));
        // The closed loop cuts the crossing chain in one interior point only.
        CHECK(!twoCuts.crosses(oneCut));
    }

    SUBCASE("a self-intersecting remover") {
        const PLine bow({0, 0, 2, 2, 2, 0, 0, 2});  // crosses itself at (1,1)
        const PLine straight({-1, 1, 3, 1});        // passes through the bow twice
        CHECK(bow.separates(straight));
        CHECK(straight.separates(bow));
        CHECK(bow.crosses(straight));
    }

    SUBCASE("a single-vertex polyline acts as a point") {
        const PLine dot({Point(1, 1)});
        CHECK(dot.separates(zig));
        CHECK(!dot.separates(loop));
    }
}

TEST_CASE("Polyline distances to Polyline") {
    CHECK(zig.squaredDistance(PLine({1, -1, 1, 3})) == 0);
    CHECK(zig.squaredDistance<double>(PLine({0, 3, 4, 3})) == doctest::Approx(1.0));
    CHECK(zig.squaredDistance<double>(PLine({6, 0, 7, 0, 7, 3})) == doctest::Approx(4.0));
    CHECK(zig.distanceL1(PLine({6, 0, 7, 0})) == 2);
    CHECK(zig.distanceLInf(PLine({6, 0, 7, 0})) == 2);

    SUBCASE("exact rational distances") {
        const pgl::EPolyline a(zig);
        const pgl::EPolyline b(PLine({1, 3, 3, 3}));
        // The apex (2,2) is one unit below the segment y = 3.
        CHECK(a.squaredDistance<pgl::ERational>(b) == pgl::ERational(1));
    }
}

TEST_CASE("Polyline and Polyline intersection pieces") {
    SUBCASE("touch points covered by an overlap segment are dropped") {
        // bow shares zig's first edge; the bow's other edges touch it at
        // (1,1) and (2,2), both inside the reported overlap.
        const PLine bow({0, 0, 2, 2, 2, 0, 0, 2});
        const auto pieces = zig.intersection(bow);
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(2, 2)));
    }

    SUBCASE("collinear overlaps merge across interleaved pieces") {
        // The first and last edges of `strands` overlap the other's bottom
        // edge but sandwich the unrelated overlap at y = 1 between them in
        // lexicographic order; the merge must still join them.
        const PLine strands({0, 0, 10, 0, 3, 1, 4, 1, 5, 0, 12, 0});
        const PLine other({0, 0, 12, 0, 12, 1, 0, 1});
        const auto pieces = strands.intersection(other);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(12, 0)));
        REQUIRE(std::holds_alternative<Segment>(pieces[1]));
        CHECK(std::get<Segment>(pieces[1]) == Segment(Point(3, 1), Point(4, 1)));
    }

    SUBCASE("two proper crossings yield two points") {
        const auto pieces = zig.intersection(PLine({1, -1, 1, 3, 3, 3, 3, -1}));
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 1));
        REQUIRE(std::holds_alternative<Point>(pieces[1]));
        CHECK(std::get<Point>(pieces[1]) == Point(3, 1));
    }

    SUBCASE("disjoint bounding boxes") {
        CHECK(zig.intersection(PLine({10, 10, 12, 12})).empty());
    }

    SUBCASE("point-sized operands") {
        const auto hit = zig.intersection(PLine({Point(3, 1)}));
        REQUIRE(hit.size() == 1);
        REQUIRE(std::holds_alternative<Point>(hit[0]));
        CHECK(std::get<Point>(hit[0]) == Point(3, 1));
        CHECK(PLine().intersection(zig).empty());
    }
}

}  // namespace
