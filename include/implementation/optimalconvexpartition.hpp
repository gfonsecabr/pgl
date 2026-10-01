#pragma once

// Defines Polygon::optimalConvexPartition out of line. It needs the visibility
// expansion defined in implementation/visibilitygraph.hpp, which is also its
// predecessor in pgl.hpp, so it includes that header.
#include "implementation/visibilitygraph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @file optimalconvexpartition.hpp
 * @brief Minimum convex partition of a simple polygon without Steiner points.
 *
 * The dynamic program of Keil (1985) and Keil & Snoeyink (2002), extended to
 * collinear vertices. Vertices are numbered `0..n-1` counterclockwise, starting
 * at a reflex vertex. A piece's vertices are all the polygon vertices on its
 * boundary, so it may have straight angles, and every edge of a piece is a side
 * of the polygon or a clear diagonal (an open segment inside the polygon
 * through no vertex). Only diagonals with a reflex end are ever cut along: one
 * between two convex vertices can be removed, since merging its two pieces
 * leaves each end's angle below the polygon's own.
 *
 * For `i < k`, `P(i,k)` is the subpolygon `i, i+1, ..., k` and `W(i,k)` the
 * fewest convex pieces it splits into. In a partition of `P(i,k)` the piece `C`
 * holding the segment `ik` has vertices `i = c0 < c1 < ... < cm = k`; its `a`
 * is `c1` and its `b` is `c(m-1)`. A piece on the other side can merge with `C`
 * only if both merged angles stay convex, and only a reflex end can fail that.
 * So each link keeps the Pareto front of the `(a, b)` pairs over its *optimal*
 * partitions: the best `a` alone when only `i` is reflex, the best `b` alone
 * when only `k` is, a list when both are. Merging saves exactly the one piece
 * keeping the cut would cost, so a suboptimal subpartition never needs merging.
 *
 * - `i` reflex: `C = C'(i..j) + triangle(i,j,k)` with `j = c(m-1)`.
 * - `i` convex, so `k` reflex: `C = triangle(i,j,k) + C''(j..k)` with `j = c1`.
 *
 * Collinear vertices make one of those triangles flat in two ways, and each has
 * its own kind of *link* besides the clear diagonals:
 *
 * - A **chain**: `C'` (or `C''`) has no area, because the piece's first (or
 *   last) vertices lie on one line. The line is a run of sides and diagonals
 *   through reflex or flat vertices inside the range, and the subpolygons behind
 *   its diagonals are cut off whole.
 * - A **touching** link: `C` is straight at `k`, so `triangle(i,j,k)` is flat
 *   with `k` between `i` and `j`. Then `C'` lies in `P(i,j)`, whose segment
 *   `ij` passes through vertices outside the range; `C` is `C'` with `k` added.
 *   For a given `(i,k)` the only such `j` is the vertex straight past `k`.
 *
 * Candidates `j` are the common neighbours of `i` and `k` among the links,
 * found by merging two sorted lists. When both ends are reflex and the polygon
 * has many diagonals between reflex vertices, a first pass takes the minimum
 * `m` of `W(i,j) + W(j,k)` from a dense table, and only the candidates at `m`
 * (at `m + 1` when nothing at `m` merges) are tested for merging: any other
 * costs more than an unmerged one at `m`.
 */

