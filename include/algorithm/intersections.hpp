#pragma once

#include "algorithm/redblacktree.hpp"

/**
 * @file intersections.hpp
 * @brief Segment intersection and crossing algorithms.
 *
 * This header contains the Bentley-Ottmann sweep-line machinery together with
 * the public helpers that expose it through the Pangolin API.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>


namespace pgl::detail{

/**
 * @brief Sorts pair keys in place, by radix over the bits they actually use.
 *
 * A key packs two ranks, each below the input size, so its high bits are
 * mostly zero and a least-significant-digit radix sort needs only as many
 * passes as the largest key has digits. Over a million reported pairs that is
 * several times cheaper than a comparison sort, and the order is the same.
 */
inline void sortPairKeys(std::vector<std::uint64_t> &keys) {
    if (keys.size() < 512) {
        std::sort(keys.begin(), keys.end());
        return;
    }
    constexpr int digitBits = 11;
    constexpr std::size_t buckets = std::size_t(1) << digitBits;
    const std::uint64_t largest = *std::max_element(keys.begin(), keys.end());
    std::vector<std::uint64_t> buffer(keys.size());
    for (int shift = 0; shift < 64 && (largest >> shift) != 0; shift += digitBits) {
        std::array<std::size_t, buckets + 1> start{};
        for (const std::uint64_t key : keys) {
            ++start[((key >> shift) & (buckets - 1)) + 1];
        }
        for (std::size_t b = 1; b <= buckets; ++b) {
            start[b] += start[b - 1];
        }
        for (const std::uint64_t key : keys) {
            buffer[start[(key >> shift) & (buckets - 1)]++] = key;
        }
        keys.swap(buffer);
    }
}

/**
 * @brief Largest coordinate range at which @ref BentleyOttmann over integer
 * coordinates places its crossings in `int64_t`.
 *
 * The one quantity the sweep forms beyond an orientation is
 * `(a.x - p.x) * det + along * r.x` (see @ref BentleyOttmann::locate), a sum
 * of two products of a coordinate difference and a 2x2 determinant of them.
 * With every coordinate difference at most `D`, each determinant is at most
 * `2 D^2` and the sum at most `4 D^3`, which fits an `int64_t` up to here.
 */
inline constexpr long long sweepInt64Range = 1321122;

/**
 * @brief Largest coordinate range at which the same quantity fits an `int128`.
 *
 * Every `int` input is within it: its differences are below `2^32`.
 */
inline constexpr long long sweepInt128Range = 3490731829165;

/**
 * @brief The Bentley-Ottmann sweep over exact segments, as Boissonnat and
 * Preparata run it with predicates of degree three.
 *
 * Bentley and Ottmann order every event by its abscissa, crossings included,
 * and comparing the abscissas of two crossings is a predicate of degree five.
 * Boissonnat and Preparata ("Robust plane sweep for intersecting segments",
 * SIAM J. Comput. 29(5), 2000, section 6.1) observe that only the endpoints
 * need that order. The sweep visits the distinct endpoints in lexicographic
 * order, and a crossing found between two neighbours of the status is filed
 * under the endpoint it precedes, which takes comparing a crossing with an
 * endpoint: degree three. Before each endpoint, the crossings filed under it
 * are turned over in whatever order they come, each as it is found between
 * two neighbours: within a stretch free of endpoints, any crossing of two
 * neighbours can go first. When none is left, the status is in the order the
 * segments have just past the endpoint, and the endpoint is placed in it with
 * orientations alone. Two crossings are never compared, and no crossing is
 * ever built as a point.
 *
 * Integer coordinates evaluate that degree-three quantity in an integer type
 * chosen from their range: `int64_t` up to @ref sweepInt64Range, `int128` up
 * to @ref sweepInt128Range, and `BigInt`, behind a floating-point filter,
 * beyond. Other exact coordinates evaluate it in @p Rational, behind the same
 * filter.
 *
 * For `n` segments, `findCrossings`, `findIntersections` and
 * `findInteriorIntersections` run in `O((n + k) log n)`, `k` being the number of
 * pairs reported; `detectCrossings`, `detectIntersections`,
 * `detectInteriorIntersections`, `testPolygon` and `testPolyLine` run in
 * `O(n log n)`. Both hold for degenerate input: many segments through one
 * point, collinear overlaps, and vertical segments touching others at an end.
 * Arithmetic operations count as `O(1)`.
 *
 * @tparam Rational Exact type the crossings are placed in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Segment Segment type of the input.
 */
template <class Rational, SegmentConcept Segment>
class BentleyOttmann {
    using Point = Segment::PointType;
    using Number = Point::NumberType;
    static_assert(!std::is_floating_point_v<Number>,
                  "Bentley-Ottmann requires exact (non-floating-point) input "
                  "coordinates; the sweep line's predicates are not robust under "
                  "rounding. Use integer or rational coordinates.");
    using CrossingPair = std::array<Segment,2>;

    // Integer coordinates place crossings in an integer type of their own
    // choosing, by range; see @ref chooseWidth.
    static constexpr bool wholeCoordinates =
        pgl::detail::extended_integral<Number> || std::same_as<Number, pgl::BigInt>;
    // A type holding a coordinate and the difference of any two exactly:
    // built-in integers of up to 64 bits promote once, and anything else goes
    // through BigInt.
    using Range = std::conditional_t<wholeCoordinates && std::signed_integral<Number> &&
                                         (sizeof(Number) <= 8),
                                     pgl::detail::promoted_number_t<Number>, pgl::BigInt>;
    // The type the endpoint filters are kept for, as the orientations read them.
    using Coordinate = pgl::detail::promoted_number_t<Number>;

    // ── Segments by number ──────────────────────────────────────────────────
    //
    // The sweep never holds a segment. The status tree, the filed crossings
    // and the reported pairs all name one by an `Id`, its position in the
    // caller's vector, and read it through @ref seg. A segment is small for
    // `int` coordinates, but over exact fractions it is eight arbitrary-
    // precision integers, and holding it by value made every tree node and
    // pair a copy of those.
    //
    // A pair is named by a `Key`, the two segments' ranks packed into one
    // integer. The rank is a segment's place in the value order of the input,
    // with equal segments sharing one, so ordering the keys orders the pairs
    // exactly as sorting the pairs themselves would, and one sort of the input
    // at the start replaces a sort of everything reported at the end.
    using Id = std::uint32_t;
    using Rank = std::uint32_t;
    using Key = std::uint64_t;

    // The caller's segments, which outlive the sweep and stay where they are.
    const std::vector<Segment> *input = nullptr;

    const Segment &seg(Id id) const { return (*input)[id]; }

    // Each segment's rank, and one Id per rank. Duplicated values share a rank
    // and only the first of them is swept: the status tree could not hold the
    // second anyway, since the two compare equal. @ref duplicated lists the
    // ranks that had more than one.
    std::vector<Rank> rank;
    std::vector<Id> byRank;
    std::vector<Rank> duplicated;
    // Every copy of each rank, in input order, as the stretch
    // `copies[copiesStart[r]]` up to `copies[copiesStart[r + 1]]`; see
    // @ref pairsOf, which is the only reader. Left empty when no value repeats,
    // which is the common case, and then each rank is its Id in @ref byRank.
    std::vector<Id> copies;
    std::vector<std::uint32_t> copiesStart;

    Key keyOf(Id a, Id b) const {
        const Rank ra = rank[a], rb = rank[b];
        return ra < rb ? (Key(ra) << 32) | rb : (Key(rb) << 32) | ra;
    }

    // ── Exact arithmetic ────────────────────────────────────────────────────

    /** @brief The type crossings are placed in; see @ref locate. */
    enum class Width : unsigned char { int64, int128, unbounded };
    Width width = Width::unbounded;
    // Under a machine-word width, each segment's coordinates as offsets from
    // the lowest ones, `{min.x, min.y, max.x, max.y}` by Id, and the lowest
    // ones themselves; see @ref chooseWidth.
    std::vector<std::array<std::int64_t, 4>> offsets;
    Range originX{}, originY{};

    using FilteredEnd = decltype(pgl::detail::filtered<Coordinate>(std::declval<const Point&>()));
    // Each segment's two endpoints, filtered, by Id; see @ref prepare. They
    // refer to the segments where they stand, in the input.
    struct Ends {
        FilteredEnd lo, hi;
    };
    std::vector<Ends> endsOf;
    // Whether each segment is vertical, horizontal, both (a point) or
    // neither, by Id; see @ref locateIn.
    static constexpr unsigned char vertical = 1, horizontal = 2;
    std::vector<unsigned char> axisParallel;

    static int orientation(const FilteredEnd &a, const FilteredEnd &b, const FilteredEnd &c) {
        return pgl::detail::signOf(pgl::detail::orientationSignOf(a, b, c).value());
    }

    // ── The endpoints ───────────────────────────────────────────────────────
    //
    // What happens at an endpoint, in the order a point's events are taken:
    // the segments ending there leave the status, segments of no length are
    // looked up in it, and the segments starting there enter it.
    enum class Kind : unsigned char { right, point, left };
    struct Event {
        Id id;
        Kind kind;
    };
    const Point &pointOf(const Event &e) const {
        return e.kind == Kind::right ? seg(e.id).max() : seg(e.id).min();
    }
    // Every event, by point and then by kind, and the distinct points: point
    // k's events are `events[pointStart[k]]` up to `events[pointStart[k + 1]]`.
    std::vector<Event> events;
    std::vector<const Point *> points;
    std::vector<std::uint32_t> pointStart;
    // Each point's abscissa in double, as an offset under a machine-word
    // width, where the search placing a crossing among the points starts;
    // and, where that placement filters, both coordinates with their error
    // bounds.
    std::vector<double> guessX;
    std::vector<pgl::detail::ApproximatePoint> pointApproximations;
    // Under a machine-word width, each point's coordinates as offsets.
    std::vector<std::array<std::int64_t, 2>> pointOffsets;

