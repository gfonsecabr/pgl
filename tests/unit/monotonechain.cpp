#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_set>
#include <vector>

// MonotoneChain treats its input as a point set: the constructor sorts the
// vertices lexicographically and removes duplicates, so any permutation of the
// same points yields the same canonical chain.
TEST_CASE("MonotoneChain construction normalizes to the canonical sorted chain") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    SUBCASE("shuffled input is sorted and deduplicated") {
        const Chain chain({Point(2, 1), Point(0, 0), Point(1, 5), Point(1, 2), Point(0, 0)});
        REQUIRE(chain.size() == 4);
        CHECK(chain[0] == Point(0, 0));
        CHECK(chain[1] == Point(1, 2));
        CHECK(chain[2] == Point(1, 5));
        CHECK(chain[3] == Point(2, 1));
    }

    SUBCASE("any permutation yields the same chain") {
        const Chain sorted({Point(0, 0), Point(1, 2), Point(2, 1)});
        const Chain shuffled({Point(2, 1), Point(0, 0), Point(1, 2)});
        CHECK(sorted == shuffled);
        CHECK((sorted <=> shuffled) == std::strong_ordering::equal);
    }

    SUBCASE("flat coordinate list constructor") {
        const Chain chain({0, 0, 5, 5, 10, 0});
        REQUIRE(chain.size() == 3);
        CHECK(chain[1] == Point(5, 5));
    }

    SUBCASE("trusted input is stored as given") {
        const std::vector<Point> pts{Point(0, 0), Point(1, 2), Point(2, 1)};
        const Chain chain(pts, true);
        CHECK(chain == Chain(pts));
    }

    SUBCASE("class template argument deduction") {
        const std::vector<Point> pts{Point(3, 1), Point(0, 2)};
        const pgl::MonotoneChain fromRange(pts);
        CHECK(fromRange.size() == 2);
        const pgl::MonotoneChain fromCoords({0, 0, 1, 1});
        CHECK(fromCoords.size() == 2);
        static_assert(std::same_as<decltype(fromCoords), const pgl::MonotoneChain<pgl::Point<int>>>);
    }

    SUBCASE("empty and single-vertex chains") {
        const Chain none;
        CHECK(none.size() == 0);
        CHECK(none.empty());
        CHECK(none.isDegenerate());
        CHECK(none.edges().empty());

        const Chain one({Point(3, 4)});
        CHECK(one.size() == 1);
        CHECK(one.isDegenerate());
        CHECK(one.edges().empty());
        CHECK(one.isStrictlyMonotone());
    }

    SUBCASE("converting constructor changes the coordinate type") {
        const Chain chain({0, 0, 1, 2});
        const pgl::MonotoneChain<pgl::Point<double>> converted(chain);
        CHECK(converted[1] == pgl::Point<double>(1.0, 2.0));
    }
}

