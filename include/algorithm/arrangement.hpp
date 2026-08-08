#pragma once

#include "algorithm/triangulation.hpp"

/**
 * @file arrangement.hpp
 * @brief Planar subdivision induced by a set of one-dimensional shapes.
 *
 * An @ref pgl::Arrangement is the decomposition of the plane produced by a
 * collection of segments: its **vertices** are the segment endpoints together
 * with every crossing and overlap end, its **edges** are the pieces the
 * vertices cut the segments into, and its **faces** are the connected
 * components of what is left of the plane. Every point of the plane belongs to
 * exactly one of those cells, which is what makes an arrangement the natural
 * back end of any operation that has to answer "the same question" on each
 * piece of a subdivided plane — the regularized booleans, the Minkowski sums,
 * and the connectivity tests behind `separates` all do exactly that.
 *
 * The topology is a doubly connected edge list (DCEL). Halfedges are stored in
 * twin-adjacent pairs, so `twin(h)` is `h.index() ^ 1` and costs no memory
 * access, and the whole structure is three `std::uint32_t` arrays (`origin`,
 * `next`, `face`) beside the vertex coordinates. The three handle families are
 * distinct types (@ref pgl::VertexId, @ref pgl::HalfedgeId, @ref pgl::FaceId),
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
 * meet at their endpoints alone.
 *
 * Unbounded input — lines, rays and half-planes — is not accepted yet. The
 * accessors it will need, @ref pgl::Arrangement::isUnbounded and
 * @ref pgl::Arrangement::isFictitious, are already part of the interface so
 * that supporting it stays an additive change.
 */

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <span>
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

/** @brief Tag making @ref VertexId a distinct type. */
struct ArrangementVertexTag;
/** @brief Tag making @ref HalfedgeId a distinct type. */
struct ArrangementHalfedgeTag;
/** @brief Tag making @ref FaceId a distinct type. */
struct ArrangementFaceTag;

/** @brief Handle of a vertex of an @ref Arrangement. */
using VertexId = detail::Handle<ArrangementVertexTag>;
/** @brief Handle of a halfedge of an @ref Arrangement. */
using HalfedgeId = detail::Handle<ArrangementHalfedgeTag>;
/** @brief Handle of a face of an @ref Arrangement. */
using FaceId = detail::Handle<ArrangementFaceTag>;

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
 * @ref VertexId, @ref HalfedgeId and @ref FaceId. Each edge is a pair of
 * twin halfedges: `twin(h)` is `h.index() ^ 1`, and the face of a halfedge is
 * always the one on its **left**, so a bounded face is enclosed by a
 * counterclockwise cycle of `next` and the outer boundary of a connected piece
 * of the input runs clockwise.
 *
 * Face 0 is always the unbounded face, and it is the only face of an empty
 * arrangement.
 *
 * Construction is exact and takes any range of bounded one-dimensional or
 * polygonal shapes; a @ref Point in the range becomes an isolated vertex, and
 * a shape that is not bounded (a line, a ray, a half-plane) is rejected at
 * compile time.
 */
template <class PointType_, class TLabel>
class Arrangement {
public:
    /** @brief Vertex type. */
    using PointType = PointType_;
    /** @brief Coordinate type of the vertices. */
    using NumberType = typename PointType::NumberType;
    /** @brief Label type carried by edges and faces. */
    using LabelType = TLabel;
    /** @brief Type returned for an edge. */
    using SegmentType = Segment<PointType, TLabel>;

    static_assert(detail::is_point_v<PointType>, "Arrangement requires pgl::Point vertices");

    /** @brief Creates the empty arrangement: no cell but the unbounded face. */
    Arrangement() {
        buildFaces();
    }