namespace pgl {

namespace detail {

// Friend of Triangulation: the clear visibility of a polygon's reflex vertices.
struct OptimalConvexPartitionBuilder {
    // For a reflex ring vertex, every ring vertex it sees through a diagonal;
    // for any other, the reflex ones that see it. Indices are ring positions.
    // `work` receives the sum over the reflex vertices of their squared numbers
    // of diagonals, and once it passes `maxWork` the traversal stops and
    // nothing comes back.
    template <class Tri, class PointType>
    static std::vector<std::vector<int>> reflexVisibility(
        const Tri& tri, const std::vector<PointType>& ring, const std::vector<char>& reflex,
        long long& work, long long maxWork = std::numeric_limits<long long>::max()) {
        using VertexIndex = typename Tri::VertexIndex;
        const int n = static_cast<int>(ring.size());
        std::unordered_map<PointType, int> position;
        position.reserve(ring.size());
        for (int v = 0; v < n; ++v) {
            position.emplace(ring[static_cast<std::size_t>(v)], v);
        }
        std::vector<int> ringOf(tri.vertices_.size(), -1);
        for (std::size_t v = Tri::GHOST + 1; v < tri.vertices_.size(); ++v) {
            const auto it = position.find(tri.vertices_[v]);
            if (it != position.end()) {
                ringOf[v] = it->second;
            }
        }
        work = 0;
        const auto adjacency = tri.clearVisibleAdjacencyFrom(
            [&](VertexIndex v) {
                const int at = ringOf[static_cast<std::size_t>(v)];
                return at >= 0 && reflex[static_cast<std::size_t>(at)];
            },
            [&](VertexIndex, std::size_t seen) {
                const long long degree = static_cast<long long>(seen);
                work = degree > 0 && work > maxWork - degree * degree ? maxWork + 1
                                                                      : work + degree * degree;
                return work <= maxWork;
            });
        if (work > maxWork) {
            return {};
        }

        std::vector<std::vector<int>> result(ring.size());
        for (std::size_t v = Tri::GHOST + 1; v < adjacency.size(); ++v) {
            const int from = ringOf[v];
            if (from < 0 || !reflex[static_cast<std::size_t>(from)]) {
                continue;
            }
            for (const VertexIndex w : adjacency[v]) {
                const int to = ringOf[static_cast<std::size_t>(w)];
                if (to < 0) {
                    continue;
                }
                result[static_cast<std::size_t>(from)].push_back(to);
                if (!reflex[static_cast<std::size_t>(to)]) {
                    result[static_cast<std::size_t>(to)].push_back(from);
                }
            }
        }
        return result;
    }
};

// Whether MinimumConvexPartition builds its dense table: by default only when
// the diagonals between reflex vertices are many; the tests force either way.
enum class DenseTable { automatic, never, always };

// The dynamic program over one simple polygon with at least one reflex vertex.
// `solve` returns the pieces as rings of positions in the input, counterclockwise.
template <class PointType>
class MinimumConvexPartition {
  public:
    MinimumConvexPartition(const std::vector<PointType>& ring, const std::vector<char>& reflex,
                           const std::vector<std::vector<int>>& visible,
                           DenseTable dense = DenseTable::automatic)
        : n_(static_cast<int>(ring.size())) {
        const std::size_t n = ring.size();
        shift_ = static_cast<int>(std::find(reflex.begin(), reflex.end(), 1) - reflex.begin());
        pts_.reserve(n);
        for (int v = 0; v < n_; ++v) {
            pts_.push_back(ring[static_cast<std::size_t>(original(v))]);
        }
        reflex_.assign(n, 0);
        flat_.assign(n, 0);
        rank_.assign(n, -1);
        for (int v = 0; v < n_; ++v) {
            const int turn = orient(prev(v), v, next(v));
            reflex_[at(v)] = turn < 0;
            flat_[at(v)] = turn == 0;
            if (turn < 0) {
                rank_[at(v)] = static_cast<int>(reflexList_.size());
                reflexList_.push_back(v);
            }
        }

        // Clear diagonals in rotated positions, sorted, sides dropped.
        std::vector<std::vector<int>> clear(n);
        for (int v = 0; v < n_; ++v) {
            for (const int w : visible[static_cast<std::size_t>(original(v))]) {
                const int rw = rotated(w);
                if (!isSide(v, rw) && rw != v) {
                    clear[at(v)].push_back(rw);
                }
            }
        }
        for (auto& list : clear) {
            std::sort(list.begin(), list.end());
            list.erase(std::unique(list.begin(), list.end()), list.end());
        }

        // Around each reflex vertex, its clear neighbours and its two sides by
        // angle, which is what finding the vertex straight past it needs; sorted
        // on first use, since most vertices are never passed straight.
        around_.assign(n, {});
        aroundSorted_.assign(n, 0);
        for (const int v : reflexList_) {
            auto& list = around_[at(v)];
            list = clear[at(v)];
            list.push_back(prev(v));
            list.push_back(next(v));
        }

        // Every link, as (u, w, kind, mid) with u < w.
        struct Link {
            int u, w;
            Kind kind;
            int mid;
        };
        std::vector<Link> links;
        for (const int x : reflexList_) {
            for (const int y : clear[at(x)]) {
                if (!reflex_[at(y)] || y > x) {
                    links.push_back({std::min(x, y), std::max(x, y), Kind::clear, -1});
                }
            }
        }
        // Chains, walked from a reflex end along each ray while the indices keep
        // going the same way; touching links, from a reflex lower end while they
        // keep coming back toward it.
        hasTouching_.assign(n, 0);
        // From x through its neighbour d, the ray goes on while each vertex it
        // meets can be passed straight; the first step decides which kind of
        // link the walk yields, and the walk stops when its indices turn.
        // Starting at x = 0 through its side to n-1 covers the root (0, n-1).
        for (const int x : reflexList_) {
            std::vector<int> firsts(clear[at(x)]);
            firsts.push_back(prev(x));
            firsts.push_back(next(x));
            for (const int d : firsts) {
                int before = d;
                int cur = straightPast(d, x);
                if (cur < 0) {
                    continue;
                }
                const bool up = d > x;
                const Kind kind = up && cur > d ? Kind::chain
                                  : up          ? Kind::touching
                                                : Kind::chain;
                if (!up && cur > d) {
                    continue;
                }
                while (true) {
                    if (kind == Kind::touching) {
                        if (cur <= x) {
                            break;
                        }
                        links.push_back({x, cur, kind, -1});
                        hasTouching_[at(x)] = 1;
                    } else if (up) {
                        links.push_back({x, cur, kind, before});
                    } else {
                        links.push_back({cur, x, kind, before});
                    }
                    const int after = straightPast(cur, before);
                    const bool onward = kind == Kind::touching || !up ? after < cur : after > cur;
                    if (after < 0 || !onward) {
                        break;
                    }
                    before = cur;
                    cur = after;
                }
            }
        }
        std::sort(links.begin(), links.end(), [](const Link& a, const Link& b) {
            return a.u != b.u ? a.u < b.u : a.w < b.w;
        });
        links.erase(std::unique(links.begin(), links.end(),
                                [](const Link& a, const Link& b) {
                                    return a.u == b.u && a.w == b.w;
                                }),
                    links.end());

        // Neighbour lists over the links; a link's slot lies in its owner's
        // list: the lower end if it is reflex, the upper one otherwise.
        nbr_.assign(n, {});
        for (const Link& l : links) {
            nbr_[at(l.u)].push_back(l.w);
            nbr_[at(l.w)].push_back(l.u);
        }
        for (auto& list : nbr_) {
            std::sort(list.begin(), list.end());
        }
        const std::size_t r = reflexList_.size();
        slotBase_.assign(r + 1, 0);
        for (std::size_t q = 0; q < r; ++q) {
            slotBase_[q + 1] = slotBase_[q] + static_cast<int>(nbr_[at(reflexList_[q])].size());
        }
        root_ = slotBase_[r];
        const std::size_t slots = static_cast<std::size_t>(root_) + 1;
        W_.assign(slots, kInf);
        begin_.assign(slots, 0);
        end_.assign(slots, 0);
        kind_.assign(slots, Kind::clear);
        mid_.assign(slots, -1);
        bySpan_.assign(n, {});
        long long bothReflex = 0;
        for (const Link& l : links) {
            const int s = slot(l.u, l.w);
            kind_[static_cast<std::size_t>(s)] = l.kind;
            mid_[static_cast<std::size_t>(s)] = l.mid;
            bySpan_[static_cast<std::size_t>(l.w - l.u)].emplace_back(l.u, l.w);
            bothReflex += l.kind == Kind::clear && reflex_[at(l.u)] && reflex_[at(l.w)];
        }
        slotOf_.assign(n, {});
        for (int v = 0; v < n_; ++v) {
            auto& slotsOfV = slotOf_[at(v)];
            slotsOfV.reserve(nbr_[at(v)].size());
            for (const int w : nbr_[at(v)]) {
                slotsOfV.push_back(slot(std::min(v, w), std::max(v, w)));
            }
        }
        // The dense table pays for its r x n entries only where diagonals
        // between reflex vertices are many.
        if (dense == DenseTable::always ||
            (dense == DenseTable::automatic && bothReflex > 16LL * n_)) {
            table_.assign(r * n, kInf);
            for (const int k : reflexList_) {
                if (k > 0) {
                    table_[cell(k, k - 1)] = 0;  // the side (k-1, k)
                }
            }
        }
    }

