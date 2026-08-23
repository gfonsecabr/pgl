#pragma once

#include "algorithm/intersections.hpp"

/**
 * @file redbluesweep.hpp
 * @brief Bichromatic (red-blue) boundary contact by one combined plane sweep.
 *
 * Two closed edge sequences — typically the boundaries of two simple polygons —
 * are swept together left to right in a single pass. The sweep answers the two
 * booleans a containment test needs, and nothing more:
 *
 *   - does some red edge *properly cross* some blue edge, and
 *   - do the two boundaries meet at all?
 *
 * Only red-blue pairs are ever tested: each input is assumed non-self-crossing
 * (the simple-polygon contract), so same-colour pairs cannot contribute an
 * intersection that the other colour does not already witness. That is what
 * makes a single status structure enough, and what keeps the whole thing at
 * O((n + m) log(n + m)) no matter how the boundaries wiggle — unlike a
 * decomposition into lexicographically monotone chains, whose cost is the
 * product of the two chain counts and therefore degrades to quadratic on
 * jagged, comb-like or star-shaped input.
 */

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

/**
 * @brief How two edge sets meet, as classified by @ref redBlueSweep.
 *
 * The three states are deliberately asymmetric in strength, because the sweep
 * stops at the first crossing it sees rather than enumerating everything:
 *
 *   - @ref BoundaryContact::Disjoint is a *complete* claim: no red edge meets
 *     any blue edge, anywhere.
 *   - @ref BoundaryContact::Crossing is a *verified* claim: a concrete pair was
 *     checked with @ref Segment::crosses and does properly cross.
 *   - @ref BoundaryContact::Touching means the boundaries do meet and no
 *     crossing pair was found — which is not quite the same as there being
 *     none, since the search for one stops early in a few degenerate corners
 *     (see @ref redBlueSweep). A caller that needs certainty either way must
 *     treat it as "meets, crossing status unknown" and settle the question by
 *     other means; a caller that only wants to skip work when the boundaries
 *     are clear of each other can rely on @ref BoundaryContact::Disjoint.
 */
enum class BoundaryContact {
    Disjoint,  ///< No red edge meets any blue edge.
    Touching,  ///< The two edge sets meet; no crossing pair was found.
    Crossing   ///< A red edge properly crosses a blue edge.
};

namespace detail {

/**
 * @brief The red-blue sweep itself; see @ref pgl::redBlueSweep.
 *
 * @tparam Number Coordinate type both edge sets are converted to, so that one
 *         status structure can hold edges of either colour. It is the common
 *         type of the two inputs, so the conversion is exact.
 */
template <class Number>
class RedBlueSweeper {
  public:
    using SweepPoint = pgl::Point<Number>;
    using SweepSegment = pgl::Segment<SweepPoint>;

  private:
    /// One input edge, endpoints in lexicographic order, tagged with its colour.
    struct Edge {
        SweepPoint lo;   ///< Lexicographically smaller endpoint.
        SweepPoint hi;   ///< Lexicographically larger endpoint.
        bool blue = false;

        /// Vertical (and, degenerately, zero-length) edges never enter the status.
        bool upright() const { return lo.x() == hi.x(); }
        SweepSegment segment() const { return SweepSegment(lo, hi); }
    };

    /**
     * @brief Event kinds, in the order they are *stored*; the sweep still walks
     * the phases of an x-batch explicitly rather than relying on this order.
     */
    enum class Kind : unsigned char {
        Right = 0,       ///< Right endpoint of a non-vertical edge: leaves the status.
        Upper = 1,       ///< Upper endpoint of a vertical edge: a vertex, nothing else.
        UprightLow = 2,  ///< Lower endpoint of a vertical edge: triggers its scan.
        Left = 3         ///< Left endpoint of a non-vertical edge: enters the status.
    };

    struct Event {
        SweepPoint at;
        std::size_t edge = 0;
        Kind kind = Kind::Left;

        bool operator<(const Event& other) const {
            if (at < other.at) return true;
            if (other.at < at) return false;
            if (kind != other.kind) return kind < other.kind;
            return edge < other.edge;
        }
    };

