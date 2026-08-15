#pragma once

#include "algorithm/triangulation.hpp"

/**
 * @file arrangement.hpp
 * @brief Planar subdivision induced by a set of one-dimensional shapes.
 *
 * An @ref pgl::Arrangement is the decomposition of the plane produced by a
 * collection of segments, rays and lines: its finite **vertices** are the
 * endpoints together with every crossing and overlap end, its **edges** are
 * the pieces those vertices cut the input into, and its **faces** are the
 * connected components of what is left of the plane. Every point of the plane
 * belongs to exactly one of those cells, which is what makes an arrangement
 * the natural back end of any operation that has to answer "the same question" on each
 * piece of a subdivided plane — the regularized booleans, the Minkowski sums,
 * and the connectivity tests behind `separates` all do exactly that.
 *
 * The topology is a doubly connected edge list (DCEL). Halfedges are stored in
 * twin-adjacent pairs, so `twin(h)` is `h.index() ^ 1` and costs no memory
 * access, and the whole structure is three `std::uint32_t` arrays (`origin`,
 * `next`, `face`) beside the vertex coordinates. The three handle families are
 * distinct types (@ref pgl::Arrangement::VertexId,
 * @ref pgl::Arrangement::HalfedgeId and @ref pgl::Arrangement::FaceId),
 * which is what lets @ref pgl::Arrangement::operator[] and
 * @ref pgl::Arrangement::witness be one name each across cell families instead
 * of three, and what lets a caller classify cells of all three dimensions in a
 * single generic loop.
 *
 * Everything is exact: the construction uses only the library's exact
 * predicates and constructions, and coordinates are converted to the caller's
 * number type at extraction time through the usual `ResultNumber` template
 * parameter. Because arrangement vertices are crossings, the arrangement's own
 * point type normally has to be a rational one (`pgl::EPoint` is the usual
 * choice); an integral point type is only adequate when the input segments
 * meet at their endpoints alone. Unbounded edge ends meet at one symbolic
 * vertex at infinity; no finite frame and no fictitious edge is introduced.
 */

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace pgl {

namespace detail {

/**
 * @brief A strongly typed index handle.
 *
 * @tparam Tag Empty type that makes each handle family distinct, so a vertex
 *         handle cannot bind where a face handle is meant and the families can
 *         take part in overload resolution.
 *
 * A default-constructed handle is the invalid one, which removes the need for a
 * `NO_FACE`-style sentinel constant per family. The class costs nothing:
 * `sizeof(Handle) == sizeof(std::uint32_t)` and it is trivially copyable, so a
 * `std::vector<Handle>` has exactly the layout of a `std::vector<std::uint32_t>`
 * and the topology arrays stay as dense as they would be with raw indices.
 *
 * Construction and @ref index are both explicit: an implicit conversion in
 * either direction would give back exactly the confusion the type exists to
 * prevent.
 */
template <class Tag>
class Handle {
public:
    /** @brief The underlying index type. */
    using IndexType = std::uint32_t;

    /** @brief Creates the invalid handle. */
    constexpr Handle() = default;

    /**
     * @brief Creates a handle for a given index.
     *
     * @param index Index of the cell.
     */
    constexpr explicit Handle(IndexType index) : index_(index) {}

    /** @brief Returns the underlying index. */
    [[nodiscard]] constexpr IndexType index() const {
        return index_;
    }

    /** @brief Tells whether the handle refers to a cell. */
    [[nodiscard]] constexpr bool valid() const {
        return index_ != invalidIndex;
    }

    /** @brief Same as @ref valid, for use in a condition. */
    constexpr explicit operator bool() const {
        return valid();
    }

    /** @brief Compares two handles of the same family for equality. */
    constexpr bool operator==(const Handle&) const = default;

    /**
     * @brief Orders two handles of the same family.
     *
     * The order is the index order: arbitrary, but stable, which is all the
     * radial buckets and boundary sets that sort handles need.
     */
    constexpr auto operator<=>(const Handle&) const = default;

private:
    static constexpr IndexType invalidIndex = ~IndexType{};
    IndexType index_ = invalidIndex;
};

}  // namespace detail

}  // namespace pgl

namespace std {

/** @brief Hash support for @ref pgl::detail::Handle, so handles can key a set or map. */
template <class Tag>
struct hash<pgl::detail::Handle<Tag>> {
    /** @brief Returns the hash of the handle's index. */
    std::size_t operator()(const pgl::detail::Handle<Tag>& handle) const noexcept {
        return std::hash<std::uint32_t>{}(handle.index());
    }
};

}  // namespace std

namespace pgl {

namespace detail {

/**
 * @brief The orientation of a **simple** ring: positive when it runs
 *        counterclockwise, negative when clockwise, zero when it bounds no area.
 *
 * The ring turns left at its lexicographically smallest vertex exactly when it
 * runs counterclockwise: both edges there point into the closed right
 * half-plane, so the interior angle is the one swept counterclockwise from the
 * outgoing edge to the incoming one, and that angle is the convex one precisely
 * when the ring encloses what is to its left.
 *
 * The alternative is to sum the signed area, and over rational coordinates that
 * is not a constant-factor difference: adding rationals compounds denominators,
 * so accumulating a thousand of them costs far more than the thousand exact
 * multiplications it is made of. One orientation predicate settles the same
 * question with no accumulation at all.
 *
 * A simple ring of three or more distinct vertices never turns through nothing
 * at its smallest vertex — the two edges there would have to run along the same
 * ray, and so overlap — so the zero answer means a ring that is degenerate
 * rather than one this test cannot decide.
 */
template <class ExactPoint>
int ringOrientation(const std::vector<ExactPoint>& ring) {
    if (ring.size() < 3) {
        return 0;
    }
    const std::size_t leftmost =
        static_cast<std::size_t>(std::min_element(ring.begin(), ring.end()) - ring.begin());
    const ExactPoint& corner = ring[leftmost];
    const ExactPoint& ahead = ring[(leftmost + 1) % ring.size()];
    const ExactPoint& behind = ring[(leftmost + ring.size() - 1) % ring.size()];
    const auto turn = orientationSign(corner, ahead, behind);
    if (turn > 0) {
        return 1;
    }
    return turn < 0 ? -1 : 0;
}

/**
 * @brief Cuts a closed boundary walk into simple rings at its repeated vertices.
 *
 * A boundary walk visits a vertex twice exactly where the region pinches shut
 * there — a hole closing over the outer ring at a point, or two holes meeting.
 * The walk is then a correct description of the boundary but not a polygon, so
 * the loop between the two visits is peeled off as a ring of its own. That is
 * precisely the decomposition @ref PolygonWithHoles wants: the peeled loop is
 * the hole and what remains is the ring it touches.
 */
template <class ExactPoint>
void splitWalkIntoRings(const std::vector<ExactPoint>& walk,
                        std::vector<std::vector<ExactPoint>>& out) {
    std::vector<ExactPoint> pending;
    std::map<ExactPoint, std::size_t> position;
    for (const ExactPoint& vertex : walk) {
        const auto seen = position.find(vertex);
        if (seen != position.end()) {
            const std::size_t from = seen->second;
            out.emplace_back(pending.begin() + static_cast<std::ptrdiff_t>(from), pending.end());
            for (std::size_t i = from; i < pending.size(); ++i) {
                position.erase(pending[i]);
            }
            pending.resize(from);
        }
        position.emplace(vertex, pending.size());
        pending.push_back(vertex);
    }
    if (!pending.empty()) {
        out.push_back(std::move(pending));
    }
}

}  // namespace detail

/**
 * @brief The planar subdivision induced by a set of one-dimensional shapes.
 *
 * @tparam PointType_ Vertex type, @ref pgl::EPoint by default. It must be able
 *         to represent the crossings of the input, so a rational point type
 *         unless the input segments meet only at their endpoints.
 * @tparam TLabel Label carried by an edge (inherited from the input shape that
 *         produced it) and by a face (default-constructed, editable).
 *
 * The cells of the subdivision are its vertices, edges and faces, handled by
 * @ref Arrangement::VertexId, @ref Arrangement::HalfedgeId and
 * @ref Arrangement::FaceId. Each edge is a pair of twin halfedges: `twin(h)`
 * is `h.index() ^ 1`, and the face of a halfedge is always the one on its
 * **left**, so a bounded face is enclosed by a counterclockwise cycle of `next`
 * and the outer boundary of a connected piece of the input runs clockwise.
 *
 * Face 0 is an unbounded face. Lines and pairs of rays can produce several
 * unbounded faces; the empty arrangement has just face 0.
 *
 * Construction is exact and takes any range of points, segment-bounded shapes,
 * lines, oriented lines and rays. A @ref Point becomes an isolated vertex.
 * Half-planes, disks and other two-dimensional input are rejected.
 */
template <class PointType_, class TLabel>
class Arrangement {
    // Tags that keep the three handle families distinct types, so a vertex
    // handle can never be passed where a halfedge or face one is meant.
    struct VertexTag;
    struct HalfedgeTag;
    struct FaceTag;
    class TrapezoidPointLocation;

public:
    /** @brief Handle of a vertex of this arrangement specialization. */
    using VertexId = detail::Handle<VertexTag>;
    /** @brief Handle of a halfedge of this arrangement specialization. */
    using HalfedgeId = detail::Handle<HalfedgeTag>;
    /** @brief Handle of a face of this arrangement specialization. */
    using FaceId = detail::Handle<FaceTag>;
    /** @brief Handle of the arrangement cell containing a finite query point. */
    using CellId = std::variant<VertexId, HalfedgeId, FaceId>;

    /** @brief Vertex type. */
    using PointType = PointType_;
    /** @brief Coordinate type of the vertices. */
    using NumberType = typename PointType::NumberType;
    /** @brief Label type carried by edges and faces. */
    using LabelType = TLabel;
    /** @brief Segment alternative returned for a bounded edge. */
    using SegmentType = Segment<PointType, TLabel>;
    /** @brief Line alternative returned for an edge. */
    using LineType = Line<PointType, TLabel>;
    /** @brief Type returned for a halfedge. */
    using OrientedSegmentType = OrientedSegment<PointType, TLabel>;
    /** @brief Oriented-line alternative returned for a halfedge. */
    using OrientedLineType = OrientedLine<PointType, TLabel>;
    /** @brief Ray alternative returned for a halfedge. */
    using RayType = Ray<PointType, TLabel>;
    /** @brief Geometry of an oriented halfedge. */
    using HalfedgeType = std::variant<OrientedSegmentType, OrientedLineType, RayType>;
    /** @brief Geometry of an unoriented edge. */
    using EdgeType = std::variant<SegmentType, LineType, RayType>;
    /** @brief Vertex or edge met by a directed intersection query. */
    using IntersectionId = std::variant<HalfedgeId, VertexId>;

    static_assert(detail::is_point_v<PointType>, "Arrangement requires pgl::Point vertices");

    /** @brief Creates the empty arrangement: no cell but the unbounded face. */
    Arrangement() {
        buildFaces();
    }

    /**
     * @brief Builds the arrangement of a range of shapes.
     *
     * Accepted shapes are a @ref Point (an isolated vertex),
     * a @ref Segment or @ref OrientedSegment, a @ref Polyline or
     * @ref MonotoneChain, a @ref Polygon, @ref Triangle, @ref Rectangle,
     * @ref Convex or @ref PolygonWithHoles (their boundaries), a @ref Line,
     * @ref OrientedLine or @ref Ray, and a @ref Shape variant holding any of
     * those. Half-planes, disks and other two-dimensional input are rejected.
     *
     * The input may be as degenerate as it likes:
     * segments may cross, overlap collinearly, repeat, share endpoints, dangle
     * with a free end, or run vertically. Overlapping and duplicated stretches
     * are merged into a single edge, which then remembers every input shape it
     * came from (@ref originsOf).
     *
     * Complexity: `O(E log E)` in the size of the arrangement — interning the
     * vertices, sorting each rotational fan, tracing the cycles, and nesting
     * each boundary cycle in the face holding it — plus the splitting step,
     * which is the one term that is not output-sensitive: it is quadratic in the
     * number of *distinct* input segments in the worst case, although a sweep
     * over the segments' `x`-projections keeps the pairs actually tested to those
     * whose bounding boxes overlap. Replacing the split by a sweep that reports
     * the crossings per segment is what the rest of the construction is arranged
     * around; it is what stands between this and a construction whose whole cost
     * follows the size of what it produces.
     *
     * The nesting is the one step with two implementations rather than one, so
     * the bound deserves a word: @ref halfedgesLeftOf answers the batch of `Q`
     * questions either by a sweep, in `O((E + Q) log E)`, or by a scan over the
     * edges per question, in `O(Q E)`, and it takes the scan only when `Q E`
     * comes out below the sweep's own bound. The scan is therefore capped by the
     * inequality that admits it and not a fallback out of the bound; and `Q` is
     * at most `2E`, a question being one boundary cycle and the cycles being
     * halfedge-disjoint, so `O((E + Q) log E)` is `O(E log E)` either way.
     * Input containing a ray or line uses the general carrier overlay, which
     * tests every pair of distinct supporting lines and is quadratic in their
     * number before the same `O(E log E)` topology construction.
     *
     * @tparam ShapeRange Range of shapes.
     * @param shapes Shapes whose subdivision of the plane to compute.
     */
    template <std::ranges::input_range ShapeRange>
    explicit Arrangement(const ShapeRange& shapes) {
        std::vector<InputSegment> segments;
        std::vector<InputCurve> curves;
        std::vector<PointType> isolated;
        collect(shapes, segments, curves, isolated);
        build(segments, curves, isolated);
    }

    /**
     * @brief Builds the arrangement of a range of shapes together with a range
     *        of points.
     *
     * The points are vertices of the arrangement wherever they fall: a point on
     * a shape splits it there, and a point on nothing becomes a vertex of its
     * own, incident to no edge and lying in the interior of the face that holds
     * it. Passing them separately is a convenience — a @ref Point in the shape
     * range does the same thing — for the callers whose points and shapes come
     * from different places, such as the cell decomposition behind
     * @ref pgl::Polygon::separates.
     *
     * @tparam ShapeRange Range of shapes; see the single-range constructor for
     *         what is accepted.
     * @tparam PointRange Range of points.
     * @param shapes Shapes whose subdivision of the plane to compute.
     * @param points Points to add as vertices.
     */
    template <std::ranges::input_range ShapeRange, std::ranges::input_range PointRange>
    Arrangement(const ShapeRange& shapes, const PointRange& points) {
        std::vector<InputSegment> segments;
        std::vector<InputCurve> curves;
        std::vector<PointType> isolated;
        collect(shapes, segments, curves, isolated);
        for (const auto& point : points) {
            isolated.emplace_back(point);
        }
        build(segments, curves, isolated);
    }

    // -------------------------------------------------------------------------
    // Cells

    /** @brief Returns the number of finite vertices. */
    [[nodiscard]] std::size_t vertexCount() const {
        return points_.size();
    }

    /** @brief Returns the number of halfedges: always even, twins being adjacent. */
    [[nodiscard]] std::size_t halfedgeCount() const {
        return origin_.size();
    }

    /** @brief Returns the number of edges, i.e. half the number of halfedges. */
    [[nodiscard]] std::size_t edgeCount() const {
        return origin_.size() / 2;
    }

    /** @brief Returns the number of faces, including every unbounded face. */
    [[nodiscard]] std::size_t faceCount() const {
        return outerCycle_.size();
    }

    /**
     * @brief Returns the position of every finite vertex, in @ref VertexId index order.
     *
     * The symbolic infinity vertex, when present, has handle index
     * `vertexCount()` and no position, so it is absent from both this vector and
     * the count.
     */
    [[nodiscard]] const std::vector<PointType>& vertices() const {
        return points_;
    }

    /** @brief Returns the bounded edges, omitting every ray and line. */
    [[nodiscard]] std::vector<SegmentType> boundedEdges() const {
        std::vector<SegmentType> result;
        result.reserve(edgeCount());
        for (std::size_t i = 0; i < edgeGeometry_.size(); ++i) {
            const EdgeGeometry& geometry = edgeGeometry_[i];
            if (geometry.kind != EdgeKind::segment) {
                continue;
            }
            SegmentType edge(geometry.a, geometry.b);
            if constexpr (detail::has_label_v<TLabel>) {
                edge.label() = edgeLabel_[i];
            }
            result.push_back(std::move(edge));
        }
        return result;
    }

    /**
     * @brief Returns every edge as a segment, line, or ray.
     *
     * The edge at index `i` is the one represented by halfedges `2i` and
     * `2i + 1`. Lines are unoriented; a ray keeps its unique finite source.
     */
    [[nodiscard]] std::vector<EdgeType> edges() const {
        std::vector<EdgeType> result;
        result.reserve(edgeCount());
        for (std::size_t i = 0; i < edgeGeometry_.size(); ++i) {
            const EdgeGeometry& geometry = edgeGeometry_[i];
            if (geometry.kind == EdgeKind::segment) {
                SegmentType edge(geometry.a, geometry.b);
                if constexpr (detail::has_label_v<TLabel>) {
                    edge.label() = edgeLabel_[i];
                }
                result.emplace_back(std::move(edge));
            } else if (geometry.kind == EdgeKind::line) {
                LineType edge(geometry.a, geometry.b);
                if constexpr (detail::has_label_v<TLabel>) {
                    edge.label() = edgeLabel_[i];
                }
                result.emplace_back(std::move(edge));
            } else {
                RayType edge(geometry.a, geometry.b);
                if constexpr (detail::has_label_v<TLabel>) {
                    edge.label() = edgeLabel_[i];
                }
                result.emplace_back(std::move(edge));
            }
        }
        return result;
    }

    /**
     * @brief Returns the position of a vertex.
     *
     * @param v Vertex handle.
     * @throws std::logic_error if @p v is invalid or is the infinity vertex.
     */
    [[nodiscard]] const PointType& operator[](VertexId v) const {
        if (!v.valid() || v.index() >= points_.size()) {
            throw std::logic_error("the fictitious arrangement vertex has no finite position");
        }
        return points_[v.index()];
    }

    /**
     * @brief Returns the geometry of a halfedge, carrying the edge label.
     *
     * The result is an @ref OrientedSegment, @ref OrientedLine or @ref Ray.
     * Segment and line twins reverse orientation. A ray has only one finite
     * source and therefore returns the same ray for both twins.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] HalfedgeType operator[](HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        const EdgeGeometry& geometry = edgeGeometry_[h.index() / 2];
        const TLabel& label = edgeLabel_[h.index() / 2];
        if (geometry.kind == EdgeKind::segment) {
            OrientedSegmentType segment(points_[origin_[h.index()]],
                                        points_[origin_[h.index() ^ 1]]);
            if constexpr (detail::has_label_v<TLabel>) {
                segment.label() = label;
            }
            return segment;
        }
        if (geometry.kind == EdgeKind::line) {
            OrientedLineType line(h.index() % 2 == 0 ? geometry.a : geometry.b,
                                  h.index() % 2 == 0 ? geometry.b : geometry.a);
            if constexpr (detail::has_label_v<TLabel>) {
                line.label() = label;
            }
            return line;
        }
        RayType ray(geometry.a, geometry.b);
        if constexpr (detail::has_label_v<TLabel>) {
            ray.label() = label;
        }
        return ray;
    }

    // -------------------------------------------------------------------------
    // Incidence

    /**
     * @brief Returns the halfedge running along the same edge the other way.
     *
     * @param h Halfedge handle, which must be valid: the invalid handle has no
     *          twin, and flipping its index would only produce a second, less
     *          canonical invalid value.
     */
    [[nodiscard]] HalfedgeId twin(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        return HalfedgeId(h.index() ^ 1);
    }

    /**
     * @brief Returns the next halfedge along the boundary of the face on the left.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] HalfedgeId next(HalfedgeId h) const {
        assert(h.valid() && h.index() < next_.size());
        return HalfedgeId(next_[h.index()]);
    }

    /**
     * @brief Returns the vertex a halfedge leaves.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] VertexId source(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        return VertexId(origin_[h.index()]);
    }

    /**
     * @brief Returns the vertex a halfedge arrives at.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] VertexId target(HalfedgeId h) const {
        return source(twin(h));
    }

    /**
     * @brief Returns the face to the left of a halfedge.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] FaceId face(HalfedgeId h) const {
        assert(h.valid() && h.index() < face_.size());
        return FaceId(face_[h.index()]);
    }

    /**
     * @brief Returns one halfedge leaving a vertex, or the invalid handle when
     *        the vertex is isolated.
     *
     * This is the entry point of the rotational fan around the vertex:
     * `next(twin(h))` is the next halfedge leaving it **clockwise**, and
     * iterating that until the start comes back visits every incident edge
     * exactly once.
     *
     * @param v Vertex handle.
     */
    [[nodiscard]] HalfedgeId outgoing(VertexId v) const {
        assert(v.valid() && v.index() < outgoing_.size());
        return outgoing_[v.index()];
    }

