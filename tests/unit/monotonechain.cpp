#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <functional>
#include <random>
#include <sstream>
#include <unordered_set>
#include <variant>
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

TEST_CASE("MonotoneChain asPolyline traverses the canonical vertices") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain chain({4, 4, 0, 0, 8, 0});   // sorted to (0,0), (4,4), (8,0)
    const auto poly = chain.asPolyline();
    const auto verts = chain.vertices();
    REQUIRE(poly.size() == verts.size());
    for (std::size_t i = 0; i < verts.size(); ++i) {
        CHECK(poly[i] == verts[i]);
    }
}

namespace {

using Point = pgl::Point<int>;
using Chain = pgl::MonotoneChain<Point>;
using Rational = pgl::Rational<int>;
using RationalPoint = pgl::Point<Rational>;
using Piece = std::variant<RationalPoint, pgl::Segment<RationalPoint>>;

// Reference chain: up to a peak at (2,4), down to (4,0), up the vertical edge
// to (4,4), and down to (6,0).
const Chain zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});

bool pieceCovers(const Piece& covering, const Piece& covered) {
    return std::visit(
        [](const auto& outer, const auto& inner) {
            if constexpr (pgl::detail::is_point_v<std::remove_cvref_t<decltype(outer)>>) {
                if constexpr (pgl::detail::is_point_v<std::remove_cvref_t<decltype(inner)>>) {
                    return outer == inner;
                } else {
                    return inner.isDegenerate() && outer == inner.min();
                }
            } else {
                return outer.contains(inner);
            }
        },
        covering, covered);
}

}  // namespace

TEST_CASE("MonotoneChain vs MonotoneChain intersects via the merge sweep") {
    SUBCASE("crossing chains") {
        const Chain horizontal({0, 3, 6, 3});
        CHECK(zigzag.intersects(horizontal));
        CHECK(horizontal.intersects(zigzag));
    }

    SUBCASE("disjoint x-extents take the early exit") {
        const Chain farRight({7, 0, 9, 9});
        CHECK_FALSE(zigzag.intersects(farRight));
        CHECK_FALSE(farRight.intersects(zigzag));
    }

    SUBCASE("overlapping x-extents but no common point") {
        const Chain above({0, 5, 6, 6});
        CHECK_FALSE(zigzag.intersects(above));
        const Chain below({-2, -1, 8, -1});
        CHECK_FALSE(zigzag.intersects(below));
    }

    SUBCASE("touching at a single vertex") {
        const Chain fromPeak({2, 4, 5, 9});
        CHECK(zigzag.intersects(fromPeak));
        const Chain fromEnd({6, 0, 8, 2});
        CHECK(zigzag.intersects(fromEnd));
        CHECK(fromEnd.intersects(zigzag));
    }

    SUBCASE("vertical edges at the same x") {
        const Chain belowValley({4, -2, 4, -1});   // vertical, below the chain's vertical edge
        CHECK_FALSE(zigzag.intersects(belowValley));
        const Chain overlapping({4, 3, 4, 6, 5, 7});   // vertical overlap on [3,4]
        CHECK(zigzag.intersects(overlapping));
        const Chain stacked({4, 4, 4, 6});   // touches exactly at the top vertex (4,4)
        CHECK(zigzag.intersects(stacked));
    }

    SUBCASE("mixed numeric types") {
        const pgl::MonotoneChain<pgl::Point<long>> wide({0L, 2L, 6L, 2L});
        CHECK(zigzag.intersects(wide));
    }
}

