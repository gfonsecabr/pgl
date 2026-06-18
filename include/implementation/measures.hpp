#pragma once

#include "pgl.hpp"

/**
 * @file measures.hpp
 * @brief Geometric measurements and canonical representative-point helpers.
 *
 * Lengths, areas, slopes, centers, and similar quantitative operations are kept
 * together here so shapes can share the same arithmetic conventions.
 */


namespace pgl {

// -----------------------------------------------------------------------------
// Point

template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
constexpr auto operator*(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right) {
    return left.x() * right.x() + left.y() * right.y();
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
constexpr typename Segment<PointType, LabelType>::NumberType Segment<PointType, LabelType>::area() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr typename Segment<PointType, LabelType>::NumberType Segment<PointType, LabelType>::twiceArea() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr auto Segment<PointType, LabelType>::squaredLength() const {
    return min().squaredDistance(max());
}

template <class PointType, class LabelType>
template <class ApproximateNumber>
ApproximateNumber Segment<PointType, LabelType>::length() const {
    return min().template distance<ApproximateNumber>(max());
}

template <class PointType, class LabelType>
constexpr auto Segment<PointType, LabelType>::lengthL1() const {
    return min().distanceL1(max());
}

template <class PointType, class LabelType>
constexpr auto Segment<PointType, LabelType>::lengthLInf() const {
    return min().distanceLInf(max());
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber Segment<PointType, LabelType>::slope() const {
    const auto dy = static_cast<ResultNumber>(max().y()) - static_cast<ResultNumber>(min().y());
    const auto dx = static_cast<ResultNumber>(max().x()) - static_cast<ResultNumber>(min().x());
    return detail::abs(dy / dx);
}

template <class PointType, class LabelType>
constexpr Segment<PointType, LabelType> Segment<PointType, LabelType>::diameter() const {
    return *this;
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Segment<PointType, LabelType>::midpoint() const {
    return Point<ResultNumber>(
        (static_cast<ResultNumber>(min().x()) + static_cast<ResultNumber>(max().x())) / static_cast<ResultNumber>(2),
        (static_cast<ResultNumber>(min().y()) + static_cast<ResultNumber>(max().y())) / static_cast<ResultNumber>(2)
    );
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Segment<PointType, LabelType>::pointInside() const {
    return midpoint<ResultNumber>();
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
constexpr typename OrientedSegment<PointType, LabelType>::NumberType OrientedSegment<PointType, LabelType>::area() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr typename OrientedSegment<PointType, LabelType>::NumberType OrientedSegment<PointType, LabelType>::twiceArea() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr auto OrientedSegment<PointType, LabelType>::squaredLength() const {
    return source().squaredDistance(target());
}

template <class PointType, class LabelType>
template <class ApproximateNumber>
ApproximateNumber OrientedSegment<PointType, LabelType>::length() const {
    return source().template distance<ApproximateNumber>(target());
}

template <class PointType, class LabelType>
constexpr auto OrientedSegment<PointType, LabelType>::lengthL1() const {
    return source().distanceL1(target());
}

template <class PointType, class LabelType>
constexpr auto OrientedSegment<PointType, LabelType>::lengthLInf() const {
    return source().distanceLInf(target());
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber OrientedSegment<PointType, LabelType>::slope() const {
    const auto dy = static_cast<ResultNumber>(target().y()) - static_cast<ResultNumber>(source().y());
    const auto dx = static_cast<ResultNumber>(target().x()) - static_cast<ResultNumber>(source().x());
    return dy / dx;
}

template <class PointType, class LabelType>
constexpr Segment<PointType> OrientedSegment<PointType, LabelType>::diameter() const {
    return static_cast<Segment<PointType>>(*this);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> OrientedSegment<PointType, LabelType>::midpoint() const {
    return Point<ResultNumber>(
        (static_cast<ResultNumber>(source().x()) + static_cast<ResultNumber>(target().x())) / static_cast<ResultNumber>(2),
        (static_cast<ResultNumber>(source().y()) + static_cast<ResultNumber>(target().y())) / static_cast<ResultNumber>(2));
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> OrientedSegment<PointType, LabelType>::pointInside() const {
    return midpoint<ResultNumber>();
}

// -----------------------------------------------------------------------------
// Line

template <class PointType, class LabelType>
constexpr typename Line<PointType, LabelType>::NumberType Line<PointType, LabelType>::area() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr typename Line<PointType, LabelType>::NumberType Line<PointType, LabelType>::twiceArea() const {
    return NumberType{};
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber Line<PointType, LabelType>::slope() const {
    const auto dy = static_cast<ResultNumber>(max().y()) - static_cast<ResultNumber>(min().y());
    const auto dx = static_cast<ResultNumber>(max().x()) - static_cast<ResultNumber>(min().x());
    return detail::abs(dy / dx);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Line<PointType, LabelType>::pointInside() const {
    return Point<ResultNumber>(min());
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType, class LabelType>
constexpr typename OrientedLine<PointType, LabelType>::NumberType OrientedLine<PointType, LabelType>::area() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr typename OrientedLine<PointType, LabelType>::NumberType OrientedLine<PointType, LabelType>::twiceArea() const {
    return NumberType{};
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber OrientedLine<PointType, LabelType>::slope() const {
    const auto dy = static_cast<ResultNumber>(target().y()) - static_cast<ResultNumber>(source().y());
    const auto dx = static_cast<ResultNumber>(target().x()) - static_cast<ResultNumber>(source().x());
    return dy / dx;
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> OrientedLine<PointType, LabelType>::pointInside() const {
    return Point<ResultNumber>(source());
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType, class LabelType>
constexpr typename Ray<PointType, LabelType>::NumberType Ray<PointType, LabelType>::area() const {
    return NumberType{};
}

template <class PointType, class LabelType>
constexpr typename Ray<PointType, LabelType>::NumberType Ray<PointType, LabelType>::twiceArea() const {
    return NumberType{};
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber Ray<PointType, LabelType>::slope() const {
    const auto dy = static_cast<ResultNumber>(target().y()) - static_cast<ResultNumber>(source().y());
    const auto dx = static_cast<ResultNumber>(target().x()) - static_cast<ResultNumber>(source().x());
    return dy / dx;
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Ray<PointType, LabelType>::pointInside() const {
    return Point<ResultNumber>(target());
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType, class LabelType>
constexpr auto Rectangle<PointType, LabelType>::area() const {
    return width() * height();
}

template <class PointType, class LabelType>
constexpr auto Rectangle<PointType, LabelType>::twiceArea() const {
    const auto rectangle_area = area();
    return rectangle_area + rectangle_area;
}

template <class PointType, class LabelType>
constexpr Segment<PointType> Rectangle<PointType, LabelType>::diameter() const {
    return Segment<PointType>(min(), max());
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Rectangle<PointType, LabelType>::midpoint() const {
    return Point<ResultNumber>(
        (static_cast<ResultNumber>(min().x()) + static_cast<ResultNumber>(max().x())) / static_cast<ResultNumber>(2),
        (static_cast<ResultNumber>(min().y()) + static_cast<ResultNumber>(max().y())) / static_cast<ResultNumber>(2));
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Rectangle<PointType, LabelType>::centroid() const {
    return midpoint<ResultNumber>();
}

template <class PointType, class LabelType>
constexpr Disk<PointType, NoLabel> Rectangle<PointType, LabelType>::circumcircle() const {
    return Disk<PointType, NoLabel>(min(), bottomRight(), max()); //Choosen arbitrarly 
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Rectangle<PointType, LabelType>::center() const {
    return midpoint<ResultNumber>();
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Rectangle<PointType, LabelType>::pointInside() const {
    return midpoint<ResultNumber>();
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType, class LabelType>
constexpr typename Triangle<PointType, LabelType>::NumberType Triangle<PointType, LabelType>::twiceArea() const {
    return static_cast<NumberType>(detail::abs(orientationDeterminant(a(), b(), c())));
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber Triangle<PointType, LabelType>::area() const {
    const auto area2 = twiceArea();
    return static_cast<ResultNumber>(area2) / static_cast<ResultNumber>(2);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Triangle<PointType, LabelType>::centroid() const {
    const ResultNumber three = static_cast<ResultNumber>(3);
    return Point<ResultNumber>(
        (static_cast<ResultNumber>(a().x()) +
         static_cast<ResultNumber>(b().x()) +
         static_cast<ResultNumber>(c().x())) / three,
        (static_cast<ResultNumber>(a().y()) +
         static_cast<ResultNumber>(b().y()) +
         static_cast<ResultNumber>(c().y())) / three);
}

template <class PointType, class LabelType>
constexpr Disk<PointType, NoLabel> Triangle<PointType, LabelType>::circumcircle() const {
    return Disk<PointType, NoLabel>(a(), b(), c());
}

template <class PointType, class LabelType>
constexpr Segment<PointType> Triangle<PointType, LabelType>::diameter() const {
    const auto ab = a().squaredDistance(b());
    const auto bc = b().squaredDistance(c());
    const auto ca = c().squaredDistance(a());

    if (ab < bc) {
        if (ca < bc) {
            return Segment<PointType>(b(), c());
        }
        return Segment<PointType>(c(), a());
    }

    if (ca < ab) {
        return Segment<PointType>(a(), b());
    }
    return Segment<PointType>(c(), a());
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Triangle<PointType, LabelType>::pointInside() const {
    Point<ResultNumber> p = points_[0] + points_[1];
    p = (p/2 + points_[2])/2;
    return p;
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isRectangle() const {
    if (isDegenerate()) {
        return false;
    }

    const auto ab = b() - a();
    const auto ac = c() - a();
    const auto ba = a() - b();
    const auto bc = c() - b();
    const auto ca = a() - c();
    const auto cb = b() - c();

    return (ab * ac) == 0 || (ba * bc) == 0 || (ca * cb) == 0;
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isObtuse() const {
    if (isDegenerate()) {
        return false;
    }

    const auto ab = b() - a();
    const auto ac = c() - a();
    const auto ba = a() - b();
    const auto bc = c() - b();
    const auto ca = a() - c();
    const auto cb = b() - c();

    return (ab * ac) < 0 || (ba * bc) < 0 || (ca * cb) < 0;
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isIsosceles() const {
    const auto ab = a().squaredDistance(b());
    const auto bc = b().squaredDistance(c());
    const auto ca = c().squaredDistance(a());
    return ab == bc || bc == ca || ca == ab;
}

// -----------------------------------------------------------------------------
// Halfplane

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr ResultNumber Halfplane<PointType, LabelType>::slope() const {
    const auto dy = static_cast<ResultNumber>(target().y()) - static_cast<ResultNumber>(source().y());
    const auto dx = static_cast<ResultNumber>(target().x()) - static_cast<ResultNumber>(source().x());
    return dy / dx;
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Halfplane<PointType, LabelType>::pointInside() const {
    const ResultNumber x = static_cast<ResultNumber>(source().x());
    const ResultNumber y = static_cast<ResultNumber>(source().y());
    const ResultNumber one = static_cast<ResultNumber>(1);

    if (source().x() < target().x()) {
        return {x, y + one};
    }
    if (source().x() > target().x()) {
        return {x, y - one};
    }
    if (source().y() < target().y()) {
        return {x - one, y};
    }
    return {x + one, y - one};
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
constexpr auto Convex<PointType, LabelType>::twiceArea() const {
    if (points_.size() < 3) {
        return NumberType(0);
    }
    NumberType sum = 0;
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const auto& p1 = points_[i];
        const auto& p2 = points_[(i + 1) % points_.size()];
        sum += p1.x() * p2.y() - p2.x() * p1.y();
    }
    return pgl::detail::abs(sum);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr auto Convex<PointType, LabelType>::area() const {
    ResultNumber result = static_cast<ResultNumber>(twiceArea());
    return result / ResultNumber(2);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Convex<PointType, LabelType>::centroid() const {
    if (points_.empty()) {
        return Point<ResultNumber>();
    }
    if (points_.size() == 1) {
        return Point<ResultNumber>(points_[0]);
    }
    auto area_twice = twiceArea();
    if (points_.size() == 2 || area_twice == NumberType(0)) {
        const  Point<ResultNumber> p1 = static_cast<Point<ResultNumber>>(points_[0]);
        const  Point<ResultNumber> p2 = static_cast<Point<ResultNumber>>(points_[maxIndex()]);
        return (p1 + p2) / ResultNumber(2) + static_cast<Point<ResultNumber>>(translation_);
    }
    ResultNumber cx = 0;
    ResultNumber cy = 0;
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const auto& p1 = points_[i];
        const auto& p2 = points_[(i + 1) % points_.size()];
        const auto cross = p1.x() * p2.y() - p2.x() * p1.y();
        cx += (p1.x() + p2.x()) * cross;
        cy += (p1.y() + p2.y()) * cross;
    }
    return Point<ResultNumber>(cx / (ResultNumber(3) * area_twice), cy / (ResultNumber(3) * area_twice))
           + static_cast<Point<ResultNumber>>(translation_);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Convex<PointType, LabelType>::verticesCentroid() const {
    if (points_.empty()) {
        return Point<ResultNumber>();
    }
    ResultNumber cx = 0;
    ResultNumber cy = 0;
    for (const auto& vertex : points_) {
        cx += static_cast<ResultNumber>(vertex.x());
        cy += static_cast<ResultNumber>(vertex.y());
    }
    return Point<ResultNumber>(cx / static_cast<ResultNumber>(points_.size()), cy / static_cast<ResultNumber>(points_.size()))
           + static_cast<Point<ResultNumber>>(translation_);
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr Point<ResultNumber> Convex<PointType, LabelType>::pointInside() const {
    if (points_.empty()) {
        return Point<ResultNumber>();
    }
    if (points_.size() == 1) {
        return Point<ResultNumber>(points_[0]) + static_cast<Point<ResultNumber>>(translation_);
    }
    if (points_.size() == 2) {
        const Point<ResultNumber> p1(points_[0]);
        const Point<ResultNumber> p2(points_[1]);
        return (p1 + p2) / ResultNumber(2) + static_cast<Point<ResultNumber>>(translation_);
    }
    Triangle<PointType> triangle(points_[0], points_[1], points_[2]);
    // Compute the inner triangle's interior point in ResultNumber too; using
    // the (now integer) default would truncate and can fall outside the polygon.
    return Point<ResultNumber>(triangle.template pointInside<ResultNumber>())
           + static_cast<Point<ResultNumber>>(translation_);
}

template <class PointType, class LabelType>
constexpr std::vector<std::pair<std::size_t, std::size_t>>
Convex<PointType, LabelType>::antipodalPairs() const {
    const std::size_t n = size();
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    if (n < 2) {
        return pairs;
    }
    if (n == 2) {
        pairs.emplace_back(0, 1);
        return pairs;
    }

    const auto next = [n](std::size_t v) { return v + 1 == n ? std::size_t{0} : v + 1; };

    // Cross product of edge(i) = p[i+1]-p[i] with edge(j), in exact promoted
    // arithmetic. As the caliper direction rotates CCW, the i-side caliper
    // reaches the next edge normal before the j-side one exactly when this is
    // negative; a zero means the two edges are parallel (a shared supporting
    // direction touching both edges at once).
    using Coord = detail::promoted_number_t<NumberType>;
    const auto edgeCross = [this, n](std::size_t i, std::size_t j) {
        const auto pi = (*this)[i];
        const auto pi1 = (*this)[i + 1 == n ? std::size_t{0} : i + 1];
        const auto pj = (*this)[j];
        const auto pj1 = (*this)[j + 1 == n ? std::size_t{0} : j + 1];
        const Coord eix = static_cast<Coord>(pi1.x()) - static_cast<Coord>(pi.x());
        const Coord eiy = static_cast<Coord>(pi1.y()) - static_cast<Coord>(pi.y());
        const Coord ejx = static_cast<Coord>(pj1.x()) - static_cast<Coord>(pj.x());
        const Coord ejy = static_cast<Coord>(pj1.y()) - static_cast<Coord>(pj.y());
        return eix * ejy - eiy * ejx;
    };

    // Start the calipers at the lexicographically smallest vertex (index 0, the
    // min-x support) and the max-x support; these admit vertical parallel
    // supporting lines, so they form an antipodal pair.
    const std::size_t i0 = maxIndex();
    const std::size_t j0 = 0;
    std::size_t i = i0;
    std::size_t j = j0;

    const auto isStart = [i0, j0](std::size_t a, std::size_t b) {
        return (a == i0 && b == j0) || (a == j0 && b == i0);
    };
    const auto emit = [&pairs, &isStart](std::size_t a, std::size_t b) {
        // The starting pair is emitted once up front; skip any later reappearance
        // (it would otherwise close the sweep with a duplicate).
        if (isStart(a, b)) {
            return;
        }
        pairs.emplace_back(std::min(a, b), std::max(a, b));
    };

    pairs.emplace_back(std::min(i0, j0), std::max(i0, j0));

    // Sweep the caliper direction through half a turn. Advancing whichever side
    // reaches its next edge normal first walks every antipodal pair once.
    while (true) {
        const auto c = edgeCross(i, j);
        if (c < 0) {
            i = next(i);
        } else if (c > 0) {
            j = next(j);
        } else {
            // Parallel edges: vertices i, i+1 are co-extreme with j, j+1, so the
            // three remaining cross pairs of this edge/edge contact are antipodal.
            const std::size_t i2 = next(i);
            const std::size_t j2 = next(j);
            emit(i, j2);
            emit(i2, j);
            i = i2;
            j = j2;
        }
        if (i == j0 && j == i0) {
            break;  // returned to the swapped start: sweep complete.
        }
        emit(i, j);
    }

    return pairs;
}

template <class PointType, class LabelType>
constexpr Segment<PointType> Convex<PointType, LabelType>::diameter() const {
    const std::size_t n = size();
    if (n == 0) {
        return Segment<PointType>();
    }
    if (n == 1) {
        return Segment<PointType>((*this)[0], (*this)[0]);
    }

    // The farthest vertex pair is antipodal, so the diameter is the longest of
    // the O(n) antipodal segments. Compare squared lengths for exactness.
    const auto pairs = antipodalPairs();
    Segment<PointType> best((*this)[pairs.front().first], (*this)[pairs.front().second]);
    auto bestSquared = (*this)[pairs.front().first].squaredDistance((*this)[pairs.front().second]);
    for (const auto& [i, j] : pairs) {
        const auto pi = (*this)[i];
        const auto pj = (*this)[j];
        const auto squared = pi.squaredDistance(pj);
        if (bestSquared < squared) {
            bestSquared = squared;
            best = Segment<PointType>(pi, pj);
        }
    }
    return best;
}

}  // namespace pgl