    /**
     * @brief Returns the number of halfedges leaving a vertex.
     *
     * This is the size @ref outgoingHalfedges would return, obtained by walking
     * the rotational fan without building the vector. An isolated vertex has
     * degree zero, and every other vertex has one halfedge per incident edge
     * end, so a vertex where `k` lines cross has degree `2k`.
     *
     * @param v Vertex handle.
     */
    [[nodiscard]] std::size_t degree(VertexId v) const {
        assert(v.valid() && v.index() < outgoing_.size());
        const HalfedgeId start = outgoing_[v.index()];
        if (!start.valid()) {
            return 0;
        }
        std::size_t count = 0;
        HalfedgeId h = start;
        do {
            ++count;
            h = next(twin(h));
        } while (h != start);
        return count;
    }

    /**
     * @brief Returns every halfedge leaving a vertex, in clockwise order.
     *
     * The first entry is @ref outgoing(v), and each following entry is
     * `next(twin(h))` of the previous one. An isolated vertex has no outgoing
     * halfedges and returns an empty vector.
     *
     * @param v Vertex handle.
     */
    [[nodiscard]] std::vector<HalfedgeId> outgoingHalfedges(VertexId v) const {
        assert(v.valid() && v.index() < outgoing_.size());
        std::vector<HalfedgeId> halfedges;
        const HalfedgeId start = outgoing_[v.index()];
        if (!start.valid()) {
            return halfedges;
        }
        HalfedgeId h = start;
        do {
            halfedges.push_back(h);
            h = next(twin(h));
        } while (h != start);
        return halfedges;
    }

    // -------------------------------------------------------------------------
    // The uniform cell interface

    /**
     * @brief Returns the vertex itself, as the witness of a zero-dimensional cell.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @param v Vertex handle.
     * @throws std::logic_error if @p v is the infinity vertex.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] Point<ResultNumber> witness(VertexId v) const {
        return Point<ResultNumber>((*this)[v]);
    }

    /**
     * @brief Returns the midpoint of an edge, which lies in its relative interior.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @param h Halfedge handle.
     * @warning Halves coordinates, so an integral @p ResultNumber may round the
     *          midpoint onto an endpoint.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] Point<ResultNumber> witness(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        const EdgeGeometry& geometry = edgeGeometry_[h.index() / 2];
        const PointType& a = geometry.a;
        const PointType& b = geometry.b;
        const ResultNumber two = static_cast<ResultNumber>(NumberType(2));
        return Point<ResultNumber>(
            (static_cast<ResultNumber>(a.x()) + static_cast<ResultNumber>(b.x())) / two,
            (static_cast<ResultNumber>(a.y()) + static_cast<ResultNumber>(b.y())) / two);
    }

    /**
     * @brief Returns a point strictly inside a bounded face.
     *
     * A face whose boundary is a single simple ring — no hole to fall into and
     * no dangling edge to land on — gets the same witness a polygon does: the
     * midpoint of a diagonal from its leftmost vertex, or the interior point of
     * an ear. That divides the coordinates by two or four and no more, which
     * matters far beyond the cost of computing it: the witness exists to be fed
     * to containment predicates, and an exact predicate costs what the size of
     * its operands says it costs.
     *
     * Any other face — one with holes, or with a slit running into it — is
     * settled by leaving the midpoint of a boundary halfedge along the inward
     * normal and stopping halfway to the first point where that ray meets the
     * face's own boundary, which is the whole of what can stop it since the ray
     * starts inside the face. That answer is a ratio of products of coordinates,
     * so it is the expensive one, and it is used only where it is needed.
     *
     * Either way the cost is linear in the face's boundary, hence linear in the
     * arrangement over all faces together.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @param f Face handle.
     * @pre The face is bounded (@ref isUnbounded is false).
     * @warning The witness is a genuine interior point in exact arithmetic; an
     *          integral @p ResultNumber may round it onto the boundary, exactly
     *          as @ref Triangle::pointInside may.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] Point<ResultNumber> witness(FaceId f) const {
        assert(f.valid() && !isUnbounded(f));
        if (hasSimpleBoundary(f)) {
            return ringWitness<ResultNumber>(outerCycle_[f.index()]);
        }
        return sweptWitness<ResultNumber>(f);
    }

    // -------------------------------------------------------------------------
    // Faces

    /**
     * @brief Tells whether a face's boundary is a single simple ring: no hole,
     *        and no edge with the face on both sides.
     *
     * Those are exactly the faces whose closure is the interior of their outer
     * cycle, so a witness for the ring is a witness for the face. A face never
     * pinches shut at a vertex — two lobes meeting at a point are not connected
     * as open sets, so they are two faces — which is why a dangling edge is the
     * only thing left to rule out.
     *
     * @param f Face handle.
     */
    [[nodiscard]] bool hasSimpleBoundary(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        if (!outerCycle_[f.index()].valid() || !innerCycles(f).empty()) {
            return false;
        }
        const std::uint32_t start = outerCycle_[f.index()].index();
        std::uint32_t h = start;
        do {
            if (face_[h ^ 1] == f.index()) {
                return false;
            }
            h = next_[h];
        } while (h != start);
        return true;
    }

private:
    // The witness of a simple ring, by the argument Polygon::pointInside uses:
    // the ring's leftmost vertex is convex, so either the triangle it makes with
    // its two neighbours is an ear — and its interior point will do — or the
    // nearest vertex inside that triangle cuts a diagonal whose midpoint is
    // interior. Reading the cycle in place keeps this allocation-free.
    template <class ResultNumber>
    [[nodiscard]] Point<ResultNumber> ringWitness(HalfedgeId start) const {
        std::uint32_t leftmost = start.index();
        std::uint32_t beforeLeftmost = start.index();
        std::uint32_t previous = start.index();
        for (std::uint32_t h = next_[start.index()]; h != start.index();
             previous = h, h = next_[h]) {
            if (points_[origin_[h]] < points_[origin_[leftmost]]) {
                leftmost = h;
                beforeLeftmost = previous;
            }
        }
        if (leftmost == start.index()) {
            beforeLeftmost = previous;  // the cycle's last halfedge closes onto it
        }
        const PointType& corner = points_[origin_[leftmost]];
        const PointType& ahead = points_[origin_[next_[leftmost]]];
        const PointType& behind = points_[origin_[beforeLeftmost]];
        const Triangle<PointType> ear(corner, ahead, behind);
        assert(!ear.isDegenerate());

        // The leftmost vertex inside the ear, skipping the ear's own corners.
        const PointType* diagonal = nullptr;
        for (std::uint32_t h = next_[next_[leftmost]]; h != leftmost; h = next_[h]) {
            const PointType& vertex = points_[origin_[h]];
            if (ear.interiorContains(vertex) &&
                (diagonal == nullptr || vertex < *diagonal)) {
                diagonal = &vertex;
            }
        }
        if (diagonal != nullptr) {
            return (Point<ResultNumber>(corner) + Point<ResultNumber>(*diagonal)) /
                   static_cast<ResultNumber>(NumberType(2));
        }
        return ear.template pointInside<ResultNumber>();
    }

    // The witness of a face whose boundary is anything else.
    template <class ResultNumber>
    [[nodiscard]] Point<ResultNumber> sweptWitness(FaceId f) const {
        const HalfedgeId seed = outerCycle_[f.index()];
        const PointType& a = points_[origin_[seed.index()]];
        const PointType& b = points_[origin_[seed.index() ^ 1]];

        // Twice the midpoint of the seed halfedge, and the normal pointing into
        // the face — the face is the one to the left of `seed`, and rotating a
        // direction a quarter turn counterclockwise points left. Keeping the
        // midpoint doubled leaves every quantity below a polynomial in the input
        // coordinates, so the only division is the final one.
        const NumberType midX = a.x() + b.x();
        const NumberType midY = a.y() + b.y();
        const NumberType normalX = a.y() - b.y();
        const NumberType normalY = b.x() - a.x();

        // The first point where the inward ray meets the boundary, as the exact
        // fraction hitNumerator / hitDenominator with a positive denominator.
        NumberType hitNumerator(0);
        NumberType hitDenominator(0);
        const auto offer = [&](NumberType numerator, NumberType denominator) {
            if (denominator < NumberType(0)) {
                numerator = -numerator;
                denominator = -denominator;
            }
            if (numerator <= NumberType(0)) {
                return;  // behind the ray's start, or at it
            }
            if (hitDenominator == NumberType(0) ||
                numerator * hitDenominator < hitNumerator * denominator) {
                hitNumerator = numerator;
                hitDenominator = denominator;
            }
        };

        forEachBoundaryHalfedge(f, [&](HalfedgeId h) {
            if (h.index() / 2 == seed.index() / 2) {
                return;  // the ray leaves its own edge, and meets it nowhere else
            }
            const PointType& p = points_[origin_[h.index()]];
            const PointType& q = points_[origin_[h.index() ^ 1]];
            const NumberType edgeX = q.x() - p.x();
            const NumberType edgeY = q.y() - p.y();
            // Twice the vector from the ray's start to the edge's first endpoint.
            const NumberType toEdgeX = NumberType(2) * p.x() - midX;
            const NumberType toEdgeY = NumberType(2) * p.y() - midY;
            const NumberType denominator = normalX * edgeY - normalY * edgeX;
            const NumberType alongEdge = toEdgeX * normalY - toEdgeY * normalX;
            if (denominator != NumberType(0)) {
                // A proper crossing: the ray meets the edge's line at parameter
                // (toEdge x edge) / 2·denominator, inside the edge when the
                // parameter along the edge stays within [0, 1].
                NumberType along = alongEdge;
                NumberType scale = NumberType(2) * denominator;
                if (scale < NumberType(0)) {
                    along = -along;
                    scale = -scale;
                }
                if (along < NumberType(0) || along > scale) {
                    return;
                }
                offer(toEdgeX * edgeY - toEdgeY * edgeX, NumberType(2) * denominator);
            } else if (alongEdge == NumberType(0)) {
                // The edge lies along the ray: it blocks it at whichever of its
                // endpoints comes first.
                const NumberType squaredNormal = normalX * normalX + normalY * normalY;
                for (const PointType& endpoint : {p, q}) {
                    offer((NumberType(2) * endpoint.x() - midX) * normalX +
                              (NumberType(2) * endpoint.y() - midY) * normalY,
                          NumberType(2) * squaredNormal);
                }
            }
        });

        // A bounded face confines the ray, so it is always stopped.
        assert(hitDenominator != NumberType(0));
        const ResultNumber scale =
            static_cast<ResultNumber>(NumberType(2) * hitDenominator);
        return Point<ResultNumber>(
            static_cast<ResultNumber>(midX * hitDenominator + hitNumerator * normalX) / scale,
            static_cast<ResultNumber>(midY * hitDenominator + hitNumerator * normalY) / scale);
    }

public:
    // -------------------------------------------------------------------------
    // Faces, continued

    /**
     * @brief Tells whether the arrangement contains an unbounded edge.
     *
     * This tests for the symbolic infinity vertex, not for an unbounded face:
     * every arrangement has an unbounded face, including the empty arrangement
     * and arrangements made entirely of segments.
     */
    [[nodiscard]] bool isUnbounded() const {
        return infinity_.valid();
    }

    /**
     * @brief Tells whether a halfedge is adjacent to the symbolic vertex at
     *        infinity.
     *
     * Both twins of a ray or line edge are unbounded. A segment halfedge is
     * bounded, including one obtained by splitting an unbounded input curve
     * between two finite vertices.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] bool isUnbounded(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        return infinity_.valid() &&
               (source(h) == infinity_ || target(h) == infinity_);
    }

    /**
     * @brief Tells whether a face is unbounded.
     *
     * @param f Face handle.
     */
    [[nodiscard]] bool isUnbounded(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        return unboundedFace_[f.index()];
    }

    /** @brief Tells whether a vertex is the symbolic point at infinity. */
    [[nodiscard]] bool isFictitious(VertexId v) const {
        assert(v.valid() && v.index() < topologicalVertexCount());
        return infinity_.valid() && v == infinity_;
    }

    /**
     * @brief Returns a halfedge of the face's outer boundary cycle, which runs
     *        counterclockwise, or the invalid handle for an unbounded face.
     *
     * @param f Face handle.
     */
    [[nodiscard]] HalfedgeId outerCycle(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        return outerCycle_[f.index()];
    }

    /**
     * @brief Returns one halfedge of each of the face's inner boundary cycles,
     *        each of which runs clockwise.
     *
     * An inner cycle is what encloses a hole of the face — either material of
     * the input stranded inside it, or a boundary walk through infinity for an
     * unbounded face.
     *
     * @param f Face handle.
     */
    [[nodiscard]] std::span<const HalfedgeId> innerCycles(FaceId f) const {
        assert(f.valid() && f.index() + 1 < innerOffset_.size());
        const std::size_t from = innerOffset_[f.index()];
        const std::size_t to = innerOffset_[f.index() + 1];
        return std::span<const HalfedgeId>(innerCycle_.data() + from, to - from);
    }

    /**
     * @brief Returns every halfedge bounding a face, outer cycle first.
     *
     * The returned halfedges follow each boundary cycle under @ref next. For a
     * bounded face, its counterclockwise outer cycle comes first, followed by
     * each clockwise inner cycle. An unbounded face has no outer cycle, so
     * its result consists only of its inner cycles. Consequently, the boundary
     * of the empty arrangement is empty.
     *
     * @param f Face handle.
     */
    [[nodiscard]] std::vector<HalfedgeId> boundaryOf(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        std::vector<HalfedgeId> boundary;
        const auto walkCycle = [&](HalfedgeId start) {
            HalfedgeId h = start;
            do {
                boundary.push_back(h);
                h = next(h);
            } while (h != start);
        };
        if (outerCycle_[f.index()].valid()) {
            walkCycle(outerCycle_[f.index()]);
        }
        for (HalfedgeId inner : innerCycles(f)) {
            walkCycle(inner);
        }
        return boundary;
    }

    /**
     * @brief Returns the halfedges of a face's counterclockwise outer boundary.
     *
     * The halfedges follow one another under @ref next. An unbounded face has
     * no outer boundary, so its result is empty.
     *
     * @param f Face handle.
     */
    [[nodiscard]] std::vector<HalfedgeId> outerBoundaryOf(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        std::vector<HalfedgeId> boundary;
        const HalfedgeId start = outerCycle_[f.index()];
        if (!start.valid()) {
            return boundary;
        }
        HalfedgeId h = start;
        do {
            boundary.push_back(h);
            h = next(h);
        } while (h != start);
        return boundary;
    }

    /**
     * @brief Returns the halfedges of every clockwise inner boundary cycle.
     *
     * Each vector follows one cycle under @ref next, and the vectors have the
     * same order as @ref innerCycles. The result is empty when the face has no
     * inner boundaries.
     *
     * @param f Face handle.
     */
    [[nodiscard]] std::vector<std::vector<HalfedgeId>> innerBoundariesOf(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        std::vector<std::vector<HalfedgeId>> boundaries;
        boundaries.reserve(innerCycles(f).size());
        for (HalfedgeId start : innerCycles(f)) {
            std::vector<HalfedgeId>& boundary = boundaries.emplace_back();
            HalfedgeId h = start;
            do {
                boundary.push_back(h);
                h = next(h);
            } while (h != start);
        }
        return boundaries;
    }

    /**
     * @brief Returns the closure of a bounded face as a region.
     *
     * The result is the **regularized** face: a dangling edge sticking into the
     * face, which the face's boundary cycle walks down and back, has no place in
     * a polygon and is dropped, and a boundary cycle that pinches shut at a
     * vertex is cut there into one ring per side, as @ref PolygonWithHoles
     * requires. The vertices are kept as they are otherwise, so a vertex lying
     * in the middle of a straight stretch of boundary stays in the ring.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @param f Face handle.
     * @throws std::logic_error if @p f is invalid or names an unbounded face.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber>> polygonWithHoles(FaceId f) const {
        if (!f.valid() || f.index() >= outerCycle_.size() || isUnbounded(f)) {
            throw std::logic_error(
                "Arrangement::polygonWithHoles is only defined for bounded faces");
        }
        using ExactPolygon = Polygon<PointType>;

        std::vector<ExactPolygon> rings;
        collectRings(cycleRing(outerCycle_[f.index()]), rings);
        assert(!rings.empty());
        // An outer cycle that pinches shut comes apart into several rings, and
        // the one holding the rest — the largest, since they are nested — is the
        // outer boundary.
        const auto largest =
            std::max_element(rings.begin(), rings.end(),
                             [](const ExactPolygon& left, const ExactPolygon& right) {
                                 return left.twiceArea() < right.twiceArea();
                             });
        if (largest != rings.begin()) {
            std::iter_swap(rings.begin(), largest);
        }
        std::vector<ExactPolygon> holes(rings.begin() + 1, rings.end());
        for (HalfedgeId inner : innerCycles(f)) {
            collectRings(cycleRing(inner), holes);
        }
        const PolygonWithHoles<PointType> exact(std::move(rings.front()), std::move(holes));
        return PolygonWithHoles<Point<ResultNumber>>(exact);
    }

    /**
     * @brief Returns the outer boundary constraints of a face as a half-plane
     *        intersection, ignoring holes.
     *
     * For a bounded face, the constraints come from its outer cycle. For an
     * unbounded face, they come from the boundary cycles that visit the
     * symbolic vertex at infinity; bounded inner cycles are holes and are
     * ignored. An edge with the face on both sides is a slit or dangling edge,
     * not part of the regularized outer boundary, and is ignored as well.
     * Consequently, the empty arrangement and the unbounded face outside an
     * isolated bounded component produce the whole plane.
     *
     * Each constraint uses the defining coordinates already stored for the
     * arrangement edge. As for any @ref HalfplaneIntersection, the result is
     * the intersection of the supporting half-planes; it equals the face with
     * its holes filled when that outer boundary is convex.
     *
     * @tparam ResultNumber Coordinate type of the half-planes' defining points.
     * @param f Face handle.
     * @throws std::logic_error if @p f is invalid.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] HalfplaneIntersection<Point<ResultNumber>>
    halfplaneIntersection(FaceId f) const {
        if (!f.valid() || f.index() >= outerCycle_.size()) {
            throw std::logic_error(
                "Arrangement::halfplaneIntersection requires a valid face");
        }

        using ResultPoint = Point<ResultNumber>;
        using ResultHalfplane = Halfplane<ResultPoint>;
        std::vector<ResultHalfplane> halfplanes;
        const auto appendCycle = [&](HalfedgeId start) {
            HalfedgeId h = start;
            do {
                if (face(twin(h)) != f) {
                    const EdgeGeometry& geometry = edgeGeometry_[h.index() / 2];
                    const bool forward = h.index() % 2 == 0;
                    halfplanes.emplace_back(
                        ResultPoint(forward ? geometry.a : geometry.b),
                        ResultPoint(forward ? geometry.b : geometry.a));
                }
                h = next(h);
            } while (h != start);
        };

        if (outerCycle_[f.index()].valid()) {
            appendCycle(outerCycle_[f.index()]);
        } else {
            for (HalfedgeId start : innerCycles(f)) {
                bool reachesInfinity = false;
                HalfedgeId h = start;
                do {
                    reachesInfinity = reachesInfinity || isUnbounded(h);
                    h = next(h);
                } while (h != start);
                if (reachesInfinity) {
                    appendCycle(start);
                }
            }
        }
        return HalfplaneIntersection<ResultPoint>(std::move(halfplanes));
    }

    // -------------------------------------------------------------------------
    // Labels and history

    /**
     * @brief Returns the label an edge inherited from the input shape that
     *        produced it.
     *
     * An edge produced by several input shapes at once — overlapping or
     * duplicated input — carries the label of the first of them, and
     * @ref originsOf lists them all.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] const TLabel& label(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        return edgeLabel_[h.index() / 2];
    }

    /** @brief Returns the mutable label of an edge. */
    [[nodiscard]] TLabel& label(HalfedgeId h) {
        assert(h.valid() && h.index() < origin_.size());
        return edgeLabel_[h.index() / 2];
    }

    /**
     * @brief Returns the label of a face.
     *
     * Faces have no label of their own to inherit — nothing in the input is a
     * face — so a face's label starts default-constructed and is there for the
     * caller to fill in, typically with whatever classification it ran per cell.
     *
     * @param f Face handle.
     */
    [[nodiscard]] const TLabel& label(FaceId f) const {
        assert(f.valid() && f.index() < faceLabel_.size());
        return faceLabel_[f.index()];
    }

