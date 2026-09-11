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
#include <cassert>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
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
    // at this step when @ref collectedAt holds this step's @ref step;
    // @ref runOrder is one run's nodes in the order the crossing leaves them in.
    std::vector<std::uint32_t> collectedAt;
    std::uint32_t step = 0;
    std::vector<Node> runOrder;

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
        assert(segments.size() + extras.size() <= std::numeric_limits<Id>::max());
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
            assert(line.x == a.min().x());
            if (a.min().y() < std::min(b.min().y(),b.max().y()))
                return true;

            if (std::max(b.min().y(),b.max().y()) < a.min().y())
                return false;

            return pgl::orientationSign(b.min(), b.max(), a.min()) < 0;
        }
        if(b.isVertical()) {
            assert(line.x == b.min().x());
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
        assert(ita && itb && "the bbox sentinels bound every neighbour walk");
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
            assert(it1 && "RIGHT event for a segment not in the status");
            if (!it1) {
                continue;
            }
            // Both bbox sentinels sit in the tree, so a real segment always has
            // a neighbour on either side of it.
            const Node it0 = Tree::prev(it1);
            const Node it2 = Tree::next(it1);
            removeFromTree(it1);

            possibleCrossing(it0, it2);
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
                assert(collectedAt[ev.upper->value] == step);
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
                assert(it && "the event's upper segment lies above its lower one");
                assert(sameHeight(it->value, ev.lower->value, at));
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
     * the tree it occupied before.
     */
    void reorderRun(Run &run) {
        const std::size_t size = run.nodes.size();
        if (size < 2) {
            return;
        }
        runOrder.assign(run.nodes.rbegin(), run.nodes.rend());
        // A two-segment run is the event's own crossing pair.
        if (size > 2) {
            for (std::size_t begin = 0; begin < size;) {
                std::size_t end = begin + 1;
                // Both pass through the crossing, and the far endpoint is not
                // the crossing, so it lies on the other's line exactly when the
                // two share a line.
                while (end < size) {
                    const Ends &p = endsOf[runOrder[end - 1]->value];
                    const Ends &q = endsOf[runOrder[end]->value];
                    if (pgl::detail::orientationSignOf(p.lo, p.hi, q.hi).value() != 0) {
                        break;
                    }
                    ++end;
                }
                std::reverse(runOrder.begin() + static_cast<std::ptrdiff_t>(begin),
                             runOrder.begin() + static_cast<std::ptrdiff_t>(end));
                begin = end;
            }
        }
        assert(std::is_sorted(runOrder.begin(), runOrder.end(), [this](Node a, Node b) {
            return CompareAlongLine(a->value, b->value);
        }));

        // `run.nodes[i]` is the node at the run's i-th position as the exchanges
        // go on, and ends up being the run's new order. Each step puts the right
        // node in the next position and sends whatever was there to where that
        // node came from, so one pass over the run settles it. The inner scan is
        // over a run, not over the tree.
        for (std::size_t i = 0; i < size; ++i) {
            if (run.nodes[i] == runOrder[i]) {
                continue;
            }
            std::size_t from = i + 1;
            while (from < size && run.nodes[from] != runOrder[i]) {
                ++from;
            }
            assert(from < size && "the run's nodes are a permutation of themselves");
            tree.swap(run.nodes[i], run.nodes[from]);
            run.nodes[from] = run.nodes[i];
            run.nodes[i] = runOrder[i];
        }
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

        // 7) Add crossings to set
        for (const Run &run : crossingAt) {
            const std::vector<Id> &ids = run.ids;
            for (size_t i = 0; i+1 < ids.size(); i++) {
                for (size_t j = i+1; j < ids.size(); j++) {
                    if (seg(ids[i]).crosses(seg(ids[j]))) {
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

    void processVERTICAL(const std::vector<Event> &evts) {
        for (const Event &ev : evts) {
            const Segment &vertical = seg(ev.s1);
            for (Node it = tree.lowerBound(ev.s1); it; it = Tree::next(it)) {
                const Segment &other = seg(it->value);
                if (!vertical.intersects(other))
                    break;
                if (onlyCrossings) {
                    if (vertical.crosses(other)) {
                        addCrossing(ev.s1, it->value);
                    }
                }
                else {
                    addCrossing(ev.s1, it->value);
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
                if (it->second != ev.s1) {
                    addIntersection(ev.s1, it->second);
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

            if (!onlyCrossings) {
                while (it0 && seg(it0->value).contains(s.min())) {
                    addIntersection(it0->value, ev.s1);
                    it0 = Tree::prev(it0);
                }
                while (it2 && seg(it2->value).contains(s.min())) {
                    addIntersection(it2->value, ev.s1);
                    it2 = Tree::next(it2);
                }
                if (stopNow) {
                    break;
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

            // 3) Check possible new cross events
            // 4) Do all CROSS removals from tree
            // 5) Move the line to currentX
            // 6) Do all CROSS insertions to tree
            // 7) Create all CROSS new events
            // 8) Add new crossings to the output
            processCROSS(events[1], currentX);

            if (!onlyCrossings) {
                processRIGHT_interior(events[(size_t)EventEnum::RIGHT]);
                if (stopNow)
                    break;
            }

            // 10) Do all VERTICAL events
            processVERTICAL(events[(size_t)EventEnum::VERTICAL]);
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
} // namespace pgl::detail

namespace pgl {

/**
 * @brief Finds all intersecting segment pairs with Bentley-Ottmann.
 *
 * Runs in `O((n + k) log n)` where `n` is the number of input segments and
 * `k` is the number of reported pairs.
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

    return pgl::detail::sweepInNarrowestRational<Rational>(v, [](auto &sweep, const auto &input) {
        return sweep.findIntersections(input);
    });
}

/**
 * @brief Finds all proper crossing segment pairs with Bentley-Ottmann.
 *
 * Runs in `O((n + k) log n)` where `n` is the number of input segments and
 * `k` is the number of reported crossing pairs.
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

    return pgl::detail::sweepInNarrowestRational<Rational>(v, [](auto &sweep, const auto &input) {
        return sweep.findCrossings(input);
    });
}


/**
 * @brief Detects whether any two segments intersect.
 *
 * Runs in `O(n log n)` in the positive or negative detection mode used here.
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

    return pgl::detail::sweepInNarrowestRational<Rational>(v, [](auto &sweep, const auto &input) {
        return sweep.detectIntersections(input);
    });
}

/**
 * @brief Detects whether any two segments properly cross.
 *
 * Runs in `O(n log n)` in the positive or negative detection mode used here.
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

    return pgl::detail::sweepInNarrowestRational<Rational>(v, [](auto &sweep, const auto &input) {
        return sweep.detectCrossings(input);
    });
}

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
    // nested holes are not. The bounding boxes prefilter the quadratic scan.
    for (std::size_t i = 0; i < holes_.size(); ++i) {
        for (std::size_t j = i + 1; j < holes_.size(); ++j) {
            if (!holes_[i].bbox().intersects(holes_[j].bbox())) {
                continue;
            }
            if (holes_[i].interiorsIntersect(holes_[j])) {
                return false;
            }
        }
    }
    return true;
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
    for (std::size_t i = 0; i < components_.size(); ++i) {
        for (std::size_t j = i + 1; j < components_.size(); ++j) {
            if (!components_[i].bbox().intersects(components_[j].bbox())) {
                continue;  // the boxes prefilter the quadratic scan
            }
            // Interiors pairwise disjoint. Boundaries meeting at isolated points
            // is allowed, which is exactly what interiorsIntersect lets through.
            if (components_[i].interiorsIntersect(components_[j])) {
                return false;
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
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

} // namespace pgl