TEST_CASE("MonotoneChain structure queries") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain vertical({0, 0, 1, 2, 1, 5, 2, 1});
    const Chain strict({0, 0, 5, 5, 10, 0});

    SUBCASE("isStrictlyMonotone detects vertical edges") {
        CHECK(!vertical.isStrictlyMonotone());
        CHECK(strict.isStrictlyMonotone());
        CHECK(!vertical.isDegenerate());
    }

    SUBCASE("edges are the n-1 consecutive segments, no closing edge") {
        const auto edges = strict.edges();
        REQUIRE(edges.size() == 2);
        CHECK(edges[0] == pgl::Segment<Point>(Point(0, 0), Point(5, 5)));
        CHECK(edges[1] == pgl::Segment<Point>(Point(5, 5), Point(10, 0)));

        const auto oriented = strict.orientedEdges();
        REQUIRE(oriented.size() == 2);
        CHECK(oriented[1].source() == Point(5, 5));
        CHECK(oriented[1].target() == Point(10, 0));
    }

    SUBCASE("edge iterators match edges()") {
        std::vector<pgl::Segment<Point>> collected;
        for (auto it = vertical.edgesBegin(); it != vertical.edgesEnd(); ++it) {
            collected.push_back(*it);
        }
        CHECK(collected == vertical.edges());
        CHECK(std::distance(strict.edgesBegin(), strict.edgesEnd()) == 2);
    }

    SUBCASE("index locates vertices by binary search") {
        CHECK(vertical.index(Point(1, 5)) == 2);
        CHECK(vertical.index(Point(1, 3)) == -1);
        CHECK(vertical.index(Point(7, 7)) == -1);
    }

    SUBCASE("vertices returns the translated canonical sequence") {
        const auto chain = strict + Point(1, 1);
        const auto verts = chain.vertices();
        REQUIRE(verts.size() == 3);
        CHECK(verts[0] == Point(1, 1));
        CHECK(verts[2] == Point(11, 1));
    }

    SUBCASE("diameter is the farthest vertex pair") {
        CHECK(strict.diameter() == pgl::Segment<Point>(Point(0, 0), Point(10, 0)));
    }

    SUBCASE("bbox and fbox") {
        const Chain chain({0, 0, 1, 5, 2, -3});
        CHECK(chain.bbox() == pgl::Rectangle<Point>(Point(0, -3), Point(2, 5)));
        const auto f = chain.fbox();
        CHECK(f.min() == pgl::Point<double>(0.0, -3.0));

        const Chain none;
        CHECK(none.bbox() == pgl::Rectangle<Point>());
    }

    SUBCASE("lengths") {
        const Chain hypot({0, 0, 3, 4});
        CHECK(hypot.length() == doctest::Approx(5.0));
        CHECK(hypot.lengthL1() == 7);
        CHECK(hypot.lengthLInf() == 4);

        const Chain none;
        CHECK(none.length() == doctest::Approx(0.0));
    }

    SUBCASE("stream format") {
        std::ostringstream out;
        out << Chain({0, 0, 1, 2});
        CHECK(out.str() == "MonotoneChain[(0,0),(1,2)]");
    }
}

// indexAtX returns the smallest index i with vertex x equal to the query, or
// the edge whose open x-range contains the query; at a vertical run of
// vertices this is the bottom vertex.
TEST_CASE("MonotoneChain indexAtX and yAtX") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Rational = pgl::Rational<int>;

    // Vertices: (0,0), (2,4), (4,0), (4,2), (4,6), (6,6)
    const Chain chain({0, 0, 2, 4, 4, 0, 4, 2, 4, 6, 6, 6});

    SUBCASE("outside the x-extent") {
        CHECK(!chain.indexAtX(-1).has_value());
        CHECK(!chain.indexAtX(7).has_value());
        CHECK(!chain.yAtX(-1).has_value());
        const Chain none;
        CHECK(!none.indexAtX(0).has_value());
    }

    SUBCASE("exactly at a vertex") {
        CHECK(chain.indexAtX(0) == 0);
        CHECK(chain.indexAtX(2) == 1);
        CHECK(chain.indexAtX(6) == 5);
        CHECK(chain.yAtX(2) == 4);
        CHECK(chain.yAtX(6) == 6);
    }

    SUBCASE("at a vertical run the bottom vertex wins") {
        CHECK(chain.indexAtX(4) == 2);
        CHECK(chain.yAtX(4) == 0);
    }

    SUBCASE("strictly inside an edge") {
        CHECK(chain.indexAtX(1) == 0);
        CHECK(chain.indexAtX(5) == 4);
        CHECK(chain.yAtX(1) == 2);       // edge (0,0)-(2,4) at x=1
        CHECK(chain.yAtX(5) == 6);       // edge (4,6)-(6,6) at x=5
    }

    SUBCASE("exact rational interpolation") {
        const Chain simple({0, 0, 2, 1});
        CHECK(simple.yAtX<Rational>(1) == Rational(1, 2));
        // Truncating integer division is the documented behavior for an
        // integer ResultNumber.
        CHECK(simple.yAtX(1) == 0);
    }
}

