#ifndef PGL_HPP_INCLUDED
// Entered out of order (before pgl.hpp): defer to the umbrella header,
// which re-includes this file at the correct layer.
#include "pgl.hpp"
#else
#ifndef PGL_IMPLEMENTATION_TRANSFORMATIONS_HPP
#define PGL_IMPLEMENTATION_TRANSFORMATIONS_HPP

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
    coords_[0] += static_cast<Number>(other[0]);
    coords_[1] += static_cast<Number>(other[1]);
    return *this;
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr Point<Number, Label>& Point<Number, Label>::operator-=(const OtherPoint& other) {
    coords_[0] -= static_cast<Number>(other[0]);
    coords_[1] -= static_cast<Number>(other[1]);
    return *this;
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label>& Point<Number, Label>::operator*=(const OtherNumber scalar) {
    coords_[0] *= static_cast<Number>(scalar);
    coords_[1] *= static_cast<Number>(scalar);
    return *this;
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label>& Point<Number, Label>::operator/=(const OtherNumber scalar) {
    coords_[0] /= static_cast<Number>(scalar);
    coords_[1] /= static_cast<Number>(scalar);
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
    coords_[0] *= static_cast<Number>(scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledUpY(const OtherNumber scalar) const {
    return Point<Number, Label>(x(), y() * scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleUpY(const OtherNumber scalar) {
    coords_[1] *= static_cast<Number>(scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledDownX(const OtherNumber scalar) const {
    return Point<Number, Label>(x() / scalar, y());
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleDownX(const OtherNumber scalar) {
    coords_[0] /= static_cast<Number>(scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr Point<Number, Label> Point<Number, Label>::scaledDownY(const OtherNumber scalar) const {
    return Point<Number, Label>(x(), y() / scalar);
}

template <class Number, class Label>
template <class OtherNumber>
constexpr void Point<Number, Label>::scaleDownY(const OtherNumber scalar) {
    coords_[1] /= static_cast<Number>(scalar);
}

template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
constexpr auto operator+(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right) {
    using ResultNumber = std::common_type_t<LeftNumber, RightNumber>;
    return Point<ResultNumber, LeftLabel>(
        static_cast<ResultNumber>(left.x()) + static_cast<ResultNumber>(right.x()),
        static_cast<ResultNumber>(left.y()) + static_cast<ResultNumber>(right.y()));
}

template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
constexpr auto operator-(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right) {
    using ResultNumber = std::common_type_t<LeftNumber, RightNumber>;
    return Point<ResultNumber, LeftLabel>(
        static_cast<ResultNumber>(left.x()) - static_cast<ResultNumber>(right.x()),
        static_cast<ResultNumber>(left.y()) - static_cast<ResultNumber>(right.y()));
}

template <class Number, class Label, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Point<Number, Label>& point, const Scalar& scalar) {
    using ResultNumber = std::common_type_t<Number, Scalar>;
    return Point<ResultNumber, Label>(
        static_cast<ResultNumber>(point.x()) * static_cast<ResultNumber>(scalar),
        static_cast<ResultNumber>(point.y()) * static_cast<ResultNumber>(scalar));
}

template <class Scalar, class Number, class Label>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Point<Number, Label>& point) {
    return point * scalar;
}

template <class Number, class Label, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Point<Number, Label>& point, const Scalar& scalar) {
    using ResultNumber = std::common_type_t<Number, Scalar>;
    return Point<ResultNumber, Label>(
        static_cast<ResultNumber>(point.x()) / static_cast<ResultNumber>(scalar),
        static_cast<ResultNumber>(point.y()) / static_cast<ResultNumber>(scalar));
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
    requires(!detail::is_point_v<Scalar>)
constexpr Segment<PointType, LabelType>& Segment<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Segment<PointType, LabelType>& Segment<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Segment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = segment.min() + translation;
    const auto second = segment.max() + translation;
    return Segment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Segment<PointType, LabelType>& segment) {
    return segment + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Segment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = segment.min() - translation;
    const auto second = segment.max() - translation;
    return Segment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Segment<PointType, LabelType>& segment, const Scalar& scalar) {
    const auto first = segment.min() * scalar;
    const auto second = segment.max() * scalar;
    return Segment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Segment<PointType, LabelType>& segment) {
    return segment * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
constexpr OrientedSegment<PointType, LabelType>& OrientedSegment<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr OrientedSegment<PointType, LabelType>& OrientedSegment<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const OrientedSegment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = segment.source() + translation;
    const auto second = segment.target() + translation;
    return OrientedSegment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const OrientedSegment<PointType, LabelType>& segment) {
    return segment + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedSegment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = segment.source() - translation;
    const auto second = segment.target() - translation;
    return OrientedSegment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const OrientedSegment<PointType, LabelType>& segment, const Scalar& scalar) {
    const auto first = segment.source() * scalar;
    const auto second = segment.target() * scalar;
    return OrientedSegment<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedSegment<PointType, LabelType>& segment) {
    return segment * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
constexpr Line<PointType, LabelType>& Line<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Line<PointType, LabelType>& Line<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    if (points_[1] < points_[0]) std::swap(points_[0], points_[1]);
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Line<PointType, LabelType>& line, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = line.min() + translation;
    const auto second = line.max() + translation;
    return Line<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Line<PointType, LabelType>& line) {
    return line + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Line<PointType, LabelType>& line, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = line.min() - translation;
    const auto second = line.max() - translation;
    return Line<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Line<PointType, LabelType>& line, const Scalar& scalar) {
    const auto first = line.min() * scalar;
    const auto second = line.max() * scalar;
    return Line<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Line<PointType, LabelType>& line) {
    return line * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
constexpr OrientedLine<PointType, LabelType>& OrientedLine<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr OrientedLine<PointType, LabelType>& OrientedLine<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const OrientedLine<PointType, LabelType>& line, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = line.source() + translation;
    const auto second = line.target() + translation;
    return OrientedLine<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const OrientedLine<PointType, LabelType>& line) {
    return line + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedLine<PointType, LabelType>& line, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = line.source() - translation;
    const auto second = line.target() - translation;
    return OrientedLine<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const OrientedLine<PointType, LabelType>& line, const Scalar& scalar) {
    const auto first = line.source() * scalar;
    const auto second = line.target() * scalar;
    return OrientedLine<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedLine<PointType, LabelType>& line) {
    return line * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
constexpr Ray<PointType, LabelType>& Ray<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Ray<PointType, LabelType>& Ray<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Ray<PointType, LabelType>& ray, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = ray.source() + translation;
    const auto second = ray.target() + translation;
    return Ray<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Ray<PointType, LabelType>& ray) {
    return ray + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Ray<PointType, LabelType>& ray, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = ray.source() - translation;
    const auto second = ray.target() - translation;
    return Ray<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Ray<PointType, LabelType>& ray, const Scalar& scalar) {
    const auto first = ray.source() * scalar;
    const auto second = ray.target() * scalar;
    return Ray<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Ray<PointType, LabelType>& ray) {
    return ray * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    points_[0] += translation;
    points_[1] += translation;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    points_[0] -= translation;
    points_[1] -= translation;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator*=(const Scalar& scalar) {
    auto saved = label_;
    *this = *this * scalar;
    label_ = std::move(saved);
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Rectangle<PointType, LabelType>& Rectangle<PointType, LabelType>::operator/=(const Scalar& scalar) {
    auto saved = label_;
    *this = *this / scalar;
    label_ = std::move(saved);
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Rectangle<PointType, LabelType>& rectangle, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = rectangle.min() + translation;
    const auto second = rectangle.max() + translation;
    return Rectangle<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Rectangle<PointType, LabelType>& rectangle) {
    return rectangle + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Rectangle<PointType, LabelType>& rectangle, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = rectangle.min() - translation;
    const auto second = rectangle.max() - translation;
    return Rectangle<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Rectangle<PointType, LabelType>& rectangle, const Scalar& scalar) {
    const auto first = rectangle.min() * scalar;
    const auto second = rectangle.max() * scalar;
    return Rectangle<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Rectangle<PointType, LabelType>& rectangle) {
    return rectangle * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Rectangle<PointType, LabelType>& rectangle, const Scalar& scalar) {
    const auto first = rectangle.min() / scalar;
    const auto second = rectangle.max() / scalar;
    return Rectangle<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType>
constexpr Rectangle<PointType, LabelType> Rectangle<PointType, LabelType>::rotated90(int k) const {
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
    requires(!detail::is_point_v<Scalar>)
constexpr Triangle<PointType, LabelType>& Triangle<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    points_[2] *= scalar;
    normalize();
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Triangle<PointType, LabelType>& Triangle<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    points_[2] /= scalar;
    normalize();
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Triangle<PointType, LabelType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = triangle.a() + translation;
    const auto second = triangle.b() + translation;
    const auto third = triangle.c() + translation;
    return Triangle<std::decay_t<decltype(first)>, LabelType>(first, second, third);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Triangle<PointType, LabelType>& triangle) {
    return triangle + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Triangle<PointType, LabelType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = triangle.a() - translation;
    const auto second = triangle.b() - translation;
    const auto third = triangle.c() - translation;
    return Triangle<std::decay_t<decltype(first)>, LabelType>(first, second, third);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Triangle<PointType, LabelType>& triangle, const Scalar& scalar) {
    const auto first = triangle.a() * scalar;
    const auto second = triangle.b() * scalar;
    const auto third = triangle.c() * scalar;
    return Triangle<std::decay_t<decltype(first)>, LabelType>(first, second, third);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Triangle<PointType, LabelType>& triangle) {
    return triangle * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
constexpr Halfplane<PointType, LabelType>& Halfplane<PointType, LabelType>::operator*=(const Scalar& scalar) {
    points_[0] *= scalar;
    points_[1] *= scalar;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr Halfplane<PointType, LabelType>& Halfplane<PointType, LabelType>::operator/=(const Scalar& scalar) {
    points_[0] /= scalar;
    points_[1] /= scalar;
    return *this;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Halfplane<PointType, LabelType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = halfplane.source() + translation;
    const auto second = halfplane.target() + translation;
    return Halfplane<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Halfplane<PointType, LabelType>& halfplane) {
    return halfplane + translation;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Halfplane<PointType, LabelType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation) {
    const auto first = halfplane.source() - translation;
    const auto second = halfplane.target() - translation;
    return Halfplane<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Halfplane<PointType, LabelType>& halfplane, const Scalar& scalar) {
    const auto first = halfplane.source() * scalar;
    const auto second = halfplane.target() * scalar;
    return Halfplane<std::decay_t<decltype(first)>, LabelType>(first, second);
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Halfplane<PointType, LabelType>& halfplane) {
    return halfplane * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
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
    if (bbox_) {
        *bbox_ += translation;
    }
    hash_ = hashUnset_;
    return *this;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator-=(const OtherPoint& translation) {
    translation_ -= translation;
    if (bbox_) {
        *bbox_ -= translation;
    }
    hash_ = hashUnset_;
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
requires(!detail::is_point_v<Scalar>)
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator*=(const Scalar& scalar) {
    for (auto& vertex : points_) {
        vertex += translation_;
        vertex *= scalar;
    }
    translation_ = {};
    resetCache();
    return *this;
}

template <class PointType, class LabelType>
template <class Scalar>
requires(!detail::is_point_v<Scalar>)
constexpr Convex<PointType, LabelType>& Convex<PointType, LabelType>::operator/=(const Scalar& scalar) {
    for (auto& vertex : points_) {
        vertex += translation_;
        vertex /= scalar;
    }
    translation_ = {};
    resetCache();
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

}  // namespace pgl

#endif // PGL_IMPLEMENTATION_TRANSFORMATIONS_HPP
#endif // PGL_HPP_INCLUDED
