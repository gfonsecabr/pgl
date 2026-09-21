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
#include <queue>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>


namespace pgl::detail{

/**
 * @brief The number type @ref BentleyOttmann evaluates its height expression in.
 *
 * That expression is a degree-three product of coordinate differences times one
 * part of the sweep abscissa. Over integer coordinates it is an integer, and
 * the type is the wider of the two operands' types, promoted once so their
 * product fits as well. Over rational coordinates the coefficients are already
 * fractions and nothing clears to integers, so the sweep's own fraction type is
 * the answer — and asking for a common type of a fraction and an integer, which
 * is what the general form does, would not even be well formed.
 *
 * @tparam Coefficient Type of the expression's coefficients.
 * @tparam Part Type of the sweep abscissa's numerator and denominator.
 * @tparam Fraction The sweep's own rational type.
 */
template <class Coefficient, class Part, class Fraction>
struct sweepHeightNumber {
    using type = promoted_number_t<std::common_type_t<Coefficient, Part>>;
};

template <class Int, class Part, class Fraction>
struct sweepHeightNumber<pgl::Rational<Int>, Part, Fraction> {
    using type = Fraction;
};

template <class Coefficient, class Part, class Fraction>
using sweepHeightNumber_t = typename sweepHeightNumber<Coefficient, Part, Fraction>::type;

/**
 * @brief The Bentley-Ottmann sweep over exact segments.
 *
 * For `n` segments, `findCrossings`, `findIntersections` and
 * `findInteriorIntersections` run in `O((n + k) log n)`, `k` being the number of
 * pairs reported; `detectCrossings`, `detectIntersections`,
 * `detectInteriorIntersections`, `testPolygon` and `testPolyLine` run in
 * `O(n log n)`. Both hold for degenerate input: many segments through one
 * point, collinear overlaps, and vertical segments touching others at an end.
 * Arithmetic operations count as `O(1)`.
 */
template <class Rational, SegmentConcept Segment>
class BentleyOttmann {
    using Point = Segment::PointType;
    using Number = Point::NumberType;
    static_assert(!std::is_floating_point_v<Number>,
                  "Bentley-Ottmann requires exact (non-floating-point) input "
                  "coordinates; the sweep line's predicates are not robust under "
                  "rounding. Use integer or rational coordinates.");
    using Rectangle = pgl::Rectangle<Point>;
    // The point label rides along with the point type through
    // Segment::intersection, so this must carry the input's label to name the
    // alternative that variant actually holds. Spelling it without it made the
    // sweep instantiable only for unlabelled points.
    using RPoint = pgl::Point<Rational, typename Point::LabelType>;
    using CrossingPair = std::array<Segment,2>;

    // Integer types the status order's arithmetic runs in. `Integer` is what
    // the sweep abscissa's numerator and denominator are; `Wide` holds a
    // degree-three product of coordinate differences, which is as far as the
    // height comparison's coefficients go; `Exact` multiplies one of those by
    // one of the abscissa's parts, and is the widest quantity the sweep forms.
    using Integer = pgl::rational_int_t<Rational>;
    using Coordinate = pgl::detail::promoted_number_t<Number>;
    using Wide = pgl::detail::promoted_number_t<Coordinate>;
    using Exact = pgl::detail::sweepHeightNumber_t<Wide, Integer, Rational>;
    static constexpr bool holdsWide =
        pgl::detail::extended_integral<Number> && pgl::detail::extended_integral<Wide> &&
        pgl::detail::extended_integral<Integer>;

    // ── Segments by number ──────────────────────────────────────────────────
    //
    // The sweep never holds a segment. The status tree, the events, the runs
    // and the reported pairs all name one by an `Id`, its position in the
    // caller's vector, and read it through @ref seg. A segment is small for
    // `int` coordinates, but over exact fractions it is eight arbitrary-
    // precision integers, and holding it by value made every tree node, event
    // and pair a copy of those, every lookup a hash of them and every final
    // ordering a sort by them. Over 5,000 polygon edges in `ERational`, which
    // cross nowhere, that bookkeeping was the whole of the sweep's cost.
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
    // The segments the sweep needs that the input does not have, after it in
    // Id order: the two sentinels bounding the status below and above, and a
    // probe the sweep builds to search the status for a point.
    std::array<Segment, 3> extras;
    Id bottomId() const { return static_cast<Id>(input->size()); }
    Id topId() const { return static_cast<Id>(input->size() + 1); }
    Id probeId() const { return static_cast<Id>(input->size() + 2); }

    const Segment &seg(Id id) const {
        return id < input->size() ? (*input)[id] : extras[id - input->size()];
    }

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

    // A plain function object rather than a std::function: the tree calls this
    // O(log n) times per operation and millions of times per sweep, and the
    // type-erased call cannot be inlined.
    struct AlongLine {
        const BentleyOttmann *sweep;
        bool operator()(Id a, Id b) const {
            return sweep->CompareAlongLine(a, b);
        }
    };
    // A tree the sweep can address by node rather than by value; see
    // @ref pgl::detail::RedBlackTree. The sweep knows where almost everything
    // it touches already is, and every comparison it avoids asking for is a
    // geometric predicate over exact arithmetic that does not run.
    using Tree = pgl::detail::RedBlackTree<Id, AlongLine>;
    using Node = typename Tree::Handle;

    enum class EventEnum {
        RIGHT, CROSS, VERTICAL, LEFT
    };

    struct Event {
        Rational x;
        // The abscissa in double, with the bound that makes it a proof. Two
        // events are ordered by their abscissas, and the queue compares
        // O(log n) pairs per push and per pop; comparing fractions over
        // arbitrary-precision parts exactly, every time, costs far more than
        // the events whose abscissas are nowhere near each other are worth.
        pgl::detail::Approximate approx;
        EventEnum type;
        Id s1;
        // A CROSS event's two segments, lower then upper in the status, by
        // the nodes holding them; null for every other kind. Both stay in the
        // status until the crossing, which is interior to both, and they keep
        // that order until then, since the crossing is the only place they can
        // exchange it. Knowing them is what lets the crossing's step skip
        // asking whether they meet there — a question whose answer is exactly
        // zero, which no filter can settle and exact arithmetic has to.
        Node lower = nullptr, upper = nullptr;

        Event(Rational x_, EventEnum type_, Id s1_)
            : x(std::move(x_)), approx(pgl::detail::approximate(x)),
              type(type_), s1(s1_) {}

        Event(Rational x_, pgl::detail::Approximate approx_, Node lower_, Node upper_)
            : x(std::move(x_)), approx(approx_),
              type(EventEnum::CROSS), s1(lower_->value),
              lower(lower_), upper(upper_) {}

        auto operator<(const Event &other) const { // Order is backwards by x
            const std::partial_ordering filtered =
                pgl::detail::approximateSign(other.approx - approx);
            if (filtered != std::partial_ordering::unordered) {
                return filtered < 0;
            }
            return other.x < x;
        }

        friend std::ostream &operator<<(std::ostream &out, const Event &e) {
            return out << static_cast<int>(e.type) << "@" << e.x << ": #" << e.s1;
        }
    };

    std::priority_queue<Event> queue;
    Rectangle bbox;

    /**
     * @brief A sweep abscissa, carrying the two integers the exact height test
     * needs alongside the fraction itself.
     *
     * Splitting the fraction once, rather than at each of the many comparisons
     * made at that abscissa, is the whole reason this exists: reading
     * `numerator()` off an unreduced fraction runs a gcd of its own, and
     * reading both parts runs two.
     */
    struct Abscissa {
        Rational x;
        Integer num{};
        Integer den{1};
        // The same value in double, with the bound that makes it a proof. A
        // fraction over arbitrary-precision parts reaches double the slow way,
        // through a long double per part, so converting it once per step rather
        // than once per comparison is the difference between the filter paying
        // for itself and not.
        pgl::detail::Approximate approx{};
    };

    // Where the sweep stands. The status tree's order is the order *at this
    // abscissa*, so moving it and putting the segments it reorders back in
    // order are one step: nothing may search the tree in between.
    Abscissa line;