    /**
     * @brief Orders the status by height along the sweep line — exactly, and
     * without ever consulting the sweep line's position.
     *
     * Two edges that are in the status at the same time both span the current
     * sweep x, so their x-ranges overlap in more than a point (the sweep deletes
     * before it inserts, so an edge ending at x is gone before one starting
     * there arrives). Over that overlap two non-crossing edges keep a fixed
     * order, so it can be read off once, at the left end of the overlap, where
     * one edge contributes an endpoint and a single @ref orientationSign settles
     * the comparison — no y-at-x evaluation, no division, no rationals. Edges
     * that meet at that left end are separated by which way they go from there
     * (a second orientation test), and truly collinear overlapping edges by
     * index, which keeps the relation a strict total order.
     *
     * Because the answer does not depend on the sweep position, the order of
     * stored elements never silently changes underneath `std::set`, and an
     * erase always finds the element where the insert left it.
     */
    struct Below {
        const std::vector<Edge>* edges = nullptr;
        using is_transparent = void;

        /// Side of @p that relative to @p one, given `one.lo.x() <= that.lo.x()`.
        static int sideOf(const Edge& one, const Edge& that) {
            int side = signOf(orientationSign(one.lo, one.hi, that.lo));
            if (side == 0) {
                side = signOf(orientationSign(one.lo, one.hi, that.hi));
            }
            // one.lo -> one.hi points right, so counterclockwise means above.
            return side > 0 ? -1 : (side < 0 ? 1 : 0);
        }

        int compare(std::size_t first, std::size_t second) const {
            if (first == second) {
                return 0;
            }
            const Edge& one = (*edges)[first];
            const Edge& other = (*edges)[second];
            const int side =
                one.lo.x() <= other.lo.x() ? sideOf(one, other) : -sideOf(other, one);
            if (side != 0) {
                return side;
            }
            return first < second ? -1 : 1;
        }

        bool operator()(std::size_t first, std::size_t second) const {
            return compare(first, second) < 0;
        }

        // A bare point probes the status at the sweep line: it sorts below every
        // edge it lies on, so lower_bound(p) is the first edge at or above p.
        bool operator()(std::size_t first, const SweepPoint& probe) const {
            const Edge& one = (*edges)[first];
            return signOf(orientationSign(one.lo, one.hi, probe)) > 0;
        }
        bool operator()(const SweepPoint& probe, std::size_t second) const {
            const Edge& other = (*edges)[second];
            return signOf(orientationSign(other.lo, other.hi, probe)) <= 0;
        }
    };

    std::vector<Edge> edges_;
    std::vector<Event> events_;
    std::set<std::size_t, Below> status_;
    bool touching_ = false;

    /// Axis-aligned extent of one colour, used only to discard hopeless edges.
    struct Extent {
        Number xlo{}, ylo{}, xhi{}, yhi{};
        bool any = false;

        void insert(const SweepPoint& point) {
            if (!any) {
                xlo = xhi = point.x();
                ylo = yhi = point.y();
                any = true;
                return;
            }
            if (point.x() < xlo) xlo = point.x();
            if (xhi < point.x()) xhi = point.x();
            if (point.y() < ylo) ylo = point.y();
            if (yhi < point.y()) yhi = point.y();
        }

        bool overlaps(const SweepPoint& lo, const SweepPoint& hi) const {
            return any && !(hi.x() < xlo) && !(xhi < lo.x()) &&
                   !(std::max(lo.y(), hi.y()) < ylo) && !(yhi < std::min(lo.y(), hi.y()));
        }
    };

    template <class Range>
    void gather(const Range& range, bool blue, Extent& extent) {
        for (const auto& edge : range) {
            Edge stored{SweepPoint(edge.min()), SweepPoint(edge.max()), blue};
            extent.insert(stored.lo);
            extent.insert(stored.hi);
            edges_.push_back(std::move(stored));
        }
    }

    /// A red-blue pair that just became adjacent in the status.
    bool adjacent(std::size_t first, std::size_t second) {
        const Edge& one = edges_[first];
        const Edge& other = edges_[second];
        if (one.blue == other.blue) {
            return false;
        }
        const SweepSegment mine = one.segment();
        const SweepSegment theirs = other.segment();
        if (mine.crosses(theirs)) {
            return true;
        }
        if (mine.intersects(theirs)) {
            touching_ = true;
        }
        return false;
    }

