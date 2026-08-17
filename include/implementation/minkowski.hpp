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
 * - A `Halfplane` and anything bounded and polygonal sum to that same
 *   half-plane, translated to the operand's support point: a half-plane absorbs
 *   whatever is bounded, whether or not it is convex.
 * - Two **convex polyhedra**, at least one of them unbounded (`Halfplane`,
 *   `Line`, `OrientedLine`, `Ray`, `HalfplaneIntersection`, and on the other
 *   side those or any bounded convex shape) sum to a convex polyhedron, returned
 *   as a `HalfplaneIntersection`. See @ref pgl::detail::minkowskiPolyhedralSum.
 *
 * Every vertex of the result is a sum of two input vertices, so the whole
 * construction is exact in the operands' coordinate type: integers in,
 * integers out. The one operand that is not on the lattice to begin with is a
 * `HalfplaneIntersection`, whose vertices are line crossings; a sum with one is
 * exact over @ref pgl::division_result_t coordinates instead, the same type that
 * shape's own accessors report a vertex in.
 *
 * @ref pgl::detail::minkowskiSumOf is the single dispatcher, and
 * @ref pgl::MinkowskiSummableConcept the single gate: widening the set of pairs
 * whose sum is one shape means relaxing that concept and adding one branch to
 * the dispatcher. Nothing else here, and no shape header, encodes which pairs
 * are allowed.
 *
 * The **non-convex** sums are not here, and are not a widening of that concept:
 * a sum that can enclose a hole needs a `PolygonWithHoles` region result, so it
 * needs a triangulation and the boolean engine and lives in
 * `implementation/minkowskisum.hpp`, as an overload set on @ref pgl::Polygon,
 * @ref pgl::PolygonWithHoles and @ref pgl::Polyline over exactly the pairs this
 * file turns away.
 */

#include <algorithm>
#include <cstddef>
#include <optional>
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

/**
 * @brief Vertex type of a Minkowski sum returned as a @ref HalfplaneIntersection.
 *
 * Every other sum in this file puts its result on the lattice the operands
 * live on, because every vertex of it is a sum of two input vertices. A
 * @ref HalfplaneIntersection operand is the one exception: its own vertices are
 * crossings of stored boundary lines, so the point that attains its support in
 * an operand-supplied direction is generally rational, and the sum asks for a
 * coordinate type that can hold one — @ref division_result_t, exactly as that
 * shape's own `vertex` and `getIfPoint` accessors do.
 */
template <class A, class B>
using minkowskiRegionPoint_t = Point<
    std::conditional_t<is_halfplane_intersection_v<A> || is_halfplane_intersection_v<B>,
                       division_result_t<typename minkowskiPoint_t<A, B>::NumberType>,
                       typename minkowskiPoint_t<A, B>::NumberType>,
    typename minkowskiPoint_t<A, B>::LabelType>;

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
            detail::asNumber<ResultNumber>(vertex.x()) + detail::asNumber<ResultNumber>(translation.x()),
            detail::asNumber<ResultNumber>(vertex.y()) + detail::asNumber<ResultNumber>(translation.y()));
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
            // Translation preserves the corner order, so the result needs no
            // normalizing -- and an empty rectangle stays empty.
            return Rectangle<ResultPoint, Label>(moved(shape.min()), moved(shape.max()), true);
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
            } else if constexpr (is_polygon_set_v<ShapeT>) {
                return translate(PolygonSet<ResultPoint, Label>(shape));
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
        vertices.emplace_back(detail::asNumber<ResultNumber>(vertex.x()),
                              detail::asNumber<ResultNumber>(vertex.y()));
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
        // both orders them and drops what collapsed. An empty shape has no
        // vertices to read, and its caller absorbs the empty list.
        if (coversNoPoint(shape)) {
            return vertices;
        }
        for (const auto& vertex : shape.vertices()) {
            append(vertex);
        }
        return grahamScan(vertices);
    }
}