TEST_CASE("MonotoneChain vs MonotoneChain intersection pieces") {
    SUBCASE("single crossing at a lattice point") {
        const Chain up({0, 0, 4, 4});
        const Chain down({0, 4, 4, 0});
        const auto pieces = up.intersection(down);
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(2, 2));
    }

    SUBCASE("crossing at a non-lattice point requires a rational result") {
        const Chain up({0, 0, 3, 3});
        const Chain down({0, 3, 3, 0});
        const auto pieces = up.intersection<Rational>(down);
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<RationalPoint>(pieces[0]));
        CHECK(std::get<RationalPoint>(pieces[0]) == RationalPoint(Rational(3, 2), Rational(3, 2)));
    }

    SUBCASE("touch at a vertex shared by four edges is reported once") {
        const Chain valley({0, 4, 2, 0, 4, 4});
        const Chain peak({0, -4, 2, 0, 4, -4});
        const auto pieces = valley.intersection(peak);
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(2, 0));
    }

    SUBCASE("collinear overlap split by vertices coalesces into one segment") {
        const Chain straight({0, 0, 4, 4});
        const Chain overlapping({1, 1, 2, 2, 3, 3, 6, 3});
        const auto pieces = straight.intersection(overlapping);
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[0]));
        CHECK(std::get<pgl::Segment<Point>>(pieces[0]) == pgl::Segment<Point>({1, 1}, {3, 3}));
    }

    SUBCASE("overlap across a bend stays two segments") {
        const Chain tent({0, 0, 2, 2, 4, 0});
        const Chain subPath({1, 1, 2, 2, 3, 1});
        const auto pieces = tent.intersection(subPath);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[0]));
        REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[1]));
        CHECK(std::get<pgl::Segment<Point>>(pieces[0]) == pgl::Segment<Point>({1, 1}, {2, 2}));
        CHECK(std::get<pgl::Segment<Point>>(pieces[1]) == pgl::Segment<Point>({2, 2}, {3, 1}));
    }

    SUBCASE("multiple crossings arrive sorted, including through the vertical edge") {
        const Chain horizontal({0, 2, 6, 2});
        const auto pieces = zigzag.intersection(horizontal);
        REQUIRE(pieces.size() == 4);
        const std::vector<Point> expected{Point(1, 2), Point(3, 2), Point(4, 2), Point(5, 2)};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(std::holds_alternative<Point>(pieces[i]));
            CHECK(std::get<Point>(pieces[i]) == expected[i]);
        }
    }

    SUBCASE("disjoint chains produce no pieces") {
        const Chain above({0, 5, 6, 6});
        CHECK(zigzag.intersection(above).empty());
        CHECK(zigzag.intersection(Chain({7, 0, 9, 9})).empty());
    }
}

TEST_CASE("MonotoneChain vs MonotoneChain separates and crosses") {
    SUBCASE("a proper crossing cuts each chain in two") {
        const Chain up({0, 0, 2, 2});
        const Chain down({0, 2, 2, 0});
        CHECK(up.separates(down));
        CHECK(down.separates(up));
        CHECK(up.crosses(down));
    }

    SUBCASE("covering the middle of a chain separates it; an end run does not") {
        const Chain base({0, 0, 10, 0});
        CHECK(Chain({2, 0, 4, 0}).separates(base));          // interior overlap
        CHECK_FALSE(Chain({-2, 0, 4, 0}).separates(base));   // reaches the left end
        CHECK_FALSE(Chain({4, 0, 12, 0}).separates(base));   // reaches the right end
        CHECK_FALSE(Chain({-2, 0, 12, 0}).separates(base));  // swallows the whole chain
        CHECK_FALSE(base.separates(base));                   // equal chains
    }

    SUBCASE("touching an interior vertex cuts the arc, an extreme vertex does not") {
        // The other chain bends at (2,2); a transversal touch there separates.
        const Chain tent({0, 0, 2, 2, 4, 0});
        CHECK(Chain({1, 3, 2, 2, 3, 3}).separates(tent));
        // Touch at the extreme vertex (0,0) removes only an endpoint.
        CHECK_FALSE(Chain({-1, 1, 0, 0, 1, -1}).separates(tent));
    }

    SUBCASE("two covered runs pinned at both ends leave a single connected gap") {
        const Chain base({0, 0, 10, 0});
        // Runs along the base at both ends, detouring off in the middle: prefix
        // and suffix are covered, one connected gap survives -> no separation.
        CHECK_FALSE(Chain({0, 0, 4, 0, 5, 3, 6, 0, 10, 0}).separates(base));
        // Shift the left run inward so it is interior, not a prefix -> separates.
        CHECK(Chain({2, 0, 4, 0, 5, 3, 6, 0, 10, 0}).separates(base));
    }

    SUBCASE("a covered stretch strictly inside one edge separates that edge") {
        const Chain base({0, 0, 4, 0, 10, 0});
        CHECK(Chain({5, 0, 6, 0}).separates(base));          // interior of the last edge
        CHECK_FALSE(Chain({5, 0, 10, 0}).separates(base));   // reaches the end vertex
    }

    SUBCASE("crosses is symmetric only for mutual cuts") {
        // Asymmetric: B's endpoint sits in A's interior, but A never crosses B.
        const Chain a({0, 0, 2, 2});
        const Chain b({-1, 1, 0, 0, 1, -1});
        CHECK(a.separates(b));
        CHECK_FALSE(b.separates(a));
        CHECK_FALSE(a.crosses(b));
    }

    SUBCASE("disjoint bounding boxes take the early exit") {
        CHECK_FALSE(zigzag.separates(Chain({7, 0, 9, 9})));
        CHECK_FALSE(Chain({7, 0, 9, 9}).separates(zigzag));
    }
}