    /**
     * @brief Tests one vertical edge against the status.
     *
     * Every status edge spans the sweep x, so the ones meeting the vertical are
     * exactly those whose height at x falls inside its y-range: a contiguous
     * run, located in O(log n) and walked from below. The walk stops at the
     * first red-blue pair that merely touches, which keeps a vertical's cost
     * O(log n) — at the price of possibly reporting @ref
     * BoundaryContact::Touching where a later edge in the same run would have
     * crossed. Same-coloured hits are skipped and, for a simple polygon, there
     * are at most the two boundary edges sharing an endpoint with the vertical.
     */
    bool scanUpright(std::size_t index) {
        const Edge& upright = edges_[index];
        const SweepSegment bar = upright.segment();
        for (auto it = status_.lower_bound(upright.lo); it != status_.end(); ++it) {
            const Edge& other = edges_[*it];
            if (signOf(orientationSign(other.lo, other.hi, upright.hi)) < 0) {
                break;  // past the top of the vertical
            }
            if (other.blue == upright.blue) {
                continue;
            }
            if (other.segment().crosses(bar)) {
                return true;
            }
            touching_ = true;
            break;
        }
        return false;
    }

    /**
     * @brief Detects a red-blue overlap among the vertical edges sharing an x.
     *
     * They are collinear, so they can never cross; all that is at stake is
     * whether two of opposite colour share a point. The events arrive sorted by
     * y, so the two colour lists are sorted too and one merge finds an
     * overlapping pair if there is one.
     */
    void uprightPairs(std::size_t begin, std::size_t end) {
        std::vector<std::size_t> red;
        std::vector<std::size_t> blue;
        for (std::size_t k = begin; k < end; ++k) {
            if (events_[k].kind != Kind::UprightLow) {
                continue;
            }
            (edges_[events_[k].edge].blue ? blue : red).push_back(events_[k].edge);
        }
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < red.size() && j < blue.size()) {
            const Edge& one = edges_[red[i]];
            const Edge& other = edges_[blue[j]];
            if (one.hi.y() < other.lo.y()) {
                ++i;
            } else if (other.hi.y() < one.lo.y()) {
                ++j;
            } else {
                touching_ = true;
                return;
            }
        }
    }

    /// Processes every event sharing one x. Returns true on a verified crossing.
    bool batch(std::size_t begin, std::size_t end) {
        // A point that is an endpoint of edges of both colours is a meeting the
        // status structure cannot see: an edge ending there is erased before one
        // starting there is inserted, so such a pair is never adjacent. It is
        // never a crossing either — a shared endpoint makes one of the four
        // orientations in Segment::crosses vanish — so recognizing it here, from
        // the events alone, closes the only gap in the adjacency argument.
        for (std::size_t k = begin; k < end;) {
            std::size_t run = k;
            bool red = false;
            bool blue = false;
            while (run < end && !(events_[k].at < events_[run].at)) {
                (edges_[events_[run].edge].blue ? blue : red) = true;
                ++run;
            }
            if (red && blue) {
                touching_ = true;
            }
            k = run;
        }

        // Verticals see the status twice, since neither snapshot alone holds
        // every edge spanning this x: before the deletions it still has the ones
        // ending here, after the insertions it has the ones starting here.
        for (std::size_t k = begin; k < end; ++k) {
            if (events_[k].kind == Kind::UprightLow && scanUpright(events_[k].edge)) {
                return true;
            }
        }
        uprightPairs(begin, end);

        for (std::size_t k = begin; k < end; ++k) {
            if (events_[k].kind != Kind::Right) {
                continue;
            }
            const auto it = status_.find(events_[k].edge);
            if (it == status_.end()) {
                continue;
            }
            const bool hasBelow = it != status_.begin();
            auto below = it;
            if (hasBelow) {
                --below;
            }
            const auto above = std::next(it);
            status_.erase(it);
            if (hasBelow && above != status_.end() && adjacent(*below, *above)) {
                return true;
            }
        }

        for (std::size_t k = begin; k < end; ++k) {
            if (events_[k].kind != Kind::Left) {
                continue;
            }
            const auto [it, fresh] = status_.insert(events_[k].edge);
            if (!fresh) {
                continue;
            }
            if (it != status_.begin() && adjacent(*std::prev(it), *it)) {
                return true;
            }
            const auto above = std::next(it);
            if (above != status_.end() && adjacent(*it, *above)) {
                return true;
            }
        }

        for (std::size_t k = begin; k < end; ++k) {
            if (events_[k].kind == Kind::UprightLow && scanUpright(events_[k].edge)) {
                return true;
            }
        }
        return false;
    }