/**
 * @brief Returns the Minkowski sum of a half-plane and a bounded polygonal
 *        shape: the half-plane, translated.
 *
 * A half-plane absorbs anything bounded. With the boundary running from `s` to
 * `t` and `d = t − s`, the half-plane is `{p : cross(d, p − s) ≥ 0}`, and
 * translating it by `q` gives `{p : cross(d, p − s) ≥ cross(d, q)}`. The sum is
 * the union of those over `q ∈ B`, which is the loosest of them:
 *
 *     H ⊕ B = {p : cross(d, p − s) ≥ min_{q ∈ B} cross(d, q)},
 *
 * the half-plane translated by the operand's **support point** in `−d`'s normal
 * direction. That minimum is attained at a vertex, whatever the operand's shape
 * — a linear function on a bounded polygonal set is extremal at an extreme point
 * — so the operand's concavity, holes and disconnection are all irrelevant here,
 * and the answer is exact: one input vertex added to each boundary point.
 *
 * A degenerate half-plane bounds no side and is @ref Halfplane::isUndefined, so
 * as everywhere in the library nothing is promised for one; the comparison picks
 * an arbitrary support vertex and translates by it.
 *
 * @pre The half-plane is defined, and the operand has at least one vertex. An
 *      empty `Convex`, `Polygon` or `PolygonSet` sums to the empty set, which no
 *      half-plane represents; the half-plane comes back untranslated.
 */
template <class HalfplaneT, class ShapeT>
constexpr auto minkowskiHalfplaneSum(const HalfplaneT& halfplane, const ShapeT& shape) {
    using ResultPoint = minkowskiPoint_t<HalfplaneT, ShapeT>;
    using ResultNumber = typename ResultPoint::NumberType;
    using ResultHalfplane = Halfplane<ResultPoint, typename HalfplaneT::LabelType>;

    const auto& source = halfplane.source();
    const auto& target = halfplane.target();
    const ResultNumber dx = detail::asNumber<ResultNumber>(target.x()) - detail::asNumber<ResultNumber>(source.x());
    const ResultNumber dy = detail::asNumber<ResultNumber>(target.y()) - detail::asNumber<ResultNumber>(source.y());

    bool found = false;
    ResultPoint support(ResultNumber{}, ResultNumber{});
    ResultNumber best{};
    for (const auto& vertex : shape.vertices()) {
        const ResultNumber x = static_cast<ResultNumber>(vertex.x());
        const ResultNumber y = static_cast<ResultNumber>(vertex.y());
        const ResultNumber side = dx * y - dy * x;  // cross(d, vertex)
        if (!found || side < best) {
            found = true;
            best = side;
            support = ResultPoint(x, y);
        }
    }
    if (!found) {
        return ResultHalfplane(ResultPoint(detail::asNumber<ResultNumber>(source.x()),
                                           detail::asNumber<ResultNumber>(source.y())),
                               ResultPoint(detail::asNumber<ResultNumber>(target.x()),
                                           detail::asNumber<ResultNumber>(target.y())));
    }
    const auto moved = [&support](const auto& point) {
        return ResultPoint(detail::asNumber<ResultNumber>(point.x()) + support.x(),
                           detail::asNumber<ResultNumber>(point.y()) + support.y());
    };
    return ResultHalfplane(moved(source), moved(target));
}

/**
 * @brief A convex operand reduced to what a Minkowski sum asks of it.
 *
 * The sum of two convex polyhedra is read off their **support functions**, which
 * simply add: writing a closed half-plane as `{p : cross(d, p) >= c}` for a
 * boundary direction `d`, the tightest such constraint a set `S` satisfies has
 * `c = inf_{p in S} cross(d, p)`, and the tightest one `A ⊕ B` satisfies has
 * `c_A(d) + c_B(d)`. So the sum is the intersection, over every candidate
 * direction, of the constraint whose two infima are both finite. That is all
 * three fields here are for:
 *
 * - @ref directions are the candidate directions this operand contributes. Only
 *   they and the other operand's are needed: an edge of the sum is a sum of
 *   faces, so its direction is an edge direction of one of the two operands.
 * - @ref anchors are points of the operand where a finite infimum is attained,
 *   which is what turns the number `c_A(d) + c_B(d)` back into a `Halfplane`:
 *   the sum of the two attaining points is on the boundary of the constraint.
 *   A linear function attains its minimum over a polyhedron at a face, so the
 *   vertices — plus a point of each unbounded edge for a polyhedron that has no
 *   vertex at all — are enough.
 * - @ref recessions generate the recession cone, the directions the operand runs
 *   off in. The infimum is finite exactly when `cross(d, r) >= 0` for every one
 *   of them, which is the whole of the `-∞` test.
 */
