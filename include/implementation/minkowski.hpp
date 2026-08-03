#pragma once

#include "implementation/transformations.hpp"

/**
 * @file minkowski.hpp
 * @brief Minkowski sums of two shapes, and the `operator+` that spells them.
 *
 * The Minkowski sum of two shapes is the set
 * \f$A \oplus B = \{a + b : a \in A,\ b \in B\}\f$. It is a construction, so it
 * lives here rather than in a shape header, and it is defined only for the
 * pairs whose sum is representable by a Pangolin shape
 * (@ref pgl::MinkowskiSummableConcept):
 *
 * - The empty shape absorbs: `∅ ⊕ B` is empty.
 * - Summing with a `Point` is a translation, so it is closed for every shape
 *   kind and returns the other operand's type. This is also what `shape +
 *   point` has always meant, which is why the translating `operator+`
 *   overloads live in this file too.
 * - Two bounded convex shapes (`Point`, `Segment`, `OrientedSegment`,
 *   `Rectangle`, `Triangle`, `Convex`) sum to a `Convex` with at most `m + n`
 *   vertices, computed in `O(m + n)` by merging the two edge-direction
 *   sequences. Two `Rectangle`s are the one non-trivial pair closed under the
 *   sum, and return a `Rectangle`.
 *
 * Every vertex of the result is a sum of two input vertices, so the whole
 * construction is exact in the operands' coordinate type: integers in,
 * integers out.
 *
 * @ref pgl::detail::minkowskiSumOf is the single dispatcher, and
 * @ref pgl::MinkowskiSummableConcept the single gate: widening the set of pairs
 * whose sum is one shape — an unbounded one over
 * @ref pgl::HalfplaneIntersection, say — means relaxing that concept and adding
 * one branch to the dispatcher. Nothing else here, and no shape header, encodes
 * which pairs are allowed.
 *
 * The **non-convex** sums are not here, and are not a widening of that concept:
 * a sum that can enclose a hole is a set of regions rather than a shape, so it
 * needs a triangulation and the boolean engine and lives in
 * `implementation/minkowskisum.hpp`, as an overload set on @ref pgl::Polygon,
 * @ref pgl::PolygonWithHoles and @ref pgl::Polyline over exactly the pairs this
 * file turns away.
 */

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

/** @brief Vertex type of a shape; a `Point` is its own vertex type. */
template <class T> struct minkowskiPointOf { using type = typename T::PointType; };
template <class Number, class Label> struct minkowskiPointOf<Point<Number, Label>> { using type = Point<Number, Label>; };
template <class PointType> struct minkowskiPointOf<Shape<PointType>> { using type = PointType; };
template <class T> using minkowskiPointOf_t = typename minkowskiPointOf<std::remove_cvref_t<T>>::type;

/**
 * @brief Vertex type of the Minkowski sum of two shapes.
 *
 * The coordinate type is promoted the same way `Point + Point` promotes it;
 * the point label type is taken from the left operand, as every other
 * construction does.
 */
template <class A, class B>
using minkowskiPoint_t = Point<
    std::common_type_t<typename minkowskiPointOf_t<A>::NumberType,
                       typename minkowskiPointOf_t<B>::NumberType>,
    typename minkowskiPointOf_t<A>::LabelType>;

template <class A, class B>
    requires MinkowskiSummableConcept<A, B>
constexpr auto minkowskiSumOf(const A& a, const B& b);

/**
 * @brief Returns @p shape translated by @p translation, that is `shape ⊕ {t}`.
 *
 * Every shape kind is closed under translation, so the result has the operand's
 * own type over the promoted point type. Shape labels do not survive a
 * construction and are left default-initialized.
 */