  public:
    template <class RedRange, class BlueRange>
    RedBlueSweeper(const RedRange& red, const BlueRange& blue) : status_(Below{&edges_}) {
        Extent redExtent;
        Extent blueExtent;
        gather(red, false, redExtent);
        const std::size_t split = edges_.size();
        gather(blue, true, blueExtent);

        // An edge whose box misses the other colour's box cannot meet anything
        // of that colour, and dropping it can only remove intersections, never
        // create one. On two boundaries that overlap everywhere this costs one
        // extra pass; on two that barely touch it removes almost all the work.
        std::vector<Edge> kept;
        kept.reserve(edges_.size());
        for (std::size_t i = 0; i < edges_.size(); ++i) {
            const Extent& against = i < split ? blueExtent : redExtent;
            if (against.overlaps(edges_[i].lo, edges_[i].hi)) {
                kept.push_back(edges_[i]);
            }
        }
        edges_ = std::move(kept);

        events_.reserve(2 * edges_.size());
        for (std::size_t i = 0; i < edges_.size(); ++i) {
            const Edge& edge = edges_[i];
            if (edge.upright()) {
                events_.push_back(Event{edge.lo, i, Kind::UprightLow});
                events_.push_back(Event{edge.hi, i, Kind::Upper});
            } else {
                events_.push_back(Event{edge.lo, i, Kind::Left});
                events_.push_back(Event{edge.hi, i, Kind::Right});
            }
        }
    }

    BoundaryContact run() {
        std::sort(events_.begin(), events_.end());
        std::size_t i = 0;
        while (i < events_.size()) {
            std::size_t j = i;
            while (j < events_.size() && !(events_[i].at.x() < events_[j].at.x())) {
                ++j;
            }
            if (batch(i, j)) {
                return BoundaryContact::Crossing;
            }
            i = j;
        }
        return touching_ ? BoundaryContact::Touching : BoundaryContact::Disjoint;
    }
};

}  // namespace detail

/**
 * @brief Classifies how two edge sets meet, in one combined left-to-right sweep.
 *
 * @p red and @p blue are ranges of segment-like values — the edges of two
 * polygon boundaries, say, from @ref Polygon::edgesView. Neither set may
 * properly cross itself: that is the simple-polygon contract, and it is what
 * lets the sweep test red against blue only, and ignore how tangled either
 * boundary is on its own.
 *
 * The sweep keeps the edges crossing the sweep line in a height-ordered status
 * structure and tests a pair the moment the two become neighbours in it. Every
 * pair that ever meets becomes adjacent first (or shares an endpoint, which the
 * event stream reports directly), so a @ref BoundaryContact::Disjoint answer is
 * exhaustive. It stops at the first *verified* crossing — one that
 * @ref Segment::crosses confirms — and merely records that the boundaries touch
 * otherwise, so touchings never enlarge the event queue and cannot slow it down.
 *
 * Every decision goes through @ref orientationSign on the original coordinates,
 * so nothing is evaluated in floating point that was not already, and the sweep
 * needs neither y-at-x interpolation nor rational arithmetic.
 *
 * Complexity: O((n + m) log(n + m)) for n red and m blue edges, whatever the
 * shape of either boundary and however many touchings there are.
 *
 * @param red First edge set.
 * @param blue Second edge set.
 * @return @ref BoundaryContact::Disjoint, @ref BoundaryContact::Touching or
 *         @ref BoundaryContact::Crossing; see @ref BoundaryContact for how far
 *         each of the three can be trusted.
 */
template <class RedRange, class BlueRange>
BoundaryContact redBlueSweep(const RedRange& red, const BlueRange& blue) {
    using RedNumber = typename std::ranges::range_value_t<RedRange>::PointType::NumberType;
    using BlueNumber = typename std::ranges::range_value_t<BlueRange>::PointType::NumberType;
    using Number = std::common_type_t<RedNumber, BlueNumber>;
    return detail::RedBlueSweeper<Number>(red, blue).run();
}