    // The status: the segments straddling the sweep line, bottom to top.
    Tree tree;

    // Where each segment of the status is, by Id, or null. The status order
    // locates a segment in O(log n) of those predicates; this locates it in
    // one read, which is what a RIGHT event needs. It survives a crossing
    // untouched: turning a crossing over exchanges the nodes' places, never
    // their segments.
    std::vector<Node> nodeOf;

    // This step's events, split by kind; see @ref getEvents.
    std::array<std::vector<Event>, 4> events;

    // Scratch reused across steps, for the same reason @ref events is: a sweep
    // takes a step per event, and these are small enough that allocating them
    // afresh costs more than what they hold. A segment is gathered into a run
    // at this step when @ref collectedAt holds this step's @ref step.
    std::vector<std::uint32_t> collectedAt;
    std::uint32_t step = 0;
    // This step's collinear blocks, every run's in turn (see @ref Run), and the
    // permutation @ref reorderRun applies: the position each place of the run
    // takes its node from, and which of the run's old places each place holds
    // now and where each old place's node is.
    std::vector<std::uint32_t> runBlocks;
    std::vector<std::uint32_t> runTarget, runHolds, runWhere;

    /**
     * @brief One crossing's worth of the status tree: the segments meeting the
     * sweep line at a single point, bottom to top.
     *
     * They occupy consecutive positions — two segments at the same height are
     * neighbours, and every segment between two of them is at that height too —
     * so the run is exactly a stretch of the tree, and holding its nodes says
     * where both of its ends are without either being searched for.
     */
    struct Run {
        // The run's nodes and their segments, both in tree order. After
        // @ref reorderRun the nodes are in the run's new order, which is the
        // same set of positions, so the run's two ends are still its two ends.
        std::vector<Node> nodes;
        std::vector<Id> ids;
        // The run's maximal stretches of collinear segments, as positions in
        // `ids`: block b is `ids[runBlocks[firstBlock + b]]` up to
        // `ids[runBlocks[firstBlock + b + 1]]`, for b below `blockCount`. Set by
        // @ref reorderRun. Two segments of one block overlap and two of
        // different blocks cross, at the run's point.
        std::size_t firstBlock = 0;
        std::size_t blockCount = 0;
    };

    // The pairs found. Crossings are hashed, since the sweep asks whether it
    // has already scheduled a pair; the rest are only ever added to. Either
    // way they are put in order once, at the end, where it costs one sort of
    // integers.
    std::unordered_set<Key> crossingsSet;
    std::vector<Key> intersectionKeys;

