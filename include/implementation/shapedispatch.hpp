#pragma once

#include "implementation/closest.hpp"
#include "implementation/distancel1.hpp"
#include "implementation/distancelinf.hpp"

/**
 * @file shapedispatch.hpp
 * @brief The one place where a runtime @ref pgl::Shape operand is unwrapped.
 *
 * Every binary operation that accepts a `Shape` on either side -- a member of
 * `Shape` itself, or the `Shape` overload of a concrete shape's member -- is a
 * one-line call into a dispatcher here. Each dispatcher visits every `Shape`
 * operand down to the alternative it holds, then calls the concrete member for
 * that pair when one exists, and throws @ref pgl::unsupported_operation naming
 * the pair when none does. A pair gains `Shape` support as soon as the concrete
 * member for it is written; nothing here lists pairs.
 *
 * The probes are safe against recursion: a concrete shape's `Shape` overload is
 * constrained on @ref pgl::ShapeConcept, so once both operands are unwrapped it
 * is not a candidate, and nothing converts a concrete shape into a `Shape`
 * implicitly.
 */

namespace pgl {

namespace detail {

template <class T>
struct is_std_optional : std::false_type {};
template <class T>
struct is_std_optional<std::optional<T>> : std::true_type {};

template <class T>
struct is_std_vector : std::false_type {};
template <class T, class A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template <class T>
struct is_std_variant : std::false_type {};
template <class... Ts>
struct is_std_variant<std::variant<Ts...>> : std::true_type {};

// The point type of whatever a concrete operation returns: a shape, or a
// std::optional, std::variant or std::vector of shapes. A variant is read off
// its first alternative, since every alternative shares the point type.
template <class T>
struct result_point_type {
    using type = shape_point_type_t<T>;
};
template <class T>
struct result_point_type<std::optional<T>> : result_point_type<T> {};
template <class T, class A>
struct result_point_type<std::vector<T, A>> : result_point_type<T> {};
template <class T, class... Ts>
struct result_point_type<std::variant<T, Ts...>> : result_point_type<T> {};

template <class T>
using result_point_type_t = typename result_point_type<std::remove_cvref_t<T>>::type;

// Label type carried by the points of a shape or of a Shape.
template <class T>
using shape_label_t = typename shape_point_type_t<std::remove_cvref_t<T>>::LabelType;

// Appends the non-empty connected pieces of a concrete result to @p out. A
// PolygonSet contributes one piece per component; anything that covers no point
// contributes nothing.
template <class ResultShape, class Result>
void appendPieces(std::vector<ResultShape>& out, const Result& result) {
    if constexpr (is_std_optional<Result>::value) {
        if (result) {
            appendPieces(out, *result);
        }
    } else if constexpr (is_std_vector<Result>::value) {
        for (const auto& element : result) {
            appendPieces(out, element);
        }
    } else if constexpr (is_std_variant<Result>::value) {
        std::visit([&out](const auto& alternative) { appendPieces(out, alternative); }, result);
    } else if constexpr (ShapeConcept<Result>) {
        result.visit([&out](const auto& alternative) { appendPieces(out, alternative); });
    } else if constexpr (EmptyShapeConcept<Result>) {
        // The empty set has no piece.
    } else if constexpr (PolygonSetConcept<Result>) {
        for (std::size_t i = 0; i < result.componentCount(); ++i) {
            out.emplace_back(result.component(i));
        }
    } else if (!coversNoPoint(result)) {
        out.emplace_back(result);
    }
}

template <class ResultShape, class Result>
std::vector<ResultShape> piecesAs(const Result& result) {
    std::vector<ResultShape> out;
    appendPieces(out, result);
    return out;
}

// Unwraps every Shape operand, then applies @p op to the concrete pair. @p op
// must return @p Result for every pair of alternatives, throwing for the pairs
// it cannot answer.
template <class Result, class Op, class A, class B>
constexpr Result visitPair(const Op& op, const A& a, const B& b) {
    if constexpr (ShapeConcept<A>) {
        return std::visit([&op, &b](const auto& x) -> Result { return visitPair<Result>(op, x, b); },
                          a.variant());
    } else if constexpr (ShapeConcept<B>) {
        return std::visit([&op, &a](const auto& y) -> Result { return visitPair<Result>(op, a, y); },
                          b.variant());
    } else {
        return op(a, b);
    }
}

// ---------------------------------------------------------------------------
// Predicates

template <class A, class B>
constexpr bool containsAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.contains(y); }) {
                return x.contains(y);
            } else {
                throw unsupportedPair("contains", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool boundaryContainsAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.boundaryContains(y); }) {
                return x.boundaryContains(y);
            } else {
                throw unsupportedPair("boundaryContains", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool interiorContainsAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.interiorContains(y); }) {
                return x.interiorContains(y);
            } else {
                throw unsupportedPair("interiorContains", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool intersectsAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.intersects(y); }) {
                return x.intersects(y);
            } else {
                throw unsupportedPair("intersects", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool interiorsIntersectAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.interiorsIntersect(y); }) {
                return x.interiorsIntersect(y);
            } else {
                throw unsupportedPair("interiorsIntersect", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool separatesAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.separates(y); }) {
                return x.separates(y);
            } else {
                throw unsupportedPair("separates", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool crossesAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.crosses(y); }) {
                return x.crosses(y);
            } else {
                throw unsupportedPair("crosses", x, y);
            }
        },
        a, b);
}