    // ── The status ──────────────────────────────────────────────────────────
    //
    // The segments the sweep line meets, bottom to top. The tree is only ever
    // searched at an endpoint, when the status is exactly the order just past
    // it, and only for where that endpoint falls: the segment starting there
    // (@ref inserting), or the point itself (a @ref PointProbe). Everything
    // else that moves a segment knows where it is.

    struct PointProbe {};

    struct AlongLine {
        const BentleyOttmann *sweep;
        bool operator()(Id a, Id b) const { return sweep->insertionLess(a, b); }
        bool operator()(Id a, PointProbe) const { return sweep->pointSide(a) > 0; }
        bool operator()(PointProbe, Id b) const { return sweep->pointSide(b) < 0; }
    };
    using Tree = pgl::detail::RedBlackTree<Id, AlongLine>;
    using Node = typename Tree::Handle;

    Tree tree{AlongLine{this}};
    // Where each segment of the status is, by Id, or null. A crossing names its
    // two segments, and a segment ending names itself, so neither searches.
    std::vector<Node> nodeOf;
    // The segment being inserted, and the point being looked up.
    Id inserting = 0;
    FilteredEnd probe{};

    // Whether the point @ref probe lies above (1), on (0) or below (-1) the
    // line of the status segment @p id, which spans it.
    int pointSide(Id id) const {
        return orientation(endsOf[id].lo, endsOf[id].hi, probe);
    }

    // Whether the segment being inserted at its left endpoint lies below the
    // status segment @p b just past that endpoint. Segments through it go by
    // the direction they leave in, a vertical one leaving straight up, and
    // collinear ones by rank.
    bool insertedBelow(Id b) const {
        const Ends &s = endsOf[inserting];
        const Ends &other = endsOf[b];
        int side = orientation(other.lo, other.hi, s.lo);
        if (side == 0) {
            side = orientation(other.lo, other.hi, s.hi);
        }
        return side == 0 ? rank[inserting] < rank[b] : side < 0;
    }

    bool insertionLess(Id a, Id b) const {
        if (a == b) {
            return false;
        }
        if (a == inserting) {
            return insertedBelow(b);
        }
        PGL_ASSERT(b == inserting && "the status only compares a segment going in");
        return !insertedBelow(a);
    }

    // ── Filed crossings ─────────────────────────────────────────────────────
    //
    // A crossing of two neighbours, lower one first, filed under the point it
    // is to be turned over before. Each point's crossings form a list through
    // @ref filed, the head in @ref firstFiled. Nothing is unfiled when its two
    // segments stop being neighbours: a record is checked as it comes off the
    // list, and dropped if they no longer are. A pair crosses once, so two
    // records of one pair never both pass that check.
    struct Filed {
        Id lower, upper;
        std::int32_t next;
    };
    std::vector<Filed> filed;
    std::vector<std::int32_t> firstFiled;
    // The earliest point a crossing filed now can be turned over before: the
    // current one while its segments ending there leave, the next one once
    // those starting there enter. A crossing at the current point itself is
    // found only once a segment ending there has left from between its two,
    // and must still be turned over before anything starts there.
    std::size_t earliest = 0;

    // The pairs found, put in order once, at the end.
    std::vector<Key> crossingKeys;
    std::vector<Key> intersectionKeys;

    // Scratch for @ref reportThrough.
    std::vector<Id> through;

    // The keys in order, each once.
    static std::vector<Key> ordered(std::vector<Key> keys) {
        pgl::detail::sortPairKeys(keys);
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        return keys;
    }

    // Calls `visit(i, j)` with the input positions of the pairs of segments
    // the ordered keys name, in their order.
    //
    // A key names two values, and a value given k times stands for k input
    // segments, so a key between two values stands for every pairing of their
    // copies and a key of a value with itself for every pair of its copies.
    // That is what the brute-force scan reports, one pair per two positions of
    // the input, and each copy comes back as itself, label included.
    template <class Visit>
    void forEachPair(const std::vector<Key> &keys, Visit visit) const {
        for (const Key key : keys) {
            const Rank ra = static_cast<Rank>(key >> 32);
            const Rank rb = static_cast<Rank>(key & 0xffffffffu);
            if (copies.empty()) {
                if (ra != rb) {
                    visit(byRank[ra], byRank[rb]);
                }
                continue;
            }
            for (std::uint32_t i = copiesStart[ra]; i < copiesStart[ra + 1]; ++i) {
                for (std::uint32_t j = ra == rb ? i + 1 : copiesStart[rb];
                     j < copiesStart[rb + 1]; ++j) {
                    visit(copies[i], copies[j]);
                }
            }
        }
    }

    std::vector<CrossingPair> pairsOf(const std::vector<Key> &keys) const {
        std::vector<CrossingPair> pairs;
        pairs.reserve(keys.size());
        forEachPair(keys, [this, &pairs](Id a, Id b) { pairs.push_back({seg(a), seg(b)}); });
        return pairs;
    }

    // Called with every pair as it is found, the lesser segment first; a true
    // return stops the sweep. Empty unless an entry point needs one.
    std::function<bool(const Segment&, const Segment&)> onCrossing, onIntersection;
    bool onlyCrossings = true;
    // With onlyCrossings: also report every pair overlapping collinearly along
    // a stretch of positive length, which with the crossings are exactly the
    // pairs whose relative interiors meet.
    bool interiorsOnly = false;
    // When nonzero, a point that more segments end at than this stops the
    // sweep and sets @ref crowded: what the simplicity tests ask of a vertex.
    std::size_t endpointLimit = 0;
    bool crowded = false;
    bool stopNow = false;

    bool addCrossing(Id a, Id b) {
        if (rank[b] < rank[a]) std::swap(a, b);
        crossingKeys.push_back(keyOf(a, b));
        if (onCrossing && onCrossing(seg(a), seg(b))) {
            stopNow = true;
            return true;
        }

        return false;
    }

    bool addIntersection(Id a, Id b) {
        if (rank[b] < rank[a]) std::swap(a, b);
        intersectionKeys.push_back(keyOf(a, b));
        if (onIntersection && onIntersection(seg(a), seg(b))) {
            stopNow = true;
            return true;
        }

        return false;
    }

    /**
     * @brief Ranks the input and sets up everything indexed by Id.
     *
     * Sorting Ids by segment value is the one place the sweep compares
     * segments as values; after it, equality is a comparison of ranks.
     */
    void prepare(const std::vector<Segment> &segments) {
        PGL_ASSERT(segments.size() <= std::numeric_limits<Id>::max());
        input = &segments;
        const std::size_t count = segments.size();

        std::vector<Id> order(count);
        std::iota(order.begin(), order.end(), Id(0));
        std::sort(order.begin(), order.end(),
                  [&segments](Id a, Id b) { return segments[a] < segments[b]; });
        rank.assign(count, 0);
        byRank.clear();
        duplicated.clear();
        for (const Id id : order) {
            if (!byRank.empty() && segments[byRank.back()] == segments[id]) {
                const Rank shared = static_cast<Rank>(byRank.size() - 1);
                if (duplicated.empty() || duplicated.back() != shared) {
                    duplicated.push_back(shared);
                }
                rank[id] = shared;
                continue;
            }
            rank[id] = static_cast<Rank>(byRank.size());
            byRank.push_back(id);
        }
        copies.clear();
        copiesStart.clear();
        if (!duplicated.empty()) {
            // `order` is already every copy, grouped by rank.
            copies = std::move(order);
            copiesStart.reserve(byRank.size() + 1);
            for (std::uint32_t i = 0; i < copies.size(); ++i) {
                if (i == 0 || rank[copies[i]] != rank[copies[i - 1]]) {
                    copiesStart.push_back(i);
                }
            }
            copiesStart.push_back(static_cast<std::uint32_t>(copies.size()));
            // Each value's copies in input order, so they come back in the
            // order the caller gave them. Only here, and not as a tie-break in
            // the sort above, which every input would pay for.
            for (const Rank r : duplicated) {
                std::sort(copies.begin() + copiesStart[r], copies.begin() + copiesStart[r + 1]);
            }
        }

        // Every endpoint approximated once, for all the orientations that will
        // read it.
        endsOf.assign(count, Ends{});
        axisParallel.assign(count, 0);
        for (const Id id : byRank) {
            const Segment &s = seg(id);
            endsOf[id] = {pgl::detail::filtered<Coordinate>(s.min()),
                          pgl::detail::filtered<Coordinate>(s.max())};
            axisParallel[id] = static_cast<unsigned char>(
                (s.min().x() == s.max().x() ? vertical : 0) |
                (s.min().y() == s.max().y() ? horizontal : 0));
        }
        nodeOf.assign(count, nullptr);
    }