template <class ResultPoint>
struct MinkowskiPolyhedron {
    /** @brief Boundary directions this operand can contribute to the sum. */
    std::vector<ResultPoint> directions;
    /** @brief Points of the operand attaining a finite infimum. */
    std::vector<ResultPoint> anchors;
    /** @brief Generators of the recession cone. */
    std::vector<ResultPoint> recessions;
    /** @brief The operand is the empty set, which absorbs. */
    bool empty = false;
};

/** @brief Cross product of two vectors written as points. */
template <class ResultPoint>
constexpr typename ResultPoint::NumberType minkowskiCross(const ResultPoint& u,
                                                          const ResultPoint& v) {
    return u.x() * v.y() - u.y() * v.x();
}

/**
 * @brief Reduces a convex operand to its @ref MinkowskiPolyhedron.
 *
 * Every branch answers in the result's coordinate type, so the caller's
 * arithmetic never sees the operands' own. Only a @ref HalfplaneIntersection
 * needs that type to be divisible: its vertices are crossings of stored boundary
 * lines and are rational even when every half-plane is integral, which is why
 * the pairs involving one carry a rational result while all the others stay on
 * the lattice.
 *
 * @pre The operand is not undefined: a degenerate `Line`, `Ray` or `Halfplane`
 *      has no direction and bounds nothing, as everywhere in the library.
 */