/**
 * @brief True exactly when some red edge properly crosses some blue edge.
 *
 * A `bool`-returning shorthand for `redBlueSweep(red, blue) ==
 * BoundaryContact::Crossing`, for callers that only need that one bit and
 * would otherwise have to name @ref BoundaryContact themselves. Naming it from
 * a call site defined earlier in the include order than this header — such as
 * `PolygonWithHoles::areaContains` in implementation/contains.hpp — would
 * require the enum to already be visible there, which the library's layering
 * does not guarantee; a dependent call to this function, by contrast, is
 * resolved by argument-dependent lookup at instantiation time, so it works
 * regardless of physical include order as long as this header is reachable by
 * then (which the umbrella pgl.hpp guarantees).
 */
template <class RedRange, class BlueRange>
bool boundariesCross(const RedRange& red, const BlueRange& blue) {
    return redBlueSweep(red, blue) == BoundaryContact::Crossing;
}

/**
 * @brief True exactly when some red edge meets some blue edge, crossing or not.
 *
 * A `bool`-returning shorthand for `redBlueSweep(red, blue) !=
 * BoundaryContact::Disjoint`; see @ref boundariesCross for why this exists as
 * a dependent-call wrapper rather than a direct comparison at the call site.
 */
template <class RedRange, class BlueRange>
bool boundariesMeet(const RedRange& red, const BlueRange& blue) {
    return redBlueSweep(red, blue) != BoundaryContact::Disjoint;
}

/**
 * @brief Both @ref boundariesCross and @ref boundariesMeet, from one sweep.
 *
 * A caller that needs both bits — such as `Polygon::interiorsIntersect`,
 * which returns early on a crossing and takes a different shortcut when the
 * boundaries miss entirely — would otherwise pay for two sweeps, one per
 * wrapper. Bundling them costs one. Destructure the result with a structured
 * binding (`const auto [crossed, met] = boundaryContactBits(...)`) so the
 * call site need not name this type either, for the same reason it must not
 * name @ref BoundaryContact.
 */
struct SweepContact {
    bool crossed;
    bool met;
};

template <class RedRange, class BlueRange>
SweepContact boundaryContactBits(const RedRange& red, const BlueRange& blue) {
    const BoundaryContact contact = redBlueSweep(red, blue);
    return {contact == BoundaryContact::Crossing, contact != BoundaryContact::Disjoint};
}

/**
 * @defgroup boundary-dispatch Sweep-versus-chains dispatch
 *
 * Two ways to find out how two polygon boundaries meet, with opposite cost
 * profiles, and the single rule that picks between them.
 *
 * @ref redBlueSweep is O((n + m) log(n + m)) come what may. Testing the two
 * boundaries' maximal lexicographically monotone chains against each other
 * pairwise (see @ref Polygon::BoundaryChains) is not: it runs one merge per
 * chain pair, so it costs the *product* of the two chain counts — two chains a
 * side for a convex polygon, up to n for a comb or a star. Neither dominates:
 *
 *   - The chain test wins on near-convex input, where the product is tiny and
 *     the merges are seeded by binary search, and it wins again whenever the
 *     answer comes early, since it produces chains lazily and stops at the
 *     first hit. The sweep cannot stop early in the same sense: it builds and
 *     sorts every event before it looks at anything.
 *   - The sweep wins once the chain counts climb, by margins that reach two
 *     orders of magnitude on a 4096-vertex star.
 *
 * Absolute size matters as much as the ratio does. The sweep allocates edge and
 * event vectors and drives a `std::set`; on a pair of 32-gons that fixed
 * overhead swamps the handful of orientation tests either method needs, and the
 * chain test wins however jagged the two are. The crossover therefore moves with
 * how expensive one orientation test is: for @ref BigInt or a rational built on
 * it, a single predicate costs more than the sweep's whole bookkeeping, so the
 * method doing fewer of them pulls ahead at a much smaller size than it does for
 * `int` or `double`.
 *
 * @{
 */

/**
 * @brief Overrides the dispatch, for benchmarking one strategy in isolation.
 *
 * Define to 1 to force the chain-pair tests everywhere, 2 to force the sweep.
 * Left at its default of 0, @ref preferSweep decides per call. Only the default
 * is a supported configuration; the other two exist so a benchmark can time the
 * two strategies over the same inputs (see tests/benchmark/extra).
 */
#ifndef PGL_BOUNDARY_STRATEGY
#define PGL_BOUNDARY_STRATEGY 0
#endif

