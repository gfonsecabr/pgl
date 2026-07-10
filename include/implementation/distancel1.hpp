#pragma once

#include "implementation/distance.hpp"

/**
 * @file distancel1.hpp
 * @brief Manhattan (L1) distance between shapes.
 *
 * Mirrors the shape coverage of distance.hpp's `squaredDistance`, but for the
 * L1 metric `|dx| + |dy|` instead of the Euclidean one. Every overload beyond
 * Point-Point is templated on a `ResultNumber` coordinate type exactly like
 * `squaredDistance`, and shares its caveat: an integer `ResultNumber` divides
 * (and therefore truncates) whenever the true distance is fractional, which
 * happens for any segment/ray/line that isn't axis-aligned. Request a
 * floating-point or `pgl::Rational` `ResultNumber` for an accurate result.
 *
 * L1 and LInf are implemented independently in this file and in
 * distancelinf.hpp respectively -- neither is derived from the other via a
 * coordinate transform.
 */

#include <cmath>
#include <numbers>

namespace pgl {

namespace detail {

/**
 * @brief Minimum L1 distance from @p q to the point `a + t*(b-a)`, minimized
 * over the domain of `t` described by @p boundedLow / @p boundedHigh.
 *
 * `boundedLow` gates the `t >= 0` endpoint (used by Segment and Ray);
 * `boundedHigh` gates the `t <= 1` endpoint (used by Segment only). Passing
 * both false models an infinite Line.
 *
 * `f(t) = |dx(t)| + |dy(t)|` is convex and piecewise-linear in `t`, so its
 * constrained minimum is always one of: the domain's endpoints (if bounded),
 * or the points where one of `dx(t)`, `dy(t)` is individually zero. At such a
 * zero-crossing the other coordinate's gap works out to `|cross| / |D|`,
 * where `cross = dx*py - dy*px` is the usual 2D cross product and `D` is the
 * denominator of that crossing's `t` (`dx` or `dy`). A crossing at `t = N/D`
 * lies in `[0,1]` iff `N*D >= 0` (for the low bound) and `N*D <= D*D` (for
 * the high bound), which keeps the whole search free of division except for
 * the final candidate value.
 */
template <class ResultNumber>
constexpr ResultNumber segmentLikeDistanceL1(const Point<ResultNumber>& a, const Point<ResultNumber>& b,
                                              const Point<ResultNumber>& q, bool boundedLow, bool boundedHigh) {
    const ResultNumber dx = b.x() - a.x();
    const ResultNumber dy = b.y() - a.y();
    const ResultNumber px = q.x() - a.x();
    const ResultNumber py = q.y() - a.y();

    if (dx == ResultNumber{} && dy == ResultNumber{}) {
        return pgl::detail::abs(px) + pgl::detail::abs(py);
    }

    bool haveBest = false;
    ResultNumber best{};
    const auto consider = [&](const ResultNumber& value) {
        if (!haveBest || value < best) {
            best = value;
            haveBest = true;
        }
    };

    if (boundedLow) {
        consider(pgl::detail::abs(px) + pgl::detail::abs(py));
    }
    if (boundedHigh) {
        consider(pgl::detail::abs(px - dx) + pgl::detail::abs(py - dy));
    }

    const ResultNumber cross = dx * py - dy * px;
    const auto considerCrossing = [&](const ResultNumber& n, const ResultNumber& d) {
        if (d == ResultNumber{}) {
            return;
        }
        if (boundedLow && n * d < ResultNumber{}) {
            return;
        }
        if (boundedHigh && n * d > d * d) {
            return;
        }
        consider(pgl::detail::abs(cross) / pgl::detail::abs(d));
    };
    considerCrossing(px, dx);
    considerCrossing(py, dy);

    return best;
}

/**
 * @brief Farthest vertex of `self` from `other` under L1: sup_{v in self} distanceL1(v, other).
 *
 * Mirrors @ref maxVertexSquaredDistance: distance to `other` is convex
 * whenever `other` is convex, which holds for every bounded shape combined
 * here, so the supremum over any polygonal domain is attained at a vertex.
 * That argument only relies on convexity, not on which metric induces the
 * distance, so it applies to L1 exactly as it does to the Euclidean case.
 *
 * `other.distanceL1` is only templated on `ResultNumber` from `Segment` rank
 * upward; `Point::distanceL1` takes no such template (it needs no division
 * to stay exact), so the vertex-to-`other` query below falls back to the
 * untemplated call, casting its result, whenever `other` is itself a `Point`.
 */
template <class ResultNumber, class Self, class OtherShape>
constexpr ResultNumber maxVertexDistanceL1(const Self& self, const OtherShape& other) {
    const auto self_vertices = self.vertices();
    const auto distanceToVertex = [&other](const auto& vertex) -> ResultNumber {
        if constexpr (requires { other.template distanceL1<ResultNumber>(vertex); }) {
            return other.template distanceL1<ResultNumber>(vertex);
        } else {
            return static_cast<ResultNumber>(other.distanceL1(vertex));
        }
    };
    ResultNumber worst = distanceToVertex(self_vertices[0]);
    for (std::size_t index = 1; index < self_vertices.size(); ++index) {
        const ResultNumber current = distanceToVertex(self_vertices[index]);
        if (worst < current) {
            worst = current;
        }
    }
    return worst;
}

/**
 * @brief L1 distance from an external point to a disk's circle boundary.
 *
 * @p a, @p b are the coordinates of the vector from the query point to the
 * disk's center, and @p r is the radius. Unlike the Euclidean case
 * (`|point - center| - radius`), minimizing `|a + r*cos(t)| + |b + r*sin(t)|`
 * over the angle `t` has no closed form: it is piecewise sinusoidal with a
 * variable number of kinks depending on which quadrant of (a,b) applies. A
 * coarse angular scan brackets the global minimum (the function is not
 * generally unimodal enough to trust a single golden-section search from an
 * arbitrary start), and a golden-section search then refines it to double
 * precision. Always returns `double`, matching every other Disk overload.
 */
inline double diskPointDistanceL1(double a, double b, double r) {
    const auto h = [&](double theta) {
        return std::abs(a + r * std::cos(theta)) + std::abs(b + r * std::sin(theta));
    };

    constexpr int coarseSteps = 720;
    double bestTheta = 0.0;
    double bestValue = h(0.0);
    for (int i = 1; i < coarseSteps; ++i) {
        const double theta = 2.0 * std::numbers::pi_v<double> * i / coarseSteps;
        const double value = h(theta);
        if (value < bestValue) {
            bestValue = value;
            bestTheta = theta;
        }
    }

    const double step = 2.0 * std::numbers::pi_v<double> / coarseSteps;
    double lo = bestTheta - step;
    double hi = bestTheta + step;
    constexpr double invPhi = 0.6180339887498949;
    double c1 = hi - invPhi * (hi - lo);
    double c2 = lo + invPhi * (hi - lo);
    double hc1 = h(c1);
    double hc2 = h(c2);
    for (int i = 0; i < 100; ++i) {
        if (hc1 < hc2) {
            hi = c2;
            c2 = c1;
            hc2 = hc1;
            c1 = hi - invPhi * (hi - lo);
            hc1 = h(c1);
        } else {
            lo = c1;
            c1 = c2;
            hc1 = hc2;
            c2 = lo + invPhi * (hi - lo);
            hc2 = h(c2);
        }
    }

    return std::min({bestValue, hc1, hc2});
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Point<Number, Label>::hausdorffDistanceL1(const OtherPoint& other) const {
    return static_cast<ResultNumber>(distanceL1(other));
}

// -----------------------------------------------------------------------------
// Disk

template <class PointType_, class TLabel>
template <PointConcept OtherPoint>
double Disk<PointType_, TLabel>::distanceL1(const OtherPoint& point) const {
    if (contains(point)) {
        return 0.0;
    }
    const double a = center<double>().x() - static_cast<double>(point.x());
    const double b = center<double>().y() - static_cast<double>(point.y());
    return detail::diskPointDistanceL1(a, b, radius<double>());
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Segment<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    return detail::segmentLikeDistanceL1<ResultNumber>(
        Point<ResultNumber>(min()), Point<ResultNumber>(max()), Point<ResultNumber>(point), true, true);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Segment<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_min_to_other = other.template distanceL1<ResultNumber>(min());
    const auto this_max_to_other = other.template distanceL1<ResultNumber>(max());
    const auto other_min_to_this = this->template distanceL1<ResultNumber>(other.min());
    const auto other_max_to_this = this->template distanceL1<ResultNumber>(other.max());

    const auto best_from_this = this_min_to_other < this_max_to_other ? this_min_to_other : this_max_to_other;
    const auto best_from_other = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;

    return best_from_this < best_from_other ? best_from_this : best_from_other;
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    return static_cast<Segment<PointType>>(*this).template distanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedSegment<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedSegment<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template distanceL1<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

// -----------------------------------------------------------------------------
// Line

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Line<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    return detail::segmentLikeDistanceL1<ResultNumber>(
        Point<ResultNumber>(min()), Point<ResultNumber>(max()), Point<ResultNumber>(point), false, false);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Line<PointType, LabelType>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return other.template distanceL1<ResultNumber>(min());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Line<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_distance = this->template distanceL1<ResultNumber>(other.min());
    const auto target_distance = this->template distanceL1<ResultNumber>(other.max());
    return source_distance < target_distance ? source_distance : target_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Line<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    return this->template distanceL1<ResultNumber>(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedLine<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    return this->asLine().template distanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto OrientedLine<PointType, LabelType>::distanceL1(const OtherLine& other) const {
    return this->asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto OrientedLine<PointType, LabelType>::distanceL1(const OtherOrientedLine& other) const {
    return this->asLine().template distanceL1<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedLine<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    return this->asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedLine<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    return this->asLine().template distanceL1<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Ray<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    return detail::segmentLikeDistanceL1<ResultNumber>(
        Point<ResultNumber>(source()), Point<ResultNumber>(target()), Point<ResultNumber>(point), true, false);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Ray<PointType, LabelType>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return other.template distanceL1<ResultNumber>(source());
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Ray<PointType, LabelType>::distanceL1(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return other.template distanceL1<ResultNumber>(source());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Ray<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_to_other = other.template distanceL1<ResultNumber>(source());
    const auto other_min_to_this = this->template distanceL1<ResultNumber>(other.min());
    const auto other_max_to_this = this->template distanceL1<ResultNumber>(other.max());
    const auto best_from_segment = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Ray<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_to_other = other.template distanceL1<ResultNumber>(source());
    const auto other_source_to_this = this->template distanceL1<ResultNumber>(other.source());
    const auto other_target_to_this = this->template distanceL1<ResultNumber>(other.target());
    const auto best_from_segment = other_source_to_this < other_target_to_this ? other_source_to_this : other_target_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Ray<PointType, LabelType>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_source_to_other = other.template distanceL1<ResultNumber>(source());
    const auto other_source_to_this = this->template distanceL1<ResultNumber>(other.source());
    return this_source_to_other < other_source_to_this ? this_source_to_other : other_source_to_this;
}

// -----------------------------------------------------------------------------
// Halfplane

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Halfplane<PointType, LabelType>::distanceL1(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceL1<ResultNumber>(other.asLine());
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    const ResultNumber dx = static_cast<ResultNumber>(axisDistance(min().x(), max().x(), point.x(), point.x()));
    const ResultNumber dy = static_cast<ResultNumber>(axisDistance(min().y(), max().y(), point.y(), point.y()));
    return dx + dy;
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto rectangle_vertices = vertices();
    auto best_distance = other.template distanceL1<ResultNumber>(rectangle_vertices[0]);
    for (std::size_t index = 1; index < rectangle_vertices.size(); ++index) {
        const auto current_distance = other.template distanceL1<ResultNumber>(rectangle_vertices[index]);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherOrientedLine& other) const {
    return this->template distanceL1<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = this->template distanceL1<ResultNumber>(other.min());
    const auto other_max_distance = this->template distanceL1<ResultNumber>(other.max());
    if (other_max_distance < best_distance) {
        best_distance = other_max_distance;
    }

    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = edge.template distanceL1<ResultNumber>(other);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    return this->template distanceL1<ResultNumber>(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = this->template distanceL1<ResultNumber>(other.source());
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = other.template distanceL1<ResultNumber>(edge);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template distanceL1<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::distanceL1(const OtherRectangle& other) const {
    const ResultNumber dx = static_cast<ResultNumber>(axisDistance(min().x(), max().x(), other.min().x(), other.max().x()));
    const ResultNumber dy = static_cast<ResultNumber>(axisDistance(min().y(), max().y(), other.min().y(), other.max().y()));
    return dx + dy;
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Triangle<PointType, LabelType>::edgeMinDistanceL1(const OtherShape& other) const {
    const auto triangle_edges = edges();
    auto best = triangle_edges[0].template distanceL1<ResultNumber>(other);
    for (std::size_t index = 1; index < triangle_edges.size(); ++index) {
        const auto current = triangle_edges[index].template distanceL1<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Triangle<PointType, LabelType>::vertexMinDistanceL1(const OtherShape& other) const {
    const auto triangle_vertices = vertices();
    auto best = other.template distanceL1<ResultNumber>(triangle_vertices[0]);
    for (std::size_t index = 1; index < triangle_vertices.size(); ++index) {
        const auto current = other.template distanceL1<ResultNumber>(triangle_vertices[index]);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template vertexMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template vertexMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template distanceL1<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Triangle<PointType, LabelType>::distanceL1(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Convex
//
// Always uses the O(n) edge scan below (no cyclic support-function fast path):
// see the distanceL1 declarations in convex.hpp for why the Euclidean fast
// path's search functional does not carry over to the L1 gauge.

template <class PointType_, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherPoint& point) const {
    if (contains(point)) {
        return ResultNumber{};
    }
    auto edgeVector = edges();
    ResultNumber best = edgeVector[0].template distanceL1<ResultNumber>(point);
    for (auto& e : edgeVector) {
        const ResultNumber current = e.template distanceL1<ResultNumber>(point);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    auto edgeVector = edges();
    ResultNumber best = edgeVector[0].template distanceL1<ResultNumber>(other);
    for (auto& e : edgeVector) {
        const ResultNumber current = e.template distanceL1<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherOrientedSegment& other) const {
    return this->template distanceL1<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType_, class LabelType>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto minOverEdges = [](const auto& source, const auto& target) {
        const auto edgeVector = source.edges();
        ResultNumber best = target.template distanceL1<ResultNumber>(edgeVector[0]);
        for (const auto& edge : edgeVector) {
            const ResultNumber current = target.template distanceL1<ResultNumber>(edge);
            if (current < best) {
                best = current;
            }
        }
        return best;
    };

    if (size() <= other.size()) {
        return minOverEdges(*this, other);
    }
    return minOverEdges(other, *this);
}

template <class PointType_, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherTriangle& other) const {
    return this->template distanceL1<ResultNumber>(other.asConvex());
}

template <class PointType_, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherRectangle& other) const {
    return this->template distanceL1<ResultNumber>(other.asConvex());
}

template <class PointType_, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    ResultNumber best = other.template distanceL1<ResultNumber>(get(0));
    for (std::ptrdiff_t i = 1; i < static_cast<std::ptrdiff_t>(size()); ++i) {
        const ResultNumber current = other.template distanceL1<ResultNumber>(get(i));
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherOrientedLine& other) const {
    return this->template distanceL1<ResultNumber>(other.asLine());
}

template <class PointType_, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    auto edgeVector = edges();
    ResultNumber best = other.template distanceL1<ResultNumber>(edgeVector[0]);
    for (const auto& e : edgeVector) {
        const ResultNumber current = other.template distanceL1<ResultNumber>(e);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Convex<PointType_, LabelType>::distanceL1(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template distanceL1<ResultNumber>(other.asLine());
}

// -----------------------------------------------------------------------------
// Polygon

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Polygon<PointType_, TLabel>::edgeMinDistanceL1(const OtherShape& other) const {
    const auto boundaryEdges = edges();
    ResultNumber best = boundaryEdges[0].template distanceL1<ResultNumber>(other);
    for (std::size_t index = 1; index < boundaryEdges.size(); ++index) {
        const ResultNumber current = boundaryEdges[index].template distanceL1<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class TLabel>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(point);
}

template <class PointType_, class TLabel>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherPolygon& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Hausdorff distance (added only where a Euclidean squaredHausdorffDistance
// overload already exists; Polygon has none today, so it gets none here).

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Segment<PointType, LabelType>::hausdorffDistanceL1(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Segment<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::maxVertexDistanceL1<ResultNumber>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedSegment<PointType, LabelType>::hausdorffDistanceL1(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template hausdorffDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedSegment<PointType, LabelType>::hausdorffDistanceL1(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template hausdorffDistanceL1<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return static_cast<Segment<PointType>>(*this).template hausdorffDistanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceL1(const OtherRectangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::maxVertexDistanceL1<ResultNumber>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceL1(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceL1(const OtherOrientedSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::maxVertexDistanceL1<ResultNumber>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceL1(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceL1(const OtherOrientedSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceL1(const OtherRectangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceL1(const OtherTriangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::maxVertexDistanceL1<ResultNumber>(*this, point);
}

template <class PointType_, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceL1(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceL1(const OtherOrientedSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceL1(const OtherRectangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceL1(const OtherTriangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceL1(const OtherConvex& other) const {
    const auto worst_from_this = detail::maxVertexDistanceL1<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceL1<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}


template <class PointType_, class TLabel>
template <class ResultNumber, MonotoneChainConcept OtherChain>
constexpr auto Polygon<PointType_, TLabel>::distanceL1(const OtherChain& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// MonotoneChain

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber MonotoneChain<PointType, LabelType, Storage>::edgeMinDistanceL1(const OtherShape& other) const {
    assert(size() >= 2);
    ResultNumber best = this->template boundaryAt<false>(0).template distanceL1<ResultNumber>(other);
    for (std::size_t index = 1; index + 1 < size(); ++index) {
        const ResultNumber current =
            this->template boundaryAt<false>(index).template distanceL1<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, MonotoneChainConcept OtherChain>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::distanceL1(const OtherChain& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Polyline

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Polyline<PointType, LabelType>::edgeMinDistanceL1(const OtherShape& other) const {
    assert(size() >= 2);
    ResultNumber best = this->template boundaryAt<false>(0).template distanceL1<ResultNumber>(other);
    for (std::size_t index = 1; index + 1 < size(); ++index) {
        const ResultNumber current =
            this->template boundaryAt<false>(index).template distanceL1<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polyline<PointType, LabelType>::distanceL1(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Polyline<PointType, LabelType>::distanceL1(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, PolylineConcept OtherPolyline>
constexpr auto Polyline<PointType, LabelType>::distanceL1(const OtherPolyline& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceL1<ResultNumber>(other);
}

}  // namespace pgl