    // Each key once, in order, as the pairs of segments they name.
    //
    // A key names two values, and a value given k times stands for k input
    // segments, so a key between two values stands for every pairing of their
    // copies and a key of a value with itself for every pair of its copies.
    // That is what the brute-force scan reports, one pair per two positions of
    // the input, and each copy comes back as itself, label included.
    template <class Keys>
    std::vector<CrossingPair> pairsOf(const Keys &keys) const {
        std::vector<Key> ordered(keys.begin(), keys.end());
        std::sort(ordered.begin(), ordered.end());
        ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
        std::vector<CrossingPair> pairs;
        pairs.reserve(ordered.size());
        for (const Key key : ordered) {
            const Rank ra = static_cast<Rank>(key >> 32);
            const Rank rb = static_cast<Rank>(key & 0xffffffffu);
            if (copies.empty()) {
                if (ra != rb) {
                    pairs.push_back({seg(byRank[ra]), seg(byRank[rb])});
                }
                continue;
            }
            for (std::uint32_t i = copiesStart[ra]; i < copiesStart[ra + 1]; ++i) {
                for (std::uint32_t j = ra == rb ? i + 1 : copiesStart[rb];
                     j < copiesStart[rb + 1]; ++j) {
                    pairs.push_back({seg(copies[i]), seg(copies[j])});
                }
            }
        }
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
    bool stopNow = false;

    bool addCrossing(Id a, Id b) {
        if (rank[b] < rank[a]) std::swap(a, b);
        crossingsSet.insert(keyOf(a, b));
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
        PGL_ASSERT(segments.size() + extras.size() <= std::numeric_limits<Id>::max());
        input = &segments;
        const std::size_t count = segments.size();
        const std::size_t slots = count + extras.size();

        std::vector<Id> order(count);
        std::iota(order.begin(), order.end(), Id(0));
        std::sort(order.begin(), order.end(),
                  [&segments](Id a, Id b) { return segments[a] < segments[b]; });
        rank.assign(slots, 0);
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
        for (std::size_t i = 0; i < extras.size(); ++i) {
            rank[count + i] = static_cast<Rank>(byRank.size() + i);
        }

        initBbox();
        extras[0] = bbox.edges()[0]; // Bottom edge as sentinel
        extras[1] = bbox.edges()[2]; // Top edge as sentinel

        // Every endpoint approximated once, for all the comparisons that will
        // read it. The probe is vertical and never reaches a height test.
        endsOf.assign(slots, Ends{});
        for (const Id id : byRank) {
            endsOf[id] = endsFor(seg(id));
        }
        endsOf[bottomId()] = endsFor(extras[0]);
        endsOf[topId()] = endsFor(extras[1]);

        nodeOf.assign(slots, nullptr);
        collectedAt.assign(slots, 0);
        step = 0;
    }

    void initQueue() {
        for (const Id id : byRank) {
            const Segment &s = seg(id);
            if(s.isVertical()) {
                queue.emplace(static_cast<Rational>(s.min().x()), EventEnum::VERTICAL, id);
            }
            else {
                queue.emplace(static_cast<Rational>(s.min().x()), EventEnum::LEFT, id);
            }
        }
    }

    void initBbox() {
        // Min and Max y-coordinate for sentinels
        bbox = Rectangle(seg(byRank.front()));
        for (const Id id : byRank) {
            bbox.insert(seg(id));
        }
        // Grow bbox by 1
        bbox = Rectangle(bbox.min().x()-1, bbox.min().y()-1, bbox.max().x()+1, bbox.max().y()+1);
    }

    // ── The status order ────────────────────────────────────────────────────
    //
    // The tree holds the segments straddling the sweep line in the order their
    // heights on it run, bottom to top, with segments meeting the line at one
    // point ordered by which of them leaves that point above. What that order
    // *is* is fixed by the geometry; everything below is about computing it
    // without ever evaluating those heights.
    //
    // Evaluating them is what the direct implementation does — one `yAtX` per
    // segment in the sweep's rational type, then a comparison — and it is what
    // the sweep used to spend itself on. The sweep abscissa is a fraction, so
    // each height comes out over a wider denominator still, and every one of
    // the O(log n) comparisons a single tree operation makes builds two of them
    // through a chain of rational operations that each reduce to lowest terms.
    // Over 3,000 small segments that comparison alone was 62% of the whole run.
    //
    // The difference of the two heights is affine in x, so its sign across the
    // x-range the two segments share is pinned by its sign at that range's two
    // ends — and each of those is one endpoint of one segment tested against
    // the other, an orientation over the *input* coordinates. Only when the
    // sign differs between the two ends do the segments meet inside the shared
    // range, and only then does where the sweep sits relative to that meeting
    // decide anything. That case, and nothing else, touches the abscissa.

    /**
     * @brief The two segments' four endpoints, each with its approximation.
     *
     * The height sign and everything it defers to read these rather than the
     * points, so the conversion behind the filter happens once per endpoint per
     * comparison instead of once per predicate that reads it.
     */
    using FilteredEnd = decltype(pgl::detail::filtered<Coordinate>(std::declval<const Point&>()));
    struct Endpoints {
        FilteredEnd aLo, aHi, bLo, bHi;
    };

    // Each segment's two endpoints, filtered, by Id; see @ref prepare. They
    // refer to the segments where they stand, in the input and in @ref extras.
    struct Ends {
        FilteredEnd lo, hi;
    };
    std::vector<Ends> endsOf;
    static Ends endsFor(const Segment &s) {
        return {pgl::detail::filtered<Coordinate>(s.min()),
                pgl::detail::filtered<Coordinate>(s.max())};
    }

    /**
     * @brief Sign of `b`'s height minus `a`'s height at the sweep line.
     *
     * Both segments must be non-vertical, and both must straddle the sweep
     * abscissa — which is what every segment in the status tree does.
     */
    int heightSign(Id ia, Id ib, const Abscissa &at) const {
        const Segment &a = seg(ia), &b = seg(ib);
        // Four endpoints, approximated once per segment when the input was
        // ranked. Every sign below reads the same four, and an approximation of
        // an exact coordinate is not cheap: converting them afresh for each of
        // the O(log n) comparisons a tree operation makes repeats the same
        // conversions over and over.
        const Endpoints ends{endsOf[ia].lo, endsOf[ia].hi, endsOf[ib].lo, endsOf[ib].hi};

        // The height difference at each end of the shared x-range. Whichever
        // segment contributes the end, its endpoint is tested against the other
        // segment, and the sign flips when the endpoint is a's, since the
        // difference is measured b minus a.
        const int atLeft = a.min().x() < b.min().x()
            ? pgl::detail::signOf(
                  pgl::detail::orientationSignOf(ends.aLo, ends.aHi, ends.bLo).value())
            : -pgl::detail::signOf(
                  pgl::detail::orientationSignOf(ends.bLo, ends.bHi, ends.aLo).value());
        const int atRight = b.max().x() < a.max().x()
            ? pgl::detail::signOf(
                  pgl::detail::orientationSignOf(ends.aLo, ends.aHi, ends.bHi).value())
            : -pgl::detail::signOf(
                  pgl::detail::orientationSignOf(ends.bLo, ends.bHi, ends.aHi).value());

        if (atLeft == atRight) {
            // Same sign at both ends: one segment runs clear of the other
            // across the whole range, the sweep included. Both ends zero: an
            // affine function with two roots is the zero function, so the two
            // segments are collinear and the difference vanishes everywhere.
            return atLeft;
        }
        if (atLeft == 0) {
            // The segments touch at the range's left end and separate to the
            // right of it, so all that matters is whether the sweep has left
            // that end behind — and the end is an input coordinate.
            return at.x > std::max(a.min().x(), b.min().x()) ? atRight : 0;
        }
        if (atRight == 0) {
            return at.x < std::min(a.max().x(), b.max().x()) ? atLeft : 0;
        }
        return crossedHeightSign(a, b, at, ends);
    }

    /**
     * @brief Whether two segments of the status tree meet the sweep line at the
     * same point, which is what makes them one crossing's worth of segments.
     */
    bool sameHeight(Id a, Id b, const Abscissa &at) const {
        return a == b || heightSign(a, b, at) == 0;
    }

    /**
     * @brief The height expression's two coefficients:
     * `(b's height - a's height) * dax * dbx == slope * x + offset`.
     *
     * Both x-extents are positive — neither segment is vertical, and a Segment
     * stores its endpoints in lexicographic order — so the scaling they apply
     * leaves the sign alone.
     */
    static std::pair<Wide, Wide> heightCoefficients(const Segment &a, const Segment &b) {
        const Wide dax = static_cast<Wide>(a.max().x()) - static_cast<Wide>(a.min().x());
        const Wide day = static_cast<Wide>(a.max().y()) - static_cast<Wide>(a.min().y());
        const Wide dbx = static_cast<Wide>(b.max().x()) - static_cast<Wide>(b.min().x());
        const Wide dby = static_cast<Wide>(b.max().y()) - static_cast<Wide>(b.min().y());

        return {dax * dby - dbx * day,
                dax * dbx * (static_cast<Wide>(b.min().y()) - static_cast<Wide>(a.min().y())) -
                    dax * dby * static_cast<Wide>(b.min().x()) +
                    dbx * day * static_cast<Wide>(a.min().x())};
    }

    /**
     * @brief The abscissa at which two properly crossing segments meet.
     *
     * A crossing event is ordered by its abscissa alone, so building the whole
     * point, as Segment::intersection does, pays for an ordinate nothing reads
     * and for the four orientations that re-establish a crossing the caller has
     * just tested. Over integer coordinates it also pays for a chain of fraction
     * operations, each widening its parts and reducing them once they outgrow a
     * machine word, where the abscissa is a single fraction,
     * `(a1.x * cross(r, s) + cross(b1 - a1, s) * r.x) / cross(r, s)` in the terms
     * of @ref detail::carrierCrossing. Its numerator is cubic in the
     * coordinates, and `Wide`, two promotions of them, holds that with bits to
     * spare. It is left unreduced:
     * @ref abscissa reduces it once, if the sweep ever stands there.
     *
     * A sweep whose fractions are bounded, or not fractions at all, keeps the
     * point's construction, which is written to respect those types' limits.
     *
     * @pre `a.crosses(b)`, so the two carriers are not parallel.
     */
    Rational crossingAbscissa(const Segment &a, const Segment &b) const {
        if constexpr (!pgl::is_Rational_v<Rational> ||
                      (!pgl::detail::arbitraryPrecision<Integer> && !holdsWide)) {
            return std::get<RPoint>(*a.template intersection<Rational>(b)).x();
        } else if constexpr (pgl::detail::extended_integral<Number>) {
            return wholeCrossingAbscissa<Wide>(a, b, [](const Number &value) {
                return static_cast<Wide>(value);
            });
        } else {
            if constexpr (pgl::is_Rational_v<Number>) {
                // Fractions that are whole, as integer data read into exact
                // coordinates is, clear into one fraction just the same.
                if (a.min().x().isInteger() && a.min().y().isInteger() &&
                    a.max().x().isInteger() && a.max().y().isInteger() &&
                    b.min().x().isInteger() && b.min().y().isInteger() &&
                    b.max().x().isInteger() && b.max().y().isInteger()) {
                    return wholeCrossingAbscissa<Integer>(a, b, [](const Number &value) {
                        return static_cast<Integer>(value.numerator());
                    });
                }
            }
            // Fractional coordinates leave nothing to clear into one fraction;
            // this is the carrier crossing's abscissa, in the sweep's own type.
            const auto exact = [](const auto &value) -> decltype(auto) {
                return pgl::detail::asNumber<Rational>(value);
            };
            const Rational rx = exact(a.max().x()) - exact(a.min().x());
            const Rational ry = exact(a.max().y()) - exact(a.min().y());
            const Rational sx = exact(b.max().x()) - exact(b.min().x());
            const Rational sy = exact(b.max().y()) - exact(b.min().y());
            const Rational ox = exact(b.min().x()) - exact(a.min().x());
            const Rational oy = exact(b.min().y()) - exact(a.min().y());
            return exact(a.min().x()) + (ox * sy - oy * sx) * rx / (rx * sy - ry * sx);
        }
    }

    // @ref crossingAbscissa as one fraction over whole coordinates, each read
    // as a `Whole` by `whole`.
    template <class Whole, class Read>
    static Rational wholeCrossingAbscissa(const Segment &a, const Segment &b, Read whole) {
        const Whole rx = whole(a.max().x()) - whole(a.min().x());
        const Whole ry = whole(a.max().y()) - whole(a.min().y());
        const Whole sx = whole(b.max().x()) - whole(b.min().x());
        const Whole sy = whole(b.max().y()) - whole(b.min().y());
        const Whole ox = whole(b.min().x()) - whole(a.min().x());
        const Whole oy = whole(b.min().y()) - whole(a.min().y());
        const Whole determinant = rx * sy - ry * sx;
        const Whole along = ox * sy - oy * sx;
        return Rational(Integer(whole(a.min().x()) * determinant + along * rx),
                        Integer(determinant));
    }

    /**
     * @brief @ref heightSign for the one case its endpoint tests leave open:
     * the two segments meet strictly inside the range they share, so which side
     * of that meeting the sweep sits on is what decides.
     *
     * Scaling the height difference by the two segments' x-extents and by the
     * abscissa's denominator — all three positive — clears every division out
     * of it and leaves one integer expression whose sign is the answer.
     */
    /**
     * @brief The height expression's sign in bounded `double` arithmetic, or
     * `unordered` where the bound does not separate it from zero.
     *
     * The coefficients of @ref heightCoefficients, formed from approximations
     * of the eight endpoint coordinates instead of exactly, each operation
     * widening the bound by what it rounds.
     */
    static std::partial_ordering approximateHeightSign(const Endpoints &ends,
                                                       const Abscissa &at) {
        const pgl::detail::ApproximatePoint aMin = pgl::detail::approximationOf(ends.aLo);
        const pgl::detail::ApproximatePoint aMax = pgl::detail::approximationOf(ends.aHi);
        const pgl::detail::ApproximatePoint bMin = pgl::detail::approximationOf(ends.bLo);
        const pgl::detail::ApproximatePoint bMax = pgl::detail::approximationOf(ends.bHi);
        const auto axMin = aMin.x;
        const auto ayMin = aMin.y;
        const auto bxMin = bMin.x;
        const auto byMin = bMin.y;
        const auto dax = aMax.x - axMin;
        const auto day = aMax.y - ayMin;
        const auto dbx = bMax.x - bxMin;
        const auto dby = bMax.y - byMin;

        const auto slope = dax * dby - dbx * day;
        const auto offset = dax * dbx * (byMin - ayMin) - dax * dby * bxMin + dbx * day * axMin;
        return pgl::detail::approximateSign(slope * at.approx + offset);
    }

    int crossedHeightSign(const Segment &a, const Segment &b, const Abscissa &at,
                          const Endpoints &ends) const {
        if constexpr (pgl::detail::filtersSign<Wide>) {
            // Where a coefficient is an arbitrary-precision fraction, forming
            // the two of them exactly is itself most of what this costs, and
            // the filter has to come before them rather than after: the whole
            // expression is evaluated in bounded double arithmetic, and only a
            // sign those bounds cannot settle pays for the exact coefficients
            // below. Over machine-integer coefficients the trade goes the other
            // way — forming them is a few multiplications — and the tighter
            // filter over the exact pair is the one that runs.
            const std::partial_ordering approximated = approximateHeightSign(ends, at);
            if (approximated != std::partial_ordering::unordered) {
                return pgl::detail::signOf(approximated);
            }
        }

        const auto [slope, offset] = heightCoefficients(a, b);

        if constexpr (pgl::detail::filtersSign<Exact>) {
            // Same bargain the orientation predicates strike: a double
            // evaluation carrying its own error bound proves the sign outright
            // for all but the near-degenerate pairs, and only those pay below.
            //
            // Filtering the coefficients rather than rebuilding the whole
            // expression in bounded double arithmetic: the expression is degree
            // three, and propagating a bound through that many operations
            // measured slower than forming the coefficients exactly and
            // converting the two of them.
            const std::partial_ordering filtered = pgl::detail::approximateSign(
                pgl::detail::approximate(slope) * at.approx +
                pgl::detail::approximate(offset));
            if (filtered != std::partial_ordering::unordered) {
                return pgl::detail::signOf(filtered);
            }
        }

        // Scaled once more by the abscissa's denominator, which is positive,
        // and there is nothing left to divide.
        const Exact scaled = static_cast<Exact>(slope) * static_cast<Exact>(at.num) +
                             static_cast<Exact>(offset) * static_cast<Exact>(at.den);
        return scaled > Exact(0) ? 1 : (scaled < Exact(0) ? -1 : 0);
    }

    // Compare segments by intersection points vertically along line
    bool CompareAlongLine (Id ia, Id ib) const {
        const Segment &a = seg(ia), &b = seg(ib);
        // Vertical segments are never stored in the set. They reach the
        // comparator only as a probe, and only ever at their own abscissa —
        // which is the sweep's, since a vertical segment is probed at the step
        // its event belongs to. So the probe's own endpoint is the point whose
        // side of the stored segment is wanted, and no height is needed at all.
        if (a.isVertical()) {
            if (b.isVertical()) {
                return a.min().y() < b.min().y();
            }
            PGL_ASSERT(line.x == a.min().x());
            if (a.min().y() < std::min(b.min().y(),b.max().y()))
                return true;

            if (std::max(b.min().y(),b.max().y()) < a.min().y())
                return false;

            return pgl::orientationSign(b.min(), b.max(), a.min()) < 0;
        }
        if(b.isVertical()) {
            PGL_ASSERT(line.x == b.min().x());
            if (std::max(a.min().y(),a.max().y()) < b.min().y())
                return true;

            if (b.min().y() < std::min(a.min().y(),a.max().y()))
                return false;

            return pgl::orientationSign(a.min(), a.max(), b.min()) > 0;
        }

        if (std::max(a.min().y(),a.max().y()) < std::min(b.min().y(),b.max().y()))
            return true;

        if (std::max(b.min().y(),b.max().y()) < std::min(a.min().y(),a.max().y()))
            return false;

        if (ia == ib) { // Same segment
            return false;
        }

        const int height = heightSign(ia, ib, line);
        if (height != 0) {
            return height > 0;
        }

        // Segments intersecting line at same point
        auto o = pgl::detail::orientationSignOf(endsOf[ia].lo, endsOf[ia].hi,
                                                endsOf[ib].hi).value();
        if (o > 0) {
            return true;
        }
        if (o < 0) {
            return false;
        }
        // One segment is a subset of the other
        return rank[ia] < rank[ib];
    }

    // Splits `x` into the parts the height tests read.
    //
    // Reducing the fraction here, once per step, is the same work the status
    // order would otherwise do over and over: every comparison that reaches the
    // exact fallback reads both parts, and reading either off an unreduced
    // fraction runs a gcd that is thrown away with the expression's locals.
    //
    // Unconditionally, and not @ref Rational::simplifyIfLarge: an abscissa
    // narrow enough that no operation would reduce it still feeds every
    // comparison at this step. Measured over a sweep of 3000 segments, gating
    // on width left the gcd count untouched while reducing outright cut it by
    // 41%.
    Abscissa abscissa(Rational x) const {
        Abscissa at;
        at.x = std::move(x);
        if constexpr (pgl::is_Rational_v<Rational>) {
            at.x.simplify();
            at.num = at.x.numerator();
            at.den = at.x.denominator();
        } else {
            // pgl::arrangement runs this sweep over a plain integer when its
            // coordinates are integral, and then the abscissa is whole.
            at.num = at.x;
            at.den = Integer(1);
        }
        at.approx = pgl::detail::approximate(at.x);
        return at;
    }

    void initTree() {
        tree = Tree(AlongLine{this});
        addToTree(bottomId());
        addToTree(topId());
    }

    // Inserts `id` into the status and records where it went.
    Node addToTree(Id id) {
        const Node node = tree.insert(id).first;
        nodeOf[id] = node;
        return node;
    }

    // Removes the node `n` from the status and forgets where it was.
    void removeFromTree(Node n) {
        nodeOf[n->value] = nullptr;
        tree.erase(n);
    }

    void printTree() const {
        std::cout << "Tree: ";
        Id previous = tree.first()->value;
        for(Id s : tree) {
            if (s != tree.first()->value) {
                if (CompareAlongLine(previous,s)) {
                    std::cout << " < ";
                } else if (CompareAlongLine(s,previous)) {
                    std::cout << " _>_ ";
                }
                else {
                    std::cout << " _=_ ";
                }
            }
            std::cout << seg(s);
            previous = s;
        }
        std::cout << std::endl;
    };

    void printCrossings() const {
        std::cout << "Crossings: ";
        for(const auto &[sa,sb] : pairsOf(crossingsSet)) {
            auto p = std::get<0>(*sa.template intersection<Rational>(sb));
            std::cout << sa << "crosses" << sb << " at " << p << "; ";
        }
        std::cout << std::endl;
    }

    void printQueue() const {
        std::cout << "Queue: ";
        auto l = queue;
        while (!l.empty()) {
            auto ev = l.top();
            std::cout << ev;
            l.pop();
        }
        std::cout << std::endl;
    }

    // Moves every event at `currentX` off the queue and into @ref events.
    //
    // The buckets are a member reused across steps rather than four fresh
    // vectors per step: there is a step per event, and the sweep's heap traffic
    // is worth more than the reallocation saves.
    void getEvents(const Rational &currentX) {
        for (std::vector<Event> &bucket : events) {
            bucket.clear();
        }
        do {
            events[static_cast<std::size_t>(queue.top().type)].push_back(queue.top());
            queue.pop();
        } while (!queue.empty() && queue.top().x == currentX);
    }

    void possibleCrossing(Node ita, Node itb) {
        PGL_ASSERT(ita && itb && "the bbox sentinels bound every neighbour walk");
        const Id a = ita->value, b = itb->value;
        const Segment &sa = seg(a), &sb = seg(b);

        if (sa.crosses(sb) && !crossingsSet.contains(keyOf(a, b))) {
            Rational x = crossingAbscissa(sa, sb);
            // The event needs this approximation anyway, and it settles whether
            // the crossing is still ahead of the sweep without an exact
            // comparison of two fractions.
            const pgl::detail::Approximate approx = pgl::detail::approximate(x);
            const std::partial_ordering ahead =
                pgl::detail::approximateSign(approx - line.approx);
            if (ahead == std::partial_ordering::unordered ? x > line.x : ahead > 0) {
                queue.emplace(std::move(x), approx, ita, itb);
                addCrossing(a, b);
            }
        }
    }

    void processRIGHT(const std::vector<Event> &evts) {
        for (const Event &ev : evts) {
            const Node it1 = nodeOf[ev.s1];
            // A RIGHT event names a segment the sweep put in the status at its
            // LEFT event and has not taken out since, so a miss here is the
            // sweep having lost track of it. Dropping the event loses whatever
            // crossings it would have reported — wrong, but bounded and
            // diagnosable, where erasing a node that is not there is not.
            PGL_ASSERT(it1 && "RIGHT event for a segment not in the status");
            if (!it1) {
                continue;
            }
            // Both bbox sentinels sit in the tree, so a real segment always has
            // a neighbour on either side of it.
            const Node it0 = Tree::prev(it1);
            const Node it2 = Tree::next(it1);
            removeFromTree(it1);

            possibleCrossing(it0, it2);
            if (stopNow) {
                return;
            }
        }

    }

    void getNewCrossEvents(std::vector<Event> &crossEvents, const Rational &currentX) {
        while (!queue.empty() && queue.top().x == currentX) {
            crossEvents.push_back(queue.top());
            queue.pop();
        }
    }

    // The segments of the status tree that meet at each of this step's crossing
    // points, each run in tree order, the runs in no particular order.
    //
    // Grouping by identity of the crossing point, rather than by the point
    // itself: what used to key this by an exact y-coordinate paid for that
    // coordinate — one `yAtX` per event and a std::map over fractions — to
    // express something the runs already say, since two segments meet the sweep
    // line at the same point exactly when they are one run.
    //
    // Must run while the tree still holds the previous abscissa's order, which
    // is the one that keeps each run contiguous.
    std::vector<Run> getCrossingSegments(const std::vector<Event> &evts, const Abscissa &at) {
        std::vector<Run> ret;
        // A fresh stamp rather than a cleared set: there is a step per event,
        // and clearing anything per step would cost in proportion to it.
        ++step;

        for (const Event &ev : evts) {
            if (collectedAt[ev.lower->value] == step) {
                // Already collected as part of an earlier run, and the event's
                // other segment with it: the two meet at the same point.
                PGL_ASSERT(collectedAt[ev.upper->value] == step);
                continue;
            }
            // The event's own two segments meet here by construction, and so
            // does everything the status holds between them: anything between
            // them that missed the point would have had to cross one of them,
            // or end, before it — an event already processed. So only the run's
            // two ends need asking about, and in general position the segment
            // beyond each end misses the point, a sign the filter settles.
            // Asking about the pair itself is what this avoids: the height
            // difference at its own crossing is exactly zero, the one answer no
            // filter can give, so every crossing used to pay for it in exact
            // arithmetic.
            Node first = ev.lower, last = ev.upper;
            while (Tree::prev(first) && sameHeight(Tree::prev(first)->value, ev.lower->value, at)) {
                first = Tree::prev(first);
            }
            while (Tree::next(last) && sameHeight(Tree::next(last)->value, ev.upper->value, at)) {
                last = Tree::next(last);
            }
            Run run;
            for (Node it = first;; it = Tree::next(it)) {
                PGL_ASSERT(it && "the event's upper segment lies above its lower one");
                PGL_ASSERT(sameHeight(it->value, ev.lower->value, at));
                run.nodes.push_back(it);
                run.ids.push_back(it->value);
                collectedAt[it->value] = step;
                if (it == last) {
                    break;
                }
            }
            ret.push_back(std::move(run));
        }

        return ret;
    }

    /**
     * @brief Puts a crossing's run into the order it has just past the crossing,
     * by exchanging the places of the nodes already holding it.
     *
     * Every segment of the run passes through the same point, in its interior,
     * so just before the point the run is in decreasing order of slope and just
     * past it in increasing order: the new order is the old one reversed. The
     * exception is segments of one slope, which are collinear, since they share
     * the point. Those overlap rather than cross, the comparator's tie-break
     * orders them the same way on both sides, and reversing each such block
     * again restores it.
     *
     * Asking the comparator instead, as sorting the run would, is asking at the
     * crossing abscissa, where every height difference in the run is exactly
     * zero: the filter settles none of them and each pays for exact arithmetic
     * before falling through to the slopes anyway. A run is nearly always two
     * segments long, and two segments that cross have different slopes, so
     * nearly always this is a single exchange and no predicate at all.
     *
     * Leaves `run.nodes` in the run's new order, which is the same stretch of
     * the tree it occupied before, and records the run's collinear blocks in
     * @ref runBlocks. `O(s)` for a run of `s` segments: one orientation per
     * neighbouring pair and at most one exchange per position.
     */
    void reorderRun(Run &run) {
        const std::size_t size = run.nodes.size();
        run.firstBlock = runBlocks.size();
        runBlocks.push_back(0);
        for (std::size_t i = 1; i < size; ++i) {
            // A two-segment run is the event's own crossing pair. Otherwise
            // both pass through the crossing, and the far endpoint is not the
            // crossing, so it lies on the other's line exactly when the two
            // share a line.
            if (size == 2 ||
                pgl::detail::orientationSignOf(endsOf[run.ids[i - 1]].lo, endsOf[run.ids[i - 1]].hi,
                                               endsOf[run.ids[i]].hi).value() != 0) {
                runBlocks.push_back(static_cast<std::uint32_t>(i));
            }
        }
        runBlocks.push_back(static_cast<std::uint32_t>(size));
        run.blockCount = runBlocks.size() - run.firstBlock - 1;
        if (size < 2) {
            return;
        }

        // The new order is the blocks' order reversed, each block keeping its
        // own: `runTarget[t]` is the old place whose node goes to place t.
        runTarget.clear();
        for (std::size_t b = run.blockCount; b-- > 0;) {
            for (std::uint32_t p = runBlocks[run.firstBlock + b];
                 p < runBlocks[run.firstBlock + b + 1]; ++p) {
                runTarget.push_back(p);
            }
        }

        // `run.nodes[t]` is the node at the run's t-th place as the exchanges go
        // on, and ends up being the run's new order. Each exchange puts the
        // right node in the next place and sends whatever was there to where
        // that node was, which the two maps say without a search.
        runHolds.resize(size);
        runWhere.resize(size);
        std::iota(runHolds.begin(), runHolds.end(), std::uint32_t(0));
        std::iota(runWhere.begin(), runWhere.end(), std::uint32_t(0));
        for (std::uint32_t t = 0; t < size; ++t) {
            const std::uint32_t wanted = runTarget[t];
            const std::uint32_t from = runWhere[wanted];
            if (from == t) {
                continue;
            }
            tree.swap(run.nodes[t], run.nodes[from]);
            std::swap(run.nodes[t], run.nodes[from]);
            const std::uint32_t displaced = runHolds[t];
            runHolds[from] = displaced;
            runWhere[displaced] = from;
            runHolds[t] = wanted;
            runWhere[wanted] = t;
        }
        // PGL_ASSERT(std::is_sorted(run.nodes.begin(), run.nodes.end(), [this](Node a, Node b) {
        //     return CompareAlongLine(a->value, b->value);
        // }));  // O(run): uncomment when debugging
    }

    void processCROSS(std::vector<Event> &evts, const Rational &currentX) {
        // 3) Check possible new cross events
        getNewCrossEvents(evts, currentX);

        // Where the crossings are, split once for the whole step. Found before
        // the abscissa becomes the sweep's own at 4): a run is contiguous under
        // the order the tree is still holding, which is the previous abscissa's,
        // and it is that order the walk out from each event follows.
        const Abscissa crossing = abscissa(currentX);

        std::vector<Run> crossingAt = getCrossingSegments(evts, crossing);

        // 4) Move the line to currentX
        line = crossing;
        runBlocks.clear();

        // 5) Turn each crossing over.
        //    Past the crossing the run's segments occupy the same stretch of
        //    the tree in a different order, and that is all that changes: the
        //    tree's shape, and every node in it, stay as they are. So the run
        //    is sorted among itself and its nodes exchange places, which the
        //    tree does without a comparison. Taking the run out and putting it
        //    back is what this replaces, and that charged a full search of the
        //    tree — a logarithmic run of height predicates — per segment, twice
        //    over, to rediscover an order the run already fixes.
        for (Run &run : crossingAt) {
            reorderRun(run);
        }

        // 6) Create all CROSS new events
        for (const Run &run : crossingAt) {
            // The run's own ends. Nothing outside it meets the sweep line where
            // it does, or it would have been collected into it, so the segments
            // that could newly become neighbours are the two just past them.
            const Node it1 = run.nodes.front();
            const Node it2 = run.nodes.back();

            // Possible new crossings
            possibleCrossing(Tree::prev(it1), it1);
            possibleCrossing(it2, Tree::next(it2));
        }
        if (stopNow) {
            return;
        }

        // 7) Add crossings to set
        //    Every segment of a run has the run's point in its interior, so two
        //    of them cross exactly when they are not collinear, which is when
        //    they lie in different blocks. Those pairs are listed outright, in
        //    time proportional to them; the pairs within a block overlap, and
        //    were reported, where a mode reports them, by whichever of the two
        //    went into the status second. Testing every pair of the run instead
        //    cost the square of the run even where a long collinear block made
        //    nearly all of them overlaps.
        for (const Run &run : crossingAt) {
            const std::vector<Id> &ids = run.ids;
            const std::uint32_t *starts = runBlocks.data() + run.firstBlock;
            for (std::size_t b = 0; b + 1 < run.blockCount; ++b) {
                for (std::uint32_t i = starts[b]; i < starts[b + 1]; ++i) {
                    for (std::size_t j = starts[b + 1]; j < ids.size(); ++j) {
                        PGL_ASSERT(seg(ids[i]).crosses(seg(ids[j])));
                        if (addCrossing(ids[i], ids[j])) {
                            return;
                        }
                    }
                }
            }
        }
    }

    void processRIGHT_interior(const std::vector<Event> &evts) {
        for (const Event &ev : evts) {
            // Find top segment intersecting ev.s1 on line
            // Use fake vertical segment
            const Point &end = seg(ev.s1).max();
            extras[2] = Segment(end.x(), end.y(), end.x(), end.y()+1);
            Node it = tree.lowerBound(probeId());

            for (; it && Tree::prev(it) && seg(it->value).contains(end); it = Tree::prev(it)) {
            }
            if (it) {
                it = Tree::next(it);
            }

            for (; it && seg(it->value).contains(end); it = Tree::next(it)) {
                if (addIntersection(ev.s1, it->value)) {
                    return;
                }
            }
        }
    }

    // The segments of the status meeting each vertical segment at this
    // abscissa. Every one of them has the meeting point in its interior, since
    // the segments ending here are already out and those starting here not yet
    // in, so it meets the vertical at a point of height between the vertical's
    // two ends, and crosses it exactly when that height is strictly between
    // them. Reporting intersections, the walk covers the closed range of
    // heights; reporting crossings, which is also what an interior intersection
    // with a vertical segment is, it covers the open one, so that a stack of
    // segments through one of the vertical's endpoints, none of which it
    // crosses, is never walked. Either way it is `O(log n)` for the search plus
    // one step per pair reported.
    void processVERTICAL(const std::vector<Event> &evts) {
        for (const Event &ev : evts) {
            const Segment &vertical = seg(ev.s1);
            if (!onlyCrossings) {
                for (Node it = tree.lowerBound(ev.s1); it; it = Tree::next(it)) {
                    if (!vertical.intersects(seg(it->value)))
                        break;
                    if (addCrossing(ev.s1, it->value)) {
                        return;
                    }
                }
                continue;
            }
            if (vertical.isDegenerate()) {
                continue;
            }
            // The first segment strictly above the vertical's lower end, up to
            // the first not strictly below its upper end, which a probe
            // standing on that end tells.
            const Point &top = vertical.max();
            extras[2] = Segment(top.x(), top.y(), top.x(), top.y() + 1);
            for (Node it = tree.upperBound(ev.s1); it && CompareAlongLine(it->value, probeId());
                 it = Tree::next(it)) {
                PGL_ASSERT(vertical.crosses(seg(it->value)));
                if (addCrossing(ev.s1, it->value)) {
                    return;
                }
            }
        }
    }

    void processVERTICAL_interior(const std::vector<Event> &v_evts, const std::vector<Event> &r_evts, const std::vector<Event> &l_evts) {
        std::vector<std::pair<Number, Id>> order;
        for (const Event &ev : l_evts) {
            order.emplace_back(seg(ev.s1).min().y(), ev.s1);
        }
        for (const Event &ev : r_evts) {
            order.emplace_back(seg(ev.s1).max().y(), ev.s1);
        }
        for (const Event &ev : v_evts) {
            order.emplace_back(seg(ev.s1).min().y(), ev.s1);
            order.emplace_back(seg(ev.s1).max().y(), ev.s1);
        }
        std::sort(order.begin(),order.end());

        for (const Event &ev : v_evts) {
            const auto &y1 = seg(ev.s1).min().y();
            const auto &y2 = seg(ev.s1).max().y();
            for (auto it = std::lower_bound(order.begin(), order.end(), std::make_pair(y1, Id(0)));
                 it != order.end() && it->first < y2;
                 ++it) {
                if (it->second != ev.s1 && addIntersection(ev.s1, it->second)) {
                    return;
                }
            }
        }
    }

    void processLEFT(const std::vector<Event> &evts) {
        for (const Event &ev : evts) {
            const Segment &s = seg(ev.s1);
            const Node it1 = addToTree(ev.s1);
            Node it0 = Tree::prev(it1);
            Node it2 = Tree::next(it1);

            queue.emplace(static_cast<Rational>(s.max().x()), EventEnum::RIGHT, ev.s1);
            possibleCrossing(it0, it1);
            possibleCrossing(it1, it2);
            if (stopNow) {
                break;
            }

            if (!onlyCrossings) {
                while (it0 && !stopNow && seg(it0->value).contains(s.min())) {
                    addIntersection(it0->value, ev.s1);
                    it0 = Tree::prev(it0);
                }
                while (it2 && !stopNow && seg(it2->value).contains(s.min())) {
                    addIntersection(it2->value, ev.s1);
                    it2 = Tree::next(it2);
                }
                if (stopNow) {
                    break;
                }
            } else if (interiorsOnly) {
                // A segment overlapping this one along a stretch of positive
                // length is collinear with it and, being in the status at this
                // abscissa, passes through its left endpoint. The status orders
                // the segments through one point by slope, so those collinear
                // with this one are the neighbours on either side of it until
                // the first that is not, and nothing else needs asking. The ones
                // that merely end here were taken out by this step's RIGHT
                // events, and one that starts here too is met from whichever of
                // the two goes in second.
                const Ends &ends = endsOf[ev.s1];
                const auto collinear = [this, &ends](Node it) {
                    if (!it || it->value >= input->size()) {
                        return false;
                    }
                    const Ends &other = endsOf[it->value];
                    return pgl::detail::orientationSignOf(ends.lo, ends.hi, other.lo).value() == 0 &&
                           pgl::detail::orientationSignOf(ends.lo, ends.hi, other.hi).value() == 0;
                };
                for (Node it = Tree::prev(it1); collinear(it) && !stopNow; it = Tree::prev(it)) {
                    addIntersection(it->value, ev.s1);
                }
                for (Node it = Tree::next(it1); collinear(it) && !stopNow; it = Tree::next(it)) {
                    addIntersection(it->value, ev.s1);
                }
                if (stopNow) {
                    break;
                }
            }
        }
    }

    // The vertical segments at this abscissa that overlap along a stretch of
    // positive length. Ordered by their lower ends, each is compared with the
    // earlier ones still reaching above that end, which it overlaps, and the
    // ones that do not reach are dropped for good, so the work is the pairs
    // reported plus one drop per segment.
    void processVERTICAL_overlaps(const std::vector<Event> &evts) {
        if (evts.size() < 2) {
            return;
        }
        std::vector<Id> order;
        for (const Event &ev : evts) {
            if (!seg(ev.s1).isDegenerate()) {
                order.push_back(ev.s1);
            }
        }
        std::sort(order.begin(), order.end(),
                  [this](Id a, Id b) { return seg(a).min().y() < seg(b).min().y(); });
        std::vector<Id> reaching;
        for (const Id id : order) {
            const auto &bottom = seg(id).min().y();
            std::size_t write = 0;
            for (std::size_t read = 0; read < reaching.size(); ++read) {
                const Id other = reaching[read];
                if (!(bottom < seg(other).max().y())) {
                    continue;
                }
                reaching[write++] = other;
                if (addIntersection(other, id)) {
                    return;
                }
            }
            reaching.resize(write);
            reaching.push_back(id);
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

        initQueue();
        line = abscissa(static_cast<Rational>(bbox.min().x()));
        initTree();

        while (!queue.empty()) {
            // printTree();
            // 1) Get all events with same x into events
            const Rational currentX = queue.top().x;
            getEvents(currentX);

            // 2) Do all RIGHT events
            processRIGHT(events[(size_t)EventEnum::RIGHT]);
            // A detection stops at the first pair reported, and every phase
            // checks: a step can hold a run or a walk as long as the input,
            // and finishing one after the answer is known is what kept a
            // detection from its bound.
            if (stopNow)
                break;

            // 3) Check possible new cross events
            // 4) Do all CROSS removals from tree
            // 5) Move the line to currentX
            // 6) Do all CROSS insertions to tree
            // 7) Create all CROSS new events
            // 8) Add new crossings to the output
            processCROSS(events[1], currentX);
            if (stopNow)
                break;

            if (!onlyCrossings) {
                processRIGHT_interior(events[(size_t)EventEnum::RIGHT]);
                if (stopNow)
                    break;
            }

            // 10) Do all VERTICAL events
            processVERTICAL(events[(size_t)EventEnum::VERTICAL]);
            if (stopNow)
                break;
            if (interiorsOnly) {
                processVERTICAL_overlaps(events[(size_t)EventEnum::VERTICAL]);
                if (stopNow)
                    break;
            }
            if (!onlyCrossings) {
                processVERTICAL_interior(events[(size_t)EventEnum::VERTICAL],
                                         events[(size_t)EventEnum::RIGHT],
                                         events[(size_t)EventEnum::LEFT]);
                if (stopNow)
                    break;
            }

            // 11) Do all LEFT events
            processLEFT(events[(size_t)EventEnum::LEFT]);

            if (stopNow)
                break;
        }
    }

public:
    std::vector<CrossingPair> findCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        run(segments);
        return pairsOf(crossingsSet);
    }

    std::vector<CrossingPair> findIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        run(segments);
        if (segments.empty()) {
            return {};
        }
        intersectionKeys.insert(intersectionKeys.end(), crossingsSet.begin(), crossingsSet.end());

        // Insert segments sharing an endpoint. Sorting the endpoints brings the
        // segments at each point together; a duplicate was already reported
        // with itself, so the distinct segments are all that need visiting.
        std::vector<std::pair<const Point *, Rank>> ends;
        ends.reserve(2 * byRank.size());
        for (Rank r = 0; r < byRank.size(); ++r) {
            ends.emplace_back(&seg(byRank[r]).min(), r);
            // A zero-length segment's two ends are one point, and listing it
            // twice there would pair the segment with itself.
            if (!seg(byRank[r]).isDegenerate()) {
                ends.emplace_back(&seg(byRank[r]).max(), r);
            }
        }
        std::sort(ends.begin(), ends.end(), [](const auto &a, const auto &b) {
            return *a.first < *b.first || (!(*b.first < *a.first) && a.second < b.second);
        });
        for (std::size_t first = 0; first < ends.size();) {
            std::size_t last = first + 1;
            while (last < ends.size() && *ends[last].first == *ends[first].first) {
                ++last;
            }
            for (std::size_t i = first; i + 1 < last; ++i) {
                for (std::size_t j = i + 1; j < last; ++j) {
                    intersectionKeys.push_back((Key(ends[i].second) << 32) | ends[j].second);
                }
            }
            first = last;
        }

        return pairsOf(intersectionKeys);
    }