namespace detail {

/// Boundary edge count: every ring of a region, not just its outer one.
template <class Shape>
constexpr std::size_t boundaryEdgeCount(const Shape& shape) {
    if constexpr (PolygonWithHolesConcept<Shape>) {
        return shape.vertexCount();
    } else {
        return shape.size();
    }
}

/**
 * @brief The size floor half of the dispatch rule; see @ref preferSweep.
 *
 * Below this many edges the sweep's setup — two vectors, a sort and a
 * `std::set` — costs more than everything the chain test does, at any chain
 * count, so nothing past this check needs the chain counts at all. The floor
 * measured at 128 edges for native coordinates.
 *
 * It is a fixed count, not scaled down for `Rational`/`BigInt` coordinates: an
 * earlier version of this rule did scale it down, on the reasoning that an
 * expensive @ref orientationSign inflates only the chain test's work. Measured
 * against the real shape-pair benchmark cube that turned out to be wrong — the
 * sweep's own status-structure comparisons and `Segment::crosses` calls run
 * exactly as many orientation predicates, so an expensive `Number` inflates
 * both strategies by roughly the same factor and the crossover, in chain and
 * edge counts, barely moves. Scaling the floor down made the sweep get chosen
 * at sizes where it was still slower in absolute terms — e.g. `BigInt`
 * `intersects` on two 32-vertex polygons regressed 5.7x before this fix, for
 * nothing: the fixed floor alone (below) already keeps small operands on the
 * chain test regardless of `Number`, without ever reading a chain count.
 *
 * It is also checked on *each* operand separately, not their sum: an earlier
 * version summed them, which passed a tiny operand (as small as a triangle)
 * straight through to the chain-count comparison the moment the *other*
 * operand was large — and a triangle's boundary is only 2 chains, which that
 * comparison reads as "barely jagged, so sweep must be cheap here" without
 * ever noticing that those 2 chains are only a couple of vertices long.
 * A large convex polygon also has 2 chains, but each spans half its
 * vertices, and pairwise-testing two long chains against a jagged operand's
 * many short ones is genuinely expensive — chain count alone cannot tell the
 * two apart, only chain count *together with the edge count it was measured
 * on* can. Requiring both operands to individually clear the floor sidesteps
 * the distinction entirely: a triangle-sized operand never reaches the
 * chain-count comparison regardless of the other side, exactly matching
 * measurement (chain-based stayed faster for every tiny-vs-jagged pair tried,
 * up to a 4096-vertex, maximally jagged partner).
 *
 * This is checked on edge counts alone — both O(1), read straight off
 * `size()`/`vertexCount()` — specifically so a caller need not pay for
 * @ref Polygon::chainCount, an O(n) pass, on operands this check already
 * settles; that pass is not free for `Rational` or `BigInt` coordinates,
 * where a single lexicographic comparison is not O(1) in practice either.
 */
constexpr bool clearsEdgeFloor(std::size_t redEdges, std::size_t blueEdges) {
    constexpr std::size_t kMinEdges = 128;
    return redEdges >= kMinEdges && blueEdges >= kMinEdges;
}

/**
 * @brief The chain-count half of the dispatch rule; see @ref preferSweep.
 *
 * Only called once @ref clearsEdgeFloor has passed, which is what makes a
 * chain-count-only comparison safe here: both operands are already known to
 * be large enough that a low chain count means genuinely few, long chains,
 * not a small operand's short ones (see @ref clearsEdgeFloor).
 *
 * The chain test's pair count must exceed the sweep's linear term by a
 * margin, not just cross it: measured against a wider benchmark than the
 * original fit (adding operand pairs of unequal jaggedness, not just two
 * matched star polygons), the extra margin (3x rather than 2x) removed
 * several cases where the chain test was still faster despite crossing the
 * old, tighter threshold, without losing any case where the sweep was
 * genuinely faster. The reverse case (sweep chosen but chain test faster)
 * cannot be eliminated by any multiplier tried, on this benchmark or the
 * original one — it comes from asymmetric-jaggedness pairs whose boundaries
 * happen to touch (an early-exit case for the chain test's lazy production
 * that a decision based on aggregate chain and edge counts alone cannot see
 * coming) rather than from the chain-count model itself being wrong, so
 * pushing the multiplier further only trades those cases for losing correct
 * sweep calls elsewhere.
 */
constexpr bool sweepBeatsChains(std::size_t redEdges, std::size_t redChains,
                                std::size_t blueEdges, std::size_t blueChains) {
    const std::size_t edges = redEdges + blueEdges;
    return 3 * redChains * blueChains > edges;
}

}  // namespace detail