    // The pieces, or nothing when the predicates contradicted each other (only
    // possible for floating-point coordinates).
    std::vector<std::vector<int>> solve() {
        for (int span = 2; span < n_; ++span) {
            for (const auto& [u, w] : bySpan_[static_cast<std::size_t>(span)]) {
                const int s = slot(u, w);
                if (kind_[static_cast<std::size_t>(s)] == Kind::chain) {
                    const int m = mid_[static_cast<std::size_t>(s)];
                    W_[static_cast<std::size_t>(s)] = linkCost(u, m) + linkCost(m, w);
                } else {
                    compute(u, w, s);
                }
                if (!table_.empty()) {
                    const Kind kind = kind_[static_cast<std::size_t>(s)];
                    if (kind == Kind::clear && reflex_[at(w)]) {
                        table_[cell(w, u)] = W_[static_cast<std::size_t>(s)];
                    }
                    if (kind != Kind::touching && reflex_[at(u)]) {
                        table_[cell(u, w)] = W_[static_cast<std::size_t>(s)];
                    }
                }
            }
        }
        compute(0, n_ - 1, root_);
        std::vector<std::vector<int>> pieces;
        if (W_[static_cast<std::size_t>(root_)] >= kInf) {
            return pieces;
        }
        reconstruct(pieces);
        for (auto& piece : pieces) {
            for (auto& v : piece) {
                v = original(v);
            }
        }
        return pieces;
    }

  private:
    enum class Kind : char { clear, touching, chain };

    struct Entry {
        int a, b, j, sub;  // sub: the merged link's entry, or -1
    };

    // Above any piece count, and twice it still fits an int.
    static constexpr int kInf = 1 << 28;