    std::vector<CrossingPair> findInteriorIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        interiorsOnly = true;
        run(segments);
        intersectionKeys.insert(intersectionKeys.end(), crossingsSet.begin(), crossingsSet.end());
        return pairsOf(intersectionKeys);
    }

    bool detectInteriorIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        interiorsOnly = true;
        onCrossing = [] (const Segment &, const Segment &) {return true;};
        onIntersection = [] (const Segment &, const Segment &) {return true;};
        run(segments);
        return !crossingsSet.empty() || !intersectionKeys.empty();
    }

    bool detectCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        onCrossing = [] (const Segment &, const Segment &) {return true;};
        run(segments);
        return !crossingsSet.empty();
    }

    bool detectIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        onCrossing = [] (const Segment &, const Segment &) {return true;};
        onIntersection = [] (const Segment &, const Segment &) {return true;};
        run(segments);

        if (!crossingsSet.empty() || !intersectionKeys.empty())
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
        onlyCrossings = false;
        bool notSimple = false;
        onCrossing = [&notSimple] (const Segment &, const Segment &) {notSimple = true; return true;};
        size_t count = 0;
        size_t n = segments.size();
        onIntersection = [n,&count, &notSimple] (const Segment &p0, const Segment &p1) {
                if (p0.collinear(p1) && p0.interiorsIntersect(p1)) {
                    notSimple = true; // Collinear overlap
                    return true;
                }
                const int shared = (p0.min() == p1.min()) + (p0.min() == p1.max())
                                 + (p0.max() == p1.min()) + (p0.max() == p1.max());
                if (!shared) {
                    notSimple = true; // Vertex inside an edge
                    return true;
                }
                count++;
                if (count > 2*n) { // A vertex appears twice
                    notSimple = true;
                }
                return notSimple;
            };
        run(segments);

        return !notSimple;
    }

    // Tests if every vertex appears in exactly 2 segments
    // except for two vertices appearing only once
    // and has no intersection elsewhere
    bool testPolyLine(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        bool notSimple = false;
        onCrossing = [&notSimple] (const Segment &, const Segment &) {notSimple = true; return true;};
        size_t count = 0;
        size_t n = segments.size();
        onIntersection = [n,&count, &notSimple] (const Segment &p0, const Segment &p1) {
                if (p0.collinear(p1) && p0.interiorsIntersect(p1)) {
                    notSimple = true; // Collinear overlap
                    return true;
                }
                const int shared = (p0.min() == p1.min()) + (p0.min() == p1.max())
                                 + (p0.max() == p1.min()) + (p0.max() == p1.max());
                if (!shared) {
                    notSimple = true; // Vertex inside an edge
                    return true;
                }
                count++;
                if (count > 2*n - 2) { // A vertex appears twice
                    notSimple = true;
                }
                return notSimple;
            };
        run(segments);

        return !notSimple;
    }
}; // class BentleyOttmann