// isBelow/isAbove shoot a vertical ray down/up from the query point; both are
// engaged when the point is on the chain, and both empty outside the x-extent.
TEST_CASE("MonotoneChain isBelow and isAbove") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // Vertices: (0,0), (2,4), (4,0), (4,2), (4,6), (6,6)
    const Chain chain({0, 0, 2, 4, 4, 0, 4, 2, 4, 6, 6, 6});

    SUBCASE("outside the x-extent both are empty") {
        CHECK(!chain.isBelow(Point(-1, 0)).has_value());
        CHECK(!chain.isAbove(Point(7, 0)).has_value());
    }

    SUBCASE("strictly above the chain") {
        CHECK(chain.isBelow(Point(1, 3)) == 0);
        CHECK(!chain.isAbove(Point(1, 3)).has_value());
    }

    SUBCASE("strictly below the chain") {
        CHECK(!chain.isBelow(Point(1, 1)).has_value());
        CHECK(chain.isAbove(Point(1, 1)) == 0);
    }

    SUBCASE("on the chain both are engaged") {
        CHECK(chain.isBelow(Point(1, 2)) == 0);
        CHECK(chain.isAbove(Point(1, 2)) == 0);
        CHECK(chain.isBelow(Point(2, 4)) == 1);
        CHECK(chain.isAbove(Point(2, 4)) == 1);
    }

    SUBCASE("at a vertical run the chain spans [bottom, top]") {
        // Chain covers x = 4 for y in [0, 6].
        CHECK(chain.isBelow(Point(4, 7)) == 2);
        CHECK(!chain.isAbove(Point(4, 7)).has_value());
        CHECK(!chain.isBelow(Point(4, -1)).has_value());
        CHECK(chain.isAbove(Point(4, -1)) == 2);
        CHECK(chain.isBelow(Point(4, 3)) == 2);
        CHECK(chain.isAbove(Point(4, 3)) == 2);
    }

    SUBCASE("at the extreme vertices") {
        CHECK(chain.isBelow(Point(0, 0)) == 0);
        CHECK(chain.isAbove(Point(0, 0)) == 0);
        CHECK(chain.isBelow(Point(6, 10)) == 5);
        CHECK(chain.isAbove(Point(6, 10)).has_value() == false);
    }
}

TEST_CASE("MonotoneChain insert") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    SUBCASE("single insert keeps the chain sorted") {
        Chain chain({0, 0, 10, 0});
        chain.insert(Point(5, 3));
        REQUIRE(chain.size() == 3);
        CHECK(chain[1] == Point(5, 3));
        CHECK(chain.yAtX(5) == 3);

        chain.insert(Point(-2, 1));
        CHECK(chain[0] == Point(-2, 1));
        chain.insert(Point(12, 1));
        CHECK(chain[chain.size() - 1] == Point(12, 1));
    }

    SUBCASE("inserting an existing vertex is a no-op") {
        Chain chain({0, 0, 10, 0});
        chain.insert(Point(10, 0));
        CHECK(chain.size() == 2);
    }

    SUBCASE("insert invalidates the cached bounding box") {
        Chain chain({0, 0, 10, 0});
        CHECK(chain.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(10, 0)));
        chain.insert(Point(5, 7));
        CHECK(chain.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(10, 7)));
    }

    SUBCASE("bulk insert merges and deduplicates") {
        Chain chain({0, 0, 10, 0});
        chain.insert(std::vector<Point>{Point(5, 3), Point(0, 0), Point(2, 1), Point(5, 3)});
        REQUIRE(chain.size() == 4);
        CHECK(chain[1] == Point(2, 1));
        CHECK(chain[2] == Point(5, 3));
    }

    SUBCASE("insert respects an active translation") {
        Chain chain = Chain({0, 0, 10, 0}) + Point(100, 100);
        chain.insert(Point(105, 103));
        REQUIRE(chain.size() == 3);
        CHECK(chain[1] == Point(105, 103));
        CHECK(chain.yAtX(105) == 103);
    }
}

TEST_CASE("MonotoneChain equality, ordering, and hashing") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain chain({0, 0, 1, 2, 3, 1});

    SUBCASE("translation-consistent equality") {
        const auto moved = chain + Point(5, 5);
        CHECK(moved == Chain({5, 5, 6, 7, 8, 6}));
        CHECK(moved != chain);
        CHECK((chain - Point(0, 0)) == chain);
    }

    SUBCASE("lexicographic ordering, shorter first") {
        CHECK(Chain({0, 0}) < Chain({0, 0, 1, 1}));
        CHECK(Chain({0, 0, 1, 1}) < Chain({0, 0, 1, 2}));
    }

    SUBCASE("equal chains hash equally, and the memo survives translation") {
        const std::hash<Chain> hasher;
        const auto moved = (chain + Point(1, 1)) - Point(1, 1);
        CHECK(hasher(moved) == hasher(chain));

        std::unordered_set<Chain> set;
        set.insert(chain);
        set.insert(moved);
        CHECK(set.size() == 1);
        set.insert(chain + Point(1, 0));
        CHECK(set.size() == 2);
    }
}