    int n_ = 0, shift_ = 0, root_ = 0;
    std::vector<PointType> pts_;
    std::vector<char> reflex_, flat_, hasTouching_, aroundSorted_;
    std::vector<int> rank_, reflexList_;
    std::vector<std::vector<int>> around_, nbr_, slotOf_;
    std::vector<std::vector<std::pair<int, int>>> bySpan_;
    std::vector<int> slotBase_, W_, begin_, end_, mid_;
    std::vector<Kind> kind_;
    std::vector<Entry> entries_;
    std::vector<int> table_;  // cell(x, y): W of the link {x, y}, x reflex; see solve
    std::vector<std::pair<int, Entry>> scratch_;
    std::vector<int> near_, blockMin_;

    static std::size_t at(int v) { return static_cast<std::size_t>(v); }
    int original(int v) const { return (v + shift_) % n_; }
    int rotated(int v) const { return (v - shift_ + n_) % n_; }
    int prev(int v) const { return (v + n_ - 1) % n_; }
    int next(int v) const { return (v + 1) % n_; }
    bool isSide(int u, int w) const { return w == next(u) || u == next(w); }
    std::size_t cell(int x, int y) const {
        return static_cast<std::size_t>(rank_[at(x)]) * static_cast<std::size_t>(n_) + at(y);
    }

    int orient(int a, int b, int c) const {
        return signValue(orientationSign(pts_[at(a)], pts_[at(b)], pts_[at(c)]));
    }

    // Around v, by angle from the positive x axis: x before y.
    bool upperHalf(int v, int x) const {
        const PointType& o = pts_[at(v)];
        const PointType& p = pts_[at(x)];
        return p.y() > o.y() || (p.y() == o.y() && p.x() > o.x());
    }
    bool angleBefore(int v, int x, int y) const {
        const bool hx = upperHalf(v, x);
        const bool hy = upperHalf(v, y);
        if (hx != hy) {
            return hx;
        }
        return orient(v, x, y) > 0;
    }

    // p strictly between a and b on their segment, given the three collinear.
    bool strictlyBetween(int a, int p, int b) const {
        const PointType& pa = pts_[at(a)];
        const PointType& pp = pts_[at(p)];
        const PointType& pb = pts_[at(b)];
        return (pa < pp && pp < pb) || (pb < pp && pp < pa);
    }

    // The vertex w with v strictly between u and w and the segment vw a side or
    // a clear diagonal; -1 when v cannot be passed straight (a convex v, or no
    // vertex in that direction).
    int straightPast(int v, int u) {
        if (flat_[at(v)]) {
            // Only along the two sides, which are one line.
            for (const int w : {prev(v), next(v)}) {
                if (w != u && orient(u, v, w) == 0 && strictlyBetween(u, v, w)) {
                    return w;
                }
            }
            return -1;
        }
        if (!reflex_[at(v)]) {
            return -1;
        }
        // Straight on from u points out of the polygon when it falls strictly
        // inside the exterior wedge, from the side to prev(v) round to the side
        // to next(v): nothing to find there.
        if (orient(v, prev(v), u) < 0 && orient(v, u, next(v)) < 0) {
            return -1;
        }
        // The direction opposite u's: in the other half, and within a half,
        // x comes before it exactly when x comes after u.
        auto& list = around_[at(v)];
        if (!aroundSorted_[at(v)]) {
            std::sort(list.begin(), list.end(),
                      [&](int x, int y) { return angleBefore(v, x, y); });
            aroundSorted_[at(v)] = 1;
        }
        const bool half = !upperHalf(v, u);
        const auto it = std::partition_point(list.begin(), list.end(), [&](int x) {
            const bool hx = upperHalf(v, x);
            if (hx != half) {
                return hx;
            }
            return orient(v, x, u) < 0;
        });
        if (it == list.end() || orient(u, v, *it) != 0 || !strictlyBetween(u, v, *it)) {
            return -1;
        }
        return *it;
    }

    int owner(int u, int w) const { return reflex_[at(u)] ? u : w; }

    // Slot of the link (u,w), u < w; -1 when there is none.
    int slot(int u, int w) const {
        if (!reflex_[at(u)] && !reflex_[at(w)]) {
            return -1;
        }
        const int o = owner(u, w);
        const int other = o == u ? w : u;
        const auto& list = nbr_[at(o)];
        const auto it = std::lower_bound(list.begin(), list.end(), other);
        if (it == list.end() || *it != other) {
            return -1;
        }
        return slotBase_[at(rank_[at(o)])] + static_cast<int>(it - list.begin());
    }

    // What the subpolygon behind a side or a link costs.
    int linkCost(int u, int w) const {
        return w == u + 1 ? 0 : W_[static_cast<std::size_t>(slot(u, w))];
    }

    // a1 closer than a2 to the direction i->k, turning clockwise from it.
    bool betterA(int i, int a1, int a2) const { return orient(i, a2, a1) > 0; }
    // b1 closer than b2 to the direction k->i, turning counterclockwise from it.
    bool betterB(int k, int b1, int b2) const { return orient(k, b2, b1) < 0; }