    /** @brief Returns the mutable label of a face. */
    [[nodiscard]] TLabel& label(FaceId f) {
        assert(f.valid() && f.index() < faceLabel_.size());
        return faceLabel_[f.index()];
    }

    /**
     * @brief Returns the positions, in the range the arrangement was built from,
     *        of every input shape that produced an edge.
     *
     * There is more than one exactly when input shapes overlap along the edge.
     * The positions come out sorted and without repetition.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] std::span<const std::uint32_t> originsOf(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        const std::size_t edge = h.index() / 2;
        const std::size_t from = originOffset_[edge];
        const std::size_t to = originOffset_[edge + 1];
        return std::span<const std::uint32_t>(originIndex_.data() + from, to - from);
    }

    /**
     * @brief Returns the positions, in the range the arrangement was built from,
     *        of every input shape passing through a vertex.
     *
     * This is the union of @ref originsOf(HalfedgeId) over the edges incident to
     * the vertex, sorted and without repetition. An isolated vertex is incident
     * to no edge and has no origins, even when an input point put it there.
     *
     * @param v Vertex handle.
     */
    [[nodiscard]] std::vector<std::uint32_t> originsOf(VertexId v) const {
        assert(v.valid() && v.index() < outgoing_.size());
        std::vector<std::uint32_t> origins;
        const HalfedgeId start = outgoing_[v.index()];
        if (!start.valid()) {
            return origins;
        }
        HalfedgeId h = start;
        do {
            const std::span<const std::uint32_t> edgeOrigins = originsOf(h);
            origins.insert(origins.end(), edgeOrigins.begin(), edgeOrigins.end());
            h = next(twin(h));
        } while (h != start);
        std::sort(origins.begin(), origins.end());
        origins.erase(std::unique(origins.begin(), origins.end()), origins.end());
        return origins;
    }

    // -------------------------------------------------------------------------
    // Location

    /**
     * @brief Builds the randomized trapezoidal point-location index.
     *
     * The operation is idempotent. Until it is called, @ref locateFace and
     * @ref locateCell use their linear-scan implementations; afterwards they
     * use the immutable index. Copies of an indexed arrangement share the
     * index because arrangement topology cannot change through the public API.
     * The randomized insertion order is generated from a fixed seed, making
     * the resulting index deterministic across runs. Use the generator-taking
     * overload to select a different order.
     *
     * @complexity Expected `O(E log E)` time and `O(E)` space.
     */
    void buildPointLocation();

    /**
     * @brief Builds the point-location index using a caller-provided generator.
     *
     * This overload makes the randomized incremental order reproducible in
     * tests and applications that require it.
     */
    template <class UniformRandomBitGenerator>
    void buildPointLocation(UniformRandomBitGenerator&& generator);

    /** @brief Releases this arrangement's reference to its point-location index. */
    void clearPointLocation() noexcept {
        pointLocation_.reset();
    }

    /** @brief Tells whether @ref locateFace currently uses the point-location index. */
    [[nodiscard]] bool hasPointLocation() const noexcept {
        return static_cast<bool>(pointLocation_);
    }

    /**
     * @brief Returns the face containing a point.
     *
     * A point on an edge or on a vertex belongs to no face; the answer is then
     * the face that an infinitesimal displacement of the query point towards
     * `-x` (and, if that is not enough to leave the boundary, towards `+y`)
     * lands in.
     *
     * Without a point-location index the search is a linear scan over the
     * edges. After @ref buildPointLocation it uses a randomized trapezoidal
     * search DAG and takes expected logarithmic time.
     *
     * @param p Query point.
     */
    [[nodiscard]] FaceId locateFace(const PointType& p) const;

    /**
     * @brief Returns the vertex, edge, or face containing a point.
     *
     * An edge is represented by its even-numbered halfedge, independently of
     * orientation. Unlike @ref locateFace, this operation does not perturb a query
     * on the boundary into an incident face.
     */
    [[nodiscard]] CellId locateCell(const PointType& p) const;

    // -------------------------------------------------------------------------
    // Intersection traversal

    /**
     * @brief Visits the vertices and edges met by a directed curve, in order.
     *
     * The query may be a @ref Ray, @ref OrientedLine, @ref OrientedSegment,
     * @ref MonotoneChain, or @ref Polyline. Where the query meets an edge only
     * at one of its endpoints, the vertex there stands for the contact and the
     * edge is not reported for it; a straight piece meeting that same edge away
     * from its endpoints still reports the edge. An edge is represented by only
     * one of its twin halfedges, and a chain that meets the same cell more than
     * once reports it at its first encounter.
     *
     * If `fn` returns `bool`, `true` stops the traversal. A `void` visitor never
     * stops it. When the point-location index is present its vertex and curve
     * indexes supply the candidates; otherwise the arrangement arrays do.
     *
     * @return `true` if the visitor requested an early stop.
     */
    template <class Q, class Fn>
        requires (OrientedSegmentConcept<Q> || OrientedLineConcept<Q> ||
                  RayConcept<Q> || MonotoneChainConcept<Q> || PolylineConcept<Q>)
    bool visitIntersecting(const Q& r, Fn fn) const;

    /** @brief Returns every vertex and edge met by a directed curve, in order. */
    template <class Q>
        requires (OrientedSegmentConcept<Q> || OrientedLineConcept<Q> ||
                  RayConcept<Q> || MonotoneChainConcept<Q> || PolylineConcept<Q>)
    [[nodiscard]] std::vector<IntersectionId> reportIntersecting(const Q& r) const {
        std::vector<IntersectionId> result;
        visitIntersecting(r, [&](const IntersectionId& id) { result.push_back(id); });
        return result;
    }

    /** @brief Returns the first vertex or edge met by a directed curve. */
    template <class Q>
        requires (OrientedSegmentConcept<Q> || OrientedLineConcept<Q> ||
                  RayConcept<Q> || MonotoneChainConcept<Q> || PolylineConcept<Q>)
    [[nodiscard]] std::optional<IntersectionId> firstIntersecting(const Q& r) const {
        std::optional<IntersectionId> result;
        visitIntersecting(r, [&](const IntersectionId& id) {
            result = id;
            return true;
        });
        return result;
    }

    /** @brief Returns whether a directed curve meets no vertex or edge. */
    template <class Q>
        requires (OrientedSegmentConcept<Q> || OrientedLineConcept<Q> ||
                  RayConcept<Q> || MonotoneChainConcept<Q> || PolylineConcept<Q>)
    [[nodiscard]] bool emptyIntersecting(const Q& r) const {
        return !visitIntersecting(r, [](const IntersectionId&) { return true; });
    }

private:
    // Point location without the index: the face is the one left of the nearest
    // edge to the west, and a point with nothing to its west lies in the face
    // the boundary at infinity opens onto.
    [[nodiscard]] FaceId locateFaceLinear(const PointType& p) const {
        const HalfedgeId h = halfedgeLeftOf(p);
        if (h.valid()) {
            return face(h);
        }
        if (infinity_.valid()) {
            return face(infinityBoundaryAtWest(p));
        }
        return FaceId(0);
    }

    // The same without the index, but naming the cell the point lies on rather
    // than the face around it: a vertex wins over an edge, and an edge over the
    // face, since a point on the boundary belongs to the lower-dimensional cell.
    [[nodiscard]] CellId locateCellLinear(const PointType& p) const {
        for (std::uint32_t v = 0; v < points_.size(); ++v) {
            if (points_[v] == p) {
                return VertexId(v);
            }
        }
        for (std::uint32_t edge = 0; edge < edgeGeometry_.size(); ++edge) {
            const EdgeGeometry& geometry = edgeGeometry_[edge];
            if (orientationSign(geometry.a, geometry.b, p) != 0) {
                continue;
            }
            const NumberType dx = geometry.b.x() - geometry.a.x();
            const NumberType dy = geometry.b.y() - geometry.a.y();
            bool contains = geometry.kind == EdgeKind::line;
            if (geometry.kind == EdgeKind::segment) {
                contains = p.x() >= std::min(geometry.a.x(), geometry.b.x()) &&
                           p.x() <= std::max(geometry.a.x(), geometry.b.x()) &&
                           p.y() >= std::min(geometry.a.y(), geometry.b.y()) &&
                           p.y() <= std::max(geometry.a.y(), geometry.b.y());
            } else if (geometry.kind == EdgeKind::ray) {
                contains = (p.x() - geometry.a.x()) * dx +
                           (p.y() - geometry.a.y()) * dy >= NumberType(0);
            }
            if (contains) {
                return HalfedgeId(2 * edge);
            }
        }
        return locateFaceLinear(p);
    }

    // Vertices as the halfedge structure counts them, which is the geometric
    // vertices plus the single point at infinity when the arrangement has one.
    [[nodiscard]] std::size_t topologicalVertexCount() const {
        return points_.size() + (infinity_.valid() ? 1 : 0);
    }

    // How far an edge runs: a segment is bounded at both ends, a ray only at
    // the first, and a line at neither.
    enum class EdgeKind : std::uint8_t { segment, ray, line };

    // The defining coordinates of an edge, kept beside the topology so a
    // predicate never has to walk the halfedge structure to rebuild them. For a
    // ray, `a` is the source and `b` only fixes the direction; for a line the
    // two merely span it.
    struct EdgeGeometry {
        EdgeKind kind;
        PointType a;
        PointType b;
    };

    // One straight piece of a query, reported in order along the piece. The
    // seen vectors carry the cells a chain has already reported across its
    // pieces, and are null for a query that is a single piece.
    template <class Q, class Fn>
    bool visitStraightIntersecting(const Q& piece, Fn& fn,
                                   std::vector<bool>* seenVertices,
                                   std::vector<bool>* seenEdges) const;

    // A query that degenerated to a single point, which meets at most one cell.
    template <PointConcept Q, class Fn>
    bool visitPointIntersecting(const Q& point, Fn& fn,
                                std::vector<bool>* seenVertices,
                                std::vector<bool>* seenEdges) const;

    // An input segment, with the position of the shape it came from.
    struct InputSegment {
        Segment<PointType> segment;
        std::uint32_t origin;
        [[no_unique_address]] TLabel label;
    };

    // One piece of an input segment after splitting, before twin pieces of
    // overlapping input are merged into a single edge.
    struct Piece {
        PointType a;
        PointType b;
        std::uint32_t origin;
        [[no_unique_address]] TLabel label;
    };

    // A segment, ray, or line before collinear inputs are overlaid.
    struct InputCurve {
        EdgeKind kind;
        PointType a;
        PointType b;
        std::uint32_t origin;
        [[no_unique_address]] TLabel label;
    };

    // -------------------------------------------------------------------------
    // Input normalization

    // Splits a shape range into the segments it contributes and the points it
    // reduces to, keeping each shape's position in the range as its origin.
    template <class ShapeRange>
    static void collect(const ShapeRange& shapes, std::vector<InputSegment>& segments,
                        std::vector<InputCurve>& curves, std::vector<PointType>& isolated) {
        std::uint32_t index = 0;
        for (const auto& shape : shapes) {
            append(shape, index, segments, curves, isolated);
            ++index;
        }
    }

    // The shapes the arrangement can be built from directly or through Shape.
    template <class InputShape>
    static constexpr bool isSupported =
        detail::is_empty_shape_v<InputShape> || detail::is_point_v<InputShape> ||
        detail::is_segment_v<InputShape> || detail::is_oriented_segment_v<InputShape> ||
        detail::is_line_v<InputShape> || detail::is_oriented_line_v<InputShape> ||
        detail::is_ray_v<InputShape> ||
        detail::is_polyline_v<InputShape> || detail::is_monotone_chain_v<InputShape> ||
        detail::is_triangle_v<InputShape> || detail::is_rectangle_v<InputShape> ||
        detail::is_convex_v<InputShape> || detail::is_polygon_v<InputShape> ||
        detail::is_polygon_with_holes_v<InputShape>;

