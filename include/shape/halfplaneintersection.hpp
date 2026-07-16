#pragma once

#include "shape/polygon.hpp"

/**
 * @file halfplaneintersection.hpp
 * @brief Public declaration of pgl::HalfplaneIntersection.
 *
 * HalfplaneIntersection stores the intersection of a finite set of closed
 * half-planes: a convex region that — unlike pgl::Convex — may be unbounded
 * (wedge, strip, half-plane, whole plane), may be empty, and may have
 * non-integer vertices even when every stored half-plane has integer
 * coordinates. Half-planes can be inserted at any time; redundant ones are
 * discarded.
 */

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace pgl::detail {

/**
 * @brief Pseudo-angle half of a boundary direction.
 *
 * @return `0` when the direction's angle lies in `[0, pi)` — that is, `dy > 0`
 * or `dy == 0 && dx > 0` — and `1` when it lies in `[pi, 2pi)`. A zero
 * direction (degenerate half-plane) is undefined behavior, as everywhere in
 * pgl.
 */
template <class C>
constexpr int pseudoAngleHalf(const C& dx, const C& dy) {
    const C zero{};
    return (dy > zero || (dy == zero && dx > zero)) ? 0 : 1;
}

/**
 * @brief Boundary direction of a half-plane, promoted to type @p C.
 */
template <class C, HalfplaneConcept H>
constexpr std::pair<C, C> boundaryDirection(const H& h) {
    return {static_cast<C>(h.target().x()) - static_cast<C>(h.source().x()),
            static_cast<C>(h.target().y()) - static_cast<C>(h.source().y())};
}

/**
 * @brief Cross product of the boundary directions of two half-planes.
 *
 * Positive when `b`'s boundary direction lies strictly counterclockwise from
 * `a`'s within half a turn. Degree two in the input coordinates, computed with
 * a single promotion.
 */
template <HalfplaneConcept HA, HalfplaneConcept HB>
constexpr auto directionCross(const HA& a, const HB& b) {
    using C = promoted_number_t<std::common_type_t<typename HA::NumberType, typename HB::NumberType>>;
    const auto [ax, ay] = boundaryDirection<C>(a);
    const auto [bx, by] = boundaryDirection<C>(b);
    return ax * by - ay * bx;
}

/**
 * @brief Strict pseudo-angle order of two half-plane boundary directions.
 *
 * Angles are measured counterclockwise in `[0, 2pi)` starting from the `+x`
 * axis, compared exactly with sign tests only.
 */
template <HalfplaneConcept HA, HalfplaneConcept HB>
constexpr bool directionLess(const HA& a, const HB& b) {
    using C = promoted_number_t<std::common_type_t<typename HA::NumberType, typename HB::NumberType>>;
    const auto [ax, ay] = boundaryDirection<C>(a);
    const auto [bx, by] = boundaryDirection<C>(b);
    const int halfA = pseudoAngleHalf(ax, ay);
    const int halfB = pseudoAngleHalf(bx, by);
    if (halfA != halfB) {
        return halfA < halfB;
    }
    return ax * by - ay * bx > C{};
}

/**
 * @brief Dot product of the boundary directions of two half-planes.
 */
template <HalfplaneConcept HA, HalfplaneConcept HB>
constexpr auto directionDot(const HA& a, const HB& b) {
    using C = promoted_number_t<std::common_type_t<typename HA::NumberType, typename HB::NumberType>>;
    const auto [ax, ay] = boundaryDirection<C>(a);
    const auto [bx, by] = boundaryDirection<C>(b);
    return ax * bx + ay * by;
}

/**
 * @brief Tests whether the closed direction arc `[dir(from), dir(to)]`
 * (counterclockwise, of length below pi — possibly a single direction)
 * contains the boundary direction of `x`.
 */
template <HalfplaneConcept HF, HalfplaneConcept HT, HalfplaneConcept HX>
constexpr bool arcContainsDirection(const HF& from, const HT& to, const HX& x) {
    const auto crossFrom = directionCross(from, x);
    using CF = decltype(crossFrom);
    if (crossFrom < CF{}) {
        return false;
    }
    if (crossFrom == CF{} && directionDot(from, x) <= decltype(directionDot(from, x)){}) {
        return false;  // opposite to the arc start, not on it
    }
    const auto crossTo = directionCross(x, to);
    using CT = decltype(crossTo);
    if (crossTo < CT{}) {
        return false;
    }
    if (crossTo == CT{} && directionDot(to, x) <= decltype(directionDot(to, x)){}) {
        return false;  // opposite to the arc end
    }
    return true;
}

/**
 * @brief Tests whether two half-planes have the same boundary direction.
 */
template <HalfplaneConcept HA, HalfplaneConcept HB>
constexpr bool directionEqual(const HA& a, const HB& b) {
    using C = promoted_number_t<std::common_type_t<typename HA::NumberType, typename HB::NumberType>>;
    const auto [ax, ay] = boundaryDirection<C>(a);
    const auto [bx, by] = boundaryDirection<C>(b);
    return pseudoAngleHalf(ax, ay) == pseudoAngleHalf(bx, by) && ax * by - ay * bx == C{};
}

/**
 * @brief Determinant of the three boundary-line coefficient rows.
 *
 * Each half-plane `{p : a·p <= b}` contributes the row `(a_x, a_y, b)` with
 * `a = (t_y - s_y, s_x - t_x)` (the outward normal) and `b = cross(s, t)`,
 * where `s -> t` is the oriented boundary. Three concurrent boundary lines
 * give determinant zero — this is the dual of the point orientation test.
 *
 * @warning Degree four in the input coordinates; they are promoted twice, so
 * `int` coordinates up to roughly `2^30` stay exact while `int64_t`
 * coordinates beyond roughly `2^30` may overflow the doubly promoted type.
 */
template <HalfplaneConcept H1, HalfplaneConcept H2, HalfplaneConcept H3>
constexpr auto boundaryLinesDeterminant(const H1& h1, const H2& h2, const H3& h3) {
    using Common = std::common_type_t<typename H1::NumberType, typename H2::NumberType, typename H3::NumberType>;
    using C = promoted_number_t<promoted_number_t<Common>>;
    const auto row = [](const auto& h) {
        const C sx = static_cast<C>(h.source().x());
        const C sy = static_cast<C>(h.source().y());
        const C tx = static_cast<C>(h.target().x());
        const C ty = static_cast<C>(h.target().y());
        struct Row { C ax, ay, b; };
        return Row{ty - sy, sx - tx, sx * ty - sy * tx};
    };
    const auto r1 = row(h1);
    const auto r2 = row(h2);
    const auto r3 = row(h3);
    return r1.ax * (r2.ay * r3.b - r3.ay * r2.b)
         - r1.ay * (r2.ax * r3.b - r3.ax * r2.b)
         + r1.b * (r2.ax * r3.ay - r3.ax * r2.ay);
}

/**
 * @brief Position of the vertex `∂h1 ∩ ∂h2` relative to `h3`, without
 * constructing the (generally rational) vertex.
 *
 * @return `+1` when the vertex is strictly inside `h3`, `0` when it lies on
 * `∂h3`, `-1` when it is strictly outside. Parallel `h1`, `h2` are undefined
 * behavior (there is no vertex).
 */
template <HalfplaneConcept H1, HalfplaneConcept H2, HalfplaneConcept H3>
constexpr int vertexSide(const H1& h1, const H2& h2, const H3& h3) {
    const auto det = boundaryLinesDeterminant(h1, h2, h3);
    const auto cross = directionCross(h1, h2);
    const auto zero = decltype(det){};
    const auto crossZero = decltype(cross){};
    const int detSign = det > zero ? 1 : det < zero ? -1 : 0;
    const int crossSign = cross > crossZero ? 1 : cross < crossZero ? -1 : 0;
    return detSign * crossSign;
}

}  // namespace pgl::detail