template <class ResultPoint, class ShapeT>
constexpr MinkowskiPolyhedron<ResultPoint> minkowskiPolyhedronOf(const ShapeT& shape) {
    using ResultNumber = typename ResultPoint::NumberType;

    MinkowskiPolyhedron<ResultPoint> polyhedron;
    const ResultNumber zero{};
    const auto cast = [](const auto& point) {
        return ResultPoint(detail::asNumber<ResultNumber>(point.x()),
                           detail::asNumber<ResultNumber>(point.y()));
    };
    const auto reversed = [](const ResultPoint& vector) {
        return ResultPoint(-vector.x(), -vector.y());
    };

    if constexpr (is_point_v<ShapeT>) {
        // A single point bounds every direction and recedes in none. The
        // dispatcher translates instead of coming here, so this is only for
        // completeness.
        polyhedron.anchors.push_back(cast(shape));
    } else if constexpr (is_line_v<ShapeT> || is_oriented_line_v<ShapeT> || is_ray_v<ShapeT> ||
                         is_halfplane_v<ShapeT>) {
        const ResultPoint source = cast(shape[0]);
        const ResultPoint target = cast(shape[1]);
        const ResultPoint forward(target.x() - source.x(), target.y() - source.y());
        polyhedron.anchors.push_back(source);
        if constexpr (is_halfplane_v<ShapeT>) {
            // One constraint, and a recession cone that is the half-plane
            // itself: it runs off along its boundary both ways and inward along
            // the normal, so `cross(d, ·) >= 0` on those three holds exactly for
            // the positive multiples of the boundary direction.
            polyhedron.directions.push_back(forward);
            polyhedron.recessions.push_back(forward);
            polyhedron.recessions.push_back(reversed(forward));
            polyhedron.recessions.emplace_back(-forward.y(), forward.x());
        } else if constexpr (is_ray_v<ShapeT>) {
            // Two constraints pin the ray to its supporting line and one caps it
            // at the source; the cap's boundary direction is the source-ward
            // normal, since `cross((dy, -dx), p)` is `dot(d, p)`.
            polyhedron.directions.push_back(forward);
            polyhedron.directions.push_back(reversed(forward));
            polyhedron.directions.emplace_back(forward.y(), -forward.x());
            polyhedron.recessions.push_back(forward);
        } else {
            // A line is capped nowhere and recedes both ways, so a direction
            // bounds it only when it is parallel to it.
            polyhedron.directions.push_back(forward);
            polyhedron.directions.push_back(reversed(forward));
            polyhedron.recessions.push_back(forward);
            polyhedron.recessions.push_back(reversed(forward));
        }
    } else if constexpr (is_halfplane_intersection_v<ShapeT>) {
        if (shape.empty()) {
            polyhedron.empty = true;
            return polyhedron;
        }
        const std::size_t n = shape.size();
        if (n == 0) {
            // The whole plane constrains nothing and recedes everywhere.
            const ResultNumber one = static_cast<ResultNumber>(1);
            polyhedron.anchors.emplace_back(zero, zero);
            polyhedron.recessions.emplace_back(one, zero);
            polyhedron.recessions.emplace_back(-one, zero);
            polyhedron.recessions.emplace_back(zero, one);
            polyhedron.recessions.emplace_back(zero, -one);
            return polyhedron;
        }

        std::vector<ResultPoint> sources;
        std::vector<ResultNumber> offsets;
        sources.reserve(n);
        offsets.reserve(n);
        polyhedron.directions.reserve(n);
        for (const auto& halfplane : shape) {
            const ResultPoint source = cast(halfplane.source());
            const ResultPoint target = cast(halfplane.target());
            const ResultPoint direction(target.x() - source.x(), target.y() - source.y());
            sources.push_back(source);
            polyhedron.directions.push_back(direction);
            offsets.push_back(minkowskiCross(direction, source));
        }
        if (n == 1) {
            // One stored constraint *is* a half-plane, and its recession cone is
            // two-dimensional, which no pair of edge directions would report.
            const ResultPoint& forward = polyhedron.directions.front();
            polyhedron.anchors.push_back(sources.front());
            polyhedron.recessions.push_back(forward);
            polyhedron.recessions.push_back(reversed(forward));
            polyhedron.recessions.emplace_back(-forward.y(), forward.x());
            return polyhedron;
        }

        // The stored half-planes are sorted by boundary direction, and a
        // cyclically consecutive pair meets in a vertex exactly when it turns
        // left. Where it does not, the region's boundary runs off to infinity
        // between them: forward along the first edge, backward along the second.
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t next = (i + 1) % n;
            const ResultPoint& here = polyhedron.directions[i];
            const ResultPoint& there = polyhedron.directions[next];
            const ResultNumber turn = minkowskiCross(here, there);
            if (turn > zero) {
                // Cramer's rule on the two boundary lines.
                polyhedron.anchors.emplace_back(
                    (offsets[i] * there.x() - offsets[next] * here.x()) / turn,
                    (offsets[i] * there.y() - offsets[next] * here.y()) / turn);
            } else {
                polyhedron.recessions.push_back(here);
                polyhedron.recessions.push_back(reversed(there));
            }
        }
        if (polyhedron.anchors.empty()) {
            // No vertex at all, with two or more constraints: the region is a
            // slab or a line, and each stored boundary line lies in it whole.
            polyhedron.anchors = std::move(sources);
        }
    } else {
        // Bounded and convex: the hull vertices, counterclockwise and with the
        // collinear ones dropped, are both the anchors and the edge directions.
        std::vector<ResultPoint> vertices = minkowskiVertices<ResultPoint>(shape);
        if (vertices.empty()) {
            polyhedron.empty = true;
            return polyhedron;
        }
        if (vertices.size() >= 2) {
            // A shape that collapsed to a segment contributes both directions,
            // as the two edges of the degenerate polygon it is.
            polyhedron.directions.reserve(vertices.size());
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                const ResultPoint& from = vertices[i];
                const ResultPoint& to = vertices[(i + 1) % vertices.size()];
                polyhedron.directions.emplace_back(to.x() - from.x(), to.y() - from.y());
            }
        }
        polyhedron.anchors = std::move(vertices);
    }
    return polyhedron;
}

/**
 * @brief Returns a point of the operand minimizing `cross(direction, ·)`.
 *
 * @return The attaining point, or `std::nullopt` when the infimum is `-∞` — that
 * is, when the operand recedes in a direction the constraint does not bound.
 *
 * @pre The operand is not empty.
 */