    // Appends the cut segments — and the isolated points — of one input shape.
    template <class InputShape>
    static void append(const InputShape& shape, std::uint32_t index,
                       std::vector<InputSegment>& segments, std::vector<InputCurve>& curves,
                       std::vector<PointType>& isolated) {
        static_assert(isSupported<InputShape> || detail::is_shape_v<InputShape>,
                      "Arrangement accepts points, segment-bounded shapes, lines, and rays");
        const auto addSegment = [&](const auto& edge) {
            const PointType a(edge[0]);
            const PointType b(edge[1]);
            if (a == b) {
                isolated.push_back(a);
                return;
            }
            InputSegment input{Segment<PointType>(a, b), index, TLabel{}};
            if constexpr (detail::has_label_v<TLabel>) {
                input.label = detail::copyLabel<TLabel>(shape);
            }
            segments.push_back(std::move(input));
            InputCurve curve{EdgeKind::segment, a < b ? a : b, a < b ? b : a, index, TLabel{}};
            if constexpr (detail::has_label_v<TLabel>) {
                curve.label = detail::copyLabel<TLabel>(shape);
            }
            curves.push_back(std::move(curve));
        };

        if constexpr (detail::is_shape_v<InputShape>) {
            std::visit([&](const auto& alternative) {
                if constexpr (isSupported<std::remove_cvref_t<decltype(alternative)>>) {
                    append(alternative, index, segments, curves, isolated);
                } else {
                    throw std::invalid_argument(
                        "Arrangement accepts only points, segment-bounded shapes, lines, and rays");
                }
            }, shape.variant());
        } else if constexpr (detail::is_empty_shape_v<InputShape>) {
            (void)shape;
        } else if constexpr (detail::is_point_v<InputShape>) {
            isolated.emplace_back(shape);
        } else if constexpr (detail::is_segment_v<InputShape> ||
                             detail::is_oriented_segment_v<InputShape>) {
            addSegment(shape);
        } else if constexpr (detail::is_line_v<InputShape> ||
                             detail::is_oriented_line_v<InputShape> ||
                             detail::is_ray_v<InputShape>) {
            const PointType a(shape[0]);
            const PointType b(shape[1]);
            if (a == b) {
                isolated.push_back(a);
                return;
            }
            constexpr EdgeKind kind = detail::is_ray_v<InputShape> ? EdgeKind::ray : EdgeKind::line;
            InputCurve curve{kind, a, b, index, TLabel{}};
            if constexpr (detail::has_label_v<TLabel>) {
                curve.label = detail::copyLabel<TLabel>(shape);
            }
            curves.push_back(std::move(curve));
        } else if constexpr (detail::is_polygon_with_holes_v<InputShape>) {
            for (const auto& edge : shape.edges()) {
                addSegment(edge);
            }
        } else if constexpr (detail::is_polyline_v<InputShape> ||
                             detail::is_monotone_chain_v<InputShape>) {
            if (shape.size() == 1) {
                isolated.emplace_back(shape[0]);
            } else {
                for (const auto& edge : shape.edgesView()) {
                    addSegment(edge);
                }
            }
        } else if constexpr (requires { shape.edgesView(); }) {
            for (const auto& edge : shape.edgesView()) {
                addSegment(edge);
            }
        } else {
            for (const auto& edge : shape.edges()) {
                addSegment(edge);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Construction

    void build(std::vector<InputSegment>& segments, std::vector<InputCurve>& curves,
               std::vector<PointType>& isolated) {
        const bool hasUnbounded = std::ranges::any_of(
            curves, [](const InputCurve& curve) { return curve.kind != EdgeKind::segment; });
        if (hasUnbounded) {
            buildUnbounded(curves, isolated);
            return;
        }
        std::vector<Piece> pieces = split(segments, isolated);
        internVertices(pieces, isolated);
        simplifyStoredCoordinates();
        wireHalfedges();
        buildFaces();
    }

    // A stretch of one carrier that some input curve covers, as a parameter
    // range along that carrier. An absent bound runs to infinity, and `origin`
    // names the input shape the stretch came from.
    struct CarrierInterval {
        std::optional<NumberType> low;
        std::optional<NumberType> high;
        std::uint32_t origin;
        [[no_unique_address]] TLabel label;
    };

    // A supporting line, with every input stretch laid over it. Collinear input
    // shares one carrier, so overlap is settled by merging intervals instead of
    // by intersecting curves. `usesX` picks the coordinate that parameterizes
    // it, which is the abscissa unless the line is vertical. `cuts` holds the
    // parameters where the carrier must be split.
    struct Carrier {
        PointType a;
        PointType b;
        bool usesX;
        std::vector<CarrierInterval> intervals;
        std::vector<NumberType> cuts;
    };

    // A piece of a carrier between consecutive cuts, so no other curve crosses
    // its interior. These are the pieces the halfedge structure is wired from,
    // and `origins` records every input shape covering the piece.
    struct AtomicCurve {
        std::uint32_t carrier;
        std::optional<NumberType> low;
        std::optional<NumberType> high;
        std::vector<std::uint32_t> origins;
        [[no_unique_address]] TLabel label;
    };

    // Where a point on a carrier falls along it. Vertical carriers are
    // parameterized by ordinate, everything else by abscissa, so the
    // parameter is always strictly monotone along the carrier.
    static NumberType parameterOf(const Carrier& carrier, const PointType& point) {
        return carrier.usesX ? point.x() : point.y();
    }

    // Whether an interval already spans the whole of another one, absent bounds
    // reaching infinity. This is what makes a repeated or contained input
    // stretch add nothing.
    static bool covers(const CarrierInterval& interval,
                       const std::optional<NumberType>& low,
                       const std::optional<NumberType>& high) {
        if (!low.has_value()) {
            if (interval.low.has_value()) {
                return false;
            }
        } else if (interval.low.has_value() && *low < *interval.low) {
            return false;
        }
        if (!high.has_value()) {
            if (interval.high.has_value()) {
                return false;
            }
        } else if (interval.high.has_value() && *interval.high < *high) {
            return false;
        }
        return true;
    }

    // Whether an interval holds a single parameter, ends included.
    static bool covers(const CarrierInterval& interval, const NumberType& value) {
        return (!interval.low.has_value() || !(value < *interval.low)) &&
               (!interval.high.has_value() || !(*interval.high < value));
    }

    // The point at a parameter along a carrier.
    static PointType pointAt(const Carrier& carrier, const NumberType& parameter) {
        PointType point = [&] {
            if (carrier.usesX) {
                const NumberType dx = carrier.b.x() - carrier.a.x();
                const NumberType y = carrier.a.y() +
                                     (parameter - carrier.a.x()) *
                                         (carrier.b.y() - carrier.a.y()) / dx;
                return PointType(parameter, y);
            }
            return PointType(carrier.a.x(), parameter);
        }();
        // A point cut out of a carrier becomes an interned vertex, so it is
        // hashed and compared the moment it is built and read for the rest of the
        // arrangement's life. Reducing the chain of arithmetic above once, here,
        // is what puts all of that on the normalized fast path.
        if constexpr (pgl::is_Rational_v<NumberType>) {
            point.x().simplify();
            point.y().simplify();
        }
        return point;
    }

    // Whether a curve is collinear with a carrier, and so belongs on it rather
    // than on one of its own.
    static bool sameCarrier(const Carrier& carrier, const InputCurve& curve) {
        return orientationSign(carrier.a, carrier.b, curve.a) == 0 &&
               orientationSign(carrier.a, carrier.b, curve.b) == 0;
    }

    // General normalization for input containing a ray or a line. Collinear
    // inputs are overlaid as intervals on one exact carrier; intersections of
    // distinct carriers then become additional interval endpoints.
    void buildUnbounded(const std::vector<InputCurve>& curves,
                        const std::vector<PointType>& isolated) {
        std::vector<Carrier> carriers;
        for (const InputCurve& curve : curves) {
            auto found = std::find_if(carriers.begin(), carriers.end(),
                                      [&](const Carrier& carrier) {
                                          return sameCarrier(carrier, curve);
                                      });
            if (found == carriers.end()) {
                PointType a = curve.a;
                PointType b = curve.b;
                if (b < a) {
                    std::swap(a, b);
                }
                carriers.push_back(Carrier{a, b, !(a.x() == b.x()), {}, {}});
                found = std::prev(carriers.end());
            }
            Carrier& carrier = *found;
            const NumberType ta = parameterOf(carrier, curve.a);
            const NumberType tb = parameterOf(carrier, curve.b);
            CarrierInterval interval{{}, {}, curve.origin, curve.label};
            if (curve.kind == EdgeKind::segment) {
                interval.low = std::min(ta, tb);
                interval.high = std::max(ta, tb);
            } else if (curve.kind == EdgeKind::ray) {
                if (ta < tb) {
                    interval.low = ta;
                } else {
                    interval.high = ta;
                }
            }
            if (interval.low.has_value()) {
                carrier.cuts.push_back(*interval.low);
            }
            if (interval.high.has_value()) {
                carrier.cuts.push_back(*interval.high);
            }
            carrier.intervals.push_back(std::move(interval));
        }

        for (Carrier& carrier : carriers) {
            for (const PointType& point : isolated) {
                if (orientationSign(carrier.a, carrier.b, point) != 0) {
                    continue;
                }
                const NumberType value = parameterOf(carrier, point);
                if (std::ranges::any_of(carrier.intervals,
                                        [&](const CarrierInterval& interval) {
                                            return covers(interval, value);
                                        })) {
                    carrier.cuts.push_back(value);
                }
            }
        }

        for (std::size_t i = 0; i < carriers.size(); ++i) {
            for (std::size_t j = i + 1; j < carriers.size(); ++j) {
                const Line<PointType> first(carriers[i].a, carriers[i].b);
                const Line<PointType> second(carriers[j].a, carriers[j].b);
                const auto intersection = first.template intersection<NumberType>(second);
                if (!intersection || !std::holds_alternative<PointType>(*intersection)) {
                    continue;
                }
                const PointType& point = std::get<PointType>(*intersection);
                const NumberType ti = parameterOf(carriers[i], point);
                const NumberType tj = parameterOf(carriers[j], point);
                const bool onFirst = std::ranges::any_of(
                    carriers[i].intervals,
                    [&](const CarrierInterval& interval) { return covers(interval, ti); });
                const bool onSecond = std::ranges::any_of(
                    carriers[j].intervals,
                    [&](const CarrierInterval& interval) { return covers(interval, tj); });
                if (onFirst && onSecond) {
                    carriers[i].cuts.push_back(ti);
                    carriers[j].cuts.push_back(tj);
                }
            }
        }

        std::vector<AtomicCurve> atoms;
        for (std::uint32_t c = 0; c < carriers.size(); ++c) {
            Carrier& carrier = carriers[c];
            // A cut coming from a carrier crossing is an unreduced fraction, and
            // it is read by the sort and unique here, by the interval tests in
            // emit, and again by pointAt once it reaches an atom. One gcd apiece
            // covers all of it; the same reasoning as split's cut lists.
            if constexpr (pgl::is_Rational_v<NumberType>) {
                for (NumberType& cut : carrier.cuts) {
                    cut.simplify();
                }
            }
            std::sort(carrier.cuts.begin(), carrier.cuts.end());
            carrier.cuts.erase(std::unique(carrier.cuts.begin(), carrier.cuts.end()),
                               carrier.cuts.end());

            const auto emit = [&](std::optional<NumberType> low,
                                  std::optional<NumberType> high) {
                const CarrierInterval* first = nullptr;
                std::vector<std::uint32_t> origins;
                for (const CarrierInterval& interval : carrier.intervals) {
                    if (!covers(interval, low, high)) {
                        continue;
                    }
                    if (first == nullptr || interval.origin < first->origin) {
                        first = &interval;
                    }
                    origins.push_back(interval.origin);
                }
                if (first == nullptr) {
                    return;
                }
                std::sort(origins.begin(), origins.end());
                origins.erase(std::unique(origins.begin(), origins.end()), origins.end());
                atoms.push_back(AtomicCurve{c, std::move(low), std::move(high),
                                            std::move(origins), first->label});
            };

            if (carrier.cuts.empty()) {
                emit({}, {});
                continue;
            }
            emit({}, carrier.cuts.front());
            for (std::size_t i = 0; i + 1 < carrier.cuts.size(); ++i) {
                if (!(carrier.cuts[i] == carrier.cuts[i + 1])) {
                    emit(carrier.cuts[i], carrier.cuts[i + 1]);
                }
            }
            emit(carrier.cuts.back(), {});
        }

        std::unordered_map<PointType, std::uint32_t> vertexOf;
        const auto idOf = [&](const PointType& point) {
            const auto found = vertexOf.find(point);
            if (found != vertexOf.end()) {
                return found->second;
            }
            const auto id = static_cast<std::uint32_t>(points_.size());
            points_.push_back(point);
            vertexOf.emplace(point, id);
            return id;
        };

        for (const AtomicCurve& atom : atoms) {
            if (atom.low.has_value()) {
                idOf(pointAt(carriers[atom.carrier], *atom.low));
            }
            if (atom.high.has_value()) {
                idOf(pointAt(carriers[atom.carrier], *atom.high));
            }
        }
        for (const PointType& point : isolated) {
            idOf(point);
        }
        infinity_ = VertexId(static_cast<std::uint32_t>(points_.size()));

        originOffset_.push_back(0);
        for (const AtomicCurve& atom : atoms) {
            const Carrier& carrier = carriers[atom.carrier];
            if (atom.low.has_value() && atom.high.has_value()) {
                const PointType a = pointAt(carrier, *atom.low);
                const PointType b = pointAt(carrier, *atom.high);
                origin_.push_back(idOf(a));
                origin_.push_back(idOf(b));
                edgeGeometry_.push_back({EdgeKind::segment, a, b});
            } else if (atom.low.has_value() || atom.high.has_value()) {
                const bool increasing = atom.low.has_value();
                const PointType sourcePoint = pointAt(
                    carrier, increasing ? *atom.low : *atom.high);
                const NumberType dx = carrier.b.x() - carrier.a.x();
                const NumberType dy = carrier.b.y() - carrier.a.y();
                const PointType directionPoint(
                    increasing ? sourcePoint.x() + dx : sourcePoint.x() - dx,
                    increasing ? sourcePoint.y() + dy : sourcePoint.y() - dy);
                origin_.push_back(idOf(sourcePoint));
                origin_.push_back(infinity_.index());
                edgeGeometry_.push_back({EdgeKind::ray, sourcePoint, directionPoint});
            } else {
                origin_.push_back(infinity_.index());
                origin_.push_back(infinity_.index());
                edgeGeometry_.push_back({EdgeKind::line, carrier.a, carrier.b});
            }
            edgeLabel_.push_back(atom.label);
            originIndex_.insert(originIndex_.end(), atom.origins.begin(), atom.origins.end());
            originOffset_.push_back(static_cast<std::uint32_t>(originIndex_.size()));
        }

        simplifyStoredCoordinates();
        next_.assign(origin_.size(), 0);
        face_.assign(origin_.size(), 0);
        outgoing_.assign(topologicalVertexCount(), HalfedgeId());
        wireHalfedgesUnbounded();
        buildFacesUnbounded();
    }

    /**
     * Splits every input segment at every point where another input segment, or
     * an isolated input point, meets it, so that the pieces meet each other only
     * at shared endpoints.
     *
     * Input segments covering the same stretch are grouped first, so a stretch
     * is split once however many shapes contributed it. The scan below is
     * quadratic and the callers produce repeats in bulk — the union of the
     * pairwise Minkowski sums arrives with about one distinct cut segment for
     * every two — so the grouping is worth its sort several times over. The
     * pieces are still emitted once per contributing shape, so
     * @ref internVertices sees the same multiset it would without it.
     *
     * Pairs are enumerated by sweeping the groups' x-projections rather than by
     * scanning all of them: the groups are visited left to right, those whose
     * projection is still open are kept in an active list, and only that list is
     * tested against. A y-extent comparison then rejects the pairs whose boxes
     * overlap in x but miss in y, before the exact intersection is constructed.
     * Every pair the sweep does test gets the same exact treatment the all-pairs
     * scan gave it, so the result is unchanged; the sweep only skips pairs that
     * cannot meet.
     *
     * On the Minkowski workload this is worth 2x at a thousand cut segments and
     * close to 4x at seventeen thousand, because the boxes are sparse: at the
     * largest shape-pair cell it cuts 143M pair tests to 3.1M. It is not a
     * complexity change — segments with widely overlapping projections, many
     * long near-horizontal ones say, degenerate to all pairs plus a sort — and
     * what is left is dominated by the exact intersections that really do meet.
     *
     * Quadratic in the worst case, then, but in the number of *distinct* input
     * segments and only over boxes that overlap. It is deliberately kept as a
     * single self-contained function so it can be swapped out without touching
     * the rest.
     */
    static std::vector<Piece> split(std::vector<InputSegment>& segments,
                                    const std::vector<PointType>& isolated) {
        using IntegralPoint = Point<std::int64_t>;
        using IntegralSegment = Segment<IntegralPoint>;
        constexpr bool mayNeedIntegralNarrowing =
            is_Rational_v<NumberType> || std::same_as<NumberType, BigInt>;

        // Equal geometry adjacent, and within a group the contributing shapes in
        // the order internVertices expects to see them.
        std::sort(segments.begin(), segments.end(),
                  [](const InputSegment& left, const InputSegment& right) {
                      if (!(left.segment == right.segment)) {
                          return left.segment < right.segment;
                      }
                      return left.origin < right.origin;
                  });
        // One entry per group plus a trailing sentinel, so group g occupies
        // segments[group[g] .. group[g + 1]).
        std::vector<std::size_t> group;
        for (std::size_t i = 0; i < segments.size(); ++i) {
            if (i == 0 || !(segments[i].segment == segments[i - 1].segment)) {
                group.push_back(i);
            }
        }
        group.push_back(segments.size());
        const std::size_t count = group.size() - 1;

        // A segment's endpoints are in lexicographic order, so min().x() is the
        // left end of its x-projection and max().x() the right one, with nothing
        // to compute. The y-extent is not ordered, hence the explicit minmax.
        std::vector<NumberType> right, low, high;
        right.reserve(count);
        low.reserve(count);
        high.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const Segment<PointType>& current = segments[group[i]].segment;
            right.push_back(current.max().x());
            const auto [lo, hi] = std::minmax(current.min().y(), current.max().y());
            low.push_back(lo);
            high.push_back(hi);
        }

        std::vector<std::uint32_t> order(count);
        for (std::size_t i = 0; i < count; ++i) {
            order[i] = static_cast<std::uint32_t>(i);
        }
        std::sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
            return segments[group[a]].segment.min().x() < segments[group[b]].segment.min().x();
        });

        std::vector<std::vector<PointType>> cuts(count);
        std::vector<std::optional<IntegralSegment>> integral;
        if constexpr (mayNeedIntegralNarrowing) {
            integral.resize(count);
        }
        for (std::size_t i = 0; i < count; ++i) {
            const Segment<PointType>& segment = segments[group[i]].segment;
            cuts[i].push_back(segment.min());
            cuts[i].push_back(segment.max());

            // A rational arrangement is commonly fed lattice segments — in
            // particular, every convex-piece Minkowski sum keeps integral
            // vertices until this overlay creates its crossings. Remember that
            // narrower spelling so the many rejected segment pairs use native
            // int64 coordinates and int128 determinants instead of promoting
            // Rational<BigInt> predicates all the way to BigInt. The exact
            // intersection is still constructed in NumberType below.
            const auto asIntegral = [](const PointType& point)
                -> std::optional<IntegralPoint> {
                const auto store = [](const auto& x, const auto& y)
                    -> std::optional<IntegralPoint> {
                    if (!detail::representableAs<std::int64_t>(x) ||
                        !detail::representableAs<std::int64_t>(y)) {
                        return std::nullopt;
                    }
                    return IntegralPoint(detail::narrowTo<std::int64_t>(x),
                                         detail::narrowTo<std::int64_t>(y));
                };

                if constexpr (is_Rational_v<NumberType>) {
                    if (!point.x().isInteger() || !point.y().isInteger()) {
                        return std::nullopt;
                    }
                    using Integer = rational_int_t<NumberType>;
                    return store(Integer(static_cast<Integer>(point.x())),
                                 Integer(static_cast<Integer>(point.y())));
                } else if constexpr (detail::extended_integral<NumberType> ||
                                     std::same_as<NumberType, BigInt>) {
                    return store(point.x(), point.y());
                } else {
                    return std::nullopt;
                }
            };
            if constexpr (mayNeedIntegralNarrowing) {
                const auto a = asIntegral(segment.min());
                const auto b = asIntegral(segment.max());
                if (a && b) {
                    integral[i].emplace(*a, *b);
                }
            }
        }

        const auto meet = [&](std::size_t a, std::size_t b) {
            const auto add = [&](const auto& piece) {
                if (!piece) {
                    return;
                }
                if (const auto* point = std::get_if<0>(&*piece)) {
                    cuts[a].emplace_back(*point);
                    cuts[b].emplace_back(*point);
                } else {
                    const auto& overlap = std::get<1>(*piece);
                    for (const auto& end : {overlap.min(), overlap.max()}) {
                        cuts[a].emplace_back(end);
                        cuts[b].emplace_back(end);
                    }
                }
            };

            if constexpr (mayNeedIntegralNarrowing) {
                if (integral[a] && integral[b]) {
                    add(integral[a]->template intersection<NumberType>(*integral[b]));
                    return;
                }
            }
            add(segments[group[a]].segment.template intersection<NumberType>(
                segments[group[b]].segment));
        };

        // The active list is compacted by the same pass that tests it, so a
        // group is dropped exactly once and expiry costs nothing beyond the
        // comparison the test needed anyway.
        std::vector<std::uint32_t> active;
        for (const std::uint32_t current : order) {
            const NumberType& left = segments[group[current]].segment.min().x();
            std::size_t write = 0;
            for (std::size_t read = 0; read < active.size(); ++read) {
                const std::uint32_t other = active[read];
                bool expired;
                bool missesInY;
                if constexpr (mayNeedIntegralNarrowing) {
                    if (integral[current] && integral[other]) {
                        const IntegralSegment& currentSegment = *integral[current];
                        const IntegralSegment& otherSegment = *integral[other];
                        expired = otherSegment.max().x() < currentSegment.min().x();
                        const auto [currentLow, currentHigh] =
                            std::minmax(currentSegment.min().y(), currentSegment.max().y());
                        const auto [otherLow, otherHigh] =
                            std::minmax(otherSegment.min().y(), otherSegment.max().y());
                        missesInY = otherHigh < currentLow || currentHigh < otherLow;
                    } else {
                        expired = right[other] < left;
                        missesInY = high[other] < low[current] || high[current] < low[other];
                    }
                } else {
                    expired = right[other] < left;
                    missesInY = high[other] < low[current] || high[current] < low[other];
                }
                if (expired) {
                    continue;  // its projection closed before this one opened
                }
                active[write++] = other;
                if (missesInY) {
                    continue;  // boxes overlap in x but miss in y
                }
                meet(other, current);
            }
            active.resize(write);
            active.push_back(current);
        }

        std::vector<Piece> pieces;
        for (std::size_t i = 0; i < count; ++i) {
            for (const PointType& point : isolated) {
                if (segments[group[i]].segment.contains(point)) {
                    cuts[i].push_back(point);
                }
            }
            // A crossing arrives from intersection() as a fraction whose
            // normalization this type defers, and every read of one reduces it
            // again and keeps nothing. Each cut is about to be read many times
            // over — by the sort and unique just below, by the piece copies they
            // feed, and by everything internVertices and the wiring passes do
            // with those. Reducing once per distinct cut, before any of that,
            // collapses all of it to a single gcd apiece.
            if constexpr (pgl::is_Rational_v<NumberType>) {
                for (PointType& cut : cuts[i]) {
                    cut.x().simplify();
                    cut.y().simplify();
                }
            }
            // Every cut lies on the segment, so the lexicographic point order is
            // the linear order along it.
            std::sort(cuts[i].begin(), cuts[i].end());
            cuts[i].erase(std::unique(cuts[i].begin(), cuts[i].end()), cuts[i].end());
            for (std::size_t k = 0; k + 1 < cuts[i].size(); ++k) {
                for (std::size_t s = group[i]; s < group[i + 1]; ++s) {
                    pieces.push_back(Piece{cuts[i][k], cuts[i][k + 1], segments[s].origin,
                                           segments[s].label});
                }
            }
        }
        return pieces;
    }

    // Turns the split pieces into vertices and edges: equal pieces — the
    // overlapping and duplicated input of the same stretch — become one edge
    // remembering every input shape that produced it.
    void internVertices(std::vector<Piece>& pieces, const std::vector<PointType>& isolated) {
        // The piece endpoints were reduced by @ref split, before the cuts were
        // copied into pieces, so the sort and the hash lookups below already read
        // them on the normalized fast path.
        std::sort(pieces.begin(), pieces.end(), [](const Piece& left, const Piece& right) {
            if (!(left.a == right.a)) {
                return left.a < right.a;
            }
            if (!(left.b == right.b)) {
                return left.b < right.b;
            }
            return left.origin < right.origin;
        });

        std::unordered_map<PointType, std::uint32_t> vertexOf;
        const auto idOf = [&](const PointType& point) {
            const auto found = vertexOf.find(point);
            if (found != vertexOf.end()) {
                return found->second;
            }
            const auto id = static_cast<std::uint32_t>(points_.size());
            points_.push_back(point);
            vertexOf.emplace(point, id);
            return id;
        };

        originOffset_.push_back(0);
        for (std::size_t i = 0; i < pieces.size();) {
            origin_.push_back(idOf(pieces[i].a));
            origin_.push_back(idOf(pieces[i].b));
            edgeGeometry_.push_back({EdgeKind::segment, pieces[i].a, pieces[i].b});
            edgeLabel_.push_back(pieces[i].label);
            // The pieces of one stretch are adjacent and ordered by their input
            // position, so the same shape is caught by looking one back only.
            std::size_t j = i;
            while (j < pieces.size() && pieces[j].a == pieces[i].a && pieces[j].b == pieces[i].b) {
                if (j == i || pieces[j].origin != pieces[j - 1].origin) {
                    originIndex_.push_back(pieces[j].origin);
                }
                ++j;
            }
            originOffset_.push_back(static_cast<std::uint32_t>(originIndex_.size()));
            i = j;
        }

        for (const PointType& point : isolated) {
            idOf(point);
        }
        next_.assign(origin_.size(), 0);
        face_.assign(origin_.size(), 0);
        outgoing_.assign(points_.size(), HalfedgeId());
    }

    // Reduces every rational coordinate the arrangement keeps — the interned
    // vertex positions and the defining geometry of each edge — to lowest terms,
    // once, before anything reads them.
    //
    // Two kinds of coordinate arrive here unreduced. A constructed one (a
    // crossing, a point cut out of a carrier) is a fraction whose normalization
    // this type defers, and an input one may have been handed to us unreduced by
    // the caller. Either way a stored coordinate is read over and over — by the
    // wiring and face passes immediately below, by point location, and by every
    // query the caller later makes — and each of those reads recomputes the same
    // gcd and throws it away. This spends one gcd per coordinate and keeps it, so
    // all of that lands on the already-normalized fast path. On a value already
    // in lowest terms it is a flag test.
    //
    // This is the belt to @ref split's braces: it covers the coordinates that
    // never passed through a cut list — isolated input points, the source and
    // direction of a ray, the two points defining a line — and costs nothing on
    // the ones that did.
    void simplifyStoredCoordinates() {
        if constexpr (pgl::is_Rational_v<NumberType>) {
            const auto reduce = [](PointType& point) {
                point.x().simplify();
                point.y().simplify();
            };
            for (PointType& point : points_) {
                reduce(point);
            }
            for (EdgeGeometry& geometry : edgeGeometry_) {
                reduce(geometry.a);
                reduce(geometry.b);
            }
        }
    }

    // Sorts the halfedges leaving each vertex counterclockwise and links them:
    // arriving at a vertex along one edge, the boundary of the face on the left
    // leaves along the next edge clockwise, which is the previous one in
    // counterclockwise order.
    void wireHalfedges() {
        std::vector<std::vector<std::uint32_t>> fan(points_.size());
        for (std::uint32_t h = 0; h < origin_.size(); ++h) {
            fan[origin_[h]].push_back(h);
        }
        for (std::uint32_t v = 0; v < fan.size(); ++v) {
            std::vector<std::uint32_t>& around = fan[v];
            if (around.empty()) {
                continue;
            }
            const PointType& center = points_[v];
            // Half 0 holds the directions with an angle in [0, pi), half 1 the
            // rest, so the comparison never needs an angle, only a sign.
            const auto half = [&](std::uint32_t h) {
                const PointType& to = points_[origin_[h ^ 1]];
                if (to.y() > center.y()) {
                    return 0;
                }
                if (to.y() < center.y()) {
                    return 1;
                }
                return to.x() > center.x() ? 0 : 1;
            };
            std::sort(around.begin(), around.end(), [&](std::uint32_t left, std::uint32_t right) {
                const int leftHalf = half(left);
                const int rightHalf = half(right);
                if (leftHalf != rightHalf) {
                    return leftHalf < rightHalf;
                }
                return orientationSign(center, points_[origin_[left ^ 1]],
                                       points_[origin_[right ^ 1]]) > 0;
            });
            const std::size_t degree = around.size();
            for (std::size_t i = 0; i < degree; ++i) {
                next_[around[i] ^ 1] = around[(i + degree - 1) % degree];
            }
            outgoing_[v] = HalfedgeId(around.front());
        }
    }