    /**
     * @brief Builds the arrangement of a range of shapes.
     *
     * Accepted shapes are the bounded ones: a @ref Point (an isolated vertex),
     * a @ref Segment or @ref OrientedSegment, a @ref Polyline or
     * @ref MonotoneChain, a @ref Polygon, @ref Triangle, @ref Rectangle,
     * @ref Convex or @ref PolygonWithHoles (their boundaries), and a
     * @ref Shape variant holding any of those. Everything else — a line, a ray,
     * a half-plane, a disk — is rejected at compile time.
     *
     * The input may be as degenerate as it likes short of being unbounded:
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
     *
     * @tparam ShapeRange Range of shapes.
     * @param shapes Shapes whose subdivision of the plane to compute.
     */
    template <std::ranges::input_range ShapeRange>
    explicit Arrangement(const ShapeRange& shapes) {
        std::vector<InputSegment> segments;
        std::vector<PointType> isolated;
        collect(shapes, segments, isolated);
        build(segments, isolated);
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
        std::vector<PointType> isolated;
        collect(shapes, segments, isolated);
        for (const auto& point : points) {
            isolated.emplace_back(point);
        }
        build(segments, isolated);
    }

    // -------------------------------------------------------------------------
    // Cells

    /** @brief Returns the number of vertices. */
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

    /** @brief Returns the number of faces, the unbounded one included. */
    [[nodiscard]] std::size_t faceCount() const {
        return outerCycle_.size();
    }

    /** @brief Returns the range of every @ref VertexId, in index order. */
    [[nodiscard]] auto vertices() const {
        return handles<VertexId>(points_.size());
    }

    /** @brief Returns the range of every @ref HalfedgeId, in index order. */
    [[nodiscard]] auto halfedges() const {
        return handles<HalfedgeId>(origin_.size());
    }

    /** @brief Returns the range of every @ref FaceId, in index order. */
    [[nodiscard]] auto faces() const {
        return handles<FaceId>(outerCycle_.size());
    }

    /**
     * @brief Returns the position of a vertex.
     *
     * @param v Vertex handle.
     */
    [[nodiscard]] const PointType& operator[](VertexId v) const {
        assert(v.valid() && v.index() < points_.size());
        return points_[v.index()];
    }

    /**
     * @brief Returns the edge a halfedge belongs to, carrying the edge label.
     *
     * Twin halfedges return the same segment: a @ref Segment is unoriented, so
     * this is the edge, not the directed halfedge. Use @ref origin and
     * @ref target for the direction.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] SegmentType operator[](HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        SegmentType segment(points_[origin_[h.index()]], points_[origin_[h.index() ^ 1]]);
        if constexpr (detail::has_label_v<TLabel>) {
            segment.label() = edgeLabel_[h.index() / 2];
        }
        return segment;
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
    [[nodiscard]] VertexId origin(HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        return VertexId(origin_[h.index()]);
    }

    /**
     * @brief Returns the vertex a halfedge arrives at.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] VertexId target(HalfedgeId h) const {
        return origin(twin(h));
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

    // -------------------------------------------------------------------------
    // The uniform cell interface

    /**
     * @brief Returns the vertex itself, as the witness of a zero-dimensional cell.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @param v Vertex handle.
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
        const PointType& a = points_[origin_[h.index()]];
        const PointType& b = points_[origin_[h.index() ^ 1]];
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
     * @brief Tells whether a face is the unbounded one.
     *
     * @param f Face handle.
     */
    [[nodiscard]] bool isUnbounded(FaceId f) const {
        assert(f.valid() && f.index() < outerCycle_.size());
        return f.index() == 0;
    }

    /**
     * @brief Tells whether a halfedge is an artefact of clipping unbounded input.
     *
     * Always false: unbounded input is not accepted yet. The accessor exists so
     * that accepting it later does not change this interface.
     *
     * @param h Halfedge handle.
     */
    [[nodiscard]] bool isFictitious([[maybe_unused]] HalfedgeId h) const {
        assert(h.valid() && h.index() < origin_.size());
        return false;
    }