// ---------------------------------------------------------------------------
// Constructions

// The literal intersection, normalized into its connected pieces. The empty set
// on either side has none, whether or not the other side names it.
template <class ResultNumber, class A, class B>
constexpr auto intersectionAny(const A& a, const B& b) {
    using ResultShape = Shape<Point<ResultNumber, shape_label_t<A>>>;
    using Result = std::vector<ResultShape>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (EmptyShapeConcept<decltype(x)> || EmptyShapeConcept<decltype(y)>) {
                return Result{};
            } else if constexpr (requires { x.template intersection<ResultNumber>(y); }) {
                return piecesAs<ResultShape>(x.template intersection<ResultNumber>(y));
            } else {
                throw unsupportedPair("intersection", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
auto regularizedIntersectionAny(const A& a, const B& b) {
    using Result = PolygonSet<Point<ResultNumber, shape_label_t<A>>>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (requires { x.template regularizedIntersection<ResultNumber>(y); }) {
                return Result(x.template regularizedIntersection<ResultNumber>(y));
            } else {
                throw unsupportedPair("regularizedIntersection", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
auto regularizedUnionAny(const A& a, const B& b) {
    using Result = PolygonSet<Point<ResultNumber, shape_label_t<A>>>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (requires { x.template regularizedUnion<ResultNumber>(y); }) {
                return Result(x.template regularizedUnion<ResultNumber>(y));
            } else {
                throw unsupportedPair("regularizedUnion", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
auto differenceAny(const A& a, const B& b) {
    using Result = PolygonSet<Point<ResultNumber, shape_label_t<A>>>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (requires { x.template difference<ResultNumber>(y); }) {
                return Result(x.template difference<ResultNumber>(y));
            } else {
                throw unsupportedPair("difference", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
auto symmetricDifferenceAny(const A& a, const B& b) {
    using Result = PolygonSet<Point<ResultNumber, shape_label_t<A>>>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (requires { x.template symmetricDifference<ResultNumber>(y); }) {
                return Result(x.template symmetricDifference<ResultNumber>(y));
            } else {
                throw unsupportedPair("symmetricDifference", x, y);
            }
        },
        a, b);
}

// ---------------------------------------------------------------------------
// Distances
//
// The results are converted explicitly, because a pair involving a Disk answers
// in detail::floating_result_t<ResultNumber> rather than in ResultNumber: an
// exact request cannot be honoured for a distance realized on a circle, and
// Rational is only explicitly constructible from a floating-point value.

template <class ResultNumber, class A, class B>
constexpr ResultNumber squaredDistanceAny(const A& a, const B& b) {
    return visitPair<ResultNumber>(
        [](const auto& x, const auto& y) -> ResultNumber {
            if constexpr (requires { x.template squaredDistance<ResultNumber>(y); }) {
                return static_cast<ResultNumber>(x.template squaredDistance<ResultNumber>(y));
            } else if constexpr (requires { x.squaredDistance(y); }) {
                return static_cast<ResultNumber>(x.squaredDistance(y));
            } else {
                throw unsupportedPair("squaredDistance", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
constexpr ResultNumber distanceL1Any(const A& a, const B& b) {
    return visitPair<ResultNumber>(
        [](const auto& x, const auto& y) -> ResultNumber {
            if constexpr (requires { x.template distanceL1<ResultNumber>(y); }) {
                return static_cast<ResultNumber>(x.template distanceL1<ResultNumber>(y));
            } else if constexpr (requires { x.distanceL1(y); }) {
                return static_cast<ResultNumber>(x.distanceL1(y));
            } else {
                throw unsupportedPair("distanceL1", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
constexpr ResultNumber distanceLInfAny(const A& a, const B& b) {
    return visitPair<ResultNumber>(
        [](const auto& x, const auto& y) -> ResultNumber {
            if constexpr (requires { x.template distanceLInf<ResultNumber>(y); }) {
                return static_cast<ResultNumber>(x.template distanceLInf<ResultNumber>(y));
            } else if constexpr (requires { x.distanceLInf(y); }) {
                return static_cast<ResultNumber>(x.distanceLInf(y));
            } else {
                throw unsupportedPair("distanceLInf", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
constexpr ResultNumber squaredHausdorffDistanceAny(const A& a, const B& b) {
    return visitPair<ResultNumber>(
        [](const auto& x, const auto& y) -> ResultNumber {
            if constexpr (requires { x.template squaredHausdorffDistance<ResultNumber>(y); }) {
                return x.template squaredHausdorffDistance<ResultNumber>(y);
            } else {
                throw unsupportedPair("squaredHausdorffDistance", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
constexpr ResultNumber hausdorffDistanceL1Any(const A& a, const B& b) {
    return visitPair<ResultNumber>(
        [](const auto& x, const auto& y) -> ResultNumber {
            if constexpr (requires { x.template hausdorffDistanceL1<ResultNumber>(y); }) {
                return x.template hausdorffDistanceL1<ResultNumber>(y);
            } else {
                throw unsupportedPair("hausdorffDistanceL1", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
constexpr ResultNumber hausdorffDistanceLInfAny(const A& a, const B& b) {
    return visitPair<ResultNumber>(
        [](const auto& x, const auto& y) -> ResultNumber {
            if constexpr (requires { x.template hausdorffDistanceLInf<ResultNumber>(y); }) {
                return x.template hausdorffDistanceLInf<ResultNumber>(y);
            } else {
                throw unsupportedPair("hausdorffDistanceLInf", x, y);
            }
        },
        a, b);
}

// ---------------------------------------------------------------------------
// Closest elements

template <class ResultNumber, class A, class B>
constexpr auto closestPointsAny(const A& a, const B& b) {
    using ResultPoint = Point<ResultNumber, shape_label_t<A>>;
    using Result = std::optional<std::array<ResultPoint, 2>>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (requires { x.template closestPoints<ResultNumber>(y); }) {
                const auto pair = x.template closestPoints<ResultNumber>(y);
                if (!pair) {
                    return Result{};
                }
                return Result{std::array<ResultPoint, 2>{ResultPoint((*pair)[0]), ResultPoint((*pair)[1])}};
            } else {
                throw unsupportedPair("closestPoints", x, y);
            }
        },
        a, b);
}

template <class ResultNumber, class A, class B>
constexpr auto closestSegmentsAny(const A& a, const B& b) {
    using ResultSegment = Segment<Point<ResultNumber, shape_label_t<A>>>;
    using Result = std::optional<std::array<ResultSegment, 2>>;
    return visitPair<Result>(
        [](const auto& x, const auto& y) -> Result {
            if constexpr (requires { x.template closestSegments<ResultNumber>(y); }) {
                const auto pair = x.template closestSegments<ResultNumber>(y);
                if (!pair) {
                    return Result{};
                }
                return Result{std::array<ResultSegment, 2>{ResultSegment((*pair)[0]), ResultSegment((*pair)[1])}};
            } else {
                throw unsupportedPair("closestSegments", x, y);
            }
        },
        a, b);
}

// ---------------------------------------------------------------------------
// Local predicates
//
// These answer about the shapes' defining data rather than about their point
// sets, so they exist for far fewer pairs than the topological predicates do
// and a Shape throws for the rest. The point-argument ones are constrained on
// PointConcept in the concrete shapes, so the probe already fails for an
// argument holding anything else.

template <class A, class B>
constexpr bool verticesContainAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.verticesContain(y); }) {
                return x.verticesContain(y);
            } else {
                throw unsupportedPair("verticesContain", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool containsCollinearAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.containsCollinear(y); }) {
                return x.containsCollinear(y);
            } else {
                throw unsupportedPair("containsCollinear", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool containsEndpointAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.containsEndpoint(y); }) {
                return x.containsEndpoint(y);
            } else {
                throw unsupportedPair("containsEndpoint", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool parallelAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.parallel(y); }) {
                return x.parallel(y);
            } else {
                throw unsupportedPair("parallel", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool collinearAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.collinear(y); }) {
                return x.collinear(y);
            } else {
                throw unsupportedPair("collinear", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr bool interiorContainsInteriorAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.interiorContainsInterior(y); }) {
                return x.interiorContainsInterior(y);
            } else {
                throw unsupportedPair("interiorContainsInterior", x, y);
            }
        },
        a, b);
}

// The witness predicate is generic over its argument in every concrete shape
// that has it, so only the receiver decides whether the pair is supported.
template <class A, class B>
constexpr bool pointInsideInteriorContainedInAny(const A& a, const B& b) {
    return visitPair<bool>(
        [](const auto& x, const auto& y) -> bool {
            if constexpr (requires { x.pointInsideInteriorContainedIn(y); }) {
                return x.pointInsideInteriorContainedIn(y);
            } else {
                throw unsupportedPair("pointInsideInteriorContainedIn", x, y);
            }
        },
        a, b);
}

template <class A, class B>
constexpr std::partial_ordering orientationAny(const A& a, const B& b) {
    return visitPair<std::partial_ordering>(
        [](const auto& x, const auto& y) -> std::partial_ordering {
            if constexpr (requires { x.orientation(y); }) {
                return x.orientation(y);
            } else {
                throw unsupportedPair("orientation", x, y);
            }
        },
        a, b);
}

}  // namespace detail

/**
 * @brief Splits the result of a concrete operation into its connected pieces,
 *        each wrapped in a @ref Shape.
 *
 * Accepts every form an `intersection` answers in -- a shape, a
 * `std::optional` of one, a `std::variant` of several kinds, a `std::optional`
 * of such a variant, a `std::vector` of either -- as well as a @ref PolygonSet,
 * which contributes one piece per component, and a `Shape`. A piece that covers
 * no point is dropped, so a disjoint pair gives an empty vector. This is the
 * normalization @ref Shape::intersection applies to every pair.
 *
 * @param result Concrete result to split.
 * @return The pieces, in the order the result lists them.
 */
template <class Result>
[[nodiscard]] auto pieces(const Result& result) {
    return detail::piecesAs<Shape<detail::result_point_type_t<Result>>>(result);
}

}  // namespace pgl