    // The direction a halfedge leaves its origin in, with the point that fixes
    // where its carrier sits. Ordering these is what puts the halfedges around
    // a vertex in rotational order.
    struct FanDirection {
        NumberType dx;
        NumberType dy;
        PointType anchor;
        std::uint32_t halfedge;
    };

    // The direction of a halfedge, taken from the interned endpoints when the
    // edge is a segment so a split edge leaves along its own piece rather than
    // along the whole original. A line's two halfedges escape opposite ways.
    [[nodiscard]] FanDirection fanDirection(std::uint32_t h) const {
        const EdgeGeometry& geometry = edgeGeometry_[h / 2];
        NumberType dx = geometry.b.x() - geometry.a.x();
        NumberType dy = geometry.b.y() - geometry.a.y();
        if (geometry.kind == EdgeKind::segment) {
            const PointType& from = points_[origin_[h]];
            const PointType& to = points_[origin_[h ^ 1]];
            dx = to.x() - from.x();
            dy = to.y() - from.y();
        } else if (geometry.kind == EdgeKind::line && h % 2 == 0) {
            dx = -dx;
            dy = -dy;
        }
        return {dx, dy, geometry.a, h};
    }

    // Which half-turn a direction falls in, so the angular comparison below can
    // use a cross product without ever spanning more than half a turn. Due east
    // counts as the upper half and due west as the lower, which puts the cut
    // just below the positive x axis.
    static int directionHalf(const FanDirection& direction) {
        if (direction.dy > NumberType(0)) {
            return 0;
        }
        if (direction.dy < NumberType(0)) {
            return 1;
        }
        return direction.dx > NumberType(0) ? 0 : 1;
    }

    // Counterclockwise order of two directions around a shared origin, starting
    // from the cut `directionHalf` sets. Exact throughout: the angle itself is
    // never formed, only compared.
    static bool fanLess(const FanDirection& left, const FanDirection& right) {
        const int leftHalf = directionHalf(left);
        const int rightHalf = directionHalf(right);
        if (leftHalf != rightHalf) {
            return leftHalf < rightHalf;
        }
        using Vector = Point<NumberType>;
        const Vector leftDirection(left.dx, left.dy);
        const auto cross = crossSign(leftDirection, Vector(right.dx, right.dy));
        if (cross != 0) {
            return cross > 0;
        }
        // Parallel ends with the same escape direction meet only at infinity.
        // Their transverse order is the order of their carriers there; it
        // reverses automatically at the opposite end of a line.
        const auto offset = crossSign(
            leftDirection, Vector(right.anchor.x() - left.anchor.x(),
                                  right.anchor.y() - left.anchor.y()));
        if (offset != 0) {
            return offset > 0;
        }
        return left.halfedge < right.halfedge;
    }

    // The same order seen from the point at infinity, where the plane is
    // traversed the other way round and every end is entered rather than left.
    static bool infinityFanLess(const FanDirection& left, const FanDirection& right) {
        return fanLess(right, left);
    }

    // Links the halfedges into face cycles for an arrangement carrying rays or
    // lines. Sorting each vertex's fan rotationally makes the next halfedge of
    // a cycle the one just clockwise of the twin, which is what walks a face
    // boundary; the vertex at infinity uses the reversed order.
    void wireHalfedgesUnbounded() {
        std::vector<std::vector<std::uint32_t>> fan(topologicalVertexCount());
        for (std::uint32_t h = 0; h < origin_.size(); ++h) {
            fan[origin_[h]].push_back(h);
        }
        for (std::uint32_t v = 0; v < fan.size(); ++v) {
            std::vector<std::uint32_t>& around = fan[v];
            if (around.empty()) {
                continue;
            }
            const bool atInfinity = infinity_.valid() && v == infinity_.index();
            std::sort(around.begin(), around.end(), [&](std::uint32_t left, std::uint32_t right) {
                return atInfinity
                    ? infinityFanLess(fanDirection(left), fanDirection(right))
                    : fanLess(fanDirection(left), fanDirection(right));
            });
            const std::size_t degree = around.size();
            for (std::size_t i = 0; i < degree; ++i) {
                next_[around[i] ^ 1] = around[(i + degree - 1) % degree];
            }
            outgoing_[v] = HalfedgeId(around.front());
            if (infinity_.valid() && v == infinity_.index()) {
                infinityFan_ = around;
            }
        }
    }

    // The unbounded face a point with no edge to its west lies in, found by
    // placing a due-west ray from the point into the fan at infinity.
    [[nodiscard]] HalfedgeId infinityBoundaryAtWest(const PointType& point) const {
        assert(!infinityFan_.empty());
        // At an end exactly collinear with the query ray, choose the sector on
        // its +y side. This is the same symbolic perturbation halfedgeLeftOf
        // uses at finite vertices.
        const FanDirection query{NumberType(-1), NumberType(0), point, 0};
        const auto after = std::upper_bound(
            infinityFan_.begin(), infinityFan_.end(), query,
            [&](const FanDirection& value, std::uint32_t halfedge) {
                return infinityFanLess(value, fanDirection(halfedge));
            });
        const std::uint32_t nextDirection =
            after == infinityFan_.end() ? infinityFan_.front() : *after;
        return HalfedgeId(nextDirection ^ 1);
    }

    // Turns the halfedge cycles into faces for an arrangement carrying rays or
    // lines. A cycle passing through infinity bounds an unbounded face, and the
    // rest nest as holes the way the bounded builder handles them.
    void buildFacesUnbounded() {
        constexpr std::uint32_t none = ~std::uint32_t{};
        const std::uint32_t halfedges = static_cast<std::uint32_t>(origin_.size());
        std::vector<std::uint32_t> cycleOf(halfedges, none);
        std::vector<std::uint32_t> representative;
        std::vector<bool> reachesInfinity;
        for (std::uint32_t h = 0; h < halfedges; ++h) {
            if (cycleOf[h] != none) {
                continue;
            }
            const auto id = static_cast<std::uint32_t>(representative.size());
            representative.push_back(h);
            bool unbounded = false;
            std::uint32_t walk = h;
            do {
                cycleOf[walk] = id;
                unbounded = unbounded || origin_[walk] == infinity_.index();
                walk = next_[walk];
            } while (walk != h);
            reachesInfinity.push_back(unbounded);
        }

        const auto cycles = static_cast<std::uint32_t>(representative.size());
        std::vector<std::uint32_t> parent(cycles);
        for (std::uint32_t i = 0; i < cycles; ++i) {
            parent[i] = i;
        }
        const auto root = [&parent](std::uint32_t x) {
            while (parent[x] != x) {
                parent[x] = parent[parent[x]];
                x = parent[x];
            }
            return x;
        };

        std::vector<bool> isOuter(cycles, false);
        for (std::uint32_t id = 0; id < cycles; ++id) {
            if (reachesInfinity[id]) {
                continue;
            }
            const std::uint32_t leftmost = leftmostVertexOf(representative[id]);
            if (turnsLeftEverywhereAt(representative[id], leftmost)) {
                isOuter[id] = true;
                continue;
            }
            const HalfedgeId left = halfedgeLeftOf(points_[leftmost]);
            const HalfedgeId other = left.valid() ? left : infinityBoundaryAtWest(points_[leftmost]);
            parent[root(id)] = root(cycleOf[other.index()]);
        }

        std::vector<std::uint32_t> infinityCycles;
        infinityCycles.reserve(cycles);
        for (std::uint32_t id = 0; id < cycles; ++id) {
            if (reachesInfinity[id]) {
                infinityCycles.push_back(id);
            }
        }
        const PointType zero(NumberType(0), NumberType(0));
        const std::uint32_t westCycle = cycleOf[infinityBoundaryAtWest(zero).index()];
        const auto west = std::find(infinityCycles.begin(), infinityCycles.end(), westCycle);
        if (west != infinityCycles.end()) {
            std::iter_swap(infinityCycles.begin(), west);
        }

        std::vector<std::uint32_t> faceOfComponent(cycles, none);
        outerCycle_.clear();
        unboundedFace_.clear();
        for (const std::uint32_t id : infinityCycles) {
            const std::uint32_t component = root(id);
            if (faceOfComponent[component] != none) {
                continue;
            }
            faceOfComponent[component] = static_cast<std::uint32_t>(outerCycle_.size());
            outerCycle_.push_back(HalfedgeId());
            unboundedFace_.push_back(true);
        }
        for (std::uint32_t id = 0; id < cycles; ++id) {
            if (!isOuter[id]) {
                continue;
            }
            const std::uint32_t component = root(id);
            assert(faceOfComponent[component] == none);
            faceOfComponent[component] = static_cast<std::uint32_t>(outerCycle_.size());
            outerCycle_.push_back(HalfedgeId(representative[id]));
            unboundedFace_.push_back(false);
        }

        std::vector<std::vector<HalfedgeId>> inner(outerCycle_.size());
        for (std::uint32_t id = 0; id < cycles; ++id) {
            const std::uint32_t f = faceOfComponent[root(id)];
            assert(f != none);
            if (!isOuter[id]) {
                inner[f].push_back(HalfedgeId(representative[id]));
            }
            std::uint32_t walk = representative[id];
            do {
                face_[walk] = f;
                walk = next_[walk];
            } while (walk != representative[id]);
        }

        innerCycle_.clear();
        innerOffset_.assign(1, 0);
        for (const std::vector<HalfedgeId>& cyclesOfFace : inner) {
            innerCycle_.insert(innerCycle_.end(), cyclesOfFace.begin(), cyclesOfFace.end());
            innerOffset_.push_back(static_cast<std::uint32_t>(innerCycle_.size()));
        }
        faceLabel_.assign(outerCycle_.size(), TLabel{});
    }

    // Traces the `next` cycles, tells the outer boundary cycles (counterclockwise)
    // from the inner ones (clockwise, or degenerate), and gathers the cycles that
    // bound the same face.
    void buildFaces() {
        constexpr std::uint32_t none = ~std::uint32_t{};
        const std::uint32_t halfedges = static_cast<std::uint32_t>(origin_.size());

        std::vector<std::uint32_t> cycleOf(halfedges, none);
        std::vector<std::uint32_t> representative;
        for (std::uint32_t h = 0; h < halfedges; ++h) {
            if (cycleOf[h] != none) {
                continue;
            }
            const auto id = static_cast<std::uint32_t>(representative.size());
            representative.push_back(h);
            std::uint32_t walk = h;
            do {
                cycleOf[walk] = id;
                walk = next_[walk];
            } while (walk != h);
        }

        // Cycle `n` stands for the outside of everything: an inner cycle with no
        // edge to its left bounds the unbounded face.
        const auto cycles = static_cast<std::uint32_t>(representative.size());
        std::vector<std::uint32_t> parent(cycles + 1);
        for (std::uint32_t i = 0; i < parent.size(); ++i) {
            parent[i] = i;
        }
        const auto root = [&parent](std::uint32_t x) {
            while (parent[x] != x) {
                parent[x] = parent[parent[x]];
                x = parent[x];
            }
            return x;
        };

        // Which cycles are outer boundaries, and, for the inner ones, the vertex
        // each of them has to ask what holds it from. The questions are collected
        // rather than asked one by one: one sweep answers the whole batch, where
        // a horizontal ray per cycle would scan every edge again for each.
        std::vector<bool> isOuter(cycles, false);
        std::vector<std::uint32_t> asking;
        std::vector<std::uint32_t> askedFrom;
        for (std::uint32_t id = 0; id < cycles; ++id) {
            const std::uint32_t leftmost = leftmostVertexOf(representative[id]);
            if (turnsLeftEverywhereAt(representative[id], leftmost)) {
                isOuter[id] = true;
                continue;
            }
            asking.push_back(id);
            askedFrom.push_back(leftmost);
        }
        // The leftmost vertex of an inner cycle has the face this cycle bounds
        // immediately to its left, so whatever edge is there bounds the same
        // face. Merging them all afterwards builds the same partition merging
        // them one at a time would.
        const std::vector<HalfedgeId> toTheLeft = halfedgesLeftOf(askedFrom);
        for (std::size_t i = 0; i < asking.size(); ++i) {
            const std::uint32_t other =
                toTheLeft[i].valid() ? cycleOf[toTheLeft[i].index()] : cycles;
            parent[root(asking[i])] = root(other);
        }

        // The unbounded face comes first, then one face per outer cycle.
        std::vector<std::uint32_t> faceOfComponent(cycles + 1, none);
        faceOfComponent[root(cycles)] = 0;
        outerCycle_.assign(1, HalfedgeId());
        for (std::uint32_t id = 0; id < cycles; ++id) {
            if (!isOuter[id]) {
                continue;
            }
            // A component holds at most one counterclockwise cycle: it is the
            // face's outer boundary, and the rest of the component is its holes.
            assert(faceOfComponent[root(id)] == none);
            faceOfComponent[root(id)] = static_cast<std::uint32_t>(outerCycle_.size());
            outerCycle_.push_back(HalfedgeId(representative[id]));
        }

        std::vector<std::vector<HalfedgeId>> inner(outerCycle_.size());
        for (std::uint32_t id = 0; id < cycles; ++id) {
            const std::uint32_t f = faceOfComponent[root(id)];
            assert(f != none);
            if (!isOuter[id]) {
                inner[f].push_back(HalfedgeId(representative[id]));
            }
            std::uint32_t walk = representative[id];
            do {
                face_[walk] = f;
                walk = next_[walk];
            } while (walk != representative[id]);
        }

        innerOffset_.assign(1, 0);
        for (const std::vector<HalfedgeId>& cyclesOfFace : inner) {
            innerCycle_.insert(innerCycle_.end(), cyclesOfFace.begin(), cyclesOfFace.end());
            innerOffset_.push_back(static_cast<std::uint32_t>(innerCycle_.size()));
        }
        faceLabel_.assign(outerCycle_.size(), TLabel{});
        unboundedFace_.assign(outerCycle_.size(), false);
        unboundedFace_.front() = true;
    }

    // -------------------------------------------------------------------------
    // Geometry helpers

    /**
     * Returns the halfedge of the nearest edge strictly to the left of @p p,
     * directed downwards, so that the face to its left is the one holding the
     * stretch between the edge and @p p.
     *
     * The horizontal ray is taken infinitesimally above @p p — an endpoint at
     * exactly the query height counts as being below it — so it meets no vertex
     * and every crossing is transversal, and it starts infinitesimally to the
     * left of @p p, so the edges through @p p itself do not stop it.
     */
    [[nodiscard]] HalfedgeId halfedgeLeftOf(const PointType& p) const {
        HalfedgeId best;
        NumberType bestNumerator(0);
        NumberType bestDenominator(0);
        NumberType bestUpX(0);
        NumberType bestUpY(1);
        for (std::uint32_t h = 0; h < origin_.size(); h += 2) {
            const EdgeGeometry& geometry = edgeGeometry_[h / 2];
            const PointType& a = geometry.a;
            const PointType& b = geometry.b;
            NumberType dx = b.x() - a.x();
            NumberType dy = b.y() - a.y();
            if (dy == NumberType(0)) {
                continue;
            }
            if (geometry.kind == EdgeKind::segment) {
                const NumberType lowY = std::min(a.y(), b.y());
                const NumberType highY = std::max(a.y(), b.y());
                if (p.y() < lowY || !(p.y() < highY)) {
                    continue;
                }
            } else if (geometry.kind == EdgeKind::ray) {
                if ((dy > NumberType(0) && p.y() < a.y()) ||
                    (dy < NumberType(0) && !(p.y() < a.y()))) {
                    continue;
                }
            }
            // The crossing abscissa as the fraction numerator / denominator,
            // with a positive denominator; no division, so integer coordinates
            // stay exact.
            NumberType denominator = dy;
            NumberType numerator = a.x() * dy + (p.y() - a.y()) * dx;
            if (denominator < NumberType(0)) {
                denominator = -denominator;
                numerator = -numerator;
            }
            if (!(numerator < p.x() * denominator)) {
                continue;  // to the right of the query point, or through it
            }
            const NumberType upX = dy > NumberType(0) ? dx : -dx;
            const NumberType upY = dy > NumberType(0) ? dy : -dy;
            if (best.valid()) {
                const NumberType here = numerator * bestDenominator;
                const NumberType there = bestNumerator * denominator;
                if (here < there) {
                    continue;
                }
                if (here == there) {
                    if (!(crossSign(Point<NumberType>(upX, upY),
                                    Point<NumberType>(bestUpX, bestUpY)) > 0)) {
                        continue;
                    }
                }
            }
            // The halfedge running downwards has the crossing's right-hand side,
            // where the query point lies, on its left.
            best = HalfedgeId(dy < NumberType(0) ? h : h + 1);
            bestNumerator = numerator;
            bestDenominator = denominator;
            bestUpX = upX;
            bestUpY = upY;
        }
        return best;
    }

    /**
     * Orders the edges that a horizontal line crosses by where they cross it,
     * left to right. An edge is named by its downward halfedge, so the low
     * endpoint of `d` is `origin_[d ^ 1]` and the high one `origin_[d]`, and the
     * key is already the answer @ref halfedgesLeftOf has to report. A
     * @ref PointType compares as the point it is, which is what lets the
     * structure be searched for a query point without a fake edge.
     *
     * Nothing here mentions the height of the line, and that is the point. Two
     * edges that cross one horizontal line keep their order for as long as both
     * cross it — the edges of an arrangement meet at their endpoints alone — so
     * the order can be read off the endpoints once instead of being recomputed
     * from a crossing abscissa at every comparison. Take the two low endpoints
     * and pivot on the higher of them: it lies within the other edge's height
     * range, so which side of that edge it falls on *is* the order, and one
     * orientation sign settles it. Sitting on the other edge is not a third
     * case: a vertex interior to an edge would have split it, so the pivot can
     * only be the other edge's own low endpoint, and two edges leaving one
     * vertex upwards are ordered by which leans further right.
     *
     * Both cheaper and more robust than comparing crossing abscissas: one
     * orientation determinant of coordinate differences against six products of
     * whole coordinates, and no dependence on where the line currently is.
     *
     * The higher low endpoint is found by comparing positions in the sweep
     * order, which is an integer comparison, and equal positions are the same
     * vertex — so the pivot choice and the shared-endpoint case cost no
     * arithmetic at all.
     */
    struct SweepOrder {
        using is_transparent = void;
        const Arrangement* arrangement;
        const std::vector<std::uint32_t>* position;

        bool operator()(std::uint32_t left, std::uint32_t right) const {
            const std::vector<std::uint32_t>& origin = arrangement->origin_;
            const std::vector<PointType>& points = arrangement->points_;
            const std::uint32_t leftLow = origin[left ^ 1];
            const std::uint32_t rightLow = origin[right ^ 1];
            if (leftLow == rightLow) {
                // One vertex, both leaving it upwards: the one leaning further
                // right crosses the line further right.
                return orientationSign(points[leftLow], points[origin[left]],
                                       points[origin[right]]) < 0;
            }
            if ((*position)[leftLow] > (*position)[rightLow]) {
                return orientationSign(points[rightLow], points[origin[right]],
                                       points[leftLow]) > 0;
            }
            return orientationSign(points[leftLow], points[origin[left]], points[rightLow]) < 0;
        }

        // The edge is strictly left of the point when the point is strictly to
        // the right of it. A point *on* the edge is not, which is what keeps the
        // edges through a query vertex from answering it.
        bool operator()(std::uint32_t left, const PointType& p) const {
            return orientationSign(arrangement->points_[arrangement->origin_[left ^ 1]],
                                   arrangement->points_[arrangement->origin_[left]], p) < 0;
        }

        bool operator()(const PointType& p, std::uint32_t right) const {
            return orientationSign(arrangement->points_[arrangement->origin_[right ^ 1]],
                                   arrangement->points_[arrangement->origin_[right]], p) > 0;
        }
    };