    /**
     * @brief Returns a halfedge of the face's outer boundary cycle, which runs
     *        counterclockwise, or the invalid handle for the unbounded face.
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
     * the input stranded inside it, or, for the unbounded face, a whole
     * connected component of the input.
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
     * @pre The face is bounded (@ref isUnbounded is false).
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber>> polygon(FaceId f) const {
        assert(f.valid() && !isUnbounded(f));
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

    // -------------------------------------------------------------------------
    // Location

    /**
     * @brief Returns the face containing a point.
     *
     * A point on an edge or on a vertex belongs to no face; the answer is then
     * the face that an infinitesimal displacement of the query point towards
     * `-x` (and, if that is not enough to leave the boundary, towards `+y`)
     * lands in.
     *
     * The search is a linear scan over the edges. That is enough for the
     * occasional query; a query-heavy caller wants a point-location structure,
     * which this class does not have yet.
     *
     * @param p Query point.
     */
    [[nodiscard]] FaceId locate(const PointType& p) const {
        const HalfedgeId h = halfedgeLeftOf(p);
        return h.valid() ? face(h) : FaceId(0);
    }

private:
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

    template <class Id>
    static auto handles(std::size_t count) {
        return std::views::iota(std::uint32_t{0}, static_cast<std::uint32_t>(count)) |
               std::views::transform([](std::uint32_t i) { return Id(i); });
    }

    // -------------------------------------------------------------------------
    // Input normalization

    // Splits a shape range into the segments it contributes and the points it
    // reduces to, keeping each shape's position in the range as its origin.
    template <class ShapeRange>
    static void collect(const ShapeRange& shapes, std::vector<InputSegment>& segments,
                        std::vector<PointType>& isolated) {
        std::uint32_t index = 0;
        for (const auto& shape : shapes) {
            append(shape, index, segments, isolated);
            ++index;
        }
    }

    // The shapes the arrangement can be built from: the bounded ones, which are
    // exactly those whose whole point set is covered by finitely many segments.
    template <class InputShape>
    static constexpr bool isBounded =
        detail::is_empty_shape_v<InputShape> || detail::is_point_v<InputShape> ||
        detail::is_segment_v<InputShape> || detail::is_oriented_segment_v<InputShape> ||
        detail::is_polyline_v<InputShape> || detail::is_monotone_chain_v<InputShape> ||
        detail::is_triangle_v<InputShape> || detail::is_rectangle_v<InputShape> ||
        detail::is_convex_v<InputShape> || detail::is_polygon_v<InputShape> ||
        detail::is_polygon_with_holes_v<InputShape>;

    // Appends the cut segments — and the isolated points — of one input shape.
    template <class InputShape>
    static void append(const InputShape& shape, std::uint32_t index,
                       std::vector<InputSegment>& segments, std::vector<PointType>& isolated) {
        static_assert(isBounded<InputShape> || detail::is_shape_v<InputShape>,
                      "Arrangement accepts bounded shapes only: a point, a segment, a chain, or a "
                      "shape bounded by segments. Lines, rays, half-planes, disks and their "
                      "intersections are not supported yet.");
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
        };