/**
 * @brief Largest coordinate magnitude at which the sweep over `int`
 * coordinates is exact in `Rational<int64_t>`.
 *
 * A crossing's abscissa is `num / det` with `|det| <= 8 C^2`, and it lies in
 * the bounding box, so `|num| <= C |det| <= 8 C^3`, which fits an `int64_t`
 * for `C < 2^20`. Every other quantity the sweep forms is either compared in
 * the fraction's promoted type or evaluated in its own wider one.
 */
inline constexpr long long sweepInt64Limit = (1LL << 20) - 1;

/**
 * @brief Largest coordinate magnitude at which the sweep over `int`
 * coordinates is exact at all.
 *
 * The orientation predicate over `int` points evaluates in `int64_t` and
 * compares two products of coordinate differences, each up to `4 C^2`; this is
 * the largest `C` for which that stays below `2^63`. Up to here the abscissa,
 * at most `8 C^3`, fits `Rational<int128>` with room to spare.
 */
inline constexpr long long sweepInt128Limit = 1518500249;

/**
 * @brief The narrowest fraction the sweep over @p segments is exact in: 0 for
 * `Rational<int64_t>`, 1 for `Rational<int128>`, 2 for neither.
 */
template <class Segment>
int sweepFractionTier(const std::vector<Segment> &segments) {
    int tier = 0;
    const auto within = [](const auto &c, long long limit) {
        const pgl::int128 v = static_cast<pgl::int128>(c);
        return -static_cast<pgl::int128>(limit) <= v && v <= static_cast<pgl::int128>(limit);
    };
    for (const Segment &s : segments) {
        for (const auto *p : {&s.min(), &s.max()}) {
            for (const auto *c : {&p->x(), &p->y()}) {
                if (tier == 0 && !within(*c, sweepInt64Limit)) {
                    tier = 1;
                }
                if (!within(*c, sweepInt128Limit)) {
                    return 2;
                }
            }
        }
    }
    return tier;
}