    // i reflex. In the front of (i,j), the entry with the best a whose piece
    // still turns left at i once k follows j, and at j when j is reflex. The
    // front runs a best first and b worst first, so "ok at i" holds on a prefix
    // and "ok at j" on a suffix.
    int queryA(int s, int i, int j, int k) const {
        const auto okA = [&](const Entry& e) { return orient(k, i, e.a) >= 0; };
        const auto okB = [&](const Entry& e) {
            return !reflex_[at(j)] || orient(e.b, j, k) >= 0;
        };
        int lo = begin_[static_cast<std::size_t>(s)];
        int hi = end_[static_cast<std::size_t>(s)];
        while (lo < hi) {
            const int mid = lo + (hi - lo) / 2;
            if (okB(entries_[static_cast<std::size_t>(mid)])) {
                hi = mid;
            } else {
                lo = mid + 1;
            }
        }
        if (lo == end_[static_cast<std::size_t>(s)] || !okA(entries_[static_cast<std::size_t>(lo)])) {
            return -1;
        }
        return lo;
    }

    // k reflex. In the front of (j,k), the entry with the best b whose piece
    // still turns left at k once i precedes j, and at j when j is reflex.
    int queryB(int s, int i, int j, int k) const {
        const auto okA = [&](const Entry& e) {
            return !reflex_[at(j)] || orient(i, j, e.a) >= 0;
        };
        const auto okB = [&](const Entry& e) { return orient(e.b, k, i) >= 0; };
        int lo = begin_[static_cast<std::size_t>(s)];
        int hi = end_[static_cast<std::size_t>(s)];
        while (lo < hi) {
            const int mid = lo + (hi - lo) / 2;
            if (okA(entries_[static_cast<std::size_t>(mid)])) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo == begin_[static_cast<std::size_t>(s)] ||
            !okB(entries_[static_cast<std::size_t>(lo - 1)])) {
            return -1;
        }
        return lo - 1;
    }

    // Calls f(j, slot of (i,j), slot of (j,k)) for every candidate split vertex
    // other than a touching one, with -1 for a side of the polygon. With i
    // reflex, (i,j) may be a clear diagonal or a chain and (j,k) must be a clear
    // diagonal; with i convex, the other way round.
    template <class F>
    void forEachCandidate(int i, int k, F&& f) const {
        const bool fromI = reflex_[at(i)];
        const auto leftOk = [&](int s) {
            const Kind kind = kind_[static_cast<std::size_t>(s)];
            return fromI ? kind != Kind::touching : kind == Kind::clear;
        };
        const auto rightOk = [&](int s) {
            const Kind kind = kind_[static_cast<std::size_t>(s)];
            return fromI ? kind == Kind::clear : kind != Kind::touching;
        };
        const auto& li = nbr_[at(i)];
        const auto& lk = nbr_[at(k)];
        const auto& si = slotOf_[at(i)];
        const auto& sk = slotOf_[at(k)];
        const int ib = static_cast<int>(std::upper_bound(li.begin(), li.end(), i) - li.begin());
        const int ie = static_cast<int>(std::lower_bound(li.begin() + ib, li.end(), k) - li.begin());
        const int kb = static_cast<int>(std::upper_bound(lk.begin(), lk.end(), i) - lk.begin());
        const int ke = static_cast<int>(std::lower_bound(lk.begin() + kb, lk.end(), k) - lk.begin());
        // The sides: i+1 can only be in k's list, and k-1 in i's.
        if (k == i + 2) {
            f(i + 1, -1, -1);
        } else {
            if (kb < ke && lk[at(kb)] == i + 1 && rightOk(sk[at(kb)])) {
                f(i + 1, -1, sk[at(kb)]);
            }
            if (ib < ie && li[at(ie - 1)] == k - 1 && leftOk(si[at(ie - 1)])) {
                f(k - 1, si[at(ie - 1)], -1);
            }
        }
        const auto both = [&](int j, int sij, int sjk) {
            if (leftOk(sij) && rightOk(sjk)) {
                f(j, sij, sjk);
            }
        };
        const int ni = ie - ib;
        const int nk = ke - kb;
        if (8 * ni < nk) {
            for (int p = ib; p < ie; ++p) {
                const auto it = std::lower_bound(lk.begin() + kb, lk.begin() + ke, li[at(p)]);
                if (it != lk.begin() + ke && *it == li[at(p)]) {
                    both(*it, si[at(p)], sk[static_cast<std::size_t>(it - lk.begin())]);
                }
            }
        } else if (8 * nk < ni) {
            for (int q = kb; q < ke; ++q) {
                const auto it = std::lower_bound(li.begin() + ib, li.begin() + ie, lk[at(q)]);
                if (it != li.begin() + ie && *it == lk[at(q)]) {
                    both(*it, si[static_cast<std::size_t>(it - li.begin())], sk[at(q)]);
                }
            }
        } else {
            int p = ib;
            int q = kb;
            while (p < ie && q < ke) {
                const int a = li[at(p)];
                const int b = lk[at(q)];
                if (a < b) {
                    ++p;
                } else if (b < a) {
                    ++q;
                } else {
                    both(a, si[at(p)], sk[at(q)]);
                    ++p;
                    ++q;
                }
            }
        }
    }