TEST_CASE("MonotoneChain vs MonotoneChain edgesCross") {
    SUBCASE("a proper crossing strongly crosses") {
        const Chain up({0, 0, 2, 2});
        const Chain down({0, 2, 2, 0});
        CHECK(up.edgesCross(down));
        CHECK(down.edgesCross(up));
    }

    SUBCASE("crossing through a vertical edge") {
        const Chain horizontal({0, 3, 6, 3});
        CHECK(zigzag.edgesCross(horizontal));
        CHECK(horizontal.edgesCross(zigzag));
    }

    SUBCASE("touching at a single vertex without swapping sides is not a strong cross") {
        const Chain fromPeak({2, 4, 5, 9});
        CHECK_FALSE(zigzag.edgesCross(fromPeak));
        CHECK_FALSE(fromPeak.edgesCross(zigzag));
    }

    SUBCASE("collinear overlap that never returns is not a strong cross") {
        const Chain straight({0, 0, 4, 4});
        const Chain overlapping({1, 1, 2, 2, 3, 3, 6, 3});
        CHECK_FALSE(straight.edgesCross(overlapping));
        CHECK_FALSE(overlapping.edgesCross(straight));
    }

    SUBCASE("a chain that stays entirely to one side never strongly crosses") {
        const Chain base({0, 0, 10, 0});
        const Chain above({0, 5, 10, 6});
        CHECK_FALSE(base.edgesCross(above));
        CHECK_FALSE(above.edgesCross(base));
    }

    SUBCASE("a tangent dip that touches the base at both ends but stays on one side") {
        const Chain base({0, 0, 10, 0});
        const Chain dip({2, 0, 4, -2, 6, 0});
        CHECK_FALSE(base.edgesCross(dip));
        CHECK_FALSE(dip.edgesCross(base));
    }

    SUBCASE("disjoint x-extents never strongly cross") {
        CHECK_FALSE(zigzag.edgesCross(Chain({7, 0, 9, 9})));
        CHECK_FALSE(Chain({7, 0, 9, 9}).edgesCross(zigzag));
    }

    SUBCASE("a single-vertex chain never strongly crosses anything") {
        const Chain point({3, 100});
        CHECK_FALSE(zigzag.edgesCross(point));
        CHECK_FALSE(point.edgesCross(zigzag));
    }

    SUBCASE("a chain that is a single vertical edge never strongly crosses, even if it "
            "brackets the other chain's value at their shared x") {
        // vertical spans y in [0,2] at x=0; horizontal sits at y=1, also
        // starting at x=0. Naively vertical[0]=(0,0) is strictly below
        // horizontal and vertical[1]=(0,2) is strictly above it, but that
        // "crossing" lives entirely at the single x=0 the two chains share,
        // which is both chains' own boundary vertex -- not robust.
        const Chain vertical({0, 0, 0, 2});
        const Chain horizontal({0, 1, 1, 1});
        CHECK_FALSE(vertical.edgesCross(horizontal));
        CHECK_FALSE(horizontal.edgesCross(vertical));
    }

    SUBCASE("a leading vertical edge that only brackets the other chain at the shared "
            "boundary is not a proper crossing") {
        const Chain verticalThenRising({0, 0, 0, 2, 3, 5});
        const Chain horizontal({0, 1, 1, 1});
        // The vertical edge x=0 spans y in [0,2] and the horizontal starts at
        // (0,1) on it, so the chains meet only at the horizontal's endpoint and
        // the rising part stays strictly above: a tangential touch, not a
        // transversal crossing, so no edge pair *properly* crosses.
        CHECK_FALSE(verticalThenRising.edgesCross(horizontal));
        CHECK_FALSE(horizontal.edgesCross(verticalThenRising));
    }

    SUBCASE("a chain with several sign changes strongly crosses") {
        const Chain sawtooth({0, -1, 1, 5, 2, -1, 3, 5, 4, 2, 5, 5, 6, -1});
        CHECK(zigzag.edgesCross(sawtooth));
        CHECK(sawtooth.edgesCross(zigzag));
    }

    SUBCASE("mixed numeric types") {
        const pgl::MonotoneChain<pgl::Point<long>> wide({0L, 3L, 6L, 3L});
        CHECK(zigzag.edgesCross(wide));
        CHECK(wide.edgesCross(zigzag));
    }
}

