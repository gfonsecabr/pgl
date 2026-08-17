#pragma once

#include "implementation/io.hpp"

/**
 * @file transformations.hpp
 * @brief Arithmetic transformations and explicit conversions between primitives.
 *
 * Translation, scaling, negation, axis swapping, and cross-primitive conversion
 */


namespace pgl {

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
constexpr Point<Number, Label> Point<Number, Label>::operator-() const {
    return Point<Number, Label>(-x(), -y());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr Point<Number, Label>& Point<Number, Label>::operator+=(const OtherPoint& other) {
    coords_[0] += detail::asNumber<Number>(other[0]);
    coords_[1] += detail::asNumber<Number>(other[1]);
    return *this;
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr Point<Number, Label>& Point<Number, Label>::operator-=(const OtherPoint& other) {
    coords_[0] -= detail::asNumber<Number>(other[0]);
    coords_[1] -= detail::asNumber<Number>(other[1]);
    return *this;
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label>& Point<Number, Label>::operator*=(const OtherNumber scalar) {
    coords_[0] *= detail::asNumber<Number>(scalar);
    coords_[1] *= detail::asNumber<Number>(scalar);
    return *this;
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label>& Point<Number, Label>::operator/=(const OtherNumber scalar) {
    coords_[0] /= detail::asNumber<Number>(scalar);
    coords_[1] /= detail::asNumber<Number>(scalar);
    return *this;
}

template <class Number, class Label>
constexpr Point<Number, Label> Point<Number, Label>::swapped() const {
    return Point<Number, Label>(y(), x());
}

template <class Number, class Label>
constexpr Point<Number, Label> Point<Number, Label>::rotated90(int k) const {
    switch (((k % 4) + 4) % 4) {
        case 1:  return Point(-y(),  x());
        case 2:  return Point(-x(), -y());
        case 3:  return Point( y(), -x());
        default: return Point(x(), y());
    }
}

template <class Number, class Label>
constexpr void Point<Number, Label>::rotate90(int k) {
    switch (((k % 4) + 4) % 4) {
        case 1: { auto tmp = x(); x() = -y(); y() =  tmp; break; }
        case 2: { x() = -x(); y() = -y(); break; }
        case 3: { auto tmp = x(); x() =  y(); y() = -tmp; break; }
    }
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledUpX(const OtherNumber scalar) const {
    return Point<Number, Label>(x() * scalar, y());
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleUpX(const OtherNumber scalar) {
    coords_[0] *= detail::asNumber<Number>(scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledUpY(const OtherNumber scalar) const {
    return Point<Number, Label>(x(), y() * scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleUpY(const OtherNumber scalar) {
    coords_[1] *= detail::asNumber<Number>(scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledDownX(const OtherNumber scalar) const {
    return Point<Number, Label>(x() / scalar, y());
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleDownX(const OtherNumber scalar) {
    coords_[0] /= detail::asNumber<Number>(scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledDownY(const OtherNumber scalar) const {
    return Point<Number, Label>(x(), y() / scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleDownY(const OtherNumber scalar) {
    coords_[1] /= detail::asNumber<Number>(scalar);
}

// operator+ is the Minkowski sum; see implementation/minkowski.hpp.

template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
constexpr auto operator-(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right) {
    using ResultNumber = std::common_type_t<LeftNumber, RightNumber>;
    return Point<ResultNumber, LeftLabel>(
        detail::asNumber<ResultNumber>(left.x()) - detail::asNumber<ResultNumber>(right.x()),
        detail::asNumber<ResultNumber>(left.y()) - detail::asNumber<ResultNumber>(right.y()));
}

template <class Number, class Label, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Point<Number, Label>& point, const Scalar& scalar) {
    using ResultNumber = std::common_type_t<Number, Scalar>;
    return Point<ResultNumber, Label>(
        detail::asNumber<ResultNumber>(point.x()) * detail::asNumber<ResultNumber>(scalar),
        detail::asNumber<ResultNumber>(point.y()) * detail::asNumber<ResultNumber>(scalar));
}

template <class Scalar, class Number, class Label>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Point<Number, Label>& point) {
    return point * scalar;
}

template <class Number, class Label, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Point<Number, Label>& point, const Scalar& scalar) {
    using ResultNumber = std::common_type_t<Number, Scalar>;
    return Point<ResultNumber, Label>(
        detail::asNumber<ResultNumber>(point.x()) / detail::asNumber<ResultNumber>(scalar),
        detail::asNumber<ResultNumber>(point.y()) / detail::asNumber<ResultNumber>(scalar));
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Segment<PointType, LabelType>& Segment<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Segment<PointType, LabelType>& Segment<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Segment<PointType, LabelType>& Segment<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Segment<PointType, LabelType>& Segment<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Segment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = segment.min() - translation;
    const auto second = segment.max() - translation;
    return Segment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Segment<PointType, LabelType>& segment, const Scalar& scalar) {
    const auto first = segment.min() * scalar;
    const auto second = segment.max() * scalar;
    return Segment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Segment<PointType, LabelType>& segment) {
    return segment * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Segment<PointType, LabelType>& segment, const Scalar& scalar) {
    const auto first = segment.min() / scalar;
    const auto second = segment.max() / scalar;
    return Segment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType>
constexpr Segment<PointType, LabelType> Segment<PointType, LabelType>::rotated90(int k) const {
    return Segment(min().rotated90(k), max().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Segment<PointType, LabelType>::rotate90(int k) {
    points_[0].rotate90(k);
    points_[1].rotate90(k);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Segment<PointType, LabelType> Segment<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    return Segment(min().scaledUpX(scalar), max().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Segment<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    points_[0].scaleUpX(scalar);
    points_[1].scaleUpX(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Segment<PointType, LabelType> Segment<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    return Segment(min().scaledUpY(scalar), max().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Segment<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    points_[0].scaleUpY(scalar);
    points_[1].scaleUpY(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Segment<PointType, LabelType> Segment<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    return Segment(min().scaledDownX(scalar), max().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Segment<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    points_[0].scaleDownX(scalar);
    points_[1].scaleDownX(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Segment<PointType, LabelType> Segment<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    return Segment(min().scaledDownY(scalar), max().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Segment<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    points_[0].scaleDownY(scalar);
    points_[1].scaleDownY(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr OrientedSegment<PointType, LabelType>& OrientedSegment<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr OrientedSegment<PointType, LabelType>& OrientedSegment<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr OrientedSegment<PointType, LabelType>& OrientedSegment<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr OrientedSegment<PointType, LabelType>& OrientedSegment<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedSegment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = segment.source() - translation;
    const auto second = segment.target() - translation;
    return OrientedSegment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const OrientedSegment<PointType, LabelType>& segment, const Scalar& scalar) {
    const auto first = segment.source() * scalar;
    const auto second = segment.target() * scalar;
    return OrientedSegment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedSegment<PointType, LabelType>& segment) {
    return segment * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const OrientedSegment<PointType, LabelType>& segment, const Scalar& scalar) {
    const auto first = segment.source() / scalar;
    const auto second = segment.target() / scalar;
    return OrientedSegment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType>
constexpr OrientedSegment<PointType, LabelType> OrientedSegment<PointType, LabelType>::rotated90(int k) const {
    return OrientedSegment(source().rotated90(k), target().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void OrientedSegment<PointType, LabelType>::rotate90(int k) {
    points_[0].rotate90(k);
    points_[1].rotate90(k);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedSegment<PointType, LabelType> OrientedSegment<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    return OrientedSegment(source().scaledUpX(scalar), target().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedSegment<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    points_[0].scaleUpX(scalar);
    points_[1].scaleUpX(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedSegment<PointType, LabelType> OrientedSegment<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    return OrientedSegment(source().scaledUpY(scalar), target().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedSegment<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    points_[0].scaleUpY(scalar);
    points_[1].scaleUpY(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedSegment<PointType, LabelType> OrientedSegment<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    return OrientedSegment(source().scaledDownX(scalar), target().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedSegment<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    points_[0].scaleDownX(scalar);
    points_[1].scaleDownX(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedSegment<PointType, LabelType> OrientedSegment<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    return OrientedSegment(source().scaledDownY(scalar), target().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedSegment<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    points_[0].scaleDownY(scalar);
    points_[1].scaleDownY(scalar);
}

// -----------------------------------------------------------------------------
// Line

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Line<PointType, LabelType>& Line<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Line<PointType, LabelType>& Line<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Line<PointType, LabelType>& Line<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Line<PointType, LabelType>& Line<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Line<PointType, LabelType>& line, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = line.min() - translation;
    const auto second = line.max() - translation;
    return Line<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Line<PointType, LabelType>& line, const Scalar& scalar) {
    const auto first = line.min() * scalar;
    const auto second = line.max() * scalar;
    return Line<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Line<PointType, LabelType>& line) {
    return line * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Line<PointType, LabelType>& line, const Scalar& scalar) {
    const auto first = line.min() / scalar;
    const auto second = line.max() / scalar;
    return Line<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType>
constexpr Segment<PointType, LabelType>::operator Line<PointType>() const {
    return Line<PointType>(min(), max());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType, LabelType>::operator Line<PointType>() const {
    return Line<PointType>(source(), target());
}

template <class PointType, class LabelType>
constexpr Line<PointType, LabelType> Line<PointType, LabelType>::rotated90(int k) const {
    return Line(min().rotated90(k), max().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Line<PointType, LabelType>::rotate90(int k) {
    points_[0].rotate90(k);
    points_[1].rotate90(k);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Line<PointType, LabelType> Line<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    return Line(min().scaledUpX(scalar), max().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Line<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    points_[0].scaleUpX(scalar);
    points_[1].scaleUpX(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Line<PointType, LabelType> Line<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    return Line(min().scaledUpY(scalar), max().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Line<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    points_[0].scaleUpY(scalar);
    points_[1].scaleUpY(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Line<PointType, LabelType> Line<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    return Line(min().scaledDownX(scalar), max().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Line<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    points_[0].scaleDownX(scalar);
    points_[1].scaleDownX(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Line<PointType, LabelType> Line<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    return Line(min().scaledDownY(scalar), max().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Line<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    points_[0].scaleDownY(scalar);
    points_[1].scaleDownY(scalar);
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType, class LabelType>
constexpr OrientedLine<PointType, LabelType>::operator Line<PointType>() const {
    return Line<PointType>(source(), target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr OrientedLine<PointType, LabelType>& OrientedLine<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr OrientedLine<PointType, LabelType>& OrientedLine<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr OrientedLine<PointType, LabelType>& OrientedLine<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr OrientedLine<PointType, LabelType>& OrientedLine<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedLine<PointType, LabelType>& line, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = line.source() - translation;
    const auto second = line.target() - translation;
    return OrientedLine<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const OrientedLine<PointType, LabelType>& line, const Scalar& scalar) {
    const auto first = line.source() * scalar;
    const auto second = line.target() * scalar;
    return OrientedLine<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedLine<PointType, LabelType>& line) {
    return line * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const OrientedLine<PointType, LabelType>& line, const Scalar& scalar) {
    const auto first = line.source() / scalar;
    const auto second = line.target() / scalar;
    return OrientedLine<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType>
constexpr OrientedSegment<PointType, LabelType>::operator OrientedLine<PointType>() const {
    return OrientedLine<PointType>(source(), target());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType, LabelType>::operator OrientedLine<PointType>() const {
    return OrientedLine<PointType>(source(), target());
}

template <class PointType, class LabelType>
constexpr OrientedLine<PointType, LabelType> OrientedLine<PointType, LabelType>::rotated90(int k) const {
    return OrientedLine(source().rotated90(k), target().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void OrientedLine<PointType, LabelType>::rotate90(int k) {
    points_[0].rotate90(k);
    points_[1].rotate90(k);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedLine<PointType, LabelType> OrientedLine<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    return OrientedLine(source().scaledUpX(scalar), target().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedLine<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    points_[0].scaleUpX(scalar);
    points_[1].scaleUpX(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedLine<PointType, LabelType> OrientedLine<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    return OrientedLine(source().scaledUpY(scalar), target().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedLine<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    points_[0].scaleUpY(scalar);
    points_[1].scaleUpY(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedLine<PointType, LabelType> OrientedLine<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    return OrientedLine(source().scaledDownX(scalar), target().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedLine<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    points_[0].scaleDownX(scalar);
    points_[1].scaleDownX(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr OrientedLine<PointType, LabelType> OrientedLine<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    return OrientedLine(source().scaledDownY(scalar), target().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void OrientedLine<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    points_[0].scaleDownY(scalar);
    points_[1].scaleDownY(scalar);
}

namespace detail {

/**
 * @brief How many bits of every numerator and denominator keep
 * @ref pgl::OrientedLine::integralLine inside a ::pgl::int128.
 *
 * With every part below `2^b`, the steps of that computation are bounded by:
 * a coordinate difference by `2^(2b+1)`, the cleared direction by `2^(4b+1)`,
 * the offset by `2^(6b+2)`, a Bezout coefficient by `2^(4b+1)`, hence the
 * unreduced Bezout point by `2^(10b+3)` — and, once that point is reduced into
 * the fundamental domain, the widest term left is the recentring dot product at
 * `2^(10b+5)`. Twelve bits puts that at `2^125`, one bit under what a signed
 * 128-bit integer holds; thirteen would not fit.
 */
inline constexpr int integralLinePartBits = 12;

/**
 * @brief Core of @ref pgl::OrientedLine::integralLine over an exact integer type.
 *
 * Takes the numerator and denominator of each coordinate, in the order
 * `(source.x, source.y, target.x, target.y)` with each numerator before its
 * (positive) denominator, and returns the defining points of the integral line
 * as `(source.x, source.y, target.x, target.y)`, or nothing when the line has no
 * two lattice points. Every operation is exact, so @p Working has to be wide
 * enough to hold the bounds above — which is what @ref integralLinePartBits
 * decides for a fixed-width type.
 */
template <class Working>
std::optional<std::array<Working, 4>> integralLinePoints(const std::array<Working, 8>& parts) {
    const auto& [sourceXNum, sourceXDen, sourceYNum, sourceYDen,
                 targetXNum, targetXDen, targetYNum, targetYDen] = parts;

    // The direction, cleared of denominators and reduced to a primitive integer
    // vector. Every factor taken out is positive — denominators are, and so is
    // the gcd — so the direction, hence the orientation, is preserved exactly.
    //
    // With both differences in lowest terms the gcd below would factor as
    // gcd(dxNum, dyNum) * gcd(dxDen, dyDen), which builds the same primitive
    // components out of two gcds of narrow parts rather than one of the
    // double-width products. Measured, that is a 1.5x loss, not a win: reducing
    // the two differences first costs two more gcds, and a gcd here is priced by
    // how many divisions it runs, not by how wide they are.
    const Working deltaXNum = targetXNum * sourceXDen - sourceXNum * targetXDen;
    const Working deltaXDen = targetXDen * sourceXDen;
    const Working deltaYNum = targetYNum * sourceYDen - sourceYNum * targetYDen;
    const Working deltaYDen = targetYDen * sourceYDen;
    Working directionX = deltaXNum * deltaYDen;
    Working directionY = deltaYNum * deltaXDen;
    const Working scale = gcd(abs(directionX), abs(directionY));
    if (scale == Working(0)) {
        return {};   // degenerate, so undefined: no direction to build a line on
    }
    directionX = directionX / scale;
    directionY = directionY / scale;

    // With (directionX, directionY) primitive, the line is exactly the solution
    // set of directionY*x - directionX*y == offset, and x |-> directionY*x -
    // directionX*y maps the integer grid onto all of Z. So the line meets the
    // grid iff this offset is a whole number, and when it is not, no lattice
    // point exists at all — scaling the equation cannot change that, since it
    // scales gcd(a, b) and c alike.
    const Working offsetNum = directionY * sourceXNum * sourceYDen
                            - directionX * sourceYNum * sourceXDen;
    const Working offsetDen = sourceXDen * sourceYDen;   // positive
    if (offsetNum % offsetDen != Working(0)) {
        return {};
    }
    const Working offset = offsetNum / offsetDen;

    // Bezout gives one lattice point. Its gcd is 1 — the direction is primitive,
    // which is exactly what makes the equation solvable — so the coefficients
    // scaled by the offset already solve it.
    const auto bezout = extendedGcd(directionY, -directionX);
    Working x = bezout[1] * offset;
    Working y = bezout[2] * offset;

    // That point is as far out as the offset is large, while the answer is not,
    // so bring it back into one period of the direction before anything else
    // multiplies it. Reducing along the wider component keeps the exact division
    // that rebuilds the other one small, and is what bounds every term below.
    if (abs(directionX) >= abs(directionY)) {   // directionX != 0 here
        x = x % directionX;
        y = (directionY * x - offset) / directionX;
    } else {
        y = y % directionY;
        x = (directionX * y + offset) / directionY;
    }

    // Slide along the direction to the lattice point closest to the origin: it
    // is the smallest one available, which is what gives a fixed-width result
    // type its best chance of holding the answer.
    const Working steps = nearestQuotient(
        -(x * directionX + y * directionY),
        directionX * directionX + directionY * directionY);
    x = x + steps * directionX;
    y = y + steps * directionY;

    // Two lattice points tie for closest whenever the origin projects exactly
    // between them, and rounding halves up picked the one further along the
    // direction. Breaking that tie by lexicographic order instead makes the
    // choice a property of the point set alone: every oriented line on it, in
    // either direction and whatever points define it, reports this same source.
    const Working previousX = x - directionX;
    const Working previousY = y - directionY;
    if (previousX * previousX + previousY * previousY == x * x + y * y
        && (previousX < x || (previousX == x && previousY < y))) {
        x = previousX;
        y = previousY;
    }

    return std::array<Working, 4>{x, y, x + directionX, y + directionY};
}

}  // namespace detail

// The parameters are named as OrientedLine itself names them, unlike the rest
// of this file: the return type reaches through one of them for a nested type,
// and MSVC matches such a definition to its declaration by spelling, not by
// parameter position, so any other name here is C2244.
template <class PointType_, class TLabel>
template <class ResultNumber>
    requires(is_Rational_v<typename PointType_::NumberType>
             && (detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>))
std::optional<OrientedLine<Point<ResultNumber, typename PointType_::LabelType>, TLabel>>
OrientedLine<PointType_, TLabel>::integralLine() const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using Result = OrientedLine<ResultPoint, LabelType>;
    using Integer = rational_int_t<NumberType>;

    // Both parts of every coordinate are read, and a deferred fraction reduces
    // itself anew on each read, so reduce once here and take the fast path
    // twice. Lowest terms also keep the products below as narrow as they can be.
    const NumberType sourceX = source().x().simplified();
    const NumberType sourceY = source().y().simplified();
    const NumberType targetX = target().x().simplified();
    const NumberType targetY = target().y().simplified();
    const std::array<Integer, 8> parts{sourceX.numerator(), sourceX.denominator(),
                                       sourceY.numerator(), sourceY.denominator(),
                                       targetX.numerator(), targetX.denominator(),
                                       targetY.numerator(), targetY.denominator()};

    // Whether the parts are narrow enough that detail::integralLinePartBits
    // guarantees a ::pgl::int128 holds every intermediate. A coordinate type too
    // narrow to even represent the bound clears it by construction.
    const auto partsFitNarrow = [&] {
        if constexpr (detail::numeric_limits<Integer>::digits <= detail::integralLinePartBits) {
            return true;
        } else {
            const Integer bound = Integer(1) << detail::integralLinePartBits;
            for (const Integer& part : parts) {
                if (part >= bound || part <= -bound) {
                    return false;
                }
            }
            return true;
        }
    };

    const auto widened = [&]<class Working>(std::type_identity<Working>) {
        std::array<Working, 8> result;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            result[i] = Working(parts[i]);
        }
        return result;
    };

    // Both defining points must land in the result type. A line can hit the
    // integer grid arbitrarily far out, so this is a real answer, not a
    // formality — and it is only meaningful because the arithmetic above cannot
    // wrap: a wrapped intermediate would sail through it with a wrong line.
    const auto asLineOverResult = [&](const auto& points) -> std::optional<Result> {
        using Working = typename std::remove_cvref_t<decltype(points)>::value_type;
        for (const Working& coordinate : points) {
            if (!detail::representableAs<ResultNumber>(coordinate)) {
                return {};
            }
        }
        Result line(ResultPoint(detail::narrowTo<ResultNumber>(points[0]),
                                detail::narrowTo<ResultNumber>(points[1])),
                    ResultPoint(detail::narrowTo<ResultNumber>(points[2]),
                                detail::narrowTo<ResultNumber>(points[3])));
        if constexpr (detail::has_label_v<LabelType>) {
            line.label() = label();
        }
        return line;
    };

    // Clearing denominators multiplies the coordinates together several times
    // over, so the arithmetic runs in a type that provably holds the result: a
    // ::pgl::int128 while the parts are narrow enough to guarantee it, and
    // BigInt beyond that, where nothing can overflow at all. Coordinates already
    // built on an arbitrary-precision integer compute in their own type.
    if constexpr (detail::arbitraryPrecision<Integer>) {
        const auto points = detail::integralLinePoints(parts);
        return points ? asLineOverResult(*points) : std::nullopt;
    } else {
        if (partsFitNarrow()) {
            const auto points = detail::integralLinePoints(widened(std::type_identity<pgl::int128>{}));
            return points ? asLineOverResult(*points) : std::nullopt;
        }
        const auto points = detail::integralLinePoints(widened(std::type_identity<BigInt>{}));
        return points ? asLineOverResult(*points) : std::nullopt;
    }
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType, class LabelType>
constexpr Ray<PointType, LabelType>::operator Line<PointType>() const {
    return Line<PointType>(source(), target());
}

template <class PointType, class LabelType>
constexpr Ray<PointType, LabelType>::operator OrientedLine<PointType>() const {
    return OrientedLine<PointType>(source(), target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Ray<PointType, LabelType>& Ray<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Ray<PointType, LabelType>& Ray<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Ray<PointType, LabelType>& Ray<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Ray<PointType, LabelType>& Ray<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Ray<PointType, LabelType>& ray, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = ray.source() - translation;
    const auto second = ray.target() - translation;
    return Ray<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Ray<PointType, LabelType>& ray, const Scalar& scalar) {
    const auto first = ray.source() * scalar;
    const auto second = ray.target() * scalar;
    return Ray<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Ray<PointType, LabelType>& ray) {
    return ray * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Ray<PointType, LabelType>& ray, const Scalar& scalar) {
    const auto first = ray.source() / scalar;
    const auto second = ray.target() / scalar;
    return Ray<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType>
constexpr OrientedSegment<PointType, LabelType>::operator Ray<PointType>() const {
    return Ray<PointType>(source(), target());
}

template <class PointType, class LabelType>
constexpr Ray<PointType, LabelType> Ray<PointType, LabelType>::rotated90(int k) const {
    return Ray(source().rotated90(k), target().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Ray<PointType, LabelType>::rotate90(int k) {
    points_[0].rotate90(k);
    points_[1].rotate90(k);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Ray<PointType, LabelType> Ray<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    return Ray(source().scaledUpX(scalar), target().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Ray<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    points_[0].scaleUpX(scalar);
    points_[1].scaleUpX(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Ray<PointType, LabelType> Ray<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    return Ray(source().scaledUpY(scalar), target().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Ray<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    points_[0].scaleUpY(scalar);
    points_[1].scaleUpY(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Ray<PointType, LabelType> Ray<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    return Ray(source().scaledDownX(scalar), target().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Ray<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    points_[0].scaleDownX(scalar);
    points_[1].scaleDownX(scalar);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Ray<PointType, LabelType> Ray<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    return Ray(source().scaledDownY(scalar), target().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Ray<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    points_[0].scaleDownY(scalar);
    points_[1].scaleDownY(scalar);
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    if (empty()) {
        // The empty set has no points to move.
        return *this;
    }
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    if (empty()) {
        // The empty set has no points to move.
        return *this;
    }
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator*=(const Scalar& scalar) {
    auto saved = label_;
    *this = *this * scalar;
    label_ = std::move(saved);
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator/=(const Scalar& scalar) {
    auto saved = label_;
    *this = *this / scalar;
    label_ = std::move(saved);
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Rectangle<PointType, LabelType>& rectangle, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = rectangle.min() - translation;
    using ResultRectangle = Rectangle<std::decay_t<decltype(first)>, LabelType>;
    if (rectangle.empty()) {
        // The empty set has no points to transform.
        return ResultRectangle();
    }
    const auto second = rectangle.max() - translation;
    return ResultRectangle(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Rectangle<PointType, LabelType>& rectangle, const Scalar& scalar) {
    const auto first = rectangle.min() * scalar;
    using ResultRectangle = Rectangle<std::decay_t<decltype(first)>, LabelType>;
    if (rectangle.empty()) {
        // The empty set has no points to transform.
        return ResultRectangle();
    }
    const auto second = rectangle.max() * scalar;
    return ResultRectangle(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Rectangle<PointType, LabelType>& rectangle) {
    return rectangle * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Rectangle<PointType, LabelType>& rectangle, const Scalar& scalar) {
    const auto first = rectangle.min() / scalar;
    using ResultRectangle = Rectangle<std::decay_t<decltype(first)>, LabelType>;
    if (rectangle.empty()) {
        // The empty set has no points to transform.
        return ResultRectangle();
    }
    const auto second = rectangle.max() / scalar;
    return ResultRectangle(first, second);
}

template <class PointType, class LabelType>
constexpr Rectangle<PointType, LabelType> Rectangle<PointType, LabelType>::rotated90(int k) const {
    if (empty()) {
        // The empty set has no points to transform.
        return Rectangle();
    }
    return Rectangle(min().rotated90(k), max().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Rectangle<PointType, LabelType>::rotate90(int k) {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Rectangle<PointType, LabelType> Rectangle<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    if (empty()) {
        // The empty set has no points to transform.
        return Rectangle();
    }
    return Rectangle(min().scaledUpX(scalar), max().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Rectangle<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Rectangle<PointType, LabelType> Rectangle<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    if (empty()) {
        // The empty set has no points to transform.
        return Rectangle();
    }
    return Rectangle(min().scaledUpY(scalar), max().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Rectangle<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Rectangle<PointType, LabelType> Rectangle<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    if (empty()) {
        // The empty set has no points to transform.
        return Rectangle();
    }
    return Rectangle(min().scaledDownX(scalar), max().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Rectangle<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Rectangle<PointType, LabelType> Rectangle<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    if (empty()) {
        // The empty set has no points to transform.
        return Rectangle();
    }
    return Rectangle(min().scaledDownY(scalar), max().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Rectangle<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Triangle<PointType, LabelType>& Triangle<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    points_[2] += translation;
    normalize();
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Triangle<PointType, LabelType>& Triangle<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    points_[2] -= translation;
    normalize();
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Triangle<PointType, LabelType>& Triangle<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    points_[2] *= scalar;
    normalize();
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Triangle<PointType, LabelType>& Triangle<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    points_[2] /= scalar;
    normalize();
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Triangle<PointType, LabelType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = triangle.a() - translation;
    const auto second = triangle.b() - translation;
    const auto third = triangle.c() - translation;
    return Triangle<std::decay_t<decltype(first)>, LabelType>(first, second, third);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Triangle<PointType, LabelType>& triangle, const Scalar& scalar) {
    const auto first = triangle.a() * scalar;
    const auto second = triangle.b() * scalar;
    const auto third = triangle.c() * scalar;
    return Triangle<std::decay_t<decltype(first)>, LabelType>(first, second, third);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Triangle<PointType, LabelType>& triangle) {
    return triangle * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Triangle<PointType, LabelType>& triangle, const Scalar& scalar) {
    const auto first = triangle.a() / scalar;
    const auto second = triangle.b() / scalar;
    const auto third = triangle.c() / scalar;
    return Triangle<std::decay_t<decltype(first)>, LabelType>(first, second, third);
}

template <class PointType, class LabelType>
constexpr Triangle<PointType, LabelType> Triangle<PointType, LabelType>::rotated90(int k) const {
    return Triangle(a().rotated90(k), b().rotated90(k), c().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Triangle<PointType, LabelType>::rotate90(int k) {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Triangle<PointType, LabelType> Triangle<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    return Triangle(a().scaledUpX(scalar), b().scaledUpX(scalar), c().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Triangle<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Triangle<PointType, LabelType> Triangle<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    return Triangle(a().scaledUpY(scalar), b().scaledUpY(scalar), c().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Triangle<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Triangle<PointType, LabelType> Triangle<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    return Triangle(a().scaledDownX(scalar), b().scaledDownX(scalar), c().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Triangle<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Triangle<PointType, LabelType> Triangle<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    return Triangle(a().scaledDownY(scalar), b().scaledDownY(scalar), c().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Triangle<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

// -----------------------------------------------------------------------------
// Halfplane

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Halfplane<PointType, LabelType>& Halfplane<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Halfplane<PointType, LabelType>& Halfplane<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Halfplane<PointType, LabelType>& Halfplane<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Halfplane<PointType, LabelType>& Halfplane<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Halfplane<PointType, LabelType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = halfplane.source() - translation;
    const auto second = halfplane.target() - translation;
    return Halfplane<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Halfplane<PointType, LabelType>& halfplane, const Scalar& scalar) {
    const auto first = halfplane.source() * scalar;
    const auto second = halfplane.target() * scalar;
    return Halfplane<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Halfplane<PointType, LabelType>& halfplane) {
    return halfplane * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Halfplane<PointType, LabelType>& halfplane, const Scalar& scalar) {
    const auto first = halfplane.source() / scalar;
    const auto second = halfplane.target() / scalar;
    return Halfplane<std::decay_t<decltype(first)>, LabelType>(first, second);
}


template <class PointType, class LabelType>
constexpr Halfplane<PointType, LabelType> Halfplane<PointType, LabelType>::rotated90(int k) const {
    return Halfplane(source().rotated90(k), target().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Halfplane<PointType, LabelType>::rotate90(int k) {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Halfplane<PointType, LabelType> Halfplane<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    // A negative factor reflects the plane, flipping the inside side; swap
    // source and target so the oriented boundary keeps the correct side.
    if (scalar < OtherNumber{})
        return Halfplane(target().scaledUpX(scalar), source().scaledUpX(scalar));
    return Halfplane(source().scaledUpX(scalar), target().scaledUpX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Halfplane<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Halfplane<PointType, LabelType> Halfplane<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    // A negative factor reflects the plane, flipping the inside side; swap
    // source and target so the oriented boundary keeps the correct side.
    if (scalar < OtherNumber{})
        return Halfplane(target().scaledUpY(scalar), source().scaledUpY(scalar));
    return Halfplane(source().scaledUpY(scalar), target().scaledUpY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Halfplane<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Halfplane<PointType, LabelType> Halfplane<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    // A negative factor reflects the plane, flipping the inside side; swap
    // source and target so the oriented boundary keeps the correct side.
    if (scalar < OtherNumber{})
        return Halfplane(target().scaledDownX(scalar), source().scaledDownX(scalar));
    return Halfplane(source().scaledDownX(scalar), target().scaledDownX(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Halfplane<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Halfplane<PointType, LabelType> Halfplane<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    // A negative factor reflects the plane, flipping the inside side; swap
    // source and target so the oriented boundary keeps the correct side.
    if (scalar < OtherNumber{})
        return Halfplane(target().scaledDownY(scalar), source().scaledDownY(scalar));
    return Halfplane(source().scaledDownY(scalar), target().scaledDownY(scalar));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Halfplane<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    translation_ += translation;
    // A pure translation leaves maxIndex_ valid (the extreme vertex index is
    // translation-invariant) and merely shifts the bounding box, so update the
    // cached bbox in place rather than discarding it. The hash, however, depends
    // on the absolute vertex positions, so it must be invalidated.
    if (!bbox_.empty()) {
        bbox_ += translation;
    }
    hash_ = hashUnset_;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    translation_ -= translation;
    if (!bbox_.empty()) {
        bbox_ -= translation;
    }
    hash_ = hashUnset_;
    return *this;
}

// Scaling in place cannot keep the stored vertices canonical on its own, so it
// rebuilds through the normalizing constructor -- as rotate90 and the four
// scaleUp/scaleDown mutators beside it already do. A positive factor would be
// safe to apply vertex by vertex, but the other two cases are not: a negative
// factor is a point reflection, which preserves the counterclockwise cycle but
// reverses the lexicographic order, so the canonical lex-min-first rotation no
// longer starts where it must; and a zero factor collapses every vertex onto the
// origin, which has to come back as the one-vertex hull rather than as n copies
// of it. A Convex whose rotation is wrong is not merely untidy: the convex
// predicates binary-search the vertex cycle from that starting point, so such a
// value answers `contains` incorrectly.
template <class PointType, class LabelType>
template <class Scalar>
requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator*=(const Scalar& scalar) {
    std::vector<PointType> scaled;
    scaled.reserve(size());
    for (const PointType& vertex : *this) {
        PointType moved = vertex;
        moved *= scalar;
        scaled.push_back(std::move(moved));
    }
    auto saved = label_;
    *this = Convex(std::move(scaled));
    label_ = std::move(saved);
    return *this;
}

// Rebuilt for the same reason as operator*=: a negative divisor reverses the
// lexicographic order and leaves the canonical rotation starting in the wrong
// place.
template <class PointType, class LabelType>
template <class Scalar>
requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator/=(const Scalar& scalar) {
    std::vector<PointType> scaled;
    scaled.reserve(size());
    for (const PointType& vertex : *this) {
        PointType moved = vertex;
        moved /= scalar;
        scaled.push_back(std::move(moved));
    }
    auto saved = label_;
    *this = Convex(std::move(scaled));
    label_ = std::move(saved);
    return *this;
}

template <class PointType, class LabelType>
constexpr Convex<PointType, LabelType> Convex<PointType, LabelType>::rotated90(int k) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.rotated90(k));
    }
    return Convex(std::move(pts));
}

template <class PointType, class LabelType>
constexpr void Convex<PointType, LabelType>::rotate90(int k) {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Convex<PointType, LabelType> Convex<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpX(scalar));
    }
    return Convex(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Convex<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Convex<PointType, LabelType> Convex<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpY(scalar));
    }
    return Convex(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Convex<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Convex<PointType, LabelType> Convex<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownX(scalar));
    }
    return Convex(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Convex<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Convex<PointType, LabelType> Convex<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownY(scalar));
    }
    return Convex(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Convex<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

// ---------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
constexpr Polygon<PointType, LabelType> Polygon<PointType, LabelType>::rotated90(int k) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.rotated90(k));
    }
    return Polygon(std::move(pts));
}

template <class PointType, class LabelType>
constexpr void Polygon<PointType, LabelType>::rotate90(int k) {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
constexpr void Polygon<PointType, LabelType>::untangle() {
    // Work directly on the stored (untranslated) vertices: a uniform translation
    // preserves every crossing/containment relation used below, so indices stay
    // meaningful without applying translation_. Orientation is irrelevant to the
    // moves, so the sequence is only renormalized once at the end.
    const auto edge = [this](std::ptrdiff_t a) {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points_.size());
        return Segment<PointType>(points_[static_cast<std::size_t>(a)],
                                  points_[static_cast<std::size_t>((a + 1) % n)]);
    };

    while (points_.size() >= 3) {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points_.size());

        // 1) Uncross a transversally crossing pair of non-adjacent edges. The
        //    reversal of the sub-path v[i+1..j] strictly shortens the perimeter,
        //    so this can happen only finitely often for a fixed vertex count.
        bool flipped = false;
        for (std::ptrdiff_t i = 0; i < n && !flipped; ++i) {
            for (std::ptrdiff_t j = i + 1; j < n; ++j) {
                const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
                if (adjacent) {
                    continue;
                }
                if (edge(i).crosses(edge(j))) {
                    std::reverse(points_.begin() + (i + 1), points_.begin() + (j + 1));
                    flipped = true;
                    break;
                }
            }
        }
        if (flipped) {
            continue;
        }

        // 2) No transversal crossing remains, so any residual self-contact is a
        //    collinear touch/overlap, a coincident vertex, or a zero-length edge.
        //    In each case a vertex lies on a non-incident edge; deleting it clears
        //    the contact and strictly decreases the vertex count.
        bool removed = false;
        for (std::ptrdiff_t k = 0; k < n && !removed; ++k) {
            if (points_[static_cast<std::size_t>(k)] ==
                points_[static_cast<std::size_t>((k + 1) % n)]) {
                points_.erase(points_.begin() + k);  // zero-length edge
                removed = true;
                break;
            }
            for (std::ptrdiff_t e = 0; e < n; ++e) {
                if (e == k || e == (k - 1 + n) % n) {
                    continue;  // edges incident to vertex k always contain it
                }
                if (edge(e).contains(points_[static_cast<std::size_t>(k)])) {
                    points_.erase(points_.begin() + k);
                    removed = true;
                    break;
                }
            }
        }
        if (!removed) {
            break;  // no crossing and no self-contact: the polygon is simple
        }
    }

    normalize();
    resetCache();
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polygon<PointType, LabelType> Polygon<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpX(scalar));
    }
    return Polygon(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polygon<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polygon<PointType, LabelType> Polygon<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpY(scalar));
    }
    return Polygon(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polygon<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polygon<PointType, LabelType> Polygon<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownX(scalar));
    }
    return Polygon(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polygon<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polygon<PointType, LabelType> Polygon<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownY(scalar));
    }
    return Polygon(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polygon<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

// ---------------------------------------------------------------------------
// MonotoneChain
//
// Every transformed result goes through the non-trusted constructor: a
// 90-degree rotation swaps the monotone axis, and a negative or zero scale
// factor reverses or collapses the lexicographic order, so the vertex set is
// re-sorted into the canonical chain on the transformed points.

template <class PointType, class LabelType, class Storage>
constexpr MonotoneChain<PointType, LabelType> MonotoneChain<PointType, LabelType, Storage>::rotated90(int k) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.rotated90(k));
    }
    return MonotoneChain<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType, class Storage>
constexpr void MonotoneChain<PointType, LabelType, Storage>::rotate90(int k)
    requires detail::ownsChainStorage<Storage, PointType> {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr MonotoneChain<PointType, LabelType> MonotoneChain<PointType, LabelType, Storage>::scaledUpX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpX(scalar));
    }
    return MonotoneChain<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr void MonotoneChain<PointType, LabelType, Storage>::scaleUpX(const OtherNumber scalar)
    requires detail::ownsChainStorage<Storage, PointType> {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr MonotoneChain<PointType, LabelType> MonotoneChain<PointType, LabelType, Storage>::scaledUpY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpY(scalar));
    }
    return MonotoneChain<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr void MonotoneChain<PointType, LabelType, Storage>::scaleUpY(const OtherNumber scalar)
    requires detail::ownsChainStorage<Storage, PointType> {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr MonotoneChain<PointType, LabelType> MonotoneChain<PointType, LabelType, Storage>::scaledDownX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownX(scalar));
    }
    return MonotoneChain<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr void MonotoneChain<PointType, LabelType, Storage>::scaleDownX(const OtherNumber scalar)
    requires detail::ownsChainStorage<Storage, PointType> {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr MonotoneChain<PointType, LabelType> MonotoneChain<PointType, LabelType, Storage>::scaledDownY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownY(scalar));
    }
    return MonotoneChain<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType, class Storage>
template <class OtherNumber>
constexpr void MonotoneChain<PointType, LabelType, Storage>::scaleDownY(const OtherNumber scalar)
    requires detail::ownsChainStorage<Storage, PointType> {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

// ---------------------------------------------------------------------------
// Polyline
//
// Every transformed result goes through the non-trusted constructor, which
// preserves the vertex sequence and only fixes the direction canonicalization:
// a rotation or a negative scale factor can reverse the lexicographic order of
// the extreme vertices.

template <class PointType, class LabelType>
constexpr Polyline<PointType, LabelType> Polyline<PointType, LabelType>::rotated90(int k) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.rotated90(k));
    }
    return Polyline<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType>
constexpr void Polyline<PointType, LabelType>::rotate90(int k) {
    auto saved = label_;
    *this = rotated90(k);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polyline<PointType, LabelType> Polyline<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpX(scalar));
    }
    return Polyline<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polyline<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polyline<PointType, LabelType> Polyline<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledUpY(scalar));
    }
    return Polyline<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polyline<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledUpY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polyline<PointType, LabelType> Polyline<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownX(scalar));
    }
    return Polyline<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polyline<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownX(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr Polyline<PointType, LabelType> Polyline<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    std::vector<PointType> pts;
    pts.reserve(size());
    for (const auto& p : *this) {
        pts.push_back(p.scaledDownY(scalar));
    }
    return Polyline<PointType, LabelType>(std::move(pts));
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void Polyline<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    auto saved = label_;
    *this = scaledDownY(scalar);
    label_ = std::move(saved);
}

template <class PointType, class LabelType>
template <SegmentConcept OldSegment, SegmentConcept NewSegment>
constexpr std::optional<std::vector<PointType>>
Polyline<PointType, LabelType>::flipVertices(const OldSegment& oldEdge,
                                             const NewSegment& newEdge) const {
    const std::size_t n = size();
    if (n < 2) {
        return std::nullopt;  // no edge to remove
    }

    const PointType o0 = oldEdge[0];
    const PointType o1 = oldEdge[1];
    const PointType m0 = newEdge[0];
    const PointType m1 = newEdge[1];

    // The added edge must be a real, distinct edge.
    if (m0 == m1) {
        return std::nullopt;  // degenerate added edge
    }
    const auto sameSet = [](const PointType& a, const PointType& b, const PointType& c,
                            const PointType& d) {
        return (a == c && b == d) || (a == d && b == c);
    };
    if (sameSet(o0, o1, m0, m1)) {
        return std::nullopt;  // re-adding the removed edge is not a flip
    }

    const std::vector<PointType> v = vertices();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (!sameSet(v[i], v[i + 1], o0, o1)) {
            continue;  // oldEdge does not match this edge
        }
        // A = v[0 .. i] (holds the first vertex), B = v[i+1 .. n-1] (the last).
        std::vector<PointType> out;
        out.reserve(n);
        if (sameSet(m0, m1, v[i], v[n - 1])) {  // add (p_i, p_{n-1}): reverse B
            for (std::size_t k = 0; k <= i; ++k) {
                out.push_back(v[k]);
            }
            for (std::size_t k = n; k-- > i + 1;) {
                out.push_back(v[k]);
            }
            return out;
        }
        if (sameSet(m0, m1, v[0], v[i + 1])) {  // add (p_0, p_{i+1}): reverse A
            for (std::size_t k = i + 1; k-- > 0;) {
                out.push_back(v[k]);
            }
            for (std::size_t k = i + 1; k < n; ++k) {
                out.push_back(v[k]);
            }
            return out;
        }
        if (sameSet(m0, m1, v[0], v[n - 1])) {  // add (p_0, p_{n-1}): reverse both
            for (std::size_t k = i + 1; k-- > 0;) {
                out.push_back(v[k]);
            }
            for (std::size_t k = n; k-- > i + 1;) {
                out.push_back(v[k]);
            }
            return out;
        }
        // This edge matches oldEdge but newEdge is not a valid reconnection for
        // it; a later duplicate of oldEdge might still admit newEdge.
    }
    return std::nullopt;
}

template <class PointType, class LabelType>
template <SegmentConcept OldSegment, SegmentConcept NewSegment>
constexpr bool Polyline<PointType, LabelType>::flippable(const OldSegment& oldEdge,
                                                         const NewSegment& newEdge) const {
    return flipVertices(oldEdge, newEdge).has_value();
}

template <class PointType, class LabelType>
template <SegmentConcept OldSegment, SegmentConcept NewSegment>
constexpr Polyline<PointType, LabelType>
Polyline<PointType, LabelType>::flipped(const OldSegment& oldEdge, const NewSegment& newEdge) const {
    auto vertices = flipVertices(oldEdge, newEdge);
    assert(vertices.has_value());
    return Polyline<PointType, LabelType>(std::move(*vertices));
}

template <class PointType, class LabelType>
template <SegmentConcept OldSegment, SegmentConcept NewSegment>
constexpr void Polyline<PointType, LabelType>::flip(const OldSegment& oldEdge,
                                                    const NewSegment& newEdge) {
    auto saved = label_;
    *this = flipped(oldEdge, newEdge);
    label_ = std::move(saved);
}

// ---------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
constexpr Disk<PointType, LabelType> Disk<PointType, LabelType>::rotated90(int k) const {
    return Disk(a().rotated90(k), b().rotated90(k), c().rotated90(k));
}

template <class PointType, class LabelType>
constexpr void Disk<PointType, LabelType>::rotate90(int k) {
    for (auto& point : points_) {
        point.rotate90(k);
    }
    points_ = canonicalizePoints(points_[0], points_[1], points_[2]);
}

// ---------------------------------------------------------------------------
// Transformation

template <class Number, class ShapeT>
    requires (detail::shapeRank<ShapeT> >= 0 && !RectangleConcept<ShapeT> && !DiskConcept<ShapeT>)
constexpr auto operator*(const Transformation<Number>& transformation, const ShapeT& shape) {
    // Applies the transformation to a single point, promoting the coordinate
    // type the same way every other operator in this file does.
    const auto point = [&transformation](const auto& p) {
        using PLabel = typename std::decay_t<decltype(p)>::LabelType;
        using PNumber = typename std::decay_t<decltype(p)>::NumberType;
        using ResultNumber = std::common_type_t<Number, PNumber>;
        return Point<ResultNumber, PLabel>(
            transformation.a() * p.x() + transformation.b() * p.y() + transformation.tx(),
            transformation.c() * p.x() + transformation.d() * p.y() + transformation.ty());
    };

    if constexpr (detail::is_empty_shape_v<ShapeT>) {
        using ResultNumber = std::common_type_t<Number, typename ShapeT::NumberType>;
        return EmptyShape<Point<ResultNumber, typename ShapeT::LabelType>>{};
    } else if constexpr (PointConcept<ShapeT>) {
        return point(shape);
    } else if constexpr (SegmentConcept<ShapeT>) {
        const auto first = point(shape.min());
        const auto second = point(shape.max());
        return Segment<std::decay_t<decltype(first)>, typename ShapeT::LabelType>(first, second);
    } else if constexpr (OrientedSegmentConcept<ShapeT>) {
        const auto first = point(shape.source());
        const auto second = point(shape.target());
        return OrientedSegment<std::decay_t<decltype(first)>, typename ShapeT::LabelType>(first, second);
    } else if constexpr (LineConcept<ShapeT>) {
        const auto first = point(shape.min());
        const auto second = point(shape.max());
        return Line<std::decay_t<decltype(first)>, typename ShapeT::LabelType>(first, second);
    } else if constexpr (OrientedLineConcept<ShapeT>) {
        // Unlike Halfplane below, an OrientedLine has no "this is a fixed
        // region" contract to preserve: "left"/"right" are just whichever
        // side is currently left/right of source->target, so no swap is
        // needed even when the transformation reverses orientation.
        const auto first = point(shape.source());
        const auto second = point(shape.target());
        return OrientedLine<std::decay_t<decltype(first)>, typename ShapeT::LabelType>(first, second);
    } else if constexpr (RayConcept<ShapeT>) {
        const auto first = point(shape.source());
        const auto second = point(shape.target());
        return Ray<std::decay_t<decltype(first)>, typename ShapeT::LabelType>(first, second);
    } else if constexpr (HalfplaneConcept<ShapeT>) {
        const auto first = point(shape.source());
        const auto second = point(shape.target());
        using ResultPoint = std::decay_t<decltype(first)>;
        // A negative determinant reflects the plane, flipping the inside
        // side; swap source and target so the oriented boundary keeps the
        // correct side, mirroring the same fix in Halfplane::scaledUpX for a
        // negative scale factor.
        if (transformation.determinant() < decltype(transformation.determinant()){}) {
            return Halfplane<ResultPoint, typename ShapeT::LabelType>(second, first);
        }
        return Halfplane<ResultPoint, typename ShapeT::LabelType>(first, second);
    } else if constexpr (TriangleConcept<ShapeT>) {
        // Triangle's constructor normalizes (CCW, lex-min vertex first), so
        // an orientation-reversing transformation is fixed up automatically.
        const auto pa = point(shape.a());
        const auto pb = point(shape.b());
        const auto pc = point(shape.c());
        return Triangle<std::decay_t<decltype(pa)>, typename ShapeT::LabelType>(pa, pb, pc);
    } else if constexpr (ConvexConcept<ShapeT>) {
        // The non-trusted constructor reruns the Graham scan, which both
        // fixes up orientation and prunes any collinear points a degenerate
        // transformation may introduce -- the same reasoning already used by
        // Convex::rotated90.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        std::vector<ResultPoint> pts;
        pts.reserve(shape.size());
        for (const auto& p : shape) {
            pts.push_back(point(p));
        }
        return Convex<ResultPoint, typename ShapeT::LabelType>(std::move(pts));
    } else if constexpr (PolygonConcept<ShapeT>) {
        // The non-trusted constructor renormalizes (CCW, lex-min vertex
        // first), the same reasoning already used by Polygon::rotated90.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        std::vector<ResultPoint> pts;
        pts.reserve(shape.size());
        for (const auto& p : shape) {
            pts.push_back(point(p));
        }
        return Polygon<ResultPoint, typename ShapeT::LabelType>(std::move(pts));
    } else if constexpr (MonotoneChainConcept<ShapeT>) {
        // A general affine map does not preserve x-monotonicity, so the
        // non-trusted constructor re-sorts the transformed vertices into the
        // canonical chain on the mapped point set, the same reasoning already
        // used by MonotoneChain::rotated90.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        std::vector<ResultPoint> pts;
        pts.reserve(shape.size());
        for (const auto& p : shape) {
            pts.push_back(point(p));
        }
        return MonotoneChain<ResultPoint, typename ShapeT::LabelType>(std::move(pts));
    } else if constexpr (PolylineConcept<ShapeT>) {
        // The vertices are mapped in traversal order; the non-trusted
        // constructor only fixes the direction canonicalization, which an
        // orientation-reversing transformation may flip.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        std::vector<ResultPoint> pts;
        pts.reserve(shape.size());
        for (const auto& p : shape) {
            pts.push_back(point(p));
        }
        return Polyline<ResultPoint, typename ShapeT::LabelType>(std::move(pts));
    } else if constexpr (PolygonWithHolesConcept<ShapeT>) {
        // Every ring maps like the Polygon branch above, each through its own
        // non-trusted constructor; the region's normalization then re-sorts the
        // holes, whose relative order an orientation-reversing map can change,
        // the same reasoning already used by PolygonWithHoles::rotated90.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        using ResultPolygon = Polygon<ResultPoint>;
        const auto ring = [&point](const auto& source) {
            std::vector<ResultPoint> pts;
            pts.reserve(source.size());
            for (const auto& p : source) {
                pts.push_back(point(p));
            }
            return ResultPolygon(std::move(pts));
        };

        std::vector<ResultPolygon> holes;
        holes.reserve(shape.holeCount());
        for (const auto& hole : shape.holes()) {
            holes.push_back(ring(hole));
        }
        return PolygonWithHoles<ResultPoint, typename ShapeT::LabelType>(
            ring(shape.outer()), std::move(holes));
    } else if constexpr (PolygonSetConcept<ShapeT>) {
        // Every component maps like the region branch above, through its own
        // non-trusted constructor; the set's normalization then re-sorts the
        // components, whose relative order an orientation-reversing map can
        // change, and drops any that a degenerate map has collapsed.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        using ResultRegion = PolygonWithHoles<ResultPoint>;
        const auto ring = [&point](const auto& source) {
            std::vector<ResultPoint> pts;
            pts.reserve(source.size());
            for (const auto& p : source) {
                pts.push_back(point(p));
            }
            return Polygon<ResultPoint>(std::move(pts));
        };

        std::vector<ResultRegion> components;
        components.reserve(shape.componentCount());
        for (const auto& component : shape) {
            std::vector<Polygon<ResultPoint>> holes;
            holes.reserve(component.holeCount());
            for (const auto& hole : component.holes()) {
                holes.push_back(ring(hole));
            }
            components.emplace_back(ring(component.outer()), std::move(holes));
        }
        return PolygonSet<ResultPoint, typename ShapeT::LabelType>(std::move(components));
    } else if constexpr (HalfplaneIntersectionConcept<ShapeT>) {
        // Each stored half-plane maps like the Halfplane branch above,
        // swapping its defining points under a reflection; the non-trusted
        // range constructor restores the sorted invariant. The sticky empty
        // state is rebuilt from contradictory constraints, since the affine
        // image of the empty set is empty.
        using ResultPoint = decltype(point(std::declval<typename ShapeT::PointType>()));
        using ResultRegion = HalfplaneIntersection<ResultPoint, typename ShapeT::LabelType>;
        using ResultHalfplane = typename ResultRegion::HalfplaneType;
        if (shape.empty()) {
            ResultRegion result;
            result.insert(ResultHalfplane(ResultPoint(0, 0), ResultPoint(0, 1)));
            result.insert(ResultHalfplane(ResultPoint(1, 1), ResultPoint(1, 0)));
            return result;
        }
        const bool reflecting =
            transformation.determinant() < decltype(transformation.determinant()){};
        std::vector<ResultHalfplane> mapped;
        mapped.reserve(shape.size());
        for (const auto& halfplane : shape) {
            const auto first = point(halfplane.source());
            const auto second = point(halfplane.target());
            mapped.push_back(reflecting ? ResultHalfplane(second, first)
                                        : ResultHalfplane(first, second));
        }
        return ResultRegion(mapped);
    }
}

template <class Number, ShapeConcept ShapeT>
constexpr auto operator*(const Transformation<Number>& transformation, const ShapeT& shape) {
    using PointType = typename ShapeT::PointType_;
    using ResultNumber = std::common_type_t<Number, typename PointType::NumberType>;
    using ResultShape = Shape<Point<ResultNumber, typename PointType::LabelType>>;
    return std::visit(
        [&transformation](const auto& value) -> ResultShape {
            if constexpr (requires { transformation * value; }) {
                return ResultShape(transformation * value);
            } else {
                throw std::logic_error(
                    "Transformation::operator* is not defined for the Rectangle/Disk alternative");
            }
        },
        shape.variant());
}


// ---------------------------------------------------------------------------
// HalfplaneIntersection
//
// Translations and rotations preserve the sorted-by-direction invariant up to
// a cyclic rotation, and axis scalings up to a reversal; canonicalizeSorted()
// restores the canonical order in every mutating case. Feasibility and
// non-redundancy are preserved by any bijective affine map, so no rebuild is
// needed — but the truncating operations (integer division) are inexact and
// may leave a representation that no longer matches the exact image.

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr HalfplaneIntersection<PointType, LabelType>&
HalfplaneIntersection<PointType, LabelType>::operator+=(const OtherPoint& translation) {
    for (auto& halfplane : halfplanes_) {
        halfplane += translation;
    }
    resetCache();
    return *this;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr HalfplaneIntersection<PointType, LabelType>&
HalfplaneIntersection<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    for (auto& halfplane : halfplanes_) {
        halfplane -= translation;
    }
    resetCache();
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr HalfplaneIntersection<PointType, LabelType>&
HalfplaneIntersection<PointType, LabelType>::operator*=(const Scalar& scalar) {
    for (auto& halfplane : halfplanes_) {
        halfplane *= scalar;
    }
    canonicalizeSorted();
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr HalfplaneIntersection<PointType, LabelType>&
HalfplaneIntersection<PointType, LabelType>::operator/=(const Scalar& scalar) {
    for (auto& halfplane : halfplanes_) {
        halfplane /= scalar;
    }
    canonicalizeSorted();
    return *this;
}

template <class PointType, class LabelType>
constexpr HalfplaneIntersection<PointType, LabelType>
HalfplaneIntersection<PointType, LabelType>::rotated90(int k) const {
    HalfplaneIntersection result(*this);
    result.rotate90(k);
    return result;
}

template <class PointType, class LabelType>
constexpr void HalfplaneIntersection<PointType, LabelType>::rotate90(int k) {
    for (auto& halfplane : halfplanes_) {
        halfplane.rotate90(k);
    }
    canonicalizeSorted();
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr HalfplaneIntersection<PointType, LabelType>
HalfplaneIntersection<PointType, LabelType>::scaledUpX(const OtherNumber scalar) const {
    HalfplaneIntersection result(*this);
    result.scaleUpX(scalar);
    return result;
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void HalfplaneIntersection<PointType, LabelType>::scaleUpX(const OtherNumber scalar) {
    for (auto& halfplane : halfplanes_) {
        halfplane.scaleUpX(scalar);
    }
    canonicalizeSorted();
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr HalfplaneIntersection<PointType, LabelType>
HalfplaneIntersection<PointType, LabelType>::scaledUpY(const OtherNumber scalar) const {
    HalfplaneIntersection result(*this);
    result.scaleUpY(scalar);
    return result;
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void HalfplaneIntersection<PointType, LabelType>::scaleUpY(const OtherNumber scalar) {
    for (auto& halfplane : halfplanes_) {
        halfplane.scaleUpY(scalar);
    }
    canonicalizeSorted();
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr HalfplaneIntersection<PointType, LabelType>
HalfplaneIntersection<PointType, LabelType>::scaledDownX(const OtherNumber scalar) const {
    HalfplaneIntersection result(*this);
    result.scaleDownX(scalar);
    return result;
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void HalfplaneIntersection<PointType, LabelType>::scaleDownX(const OtherNumber scalar) {
    for (auto& halfplane : halfplanes_) {
        halfplane.scaleDownX(scalar);
    }
    canonicalizeSorted();
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr HalfplaneIntersection<PointType, LabelType>
HalfplaneIntersection<PointType, LabelType>::scaledDownY(const OtherNumber scalar) const {
    HalfplaneIntersection result(*this);
    result.scaleDownY(scalar);
    return result;
}

template <class PointType, class LabelType>
template <class OtherNumber>
constexpr void HalfplaneIntersection<PointType, LabelType>::scaleDownY(const OtherNumber scalar) {
    for (auto& halfplane : halfplanes_) {
        halfplane.scaleDownY(scalar);
    }
    canonicalizeSorted();
}

}  // namespace pgl