namespace pgl {

template <class PointType = Point<>, class Label>
struct HalfplaneIntersection;

HalfplaneIntersection() -> HalfplaneIntersection<Point<>, NoLabel>;

template <HalfplaneConcept H>
HalfplaneIntersection(const H&) -> HalfplaneIntersection<typename H::PointType, NoLabel>;

template <std::ranges::input_range Range>
    requires detail::is_halfplane_v<std::ranges::range_value_t<Range>>
HalfplaneIntersection(Range&&) -> HalfplaneIntersection<typename std::remove_cvref_t<std::ranges::range_value_t<Range>>::PointType, NoLabel>;

template <std::ranges::input_range Range>
    requires detail::is_halfplane_v<std::ranges::range_value_t<Range>>
HalfplaneIntersection(Range&&, bool) -> HalfplaneIntersection<typename std::remove_cvref_t<std::ranges::range_value_t<Range>>::PointType, NoLabel>;

template <RectangleConcept R>
HalfplaneIntersection(const R&) -> HalfplaneIntersection<typename R::PointType, NoLabel>;

template <TriangleConcept T>
HalfplaneIntersection(const T&) -> HalfplaneIntersection<typename T::PointType, NoLabel>;

template <ConvexConcept C>
HalfplaneIntersection(const C&) -> HalfplaneIntersection<typename C::PointType, NoLabel>;

/**
 * @brief Intersection of a finite set of closed half-planes.
 *
 * The region is stored as a vector of pgl::Halfplane sorted by the pseudo-angle
 * of their oriented boundary directions (counterclockwise from the `+x` axis).
 * The stored set is kept canonical:
 *
 * - no stored half-plane is redundant (each contributes to the boundary),
 * - at most one half-plane per boundary direction (the most restrictive wins),
 * - when the intersection becomes empty the vector is cleared and a sticky
 *   empty flag is set, so all empty regions compare equal.
 *
 * Consecutive half-planes (cyclically) define the region's vertices in
 * counterclockwise order. The vertices are implicit and generally rational:
 * constructive accessors take a `ResultNumber` template parameter, following
 * the usual pgl pattern (`vertex<pgl::Rational<int64_t>>(i)` for exact
 * results).
 *
 * A default-constructed HalfplaneIntersection is the **whole plane** (the
 * intersection of an empty family of half-planes) — note that this is the
 * opposite convention of `Convex{}`, which is the empty set.
 *
 * Lower-dimensional (degenerate) regions — a line, ray, segment, or point
 * arising from touching constraints — are supported: predicates remain
 * correct, but the stored half-plane list is no longer a canonical function of
 * the point set, so `operator==` may distinguish equal degenerate regions
 * built from different constraints.
 *
 * @tparam PointType Defining point type of the stored half-planes.
 */
template <class PointType_, class TLabel>
struct HalfplaneIntersection {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
    using HalfplaneType = Halfplane<PointType>;

    static_assert(detail::is_point_v<PointType>, "HalfplaneIntersection requires pgl::Point defining points");

    /**
     * @brief Creates the whole plane (the intersection of no half-planes).
     */
    constexpr HalfplaneIntersection() = default;

    /**
     * @brief Creates the region bounded by a single half-plane.
     */
    template <HalfplaneConcept OtherHalfplane>
    constexpr explicit HalfplaneIntersection(const OtherHalfplane& halfplane) {
        insert(halfplane);
    }

    /**
     * @brief Creates the intersection of a range of half-planes.
     *
     * The untrusted path inserts the half-planes one by one, discarding
     * redundant ones and detecting emptiness; the trusted path adopts the
     * range as-is.
     *
     * Complexity: O(n log n) comparisons (plus O(n^2) element moves in the
     * worst case) untrusted, O(n) trusted.
     *
     * @param halfplanes Range of half-planes to intersect.
     * @param trusted Set to true if the half-planes are already sorted by
     * boundary pseudo-angle, mutually non-redundant, feasible, and define a
     * region with nonempty interior.
     */
    template <std::ranges::input_range Range = std::initializer_list<HalfplaneType>>
        requires std::convertible_to<std::ranges::range_value_t<Range>, HalfplaneType>
    constexpr explicit HalfplaneIntersection(Range&& halfplanes, bool trusted = false) {
        if (trusted) {
            for (const auto& h : halfplanes) {
                halfplanes_.push_back(h);
            }
        } else {
            for (const auto& h : halfplanes) {
                insert(h);
            }
        }
    }

    /**
     * @brief Creates the region of a rectangle as four half-planes.
     */
    template <RectangleConcept OtherRectangle>
    constexpr explicit HalfplaneIntersection(const OtherRectangle& rectangle) {
        const PointType lo(rectangle.min());
        const PointType hi(rectangle.max());
        const PointType lohi(lo.x(), hi.y());
        const PointType hilo(hi.x(), lo.y());
        // Directions 0, 90, 180, 270 degrees: already in sorted order.
        halfplanes_.push_back(HalfplaneType(lo, hilo));
        halfplanes_.push_back(HalfplaneType(hilo, hi));
        halfplanes_.push_back(HalfplaneType(hi, lohi));
        halfplanes_.push_back(HalfplaneType(lohi, lo));
        degenerate_ = lo.x() == hi.x() || lo.y() == hi.y();
    }