    /**
     * Returns, for each of @p queries, what @ref halfedgeLeftOf would answer at
     * that vertex, by whichever of the two ways of answering them is cheaper.
     *
     * A scan per query costs `O(Q E)`: one straddle test per query and edge, and
     * that test is about as cheap as an exact test on a pair of coordinates gets.
     * @ref sweepHalfedgesLeftOf costs `O((E + Q) log E)`, but pays an orientation
     * predicate and a tree node per comparison, some five times the straddle test
     * on the same coordinates. So the scan wins whenever the queries are few —
     * connected input asks only a handful, and the arrangements that are large
     * are usually connected — and loses by any margin one likes as they grow:
     * scattered input asks one per component, which is where the `Q E` term was
     * the whole cost of construction.
     *
     * Both counts are known before either runs, so this is a choice and not a
     * bail-out — and the choice is what keeps the `Q E` term out of the bound.
     * The scan runs only when `Q E` is below the sweep's estimate, so it makes
     * at most that many straddle tests, and a straddle test and a sweep
     * comparison are both one exact predicate: whichever branch runs, the batch
     * costs `O((E + Q) log E)` predicate evaluations. The constant relating the
     * two is all that calibration decides; it moves where the crossover sits and
     * how tight the bound is, never its shape.
     */
    [[nodiscard]] std::vector<HalfedgeId> halfedgesLeftOf(
        const std::vector<std::uint32_t>& queries) const {
        const std::uint64_t edges = origin_.size() / 2;
        const std::uint64_t asked = queries.size();
        // log2 of the number of edges, near enough, and without a cast to double.
        const std::uint64_t depth = std::bit_width(edges);
        constexpr std::uint64_t perComparison = 5;
        if (!infinity_.valid() &&
            asked * edges > perComparison * (2 * edges + asked) * depth) {
            return sweepHalfedgesLeftOf(queries);
        }
        std::vector<HalfedgeId> answer;
        answer.reserve(queries.size());
        for (const std::uint32_t query : queries) {
            answer.push_back(halfedgeLeftOf(points_[query]));
        }
        return answer;
    }

    /**
     * Returns, for each of @p queries, what @ref halfedgeLeftOf would answer at
     * that vertex — the whole batch in `O((E + Q) log E)` rather than one linear
     * scan over the edges per query.
     *
     * One sweep by a horizontal line moving upwards. The line's status holds the
     * edges it currently crosses, ordered by @ref SweepOrder, and a query is the
     * predecessor of its own vertex in that order. What makes this a plain sweep
     * rather than a second Bentley–Ottmann is that the edges are already split:
     * they meet at their endpoints alone, so no two of them ever swap along the
     * line and the only events are the endpoints themselves.
     *
     * The vertices are visited by increasing height and, within one height, from
     * left to right; at each of them the edges ending there leave the status
     * first, then those starting there join it, then the queries are answered.
     * That reproduces @ref halfedgeLeftOf exactly, ray included: an edge whose
     * top is the query height has already left when the query is asked — the ray
     * passes infinitesimally above it — and one whose bottom is that height has
     * already joined. Within a height, everything at or left of a query vertex
     * has been dealt with before the query, and everything to its right sits
     * further right along the line than the query does, so it cannot be the
     * predecessor either.
     */
    [[nodiscard]] std::vector<HalfedgeId> sweepHalfedgesLeftOf(
        const std::vector<std::uint32_t>& queries) const {
        // Vertices by height, then abscissa. A vertex is its position here, so
        // the events sort on an integer and never on a coordinate.
        std::vector<std::uint32_t> byHeight(points_.size());
        for (std::uint32_t v = 0; v < byHeight.size(); ++v) {
            byHeight[v] = v;
        }
        std::sort(byHeight.begin(), byHeight.end(), [this](std::uint32_t a, std::uint32_t b) {
            if (!(points_[a].y() == points_[b].y())) {
                return points_[a].y() < points_[b].y();
            }
            return points_[a].x() < points_[b].x();
        });
        std::vector<std::uint32_t> position(points_.size());
        for (std::uint32_t i = 0; i < byHeight.size(); ++i) {
            position[byHeight[i]] = i;
        }

        // What happens at a vertex, in the order it has to happen.
        enum Phase : std::uint8_t { leaves = 0, joins = 1, asks = 2 };
        struct Event {
            std::uint32_t at;       // the vertex, as its position in `byHeight`
            std::uint32_t subject;  // a downward halfedge, or a query's position
            Phase phase;
        };
        std::vector<Event> events;
        events.reserve(origin_.size() + queries.size());
        for (std::uint32_t h = 0; h < origin_.size(); h += 2) {
            if (points_[origin_[h]].y() == points_[origin_[h + 1]].y()) {
                continue;  // horizontal: it crosses no horizontal line
            }
            const std::uint32_t downward =
                points_[origin_[h]].y() > points_[origin_[h + 1]].y() ? h : h + 1;
            events.push_back({position[origin_[downward]], downward, leaves});
            events.push_back({position[origin_[downward ^ 1]], downward, joins});
        }
        for (std::uint32_t q = 0; q < queries.size(); ++q) {
            events.push_back({position[queries[q]], q, asks});
        }
        std::sort(events.begin(), events.end(), [](const Event& left, const Event& right) {
            if (left.at != right.at) {
                return left.at < right.at;
            }
            return left.phase < right.phase;
        });

        std::vector<HalfedgeId> answer(queries.size());
        using Status = std::set<std::uint32_t, SweepOrder>;
        Status line(SweepOrder{this, &position});
        // Erasing by key would compare an edge that ends on the line against the
        // edges still crossing it, and an edge that has reached its top no longer
        // has a side of it to be on. Every edge remembers where it sits instead.
        std::vector<typename Status::iterator> seat(origin_.size() / 2);
        for (const Event& event : events) {
            if (event.phase == leaves) {
                line.erase(seat[event.subject / 2]);
            } else if (event.phase == joins) {
                const auto placed = line.insert(event.subject);
                assert(placed.second);
                seat[event.subject / 2] = placed.first;
            } else {
                const auto above = line.lower_bound(points_[queries[event.subject]]);
                if (above != line.begin()) {
                    answer[event.subject] = HalfedgeId(*std::prev(above));
                }
            }
        }
        return answer;
    }

    // Calls `fn` on every halfedge bounding the face, outer cycle first.
    template <class Function>
    void forEachBoundaryHalfedge(FaceId f, const Function& fn) const {
        const auto walkCycle = [&](HalfedgeId start) {
            std::uint32_t h = start.index();
            do {
                fn(HalfedgeId(h));
                h = next_[h];
            } while (h != start.index());
        };
        if (outerCycle_[f.index()].valid()) {
            walkCycle(outerCycle_[f.index()]);
        }
        for (HalfedgeId inner : innerCycles(f)) {
            walkCycle(inner);
        }
    }

    // The lexicographically smallest vertex a boundary cycle visits.
    [[nodiscard]] std::uint32_t leftmostVertexOf(std::uint32_t start) const {
        std::uint32_t leftmost = origin_[start];
        for (std::uint32_t h = next_[start]; h != start; h = next_[h]) {
            if (points_[origin_[h]] < points_[leftmost]) {
                leftmost = origin_[h];
            }
        }
        return leftmost;
    }

    /**
     * Tells whether a boundary cycle turns left every time it passes its
     * leftmost vertex — which is what distinguishes an outer boundary cycle,
     * running counterclockwise, from an inner one.
     *
     * The textbook test looks at that vertex once, on the grounds that the face
     * fills a convex corner there exactly when the cycle encloses it. Taking
     * *every* visit is what makes it survive this library's input: a cycle that
     * pinches shut at its leftmost vertex passes it more than once, and a cycle
     * whose leftmost vertex is the free end of a dangling edge turns through no
     * angle at all there. Both are inner cycles, and both are caught by asking
     * for a strict left turn every time.
     *
     * The argument for the common case: every vertex of the cycle is
     * lexicographically at least the leftmost one, so both edges at a visit
     * point into the closed right half-plane, and the face fills the sector
     * swept counterclockwise from the outgoing edge to the incoming one. A left
     * turn makes that sector narrower than a half-turn, so it stays to the right
     * of the vertex; a right turn opens it past a half-turn, so the face
     * reaches around to the left — which an outer cycle, enclosing its face,
     * cannot allow.
     *
     * This replaces summing the signed area, which is the same answer for one
     * exact multiplication per cycle instead of two per edge — the difference
     * between an arrangement whose face discovery is free and one where it costs
     * more than the sweep it is built on.
     */
    [[nodiscard]] bool turnsLeftEverywhereAt(std::uint32_t start, std::uint32_t vertex) const {
        std::uint32_t h = start;
        do {
            // (h, ahead) is the pair of halfedges meeting at the vertex between
            // them, so their origins are the previous and the next vertex.
            const std::uint32_t ahead = next_[h];
            if (origin_[ahead] == vertex &&
                !(orientationSign(points_[vertex], points_[origin_[ahead ^ 1]],
                                  points_[origin_[h]]) > 0)) {
                return false;
            }
            h = ahead;
        } while (h != start);
        return true;
    }

    // The vertices of a boundary cycle, in order.
    [[nodiscard]] std::vector<PointType> cycleRing(HalfedgeId start) const {
        std::vector<PointType> ring;
        std::uint32_t h = start.index();
        do {
            ring.push_back(points_[origin_[h]]);
            h = next_[h];
        } while (h != start.index());
        return ring;
    }

    // Turns one boundary cycle into the polygons that describe the same area:
    // the spikes a dangling edge leaves behind are dropped, a cycle pinching
    // shut at a vertex is cut there, and every ring comes out counterclockwise
    // and in canonical form.
    static void collectRings(std::vector<PointType> walk, std::vector<Polygon<PointType>>& out) {
        pruneSpikes(walk);
        if (walk.size() < 3) {
            return;
        }
        std::vector<std::vector<PointType>> rings;
        detail::splitWalkIntoRings(walk, rings);
        for (std::vector<PointType>& ring : rings) {
            pruneSpikes(ring);
            if (ring.size() < 3) {
                continue;
            }
            const int orientation = detail::ringOrientation(ring);
            if (orientation == 0) {
                continue;
            }
            if (orientation < 0) {
                std::reverse(ring.begin(), ring.end());
            }
            // Counterclockwise and rotated onto its smallest vertex is exactly
            // the canonical form, so the polygon needs no normalization.
            std::rotate(ring.begin(), std::min_element(ring.begin(), ring.end()), ring.end());
            out.emplace_back(std::move(ring), /*trusted=*/true);
        }
    }

    // Drops the stretches a boundary walk covers twice, once each way: they
    // bound no area, and no ring may repeat a vertex around them.
    static void pruneSpikes(std::vector<PointType>& ring) {
        bool changed = true;
        while (changed && ring.size() >= 3) {
            changed = false;
            for (std::size_t i = 0; i < ring.size() && ring.size() >= 3; ++i) {
                const std::size_t size = ring.size();
                if (!(ring[(i + size - 1) % size] == ring[(i + 1) % size])) {
                    continue;
                }
                const auto at = static_cast<std::ptrdiff_t>(i);
                if (i + 1 < size) {
                    ring.erase(ring.begin() + at, ring.begin() + at + 2);
                } else {
                    ring.erase(ring.begin() + at);
                    ring.erase(ring.begin());
                }
                changed = true;
                break;
            }
        }
        if (ring.size() < 3) {
            ring.clear();
        }
    }

    /**
     * Randomized incremental trapezoidal map. The search history is a DAG:
     * x-nodes discriminate endpoint walls, curve-nodes discriminate the two
     * sides of an arrangement edge, and leaves name live trapezoids. Geometry
     * is kept in a division-closed number type so the index never truncates the
     * midpoint used while constructing it for an integral arrangement.
     */
    class TrapezoidPointLocation {
        using WorkNumber = division_result_t<NumberType>;
        using WorkPoint = Point<WorkNumber>;
        using WorkLine = OrientedLine<WorkPoint>;
        using IntegralPoint = Point<std::int64_t>;
        using IntegralLine = OrientedLine<IntegralPoint>;
        using StoredLine = std::variant<WorkLine, IntegralLine>;
        using QueryInteger = rational_int_t<WorkNumber>;
        using WideIntegralPoint = Point<QueryInteger>;
        static constexpr std::uint32_t none = ~std::uint32_t{};

        struct Bound {
            // -1 and +1 are the two infinities; zero carries a finite abscissa.
            std::int8_t infinity = 0;
            std::uint32_t wall = none;
            WorkNumber x{};

            static Bound negativeInfinity() { return Bound{-1, none, WorkNumber(0)}; }
            static Bound positiveInfinity() { return Bound{1, none, WorkNumber(0)}; }

            bool operator==(const Bound& other) const {
                return infinity == other.infinity && (infinity != 0 || x == other.x);
            }
        };

        // Live trapezoids immediately to the right of one transformed vertex.
        // Entries are append-only; an old entry is ignored once its trapezoid
        // becomes inactive. This is the local adjacency needed to walk a new
        // curve across the map without searching the history DAG again.
        struct Wall {
            std::vector<std::uint32_t> starts;
        };

        struct Curve {
            // Original coordinates, oriented left to right after the shear.
            StoredLine queryLine;
            Bound left;
            Bound right;
            HalfedgeId leftToRight;
            WorkNumber originalDx{};
            WorkNumber originalDy{};
        };

        struct Query {
            const PointType* point = nullptr;
            WorkNumber x{};              // transformed abscissa; curve tests use point
            std::optional<IntegralPoint> integralPoint;
            std::optional<WideIntegralPoint> wideIntegralPoint;
        };

        struct Trapezoid {
            Bound left;
            Bound right;
            std::uint32_t bottom = none;
            std::uint32_t top = none;
            std::uint32_t leaf = none;
            FaceId face;
            bool active = true;
        };

        enum class NodeKind : std::uint8_t { leaf, x, curve };

        struct Node {
            NodeKind kind = NodeKind::leaf;
            std::uint32_t value = 0;  // trapezoid or curve
            std::uint32_t low = none;
            std::uint32_t high = none;
            WorkNumber x{};
        };

    public:
        template <class UniformRandomBitGenerator>
        TrapezoidPointLocation(const Arrangement& arrangement,
                               UniformRandomBitGenerator&& generator) {
            chooseShear(arrangement, generator);
            makeWalls(arrangement);
            makeVertexIndex(arrangement);
            makeCurves(arrangement);

            const std::uint32_t initial = newTrapezoid(
                Bound::negativeInfinity(), Bound::positiveInfinity(), none, none);
            root_ = trapezoids_[initial].leaf;

            std::vector<std::uint32_t> order(curves_.size());
            for (std::uint32_t i = 0; i < order.size(); ++i) {
                order[i] = i;
            }
            std::shuffle(order.begin(), order.end(),
                         std::forward<UniformRandomBitGenerator>(generator));
            for (const std::uint32_t curve : order) {
                insertCurve(curve);
            }
            labelTrapezoids(arrangement);
            std::vector<StoredLine>().swap(constructionLines_);
        }

        [[nodiscard]] FaceId locateFace(const Arrangement& arrangement,
                                        const PointType& point) const {
            const Query query = makeQuery(point);
            std::uint32_t node = root_;
            while (nodes_[node].kind != NodeKind::leaf) {
                const Node& decision = nodes_[node];
                if (decision.kind == NodeKind::x) {
                    if (query.x < decision.x || query.x == decision.x) {
                        node = decision.low;  // the existing -x perturbation
                    } else {
                        node = decision.high;
                    }
                    continue;
                }
                const Curve& curve = curves_[decision.value];
                const auto orientation = sideOf(curve, query);
                int side = orientation > 0 ? 1 : (orientation < 0 ? -1 : 0);
                if (side == 0) {
                    // Sign of cross(d, (-eps,+eps^2)): dy is primary and
                    // dx resolves the horizontal case.
                    side = curve.originalDy > WorkNumber(0)
                               ? 1
                               : (curve.originalDy < WorkNumber(0)
                                      ? -1
                                      : (curve.originalDx > WorkNumber(0) ? 1 : -1));
                }
                node = side < 0 ? decision.low : decision.high;
            }
            const FaceId result = trapezoids_[nodes_[node].value].face;
            assert(result.valid() && result.index() < arrangement.faceCount());
            return result;
        }

        [[nodiscard]] CellId locateCell(const Arrangement& arrangement,
                                        const PointType& point) const {
            const auto vertex = std::lower_bound(
                vertices_.begin(), vertices_.end(), point,
                [&](VertexId v, const PointType& p) { return arrangement.points_[v.index()] < p; });
            if (vertex != vertices_.end() && arrangement.points_[vertex->index()] == point) {
                return *vertex;
            }

            const Query query = makeQuery(point);
            std::uint32_t node = root_;
            while (nodes_[node].kind != NodeKind::leaf) {
                const Node& decision = nodes_[node];
                if (decision.kind == NodeKind::x) {
                    node = query.x < decision.x ? decision.low : decision.high;
                    continue;
                }
                const Curve& curve = curves_[decision.value];
                const auto side = sideOf(curve, query);
                if (side == 0) {
                    return HalfedgeId(2 * (curve.leftToRight.index() / 2));
                }
                node = side < 0 ? decision.low : decision.high;
            }
            return trapezoids_[nodes_[node].value].face;
        }

        [[nodiscard]] VertexId indexedVertex(std::size_t index) const {
            return vertices_[index];
        }

        // Vertices whose abscissa lies within the closed range, in increasing
        // order. An absent bound is unbounded on that side. The index is sorted
        // lexicographically on the original coordinates, so a query confined to
        // a narrow band of abscissae -- a vertical ray above all -- reaches only
        // the vertices that band can hold.
        template <class Number, class Fn>
        void visitCandidateVertices(const Arrangement& arrangement,
                                    const std::optional<Number>& low,
                                    const std::optional<Number>& high,
                                    Fn&& fn) const {
            auto first = vertices_.begin();
            auto last = vertices_.end();
            if (low) {
                first = std::lower_bound(
                    first, last, *low, [&](VertexId v, const Number& x) {
                        return arrangement.points_[v.index()].x() < x;
                    });
            }
            if (high) {
                last = std::upper_bound(
                    first, last, *high, [&](const Number& x, VertexId v) {
                        return x < arrangement.points_[v.index()].x();
                    });
            }
            for (auto it = first; it != last; ++it) {
                fn(*it);
            }
        }

        [[nodiscard]] HalfedgeId indexedHalfedge(std::size_t index) const {
            return HalfedgeId(2 * (curves_[index].leftToRight.index() / 2));
        }

        // Walks the search DAG carrying the query clipped to the region each
        // node stands for, held as an interval of the query's parameter. Both
        // kinds of decision cut that interval with a straight line -- an x node
        // with the wall it splits on, a curve node with the curve's supporting
        // line -- so a child is entered only where the query really reaches it,
        // and the region a node stands for stays exactly the intersection of
        // the cuts along the path. A curve is offered where its supporting line
        // still crosses the clipped query, which is a superset of the
        // intersecting arrangement edges: the caller deduplicates the offers
        // and applies the exact predicate.
        //
        // Nodes are taken earliest first, by the smallest parameter the node
        // can still hold, so once a node comes out no candidate found later can
        // precede it. That bound goes to `fn` as the frontier, which lets the
        // caller settle its answer for everything behind the frontier and stop
        // the walk there instead of running the query out to its far end. `fn`
        // is called with an invalid halfedge to carry the frontier alone, and
        // stops the walk by returning `true`. An absent frontier is the start
        // of an unbounded query, where nothing is settled yet.
        template <class Parameter, class Q, class Fn>
        bool visitCandidateHalfedges(const Q& query, Fn&& fn) const {
            const Parameter zero(0);

            const Parameter x0(query[0].x());
            const Parameter y0(query[0].y());
            const Parameter dx = Parameter(query[1].x()) - x0;
            const Parameter dy = Parameter(query[1].y()) - y0;
            const Parameter abscissa = x0 + Parameter(shear_) * y0;
            const Parameter abscissaRate = dx + Parameter(shear_) * dy;

            // The parameters still in play, each end absent where the query
            // runs off to infinity.
            struct Span {
                std::uint32_t node;
                std::optional<Parameter> low;
                std::optional<Parameter> high;
            };

            // Keeps only the parameters where `value + t * rate` holds the
            // requested sign, and answers whether any parameter is left.
            const auto narrow = [&zero](Span& span, const Parameter& value,
                                        const Parameter& rate, bool nonNegative) {
                if (rate == zero) {
                    return nonNegative ? !(value < zero) : !(value > zero);
                }
                const Parameter root = -value / rate;
                if ((rate > zero) == nonNegative) {
                    if (!span.low || *span.low < root) {
                        span.low = root;
                    }
                } else {
                    if (!span.high || root < *span.high) {
                        span.high = root;
                    }
                }
                return !(span.low && span.high && *span.high < *span.low);
            };

            // The side of a curve's supporting line, as `value + t * rate`.
            const auto sideAlongQuery = [&](const Curve& curve) {
                return std::visit(
                    [&](const auto& line) {
                        const Parameter sourceX(line.source().x());
                        const Parameter sourceY(line.source().y());
                        const Parameter edgeX = Parameter(line.target().x()) - sourceX;
                        const Parameter edgeY = Parameter(line.target().y()) - sourceY;
                        return std::pair<Parameter, Parameter>(
                            edgeX * (y0 - sourceY) - edgeY * (x0 - sourceX),
                            edgeX * dy - edgeY * dx);
                    },
                    curve.queryLine);
            };

            // Orders the heap so the span holding the earliest parameter, an
            // absent bound being earliest of all, comes out first.
            const auto laterFirst = [](const Span& left, const Span& right) {
                if (!left.low) {
                    return false;
                }
                if (!right.low) {
                    return true;
                }
                return *right.low < *left.low;
            };

            Span start{root_, std::nullopt, std::nullopt};
            if constexpr (!OrientedLineConcept<Q>) {
                start.low = zero;
                if constexpr (!RayConcept<Q>) {
                    start.high = Parameter(1);
                }
            }

            std::vector<Span> pending;
            pending.push_back(std::move(start));
            const auto offer = [&](Span span) {
                pending.push_back(std::move(span));
                std::push_heap(pending.begin(), pending.end(), laterFirst);
            };

            while (!pending.empty()) {
                std::pop_heap(pending.begin(), pending.end(), laterFirst);
                const Span span = std::move(pending.back());
                pending.pop_back();

                const Parameter* frontier = span.low ? &*span.low : nullptr;
                if (fn(HalfedgeId(), frontier)) {
                    return true;
                }

                const Node& node = nodes_[span.node];
                if (node.kind == NodeKind::leaf) {
                    continue;
                }

                if (node.kind == NodeKind::x) {
                    // A query sitting exactly on the wall enters both sides.
                    const Parameter split(node.x);
                    Span low = span;
                    if (narrow(low, split - abscissa, -abscissaRate, true)) {
                        low.node = node.low;
                        offer(std::move(low));
                    }
                    Span high = span;
                    if (narrow(high, abscissa - split, abscissaRate, true)) {
                        high.node = node.high;
                        offer(std::move(high));
                    }
                    continue;
                }

                const Curve& curve = curves_[node.value];
                const auto [value, rate] = sideAlongQuery(curve);
                Span low = span;
                const bool below = narrow(low, value, rate, false);
                Span high = span;
                const bool above = narrow(high, value, rate, true);
                if (below && above) {
                    // The clipped query reaches both sides, so it meets the
                    // supporting line inside the region this node stands for.
                    const HalfedgeId h(2 * (curve.leftToRight.index() / 2));
                    if (fn(h, frontier)) {
                        return true;
                    }
                }
                if (below) {
                    low.node = node.low;
                    offer(std::move(low));
                }
                if (above) {
                    high.node = node.high;
                    offer(std::move(high));
                }
            }
            return false;
        }