    /**
     * @brief Chooses the type crossings are placed in, from the coordinate
     * range of the input: the narrowest of `int64_t`, `int128` and an
     * unbounded type that holds every quantity @ref locate forms.
     *
     * Only integer coordinates have a choice. Where it is a machine word,
     * every coordinate is written down once as its offset from the lowest one,
     * which fits an `int64_t` within either range, so that placing a crossing
     * reads machine words and converts nothing.
     */
    void chooseWidth() {
        width = Width::unbounded;
        if constexpr (wholeCoordinates) {
            const Point &first = seg(byRank.front()).min();
            const Number *minX = &first.x(), *maxX = &first.x();
            const Number *minY = &first.y(), *maxY = &first.y();
            for (const Id id : byRank) {
                for (const Point *p : {&seg(id).min(), &seg(id).max()}) {
                    if (p->x() < *minX) minX = &p->x();
                    if (*maxX < p->x()) maxX = &p->x();
                    if (p->y() < *minY) minY = &p->y();
                    if (*maxY < p->y()) maxY = &p->y();
                }
            }
            originX = rangeOf(*minX);
            originY = rangeOf(*minY);
            const Range spanX = rangeOf(*maxX) - originX;
            const Range spanY = rangeOf(*maxY) - originY;
            const Range span = spanX < spanY ? spanY : spanX;
            if (static_cast<Range>(sweepInt128Range) < span) {
                return;
            }
            width = static_cast<Range>(sweepInt64Range) < span ? Width::int128 : Width::int64;
            offsets.resize(input->size());
            for (const Id id : byRank) {
                const Segment &s = seg(id);
                offsets[id] = {offsetOf(s.min().x(), false), offsetOf(s.min().y(), true),
                               offsetOf(s.max().x(), false), offsetOf(s.max().y(), true)};
            }
        }
    }

    // A coordinate in @ref Range, which holds it and any difference of two.
    static Range rangeOf(const Number &value) {
        return static_cast<Range>(value);
    }

    std::int64_t offsetOf(const Number &value, bool alongY) const {
        return static_cast<std::int64_t>(rangeOf(value) - (alongY ? originY : originX));
    }

    /**
     * @brief Lists the events by point, and the distinct points in
     * lexicographic order.
     *
     * At a point, segments end before any starts. A segment of no length is
     * never in the status: it is only an intersection to look up at its
     * point, so a crossing search leaves it out.
     */
    void listEvents() {
        events.clear();
        events.reserve(2 * byRank.size());
        for (const Id id : byRank) {
            if (seg(id).isDegenerate()) {
                if (!onlyCrossings) {
                    events.push_back({id, Kind::point});
                }
                continue;
            }
            events.push_back({id, Kind::right});
            events.push_back({id, Kind::left});
        }
        std::sort(events.begin(), events.end(), [this](const Event &e, const Event &f) {
            const Point &p = pointOf(e), &q = pointOf(f);
            if (p < q) return true;
            if (q < p) return false;
            return e.kind < f.kind;
        });
        points.clear();
        pointStart.clear();
        for (std::size_t i = 0; i < events.size(); ++i) {
            if (i == 0 || pointOf(events[i]) != *points.back()) {
                points.push_back(&pointOf(events[i]));
                pointStart.push_back(static_cast<std::uint32_t>(i));
            }
        }
        pointStart.push_back(static_cast<std::uint32_t>(events.size()));

        guessX.resize(points.size());
        pointApproximations.clear();
        pointOffsets.clear();
        bool words = false;
        if constexpr (wholeCoordinates) {
            words = width != Width::unbounded;
            if (words) {
                pointOffsets.reserve(points.size());
                for (std::size_t k = 0; k < points.size(); ++k) {
                    pointOffsets.push_back(
                        {offsetOf(points[k]->x(), false), offsetOf(points[k]->y(), true)});
                    guessX[k] = static_cast<double>(pointOffsets[k][0]);
                }
            }
        }
        if (!words) {
            for (std::size_t k = 0; k < points.size(); ++k) {
                guessX[k] = pgl::detail::approximate(points[k]->x()).value;
            }
            if (filtersPlacement()) {
                pointApproximations.reserve(points.size());
                for (const Point *p : points) {
                    pointApproximations.push_back(pgl::detail::approximatePoint(*p));
                }
            }
        }
        filed.clear();
        firstFiled.assign(points.size() + 1, -1);
    }

    // ── Placing a crossing ──────────────────────────────────────────────────
    //
    // Two segments `a`, below, and `b`, above, crossing ahead: `a` from `a0`
    // along `r`, `b` from `b0` along `s`. They meet at
    // `a0 + r * along / det`, with `det = r x s` and `along = (b0 - a0) x s`.
    // Since `b` comes down through `a`, `det` is negative, and the crossing's
    // abscissa lies past a point `p`'s exactly when
    // `(a0.x - p.x) * det + along * r.x` is negative; the same over the
    // ordinates. That is a product of a coordinate difference and a 2x2
    // determinant of them, plus another: degree three, and nothing is divided.

    template <class Exact>
    struct Terms {
        Exact det, along, rx, ry;
    };
    struct ApproximateTerms {
        pgl::detail::ApproximatePoint origin;
        pgl::detail::Approximate det, along, rx, ry;
    };

    // Whether placing a crossing in the unbounded type tries the filter first.
    bool filtersPlacement() const {
        if constexpr (wholeCoordinates) {
            return true;
        } else {
            return pgl::detail::filtersSign<Rational>;
        }
    }

    template <class Exact>
    static Exact difference(const Number &a, const Number &b) {
        if constexpr (std::same_as<Exact, Number>) {
            return a - b;
        } else if constexpr (wholeCoordinates) {
            return static_cast<Exact>(a) - static_cast<Exact>(b);
        } else {
            return pgl::detail::asNumber<Exact>(a) - pgl::detail::asNumber<Exact>(b);
        }
    }

    template <class Exact>
    static int negatedSign(const Exact &value) {
        return value > Exact(0) ? -1 : (value < Exact(0) ? 1 : 0);
    }

    template <class Exact>
    Terms<Exact> termsOf(Id ia, Id ib) const {
        const Segment &a = seg(ia), &b = seg(ib);
        const Exact rx = difference<Exact>(a.max().x(), a.min().x());
        const Exact ry = difference<Exact>(a.max().y(), a.min().y());
        const Exact sx = difference<Exact>(b.max().x(), b.min().x());
        const Exact sy = difference<Exact>(b.max().y(), b.min().y());
        const Exact ox = difference<Exact>(b.min().x(), a.min().x());
        const Exact oy = difference<Exact>(b.min().y(), a.min().y());
        return {rx * sy - ry * sx, ox * sy - oy * sx, rx, ry};
    }

    ApproximateTerms approximateTermsOf(Id ia, Id ib) const {
        const pgl::detail::ApproximatePoint a0 = pgl::detail::approximationOf(endsOf[ia].lo);
        const pgl::detail::ApproximatePoint a1 = pgl::detail::approximationOf(endsOf[ia].hi);
        const pgl::detail::ApproximatePoint b0 = pgl::detail::approximationOf(endsOf[ib].lo);
        const pgl::detail::ApproximatePoint b1 = pgl::detail::approximationOf(endsOf[ib].hi);
        const auto rx = a1.x - a0.x, ry = a1.y - a0.y;
        const auto sx = b1.x - b0.x, sy = b1.y - b0.y;
        const auto ox = b0.x - a0.x, oy = b0.y - a0.y;
        return {a0, rx * sy - ry * sx, ox * sy - oy * sx, rx, ry};
    }

    /**
     * @brief The point crossing @p ia (below) and @p ib (above) is to be
     * turned over before: the first point it does not lie lexicographically
     * past.
     */
    std::size_t locate(Id ia, Id ib) {
        if constexpr (wholeCoordinates) {
            switch (width) {
            case Width::int64: return locateInWord<std::int64_t>(ia, ib);
            case Width::int128: return locateInWord<pgl::int128>(ia, ib);
            case Width::unbounded: break;
            }
            return locateIn<pgl::BigInt>(ia, ib);
        } else {
            return locateIn<Rational>(ia, ib);
        }
    }