TEST_CASE("MonotoneChain separates agrees with an intersection-based oracle") {
    // Oracle: A separates B iff, walking B in arc (lexicographic) order, some
    // connected covered component avoids both extreme vertices of B. The
    // covered components come from B.intersection(A), an independent code path
    // from the edge-scan state machine that separates() uses.
    std::mt19937 rng(20260707);
    std::uniform_int_distribution<int> coord(0, 6);
    std::uniform_int_distribution<std::size_t> count(2, 6);
    const auto randomChain = [&]() {
        std::vector<Point> points;
        const std::size_t n = count(rng);
        for (std::size_t i = 0; i < n; ++i) {
            points.emplace_back(coord(rng), coord(rng));
        }
        return Chain(points);
    };
    const auto pieceMin = [](const Piece& piece) {
        return std::visit([](const auto& v) -> RationalPoint {
            if constexpr (pgl::detail::is_point_v<std::remove_cvref_t<decltype(v)>>) return v;
            else return v.min();
        }, piece);
    };
    const auto pieceMax = [](const Piece& piece) {
        return std::visit([](const auto& v) -> RationalPoint {
            if constexpr (pgl::detail::is_point_v<std::remove_cvref_t<decltype(v)>>) return v;
            else return v.max();
        }, piece);
    };
    const auto within = [](const RationalPoint& p, const RationalPoint& lo, const RationalPoint& hi) {
        return !(p < lo) && !(hi < p);
    };

    const auto oracle = [&](const Chain& remover, const Chain& target) {
        if (target.size() < 2) return false;
        const auto pieces = target.intersection<Rational>(remover);
        if (pieces.empty()) return false;
        const RationalPoint first = RationalPoint(target[0]);
        const RationalPoint last = RationalPoint(target[target.size() - 1]);
        std::vector<std::pair<RationalPoint, RationalPoint>> runs;
        for (const auto& piece : pieces) {
            const RationalPoint lo = pieceMin(piece), hi = pieceMax(piece);
            if (!runs.empty() && !(runs.back().second < lo)) {
                if (runs.back().second < hi) runs.back().second = hi;
            } else {
                runs.push_back({lo, hi});
            }
        }
        for (const auto& run : runs) {
            if (!within(first, run.first, run.second) && !within(last, run.first, run.second)) {
                return true;
            }
        }
        return false;
    };

    for (int round = 0; round < 2000; ++round) {
        const Chain a = randomChain();
        const Chain b = randomChain();
        CAPTURE(a);
        CAPTURE(b);
        CHECK(a.separates(b) == oracle(a, b));
        CHECK(b.separates(a) == oracle(b, a));
        CHECK(a.crosses(b) == (oracle(a, b) && oracle(b, a)));
    }
}

TEST_CASE("MonotoneChain merge sweep agrees with the brute-force edge-pair scan") {
    std::mt19937 rng(20260706);
    std::uniform_int_distribution<int> coord(0, 8);
    std::uniform_int_distribution<std::size_t> count(2, 7);

    const auto randomChain = [&]() {
        std::vector<Point> points;
        const std::size_t n = count(rng);
        for (std::size_t i = 0; i < n; ++i) {
            points.emplace_back(coord(rng), coord(rng));
        }
        return Chain(points);
    };

    for (int round = 0; round < 300; ++round) {
        const Chain a = randomChain();
        const Chain b = randomChain();
        CAPTURE(a);
        CAPTURE(b);

        // Brute force: test every edge pair and collect every piece.
        bool bruteIntersects = false;
        std::vector<Piece> brutePieces;
        for (auto ea = a.edgesBegin(); ea != a.edgesEnd(); ++ea) {
            for (auto eb = b.edgesBegin(); eb != b.edgesEnd(); ++eb) {
                if ((*ea).intersects(*eb)) {
                    bruteIntersects = true;
                }
                if (const auto piece = (*ea).template intersection<Rational>(*eb)) {
                    brutePieces.push_back(*piece);
                }
            }
        }

        CHECK(a.intersects(b) == bruteIntersects);
        CHECK(b.intersects(a) == bruteIntersects);

        const auto pieces = a.intersection<Rational>(b);
        CHECK(pieces.empty() == !bruteIntersects);

        // Soundness: every reported piece lies on both chains.
        for (const Piece& piece : pieces) {
            std::visit(
                [&](const auto& value) {
                    CHECK_MESSAGE(a.contains(value), a, " misses piece ", value);
                    CHECK_MESSAGE(b.contains(value), b, " misses piece ", value);
                },
                piece);
        }

        // Completeness: every brute-force piece is covered by a reported one.
        for (const Piece& expected : brutePieces) {
            bool covered = false;
            for (const Piece& piece : pieces) {
                if (pieceCovers(piece, expected)) {
                    covered = true;
                    break;
                }
            }
            std::visit([&](const auto& value) {
                CHECK_MESSAGE(covered, "piece ", value, " not covered; chains ", a, " and ", b);
            }, expected);
        }

        // The pieces arrive sorted by their lexicographically smallest point.
        for (std::size_t i = 1; i < pieces.size(); ++i) {
            const auto pieceMin = [](const Piece& piece) {
                return std::visit(
                    [](const auto& value) -> RationalPoint {
                        if constexpr (pgl::detail::is_point_v<std::remove_cvref_t<decltype(value)>>) {
                            return value;
                        } else {
                            return value.min();
                        }
                    },
                    piece);
            };
            CHECK_FALSE(pieceMin(pieces[i]) < pieceMin(pieces[i - 1]));
        }
    }
}
