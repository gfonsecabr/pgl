#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <random>
#include <variant>
#include <vector>

#include "pgl.hpp"

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