template <class ShapeT, class TranslationNumber, class TranslationLabel>
constexpr auto minkowskiTranslated(const ShapeT& shape,
                                   const Point<TranslationNumber, TranslationLabel>& translation) {
    using Translation = Point<TranslationNumber, TranslationLabel>;
    using ResultPoint = minkowskiPoint_t<ShapeT, Translation>;
    using ResultNumber = typename ResultPoint::NumberType;

    // Moves one vertex, promoting the coordinates exactly as Point + Point does.
    const auto moved = [&translation](const auto& vertex) {
        return ResultPoint(
            static_cast<ResultNumber>(vertex.x()) + static_cast<ResultNumber>(translation.x()),
            static_cast<ResultNumber>(vertex.y()) + static_cast<ResultNumber>(translation.y()));
    };

    if constexpr (is_point_v<ShapeT>) {
        return moved(shape);
    } else if constexpr (is_empty_shape_v<ShapeT>) {
        // The empty set has no point to move; only the point type promotes.
        return EmptyShape<ResultPoint>{};
    } else if constexpr (is_shape_v<ShapeT>) {
        return std::visit(
            [&translation](const auto& alternative) {
                return Shape<ResultPoint>(minkowskiTranslated(alternative, translation));
            },
            shape.variant());
    } else {
        using Label = typename ShapeT::LabelType;

        if constexpr (is_segment_v<ShapeT>) {
            return Segment<ResultPoint, Label>(moved(shape.min()), moved(shape.max()));
        } else if constexpr (is_oriented_segment_v<ShapeT>) {
            return OrientedSegment<ResultPoint, Label>(moved(shape.source()), moved(shape.target()));
        } else if constexpr (is_line_v<ShapeT>) {
            return Line<ResultPoint, Label>(moved(shape.min()), moved(shape.max()));
        } else if constexpr (is_oriented_line_v<ShapeT>) {
            return OrientedLine<ResultPoint, Label>(moved(shape.source()), moved(shape.target()));
        } else if constexpr (is_ray_v<ShapeT>) {
            return Ray<ResultPoint, Label>(moved(shape.source()), moved(shape.target()));
        } else if constexpr (is_halfplane_v<ShapeT>) {
            return Halfplane<ResultPoint, Label>(moved(shape.source()), moved(shape.target()));
        } else if constexpr (is_rectangle_v<ShapeT>) {
            return Rectangle<ResultPoint, Label>(moved(shape.min()), moved(shape.max()));
        } else if constexpr (is_triangle_v<ShapeT>) {
            return Triangle<ResultPoint, Label>(moved(shape.a()), moved(shape.b()), moved(shape.c()));
        } else if constexpr (is_disk_v<ShapeT>) {
            return Disk<ResultPoint, Label>(moved(shape.a()), moved(shape.b()), moved(shape.c()));
        } else {
            // Vector-backed shapes translate in place: their own operator+=
            // keeps whatever cached state and canonical order the shape
            // maintains, which rebuilding from vertices would throw away.
            const auto translate = [&translation](auto result) {
                result += translation;
                if constexpr (has_label_v<Label>) {
                    result.label() = Label{};
                }
                return result;
            };

            if constexpr (is_convex_v<ShapeT>) {
                return translate(Convex<ResultPoint, Label>(shape));
            } else if constexpr (is_polygon_v<ShapeT>) {
                return translate(Polygon<ResultPoint, Label>(shape));
            } else if constexpr (is_monotone_chain_v<ShapeT>) {
                return translate(MonotoneChain<ResultPoint, Label>(shape));
            } else if constexpr (is_polyline_v<ShapeT>) {
                return translate(Polyline<ResultPoint, Label>(shape));
            } else if constexpr (is_polygon_with_holes_v<ShapeT>) {
                return translate(PolygonWithHoles<ResultPoint, Label>(shape));
            } else {
                static_assert(is_halfplane_intersection_v<ShapeT>,
                              "minkowskiTranslated has no branch for this shape kind: every "
                              "shape is closed under translation, so a new one needs one here");
                return translate(HalfplaneIntersection<ResultPoint, Label>(shape));
            }
        }
    }
}

/**
 * @brief Returns the vertices of a bounded convex shape, counterclockwise.
 *
 * The list carries neither repeated nor collinear vertices, so consecutive edge
 * vectors have strictly increasing directions — the invariant the merge below
 * relies on. It is empty only for an empty @ref Convex.
 */