    private:
        static bool less(const Bound& left, const Bound& right) {
            if (left.infinity != right.infinity) {
                if (left.infinity < 0 || right.infinity > 0) {
                    return true;
                }
                if (left.infinity > 0 || right.infinity < 0) {
                    return false;
                }
            }
            return left.infinity == 0 && right.infinity == 0 && left.x < right.x;
        }

        static Bound maximum(const Bound& left, const Bound& right) {
            return less(left, right) ? right : left;
        }

        static Bound minimum(const Bound& left, const Bound& right) {
            return less(left, right) ? left : right;
        }

        [[nodiscard]] Bound finiteBound(const WorkNumber& x) const {
            const auto found = std::lower_bound(wallXs_.begin(), wallXs_.end(), x);
            assert(found != wallXs_.end() && *found == x);
            return Bound{0, static_cast<std::uint32_t>(found - wallXs_.begin()), x};
        }

        [[nodiscard]] WorkPoint transformed(const PointType& point) const {
            const WorkNumber x(point.x());
            const WorkNumber y(point.y());
            return WorkPoint(x + WorkNumber(shear_) * y, y);
        }

        [[nodiscard]] WorkNumber transformedX(const PointType& point) const {
            return WorkNumber(point.x()) + WorkNumber(shear_) * WorkNumber(point.y());
        }

        template <class UniformRandomBitGenerator>
        void chooseShear(const Arrangement& arrangement,
                         UniformRandomBitGenerator& generator) {
            const auto works = [&](std::int64_t candidate) {
                const WorkNumber k(candidate);
                for (const EdgeGeometry& geometry : arrangement.edgeGeometry_) {
                    const WorkNumber dx = WorkNumber(geometry.b.x()) - WorkNumber(geometry.a.x());
                    const WorkNumber dy = WorkNumber(geometry.b.y()) - WorkNumber(geometry.a.y());
                    if (dx + k * dy == WorkNumber(0)) {
                        return false;
                    }
                }
                std::vector<WorkNumber> abscissae;
                abscissae.reserve(arrangement.points_.size());
                for (const PointType& point : arrangement.points_) {
                    abscissae.push_back(WorkNumber(point.x()) + k * WorkNumber(point.y()));
                }
                std::sort(abscissae.begin(), abscissae.end());
                return std::adjacent_find(abscissae.begin(), abscissae.end()) ==
                       abscissae.end();
            };

            // A random shear almost surely misses the finite forbidden set.
            // Keep the candidates small so transformed coordinates do not grow
            // needlessly; the deterministic tail makes termination independent
            // of generator quality.
            for (int attempt = 0; attempt < 64; ++attempt) {
                const auto bits = static_cast<std::uint64_t>(generator());
                const std::int64_t candidate = static_cast<std::int64_t>(bits % 2000001) - 1000000;
                if (candidate != 0 && works(candidate)) {
                    shear_ = candidate;
                    return;
                }
            }
            for (std::int64_t candidate = 1;; ++candidate) {
                if (works(candidate)) {
                    shear_ = candidate;
                    return;
                }
            }
        }

        void makeVertexIndex(const Arrangement& arrangement) {
            vertices_.reserve(arrangement.points_.size());
            for (std::uint32_t v = 0; v < arrangement.points_.size(); ++v) {
                vertices_.push_back(VertexId(v));
            }
            std::sort(vertices_.begin(), vertices_.end(), [&](VertexId left, VertexId right) {
                return arrangement.points_[left.index()] < arrangement.points_[right.index()];
            });
        }

        void makeWalls(const Arrangement& arrangement) {
            wallXs_.reserve(arrangement.points_.size());
            for (const PointType& point : arrangement.points_) {
                wallXs_.push_back(transformed(point).x());
            }
            std::sort(wallXs_.begin(), wallXs_.end());
            assert(std::adjacent_find(wallXs_.begin(), wallXs_.end()) == wallXs_.end());
            walls_.resize(wallXs_.size());
        }

        void makeCurves(const Arrangement& arrangement) {
            curves_.reserve(arrangement.edgeGeometry_.size());
            constructionLines_.reserve(arrangement.edgeGeometry_.size());
            for (std::uint32_t edge = 0; edge < arrangement.edgeGeometry_.size(); ++edge) {
                const EdgeGeometry& geometry = arrangement.edgeGeometry_[edge];
                WorkPoint a = transformed(geometry.a);
                WorkPoint b = transformed(geometry.b);
                WorkNumber originalDx = WorkNumber(geometry.b.x()) - WorkNumber(geometry.a.x());
                WorkNumber originalDy = WorkNumber(geometry.b.y()) - WorkNumber(geometry.a.y());
                bool forward = a.x() < b.x();
                if (!forward) {
                    std::swap(a, b);
                    originalDx = -originalDx;
                    originalDy = -originalDy;
                }

                Bound left = Bound::negativeInfinity();
                Bound right = Bound::positiveInfinity();
                if (geometry.kind == EdgeKind::segment) {
                    left = finiteBound(a.x());
                    right = finiteBound(b.x());
                } else if (geometry.kind == EdgeKind::ray) {
                    const WorkPoint source = transformed(geometry.a);
                    if (forward) {
                        left = finiteBound(source.x());
                    } else {
                        right = finiteBound(source.x());
                    }
                }
                WorkPoint originalA(WorkNumber(geometry.a.x()), WorkNumber(geometry.a.y()));
                WorkPoint originalB(WorkNumber(geometry.b.x()), WorkNumber(geometry.b.y()));
                if (!forward) {
                    std::swap(originalA, originalB);
                }
                // The shear has determinant one, so curve-node orientation is
                // identical here and avoids enlarging every query coordinate.
                StoredLine queryLine(std::in_place_type<WorkLine>, originalA, originalB);
                StoredLine constructionLine(std::in_place_type<WorkLine>, a, b);
                if constexpr (is_Rational_v<WorkNumber>) {
                    if (auto integral = WorkLine(originalA, originalB)
                                            .template integralLine<std::int64_t>()) {
                        const auto transformedIntegralPoint = [&](const IntegralPoint& point) {
                            const int128 x = int128(point.x()) +
                                             int128(shear_) * int128(point.y());
                            return detail::representableAs<std::int64_t>(x)
                                       ? std::optional<IntegralPoint>(IntegralPoint(
                                             detail::narrowTo<std::int64_t>(x), point.y()))
                                       : std::optional<IntegralPoint>();
                        };
                        const auto source = transformedIntegralPoint(integral->source());
                        const auto target = transformedIntegralPoint(integral->target());
                        if (source && target) {
                            constructionLine = IntegralLine(*source, *target);
                        } else if (auto transformedIntegral =
                                       WorkLine(a, b).template integralLine<std::int64_t>()) {
                            constructionLine = std::move(*transformedIntegral);
                        }
                        queryLine = std::move(*integral);
                    } else if (auto transformedIntegral =
                                   WorkLine(a, b).template integralLine<std::int64_t>()) {
                        constructionLine = std::move(*transformedIntegral);
                    }
                }
                curves_.push_back(Curve{std::move(queryLine), left, right,
                                        HalfedgeId(2 * edge + (forward ? 0 : 1)),
                                        originalDx, originalDy});
                constructionLines_.push_back(std::move(constructionLine));
            }
        }

        [[nodiscard]] std::partial_ordering constructionSideOf(
            std::uint32_t curve, const WorkPoint& point) const {
            return std::visit(
                [&](const auto& line) {
                    return orientationSign(line.source(), line.target(), point);
                },
                constructionLines_[curve]);
        }

        [[nodiscard]] static std::partial_ordering sideOf(const Curve& curve,
                                                           const Query& query) {
            return std::visit(
                [&](const auto& line) {
                    using Line = std::remove_cvref_t<decltype(line)>;
                    if constexpr (std::same_as<Line, IntegralLine>) {
                        if (query.integralPoint) {
                            return orientationSign(line.source(), line.target(),
                                                   *query.integralPoint);
                        }
                        if (query.wideIntegralPoint) {
                            return orientationSign(line.source(), line.target(),
                                                   *query.wideIntegralPoint);
                        }
                    }
                    return orientationSign(line.source(), line.target(), *query.point);
                },
                curve.queryLine);
        }

        [[nodiscard]] Query makeQuery(const PointType& point) const {
            Query query{&point, transformedX(point), std::nullopt, std::nullopt};
            classifyQuery(query, point);
            return query;
        }

        void classifyQuery(Query& query, const PointType& original) const {
            const auto storeIntegral = [&](QueryInteger x, QueryInteger y) {
                if (detail::representableAs<std::int64_t>(x) &&
                    detail::representableAs<std::int64_t>(y)) {
                    query.integralPoint.emplace(detail::narrowTo<std::int64_t>(x),
                                                detail::narrowTo<std::int64_t>(y));
                } else {
                    query.wideIntegralPoint.emplace(std::move(x), std::move(y));
                }
            };

            // Curve nodes operate in the original coordinates. Only x nodes
            // need the sheared abscissa stored in Query::x.
            if constexpr (is_Rational_v<NumberType>) {
                if (original.x().isInteger() && original.y().isInteger()) {
                    storeIntegral(
                        QueryInteger(static_cast<rational_int_t<NumberType>>(
                            original.x())),
                        QueryInteger(static_cast<rational_int_t<NumberType>>(
                            original.y())));
                }
            } else if constexpr (detail::extended_integral<NumberType> ||
                                 std::same_as<NumberType, BigInt>) {
                storeIntegral(QueryInteger(original.x()), QueryInteger(original.y()));
            }
        }

        [[nodiscard]] WorkNumber sample(const Bound& left, const Bound& right) const {
            assert(less(left, right));
            if (left.infinity < 0 && right.infinity > 0) {
                return WorkNumber(0);
            }
            if (left.infinity < 0) {
                return right.x - WorkNumber(1);
            }
            if (right.infinity > 0) {
                return left.x + WorkNumber(1);
            }
            return (left.x + right.x) / WorkNumber(2);
        }

        [[nodiscard]] WorkPoint pointAt(std::uint32_t curve, const WorkNumber& x) const {
            return std::visit(
                [&](const auto& line) {
                    const WorkNumber ax(line.source().x());
                    const WorkNumber ay(line.source().y());
                    const WorkNumber bx(line.target().x());
                    const WorkNumber by(line.target().y());
                    const WorkNumber parameter = (x - ax) / (bx - ax);
                    return WorkPoint(x, ay + parameter * (by - ay));
                },
                constructionLines_[curve]);
        }

        [[nodiscard]] bool crosses(std::uint32_t curveId,
                                   const Trapezoid& trapezoid) const {
            const Curve& curve = curves_[curveId];
            const Bound left = maximum(curve.left, trapezoid.left);
            const Bound right = minimum(curve.right, trapezoid.right);
            if (!less(left, right)) {
                return false;
            }
            const WorkPoint point = pointAt(curveId, sample(left, right));
            if (trapezoid.bottom != none &&
                constructionSideOf(trapezoid.bottom, point) <= 0) {
                return false;
            }
            if (trapezoid.top != none &&
                constructionSideOf(trapezoid.top, point) >= 0) {
                return false;
            }
            return true;
        }

        [[nodiscard]] std::uint32_t newLeaf(std::uint32_t trapezoid) {
            const std::uint32_t id = static_cast<std::uint32_t>(nodes_.size());
            nodes_.push_back(Node{NodeKind::leaf, trapezoid, none, none, WorkNumber(0)});
            return id;
        }

        [[nodiscard]] std::uint32_t newTrapezoid(Bound left, Bound right,
                                                  std::uint32_t bottom,
                                                  std::uint32_t top) {
            if (trapezoids_.size() >= none || nodes_.size() >= none) {
                throw std::length_error("Arrangement point-location index exceeds 32-bit capacity");
            }
            const std::uint32_t id = static_cast<std::uint32_t>(trapezoids_.size());
            trapezoids_.push_back(Trapezoid{std::move(left), std::move(right), bottom, top,
                                            none, FaceId(), true});
            if (trapezoids_.back().left.infinity == 0) {
                walls_[trapezoids_.back().left.wall].starts.push_back(id);
            }
            trapezoids_.back().leaf = newLeaf(id);
            return id;
        }

        [[nodiscard]] std::uint32_t newNode(Node node) {
            if (nodes_.size() >= none) {
                throw std::length_error("Arrangement point-location index exceeds 32-bit capacity");
            }
            const std::uint32_t id = static_cast<std::uint32_t>(nodes_.size());
            nodes_.push_back(std::move(node));
            return id;
        }

        [[nodiscard]] Node curveNode(std::uint32_t curve, std::uint32_t below,
                                     std::uint32_t above) const {
            return Node{NodeKind::curve, curve, below, above, WorkNumber(0)};
        }

        [[nodiscard]] Node xNode(const WorkNumber& x, std::uint32_t left,
                                 std::uint32_t right) const {
            return Node{NodeKind::x, 0, left, right, x};
        }

        // Chooses an abscissa just to the right of a curve's left end, but
        // before the next arrangement vertex. No vertical wall of the
        // trapezoidal map can occur inside that open slab.
        [[nodiscard]] WorkNumber probeRightOf(const Bound& left,
                                              const Bound& right) const {
            const WorkNumber* nextWall = nullptr;
            if (left.infinity < 0) {
                if (!wallXs_.empty()) {
                    nextWall = &wallXs_.front();
                }
            } else {
                const std::size_t following = static_cast<std::size_t>(left.wall) + 1;
                if (following < wallXs_.size()) {
                    nextWall = &wallXs_[following];
                }
            }

            WorkNumber next;
            bool hasNext = false;
            if (right.infinity == 0) {
                next = right.x;
                hasNext = true;
            }
            if (nextWall != nullptr && (!hasNext || *nextWall < next)) {
                next = *nextWall;
                hasNext = true;
            }

            if (hasNext) {
                return left.infinity < 0
                           ? next - WorkNumber(1)
                           : (left.x + next) / WorkNumber(2);
            }
            return left.infinity < 0 ? WorkNumber(0) : left.x + WorkNumber(1);
        }

        [[nodiscard]] std::uint32_t locateTrapezoid(const WorkPoint& point) const {
            std::uint32_t nodeId = root_;
            while (nodes_[nodeId].kind != NodeKind::leaf) {
                const Node& node = nodes_[nodeId];
                if (node.kind == NodeKind::x) {
                    assert(point.x() != node.x);
                    nodeId = point.x() < node.x ? node.low : node.high;
                    continue;
                }
                const auto side = constructionSideOf(node.value, point);
                assert(side != 0);
                nodeId = side < 0 ? node.low : node.high;
            }
            const std::uint32_t trapezoid = nodes_[nodeId].value;
            assert(trapezoids_[trapezoid].active);
            return trapezoid;
        }

        // One DAG query finds the first trapezoid. Every subsequent one is
        // selected from the live trapezoids beginning at the current right
        // wall, so insertion work follows the curve instead of revisiting its
        // complete search history.
        [[nodiscard]] std::vector<std::uint32_t> crossedByWalk(
            std::uint32_t curveId) const {
            const Curve& curve = curves_[curveId];
            const WorkNumber probe = probeRightOf(curve.left, curve.right);
            std::uint32_t current = locateTrapezoid(pointAt(curveId, probe));
            std::vector<std::uint32_t> crossed;

            for (;;) {
                const Trapezoid& trapezoid = trapezoids_[current];
                assert(trapezoid.active && crosses(curveId, trapezoid));
                crossed.push_back(current);

                if (!less(trapezoid.right, curve.right)) {
                    break;
                }
                assert(trapezoid.right.infinity == 0);

                std::uint32_t next = none;
                for (const std::uint32_t candidate :
                     walls_[trapezoid.right.wall].starts) {
                    const Trapezoid& adjacent = trapezoids_[candidate];
                    if (adjacent.active && adjacent.left == trapezoid.right &&
                        crosses(curveId, adjacent)) {
                        assert(next == none);
                        next = candidate;
                    }
                }
                if (next == none) {
                    throw std::logic_error(
                        "arrangement edge lost its adjacent trapezoid");
                }
                current = next;
            }
            return crossed;
        }

        void insertCurve(std::uint32_t curveId) {
            const Curve& curve = curves_[curveId];
            const std::vector<std::uint32_t> crossed = crossedByWalk(curveId);
            if (crossed.empty()) {
                throw std::logic_error("arrangement edge did not cross its trapezoidal map");
            }

            std::uint32_t previousAbove = none;
            std::uint32_t previousBelow = none;
            for (const std::uint32_t oldId : crossed) {
                const Trapezoid old = trapezoids_[oldId];
                const Bound left = maximum(curve.left, old.left);
                const Bound right = minimum(curve.right, old.right);

                std::uint32_t below;
                if (previousBelow != none &&
                    trapezoids_[previousBelow].bottom == old.bottom &&
                    trapezoids_[previousBelow].right == left) {
                    below = previousBelow;
                    trapezoids_[below].right = right;
                } else {
                    below = newTrapezoid(left, right, old.bottom, curveId);
                }

                std::uint32_t above;
                if (previousAbove != none && trapezoids_[previousAbove].top == old.top &&
                    trapezoids_[previousAbove].right == left) {
                    above = previousAbove;
                    trapezoids_[above].right = right;
                } else {
                    above = newTrapezoid(left, right, curveId, old.top);
                }

                const bool hasLeftCap = less(old.left, left);
                const bool hasRightCap = less(right, old.right);
                std::uint32_t leftCap = none;
                std::uint32_t rightCap = none;
                if (hasLeftCap) {
                    leftCap = newTrapezoid(old.left, left, old.bottom, old.top);
                }
                if (hasRightCap) {
                    rightCap = newTrapezoid(right, old.right, old.bottom, old.top);
                }

                Node replacement = curveNode(curveId, trapezoids_[below].leaf,
                                             trapezoids_[above].leaf);
                if (hasRightCap) {
                    const std::uint32_t split = newNode(std::move(replacement));
                    replacement = xNode(right.x, split, trapezoids_[rightCap].leaf);
                }
                if (hasLeftCap) {
                    const std::uint32_t split = newNode(std::move(replacement));
                    replacement = xNode(left.x, trapezoids_[leftCap].leaf, split);
                }
                nodes_[old.leaf] = std::move(replacement);
                trapezoids_[oldId].active = false;
                previousBelow = below;
                previousAbove = above;
            }
        }