    /**
     * @brief The first point @p notPast holds at, which it does at the last.
     *
     * The search starts from the point at abscissa @p guess and gallops out
     * from it, so a good guess costs a few placements and a bad one
     * `O(log n)`.
     */
    template <class NotPast>
    std::size_t firstNotPast(double guess, NotPast notPast) const {
        // The crossing is interior to both segments, so it comes before the
        // last point: the answer is in range and the last point needs no test.
        const std::size_t last = points.size() - 1;
        std::size_t start = static_cast<std::size_t>(
            std::lower_bound(guessX.begin(), guessX.end(), guess) - guessX.begin());
        start = std::min(start, last);
        // notPast is false below lo and true at hi.
        std::size_t lo, hi;
        if (notPast(start)) {
            hi = start;
            for (std::size_t step = 1;; step *= 2) {
                if (hi < step) {
                    lo = 0;
                    break;
                }
                if (!notPast(hi - step)) {
                    lo = hi - step + 1;
                    break;
                }
                hi -= step;
            }
        } else {
            lo = start + 1;
            for (std::size_t step = 1;; step *= 2) {
                const std::size_t probe = lo - 1 + step;
                if (probe >= last) {
                    hi = last;
                    break;
                }
                if (notPast(probe)) {
                    hi = probe;
                    break;
                }
                lo = probe + 1;
            }
        }
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (notPast(mid)) {
                hi = mid;
            } else {
                lo = mid + 1;
            }
        }
        return lo;
    }

    // @ref locate over the coordinates' offsets, in a machine word that holds
    // everything it forms from them; see @ref chooseWidth.
    template <class Word>
    std::size_t locateInWord(Id ia, Id ib) const {
        const std::array<std::int64_t, 4> &a = offsets[ia], &b = offsets[ib];
        const Word rx = Word(a[2]) - Word(a[0]), ry = Word(a[3]) - Word(a[1]);
        const Word sx = Word(b[2]) - Word(b[0]), sy = Word(b[3]) - Word(b[1]);
        const Word ox = Word(b[0]) - Word(a[0]), oy = Word(b[1]) - Word(a[1]);
        const Word det = rx * sy - ry * sx;
        const Word along = ox * sy - oy * sx;
        const double guess = static_cast<double>(a[0]) + static_cast<double>(along) *
                                                             static_cast<double>(rx) /
                                                             static_cast<double>(det);
        return firstNotPast(guess, [&](std::size_t k) {
            const std::array<std::int64_t, 2> &p = pointOffsets[k];
            const int x = negatedSign((Word(a[0]) - Word(p[0])) * det + along * rx);
            return x != 0 ? x < 0 : negatedSign((Word(a[1]) - Word(p[1])) * det + along * ry) <= 0;
        });
    }

    // @ref locate in an unbounded type, filtered where that type is costly.
    template <class Exact>
    std::size_t locateIn(Id ia, Id ib) {
        const Segment &a = seg(ia);
        constexpr bool filters = pgl::detail::filtersSign<Exact>;
        std::optional<Terms<Exact>> exact;
        ApproximateTerms approximate{};
        double guess;
        // Over a fraction type, the crossing's coordinates themselves, built
        // the first time the filter cannot tell: a crossing it cannot place is
        // nearly always one tied with a point, and every probe of the search
        // then meets the tie again, which a comparison of two fractions settles
        // for less than the fraction products of the polynomial below.
        std::optional<Exact> crossX, crossY;
        // A crossing with a vertical segment has that segment's abscissa, and
        // one with a horizontal segment its ordinate. Those are the ties
        // axis-parallel input is made of, and they need no arithmetic at all.
        const Segment &b = seg(ib);
        const Number *knownX = (axisParallel[ia] & vertical)   ? &a.min().x()
                               : (axisParallel[ib] & vertical) ? &b.min().x()
                                                               : nullptr;
        const Number *knownY = (axisParallel[ia] & horizontal)   ? &a.min().y()
                               : (axisParallel[ib] & horizontal) ? &b.min().y()
                                                                 : nullptr;
        if constexpr (filters) {
            approximate = approximateTermsOf(ia, ib);
            guess = approximate.origin.x.value +
                    approximate.along.value * approximate.rx.value / approximate.det.value;
        } else {
            exact.emplace(termsOf<Exact>(ia, ib));
            guess = pgl::detail::approximate(a.min().x()).value +
                    static_cast<double>(exact->along) * static_cast<double>(exact->rx) /
                        static_cast<double>(exact->det);
        }

        // The sign of the crossing's coordinate minus point k's.
        const auto side = [&](std::size_t k, bool alongY) {
            if constexpr (filters) {
                const pgl::detail::ApproximatePoint &p = pointApproximations[k];
                const pgl::detail::Approximate offset =
                    alongY ? approximate.origin.y - p.y : approximate.origin.x - p.x;
                const std::partial_ordering sign = pgl::detail::approximateSign(
                    offset * approximate.det +
                    approximate.along * (alongY ? approximate.ry : approximate.rx));
                if (sign != std::partial_ordering::unordered) {
                    return sign > 0 ? -1 : 1;
                }
                if (const Number *known = alongY ? knownY : knownX) {
                    const Number &pc = alongY ? points[k]->y() : points[k]->x();
                    return *known < pc ? -1 : (pc < *known ? 1 : 0);
                }
                if (!exact) {
                    exact.emplace(termsOf<Exact>(ia, ib));
                }
            }
            if constexpr (pgl::is_Rational_v<Exact>) {
                std::optional<Exact> &cross = alongY ? crossY : crossX;
                if (!cross) {
                    const Number &ac = alongY ? a.min().y() : a.min().x();
                    cross.emplace(pgl::detail::asNumber<Exact>(ac) +
                                  exact->along * (alongY ? exact->ry : exact->rx) / exact->det);
                }
                const auto &pc = pgl::detail::asNumber<Exact>(alongY ? points[k]->y() : points[k]->x());
                return *cross < pc ? -1 : (pc < *cross ? 1 : 0);
            }
            const Exact offset = alongY ? difference<Exact>(a.min().y(), points[k]->y())
                                        : difference<Exact>(a.min().x(), points[k]->x());
            return negatedSign(offset * exact->det +
                               exact->along * (alongY ? exact->ry : exact->rx));
        };
        return firstNotPast(guess, [&side](std::size_t k) {
            const int x = side(k, false);
            return x != 0 ? x < 0 : side(k, true) <= 0;
        });
    }

    /**
     * @brief Files the crossing of two neighbours of the status, @p lower
     * below @p upper, if they cross ahead.
     *
     * They do exactly when the upper one starts above the lower one's line and
     * ends below it, and the lower one starts below the upper one's line and
     * ends above it: four orientations over the input. Neighbours that have
     * crossed already are in the other order, and fail the first two.
     */
    void consider(Node lower, Node upper) {
        if (!lower || !upper) {
            return;
        }
        const Id a = lower->value, b = upper->value;
        const Ends &ea = endsOf[a], &eb = endsOf[b];
        if (orientation(ea.lo, ea.hi, eb.hi) >= 0 || orientation(ea.lo, ea.hi, eb.lo) <= 0 ||
            orientation(eb.lo, eb.hi, ea.lo) >= 0 || orientation(eb.lo, eb.hi, ea.hi) <= 0) {
            return;
        }
        const std::size_t k = std::max(locate(a, b), earliest);
        filed.push_back({a, b, firstFiled[k]});
        firstFiled[k] = static_cast<std::int32_t>(filed.size() - 1);
    }

    // Turns over every crossing filed under point k, and every one found in
    // doing so that belongs there too.
    void drain(std::size_t k) {
        while (firstFiled[k] >= 0 && !stopNow) {
            const Filed f = filed[static_cast<std::size_t>(firstFiled[k])];
            firstFiled[k] = f.next;
            const Node lower = nodeOf[f.lower], upper = nodeOf[f.upper];
            if (!lower || !upper || Tree::next(lower) != upper) {
                continue;  // no longer neighbours, so turned over already or not yet due
            }
            if (addCrossing(f.lower, f.upper)) {
                return;
            }
            // The two exchange places in the tree, keeping their nodes.
            tree.swap(lower, upper);
            consider(Tree::prev(upper), upper);
            consider(lower, Tree::next(lower));
        }
    }

    void removeSegment(Id id) {
        const Node node = nodeOf[id];
        PGL_ASSERT(node && "a segment ends that the status does not hold");
        const Node below = Tree::prev(node), above = Tree::next(node);
        nodeOf[id] = nullptr;
        tree.erase(node);
        consider(below, above);
    }

    void insertSegment(Id id) {
        inserting = id;
        const Node node = tree.insert(id).first;
        nodeOf[id] = node;
        consider(Tree::prev(node), node);
        consider(node, Tree::next(node));
        if (interiorsOnly) {
            // A segment overlapping this one along a stretch of positive
            // length is collinear with it and passes through its left
            // endpoint, or starts there too. The status orders the segments
            // through one point by the direction they leave in, so those
            // collinear with this one are its neighbours on either side up to
            // the first that is not, and nothing else needs asking.
            const Ends &ends = endsOf[id];
            const auto collinear = [this, &ends](Node it) {
                if (!it) {
                    return false;
                }
                const Ends &other = endsOf[it->value];
                return orientation(ends.lo, ends.hi, other.lo) == 0 &&
                       orientation(ends.lo, ends.hi, other.hi) == 0;
            };
            for (Node it = Tree::prev(node); collinear(it); it = Tree::prev(it)) {
                if (addIntersection(it->value, id)) {
                    return;
                }
            }
            for (Node it = Tree::next(node); collinear(it); it = Tree::next(it)) {
                if (addIntersection(it->value, id)) {
                    return;
                }
            }
        }
    }

    // Reports every segment with an endpoint at point k against every segment
    // of the status passing through that point, which is then in the interior
    // of the latter. Those are one stretch of the status, found by one search.
    // `O(log n)` plus one step per pair reported.
    void reportThrough(std::size_t k) {
        if (tree.empty()) {
            return;
        }
        probe = pgl::detail::filtered<Coordinate>(*points[k]);
        through.clear();
        for (Node it = tree.lowerBound(PointProbe{}); it && pointSide(it->value) == 0;
             it = Tree::next(it)) {
            through.push_back(it->value);
        }
        for (std::uint32_t e = pointStart[k]; e < pointStart[k + 1]; ++e) {
            for (const Id other : through) {
                if (addIntersection(events[e].id, other)) {
                    return;
                }
            }
        }
    }

    // Everything that happens at point k; see the class description.
    void processPoint(std::size_t k) {
        const std::uint32_t first = pointStart[k], last = pointStart[k + 1];
        earliest = k;
        drain(k);
        std::uint32_t e = first;
        for (; e < last && events[e].kind == Kind::right; ++e) {
            removeSegment(events[e].id);
        }
        // Crossings at this very point between segments that one ending here
        // kept apart.
        drain(k);
        if (stopNow) {
            return;
        }
        if (endpointLimit != 0) {
            std::size_t ends = 0;
            for (std::uint32_t i = first; i < last; ++i) {
                ends += events[i].kind != Kind::point ? 1 : 0;
            }
            if (ends > endpointLimit) {
                crowded = true;
                stopNow = true;
                return;
            }
        }
        if (!onlyCrossings) {
            reportThrough(k);
            if (stopNow) {
                return;
            }
        }
        earliest = k + 1;
        for (; e < last; ++e) {
            if (events[e].kind == Kind::left) {
                insertSegment(events[e].id);
                if (stopNow) {
                    return;
                }
            }
        }
    }

    void run(const std::vector<Segment> &segments) {
        if (segments.empty())
            return;

        prepare(segments);
        // A segment given twice meets itself, as an intersection and not as a
        // crossing; only one of the two is swept.
        if (!onlyCrossings) {
            for (const Rank r : duplicated) {
                if (addIntersection(byRank[r], byRank[r])) {
                    return;
                }
            }
        } else if (interiorsOnly) {
            // Two copies of a segment with length share their whole interior;
            // a point has none.
            for (const Rank r : duplicated) {
                if (!seg(byRank[r]).isDegenerate() && addIntersection(byRank[r], byRank[r])) {
                    return;
                }
            }
        }
        // Nothing to sweep for a single distinct segment
        if (byRank.size() <= 1)
            return;

        chooseWidth();
        listEvents();
        tree.clear();
        for (std::size_t k = 0; k < points.size() && !stopNow; ++k) {
            processPoint(k);
        }
    }

    // Whether the segments only meet as the edges of a simple polygon or
    // polyline: nothing crosses, no segment ends inside another, none is given
    // twice, and no point is the end of more than two.
    bool testSimple(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        endpointLimit = 2;
        bool notSimple = false;
        onCrossing = [&notSimple](const Segment &, const Segment &) {
            notSimple = true;
            return true;
        };
        onIntersection = onCrossing;
        run(segments);
        return !notSimple && !crowded;
    }

    // Every pair that intersects, as ordered keys. Besides what the sweep
    // reports, that is the segments sharing an endpoint, which the events
    // already stand grouped by: every two at one point. A segment of no length
    // is there once, and a duplicate was already reported with itself.
    std::vector<Key> intersectingKeys(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        run(segments);
        std::vector<Key> keys = std::move(intersectionKeys);
        keys.insert(keys.end(), crossingKeys.begin(), crossingKeys.end());
        for (std::size_t k = 0; k + 1 < pointStart.size(); ++k) {
            for (std::uint32_t i = pointStart[k]; i + 1 < pointStart[k + 1]; ++i) {
                for (std::uint32_t j = i + 1; j < pointStart[k + 1]; ++j) {
                    keys.push_back(keyOf(events[i].id, events[j].id));
                }
            }
        }
        return ordered(std::move(keys));
    }