    // Keeps `e` at `cost` if it ties or beats the best so far.
    void offer(int& best, int cost, const Entry& e) {
        if (cost > best) {
            return;
        }
        if (cost < best) {
            scratch_.clear();
            best = cost;
        }
        scratch_.emplace_back(cost, e);
    }

    // i reflex: the piece straight at k, which is C' on the touching link to the
    // vertex straight past k, with k added. Always convex, so it merges.
    void offerTouching(int i, int k, int& best) {
        if (!hasTouching_[at(i)]) {
            return;
        }
        const int j = straightPast(k, i);
        if (j <= i || j >= k) {
            return;
        }
        const int s = slot(i, j);
        if (s < 0 || kind_[static_cast<std::size_t>(s)] != Kind::touching ||
            begin_[static_cast<std::size_t>(s)] == end_[static_cast<std::size_t>(s)]) {
            return;
        }
        const int e = begin_[static_cast<std::size_t>(s)];
        offer(best, W_[static_cast<std::size_t>(s)] + linkCost(j, k),
              Entry{entries_[static_cast<std::size_t>(e)].a, j, j, e});
    }

    // Stores `best` and the front of the entries in scratch_ for slot s: the
    // Pareto front when both ends are reflex, the best a or b alone otherwise.
    void store(int i, int k, int s, int best) {
        W_[static_cast<std::size_t>(s)] = best;
        begin_[static_cast<std::size_t>(s)] = static_cast<int>(entries_.size());
        if (!scratch_.empty()) {
            const bool ri = reflex_[at(i)];
            const bool rk = reflex_[at(k)];
            if (ri && rk) {
                std::sort(scratch_.begin(), scratch_.end(), [&](const auto& x, const auto& y) {
                    const Entry& p = x.second;
                    const Entry& q = y.second;
                    if (p.a != q.a) {
                        if (betterA(i, p.a, q.a)) {
                            return true;
                        }
                        if (betterA(i, q.a, p.a)) {
                            return false;
                        }
                    }
                    return betterB(k, p.b, q.b);
                });
                const std::size_t first = entries_.size();
                for (const auto& [cost, e] : scratch_) {
                    if (entries_.size() == first || betterB(k, e.b, entries_.back().b)) {
                        entries_.push_back(e);
                    }
                }
            } else {
                Entry keep = scratch_.front().second;
                for (const auto& [cost, e] : scratch_) {
                    if (ri ? betterA(i, e.a, keep.a) : betterB(k, e.b, keep.b)) {
                        keep = e;
                    }
                }
                entries_.push_back(keep);
            }
        }
        end_[static_cast<std::size_t>(s)] = static_cast<int>(entries_.size());
    }

    void compute(int i, int k, int s) {
        scratch_.clear();
        int best = kInf;
        if (reflex_[at(i)]) {
            if (!table_.empty() && reflex_[at(k)]) {
                computeDense(i, k, s);
                return;
            }
            offerTouching(i, k, best);
            forEachCandidate(i, k, [&](int j, int sij, int sjk) {
                const int wjk = sjk < 0 ? 0 : W_[static_cast<std::size_t>(sjk)];
                if (sij < 0) {  // j = i+1: the piece is the triangle
                    offer(best, 1 + wjk, Entry{j, j, j, -1});
                    return;
                }
                const int base = W_[static_cast<std::size_t>(sij)] + wjk;
                if (base > best) {
                    return;
                }
                if (kind_[static_cast<std::size_t>(sij)] == Kind::chain) {
                    offer(best, base + 1, Entry{j, j, j, -1});  // nothing to merge with
                    return;
                }
                const int idx = queryA(sij, i, j, k);
                if (idx >= 0) {
                    offer(best, base, Entry{entries_[static_cast<std::size_t>(idx)].a, j, j, idx});
                } else {
                    offer(best, base + 1, Entry{j, j, j, -1});
                }
            });
        } else {
            forEachCandidate(i, k, [&](int j, int sij, int sjk) {
                const int wij = sij < 0 ? 0 : W_[static_cast<std::size_t>(sij)];
                if (sjk < 0) {  // j = k-1: the piece is the triangle
                    offer(best, 1 + wij, Entry{j, j, j, -1});
                    return;
                }
                const int base = wij + W_[static_cast<std::size_t>(sjk)];
                if (base > best) {
                    return;
                }
                if (kind_[static_cast<std::size_t>(sjk)] == Kind::chain) {
                    offer(best, base + 1, Entry{j, j, j, -1});
                    return;
                }
                const int idx = queryB(sjk, i, j, k);
                if (idx >= 0) {
                    offer(best, base, Entry{j, entries_[static_cast<std::size_t>(idx)].b, j, idx});
                } else {
                    offer(best, base + 1, Entry{j, j, j, -1});
                }
            });
        }
        store(i, k, s, best);
    }

