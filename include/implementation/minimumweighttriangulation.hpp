#pragma once

// Defines Polygon::minimumWeightTriangulation out of line. It needs the
// visibility expansion of implementation/visibilitygraph.hpp, which its
// predecessor in pgl.hpp includes.
#include "implementation/optimalconvexpartition.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @file minimumweighttriangulation.hpp
 * @brief Minimum weight triangulation of a simple polygon.
 *
 * The dynamic program of Gilbert (1979) and Klincsek (1980), run over the clear
 * diagonals only. Vertices are numbered `0..n-1` along the ring. For `i < j`
 * with `ij` a side or a clear diagonal, `P(i,j)` is the subpolygon
 * `i, i+1, ..., j` and `C(i,j)` the least total weight of the diagonals
 * strictly inside it. The triangle of a triangulation of `P(i,j)` standing on
 * `ij` has its apex `k` strictly between `i` and `j`, and `ik` and `kj` are
 * sides or clear diagonals. Conversely any such `k` gives a triangle inside
 * `P(i,j)`, since all three of its sides are, so
 *
 *     C(i,j) = min over k of E(i,k) + E(k,j),
 *
 * where `E(u,w)` is `C(u,w)` plus the weight of the diagonal `uw`, and zero
 * for a side. The whole polygon is `P(0,n-1)`, closed by the side `(n-1,0)`.
 *
 * The candidates `k` are the common neighbours of `i` and `j` between them,
 * found by merging two sorted lists, so a link costs the degrees of its ends
 * and the program the sum of the squared degrees. Nothing but sums and
 * comparisons touches a weight, which is why any weight function will do.
 *
 * A strictly convex polygon, where every candidate is valid and the program is
 * cubic, takes a dense table instead: the inner loop then reads two contiguous
 * rows and keeps only the minimum, and the apexes are recovered afterwards for
 * the links the answer uses.
 */