public:
    std::vector<CrossingPair> findCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        run(segments);
        return pairsOf(ordered(std::move(crossingKeys)));
    }

    std::vector<CrossingPair> findIntersections(const std::vector<Segment> &segments) {
        return pairsOf(intersectingKeys(segments));
    }

    /**
     * @brief Calls `visit(i, j)` for every intersecting pair, with the two
     * segments' positions in @p segments, in @ref findIntersections' order.
     *
     * For a caller that wants the pairs by position: it spares copying them
     * out and looking them up again.
     */
    template <class Visit>
    void visitIntersections(const std::vector<Segment> &segments, Visit visit) {
        forEachPair(intersectingKeys(segments), [&visit](Id a, Id b) {
            visit(static_cast<std::size_t>(a), static_cast<std::size_t>(b));
        });
    }

    std::vector<CrossingPair> findInteriorIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        interiorsOnly = true;
        run(segments);
        intersectionKeys.insert(intersectionKeys.end(), crossingKeys.begin(), crossingKeys.end());
        return pairsOf(ordered(std::move(intersectionKeys)));
    }

    bool detectInteriorIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        interiorsOnly = true;
        onCrossing = [] (const Segment &, const Segment &) {return true;};
        onIntersection = [] (const Segment &, const Segment &) {return true;};
        run(segments);
        return !crossingKeys.empty() || !intersectionKeys.empty();
    }

    bool detectCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        onCrossing = [] (const Segment &, const Segment &) {return true;};
        run(segments);
        return !crossingKeys.empty();
    }

    bool detectIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        onCrossing = [] (const Segment &, const Segment &) {return true;};
        onIntersection = [] (const Segment &, const Segment &) {return true;};
        run(segments);

        if (!crossingKeys.empty() || !intersectionKeys.empty())
            return true;

        // Insert segments sharing an endpoint
        std::set<Point> adjacent;
        for (const Segment &s : segments) {
            auto [_1,b1] = adjacent.insert(s.min());
            if (!b1)
                return true;
            if (s.isDegenerate())
                continue; // one point, already inserted: not a second segment
            auto [_2,b2] = adjacent.insert(s.max());
            if (!b2)
                return true;
        }

        return false;
    }

    // Tests whether the segments form a simple polygon: every vertex appears in
    // exactly 2 segments, and the only intersections are those shared vertices.
    bool testPolygon(const std::vector<Segment> &segments) {
        return testSimple(segments);
    }

    // Tests if every vertex appears in exactly 2 segments
    // except for two vertices appearing only once
    // and has no intersection elsewhere
    bool testPolyLine(const std::vector<Segment> &segments) {
        return testSimple(segments);
    }
}; // class BentleyOttmann

/**
 * @brief A double interval certain to contain an exact coordinate.
 *
 * Built from the coordinate's filter approximation, whose error bound is
 * widened once more for the rounding of the two subtractions here. A value
 * too large for double comes back as the whole line, which keeps every box
 * test that reads it conservative.
 */
template <class Number>
std::pair<double, double> conservativeInterval(const Number &value) {
    const Approximate approximation = approximate(value);
    if (approximation.error == 0.0) {
        return {approximation.value, approximation.value};
    }
    const double slack = approximation.error * approximateMargin +
                         approximateAbs(approximation.value) * approximateRoundoff + 0x1p-1000;
    const double lower = approximation.value - slack;
    const double upper = approximation.value + slack;
    if (!(lower >= -std::numeric_limits<double>::max()) ||
        !(upper <= std::numeric_limits<double>::max())) {
        return {-std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity()};
    }
    return {lower, upper};
}

/** @brief Which relation between two segments a pair search reports. */
enum class SegmentPairRelation {
    intersects,          ///< The segments share a point.
    crosses,             ///< The segments cross properly.
    interiorsIntersect,  ///< The relative interiors of the segments share a point.
};

/**
 * @brief Every pair of segments that meets, by a sweep over bounding boxes.
 *
 * The boxes are held in double, conservatively rounded, so the filter that
 * picks candidate pairs never compares an exact coordinate. The sweep runs
 * along one axis: the boxes are sorted by their low end on it, and each box is
 * compared with the ones that start before it ends, on the other axis, in a
 * loop over flat arrays. Only the pairs whose boxes overlap reach the exact
 * predicate, and over coordinates that filter, that predicate reads endpoint
 * approximations taken once per segment rather than once per test.
 *
 * Its cost, past the sort, is the number of pairs whose extents overlap on the
 * swept axis plus the number whose boxes overlap, which is at least the output
 * but can be quadratic on inputs with none: long parallel segments side by side
 * overlap everywhere and meet nowhere. @ref sample measures
 * both counts on random pairs, and @ref scan takes a callback that abandons the
 * sweep once its work outgrows a budget, so a caller can hand such an input to
 * @ref BentleyOttmann, whose cost does not depend on the boxes.
 *
 * @tparam Segment Segment type of the input.
 */
template <SegmentConcept Segment>
class SegmentPairScan {
    using Point = typename Segment::PointType;
    using Number = typename Point::NumberType;
    using Coordinate = sign_coordinate_t<Number, Number>;

public:
    using Id = std::uint32_t;
    using IdPair = std::pair<Id, Id>;

    /** @brief Whether the pair predicate reads stored approximations. */
    static constexpr bool filters = filtersSign<Coordinate>;

    /** @brief Counts over a random sample of pairs. */
    struct Sample {
        std::size_t pairs = 0;    ///< Pairs drawn.
        std::size_t alongX = 0;   ///< Pairs whose x-extents overlap.
        std::size_t alongY = 0;   ///< Pairs whose y-extents overlap.
        std::size_t boxes = 0;    ///< Pairs whose boxes overlap.
        std::size_t meeting = 0;  ///< Pairs the predicate accepts.
    };

    /** @brief Work a scan has done, as reported to its budget. */
    struct Progress {
        std::size_t scanned = 0;  ///< Pairs overlapping along the swept axis.
        std::size_t tested = 0;   ///< Pairs whose boxes overlap.
        std::size_t found = 0;    ///< Pairs reported.
        std::size_t swept = 0;    ///< Boxes whose comparisons are done, of all of them.
    };

    SegmentPairScan(const std::vector<Segment> &segments, SegmentPairRelation relation)
        : segments_(&segments), relation_(relation) {
        const std::size_t count = segments.size();
        if (count > std::numeric_limits<Id>::max()) {
            throw std::length_error("segment pair scan exceeds its 32-bit segment capacity");
        }
        xlo_.resize(count);
        xhi_.resize(count);
        ylo_.resize(count);
        yhi_.resize(count);
        if (filters) {
            approximations_.resize(2 * count);
        }
        for (std::size_t i = 0; i < count; ++i) {
            const Point &lo = segments[i].min();
            const Point &hi = segments[i].max();
            if (filters) {
                approximations_[2 * i] = approximatePoint(lo);
                approximations_[2 * i + 1] = approximatePoint(hi);
            }
            // The endpoints are in lexicographic order, so x runs from lo to
            // hi; y can run either way.
            xlo_[i] = conservativeInterval(lo.x()).first;
            xhi_[i] = conservativeInterval(hi.x()).second;
            const auto [loBelow, loAbove] = conservativeInterval(lo.y());
            const auto [hiBelow, hiAbove] = conservativeInterval(hi.y());
            ylo_[i] = std::min(loBelow, hiBelow);
            yhi_[i] = std::max(loAbove, hiAbove);
        }
    }