template <class Segment>
struct isPglSegment : std::false_type {};
template <class Point, class Label>
struct isPglSegment<pgl::Segment<Point, Label>> : std::true_type {};

/**
 * @brief Runs @p body over a @ref BentleyOttmann sweep in the narrowest
 * fraction the input allows, when the caller asked for the default
 * `Rational<BigInt>` over integer coordinates.
 *
 * All three fractions compute the same exact answer wherever the narrower
 * ones are exact; they differ only in what their arithmetic costs, and a
 * machine-word fraction avoids the per-operation overhead of an
 * arbitrary-precision one. Coordinates of a wider integer type small enough
 * for the tier are narrowed to `int` for the sweep and the reported pairs
 * converted back, labels included.
 *
 * @param body Called as `body(sweep, segments)`; returns what the caller
 * returns, either a `bool` or a vector of segment pairs.
 */
template <class Rational, class Segment, class Body>
auto sweepInNarrowestRational(const std::vector<Segment> &segments, Body body) {
    using Number = typename Segment::NumberType;
    if constexpr (std::same_as<Rational, pgl::Rational<pgl::BigInt>> &&
                  (std::signed_integral<Number> || std::same_as<Number, pgl::int128>) &&
                  (std::same_as<Number, int> || isPglSegment<Segment>::value)) {
        const int tier = sweepFractionTier(segments);
        if (tier < 2) {
            const auto run = [&]<class Narrow>(std::type_identity<Narrow>) {
                if constexpr (std::same_as<Number, int>) {
                    BentleyOttmann<Narrow, Segment> sweep;
                    return body(sweep, segments);
                } else {
                    using IntSegment =
                        pgl::Segment<pgl::Point<int, typename Segment::PointType::LabelType>,
                                     typename Segment::LabelType>;
                    const std::vector<IntSegment> narrow(segments.begin(), segments.end());
                    BentleyOttmann<Narrow, IntSegment> sweep;
                    auto found = body(sweep, narrow);
                    if constexpr (std::same_as<decltype(found), bool>) {
                        return found;
                    } else {
                        std::vector<std::array<Segment, 2>> pairs;
                        pairs.reserve(found.size());
                        for (const auto &pair : found) {
                            pairs.push_back({Segment(pair[0]), Segment(pair[1])});
                        }
                        return pairs;
                    }
                }
            };
            return tier == 0 ? run(std::type_identity<pgl::Rational<std::int64_t>>{})
                             : run(std::type_identity<pgl::Rational<pgl::int128>>{});
        }
    }
    BentleyOttmann<Rational, Segment> sweep;
    return body(sweep, segments);
}

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
 * Measured on random, clustered, polygonal and adversarial segment sets over
 * `int` and `ERational` coordinates. Only their ratios matter, and only
 * roughly: a wrong choice near the break-even point costs little, and a badly
 * wrong one is caught by the scan's budget.
 */
