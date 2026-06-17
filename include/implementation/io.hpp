#pragma once

#include "pgl.hpp"

/**
 * @file io.hpp
 * @brief Stream output helpers for Pangolin value types.
 *
 * Printable geometry makes tests, debugging, and examples much easier to read,
 * so formatted output is implemented centrally here.
 */


namespace pgl {

// -----------------------------------------------------------------------------
// Point

/**
 * @brief Streams a point as `(x,y)` or `label:(x,y)`.
 *
 * @param stream Output stream.
 * @param point Point to print.
 * @return The output stream.
 */
template <class Number, class Label>
std::ostream& operator<<(std::ostream& stream, const Point<Number, Label>& point) {
    if constexpr (detail::has_label_v<Label>) {
        stream << point.label() << ":(" << point.x() << ',' << point.y() << ')';
    } else {
        stream << '(' << point.x() << ',' << point.y() << ')';
    }
    return stream;
}

// -----------------------------------------------------------------------------
// Segment

/**
 * @brief Streams a segment as `p--q`, or `label:{p--q}` when it carries a label.
 *
 * @param stream Output stream.
 * @param segment Segment to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Segment<PointType, LabelType>& segment) {
    if constexpr (detail::has_label_v<LabelType>) {
        stream << segment.label() << ":{" << segment.min() << "--" << segment.max() << "}";
    } else {
        stream << segment.min() << "--" << segment.max();
    }
    return stream;
}

// -----------------------------------------------------------------------------
// OrientedSegment

/**
 * @brief Streams an oriented segment as `p->q`, or `label:{p->q}` when it carries a label.
 *
 * Uses source()/target() (not min()/max()): the orientation is part of the value.
 *
 * @param stream Output stream.
 * @param segment Oriented segment to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const OrientedSegment<PointType, LabelType>& segment) {
    if constexpr (detail::has_label_v<LabelType>) {
        stream << segment.label() << ":{" << segment.source() << "->" << segment.target() << "}";
    } else {
        stream << segment.source() << "->" << segment.target();
    }
    return stream;
}

// -----------------------------------------------------------------------------
// Line

/**
 * @brief Streams a line as `-p--q-`.
 *
 * @param stream Output stream.
 * @param line Line to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Line<PointType, LabelType>& line) {
    if constexpr (detail::has_label_v<LabelType>) {
        stream << line.label() << ":{-" << line.min() << "--" << line.max() << "-}";
    } else {
        stream << '-' << line.min() << "--" << line.max() << '-';
    }
    return stream;
}

// -----------------------------------------------------------------------------
// OrientedLine

/**
 * @brief Streams an oriented line as `-p--q->`.
 *
 * @param stream Output stream.
 * @param line Oriented line to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const OrientedLine<PointType, LabelType>& line) {
    if constexpr (detail::has_label_v<LabelType>) {
        stream << line.label() << ":{-" << line.source() << "--" << line.target() << "->}";
    } else {
        stream << '-' << line.source() << "--" << line.target() << "->";
    }
    return stream;
}

// -----------------------------------------------------------------------------
// Ray

/**
 * @brief Streams a ray as `p--q->`.
 *
 * @param stream Output stream.
 * @param ray Ray to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Ray<PointType, LabelType>& ray) {
    if constexpr (detail::has_label_v<LabelType>) {
        stream << ray.label() << ":{" << ray.source() << "--" << ray.target() << "->}";
    } else {
        stream << ray.source() << "--" << ray.target() << "->";
    }
    return stream;
}

// -----------------------------------------------------------------------------
// Rectangle

/**
 * @brief Streams a rectangle as `[min,max]`.
 *
 * @param stream Output stream.
 * @param rectangle Rectangle to print.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Rectangle<PointType>& rectangle) {
    stream << '[' << rectangle.min() << ',' << rectangle.max() << ']';
    return stream;
}

// -----------------------------------------------------------------------------
// Triangle

/**
 * @brief Streams a triangle as `<abc>`.
 *
 * @param stream Output stream.
 * @param triangle Triangle to print.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Triangle<PointType>& triangle) {
    stream << '<' << triangle.a() << triangle.b() << triangle.c() << '>';
    return stream;
}

// -----------------------------------------------------------------------------
// Halfplane

/**
 * @brief Streams a half-plane as `^-p--q-^`.
 *
 * @param stream Output stream.
 * @param halfplane Half-plane to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Halfplane<PointType, LabelType>& halfplane) {
    if constexpr (detail::has_label_v<LabelType>) {
        stream << halfplane.label() << ":{^-" << halfplane.source() << "--" << halfplane.target() << "-^}";
    } else {
        stream << "^-" << halfplane.source() << "--" << halfplane.target() << "-^";
    }
    return stream;
}

}  // namespace pgl