    // Both ends reflex, with the dense table: the minimum m of W(i,j) + W(j,k)
    // first, then the merge test only where it can matter. A chain counts as a
    // candidate that never merges.
    void computeDense(int i, int k, int s) {
        const auto& li = nbr_[at(i)];
        const int base0 = slotBase_[at(rank_[at(i)])];
        const int ib = static_cast<int>(std::upper_bound(li.begin(), li.end(), i) - li.begin());
        const int ie = static_cast<int>(std::lower_bound(li.begin() + ib, li.end(), k) - li.begin());
        const int* left = &table_[cell(i, 0)];   // W(i, j): clear or chain
        const int* right = &table_[cell(k, 0)];  // W(j, k): clear, or 0 for the side
        const int* nb = li.data();
        const auto sumAt = [&](int p) {
            const std::size_t sl = static_cast<std::size_t>(base0 + p);
            return kind_[sl] == Kind::touching ? kInf : W_[sl] + right[nb[p]];
        };
        // Scan the index range when most of it are neighbours — two contiguous
        // rows, which vectorizes — keeping a minimum per block so that
        // collecting visits only the blocks that can hold the target.
        const bool rangeScan = 8 * (ie - ib) >= k - i;
        constexpr int kBlock = 32;
        const int lo = i + 2;
        int m = kInf;
        if (rangeScan) {
            blockMin_.clear();
            for (int jb = lo; jb < k; jb += kBlock) {
                const int je = std::min(k, jb + kBlock);
                int mm = kInf;
                for (int j = jb; j < je; ++j) {
                    const int sum = left[j] + right[j];
                    mm = sum < mm ? sum : mm;
                }
                blockMin_.push_back(mm);
                m = std::min(m, mm);
            }
        } else {
            for (int p = ib; p < ie; ++p) {
                m = std::min(m, sumAt(p));
            }
        }
        const auto collect = [&](int target) {
            near_.clear();
            if (rangeScan) {
                int p = ib;
                for (int b = 0; b < static_cast<int>(blockMin_.size()); ++b) {
                    if (blockMin_[at(b)] > target) {
                        continue;
                    }
                    const int jb = lo + b * kBlock;
                    const int je = std::min(k, jb + kBlock);
                    for (int j = jb; j < je; ++j) {
                        if (left[j] + right[j] == target) {
                            while (nb[p] < j) {
                                ++p;
                            }
                            near_.push_back(p);
                        }
                    }
                }
            } else {
                for (int p = ib; p < ie; ++p) {
                    if (sumAt(p) == target) {
                        near_.push_back(p);
                    }
                }
            }
        };

        scratch_.clear();
        int best = kInf;
        // j = i+1, along the side: the piece is the triangle.
        if (i + 1 < k) {
            const int wk = i + 1 == k - 1 ? 0 : right[i + 1];
            if (wk < kInf) {
                offer(best, 1 + wk, Entry{i + 1, i + 1, i + 1, -1});
            }
        }
        offerTouching(i, k, best);
        const auto tryAt = [&](int target) {
            bool merged = false;
            for (const int p : near_) {
                const int j = nb[p];
                const int sl = base0 + p;
                const int idx = kind_[static_cast<std::size_t>(sl)] == Kind::chain
                                    ? -1
                                    : queryA(sl, i, j, k);
                if (idx >= 0) {
                    offer(best, target, Entry{entries_[static_cast<std::size_t>(idx)].a, j, j, idx});
                    merged = true;
                } else if (target == m) {
                    offer(best, target + 1, Entry{j, j, j, -1});
                }
            }
            return merged;
        };
        if (m < kInf) {
            collect(m);
            if (!tryAt(m) && best > m) {
                collect(m + 1);
                tryAt(m + 1);
            }
        }
        store(i, k, s, best);
    }