namespace pgl {

namespace detail {

// Friend of Triangulation: the clear visibility of a polygon's ring.
struct MinimumWeightTriangulationBuilder {
    // Every ring vertex each ring vertex sees through a clear diagonal, by
    // ring position. `tri` triangulates the polygon whose ring is `ring`.
    template <class Tri, class PointType>
    static std::vector<std::vector<int>> ringVisibility(const Tri& tri,
                                                        const std::vector<PointType>& ring) {
        std::unordered_map<PointType, int> position;
        position.reserve(ring.size());
        for (std::size_t v = 0; v < ring.size(); ++v) {
            position.emplace(ring[v], static_cast<int>(v));
        }
        std::vector<int> ringOf(tri.vertices_.size(), -1);
        for (std::size_t v = Tri::GHOST + 1; v < tri.vertices_.size(); ++v) {
            const auto it = position.find(tri.vertices_[v]);
            if (it != position.end()) {
                ringOf[v] = it->second;
            }
        }
        const auto adjacency = tri.clearVisibleAdjacency();
        std::vector<std::vector<int>> result(ring.size());
        for (std::size_t v = Tri::GHOST + 1; v < adjacency.size(); ++v) {
            const int from = ringOf[v];
            if (from < 0) {
                continue;
            }
            for (const auto w : adjacency[v]) {
                const int to = ringOf[static_cast<std::size_t>(w)];
                if (to >= 0) {
                    result[static_cast<std::size_t>(from)].push_back(to);
                }
            }
        }
        return result;
    }
};

// The dynamic program over one simple polygon, run by the constructor given
// every vertex's clear neighbours by ring position and `weight(u, w)` for a
// diagonal. `solve` returns the triangles as triples of ring positions, or
// nothing when the predicates contradicted each other (only possible for
// floating-point coordinates).
template <class Number>
class MinimumWeightProgram {
  public:
    template <class EdgeWeight>
    MinimumWeightProgram(const std::vector<std::vector<int>>& clear, EdgeWeight weight)
        : n_(static_cast<int>(clear.size())) {
        // Each vertex's neighbours, sides included, sorted, in one flat array.
        // A pair only one end claims to see — floating-point predicates can
        // disagree — is dropped, so every entry has its mirror at the other end.
        std::vector<std::vector<int>> lists(clear.size());
        for (int v = 0; v < n_; ++v) {
            auto& list = lists[at(v)];
            list = clear[at(v)];
            list.push_back((v + n_ - 1) % n_);
            list.push_back((v + 1) % n_);
            std::sort(list.begin(), list.end());
            list.erase(std::unique(list.begin(), list.end()), list.end());
            list.erase(std::remove(list.begin(), list.end(), v), list.end());
        }
        offset_.assign(clear.size() + 1, 0);
        for (int v = 0; v < n_; ++v) {
            for (const int w : lists[at(v)]) {
                const auto& back = lists[at(w)];
                if (std::binary_search(back.begin(), back.end(), v)) {
                    nbr_.push_back(w);
                }
            }
            offset_[at(v) + 1] = nbr_.size();
        }
        value_.assign(nbr_.size(), Number(0));
        apex_.assign(nbr_.size(), kUnsolved);

        // The diagonals by span, the sides solved already.
        std::vector<std::vector<std::pair<int, int>>> bySpan(clear.size());
        for (int u = 0; u < n_; ++u) {
            for (std::size_t s = offset_[at(u)]; s < offset_[at(u) + 1]; ++s) {
                const int w = nbr_[s];
                if (w == u + 1 || (u == 0 && w == n_ - 1)) {
                    apex_[s] = kSide;
                    apex_[slot(w, u)] = kSide;
                } else if (w > u) {
                    bySpan[static_cast<std::size_t>(w - u)].emplace_back(u, w);
                }
            }
        }
        for (std::size_t span = 2; span < bySpan.size(); ++span) {
            for (const auto& [u, w] : bySpan[span]) {
                const auto [found, apex, cost] = best(u, w);
                if (!found) {
                    continue;
                }
                const Number withSelf = cost + Number(weight(u, w));
                for (const std::size_t s : {slot(u, w), slot(w, u)}) {
                    value_[s] = withSelf;
                    apex_[s] = apex;
                }
            }
        }
        const Best root = best(0, n_ - 1);
        rootApex_ = root.found ? root.apex : kUnsolved;
    }

    std::vector<std::array<int, 3>> solve() const {
        std::vector<std::array<int, 3>> triangles;
        if (rootApex_ == kUnsolved) {
            return triangles;
        }
        triangles.reserve(static_cast<std::size_t>(n_ - 2));
        std::vector<std::pair<int, int>> stack{{0, n_ - 1}};
        while (!stack.empty()) {
            const auto [u, w] = stack.back();
            stack.pop_back();
            const int k = u == 0 && w == n_ - 1 ? rootApex_ : apex_[slot(u, w)];
            triangles.push_back({u, k, w});
            if (k - u >= 2) {
                stack.emplace_back(u, k);
            }
            if (w - k >= 2) {
                stack.emplace_back(k, w);
            }
        }
        return triangles;
    }

  private:
    // An entry's apex, or one of these.
    static constexpr int kUnsolved = -1;
    static constexpr int kSide = -2;

    struct Best {
        bool found;
        int apex;
        Number cost;
    };

    int n_ = 0;
    int rootApex_ = kUnsolved;
    std::vector<std::size_t> offset_;
    std::vector<int> nbr_;
    std::vector<Number> value_;  // E of the link to that neighbour
    std::vector<int> apex_;

    static std::size_t at(int v) { return static_cast<std::size_t>(v); }

    // The entry of neighbour `w` in the list of `v`, which holds it.
    std::size_t slot(int v, int w) const {
        const auto first = nbr_.begin() + static_cast<std::ptrdiff_t>(offset_[at(v)]);
        const auto last = nbr_.begin() + static_cast<std::ptrdiff_t>(offset_[at(v) + 1]);
        return static_cast<std::size_t>(std::lower_bound(first, last, w) - nbr_.begin());
    }