template <class ResultPoint, class ShapeT>
constexpr std::vector<ResultPoint> minkowskiVertices(const ShapeT& shape) {
    using ResultNumber = typename ResultPoint::NumberType;

    std::vector<ResultPoint> vertices;
    const auto append = [&vertices](const auto& vertex) {
        vertices.emplace_back(static_cast<ResultNumber>(vertex.x()),
                              static_cast<ResultNumber>(vertex.y()));
    };

    if constexpr (is_convex_v<ShapeT>) {
        // Convex already stores its hull counterclockwise with the collinear
        // vertices pruned, so a scan here would be pure overhead.
        vertices.reserve(shape.size());
        for (const auto& vertex : shape) {
            append(vertex);
        }
        return vertices;
    } else {
        // At most four vertices, and any of them may be degenerate (a flat
        // triangle, a zero-width rectangle, a collapsed segment), so the scan
        // both orders them and drops what collapsed.
        for (const auto& vertex : shape.vertices()) {
            append(vertex);
        }
        return grahamScan(vertices);
    }
}

/**
 * @brief Orders two nonzero direction vectors by angle.
 *
 * The angle is measured counterclockwise from the positive x axis over
 * `[0, 2π)`, so the comparison first splits the plane into the upper half
 * (including the positive x axis) and the lower half, then breaks ties inside a
 * half with an exact cross product.
 *
 * @return Negative if @p u comes first, positive if @p v does, zero if the two
 * directions are equal.
 */
template <class PointType>
constexpr int minkowskiDirectionOrder(const PointType& u, const PointType& v) {
    using Number = typename PointType::NumberType;
    const Number zero{};

    // Its own `zero` rather than a capture of the one above: for an integral
    // Number that one is a constant expression, so clang reports capturing it as
    // unnecessary, while a Rational one genuinely cannot be used uncaptured.
    const auto half = [](const PointType& direction) {
        const Number origin{};
        return (direction.y() < origin || (direction.y() == origin && direction.x() < origin)) ? 1
                                                                                               : 0;
    };

    const int halfU = half(u);
    const int halfV = half(v);
    if (halfU != halfV) {
        return halfU < halfV ? -1 : 1;
    }

    const auto turn = orientationSign(PointType(zero, zero), u, v);
    if (turn > 0) {
        return -1;  // v lies counterclockwise from u, so u has the smaller angle
    }
    if (turn < 0) {
        return 1;
    }
    return 0;
}

/**
 * @brief Returns the Minkowski sum of two bounded convex shapes.
 *
 * Walks the boundary of the sum directly: its edges are exactly the edges of
 * both operands taken once each in increasing direction order, with equally
 * directed edges of the two operands fused into a single edge. Starting the
 * walk at the sum of the two bottom-most vertices puts both edge sequences in
 * that order already, so the whole construction is a linear merge.
 */