struct SegmentPairCosts {
    double scanned;       ///< Per pair overlapping along the swept axis.
    double tested;        ///< Per pair whose boxes overlap.
    double sorted;        ///< Per segment per bit of the input size, for the scan's sort.
    double sweepSegment;  ///< Per segment per bit of the input size, for the sweep.
    double sweepPair;     ///< Per reported pair per bit of the input size, for the sweep.
};

/** @brief The @ref SegmentPairCosts of an input of @p Segment. */
template <SegmentConcept Segment>
constexpr SegmentPairCosts segmentPairCosts() {
    return SegmentPairScan<Segment>::filters ? SegmentPairCosts{1.5, 40.0, 3.0, 150.0, 30.0}
                                             : SegmentPairCosts{1.5, 7.0, 3.0, 45.0, 30.0};
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

    const double scanEstimate =
        costs.sorted * n * bits + costs.scanned * axisPairs + costs.tested * boxPairs;
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
    constexpr double spendingCap = 3.0;
    const bool finished = scan.scan(alongY, visit, [&](const typename Scan::Progress &progress) {
        if (firstFound()) {
            return true;
        }
        const double spent = costs.scanned * static_cast<double>(progress.scanned) +
                             costs.tested * static_cast<double>(progress.tested);
        const double found = static_cast<double>(progress.found);
        if (spent > spendingCap * (sweepSegments + costs.sweepPair * found * bits)) {
            return true;
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
    return sweepInNarrowestRational<Rational>(
        segments, [relation](auto &bentleyOttmann, const auto &input) {
            switch (relation) {
            case SegmentPairRelation::intersects:
                return bentleyOttmann.detectIntersections(input);
            case SegmentPairRelation::crosses:
                return bentleyOttmann.detectCrossings(input);
            case SegmentPairRelation::interiorsIntersect:
                break;
            }
            return bentleyOttmann.detectInteriorIntersections(input);
        });
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
    return sweepInNarrowestRational<Rational>(
        segments, [relation](auto &bentleyOttmann, const auto &input) {
            switch (relation) {
            case SegmentPairRelation::intersects:
                return bentleyOttmann.findIntersections(input);
            case SegmentPairRelation::crosses:
                return bentleyOttmann.findCrossings(input);
            case SegmentPairRelation::interiorsIntersect:
                break;
            }
            return bentleyOttmann.findInteriorIntersections(input);
        });
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
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of intersecting segment pairs.
 * @warning Needs rational numbers, preferably with unbonded size
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
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of crossing segment pairs.
 * @warning Needs rational numbers, preferably with unbonded size
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
 * @tparam Rational Exact rational type used internally by the sweep line.
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
 * @tparam Rational Exact rational type used internally by the sweep line.
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
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if at least one intersecting pair exists.
 * @warning Needs rational numbers, preferably with unbonded size
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
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return `true` if at least one crossing pair exists.
 * @warning Needs rational numbers, preferably with unbonded size
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