template <class ResultPoint>
constexpr std::optional<ResultPoint> minkowskiInfimumPoint(
    const MinkowskiPolyhedron<ResultPoint>& polyhedron, const ResultPoint& direction) {
    using ResultNumber = typename ResultPoint::NumberType;

    for (const auto& recession : polyhedron.recessions) {
        if (crossSign(direction, recession) < 0) {
            return std::nullopt;
        }
    }
    const ResultPoint* best = nullptr;
    ResultNumber least{};
    for (const auto& anchor : polyhedron.anchors) {
        const ResultNumber value = minkowskiCross(direction, anchor);
        if (best == nullptr || value < least) {
            best = &anchor;
            least = value;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return *best;
}

/**
 * @brief Returns the Minkowski sum of two convex polyhedra, one of them
 *        unbounded, as a @ref HalfplaneIntersection.
 *
 * Each candidate direction contributed by either operand
 * (@ref MinkowskiPolyhedron) is clamped once: the two operands' infima in it are
 * looked up, and when both are finite the constraint through the sum of their
 * attaining points is inserted. A direction only one operand bounds is dropped,
 * which is how the sum loses the constraints the other one runs off through —
 * two half-planes facing different ways sum to the whole plane, and nothing at
 * all is inserted.
 *
 * The insertion does the rest: @ref HalfplaneIntersection::insert discards a
 * redundant constraint, keeps the tighter of two sharing a direction, and
 * notices when the region it has left is lower-dimensional. So a sum that
 * happens to be a half-plane, a line or a bounded polygon comes back as exactly
 * that region, recognizable through `getIfHalfplane`, `getIfLine` or
 * `asConvex`.
 *
 * Complexity: `O((n + m)^2)` in the two operands' constraint counts.
 */
template <class A, class B>
constexpr auto minkowskiPolyhedralSum(const A& a, const B& b) {
    using ResultPoint = minkowskiRegionPoint_t<A, B>;
    using Region = HalfplaneIntersection<ResultPoint>;

    const MinkowskiPolyhedron<ResultPoint> left = minkowskiPolyhedronOf<ResultPoint>(a);
    const MinkowskiPolyhedron<ResultPoint> right = minkowskiPolyhedronOf<ResultPoint>(b);
    if (left.empty || right.empty) {
        // Nothing to add to anything: the empty convex polygon is the region
        // constructor that spells the empty region.
        return Region(Convex<ResultPoint>());
    }

    Region region;
    const auto clamp = [&region, &left, &right](const ResultPoint& direction) {
        const std::optional<ResultPoint> here = minkowskiInfimumPoint(left, direction);
        if (!here) {
            return;
        }
        const std::optional<ResultPoint> there = minkowskiInfimumPoint(right, direction);
        if (!there) {
            return;
        }
        const ResultPoint base(here->x() + there->x(), here->y() + there->y());
        region.insert(Halfplane<ResultPoint>(
            base, ResultPoint(base.x() + direction.x(), base.y() + direction.y())));
    };
    for (const auto& direction : left.directions) {
        clamp(direction);
    }
    for (const auto& direction : right.directions) {
        clamp(direction);
    }
    return region;
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

    const auto turn = crossSign(u, v);
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
        // The constructibility is part of the test, not a consequence of it: a
        // pair whose sum is a HalfplaneIntersection over rational coordinates
        // has an answer, but not one a wrapper over integral points can hold.
        const auto sum = [](const auto& left, const auto& right) -> ResultShape {
            if constexpr (requires { ResultShape(minkowskiSumOf(left, right)); }) {
                return ResultShape(minkowskiSumOf(left, right));
            } else {
                throw std::logic_error(
                    "Shape::minkowskiSum is not defined for this pair of alternatives, or its "
                    "result does not fit the wrapper's point type");
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
        // the sum: opposite corners simply add, minima to minima and maxima to
        // maxima. Sweeping anything over the empty set covers nothing.
        if (a.empty() || b.empty()) {
            return Rectangle<ResultPoint>();
        }
        return Rectangle<ResultPoint>(minkowskiSumOf(a.min(), b.min()),
                                      minkowskiSumOf(a.max(), b.max()), true);
    } else if constexpr (is_halfplane_v<A> && !UnboundedConvexConcept<B>) {
        // A half-plane absorbs anything bounded, convex or not, and stays one.
        return minkowskiHalfplaneSum(a, b);
    } else if constexpr (is_halfplane_v<B> && !UnboundedConvexConcept<A>) {
        return minkowskiHalfplaneSum(b, a);
    } else if constexpr (UnboundedConvexConcept<A> || UnboundedConvexConcept<B>) {
        // One operand at least is stored as constraints rather than as
        // vertices, and so is their sum.
        return minkowskiPolyhedralSum(a, b);
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
PGL_DEFINE_MINKOWSKI_SUM(PolygonSet)
PGL_DEFINE_MINKOWSKI_SUM(HalfplaneIntersection)

#undef PGL_DEFINE_MINKOWSKI_SUM

// The one curved pair with an answer: centres add, radii add. It carries a
// ResultNumber of its own because the radii are square roots of what a disk
// stores, so this sum leaves the lattice where every other single-shape sum
// stays on it.
template <class PointType_, class TLabel>
template <class ResultNumber, DiskConcept OtherDisk>
Disk<Point<ResultNumber, typename Disk<PointType_, TLabel>::PointLabelType>>
Disk<PointType_, TLabel>::minkowskiSum(const OtherDisk& other) const {
    using ResultPoint = Point<ResultNumber, PointLabelType>;

    const auto leftCenter = center<ResultNumber>();
    const auto rightCenter = other.template center<ResultNumber>();
    const ResultNumber radii =
        radius<ResultNumber>() + other.template radius<ResultNumber>();
    return Disk<ResultPoint>(ResultPoint(leftCenter.x() + rightCenter.x(),
                                         leftCenter.y() + rightCenter.y()),
                             radii);
}

// The other sum whose support point is not a vertex: a half-plane slides out by
// the disk's radius along its own unit normal, and both of those are square
// roots. Same ResultNumber convention as the disk pair.
template <class PointType_, class TLabel>
template <class ResultNumber, DiskConcept OtherDisk>
Halfplane<Point<ResultNumber, typename PointType_::LabelType>>
Halfplane<PointType_, TLabel>::minkowskiSum(const OtherDisk& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType_::LabelType>;

    const ResultNumber dx = detail::asNumber<ResultNumber>(target().x()) - detail::asNumber<ResultNumber>(source().x());
    const ResultNumber dy = detail::asNumber<ResultNumber>(target().y()) - detail::asNumber<ResultNumber>(source().y());
    // Reported the way Disk::radius reports it: an exact result type has no
    // square root to offer, and says so rather than rounding silently.
    if constexpr (!requires(ResultNumber v) { std::sqrt(v); }) {
        throw std::runtime_error("std::sqrt is not available for the requested ResultNumber type");
    } else {
        const ResultNumber length = std::sqrt(dx * dx + dy * dy);

        // The outward normal of `{p : cross(d, p - s) >= 0}` is `(dy, -dx)/|d|`,
        // and the disk's support point in it is its centre moved r that way.
        const auto center = other.template center<ResultNumber>();
        const ResultNumber radius = other.template radius<ResultNumber>();
        const ResultNumber offsetX = center.x() + radius * dy / length;
        const ResultNumber offsetY = center.y() - radius * dx / length;

        const auto moved = [&offsetX, &offsetY](const auto& point) {
            return ResultPoint(detail::asNumber<ResultNumber>(point.x()) + offsetX,
                               detail::asNumber<ResultNumber>(point.y()) + offsetY);
        };
        return Halfplane<ResultPoint>(moved(source()), moved(target()));
    }
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
Halfplane<Point<ResultNumber, typename Disk<PointType_, TLabel>::PointLabelType>>
Disk<PointType_, TLabel>::minkowskiSum(const OtherHalfplane& other) const {
    return other.template minkowskiSum<ResultNumber>(*this);
}

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