template <class A, class B>
constexpr auto minkowskiConvexSum(const A& a, const B& b) {
    using ResultPoint = minkowskiPoint_t<A, B>;
    using ResultNumber = typename ResultPoint::NumberType;

    std::vector<ResultPoint> left = minkowskiVertices<ResultPoint>(a);
    std::vector<ResultPoint> right = minkowskiVertices<ResultPoint>(b);
    if (left.empty() || right.empty()) {
        return Convex<ResultPoint>();  // an empty convex polygon absorbs
    }

    // Start each walk at its bottom-most (then leftmost) vertex: the outgoing
    // edge there has an angle in [0, π), so the edges that follow it are sorted
    // by angle over the whole [0, 2π) turn.
    const auto rotateToLowest = [](std::vector<ResultPoint>& vertices) {
        auto lowest = vertices.begin();
        for (auto it = vertices.begin() + 1; it != vertices.end(); ++it) {
            if (it->y() < lowest->y() || (it->y() == lowest->y() && it->x() < lowest->x())) {
                lowest = it;
            }
        }
        std::rotate(vertices.begin(), lowest, vertices.end());
    };
    rotateToLowest(left);
    rotateToLowest(right);

    // A one-vertex operand contributes no edge, which is what makes the merge
    // below degrade gracefully into a plain translation.
    const auto edgesOf = [](const std::vector<ResultPoint>& vertices) {
        std::vector<ResultPoint> edges;
        if (vertices.size() >= 2) {
            edges.reserve(vertices.size());
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                const ResultPoint& from = vertices[i];
                const ResultPoint& to = vertices[(i + 1) % vertices.size()];
                edges.emplace_back(to.x() - from.x(), to.y() - from.y());
            }
        }
        return edges;
    };
    const std::vector<ResultPoint> leftEdges = edgesOf(left);
    const std::vector<ResultPoint> rightEdges = edgesOf(right);

    std::vector<ResultPoint> boundary;
    boundary.reserve(leftEdges.size() + rightEdges.size() + 1);
    ResultPoint current(left.front().x() + right.front().x(),
                        left.front().y() + right.front().y());
    boundary.push_back(current);

    std::size_t i = 0;
    std::size_t j = 0;
    while (i < leftEdges.size() || j < rightEdges.size()) {
        ResultPoint step(ResultNumber{}, ResultNumber{});
        if (j == rightEdges.size()) {
            step = leftEdges[i++];
        } else if (i == leftEdges.size()) {
            step = rightEdges[j++];
        } else {
            const int order = minkowskiDirectionOrder(leftEdges[i], rightEdges[j]);
            if (order < 0) {
                step = leftEdges[i++];
            } else if (order > 0) {
                step = rightEdges[j++];
            } else {
                // Equal directions fuse, which is what keeps the result free of
                // collinear vertices.
                step = ResultPoint(leftEdges[i].x() + rightEdges[j].x(),
                                   leftEdges[i].y() + rightEdges[j].y());
                ++i;
                ++j;
            }
        }
        current = ResultPoint(current.x() + step.x(), current.y() + step.y());
        boundary.push_back(current);
    }
    if (boundary.size() > 1) {
        boundary.pop_back();  // the walk closed back onto its first vertex
    }

    // Convex stores its hull from the lexicographically smallest vertex; the
    // walk above starts from the bottom-most one instead.
    std::rotate(boundary.begin(), std::min_element(boundary.begin(), boundary.end()),
                boundary.end());
    return Convex<ResultPoint>(std::move(boundary), true);
}

/**
 * @brief Returns the Minkowski sum of two shapes.
 *
 * Dispatches to the case that gives the tightest result type: an empty shape,
 * a translation, a rectangle, or the general convex merge.
 */
template <class A, class B>
    requires MinkowskiSummableConcept<A, B>