    /**
     * @brief Creates the region of a triangle as three half-planes.
     *
     * A degenerate (collinear) triangle is undefined behavior.
     */
    template <TriangleConcept OtherTriangle>
    constexpr explicit HalfplaneIntersection(const OtherTriangle& triangle) {
        assert(!triangle.isDegenerate());
        // Triangle vertices are CCW when non-degenerate, so each edge's
        // half-plane has the interior on its left.
        for (std::size_t i = 0; i < 3; ++i) {
            halfplanes_.push_back(HalfplaneType(PointType(triangle[i]), PointType(triangle[(i + 1) % 3])));
        }
        canonicalizeSorted();
    }

    /**
     * @brief Creates the region of a convex polygon as its edge half-planes.
     *
     * An empty convex polygon produces the empty region; a degenerate one (a
     * point or a segment) produces the corresponding degenerate region.
     */
    template <ConvexConcept OtherConvex>
    constexpr explicit HalfplaneIntersection(const OtherConvex& convex) {
        const std::size_t n = convex.size();
        if (n == 0) {
            empty_ = true;
            return;
        }
        if (convex.isDegenerate()) {
            // A point or a segment: clamp both slab directions. The convex
            // polygon normalizes collinear input down to its two extremes.
            const PointType a(convex[0]);
            const PointType b(convex[n - 1]);
            degenerate_ = true;
            if (a == b) {
                // Axis-aligned point slab.
                const PointType ax(a.x() + NumberType(1), a.y());
                const PointType ay(a.x(), a.y() + NumberType(1));
                halfplanes_.push_back(HalfplaneType(a, ax));
                halfplanes_.push_back(HalfplaneType(ax, a));
                halfplanes_.push_back(HalfplaneType(a, ay));
                halfplanes_.push_back(HalfplaneType(ay, a));
            } else {
                // The segment a-b: the line slab plus a perpendicular clamp
                // through each endpoint (rotating the direction by 90 degrees
                // keeps integer coordinates).
                const NumberType dx = b.x() - a.x();
                const NumberType dy = b.y() - a.y();
                const PointType aPerp(a.x() - dy, a.y() + dx);
                const PointType bPerp(b.x() - dy, b.y() + dx);
                halfplanes_.push_back(HalfplaneType(a, b));
                halfplanes_.push_back(HalfplaneType(b, a));
                halfplanes_.push_back(HalfplaneType(aPerp, a));
                halfplanes_.push_back(HalfplaneType(b, bPerp));
            }
            canonicalizeSorted();
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            halfplanes_.push_back(HalfplaneType(PointType(convex[i]), PointType(convex[(i + 1) % n])));
        }
        canonicalizeSorted();
    }

    /**
     * @brief Converts a half-plane intersection with a compatible point type.
     */
    template <PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr HalfplaneIntersection(const HalfplaneIntersection<OtherPointType, OtherLabelType>& other)
        : empty_(other.isEmpty()), degenerate_(other.isEmpty() ? false : other.isDegenerate()) {
        halfplanes_.reserve(other.size());
        for (const auto& h : other) {
            halfplanes_.push_back(HalfplaneType(h));
        }
        label_ = detail::copyLabel<LabelType>(other);
    }

    /** @brief Assigns from a half-plane intersection with a compatible point type. */
    template <PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr HalfplaneIntersection& operator=(const HalfplaneIntersection<OtherPointType, OtherLabelType>& other) {
        halfplanes_.clear();
        halfplanes_.reserve(other.size());
        for (const auto& h : other) {
            halfplanes_.push_back(HalfplaneType(h));
        }
        empty_ = other.isEmpty();
        degenerate_ = empty_ ? false : other.isDegenerate();
        label_ = detail::copyLabel<LabelType>(other);
        resetCache();
        return *this;
    }

    /**
     * @brief Returns the label.
     *
     * The label is mutable even through a const region: it is metadata that
     * does not participate in equality, hashing, or geometric predicates.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() const {
        return label_;
    }

    /**
     * @brief Intersects the region with one more half-plane.
     *
     * The half-plane is discarded when it is redundant (the region is already
     * contained in it). When it makes the intersection empty, the region
     * switches to the sticky empty state. Otherwise the half-plane is stored
     * and any stored half-planes it makes redundant are removed.
     *
     * Complexity: O(log n) comparisons amortized, plus O(n) vector element
     * moves in the worst case.
     *
     * @return `true` if the region changed, `false` if the half-plane was
     * discarded as redundant.
     */
    template <HalfplaneConcept OtherHalfplane>
    constexpr bool insert(const OtherHalfplane& other) {
        if (empty_) {
            return false;
        }
        const HalfplaneType h(PointType(other.source()), PointType(other.target()));
        assert(!h.isDegenerate());
        if (halfplanes_.empty()) {
            halfplanes_.push_back(h);
            resetCache();
            return true;
        }
        // sup a·p <= b over the region means the region is already inside h.
        const SupStatus supremum = supStatus(h);
        if (supremum == SupStatus::below || supremum == SupStatus::on) {
            return false;
        }
        // The infimum side: inf a·p > b means no point of the region satisfies
        // h; inf == b (attained) means the region collapses onto the minimum
        // face, which has empty interior.
        const SupStatus infimum = supStatus(h.opposite());
        if (infimum == SupStatus::below) {
            halfplanes_.clear();
            empty_ = true;
            degenerate_ = false;
            resetCache();
            return true;
        }
        if (infimum == SupStatus::on) {
            degenerate_ = true;
        }
        std::size_t pos = linearLowerBound(h);
        if (pos < halfplanes_.size() && detail::directionEqual(halfplanes_[pos], h)) {
            // Same direction: h is strictly more restrictive, otherwise the
            // supremum test above would have discarded it.
            halfplanes_[pos] = h;
        } else {
            if (pos > halfplanes_.size()) {
                pos = halfplanes_.size();
            }
            halfplanes_.insert(halfplanes_.begin() + static_cast<std::ptrdiff_t>(pos), h);
        }
        // Cascade: a stored half-plane g between angular neighbors pred and
        // succ is redundant exactly when the wedge pred ∩ succ fits inside g,
        // which (for an angular gap below pi) reduces to the wedge apex lying
        // weakly inside g. Deletions are paid for once, so the cascade is
        // amortized O(1) tests per insertion.
        while (halfplanes_.size() >= 3) {
            const std::size_t s1 = nextIndex(pos);
            const std::size_t s2 = nextIndex(s1);
            if (s2 == pos) {
                break;
            }
            if (detail::directionCross(halfplanes_[pos], halfplanes_[s2]) > 0 &&
                detail::vertexSide(halfplanes_[pos], halfplanes_[s2], halfplanes_[s1]) >= 0) {
                halfplanes_.erase(halfplanes_.begin() + static_cast<std::ptrdiff_t>(s1));
                if (s1 < pos) {
                    --pos;
                }
            } else {
                break;
            }
        }
        while (halfplanes_.size() >= 3) {
            const std::size_t p1 = prevIndex(pos);
            const std::size_t p2 = prevIndex(p1);
            if (p2 == pos) {
                break;
            }
            if (detail::directionCross(halfplanes_[p2], halfplanes_[pos]) > 0 &&
                detail::vertexSide(halfplanes_[p2], halfplanes_[pos], halfplanes_[p1]) >= 0) {
                halfplanes_.erase(halfplanes_.begin() + static_cast<std::ptrdiff_t>(p1));
                if (p1 < pos) {
                    --pos;
                }
            } else {
                break;
            }
        }
        resetCache();
        return true;
    }