        if constexpr (detail::is_shape_v<InputShape>) {
            std::visit([&](const auto& alternative) {
                // Every alternative of the variant is instantiated, so the
                // unbounded ones can only be rejected at run time.
                if constexpr (isBounded<std::remove_cvref_t<decltype(alternative)>>) {
                    append(alternative, index, segments, isolated);
                } else {
                    assert(false && "Arrangement does not accept an unbounded shape");
                }
            }, shape.variant());
        } else if constexpr (detail::is_empty_shape_v<InputShape>) {
            (void)shape;
        } else if constexpr (detail::is_point_v<InputShape>) {
            isolated.emplace_back(shape);
        } else if constexpr (detail::is_segment_v<InputShape> ||
                             detail::is_oriented_segment_v<InputShape>) {
            addSegment(shape);
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

    void build(std::vector<InputSegment>& segments, std::vector<PointType>& isolated) {
        std::vector<Piece> pieces = split(segments, isolated);
        internVertices(pieces, isolated);
        wireHalfedges();
        buildFaces();
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
        for (std::size_t i = 0; i < count; ++i) {
            cuts[i].push_back(segments[group[i]].segment.min());
            cuts[i].push_back(segments[group[i]].segment.max());
        }

        const auto meet = [&](std::size_t a, std::size_t b) {
            const auto piece = segments[group[a]].segment.template intersection<NumberType>(
                segments[group[b]].segment);
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

        // The active list is compacted by the same pass that tests it, so a
        // group is dropped exactly once and expiry costs nothing beyond the
        // comparison the test needed anyway.
        std::vector<std::uint32_t> active;
        for (const std::uint32_t current : order) {
            const NumberType& left = segments[group[current]].segment.min().x();
            std::size_t write = 0;
            for (std::size_t read = 0; read < active.size(); ++read) {
                const std::uint32_t other = active[read];
                if (right[other] < left) {
                    continue;  // its projection closed before this one opened
                }
                active[write++] = other;
                if (high[other] < low[current] || high[current] < low[other]) {
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
        for (std::uint32_t h = 0; h < origin_.size(); h += 2) {
            const PointType& a = points_[origin_[h]];
            const PointType& b = points_[origin_[h + 1]];
            const bool aBelow = !(a.y() > p.y());
            if (aBelow == !(b.y() > p.y())) {
                continue;  // both ends on the same side: no crossing
            }
            const PointType& low = aBelow ? a : b;
            const PointType& high = aBelow ? b : a;
            // The crossing abscissa as the fraction numerator / denominator,
            // with a positive denominator; no division, so integer coordinates
            // stay exact.
            const NumberType denominator = high.y() - low.y();
            const NumberType numerator =
                low.x() * denominator + (p.y() - low.y()) * (high.x() - low.x());
            if (!(numerator < p.x() * denominator)) {
                continue;  // to the right of the query point, or through it
            }
            if (best.valid()) {
                const NumberType here = numerator * bestDenominator;
                const NumberType there = bestNumerator * denominator;
                if (here < there) {
                    continue;
                }
                if (here == there) {
                    // Both edges cross at the same point, so that point is a
                    // vertex and both leave it upwards. Infinitesimally higher,
                    // the one leaning further right comes first.
                    const PointType& bestLow = points_[origin_[best.index() ^ 1]];
                    const PointType& bestHigh = points_[origin_[best.index()]];
                    const NumberType cross =
                        (high.x() - low.x()) * (bestHigh.y() - bestLow.y()) -
                        (high.y() - low.y()) * (bestHigh.x() - bestLow.x());
                    if (!(cross > NumberType(0))) {
                        continue;
                    }
                }
            }
            // The halfedge running downwards has the crossing's right-hand side,
            // where the query point lies, on its left.
            best = HalfedgeId(aBelow ? h + 1 : h);
            bestNumerator = numerator;
            bestDenominator = denominator;
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
        if (asked * edges > perComparison * (2 * edges + asked) * depth) {
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

    std::vector<PointType> points_;
    std::vector<HalfedgeId> outgoing_;         // one per vertex; invalid when isolated
    std::vector<std::uint32_t> origin_;        // one per halfedge
    std::vector<std::uint32_t> next_;          // one per halfedge
    std::vector<std::uint32_t> face_;          // one per halfedge
    std::vector<TLabel> edgeLabel_;            // one per edge
    std::vector<std::uint32_t> originOffset_;  // one per edge, plus a closing entry
    std::vector<std::uint32_t> originIndex_;   // input positions, grouped by edge
    std::vector<HalfedgeId> outerCycle_;       // one per face; invalid for the unbounded one
    std::vector<std::uint32_t> innerOffset_;   // one per face, plus a closing entry
    std::vector<HalfedgeId> innerCycle_;       // inner cycles, grouped by face
    std::vector<TLabel> faceLabel_;            // one per face
};

// No deduction guide, deliberately: the vertex type cannot be read off the
// input, because the input's own type is usually the wrong answer. Two integral
// segments cross at a rational point, so a guide reading the type off the input
// would silently pick a vertex type that cannot hold the vertices it is about to
// compute. What `Arrangement(shapes)` deduces instead is the default vertex
// type, which is exact whatever the input is; a caller wanting another one, or
// wanting the edges to carry the input's labels, names it.

}  // namespace pgl