        [[nodiscard]] FaceId faceAtTransformedTopInfinity(
            const Arrangement& arrangement) const {
            if (!arrangement.infinity_.valid()) {
                return FaceId(0);
            }
            const FanDirection query{NumberType(-shear_), NumberType(1),
                                     PointType(NumberType(0), NumberType(0)), 0};
            const auto after = std::upper_bound(
                arrangement.infinityFan_.begin(), arrangement.infinityFan_.end(), query,
                [&](const FanDirection& value, std::uint32_t halfedge) {
                    return infinityFanLess(value, arrangement.fanDirection(halfedge));
                });
            const std::uint32_t nextDirection =
                after == arrangement.infinityFan_.end() ? arrangement.infinityFan_.front() : *after;
            return arrangement.face(HalfedgeId(nextDirection ^ 1));
        }

        void labelTrapezoids(const Arrangement& arrangement) {
            const FaceId exterior = faceAtTransformedTopInfinity(arrangement);
            for (Trapezoid& trapezoid : trapezoids_) {
                if (!trapezoid.active) {
                    continue;
                }
                if (trapezoid.bottom != none) {
                    trapezoid.face = arrangement.face(curves_[trapezoid.bottom].leftToRight);
                    if (trapezoid.top != none) {
                        assert(trapezoid.face == arrangement.face(
                            arrangement.twin(curves_[trapezoid.top].leftToRight)));
                    }
                } else if (trapezoid.top != none) {
                    trapezoid.face = arrangement.face(
                        arrangement.twin(curves_[trapezoid.top].leftToRight));
                } else {
                    trapezoid.face = exterior;
                }
            }
        }

        std::int64_t shear_ = 1;
        std::uint32_t root_ = none;
        std::vector<VertexId> vertices_;
        std::vector<WorkNumber> wallXs_;
        std::vector<Wall> walls_;
        std::vector<Curve> curves_;
        // Used only while constructing the RIC and released by the constructor.
        std::vector<StoredLine> constructionLines_;
        std::vector<Trapezoid> trapezoids_;
        std::vector<Node> nodes_;
    };

    std::vector<PointType> points_;
    VertexId infinity_;                            // symbolic vertex, absent for bounded input
    std::vector<HalfedgeId> outgoing_;         // one per vertex; invalid when isolated
    std::vector<std::uint32_t> origin_;        // one per halfedge
    std::vector<std::uint32_t> next_;          // one per halfedge
    std::vector<std::uint32_t> face_;          // one per halfedge
    std::vector<TLabel> edgeLabel_;            // one per edge
    std::vector<EdgeGeometry> edgeGeometry_;   // finite defining geometry, one per edge
    std::vector<std::uint32_t> originOffset_;  // one per edge, plus a closing entry
    std::vector<std::uint32_t> originIndex_;   // input positions, grouped by edge
    std::vector<HalfedgeId> outerCycle_;       // one per face; invalid for unbounded faces
    std::vector<std::uint32_t> innerOffset_;   // one per face, plus a closing entry
    std::vector<HalfedgeId> innerCycle_;       // inner cycles, grouped by face
    std::vector<TLabel> faceLabel_;            // one per face
    std::vector<bool> unboundedFace_;           // one per face
    std::vector<std::uint32_t> infinityFan_;    // outgoing ends in counterclockwise order
    std::shared_ptr<const TrapezoidPointLocation> pointLocation_;
};

template <class PointType, class TLabel>
template <class Q, class Fn>
bool Arrangement<PointType, TLabel>::visitStraightIntersecting(
    const Q& piece, Fn& fn, std::vector<bool>* seenVertices,
    std::vector<bool>* seenEdges) const {
    using CommonNumber = std::common_type_t<NumberType, typename Q::NumberType>;
    using Parameter = division_result_t<CommonNumber>;

    // A cell the piece meets, at the parameter where it first meets it. An edge
    // the piece runs along from infinity has no such parameter, and sorts ahead
    // of everything instead.
    struct Event {
        bool negativeInfinity = false;
        Parameter parameter{};
        IntersectionId id;
    };

    const Parameter ax(piece[0].x());
    const Parameter ay(piece[0].y());
    const Parameter dx = Parameter(piece[1].x()) - ax;
    const Parameter dy = Parameter(piece[1].y()) - ay;
    const Parameter squaredLength = dx * dx + dy * dy;

    // Where a point falls along the piece, as the parameter that is 0 at its
    // first endpoint and 1 at its second. A degenerate piece puts everything
    // at 0, which is the only parameter it has.
    const auto parameterOf = [&](const auto& point) {
        if (squaredLength == Parameter(0)) {
            return Parameter(0);
        }
        return ((Parameter(point.x()) - ax) * dx +
                (Parameter(point.y()) - ay) * dy) /
               squaredLength;
    };

    // The parameter at which the piece first meets an edge. Crossing edges meet
    // at one parameter; a collinear edge is met along a stretch, and the earliest
    // of it is what counts. A query bounded at its start never reports earlier
    // than that start.
    const auto firstOnEdge = [&](const EdgeGeometry& geometry) {
        Event event;
        if (squaredLength == Parameter(0)) {
            return event;
        }

        const Parameter ex = Parameter(geometry.b.x()) - Parameter(geometry.a.x());
        const Parameter ey = Parameter(geometry.b.y()) - Parameter(geometry.a.y());
        Parameter denominator = dx * ey - dy * ex;
        if (denominator != Parameter(0)) {
            Parameter numerator =
                (Parameter(geometry.a.x()) - ax) * ey -
                (Parameter(geometry.a.y()) - ay) * ex;
            event.parameter = numerator / denominator;
            return event;
        }

        if (geometry.kind == EdgeKind::line) {
            event.negativeInfinity = true;
        } else if (geometry.kind == EdgeKind::ray && ex * dx + ey * dy < Parameter(0)) {
            event.negativeInfinity = true;
        } else {
            event.parameter = parameterOf(geometry.a);
            if (geometry.kind == EdgeKind::segment) {
                event.parameter = std::min(event.parameter, parameterOf(geometry.b));
            }
        }

        if constexpr (!OrientedLineConcept<Q>) {
            if (event.negativeInfinity || event.parameter < Parameter(0)) {
                event.negativeInfinity = false;
                event.parameter = Parameter(0);
            }
        }
        return event;
    };

    // Order along the piece, breaking a tie towards the vertex, since a cell the
    // piece meets at the same parameter as an edge is the edge's endpoint and
    // stands for the contact. Handle order settles the rest, so the traversal is
    // reproducible.
    const auto eventLess = [](const Event& left, const Event& right) {
        if (left.negativeInfinity != right.negativeInfinity) {
            return left.negativeInfinity;
        }
        if (!left.negativeInfinity && left.parameter != right.parameter) {
            return left.parameter < right.parameter;
        }
        const bool leftVertex = std::holds_alternative<VertexId>(left.id);
        const bool rightVertex = std::holds_alternative<VertexId>(right.id);
        if (leftVertex != rightVertex) {
            return leftVertex;
        }
        return std::visit([](const auto& id) { return id.index(); }, left.id) <
               std::visit([](const auto& id) { return id.index(); }, right.id);
    };

    // The abscissae the piece can reach, open on a side the piece runs off to.
    std::optional<Parameter> lowX;
    std::optional<Parameter> highX;
    if (Parameter(piece[0].x()) == Parameter(piece[1].x())) {
        lowX = Parameter(piece[0].x());
        highX = lowX;
    } else if constexpr (RayConcept<Q>) {
        (Parameter(piece[1].x()) > Parameter(piece[0].x()) ? lowX : highX) =
            Parameter(piece[0].x());
    } else if constexpr (!OrientedLineConcept<Q>) {
        lowX = std::min(Parameter(piece[0].x()), Parameter(piece[1].x()));
        highX = std::max(Parameter(piece[0].x()), Parameter(piece[1].x()));
    }

    // Events wait here until the search has passed them, earliest on top.
    const auto laterFirst = [&eventLess](const Event& left, const Event& right) {
        return eventLess(right, left);
    };
    std::vector<Event> queued;
    const auto queue = [&](Event event) {
        queued.push_back(std::move(event));
        std::push_heap(queued.begin(), queued.end(), laterFirst);
    };

    // Queues a vertex the piece passes through, unless an earlier piece of the
    // same chain already reported it.
    const auto offerVertex = [&](VertexId v) {
        if (seenVertices != nullptr && (*seenVertices)[v.index()]) {
            return;
        }
        const PointType& point = points_[v.index()];
        if (piece.intersects(point)) {
            queue(Event{false, parameterOf(point), v});
        }
    };

    // The walk can reach one edge from several nodes. The offers it has already
    // made live in a table sized to the offers themselves rather than to the
    // arrangement, so a query never pays for the edges it does not look at.
    static constexpr std::uint32_t noEdge = ~std::uint32_t{};
    std::vector<std::uint32_t> offered(16, noEdge);
    std::size_t offeredCount = 0;
    // Open addressing with linear probing; the table always keeps a free slot,
    // so the scan terminates.
    const auto slotOf = [](const std::vector<std::uint32_t>& table, std::uint32_t edge) {
        std::size_t slot = (edge * 2654435761U) & (table.size() - 1);
        while (table[slot] != noEdge && table[slot] != edge) {
            slot = (slot + 1) & (table.size() - 1);
        }
        return slot;
    };
    const auto offeredBefore = [&](std::uint32_t edge) {
        if (2 * (offeredCount + 1) > offered.size()) {
            std::vector<std::uint32_t> grown(2 * offered.size(), noEdge);
            for (const std::uint32_t held : offered) {
                if (held != noEdge) {
                    grown[slotOf(grown, held)] = held;
                }
            }
            offered.swap(grown);
        }
        const std::size_t slot = slotOf(offered, edge);
        if (offered[slot] == edge) {
            return true;
        }
        offered[slot] = edge;
        ++offeredCount;
        return false;
    };

    // Queues an edge the piece crosses, given a candidate the search turned up.
    // Candidates are a superset, so the exact predicate decides here.
    const auto offerEdge = [&](HalfedgeId h) {
        const std::size_t edge = h.index() / 2;
        if (offeredBefore(static_cast<std::uint32_t>(edge))) {
            return;
        }
        if (seenEdges != nullptr && (*seenEdges)[edge]) {
            return;
        }
        const EdgeGeometry& geometry = edgeGeometry_[edge];

        // Where this piece meets a finite endpoint, the vertex there is the
        // whole of the contact and represents it. Another piece meeting the
        // same edge away from its endpoints still reports the edge.
        if (piece.intersects(geometry.a) ||
            (geometry.kind == EdgeKind::segment && piece.intersects(geometry.b))) {
            return;
        }
        const bool intersects = std::visit(
            [&](const auto& value) { return piece.intersects(value); }, (*this)[h]);
        if (!intersects) {
            return;
        }
        Event event = firstOnEdge(geometry);
        event.id = h;
        queue(std::move(event));
    };

    // Records a cell as reported and answers whether it still needs reporting,
    // which is how a chain avoids naming the same cell from two of its pieces.
    const auto markSeen = [&](const IntersectionId& id) {
        if (const auto* v = std::get_if<VertexId>(&id)) {
            if (seenVertices != nullptr) {
                if ((*seenVertices)[v->index()]) {
                    return false;
                }
                (*seenVertices)[v->index()] = true;
            }
        } else if (seenEdges != nullptr) {
            const std::size_t edge = std::get<HalfedgeId>(id).index() / 2;
            if ((*seenEdges)[edge]) {
                return false;
            }
            (*seenEdges)[edge] = true;
        }
        return true;
    };

    // Hands over the events the search can no longer precede: those strictly
    // ahead of the frontier it has reached, or all of them once it is over.
    // Events tying with the frontier wait, since a candidate still to come may
    // share their parameter and order before them.
    const auto release = [&](const Parameter* frontier, bool exhausted) {
        while (!queued.empty()) {
            if (!exhausted) {
                const Event& next = queued.front();
                if (frontier == nullptr ||
                    (!next.negativeInfinity && !(next.parameter < *frontier))) {
                    break;
                }
            }
            std::pop_heap(queued.begin(), queued.end(), laterFirst);
            Event event = std::move(queued.back());
            queued.pop_back();
            if (markSeen(event.id) && detail::invokeVisitor(fn, event.id)) {
                return true;
            }
        }
        return false;
    };

    if (pointLocation_) {
        pointLocation_->visitCandidateVertices(*this, lowX, highX, offerVertex);
        // The walk reports earliest first, so a visitor that stops on its first
        // answer stops the search with it instead of running the query out.
        if (pointLocation_->template visitCandidateHalfedges<Parameter>(
                piece, [&](HalfedgeId h, const Parameter* frontier) {
                    if (h.valid()) {
                        offerEdge(h);
                    }
                    return release(frontier, /*exhausted=*/false);
                })) {
            return true;
        }
    } else {
        for (std::size_t i = 0; i < points_.size(); ++i) {
            offerVertex(VertexId(static_cast<std::uint32_t>(i)));
        }
        for (std::size_t edge = 0; edge < edgeGeometry_.size(); ++edge) {
            offerEdge(HalfedgeId(static_cast<std::uint32_t>(2 * edge)));
        }
    }
    return release(nullptr, /*exhausted=*/true);
}

template <class PointType, class TLabel>
template <PointConcept Q, class Fn>
bool Arrangement<PointType, TLabel>::visitPointIntersecting(
    const Q& point, Fn& fn, std::vector<bool>* seenVertices,
    std::vector<bool>* seenEdges) const {
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const VertexId v = pointLocation_ ? pointLocation_->indexedVertex(i)
                                          : VertexId(static_cast<std::uint32_t>(i));
        if ((seenVertices == nullptr || !(*seenVertices)[v.index()]) &&
            points_[v.index()].intersects(point)) {
            if (seenVertices != nullptr) {
                (*seenVertices)[v.index()] = true;
            }
            return detail::invokeVisitor(fn, IntersectionId(v));
        }
    }

    for (std::size_t i = 0; i < edgeGeometry_.size(); ++i) {
        const HalfedgeId h = pointLocation_
                                 ? pointLocation_->indexedHalfedge(i)
                                 : HalfedgeId(static_cast<std::uint32_t>(2 * i));
        const std::size_t edge = h.index() / 2;
        if (seenEdges != nullptr && (*seenEdges)[edge]) {
            continue;
        }
        const EdgeGeometry& geometry = edgeGeometry_[edge];
        if (point.intersects(geometry.a) ||
            (geometry.kind == EdgeKind::segment && point.intersects(geometry.b))) {
            continue;
        }
        if (!std::visit([&](const auto& value) { return point.intersects(value); },
                        (*this)[h])) {
            continue;
        }
        if (seenEdges != nullptr) {
            (*seenEdges)[edge] = true;
        }
        if (detail::invokeVisitor(fn, IntersectionId(h))) {
            return true;
        }
    }
    return false;
}

template <class PointType, class TLabel>
template <class Q, class Fn>
    requires (OrientedSegmentConcept<Q> || OrientedLineConcept<Q> ||
              RayConcept<Q> || MonotoneChainConcept<Q> || PolylineConcept<Q>)
bool Arrangement<PointType, TLabel>::visitIntersecting(const Q& r, Fn fn) const {
    if constexpr (MonotoneChainConcept<Q> || PolylineConcept<Q>) {
        if (r.empty()) {
            return false;
        }
        std::vector<bool> seenVertices(points_.size(), false);
        std::vector<bool> seenEdges(edgeGeometry_.size(), false);
        if (r.size() == 1) {
            return visitPointIntersecting(r[0], fn, &seenVertices, &seenEdges);
        }
        for (const auto& edge : r.orientedEdgesView()) {
            if (visitStraightIntersecting(edge, fn, &seenVertices, &seenEdges)) {
                return true;
            }
        }
        return false;
    } else {
        return visitStraightIntersecting(r, fn, nullptr, nullptr);
    }
}

template <class PointType, class TLabel>
void Arrangement<PointType, TLabel>::buildPointLocation() {
    // Stable across runs: callers wanting another insertion order can use the
    // generator-taking overload.
    std::mt19937 generator(0x50474cU);  // "PGL"
    buildPointLocation(generator);
}

template <class PointType, class TLabel>
template <class UniformRandomBitGenerator>
void Arrangement<PointType, TLabel>::buildPointLocation(
    UniformRandomBitGenerator&& generator) {
    if (!pointLocation_) {
        pointLocation_ = std::make_shared<const TrapezoidPointLocation>(
            *this, std::forward<UniformRandomBitGenerator>(generator));
    }
}

template <class PointType, class TLabel>
typename Arrangement<PointType, TLabel>::FaceId
Arrangement<PointType, TLabel>::locateFace(const PointType& point) const {
    return pointLocation_ ? pointLocation_->locateFace(*this, point) : locateFaceLinear(point);
}

template <class PointType, class TLabel>
typename Arrangement<PointType, TLabel>::CellId
Arrangement<PointType, TLabel>::locateCell(const PointType& point) const {
    return pointLocation_ ? pointLocation_->locateCell(*this, point) : locateCellLinear(point);
}

template <TriangleConcept TriangleType, SegmentConcept SegmentType>
template <class ResultNumber>
Arrangement<Point<ResultNumber>, typename Triangulation<TriangleType, SegmentType>::PointType>
Triangulation<TriangleType, SegmentType>::voronoiDiagram() const {
    assert(!empty() && "Triangulation::voronoiDiagram requires a nonempty triangulation");

    using ResultPoint = Point<ResultNumber>;
    using Diagram = Arrangement<ResultPoint, PointType>;

    // One exact circumcenter per current real triangle. This deliberately uses
    // the internal convex-hull triangulation, including any triangles hidden by
    // a carved domain: the method's precondition says that this whole current
    // connectivity is the Delaunay triangulation of the stored sites.
    std::vector<ResultPoint> centers(static_cast<std::size_t>(firstGhost_));
    for (TriId t = 0; t < firstGhost_; ++t) {
        centers[static_cast<std::size_t>(t)] = ResultPoint(
            triangleValue(t).circumcircle().template center<ResultNumber>());
    }

    std::vector<Shape<ResultPoint>> dualEdges;
    dualEdges.reserve(segToEdge_.size());
    for (TriId t = 0; t < firstGhost_; ++t) {
        const Tri& triangle = triangles_[static_cast<std::size_t>(t)];
        for (int side = 0; side < 3; ++side) {
            const TriId neighbor = triangle.nbr[static_cast<std::size_t>(side)];
            if (!isGhost(neighbor)) {
                // Each interior primal edge is visited from both incident
                // triangles. Emit its dual only from the lower triangle id.
                if (neighbor < t) {
                    continue;
                }
                const ResultPoint& a = centers[static_cast<std::size_t>(t)];
                const ResultPoint& b = centers[static_cast<std::size_t>(neighbor)];
                if (a != b) {
                    dualEdges.emplace_back(Segment<ResultPoint>(a, b));
                }
                continue;
            }

            // A real triangle is counterclockwise, so its directed side
            // a -> b has the hull interior on its left. Rotating b-a clockwise
            // therefore points out of the hull and orients the dual ray.
            const PointType& a = vertices_[static_cast<std::size_t>(
                triangle.v[static_cast<std::size_t>((side + 1) % 3)])];
            const PointType& b = vertices_[static_cast<std::size_t>(
                triangle.v[static_cast<std::size_t>((side + 2) % 3)])];
            const ResultNumber dx = static_cast<ResultNumber>(b.x()) -
                                    static_cast<ResultNumber>(a.x());
            const ResultNumber dy = static_cast<ResultNumber>(b.y()) -
                                    static_cast<ResultNumber>(a.y());
            const ResultPoint& center = centers[static_cast<std::size_t>(t)];
            dualEdges.emplace_back(
                Ray<ResultPoint>(center, center + ResultPoint(dy, -dx)));
        }
    }

    Diagram diagram(dualEdges);

    // Site points are strictly inside their own cells. Build the logarithmic
    // point-location index only for this attribution pass, then release it so
    // the returned Arrangement follows the usual opt-in indexing contract.
    diagram.buildPointLocation();
    for (VertexId vertex = 1; vertex < static_cast<VertexId>(vertices_.size()); ++vertex) {
        const auto face = diagram.locateFace(ResultPoint(vertices_[static_cast<std::size_t>(vertex)]));
        diagram.label(face) = vertices_[static_cast<std::size_t>(vertex)];
    }
    diagram.clearPointLocation();
    return diagram;
}

// No deduction guide, deliberately: the vertex type cannot be read off the
// input, because the input's own type is usually the wrong answer. Two integral
// segments cross at a rational point, so a guide reading the type off the input
// would silently pick a vertex type that cannot hold the vertices it is about to
// compute. What `Arrangement(shapes)` deduces instead is the default vertex
// type, which is exact whatever the input is; a caller wanting another one, or
// wanting the edges to carry the input's labels, names it.

}  // namespace pgl
