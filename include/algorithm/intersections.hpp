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
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>


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
    // Segment::intersection, so these must carry the input's label to name the
    // alternatives that variant actually holds. Spelling them without it made
    // the sweep instantiable only for unlabelled points.
    using RPoint = pgl::Point<Rational, typename Point::LabelType>;
    using RSegment = pgl::Segment<RPoint>;
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
        Segment s1;

        Event(Rational x_, EventEnum type_, Segment s1_)
            : x(std::move(x_)), approx(pgl::detail::approximate(x)),
              type(type_), s1(std::move(s1_)) {}

        auto operator<(const Event &other) const { // Order is backwards by x
            const std::partial_ordering filtered =
                pgl::detail::approximateSign(other.approx - approx);
            if (filtered != std::partial_ordering::unordered) {
                return filtered < 0;
            }
            return other.x < x;
        }

        friend std::ostream &operator<<(std::ostream &out, const Event &e) {
            return out << e.type << "@" << e.x << ": " << e.s1;
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

    // A plain function object rather than a std::function: the tree calls this
    // O(log n) times per operation and millions of times per sweep, and the
    // type-erased call cannot be inlined.
    struct AlongLine {
        const BentleyOttmann *sweep;
        bool operator()(const Segment &a, const Segment &b) const {
            return sweep->CompareAlongLine(a, b);
        }
    };
    // A tree the sweep can address by node rather than by value; see
    // @ref pgl::detail::RedBlackTree. The sweep knows where almost everything
    // it touches already is, and every comparison it avoids asking for is a
    // geometric predicate over exact arithmetic that does not run.
    using Tree = pgl::detail::RedBlackTree<Segment, AlongLine>;
    using Node = typename Tree::Handle;
    Tree tree;

    // Where each segment of the status is. The status order locates a segment
    // in O(log n) of those predicates; this locates it in a hash lookup, which
    // is what a RIGHT event and the start of a crossing's run both need. It
    // survives a crossing untouched: turning a crossing over exchanges the
    // nodes' places, never their segments.
    std::unordered_map<Segment, Node> nodeOf;

    // This step's events, split by kind; see @ref getEvents.
    std::array<std::vector<Event>, 4> events;

    // Scratch reused across steps, for the same reason @ref events is: a sweep
    // takes a step per event, and these are small enough that allocating them
    // afresh costs more than what they hold. @ref collected marks the segments
    // already gathered into a run this step; @ref runOrder is one run's nodes
    // in the order the crossing leaves them in.
    std::unordered_set<Segment> collected;
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
        std::vector<Segment> segments;
    };

    // Hashed, not ordered: these accumulate one entry per reported pair, and
    // the sweep only ever asks whether a pair is already in them. Ordering them
    // as they are built charges a log-sized run of segment comparisons, and a
    // red-black node, for every crossing found; the results are put in order
    // once, at the end, where it costs a single sort.
    struct CrossingPairHash {
        std::size_t operator()(const CrossingPair &pair) const {
            std::size_t seed = 0;
            pgl::detail::hashCombine(seed, pair[0]);
            pgl::detail::hashCombine(seed, pair[1]);
            return seed;
        }
    };
    using CrossingPairSet = std::unordered_set<CrossingPair, CrossingPairHash>;
    CrossingPairSet crossingsSet, intersectionSet;

    // The set's contents in the order the public entry points hand them back.
    static std::vector<CrossingPair> sorted(const CrossingPairSet &pairs) {
        std::vector<CrossingPair> ordered(pairs.begin(), pairs.end());
        std::sort(ordered.begin(), ordered.end());
        return ordered;
    }

    std::function<bool(const CrossingPair&)> onCrossing = [](const CrossingPair&){return false;},
                                       onIntersection = [](const CrossingPair&){return false;};
    bool onlyCrossings = true;
    bool stopNow = false;

    bool addCrossing(const CrossingPair &p) {
        crossingsSet.insert(p);
        if (onCrossing(p)) {
            stopNow = true;
            return true;
        }

        return false;
    }

    bool addIntersection(const CrossingPair &p) {
        intersectionSet.insert(p);
        if (onIntersection(p)) {
            stopNow = true;
            return true;
        }

        return false;
    }

    void initQueue(const std::vector<Segment> &segments) {
        for (const Segment &s :segments) {
            if(s.isVertical()) {
                queue.emplace(static_cast<Rational>(s.min().x()), EventEnum::VERTICAL, s);
            }
            else {
                queue.emplace(static_cast<Rational>(s.min().x()), EventEnum::LEFT, s);
            }
        }
    }

    void initBbox(const std::vector<Segment> &segments) {
        // Min and Max y-coordinate for sentinels
        bbox = Rectangle(segments[0]);
        for (const Segment &s :segments) {
            bbox.insert(s);
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

    /**
     * @brief Sign of `b`'s height minus `a`'s height at the sweep line.
     *
     * Both segments must be non-vertical, and both must straddle the sweep
     * abscissa — which is what every segment in the status tree does.
     */
    int heightSign(const Segment &a, const Segment &b, const Abscissa &at) const {
        // Four endpoints, each approximated once. Every sign below reads the
        // same four, and an approximation of an exact coordinate is not cheap:
        // letting each predicate convert its own operands would do the same
        // conversion up to five times over one call.
        const Endpoints ends{pgl::detail::filtered<Coordinate>(a.min()),
                             pgl::detail::filtered<Coordinate>(a.max()),
                             pgl::detail::filtered<Coordinate>(b.min()),
                             pgl::detail::filtered<Coordinate>(b.max())};

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
    bool sameHeight(const Segment &a, const Segment &b, const Abscissa &at) const {
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
    bool CompareAlongLine (const Segment& a, const Segment& b) const {
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

        if (a == b) { // Same segment
            return false;
        }

        const int height = heightSign(a, b, line);
        if (height != 0) {
            return height > 0;
        }

        // Segments intersecting line at same point
        auto o = pgl::orientationSign(a.min(), a.max(), b.max());
        if (o > 0) {
            return true;
        }
        if (o < 0) {
            return false;
        }
        // One segment is a subset of the other
        return a < b;
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
        nodeOf.clear();
        addToTree(bbox.edges()[0]); // Bottom edge as sentinel
        addToTree(bbox.edges()[2]); // Top edge as sentinel
    }

    // Inserts `s` into the status and records where it went.
    Node addToTree(const Segment &s) {
        const Node node = tree.insert(s).first;
        nodeOf[s] = node;
        return node;
    }

    // Removes the node `n` from the status and forgets where it was.
    void removeFromTree(Node n) {
        nodeOf.erase(n->value);
        tree.erase(n);
    }

    // The node holding `s`, or null when the status does not hold it.
    Node nodeFor(const Segment &s) const {
        const auto found = nodeOf.find(s);
        return found == nodeOf.end() ? nullptr : found->second;
    }

    void printTree() const {
        std::cout << "Tree: ";
        Segment previous = tree.first()->value;
        for(Segment s : tree) {
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
            std::cout << s;
            previous = s;
        }
        std::cout << std::endl;
    };

    void printCrossings() const {
        std::cout << "Crossings: ";
        for(auto [sa,sb] : crossingsSet) {
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
        Segment sa = ita->value, sb = itb->value;

        CrossingPair pair{sa,sb};
        if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);

        if (sa.crosses(sb) && !crossingsSet.contains(pair)) {
            RPoint cross = std::get<RPoint>(*sa.template intersection<Rational>(sb));
            if (cross.x() > line.x) {
                // assert(CompareAlongLine(sa,sb));
                queue.emplace(cross.x(), EventEnum::CROSS, sa);
                addCrossing(pair);
            }
        }
    }

    void processRIGHT(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            const Node it1 = nodeFor(ev.s1);
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

    // The lowest segment of the run that meets the sweep line where `s` does,
    // `s` itself included.
    //
    // `s` is located by the tree's own order, so this must be called while the
    // tree is still ordered where `s` sits in it — which is not necessarily
    // `at`: the run about to collapse onto one point at `at` is found while the
    // tree still holds the order that keeps it contiguous.
    Node findFirst(const Segment &s, const Abscissa &at) {
        Node it = nodeFor(s);
        assert(it && "the run's segments are the ones the status holds");
        while (Tree::prev(it) && sameHeight(it->value, s, at)) {
            it = Tree::prev(it);
        }
        return Tree::next(it);
    }

    // The segments of the status tree that meet at each of this step's crossing
    // points, each run in tree order, the runs in no particular order.
    //
    // Grouping by identity of the crossing point, rather than by the point
    // itself: what used to key this by an exact y-coordinate paid for that
    // coordinate — one `yAtX` per event and a std::map over fractions — to
    // express something the runs already say, since two segments meet the sweep
    // line at the same point exactly when they are one run.
    std::vector<Run> getCrossingSegments(const std::vector<Event> &evts, const Abscissa &at) {
        std::vector<Run> ret;
        // @ref collected, cleared rather than built: there is a step per event,
        // and a fresh hash table each time would allocate its buckets each time.
        collected.clear();

        for (const Event &ev : evts) {
            if (collected.contains(ev.s1)) {
                continue;  // already collected as part of an earlier run
            }
            Run run;
            for (Node it = findFirst(ev.s1, at); it && sameHeight(it->value, ev.s1, at);
                 it = Tree::next(it)) {
                run.nodes.push_back(it);
                run.segments.push_back(it->value);
                collected.insert(it->value);
            }
            if (!run.segments.empty()) {
                ret.push_back(std::move(run));
            }
        }

        return ret;
    }

    /**
     * @brief Puts a crossing's run into the order it has just past the crossing,
     * by exchanging the places of the nodes already holding it.
     *
     * Every segment of the run meets the sweep line at the same point, so the
     * order among them is decided by which of them leaves that point above
     * which — a comparison the sweep's own comparator makes at the crossing
     * abscissa. Sorting the run costs that comparison k log k times for a run
     * of k segments, against the k searches of the whole tree that reinserting
     * them costs, and a run is nearly always two segments long.
     *
     * Leaves `run.nodes` in the run's new order, which is the same stretch of
     * the tree it occupied before.
     */
    void reorderRun(Run &run) {
        const std::size_t size = run.nodes.size();
        if (size < 2) {
            return;
        }
        runOrder.assign(run.nodes.begin(), run.nodes.end());
        std::sort(runOrder.begin(), runOrder.end(), [this](Node a, Node b) {
            return CompareAlongLine(a->value, b->value);
        });

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
            const std::vector<Segment> &segs = run.segments;
            for (size_t i = 0; i+1 < segs.size(); i++) {
                for (size_t j = i+1; j < segs.size(); j++) {
                    CrossingPair pair{segs[i], segs[j]};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);

                    if (pair[0].crosses(pair[1])) {
                        if (addCrossing(pair)) {
                            return;
                        }
                    }
                }
            }
        }
    }

    void processRIGHT_interior(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            // Find top segment intersecting ev.s1 on line
            // Use fake vertical segment
            Segment sv(ev.s1.max().x(), ev.s1.max().y(), ev.s1.max().x(), ev.s1.max().y()+1);
            Node it = tree.lowerBound(sv);

            for (; it && Tree::prev(it) && it->value.contains(ev.s1.max()); it = Tree::prev(it)) {
            }
            if (it) {
                it = Tree::next(it);
            }

            for (; it && it->value.contains(ev.s1.max()); it = Tree::next(it)) {
                CrossingPair pair{ev.s1,it->value};
                if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                if (addIntersection(pair)) {
                    return;
                }
            }
        }
    }

    void processVERTICAL(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            for (Node it = tree.lowerBound(ev.s1); it; it = Tree::next(it)) {
                if (!ev.s1.intersects(it->value))
                    break;
                if (onlyCrossings) {
                    if (ev.s1.crosses(it->value)) {
                        CrossingPair pair{ev.s1, it->value};
                        if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                        addCrossing(pair);
                    }
                }
                else {
                    if (ev.s1.intersects(it->value)) {
                        CrossingPair pair{ev.s1, it->value};
                        if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                        addCrossing(pair);
                    }
                }
            }
        }
    }

    void processVERTICAL_interior(const std::vector<Event> &v_evts, const std::vector<Event> &r_evts, const std::vector<Event> &l_evts) {
        std::vector<std::pair<Number, Segment>> order;
        for (Event ev : l_evts) {
            order.emplace_back(ev.s1.min().y(), ev.s1);
        }
        for (Event ev : r_evts) {
            order.emplace_back(ev.s1.max().y(), ev.s1);
        }
        for (Event ev : v_evts) {
            order.emplace_back(ev.s1.min().y(), ev.s1);
            order.emplace_back(ev.s1.max().y(), ev.s1);
        }
        std::sort(order.begin(),order.end());

        for (Event ev : v_evts) {
            auto y1 = ev.s1.min().y();
            auto y2 = ev.s1.max().y();
            for (auto it = std::lower_bound(order.begin(), order.end(), std::make_pair(y1, Segment()));
                 it != order.end() && it->first < y2;
                 ++it) {
                CrossingPair pair{ev.s1, it->second};
                if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                if (pair[0] != pair[1]) {
                    addIntersection(pair);
                }
            }
        }
    }

    void processLEFT(const std::vector<Event> &evts) {
        for (Event ev : evts) {
            const Node it1 = addToTree(ev.s1);
            Node it0 = Tree::prev(it1);
            Node it2 = Tree::next(it1);

            queue.emplace(static_cast<Rational>(ev.s1.max().x()), EventEnum::RIGHT, ev.s1);
            possibleCrossing(it0, it1);
            possibleCrossing(it1, it2);

            if (!onlyCrossings) {
                while (it0 && it0->value.contains(ev.s1.min())) {
                    CrossingPair pair{it0->value, ev.s1};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                    addIntersection(pair);
                    it0 = Tree::prev(it0);
                }
                while (it2 && it2->value.contains(ev.s1.min())) {
                    CrossingPair pair{it2->value, ev.s1};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                    addIntersection(pair);
                    it2 = Tree::next(it2);
                }
                if (stopNow) {
                    break;
                }
            }
        }
    }


    void run(const std::vector<Segment> &segments) {
        // Return directly for 0 or 1 segment
        if (segments.size() <= (size_t) 1)
            return;

        initQueue(segments);
        initBbox(segments);
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
        return sorted(crossingsSet);
    }

    std::vector<CrossingPair> findIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        run(segments);
        intersectionSet.insert(crossingsSet.begin(), crossingsSet.end());

        // Insert segments sharing an endpoint
        std::map<Point,std::vector<Segment>> adjacent;
        for (const Segment &s : segments) {
            adjacent[s.min()].push_back(s);
            adjacent[s.max()].push_back(s);
        }
        for (const auto &[_,segs] : adjacent) {
            for (size_t i = 0; i+1 < segs.size(); i++) {
                for (size_t j = i+1; j < segs.size(); j++) {
                    CrossingPair pair{segs[i], segs[j]};
                    if (pair[1] < pair[0]) std::swap(pair[0],pair[1]);
                    intersectionSet.insert(pair);
                }
            }
        }

        return sorted(intersectionSet);
    }

    bool detectCrossings(const std::vector<Segment> &segments) {
        onlyCrossings = true;
        onCrossing = [] (const CrossingPair &) {return true;};
        run(segments);
        return !crossingsSet.empty();
    }

    bool detectIntersections(const std::vector<Segment> &segments) {
        onlyCrossings = false;
        onCrossing = [] (const CrossingPair &) {return true;};
        onIntersection = [] (const CrossingPair &) {return true;};
        run(segments);

        if (!crossingsSet.empty() || !intersectionSet.empty())
            return true;

        // Insert segments sharing an endpoint
        std::set<Point> adjacent;
        for (const Segment &s : segments) {
            auto [_1,b1] = adjacent.insert(s.min());
            if (!b1)
                return true;
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
        onCrossing = [&notSimple] (const CrossingPair &) {notSimple = true; return true;};
        size_t count = 0;
        size_t n = segments.size();
        onIntersection = [n,&count, &notSimple] (const CrossingPair &p) {
                if (p[0].collinear(p[1]) && p[0].interiorsIntersect(p[1])) {
                    notSimple = true; // Collinear overlap
                    return true;
                }
                const int shared = (p[0].min() == p[1].min()) + (p[0].min() == p[1].max())
                                 + (p[0].max() == p[1].min()) + (p[0].max() == p[1].max());
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
        onCrossing = [&notSimple] (const CrossingPair &) {notSimple = true; return true;};
        size_t count = 0;
        size_t n = segments.size();
        onIntersection = [n,&count, &notSimple] (const CrossingPair &p) {
                if (p[0].collinear(p[1]) && p[0].interiorsIntersect(p[1])) {
                    notSimple = true; // Collinear overlap
                    return true;
                }
                const int shared = (p[0].min() == p[1].min()) + (p[0].min() == p[1].max())
                                 + (p[0].max() == p[1].min()) + (p[0].max() == p[1].max());
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

    pgl::detail::BentleyOttmann<Rational, Segment> bo;
    return bo.findIntersections(v);
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

    pgl::detail::BentleyOttmann<Rational,Segment> bo;
    return bo.findCrossings(v);
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

    pgl::detail::BentleyOttmann<Rational,Segment> bo;
    return bo.detectIntersections(v);
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

    pgl::detail::BentleyOttmann<Rational,Segment> bo;
    return bo.detectCrossings(v);
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