    /**
     * @brief Returns the number of stored (non-redundant) half-planes.
     */
    constexpr std::size_t size() const {
        return halfplanes_.size();
    }

    /**
     * @brief Accesses a stored half-plane by index, in boundary
     * (counterclockwise pseudo-angle) order.
     */
    constexpr const HalfplaneType& operator[](std::size_t index) const {
        assert(index < size());
        return halfplanes_[index];
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end.
     */
    constexpr const HalfplaneType& get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == halfplane`, or
     * `-1` if no stored half-plane equals it.
     *
     * Complexity: O(log n).
     */
    constexpr std::ptrdiff_t index(const HalfplaneType& halfplane) const {
        const std::size_t pos = linearLowerBound(halfplane);
        if (pos < size() && halfplanes_[pos] == halfplane) {
            return static_cast<std::ptrdiff_t>(pos);
        }
        return -1;
    }

    /** @brief Returns a copy of the stored half-planes, in boundary order. */
    constexpr std::vector<HalfplaneType> halfplanes() const {
        return halfplanes_;
    }

    constexpr auto begin() const { return halfplanes_.cbegin(); }
    constexpr auto cbegin() const { return halfplanes_.cbegin(); }
    constexpr auto end() const { return halfplanes_.cend(); }
    constexpr auto cend() const { return halfplanes_.cend(); }

    /**
     * @brief Returns whether the region is the empty set.
     */
    constexpr bool isEmpty() const {
        return empty_;
    }

    /**
     * @brief Returns whether the region is the whole plane (no half-planes).
     */
    constexpr bool isPlane() const {
        return !empty_ && halfplanes_.empty();
    }

    /**
     * @brief Returns whether the region has empty interior (it is empty or
     * lower-dimensional: a line, ray, segment, or point).
     */
    constexpr bool isDegenerate() const {
        return empty_ || degenerate_;
    }

    /**
     * @brief Returns whether the region is bounded.
     *
     * The empty region is bounded; the region is otherwise bounded exactly
     * when there are at least three half-planes and every cyclic gap between
     * consecutive boundary directions is below pi.
     *
     * Complexity: O(n).
     */
    constexpr bool isBounded() const {
        if (empty_) {
            return true;
        }
        const std::size_t n = size();
        if (n < 3) {
            return false;
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (!(detail::directionCross(halfplanes_[i], halfplanes_[nextIndex(i)]) > 0)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns the number of vertices of the region.
     *
     * A vertex exists between cyclically consecutive half-planes whose
     * boundary-direction gap is below pi. Complexity: O(n).
     */
    constexpr std::size_t vertexCount() const {
        if (empty_) {
            return 0;
        }
        const std::size_t n = size();
        if (n < 2) {
            return 0;
        }
        std::size_t count = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (detail::directionCross(halfplanes_[i], halfplanes_[nextIndex(i)]) > 0) {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Returns whether the half-plane pair `(i, i+1)` (cyclically)
     * defines a vertex of the region.
     */
    constexpr bool vertexExists(std::size_t i) const {
        if (empty_ || size() < 2) {
            return false;
        }
        return detail::directionCross(halfplanes_[i], halfplanes_[nextIndex(i)]) > 0;
    }

    /**
     * @brief Returns the vertex between half-planes `i` and `i+1`
     * (cyclically).
     *
     * The pair must define a vertex (see @ref vertexExists). For a bounded
     * region, ascending `i` walks the vertices counterclockwise.
     *
     * @tparam ResultNumber Coordinate type of the returned point (default:
     * NumberType).
     * @warning Divides coordinates after casting to ResultNumber, so the
     * result is generally inexact for integer types; request
     * `pgl::Rational` coordinates for an exact vertex.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber, typename PointType::LabelType> vertex(std::size_t i) const {
        assert(vertexExists(i));
        const auto isec = halfplanes_[i].asLine().template intersection<ResultNumber>(
            halfplanes_[nextIndex(i)].asLine());
        assert(isec && isec->index() == 0);
        return std::get<0>(*isec);
    }

    /**
     * @brief Returns every vertex of the region, in pair-index order (for a
     * bounded region: counterclockwise).
     *
     * @copydetails vertex(std::size_t) const
     */
    template <class ResultNumber = NumberType>
    constexpr std::vector<Point<ResultNumber, typename PointType::LabelType>> vertices() const {
        std::vector<Point<ResultNumber, typename PointType::LabelType>> result;
        if (empty_ || size() < 2) {
            return result;
        }
        for (std::size_t i = 0; i < size(); ++i) {
            if (vertexExists(i)) {
                result.push_back(vertex<ResultNumber>(i));
            }
        }
        return result;
    }

    /**
     * @brief Returns the boundary contribution of half-plane `i` as a typed
     * one-dimensional shape.
     *
     * The edge lies on `∂halfplane(i)` clipped by the two angular neighbors: a
     * @ref Segment when both neighbor vertices exist, a @ref Ray when only one
     * does, and the whole boundary @ref Line otherwise. A degenerate region
     * may produce a zero-length segment.
     *
     * @warning Divides coordinates after casting to ResultNumber; request
     * `pgl::Rational` coordinates for exact endpoints.
     */
    template <class ResultNumber = NumberType>
    constexpr std::variant<Segment<Point<ResultNumber, typename PointType::LabelType>>,
                           Ray<Point<ResultNumber, typename PointType::LabelType>>,
                           Line<Point<ResultNumber, typename PointType::LabelType>>>
    edge(std::size_t i) const {
        using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
        assert(!empty_ && i < size());
        const std::size_t prev = prevIndex(i);
        const bool hasStart = size() >= 2 && detail::directionCross(halfplanes_[prev], halfplanes_[i]) > 0;
        const bool hasEnd = vertexExists(i);
        const auto& h = halfplanes_[i];
        if (hasStart && hasEnd) {
            return Segment<ResultPoint>(vertex<ResultNumber>(prev), vertex<ResultNumber>(i));
        }
        const ResultPoint source(h.source());
        const ResultPoint target(h.target());
        const ResultNumber dx = target.x() - source.x();
        const ResultNumber dy = target.y() - source.y();
        if (hasStart) {
            const ResultPoint start = vertex<ResultNumber>(prev);
            return Ray<ResultPoint>(start, ResultPoint(start.x() + dx, start.y() + dy));
        }
        if (hasEnd) {
            const ResultPoint finish = vertex<ResultNumber>(i);
            return Ray<ResultPoint>(finish, ResultPoint(finish.x() - dx, finish.y() - dy));
        }
        return Line<ResultPoint>(source, target);
    }

    /**
     * @brief Returns the region as a convex polygon.
     *
     * The empty region converts to the empty convex polygon. Throws
     * `std::logic_error` when the region is unbounded (including the whole
     * plane).
     *
     * @warning Divides coordinates after casting to ResultNumber; request
     * `pgl::Rational` coordinates for exact vertices.
     */
    template <class ResultNumber = NumberType>
    constexpr Convex<Point<ResultNumber, typename PointType::LabelType>> asConvex() const {
        if (empty_) {
            return {};
        }
        if (!isBounded()) {
            throw std::logic_error("HalfplaneIntersection::asConvex requires a bounded region");
        }
        return Convex<Point<ResultNumber, typename PointType::LabelType>>(vertices<ResultNumber>());
    }

    /**
     * @brief Tests equality of the stored regions.
     *
     * All empty regions are equal. For full-dimensional regions the stored
     * non-redundant half-planes are a canonical function of the point set, so
     * this is geometric equality; for lower-dimensional regions it is
     * representational (same point set *and* same stored constraints).
     */
    [[nodiscard]] constexpr bool operator==(const HalfplaneIntersection& other) const {
        if (empty_ != other.empty_) {
            return false;
        }
        if (empty_) {
            return true;
        }
        if (halfplanes_.size() != other.halfplanes_.size()) {
            return false;
        }
        for (std::size_t i = 0; i < halfplanes_.size(); ++i) {
            if (!(halfplanes_[i] == other.halfplanes_[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Provides an ordering compatible with @ref operator==.
     */
    [[nodiscard]] constexpr auto operator<=>(const HalfplaneIntersection& other) const {
        if (auto cmp = empty_ <=> other.empty_; cmp != 0) {
            return cmp;
        }
        if (empty_) {
            return std::strong_ordering::equal;
        }
        if (auto cmp = halfplanes_.size() <=> other.halfplanes_.size(); cmp != 0) {
            return cmp;
        }
        for (std::size_t i = 0; i < halfplanes_.size(); ++i) {
            if (auto cmp = halfplanes_[i] <=> other.halfplanes_[i]; cmp != 0) {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /**
     * @brief Computes the bounding box of the region.
     *
     * Throws `std::logic_error` when the region is empty or unbounded. With an
     * integer @p ResultNumber the box is rounded outward (floors for the
     * minimum corner, ceilings for the maximum corner), so it always encloses
     * the region.
     *
     * Complexity: O(n) (boundedness check) plus O(log n) extreme queries.
     */
    template <class ResultNumber = NumberType>
    constexpr Rectangle<Point<ResultNumber, typename PointType::LabelType>> bbox() const;

    /**
     * @brief Computes the floating-point bounding box of the region.
     *
     * Throws `std::logic_error` when the region is empty or unbounded.
     */
    template <std::floating_point ResultNumber = double>
    constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /** @brief Translates the region by the given point in place. */
    template <PointConcept OtherPoint>
    constexpr HalfplaneIntersection& operator+=(const OtherPoint& translation);

    /** @brief Translates the region by the negation of the given point in place. */
    template <PointConcept OtherPoint>
    constexpr HalfplaneIntersection& operator-=(const OtherPoint& translation);

    /** @brief Scales the region around the origin by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr HalfplaneIntersection& operator*=(const Scalar& scalar);

    /** @brief Divides the region coordinates by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr HalfplaneIntersection& operator/=(const Scalar& scalar);

    /** @brief Returns the region rotated by 90k degrees around the origin. */
    [[nodiscard]] constexpr HalfplaneIntersection rotated90(int k = 1) const;

    /** @brief Rotates the region by 90k degrees around the origin in place. */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the region with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr HalfplaneIntersection scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the region's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the region with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr HalfplaneIntersection scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the region's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the region with its x-coordinates divided by a divisor. @warning Inexact for integer coordinates. */
    template <class OtherNumber>
    [[nodiscard]] constexpr HalfplaneIntersection scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the region's x-coordinates by a divisor in place. @warning Inexact for integer coordinates. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the region with its y-coordinates divided by a divisor. @warning Inexact for integer coordinates. */
    template <class OtherNumber>
    [[nodiscard]] constexpr HalfplaneIntersection scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the region's y-coordinates by a divisor in place. @warning Inexact for integer coordinates. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    // --- predicates (defined in the implementation layer) ---

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool intersects(const OtherDisk& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool intersects(const OtherConvex& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool intersects(const OtherChain& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool intersects(const OtherPolyline& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool intersects(const OtherPolygon& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool intersects(const OtherRegion& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherTriangle& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherDisk& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherConvex& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherChain& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPolyline& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPolygon& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRegion& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool separates(const OtherPolyline& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool separates(const OtherRegion& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool crosses(const OtherDisk& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool crosses(const OtherConvex& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool crosses(const OtherChain& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool crosses(const OtherPolyline& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool crosses(const OtherPolygon& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool crosses(const OtherRegion& other) const;

    // The empty set is a subset of every shape and disjoint from all of them.
    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    // Runtime Shape argument: visit the wrapped alternative and re-dispatch to
    // the matching per-shape overload (defined in the implementation layer).

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                       Segment<Point<ResultNumber, typename PointType::LabelType>>,
                                                       Ray<Point<ResultNumber, typename PointType::LabelType>>,
                                                       Line<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                       Segment<Point<ResultNumber, typename PointType::LabelType>>,
                                                       Ray<Point<ResultNumber, typename PointType::LabelType>>,
                                                       Line<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                       Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                       Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                       Segment<Point<ResultNumber, typename PointType::LabelType>>,
                                                       Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    /**
     * @brief Returns the intersection with a half-plane, as a half-plane
     * intersection.
     *
     * The result type is closed under this operation and requires no
     * division, so the intersection is exact when `ResultNumber` can represent
     * the input coordinates. The result may be empty (see @ref isEmpty) —
     * no `std::optional` wrapper is needed.
     */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr HalfplaneIntersection<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherHalfplane& other) const;

    /**
     * @brief Returns the intersection with a rectangle, as a half-plane
     * intersection.
     *
     * The result type is closed under intersecting with any convex polygonal
     * region and requires no division, so the result is exact when
     * `ResultNumber` can represent the input coordinates. The result may be
     * empty (see @ref isEmpty) — no `std::optional` wrapper is needed.
     */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr HalfplaneIntersection<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherRectangle& other) const;

    /**
     * @brief Returns the intersection with a triangle, as a half-plane
     * intersection.
     * @copydetails intersection(const OtherRectangle&) const
     */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr HalfplaneIntersection<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherTriangle& other) const;

    /**
     * @brief Returns the intersection with a convex polygon, as a half-plane
     * intersection.
     * @copydetails intersection(const OtherRectangle&) const
     */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr HalfplaneIntersection<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherConvex& other) const;

    /**
     * @brief Returns the intersection with another half-plane intersection.
     * @copydetails intersection(const OtherRectangle&) const
     */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr HalfplaneIntersection<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherRegion& other) const;

    // --- distances (defined in the implementation layer) ---
    //
    // Zero when the shapes intersect; otherwise the minimum over the region's
    // boundary edges of the edge-to-shape distance. Complexity: O(n) edge
    // queries for n stored half-planes. The empty region has no distance to
    // anything; querying it is undefined behavior.

    /** @brief Returns the squared Euclidean distance to the given shape. @warning Divides after casting to ResultNumber; request a floating-point or pgl::Rational result type for an accurate value. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto squaredDistance(const OtherHalfplane& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRectangle& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherTriangle& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto squaredDistance(const OtherConvex& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto squaredDistance(const OtherChain& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolyline& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolygon& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given disk.
     *
     * Always returns `double`, like @ref Disk::squaredDistance: the gap to a
     * circle is generally irrational.
     */
    template <class ResultNumber = NumberType, DiskConcept OtherDisk>
    [[nodiscard]] double squaredDistance(const OtherDisk& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRegion& other) const;

    /** @brief Returns the Manhattan (L1) distance to the given shape. @warning Divides after casting to ResultNumber; request a floating-point or pgl::Rational result type for an accurate value. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherSegment& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedSegment& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherLine& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedLine& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceL1(const OtherRay& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceL1(const OtherHalfplane& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherRectangle& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherTriangle& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceL1(const OtherConvex& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceL1(const OtherChain& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolyline& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolygon& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr auto distanceL1(const OtherRegion& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape, using
     * symmetry to re-dispatch through the wrapper's own `distanceL1`.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (L∞) distance to the given shape. @warning Divides after casting to ResultNumber; request a floating-point or pgl::Rational result type for an accurate value. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherSegment& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedSegment& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherLine& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedLine& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRay& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceLInf(const OtherHalfplane& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRectangle& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherTriangle& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceLInf(const OtherConvex& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceLInf(const OtherChain& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolyline& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolygon& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRegion& other) const;

    /**
     * @brief Returns the Chebyshev (L∞) distance to the given shape, using
     * symmetry to re-dispatch through the wrapper's own `distanceLInf`.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    // --- measures (defined in the implementation layer) ---

    /**
     * @brief Returns twice the area of the region.
     *
     * Throws `std::logic_error` when the region is unbounded (including the
     * whole plane); the empty and degenerate regions have area zero.
     *
     * @warning Divides coordinates after casting to ResultNumber when
     * computing the vertices; request a pgl::Rational result type for an
     * exact value.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr auto twiceArea() const;

    /**
     * @brief Returns the area of the region.
     * @copydetails twiceArea() const
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr auto area() const;

    /**
     * @brief Returns the centroid of the region.
     *
     * Throws `std::logic_error` when the region is unbounded (including the
     * whole plane). The centroid of the empty region is the origin, mirroring
     * `Convex`.
     *
     * @warning Divides coordinates after casting to ResultNumber; request a
     * floating-point or pgl::Rational result type for an accurate value.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> centroid() const;

    /**
     * @brief Returns a representative point of the region: a point of its
     * interior when the region is full-dimensional, and a point of the region
     * otherwise.
     *
     * The empty region has no points; querying it is undefined behavior.
     *
     * @warning Divides coordinates after casting to ResultNumber; request a
     * pgl::Rational result type for an exact witness.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Tests whether an exact interior witness of this region lies in
     * the interior of the given shape.
     *
     * Precondition: the region is full-dimensional (see @ref isDegenerate).
     */
    template <class OtherShape>
    [[nodiscard]] constexpr bool pointInsideInteriorContainedIn(const OtherShape& shape) const;

  private:
    std::vector<HalfplaneType> halfplanes_{};
    bool empty_ = false;
    bool degenerate_ = false;
    [[no_unique_address]] mutable LabelType label_{};

    // Memoized hash, mirroring Convex: hashUnset_ means "not yet computed" and
    // the one true hash colliding with it is remapped to hashUnset_ - 1.
    static constexpr std::size_t hashUnset_ = pgl::detail::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<HalfplaneIntersection>;

    template <class OtherPointType, class OtherLabelType>
    friend struct HalfplaneIntersection;

    constexpr void resetCache() const {
        hash_ = hashUnset_;
    }

    constexpr std::size_t nextIndex(std::size_t i) const {
        return i + 1 < halfplanes_.size() ? i + 1 : 0;
    }

    constexpr std::size_t prevIndex(std::size_t i) const {
        return i == 0 ? halfplanes_.size() - 1 : i - 1;
    }

    // Restores the sorted-by-pseudo-angle invariant after bulk construction or
    // a transformation. The input is assumed pairwise direction-distinct.
    constexpr void canonicalizeSorted() {
        std::sort(halfplanes_.begin(), halfplanes_.end(),
                  [](const HalfplaneType& a, const HalfplaneType& b) { return detail::directionLess(a, b); });
        resetCache();
    }

    // First stored index whose boundary direction is >= the query's in the
    // linear pseudo-angle order; may equal size().
    template <HalfplaneConcept Query>
    constexpr std::size_t linearLowerBound(const Query& query) const {
        const auto it = std::lower_bound(
            halfplanes_.begin(), halfplanes_.end(), query,
            [](const HalfplaneType& element, const Query& value) { return detail::directionLess(element, value); });
        return static_cast<std::size_t>(it - halfplanes_.begin());
    }

    // First stored index whose boundary direction is > the query's in the
    // linear pseudo-angle order; may equal size().
    template <HalfplaneConcept Query>
    constexpr std::size_t linearUpperBound(const Query& query) const {
        const auto it = std::upper_bound(
            halfplanes_.begin(), halfplanes_.end(), query,
            [](const Query& value, const HalfplaneType& element) { return detail::directionLess(value, element); });
        return static_cast<std::size_t>(it - halfplanes_.begin());
    }

    // Index of the stored half-plane with the query's boundary direction, or
    // -1 when there is none.
    template <HalfplaneConcept Query>
    constexpr std::ptrdiff_t sameDirectionIndex(const Query& query) const {
        const std::size_t pos = linearLowerBound(query);
        if (pos < halfplanes_.size() && detail::directionEqual(halfplanes_[pos], query)) {
            return static_cast<std::ptrdiff_t>(pos);
        }
        return -1;
    }

    // The stored half-planes whose boundary direction lies strictly inside the
    // open half-circle counterclockwise from the query's direction — exactly
    // those whose outward normal has positive dot product with it, i.e. the
    // constraints imposing an upper bound on the parameter along the query
    // line. Returns {first cyclic index, count}; the range is contiguous in
    // cyclic order. Precondition: size() >= 1.
    template <HalfplaneConcept Query>
    constexpr std::pair<std::size_t, std::size_t> leftArc(const Query& query) const {
        const std::size_t n = halfplanes_.size();
        std::size_t lo = linearUpperBound(query);
        if (lo == n) {
            lo = 0;
        }
        std::size_t hi = linearLowerBound(query.opposite());
        if (hi == n) {
            hi = 0;
        }
        std::size_t count = (hi + n - lo) % n;
        if (count == 0 && detail::directionCross(query, halfplanes_[lo]) > 0) {
            count = n;  // every stored direction lies inside the half-circle
        }
        return {lo, count};
    }

    // Among the leftArc constraints, the one whose boundary the directed query
    // line crosses first (the binding upper bound). Returns -1 when the arc is
    // empty (the line never leaves the region going forward).
    // Precondition: size() >= 1 and the query is not degenerate.
    template <HalfplaneConcept Query>
    constexpr std::ptrdiff_t exitConstraint(const Query& query) const {
        const auto [lo, count] = leftArc(query);
        if (count == 0) {
            return -1;
        }
        const std::size_t n = halfplanes_.size();
        // The crossing parameters along the arc first decrease, then increase
        // (the arc's boundary lines wrap a convex chain), so a valley binary
        // search finds the minimum. The consecutive-pair comparison is the
        // vertex side test: the shared vertex strictly outside the query
        // half-plane means the parameters are still decreasing.
        std::size_t first = 0;
        std::size_t last = count - 1;
        while (first < last) {
            const std::size_t mid = first + (last - first) / 2;
            const std::size_t i = (lo + mid) % n;
            const std::size_t j = (lo + mid + 1) % n;
            if (detail::vertexSide(halfplanes_[i], halfplanes_[j], query) < 0) {
                first = mid + 1;
            } else {
                last = mid;
            }
        }
        return static_cast<std::ptrdiff_t>((lo + first) % n);
    }

    // Side of a point relative to stored constraint i: positive means strictly
    // inside, zero on the boundary, negative strictly outside.
    template <PointConcept OtherPoint>
    constexpr std::partial_ordering constraintSide(std::size_t i, const OtherPoint& point) const {
        return orientationSign(halfplanes_[i].source(), halfplanes_[i].target(), point);
    }

    enum class SupStatus { unbounded, above, on, below };

    // Status of sup_{p in region} a·p compared to b, where the query
    // half-plane is {p : a·p <= b}: `below`/`on`/`above` when the supremum is
    // attained and compares so with b, `unbounded` when it is infinite.
    // Querying with the opposite half-plane gives the infimum side: `below`
    // then means inf > b, `on` means inf == b (attained).
    // Precondition: !empty_ and size() >= 1; a degenerate query is UB.
    template <HalfplaneConcept Query>
    constexpr SupStatus supStatus(const Query& query) const {
        const std::size_t n = halfplanes_.size();
        // The supremum in direction of the query's outward normal is attained
        // where the stored boundary directions bracket the query's direction.
        const std::size_t pos = linearUpperBound(query);
        const std::size_t predIdx = (pos == 0 ? n : pos) - 1;
        const auto& pred = halfplanes_[predIdx];
        if (detail::directionEqual(pred, query)) {
            // Attained along pred's whole boundary line, where a·p is
            // constant: compare via any boundary point.
            const auto side = orientationSign(query.source(), query.target(), pred.source());
            if (side > 0) {
                return SupStatus::below;
            }
            return side == 0 ? SupStatus::on : SupStatus::above;
        }
        const std::size_t succIdx = pos == n ? 0 : pos;
        if (succIdx == predIdx) {
            return SupStatus::unbounded;  // single stored half-plane, gap 2*pi
        }
        if (!(detail::directionCross(pred, halfplanes_[succIdx]) > 0)) {
            // The query direction lies strictly inside a gap of at least pi:
            // the region is unbounded toward the query's outward normal.
            return SupStatus::unbounded;
        }
        const int side = detail::vertexSide(pred, halfplanes_[succIdx], query);
        if (side > 0) {
            return SupStatus::below;
        }
        return side == 0 ? SupStatus::on : SupStatus::above;
    }

    // Exact clip of the directed query line against the region.
    // entry/exit are constraint indices (-1: unbounded on that side);
    // onParallelBoundary reports a constraint parallel to the line whose
    // boundary contains it (the line runs along a face).
    // Precondition: !empty_ and size() >= 1; a degenerate query is UB.
    struct ClipResult {
        bool empty = false;
        bool onParallelBoundary = false;
        std::ptrdiff_t entry = -1;
        std::ptrdiff_t exit = -1;
    };

    template <HalfplaneConcept Query>
    constexpr ClipResult clipLine(const Query& query) const {
        ClipResult result;
        const auto reversed = query.opposite();
        // Constraints parallel to the line hold (or fail) along all of it.
        for (const std::ptrdiff_t parallelIdx : {sameDirectionIndex(query), sameDirectionIndex(reversed)}) {
            if (parallelIdx >= 0) {
                const auto side = constraintSide(static_cast<std::size_t>(parallelIdx), query.source());
                if (side < 0) {
                    result.empty = true;
                    return result;
                }
                if (side == 0) {
                    result.onParallelBoundary = true;
                }
            }
        }
        result.exit = exitConstraint(query);
        result.entry = exitConstraint(reversed);
        if (result.entry >= 0 && result.exit >= 0) {
            // The interval is nonempty exactly when t_entry <= t_exit, which
            // (with the entry denominator negative and the exit denominator
            // positive) is the sign of the three-line determinant.
            const auto det = detail::boundaryLinesDeterminant(
                halfplanes_[static_cast<std::size_t>(result.entry)],
                halfplanes_[static_cast<std::size_t>(result.exit)], query);
            if (det > decltype(det){}) {
                result.empty = true;
            }
        }
        return result;
    }

    // Compares the clip parameters of the entry and exit constraints strictly:
    // true when t_entry < t_exit (the clipped set has positive length).
    template <HalfplaneConcept Query>
    constexpr bool clipHasLength(const ClipResult& clip, const Query& query) const {
        if (clip.empty) {
            return false;
        }
        if (clip.entry < 0 || clip.exit < 0) {
            return true;
        }
        const auto det = detail::boundaryLinesDeterminant(
            halfplanes_[static_cast<std::size_t>(clip.entry)],
            halfplanes_[static_cast<std::size_t>(clip.exit)], query);
        return det < decltype(det){};
    }

    // -1: the point is outside the region, 0: on its boundary, +1: in its
    // interior. O(log n): only the constraints binding along the vertical
    // line through the point (plus the vertical ones) need testing.
    template <PointConcept OtherPoint>
    constexpr int pointStatus(const OtherPoint& point) const {
        if (empty_) {
            return -1;
        }
        if (halfplanes_.empty()) {
            return 1;
        }
        using OtherNumber = typename OtherPoint::NumberType;
        using QueryPoint = Point<OtherNumber>;
        const QueryPoint base(point.x(), point.y());
        const Halfplane<QueryPoint> upward(base, QueryPoint(point.x(), point.y() + OtherNumber(1)));
        int result = 1;
        const std::ptrdiff_t candidates[4] = {
            exitConstraint(upward), exitConstraint(upward.opposite()),
            sameDirectionIndex(upward), sameDirectionIndex(upward.opposite())};
        for (const std::ptrdiff_t idx : candidates) {
            if (idx < 0) {
                continue;
            }
            const auto side = constraintSide(static_cast<std::size_t>(idx), point);
            if (side < 0) {
                return -1;
            }
            if (side == 0) {
                result = 0;
            }
        }
        return result;
    }

    // Whether the direction belongs to the region's recession cone: the
    // region contains a translate of every ray with this direction. `strict`
    // additionally requires the interior recession (a·u < 0 for every stored
    // constraint). Precondition: !empty_.
    template <HalfplaneConcept Query>
    constexpr bool recessionContains(const Query& query, bool strict = false) const {
        if (halfplanes_.empty()) {
            return true;
        }
        // a_i·u <= 0 for all i means no stored boundary direction lies
        // strictly inside the open half-circle to the left of u.
        const auto [lo, count] = leftArc(query);
        (void)lo;
        if (count > 0) {
            return false;
        }
        if (!strict) {
            return true;
        }
        return sameDirectionIndex(query) < 0 && sameDirectionIndex(query.opposite()) < 0;
    }

    // The recession cone as up to two closed angular arcs, each reported as a
    // pair of half-plane copies whose boundary directions delimit the arc
    // (from, to). A gap of length g >= pi from direction d_i to d_{i+1}
    // contributes the arc [d_i, -d_{i+1}] of length g - pi. O(n).
    // Precondition: !empty_ and size() >= 2 (a single half-plane's recession
    // half-circle and the whole plane are handled by the callers).
    constexpr std::vector<std::pair<HalfplaneType, HalfplaneType>> recessionArcs() const {
        std::vector<std::pair<HalfplaneType, HalfplaneType>> arcs;
        const std::size_t n = halfplanes_.size();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = nextIndex(i);
            const auto cross = detail::directionCross(halfplanes_[i], halfplanes_[j]);
            using Cross = decltype(cross);
            bool wide = cross < Cross{};
            if (cross == Cross{}) {
                // Antiparallel consecutive directions: gap exactly pi (same
                // direction is impossible in the canonical form for distinct
                // consecutive constraints, but a single pair wraps twice).
                wide = true;
            }
            if (i == j) {
                break;  // n == 1, excluded by precondition but stay safe
            }
            if (wide) {
                arcs.emplace_back(halfplanes_[i], halfplanes_[j].opposite());
            }
        }
        return arcs;
    }
};

/**
 * @brief Translates a region by a point.
 *
 * The coordinate type follows the translation, mirroring the other shapes'
 * free translation operators.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const HalfplaneIntersection<PointType, LabelType>& region,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    return translation + region;
}

/** @copydoc operator+(const HalfplaneIntersection<PointType, LabelType>&, const Point<TranslationNumber, TranslationLabel>&) */
template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation,
                         const HalfplaneIntersection<PointType, LabelType>& region) {
    using ResultPointType = Point<TranslationNumber, typename PointType::LabelType>;
    HalfplaneIntersection<ResultPointType, LabelType> result(region);
    result += translation;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

/**
 * @brief Translates a region by a negated point.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const HalfplaneIntersection<PointType, LabelType>& region,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    return region + (-translation);
}

/**
 * @brief Scales a region around the origin.
 *
 * The coordinate type is promoted to match the scalar, mirroring the other
 * shapes' free scaling operators.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const HalfplaneIntersection<PointType, LabelType>& region, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    HalfplaneIntersection<ResultPointType, LabelType> result(region);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

/** @copydoc operator*(const HalfplaneIntersection<PointType, LabelType>&, const Scalar&) */
template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const HalfplaneIntersection<PointType, LabelType>& region) {
    return region * scalar;
}

/**
 * @brief Divides a region around the origin.
 * @warning Divides coordinates, so the result is generally inexact for
 * integer coordinate types.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const HalfplaneIntersection<PointType, LabelType>& region, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    HalfplaneIntersection<ResultPointType, LabelType> result(region);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const HalfplaneIntersection<PointType, LabelType>& region);

}  // namespace pgl