TEST_CASE("MonotoneChain transformations renormalize when needed") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain chain({0, 0, 1, 2, 3, 1});

    SUBCASE("translation is exact and O(1)") {
        const auto moved = chain + Point(7, -3);
        CHECK(moved[0] == Point(7, -3));
        CHECK(moved.bbox() == pgl::Rectangle<Point>(Point(7, -3), Point(10, -1)));
    }

    SUBCASE("an odd 90-degree rotation re-sorts the point set") {
        // rotated90 maps (x, y) -> (-y, x); sorted, the smallest vertex of the
        // result is the rotation of the highest-y input vertex.
        const auto rotated = chain.rotated90();
        CHECK(rotated == Chain({-2, 1, -1, 3, 0, 0}));
        CHECK(rotated.rotated90(-1) == chain);
        CHECK(chain.rotated90(4) == chain);
    }

    SUBCASE("negative and zero scaling renormalize") {
        const auto negated = chain * -1;
        CHECK(negated == Chain({0, 0, -1, -2, -3, -1}));
        CHECK(negated[0] == Point(-3, -1));

        const auto collapsed = chain * 0;
        CHECK(collapsed.size() == 1);
        CHECK(collapsed.isDegenerate());
    }

    SUBCASE("axis scaling") {
        CHECK(chain.scaledUpX(2) == Chain({0, 0, 2, 2, 6, 1}));
        CHECK(chain.scaledUpY(-1) == Chain({0, 0, 1, -2, 3, -1}));
        CHECK(Chain({0, 0, 2, 4}).scaledDownY(2) == Chain({0, 0, 2, 2}));
        // A negative x-scale reverses the order; the result must be canonical.
        const auto mirrored = chain.scaledUpX(-1);
        CHECK(mirrored[0] == Point(-3, 1));
    }

    SUBCASE("affine transformation re-sorts the mapped point set") {
        const auto rotated = pgl::Transformation<int>::rotation90() * chain;
        CHECK(rotated == chain.rotated90());
        const auto translated = pgl::Transformation<int>::translation(2, 3) * chain;
        CHECK(translated == chain + Point(2, 3));
    }
}

TEST_CASE("MonotoneChain works with other numeric types") {
    SUBCASE("double coordinates") {
        using Point = pgl::Point<double>;
        const pgl::MonotoneChain<Point> chain({Point(0.5, 0.0), Point(0.0, 1.5)});
        CHECK(chain[0] == Point(0.0, 1.5));
        CHECK(chain.yAtX(0.25) == 0.75);
    }

    SUBCASE("rational coordinates") {
        using Rational = pgl::Rational<int>;
        using Point = pgl::Point<Rational>;
        const pgl::MonotoneChain<Point> chain(
            {Point(Rational(0), Rational(0)), Point(Rational(1), Rational(1, 3))});
        CHECK(chain.yAtX(Rational(1, 2)) == Rational(1, 6));
    }

    SUBCASE("exact aliases (Rational<BigInt>)") {
        const pgl::MonotoneChain<pgl::Point<int>> source({0, 0, 2, 1});
        const pgl::EMonotoneChain chain(source);
        CHECK(chain.size() == 2);
        CHECK(chain.yAtX(pgl::ERational(1)) == pgl::ERational(1, 2));
        CHECK(!chain.isDegenerate());
    }
}

// The chain is not cyclic, but get() still reduces the index modulo size() to
// satisfy the common vertex access interface of Shape.
TEST_CASE("MonotoneChain get reduces the index modulo the vertex count") {
    using Point = pgl::Point<int>;
    const pgl::MonotoneChain<Point> chain({0, 0, 2, 4, 5, 1});
    CHECK(chain.get(0) == Point(0, 0));
    CHECK(chain.get(2) == Point(5, 1));
    CHECK(chain.get(3) == Point(0, 0));
    CHECK(chain.get(-1) == Point(5, 1));
    CHECK(chain.get(-4) == Point(5, 1));
}