    // Reads the chosen partition back, without recursion: each piece is walked
    // along its chain of merges, and what it cuts off is queued.
    void reconstruct(std::vector<std::vector<int>>& pieces) const {
        std::vector<std::pair<int, int>> pending{{0, n_ - 1}};
        std::vector<std::pair<int, int>> links;
        // Queues the pieces behind a side (none), a clear diagonal (one piece
        // holding it) or a chain (whatever is behind each of its links).
        const auto cutOff = [&](int u, int w) {
            links.emplace_back(u, w);
            while (!links.empty()) {
                const auto [x, y] = links.back();
                links.pop_back();
                if (y == x + 1) {
                    continue;
                }
                const int s = slot(x, y);
                if (kind_[static_cast<std::size_t>(s)] == Kind::chain) {
                    links.emplace_back(x, mid_[static_cast<std::size_t>(s)]);
                    links.emplace_back(mid_[static_cast<std::size_t>(s)], y);
                } else {
                    pending.emplace_back(x, y);
                }
            }
        };
        std::vector<int> head, tail;
        bool isRoot = true;
        while (!pending.empty()) {
            const auto [pi, pk] = pending.back();
            pending.pop_back();
            int ci = pi;
            int ck = pk;
            int e = begin_[static_cast<std::size_t>(isRoot ? root_ : slot(ci, ck))];
            isRoot = false;
            head.clear();
            tail.clear();
            while (true) {
                const Entry en = entries_[static_cast<std::size_t>(e)];
                const int j = en.j;
                if (reflex_[at(ci)]) {
                    cutOff(j, ck);
                    if (en.sub >= 0) {
                        tail.push_back(ck);
                        ck = j;
                        e = en.sub;
                        continue;
                    }
                    cutOff(ci, j);
                } else {
                    cutOff(ci, j);
                    if (en.sub >= 0) {
                        head.push_back(ci);
                        ci = j;
                        e = en.sub;
                        continue;
                    }
                    cutOff(j, ck);
                }
                std::vector<int> piece(head);
                piece.insert(piece.end(), {ci, j, ck});
                piece.insert(piece.end(), tail.rbegin(), tail.rend());
                pieces.push_back(std::move(piece));
                break;
            }
        }
    }
};

}  // namespace detail

// -----------------------------------------------------------------------------
// Polygon
// -----------------------------------------------------------------------------

namespace detail {

// A polygon's ring, as the partition works on it, with its reflex vertices.
template <class PolygonType>
struct PartitionRing {
    using PointType = typename PolygonType::PointType;

    explicit PartitionRing(const PolygonType& polygon) {
        const std::size_t n = polygon.size();
        points.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            points.push_back(PointType(polygon[i]));
        }
        reflex.assign(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            reflex[i] = n >= 3 && orientationSign(points[(i + n - 1) % n], points[i],
                                                  points[(i + 1) % n]) < 0;
            reflexCount += reflex[i];
        }
    }

    // The piece on these ring positions in the canonical form a trusted Convex
    // asks for: counterclockwise, no vertex in the middle of a straight stretch,
    // lexicographically smallest first. Fewer than three corners left means it
    // has no area, and nothing is added.
    void emit(const std::vector<int>& at, std::vector<Convex<PointType>>& pieces) const {
        std::vector<PointType> kept;
        const std::size_t m = at.size();
        for (std::size_t t = 0; t < m; ++t) {
            const PointType& before = points[static_cast<std::size_t>(at[(t + m - 1) % m])];
            const PointType& here = points[static_cast<std::size_t>(at[t])];
            const PointType& after = points[static_cast<std::size_t>(at[(t + 1) % m])];
            if (orientationSign(before, here, after) != 0) {
                kept.push_back(here);
            }
        }
        if (kept.size() >= 3) {
            std::rotate(kept.begin(), std::min_element(kept.begin(), kept.end()), kept.end());
            pieces.push_back(Convex<PointType>(kept, pgl::trusted));
        }
    }

    std::vector<PointType> points;
    std::vector<char> reflex;
    std::size_t reflexCount = 0;
};

// The fewest convex pieces of a polygon with a reflex vertex, from its
// triangulation `tri`, in canonical order. When the program's work on the
// visibility — the sum over the reflex vertices of their squared numbers of
// diagonals — would pass `maxWork`, the visibility is abandoned as soon as that
// shows and the triangulation's own Hertel-Mehlhorn partition comes back
// instead, so the polygon is triangulated once whichever it is.
template <class PolygonType, class Tri>
std::vector<Convex<typename PolygonType::PointType>> optimalConvexPieces(
    const PartitionRing<PolygonType>& ring, const Tri& tri,
    long long maxWork = std::numeric_limits<long long>::max()) {
    using PointType = typename PolygonType::PointType;
    long long work = 0;
    const auto visible =
        OptimalConvexPartitionBuilder::reflexVisibility(tri, ring.points, ring.reflex, work, maxWork);
    if (work > maxWork) {
        return tri.convexPartition();
    }
    MinimumConvexPartition<PointType> program(ring.points, ring.reflex, visible);
    const auto rings = program.solve();
    if (rings.empty()) {
        return tri.convexPartition();  // floating-point predicates contradicted each other
    }
    std::vector<Convex<PointType>> pieces;
    for (const auto& at : rings) {
        ring.emit(at, pieces);
    }
    std::sort(pieces.begin(), pieces.end());
    return pieces;
}

}  // namespace detail

template <class PointType_, class TLabel>
std::vector<Convex<PointType_>> Polygon<PointType_, TLabel>::optimalConvexPartition() const {
    std::vector<Convex<PointType_>> pieces;
    if (size() < 3) {
        return pieces;
    }
    const detail::PartitionRing<Polygon> ring(*this);
    if (ring.reflexCount == 0) {
        std::vector<int> all(size());
        for (std::size_t i = 0; i < all.size(); ++i) {
            all[i] = static_cast<int>(i);
        }
        ring.emit(all, pieces);
        return pieces;
    }
    return detail::optimalConvexPieces(ring, triangulation());
}

}  // namespace pgl