    // The cheapest apex for the link (u, w), u < w: a merge of the two ends'
    // neighbours strictly between them, the first of equal costs winning.
    Best best(int u, int w) const {
        Best result{false, kUnsolved, Number(0)};
        const auto uEnd = nbr_.begin() + static_cast<std::ptrdiff_t>(offset_[at(u) + 1]);
        const auto wEnd = nbr_.begin() + static_cast<std::ptrdiff_t>(offset_[at(w) + 1]);
        auto a = std::upper_bound(nbr_.begin() + static_cast<std::ptrdiff_t>(offset_[at(u)]),
                                  uEnd, u);
        auto b = std::upper_bound(nbr_.begin() + static_cast<std::ptrdiff_t>(offset_[at(w)]),
                                  wEnd, u);
        while (a != uEnd && b != wEnd && *a < w && *b < w) {
            if (*a < *b) {
                ++a;
            } else if (*b < *a) {
                ++b;
            } else {
                const auto sa = static_cast<std::size_t>(a - nbr_.begin());
                const auto sb = static_cast<std::size_t>(b - nbr_.begin());
                if (apex_[sa] != kUnsolved && apex_[sb] != kUnsolved) {
                    Number cost = value_[sa] + value_[sb];
                    if (!result.found || cost < result.cost) {
                        result = {true, *a, std::move(cost)};
                    }
                }
                ++a;
                ++b;
            }
        }
        return result;
    }
};

// The same program for a strictly convex polygon of `n` vertices, where every
// pair of non-adjacent vertices is a diagonal. E is held twice, row-major and
// transposed, so both operands of the inner loop run contiguously; the loop
// keeps only the minimum, and `solve` rescans the n - 2 links the answer uses
// for their apexes.
template <class Number>
class ConvexMinimumWeightProgram {
  public:
    template <class EdgeWeight>
    ConvexMinimumWeightProgram(int n, EdgeWeight weight)
        : n_(n), rows_(cell(n, 0), Number(0)), columns_(cell(n, 0), Number(0)) {
        for (int span = 2; span < n_ - 1; ++span) {
            for (int i = 0; i + span < n_; ++i) {
                const int j = i + span;
                const Number cost = minimum(i, j) + Number(weight(i, j));
                rows_[cell(i, j)] = cost;
                columns_[cell(j, i)] = cost;
            }
        }
    }

    std::vector<std::array<int, 3>> solve() const {
        std::vector<std::array<int, 3>> triangles;
        triangles.reserve(static_cast<std::size_t>(n_ - 2));
        std::vector<std::pair<int, int>> stack{{0, n_ - 1}};
        while (!stack.empty()) {
            const auto [u, w] = stack.back();
            stack.pop_back();
            const int k = apex(u, w);
            triangles.push_back({u, k, w});
            if (k - u >= 2) {
                stack.emplace_back(u, k);
            }
            if (w - k >= 2) {
                stack.emplace_back(k, w);
            }
        }
        return triangles;
    }

  private:
    int n_ = 0;
    std::vector<Number> rows_;     // cell(i, k): E(i, k)
    std::vector<Number> columns_;  // cell(j, k): E(k, j)

    std::size_t cell(int row, int column) const {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(n_) +
               static_cast<std::size_t>(column);
    }