constexpr auto minkowskiSumOf(const A& a, const B& b) {
    using ResultPoint = minkowskiPoint_t<A, B>;

    if constexpr (is_shape_v<A> || is_shape_v<B>) {
        using ResultShape = Shape<ResultPoint>;
        // Only the pair of stored alternatives decides whether the sum exists,
        // and that is not known until run time.
        const auto sum = [](const auto& left, const auto& right) -> ResultShape {
            if constexpr (requires { minkowskiSumOf(left, right); }) {
                return ResultShape(minkowskiSumOf(left, right));
            } else {
                throw std::logic_error(
                    "Shape::minkowskiSum is not defined for this pair of alternatives");
            }
        };
        if constexpr (is_shape_v<A> && is_shape_v<B>) {
            return std::visit(sum, a.variant(), b.variant());
        } else if constexpr (is_shape_v<A>) {
            return std::visit([&b, &sum](const auto& left) { return sum(left, b); }, a.variant());
        } else {
            return std::visit([&a, &sum](const auto& right) { return sum(a, right); }, b.variant());
        }
    } else if constexpr (is_empty_shape_v<A> || is_empty_shape_v<B>) {
        return EmptyShape<ResultPoint>{};
    } else if constexpr (is_point_v<B>) {
        return minkowskiTranslated(a, b);
    } else if constexpr (is_point_v<A>) {
        return minkowskiTranslated(b, a);
    } else if constexpr (is_rectangle_v<A> && is_rectangle_v<B>) {
        // Two axis-aligned rectangles are the one non-trivial pair closed under
        // the sum: opposite corners simply add.
        return Rectangle<ResultPoint>(minkowskiSumOf(a.min(), b.min()),
                                      minkowskiSumOf(a.max(), b.max()));
    } else {
        return minkowskiConvexSum(a, b);
    }
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Member entry points
//
// Each shape forwards to the same dispatcher; the pairs a shape actually
// accepts are fixed by MinkowskiSummableConcept on the declaration.

#define PGL_DEFINE_MINKOWSKI_SUM(SHAPE)                                                  \
    template <class PointType, class LabelType>                                          \
    template <class OtherShape>                                                          \
        requires MinkowskiSummableConcept<SHAPE<PointType, LabelType>, OtherShape>       \
    constexpr auto SHAPE<PointType, LabelType>::minkowskiSum(const OtherShape& other) const { \
        return detail::minkowskiSumOf(*this, other);                                     \
    }

PGL_DEFINE_MINKOWSKI_SUM(Segment)
PGL_DEFINE_MINKOWSKI_SUM(OrientedSegment)
PGL_DEFINE_MINKOWSKI_SUM(Line)
PGL_DEFINE_MINKOWSKI_SUM(OrientedLine)
PGL_DEFINE_MINKOWSKI_SUM(Ray)
PGL_DEFINE_MINKOWSKI_SUM(Halfplane)
PGL_DEFINE_MINKOWSKI_SUM(Rectangle)
PGL_DEFINE_MINKOWSKI_SUM(Triangle)
PGL_DEFINE_MINKOWSKI_SUM(Disk)
PGL_DEFINE_MINKOWSKI_SUM(Convex)
PGL_DEFINE_MINKOWSKI_SUM(Polygon)
PGL_DEFINE_MINKOWSKI_SUM(Polyline)
PGL_DEFINE_MINKOWSKI_SUM(PolygonWithHoles)
PGL_DEFINE_MINKOWSKI_SUM(HalfplaneIntersection)

#undef PGL_DEFINE_MINKOWSKI_SUM

template <class Number, class Label>
template <class OtherShape>
    requires MinkowskiSummableConcept<Point<Number, Label>, OtherShape>
constexpr auto Point<Number, Label>::minkowskiSum(const OtherShape& other) const {
    return detail::minkowskiSumOf(*this, other);
}

template <class PointType>
template <class OtherShape>
    requires MinkowskiSummableConcept<EmptyShape<PointType>, OtherShape>
constexpr auto EmptyShape<PointType>::minkowskiSum(const OtherShape& other) const {
    return detail::minkowskiSumOf(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <class OtherShape>
    requires MinkowskiSummableConcept<MonotoneChain<PointType, LabelType, Storage>, OtherShape>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::minkowskiSum(const OtherShape& other) const {
    return detail::minkowskiSumOf(*this, other);
}

template <class PointType>
template <class OtherShape>
    requires MinkowskiSummableConcept<Shape<PointType>, OtherShape>
constexpr auto Shape<PointType>::minkowskiSum(const OtherShape& other) const {
    return detail::minkowskiSumOf(*this, other);
}

/**
 * @brief Returns the Minkowski sum `a ⊕ b`, the same as `a.minkowskiSum(b)`.
 *
 * Summing a shape with a `Point` is a translation, which is the reading this
 * operator has always had; the remaining supported pairs
 * (@ref MinkowskiSummableConcept) extend it to the operation `+` denotes for
 * point sets.
 */
template <class A, class B>
    requires MinkowskiSummableConcept<A, B>
[[nodiscard]] constexpr auto operator+(const A& a, const B& b) {
    return a.minkowskiSum(b);
}

}  // namespace pgl