    /**
     * @brief Counts overlaps and meetings among @p count pairs drawn at random.
     *
     * The draw is seeded by the input size alone, so the same input always
     * takes the same decisions.
     */
    Sample sample(std::size_t count) const {
        Sample result;
        const std::uint64_t n = xlo_.size();
        if (n < 2) {
            return result;
        }
        std::uint64_t state = 0x9E3779B97F4A7C15ULL ^ n;
        const auto next = [&state] {
            state += 0x9E3779B97F4A7C15ULL;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        };
        // An index below `range`, from the top half of a draw, by multiplying
        // rather than dividing.
        const auto below = [&next](std::uint64_t range) {
            return static_cast<Id>(((next() >> 32) * range) >> 32);
        };
        result.pairs = count;
        for (std::size_t s = 0; s < count; ++s) {
            const Id i = below(n);
            Id j = below(n - 1);
            j += j >= i;
            const bool x = xlo_[i] <= xhi_[j] && xlo_[j] <= xhi_[i];
            const bool y = ylo_[i] <= yhi_[j] && ylo_[j] <= yhi_[i];
            result.alongX += x;
            result.alongY += y;
            if (x && y) {
                ++result.boxes;
                result.meeting += meets(i, j);
            }
        }
        return result;
    }

    /**
     * @brief Hands every pair whose boxes overlap to @p test, lesser Id first.
     *
     * @param alongY Sweep along y rather than x.
     * @param test Called as `test(i, j)` for each such pair; returns whether
     *        the two meet, which is what the budget counts as found. It may do
     *        whatever the caller needs with a pair that meets, @ref meets being
     *        the plain answer.
     * @param abandon Called with the @ref Progress so far after every few
     *        boxes' comparisons; a `true` return stops the scan.
     * @return `false` if @p abandon stopped the scan before every pair was
     *         tested.
     */
    template <class Test, class Abandon>
    bool scan(bool alongY, Test test, Abandon abandon) const {
        const std::size_t count = xlo_.size();
        const auto &lo = alongY ? ylo_ : xlo_;
        const auto &hi = alongY ? yhi_ : xhi_;
        const auto &acrossLo = alongY ? xlo_ : ylo_;
        const auto &acrossHi = alongY ? xhi_ : yhi_;

        std::vector<std::pair<double, Id>> order(count);
        for (std::size_t i = 0; i < count; ++i) {
            order[i] = {lo[i], static_cast<Id>(i)};
        }
        std::sort(order.begin(), order.end());
        // The swept boxes in flat arrays, in sweep order, for the inner loop.
        std::vector<double> sweptLo(count), sweptHi(count), otherLo(count), otherHi(count);
        std::vector<Id> id(count);
        for (std::size_t r = 0; r < count; ++r) {
            const Id i = order[r].second;
            sweptLo[r] = lo[i];
            sweptHi[r] = hi[i];
            otherLo[r] = acrossLo[i];
            otherHi[r] = acrossHi[i];
            id[r] = i;
        }

        // The counters stay local and the budget is consulted every few boxes,
        // so that the inner loop does not pay for being observable.
        constexpr std::size_t consultEvery = 32;
        std::size_t scanned = 0, tested = 0, found = 0;
        for (std::size_t a = 0; a < count; ++a) {
            const double end = sweptHi[a];
            const double below = otherLo[a];
            const double above = otherHi[a];
            std::size_t b = a + 1;
            for (; b < count && sweptLo[b] <= end; ++b) {
                if (otherLo[b] <= above && below <= otherHi[b]) {
                    ++tested;
                    found += test(std::min(id[a], id[b]), std::max(id[a], id[b])) ? 1 : 0;
                }
            }
            scanned += b - a - 1;
            if (a % consultEvery == consultEvery - 1 &&
                abandon(Progress{scanned, tested, found, a + 1})) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Whether two segments stand in the scan's relation.
     *
     * Four orientation signs the filter proves settle all three predicates.
     * Short of that, a proved sign on each side of one segment's line still
     * rules the pair out, and a shared endpoint — the commonest reason for a
     * sign the filter cannot prove — decides it without evaluating anything,
     * except for interiors of collinear segments; what remains goes to the
     * segment's own predicate.
     */
    bool meets(Id i, Id j) const {
        const Segment &p = (*segments_)[i];
        const Segment &q = (*segments_)[j];
        if constexpr (filters) {
            const auto a = filtered<Coordinate>(p.min(), approximations_, 2 * i);
            const auto b = filtered<Coordinate>(p.max(), approximations_, 2 * i + 1);
            const auto c = filtered<Coordinate>(q.min(), approximations_, 2 * j);
            const auto d = filtered<Coordinate>(q.max(), approximations_, 2 * j + 1);
            const auto s1 = orientationSignOf(a, b, c);
            const auto s2 = orientationSignOf(a, b, d);
            const auto s3 = orientationSignOf(c, d, a);
            const auto s4 = orientationSignOf(c, d, b);
            if (allDecided(s1, s2, s3, s4)) {
                return s1.value() != s2.value() && s3.value() != s4.value();
            }
            if ((allDecided(s1, s2) && s1.value() == s2.value()) ||
                (allDecided(s3, s4) && s3.value() == s4.value())) {
                return false;
            }
            if (p.min() == q.min() || p.min() == q.max() || p.max() == q.min() ||
                p.max() == q.max()) {
                switch (relation_) {
                case SegmentPairRelation::intersects:
                    return true;
                case SegmentPairRelation::crosses:
                    return false;
                case SegmentPairRelation::interiorsIntersect:
                    // A proved sign is a nonzero one, so the two are not
                    // collinear and meet only at the endpoint they share.
                    if (s1.decided() || s2.decided() || s3.decided() || s4.decided()) {
                        return false;
                    }
                    break;
                }
            }
        }
        switch (relation_) {
        case SegmentPairRelation::intersects:
            return p.intersects(q);
        case SegmentPairRelation::crosses:
            return p.crosses(q);
        case SegmentPairRelation::interiorsIntersect:
            break;
        }
        return p.interiorsIntersect(q);
    }

private:
    const std::vector<Segment> *segments_;
    SegmentPairRelation relation_;
    std::vector<double> xlo_, xhi_, ylo_, yhi_;
    std::vector<ApproximatePoint> approximations_;
};

/**
 * @brief Puts meeting pairs, named by position, in the order and form
 * @ref BentleyOttmann reports them.
 *
 * That order is by the two segments' ranks in the value order of the input,
 * lesser first, with the copies of a repeated segment in input order. Where no
 * segment repeats, a rank names one segment and the pairs sort as packed
 * integer keys.
 */
template <SegmentConcept Segment>
std::vector<std::array<Segment, 2>> orderedSegmentPairs(
    const std::vector<Segment> &segments,
    const std::vector<typename SegmentPairScan<Segment>::IdPair> &ids) {
    using Id = typename SegmentPairScan<Segment>::Id;
    const std::size_t count = segments.size();
    std::vector<Id> byRank(count);
    std::iota(byRank.begin(), byRank.end(), Id(0));
    std::sort(byRank.begin(), byRank.end(),
              [&segments](Id a, Id b) { return segments[a] < segments[b]; });
    std::vector<Id> rank(count);
    bool repeats = false;
    Id current = 0;
    for (std::size_t r = 0; r < count; ++r) {
        if (r > 0) {
            if (segments[byRank[r - 1]] < segments[byRank[r]]) {
                ++current;
            } else {
                repeats = true;
            }
        }
        rank[byRank[r]] = current;
    }

    std::vector<std::array<Segment, 2>> pairs;
    pairs.reserve(ids.size());
    if (!repeats) {
        std::vector<std::uint64_t> keys;
        keys.reserve(ids.size());
        for (const auto &[i, j] : ids) {
            const Id ri = rank[i], rj = rank[j];
            keys.push_back(ri < rj ? (std::uint64_t(ri) << 32) | rj
                                   : (std::uint64_t(rj) << 32) | ri);
        }
        sortPairKeys(keys);
        for (const std::uint64_t key : keys) {
            pairs.push_back({segments[byRank[key >> 32]], segments[byRank[key & 0xffffffffu]]});
        }
        return pairs;
    }

    struct Item {
        std::uint64_t key;
        Id first, second;
    };
    std::vector<Item> items;
    items.reserve(ids.size());
    for (auto [i, j] : ids) {
        if (rank[j] < rank[i] || (rank[j] == rank[i] && j < i)) {
            std::swap(i, j);
        }
        items.push_back({(std::uint64_t(rank[i]) << 32) | rank[j], i, j});
    }
    std::sort(items.begin(), items.end(), [](const Item &x, const Item &y) {
        if (x.key != y.key) {
            return x.key < y.key;
        }
        return x.first != y.first ? x.first < y.first : x.second < y.second;
    });
    for (const Item &item : items) {
        pairs.push_back({segments[item.first], segments[item.second]});
    }
    return pairs;
}

/** @brief How @ref findSegmentPairs finds the pairs. */
enum class SegmentPairMethod {
    automatic,  ///< Whichever the input suggests is cheaper, with a fallback.
    scan,       ///< @ref SegmentPairScan, run to completion.
    sweep,      ///< @ref BentleyOttmann.
};

/**
 * @brief Estimated nanoseconds per unit of work, for choosing a method.
 *
 * Fitted to the running times of both methods on random, sheared, polygonal
 * and adversarial segment sets over `int` and `ERational` coordinates. Only
 * their ratios matter, and only roughly: a wrong choice near the break-even
 * point costs little, and a badly wrong one is caught by the scan's budget.
 * The sweep's cost per pair is fitted without axis-parallel input, on which it
 * is several times cheaper, so that a near tie goes to the scan, which has a
 * budget to fall back on, rather than to the sweep, which has none.
 */
struct SegmentPairCosts {
    double scanned;       ///< Per pair overlapping along the swept axis.
    double tested;        ///< Per pair whose boxes overlap.
    double sorted;        ///< Per segment per bit of the input size, for the scan's sort.
    double reported;      ///< Per reported pair, for the scan.
    double sweepSegment;  ///< Per segment per bit of the input size, for the sweep.
    double sweepPair;     ///< Per reported pair per bit of the input size, for the sweep.
};

/** @brief The @ref SegmentPairCosts of an input of @p Segment. */
template <SegmentConcept Segment>
constexpr SegmentPairCosts segmentPairCosts() {
    return SegmentPairScan<Segment>::filters
               ? SegmentPairCosts{3.4, 49.0, 27.0, 240.0, 80.0, 48.0}
               : SegmentPairCosts{3.8, 10.0, 9.5, 9.0, 28.0, 15.0};
}

/**
 * @brief Tests the pairs of @p segments that can meet by a @ref SegmentPairScan,
 * if the input favours that over the sweep.
 *
 * A random sample of pairs estimates how many overlap along each axis, how
 * many boxes overlap and how many pairs meet, which prices a scan along the
 * better axis against @ref BentleyOttmann. When the scan is chosen it runs
 * under a budget: it is abandoned once the work still ahead of it, projected
 * from how far it has got, exceeds what the whole sweep would spend, or once
 * its work reaches a few times what the sweep would spend on the pairs found
 * so far. The scan therefore never costs more than a constant times the sweep,
 * whatever the estimate said, and on inputs whose boxes rarely overlap without
 * their segments meeting it is several times cheaper.
 *
 * @param test Called as `test(scan, i, j)`, with the positions of two segments
 *        whose boxes overlap, lesser first; returns whether they meet. A caller
 *        that only wants the pairs returns `scan.meets(i, j)`, and one that
 *        constructs something for each meeting pair can do that instead, in the
 *        same call, and spare the second test.
 * @param method @ref SegmentPairMethod::automatic, or a forced choice.
 * @param stopAtFirst Stop the scan soon after @p test first reports a meeting
 *        pair, for a caller that only asks whether there is one.
 * @return `false` if the sweep is the cheaper way, in which case the caller is
 *         to sweep instead and discard whatever @p test was already given.
 */
template <SegmentConcept Segment, class Test>
bool visitSegmentPairs(const std::vector<Segment> &segments, SegmentPairRelation relation, Test test,
                       SegmentPairMethod method = SegmentPairMethod::automatic,
                       bool stopAtFirst = false) {
    const std::size_t count = segments.size();
    if (method == SegmentPairMethod::sweep) {
        return false;
    }
    if (count < 2) {
        return true;
    }

    using Scan = SegmentPairScan<Segment>;
    const Scan scan(segments, relation);
    std::size_t met = 0;
    const auto visit = [&scan, &test, &met](typename Scan::Id i, typename Scan::Id j) {
        const bool meets = static_cast<bool>(test(scan, i, j));
        met += meets ? 1 : 0;
        return meets;
    };
    const auto firstFound = [stopAtFirst, &met] { return stopAtFirst && met > 0; };
    const double n = static_cast<double>(count);
    const double pairs = n * (n - 1) / 2;
    const std::size_t drawn =
        static_cast<std::size_t>(std::min(pairs, std::clamp(n, 256.0, 1024.0)));
    const typename Scan::Sample sample = scan.sample(drawn);
    const bool alongY = sample.alongY < sample.alongX;

    if (method == SegmentPairMethod::scan) {
        return scan.scan(alongY, visit, [&firstFound](const auto &) { return firstFound(); }) ||
               firstFound();
    }

    const SegmentPairCosts costs = segmentPairCosts<Segment>();
    const double bits = std::max(1.0, std::log2(n));
    const double share = pairs / static_cast<double>(std::max<std::size_t>(sample.pairs, 1));
    const double axisPairs = static_cast<double>(std::min(sample.alongX, sample.alongY)) * share;
    const double boxPairs = static_cast<double>(sample.boxes) * share;
    // The sweep is priced at as many meeting pairs as the sample allows, not
    // as many as it saw: three more than it caught is about a 95% bound on a
    // count it caught none of. A sample of a thousand pairs catches no meeting
    // pair at all on a sparse input that still has a hundred thousand, and
    // pricing the sweep at zero pairs there hands it inputs the scan does
    // several times faster; where the sample covers a good part of all pairs
    // the bound is tight anyway. Near the break-even point the budget below
    // settles the question with the pairs actually found.
    constexpr double missedMeetings = 3.0;
    const double meetingPairs = (static_cast<double>(sample.meeting) + missedMeetings) * share;

    const double scanEstimate = costs.sorted * n * bits + costs.scanned * axisPairs +
                                costs.tested * boxPairs + costs.reported * meetingPairs;
    const double sweepSegments = costs.sweepSegment * n * bits;
    const double sweepEstimate = sweepSegments + costs.sweepPair * meetingPairs * bits;
    if (sweepEstimate < scanEstimate) {
        return false;
    }

    // The scan is abandoned for what is still ahead of it, not for what it has
    // spent: the work already done is paid for whichever way the rest goes, so
    // a scan nearly through an input where the two methods cost about the same
    // is worth finishing. What remains is projected from the share of boxes
    // swept so far, and the sweep is priced at the pairs projected the same
    // way. The projection can be fooled by an input whose work bunches up at
    // the end of the sweep order, so the scan is also stopped outright once it
    // has spent a few times what the sweep would cost for the pairs it has
    // actually found, which keeps the whole within a constant of the sweep.
    //
    // It can be fooled the other way too, by work bunched up at the start: the
    // first boxes in sweep order can be the longest ones, as on the hull of a
    // Voronoi diagram, and a projection from the first thousandth of the boxes
    // then prices the scan at many times what it costs. So the projection is
    // only heeded once the scan has spent a fair share of the sweep's own
    // cost, which bounds what waiting for that evidence can waste.
    constexpr double spendingCap = 3.0;
    constexpr double projectionFloor = 0.25;
    const bool finished = scan.scan(alongY, visit, [&](const typename Scan::Progress &progress) {
        if (firstFound()) {
            return true;
        }
        const double found = static_cast<double>(progress.found);
        const double spent = costs.scanned * static_cast<double>(progress.scanned) +
                             costs.tested * static_cast<double>(progress.tested) +
                             costs.reported * found;
        if (spent > spendingCap * (sweepSegments + costs.sweepPair * found * bits)) {
            return true;
        }
        if (spent < projectionFloor * sweepSegments) {
            return false;
        }
        const double share = static_cast<double>(progress.swept) / n;
        const double remaining = spent * (1 - share) / share;
        return remaining > sweepSegments + costs.sweepPair * (found / share) * bits;
    });
    return finished || firstFound();
}

/**
 * @brief Whether any pair among @p segments stands in @p relation, by the
 * method the input favours.
 *
 * The scan stops at the first pair it finds, and the sweep at the first it
 * reports, so an input with a pair early in either order answers quickly and
 * one with none costs what finding all of them would.
 */
template <class Rational, SegmentConcept Segment>
bool detectSegmentPair(const std::vector<Segment> &segments, SegmentPairRelation relation,
                       SegmentPairMethod method = SegmentPairMethod::automatic) {
    using Scan = SegmentPairScan<Segment>;
    bool found = false;
    const bool scanned = visitSegmentPairs(
        segments, relation,
        [&found](const Scan &scan, typename Scan::Id i, typename Scan::Id j) {
            found = found || scan.meets(i, j);
            return found;
        },
        method, true);
    if (scanned) {
        return found;
    }
    BentleyOttmann<Rational, Segment> sweep;
    switch (relation) {
    case SegmentPairRelation::intersects:
        return sweep.detectIntersections(segments);
    case SegmentPairRelation::crosses:
        return sweep.detectCrossings(segments);
    case SegmentPairRelation::interiorsIntersect:
        break;
    }
    return sweep.detectInteriorIntersections(segments);
}

/**
 * @brief All pairs among @p segments in @p relation, by the method the
 * input favours, in @ref BentleyOttmann's order.
 *
 * @ref visitSegmentPairs decides, and scans when that is the cheaper way; the
 * sweep does the rest. Either way the pairs come back in the order the sweep
 * reports them.
 */
template <class Rational, SegmentConcept Segment>
std::vector<std::array<Segment, 2>> findSegmentPairs(
    const std::vector<Segment> &segments, SegmentPairRelation relation,
    SegmentPairMethod method = SegmentPairMethod::automatic) {
    using Scan = SegmentPairScan<Segment>;
    std::vector<typename Scan::IdPair> found;
    const bool scanned = visitSegmentPairs(
        segments, relation,
        [&found](const Scan &scan, typename Scan::Id i, typename Scan::Id j) {
            if (!scan.meets(i, j)) {
                return false;
            }
            found.emplace_back(i, j);
            return true;
        },
        method);
    if (scanned) {
        return orderedSegmentPairs(segments, found);
    }
    BentleyOttmann<Rational, Segment> sweep;
    switch (relation) {
    case SegmentPairRelation::intersects:
        return sweep.findIntersections(segments);
    case SegmentPairRelation::crosses:
        return sweep.findCrossings(segments);
    case SegmentPairRelation::interiorsIntersect:
        break;
    }
    return sweep.findInteriorIntersections(segments);
}
} // namespace pgl::detail

namespace pgl {

/**
 * @brief Finds all intersecting segment pairs.
 *
 * Runs in `O((n + k) log n)` where `n` is the number of input segments and
 * `k` is the number of reported pairs. A sample of the input decides between
 * a scan over bounding boxes and the Bentley-Ottmann sweep, and a scan that
 * outgrows what the sweep would cost is abandoned for it; see
 * @ref detail::findSegmentPairs. Either way the pairs come back in the same
 * order, by the segments' places in the value order of the input.
 *
 * @tparam Rational Exact type the sweep places crossings in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of intersecting segment pairs.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto findIntersections(const Container &segments) {
    using Segment = Container::value_type;
    std::vector<Segment> v(segments.begin(),segments.end());

    return pgl::detail::findSegmentPairs<Rational>(v, pgl::detail::SegmentPairRelation::intersects);
}

/**
 * @brief Finds all proper crossing segment pairs.
 *
 * Runs in `O((n + k) log n)` where `n` is the number of input segments and
 * `k` is the number of reported crossing pairs, choosing its method as
 * @ref findIntersections does.
 *
 * @tparam Rational Exact type the sweep places crossings in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of crossing segment pairs.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto findCrossings(const Container &segments) {
    using Segment = Container::value_type;
    std::vector<Segment> v(segments.begin(),segments.end());

    return pgl::detail::findSegmentPairs<Rational>(v, pgl::detail::SegmentPairRelation::crosses);
}


/**
 * @brief Finds all segment pairs whose relative interiors intersect.
 *
 * Reports the pairs for which `Segment::interiorsIntersect` holds: those that
 * cross properly and those that overlap collinearly along a stretch of positive
 * length. Runs in `O((n + k) log n)` where `n` is the number of input segments
 * and `k` is the number of reported pairs, choosing its method as
 * @ref findIntersections does.
 *
 * @tparam Rational Exact type the sweep places crossings in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of segment pairs whose interiors intersect.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto findInteriorIntersections(const Container &segments) {
    using Segment = Container::value_type;
    std::vector<Segment> v(segments.begin(),segments.end());

    return pgl::detail::findSegmentPairs<Rational>(
        v, pgl::detail::SegmentPairRelation::interiorsIntersect);
}

/**
 * @brief Detects whether the relative interiors of any two segments intersect.
 *
 * Runs in `O(n log n)`, choosing its method as @ref findIntersections does and
 * stopping at the first pair found.
 *
 * @tparam Rational Exact type the sweep places crossings in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if some pair satisfies `Segment::interiorsIntersect`.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
bool detectInteriorIntersections(const Container &segments) {
    using Segment = Container::value_type;
    std::vector<Segment> v(segments.begin(),segments.end());

    return pgl::detail::detectSegmentPair<Rational>(v, pgl::detail::SegmentPairRelation::interiorsIntersect);
}

/**
 * @brief Detects whether any two segments intersect.
 *
 * Runs in `O(n log n)`, choosing its method as @ref findIntersections does and
 * stopping at the first pair found.
 *
 * @tparam Rational Exact type the sweep places crossings in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if at least one intersecting pair exists.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
bool detectIntersections(const Container &segments) {
    using Segment = Container::value_type;
    std::vector<Segment> v(segments.begin(),segments.end());

    return pgl::detail::detectSegmentPair<Rational>(v, pgl::detail::SegmentPairRelation::intersects);
}

/**
 * @brief Detects whether any two segments properly cross.
 *
 * Runs in `O(n log n)`, choosing its method as @ref findIntersections does and
 * stopping at the first pair found.
 *
 * @tparam Rational Exact type the sweep places crossings in over coordinates
 *         that are not integers; integer coordinates choose their own.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if at least one crossing pair exists.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
bool detectCrossings(const Container &segments) {
    using Segment = Container::value_type;
    std::vector<Segment> v(segments.begin(),segments.end());

    return pgl::detail::detectSegmentPair<Rational>(v, pgl::detail::SegmentPairRelation::crosses);
}

namespace detail {

/**
 * @brief Finds all crossing segment pairs by brute force.
 *
 * Checks every unordered pair in quadratic time.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of crossing segment pairs.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto bruteForceCrossings(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<std::array<pgl::Segment<Point>,2>> ret;

    for (auto it_i = segments.begin(); it_i != segments.end(); ++it_i) {
        for (auto it_j = it_i; it_j != segments.end(); ++it_j) {
            if (it_i != it_j) {
                pgl::Segment<Point> s1 = *it_i;
                pgl::Segment<Point> s2 = *it_j;
                if (s1.crosses(s2)) {
                    if (s2 < s1)
                        std::swap(s1,s2);
                    ret.push_back({s1,s2});
                }
            }
        }
    }

    return ret;
}


/**
 * @brief Finds all intersecting segment pairs by brute force.
 *
 * Checks every unordered pair in quadratic time.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of intersecting segment pairs.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto bruteForceIntersections(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<std::array<pgl::Segment<Point>,2>> ret;

    for (auto it_i = segments.begin(); it_i != segments.end(); ++it_i) {
        for (auto it_j = it_i; it_j != segments.end(); ++it_j) {
            if (it_i != it_j) {
                pgl::Segment<Point> s1 = *it_i;
                pgl::Segment<Point> s2 = *it_j;
                if (s1.intersects(s2)) {
                    if (s2 < s1)
                        std::swap(s1,s2);
                    ret.push_back({s1,s2});
                }
            }
        }
    }

    return ret;
}

/**
 * @brief Finds all segment pairs whose relative interiors intersect, by brute
 * force.
 *
 * Checks every unordered pair in quadratic time.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of segment pairs whose interiors intersect.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto bruteForceInteriorIntersections(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<std::array<pgl::Segment<Point>,2>> ret;

    for (auto it_i = segments.begin(); it_i != segments.end(); ++it_i) {
        for (auto it_j = std::next(it_i); it_j != segments.end(); ++it_j) {
            pgl::Segment<Point> s1 = *it_i;
            pgl::Segment<Point> s2 = *it_j;
            if (s1.interiorsIntersect(s2)) {
                if (s2 < s1)
                    std::swap(s1,s2);
                ret.push_back({s1,s2});
            }
        }
    }

    return ret;
}

} // namespace detail

template <class PointType_, class LabelType>
template <class Rational>
bool PolygonWithHoles<PointType_, LabelType>::isValid() const {
    // The empty region carries no boundary, so it can carry no hole either.
    if (empty()) {
        return holes_.empty();
    }
    // Every ring simple on its own. This is also what rules out a zero-length
    // edge or a repeated vertex on any ring.
    if (!isSimple<Rational>()) {
        return false;
    }
    // Each hole inside the outer boundary. Polygon::contains is closed
    // containment, so a hole whose boundary touches or runs along the outer ring
    // passes, while one poking out — or one merely crossing it — fails. Closed
    // containment also gets the interior condition for free: a hole interior
    // reaching ∂outer would carry points beyond it, and the hole would not be
    // contained.
    for (const auto& hole : holes_) {
        if (!outer_.contains(hole)) {
            return false;
        }
    }
    // Hole interiors pairwise disjoint — the whole of the contract between two
    // holes. Boundaries meeting at points or along shared edges is allowed,
    // which is exactly what interiorsIntersect lets through; overlapping and
    // nested holes are not. Only pairs whose bounding boxes meet are tested.
    return !detail::anyIntersectingBoxPair(
        holes_.size(), [this](std::size_t i) -> const auto& { return holes_[i].bbox(); },
        [this](std::size_t i, std::size_t j) { return holes_[i].interiorsIntersect(holes_[j]); });
}

// Next to isValid because that is where a reader looks for the structural
// queries, though what it needs is detail::regionSlits, from separates.hpp.
template <class PointType_, class LabelType>
bool PolygonWithHoles<PointType_, LabelType>::isRegular() const {
    // The empty region is the closure of its own (empty) interior; anything else
    // without area is material that no interior comes near.
    if (empty()) {
        return true;
    }
    if (isDegenerate()) {
        return false;
    }
    // With area and a valid structure, the only points of A that closure(A°)
    // misses are the doubly covered stretches of the boundary.
    return detail::regionSlits(*this).empty();
}

// The set's structural contract, beside the region's for the same reason: this
// is where a reader looks for it.
template <class PointType_, class LabelType>
template <class Rational>
bool PolygonSet<PointType_, LabelType>::isValid() const {
    // Every component a valid region on its own.
    for (const auto& component : components_) {
        if (!component.template isValid<Rational>()) {
            return false;
        }
    }
    // Only pairs whose bounding boxes meet can break the two pairwise clauses.
    const bool broken = detail::anyIntersectingBoxPair(
        components_.size(),
        [this](std::size_t i) -> const auto& { return components_[i].bbox(); },
        [this](std::size_t i, std::size_t j) {
            // Interiors pairwise disjoint. Boundaries meeting at isolated points
            // is allowed, which is exactly what interiorsIntersect lets through.
            if (components_[i].interiorsIntersect(components_[j])) {
                return true;
            }
            // And no stretch of edge in common. Two components glued along one
            // would have interior points belonging to neither component's
            // interior, which is what the componentwise predicates would then
            // miss. Interiors being disjoint already, two of their edges can
            // only meet in a point or overlap along a stretch, so a
            // segment-valued edge intersection is exactly the case to reject.
            using ExactSegment = Segment<Point<NumberType>>;
            for (const auto& first : components_[i].edges()) {
                for (const auto& second : components_[j].edges()) {
                    const auto shared = first.template intersection<NumberType>(second);
                    if (shared && std::holds_alternative<ExactSegment>(*shared)) {
                        return true;
                    }
                }
            }
            return false;
        });
    return !broken;
}

} // namespace pgl