    // C(i, j). For a built-in number four running minima keep the additions
    // independent of one another.
    Number minimum(int i, int j) const {
        const Number* a = rows_.data() + cell(i, 0);
        const Number* b = columns_.data() + cell(j, 0);
        if constexpr (std::is_arithmetic_v<Number>) {
            Number m0 = a[i + 1] + b[i + 1];
            Number m1 = m0;
            Number m2 = m0;
            Number m3 = m0;
            int k = i + 2;
            for (; k + 3 < j; k += 4) {
                m0 = std::min(m0, a[k] + b[k]);
                m1 = std::min(m1, a[k + 1] + b[k + 1]);
                m2 = std::min(m2, a[k + 2] + b[k + 2]);
                m3 = std::min(m3, a[k + 3] + b[k + 3]);
            }
            for (; k < j; ++k) {
                m0 = std::min(m0, a[k] + b[k]);
            }
            return std::min(std::min(m0, m1), std::min(m2, m3));
        } else {
            Number best = a[i + 1] + b[i + 1];
            for (int k = i + 2; k < j; ++k) {
                Number cost = a[k] + b[k];
                if (cost < best) {
                    best = std::move(cost);
                }
            }
            return best;
        }
    }

    // The first apex of least cost for the link (i, j).
    int apex(int i, int j) const {
        const Number* a = rows_.data() + cell(i, 0);
        const Number* b = columns_.data() + cell(j, 0);
        int best = i + 1;
        Number bestCost = a[i + 1] + b[i + 1];
        for (int k = i + 2; k < j; ++k) {
            Number cost = a[k] + b[k];
            if (cost < bestCost) {
                best = k;
                bestCost = std::move(cost);
            }
        }
        return best;
    }
};

}  // namespace detail

// -----------------------------------------------------------------------------
// Polygon
// -----------------------------------------------------------------------------

template <class PointType_, class TLabel>
template <class ApproximateNumber>
auto Polygon<PointType_, TLabel>::minimumWeightTriangulation() const {
    return minimumWeightTriangulation([](const PointType_& p, const PointType_& q) {
        return p.template distance<ApproximateNumber>(q);
    });
}

template <class PointType_, class TLabel>
template <class Weight>
    requires std::invocable<const Weight&, const PointType_&, const PointType_&>
auto Polygon<PointType_, TLabel>::minimumWeightTriangulation(const Weight& weight) const {
    using Result = decltype(triangulation());
    using Number = std::remove_cvref_t<
        std::invoke_result_t<const Weight&, const PointType_&, const PointType_&>>;
    const std::size_t n = size();
    if (n < 3 || isDegenerate()) {
        return Result();
    }
    std::vector<PointType_> ring;
    ring.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        ring.push_back(PointType_((*this)[i]));
    }
    const auto edgeWeight = [&](int u, int w) {
        return std::invoke(weight, ring[static_cast<std::size_t>(u)],
                           ring[static_cast<std::size_t>(w)]);
    };
    const auto toResult = [&](const std::vector<std::array<int, 3>>& triples) {
        std::vector<Triangle<PointType_>> triangles;
        triangles.reserve(triples.size());
        for (const auto& [a, b, c] : triples) {
            triangles.emplace_back(ring[static_cast<std::size_t>(a)],
                                   ring[static_cast<std::size_t>(b)],
                                   ring[static_cast<std::size_t>(c)]);
        }
        return Result(triangles);
    };

    // A strictly convex polygon sees every pair of non-adjacent vertices
    // through a clear diagonal, so there is nothing to triangulate first.
    bool strictlyConvex = isConvex();
    for (std::size_t i = 0; strictlyConvex && i < n; ++i) {
        strictlyConvex = orientationSign(ring[(i + n - 1) % n], ring[i], ring[(i + 1) % n]) != 0;
    }
    if (strictlyConvex) {
        return toResult(
            detail::ConvexMinimumWeightProgram<Number>(static_cast<int>(n), edgeWeight).solve());
    }
    auto tri = triangulation();
    const auto triples =
        detail::MinimumWeightProgram<Number>(
            detail::MinimumWeightTriangulationBuilder::ringVisibility(tri, ring), edgeWeight)
            .solve();
    if (triples.empty()) {
        return tri;  // floating-point predicates contradicted each other
    }
    return toResult(triples);
}

}  // namespace pgl