/**
 * @brief Whether @p red against @p blue is a job for @ref redBlueSweep rather
 * than for a pairwise test of their monotone boundary chains.
 *
 * The one place the library decides between the two boundary strategies, so the
 * heuristic is tuned once and every predicate built on it — `Polygon::contains`,
 * `intersects`, `interiorContains`, `interiorsIntersect`, and
 * `PolygonWithHoles`/`PolygonSet` containment through them — moves together.
 * Both operands may be a @ref Polygon or a @ref PolygonWithHoles.
 *
 * Reading the two chain counts is O(n + m) and allocation-free (see
 * @ref Polygon::chainCount), which both strategies are dominated by anyway.
 *
 * @param red First boundary.
 * @param blue Second boundary.
 * @return `true` to sweep, `false` to compare chains.
 */
template <class RedShape, class BlueShape>
constexpr bool preferSweep([[maybe_unused]] const RedShape& red,
                           [[maybe_unused]] const BlueShape& blue) {
#if PGL_BOUNDARY_STRATEGY == 1
    return false;
#elif PGL_BOUNDARY_STRATEGY == 2
    return true;
#else
    const std::size_t redEdges = detail::boundaryEdgeCount(red);
    const std::size_t blueEdges = detail::boundaryEdgeCount(blue);
    // Settled on edge counts alone — O(1), read straight off size()/vertexCount()
    // — before either chainCount() is read, so an operand pair too small to be
    // worth a sweep never pays for that O(n) pass (see clearsEdgeFloor).
    if (!detail::clearsEdgeFloor(redEdges, blueEdges)) {
        return false;
    }
    return detail::sweepBeatsChains(redEdges, red.chainCount(), blueEdges, blue.chainCount());
#endif
}

/** @} */

/**
 * @brief Sweep-based counterpart of `Polygon::contains(Polygon)`.
 *
 * Same answer as @ref Polygon::contains for two simple polygons, reached by the
 * same three-step argument — a point of @p inner inside @p outer, no crossing of
 * the boundaries, and an edge-by-edge check only when they touch — but with the
 * middle step done by @ref redBlueSweep instead of by testing the polygons'
 * lexicographically monotone chains against each other pairwise. The chain test
 * is the faster of the two whenever both boundaries decompose into few chains,
 * which is the near-convex case; its cost is the product of the two chain
 * counts, so a comb, a star or any boundary that reverses direction at most
 * vertices drives it to O(n·m), where the sweep stays at O((n + m) log(n + m)).
 *
 * The two are kept side by side deliberately: this one is the scalable
 * implementation, @ref Polygon::contains the one tuned for the common shapes.
 *
 * @param outer Candidate container.
 * @param inner Candidate containee.
 * @return `true` when @p outer contains every point of @p inner, boundary
 *         included.
 */
template <PolygonConcept OuterPolygon, PolygonConcept InnerPolygon>
bool sweepContains(const OuterPolygon& outer, const InnerPolygon& inner) {
    using InnerPoint = typename InnerPolygon::PointType;

    if (inner.size() == 0) {
        return true;
    }
    if (!outer.bbox().contains(inner.bbox())) {
        return false;
    }
    if (inner.size() == 1) {
        return outer.contains(inner[0]);
    }
    if (const auto vertex = inner.getIfPoint()) {
        return outer.contains(*vertex);
    }
    if (outer.isPoint()) {
        // `inner` has two distinct vertices by the test above.
        return false;
    }
    if (!outer.contains(inner.get(0))) {
        return false;
    }

    const BoundaryContact contact = redBlueSweep(outer.edgesView(), inner.edgesView());
    if (contact == BoundaryContact::Crossing) {
        return false;
    }
    if (contact == BoundaryContact::Disjoint) {
        // One vertex inside and no boundary contact leaves the whole of `inner`
        // inside: its boundary cannot reach out without meeting `outer`'s.
        return true;
    }

    // The boundaries meet without a crossing having been found. Whether one is
    // hiding behind the touchings no longer matters: checking every edge settles
    // containment outright.
    for (std::size_t i = 0; i < inner.size(); ++i) {
        if (!outer.contains(Segment<InnerPoint>(inner[i], inner[(i + 1) % inner.size()]))) {
            return false;
        }
    }
    return true;
}

}  // namespace pgl
