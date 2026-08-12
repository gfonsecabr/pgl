#pragma once

/**
 * @file generators.hpp
 * @brief Shape generators, and the shrinkable case they are driven by.
 *
 * ## The case model
 *
 * Everything the harness feeds a property is derived from one plain
 * `std::vector<Point>` plus the index of the @ref Generator that consumes it.
 * That indirection is what makes shrinking possible: a witness is not an opaque
 * shape but the small list of lattice points it was built from, so the shrinker
 * can perturb *coordinates* and rebuild, without knowing anything about the
 * shape family involved (see `shrink.hpp`).
 *
 * A generator may refuse its input by returning `false`. Two reasons come up:
 *
 *  - The points give an **undefined** shape — `Segment` with equal endpoints,
 *    `Disk` through three distinct collinear points, and so on. `isUndefined()`
 *    is the one state in pgl that carries no contract at all, so a property
 *    asserted about it would be asserting about unspecified behaviour. Every
 *    generator therefore ends with an `isUndefined()` rejection. Note the
 *    contrast with merely **degenerate** shapes — a collinear `Triangle`, a
 *    zero-radius `Disk`, a collapsed `Rectangle` — which are defined, have
 *    contracted limit-case behaviour, and are exactly the inputs this harness
 *    exists to hammer. They are generated on purpose and never rejected.
 *  - The points violate a **structural precondition** the class does not check
 *    for itself: `Polygon` wants a simple ring, `PolygonWithHoles` and
 *    `PolygonSet` want the nesting and disjointness that `isValid()` describes.
 *    Where a cheap checker exists it is called and used to reject; where none
 *    exists the construction route is chosen so that validity is structural (a
 *    convex hull is always a simple ring, a polyomino region is always valid).
 *
 * ## Coordinates
 *
 * Generation is over `int` lattice points in a small box (the `--grid` option).
 * Small coordinates are not a limitation but the point: on a 13x13 grid,
 * collinear triples, coincident vertices, shared edges and touching boundaries
 * all turn up by chance within a few hundred draws, and those are the cases
 * where the predicate contracts are subtle enough to get wrong.
 */

#include "pgl.hpp"

#include "rng.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace pglprop {

/** @brief Coordinate type every generated shape is built over. */
using Coord = int;
/** @brief Vertex type of every generated shape. */
using PointShape = pgl::Point<Coord>;
/** @brief The runtime-polymorphic shape the properties are written against. */
using AnyShape = pgl::Shape<PointShape>;

/**
 * @brief Exact coordinate used wherever a construction has to divide.
 *
 * Constructions — intersections, boolean operations, Minkowski sums — generally
 * leave the integer lattice, and asking for an integral result there would
 * truncate. The properties that compare constructions against each other
 * therefore run in this type, where the comparison is exact and any mismatch is
 * a real one rather than a rounding artefact.
 */
using Exact = pgl::Rational<pgl::BigInt>;
/** @brief Vertex type of an exactly-computed result. */
using ExactPoint = pgl::Point<Exact>;
/** @brief Wrapper for an exactly-computed result. */
using ExactShape = pgl::Shape<ExactPoint>;

/** @brief Static facts about a generator's output, used to select properties. */
enum GeneratorTag : unsigned {
    /** @brief No special capability. */
    kNoTag = 0u,
    /**
     * @brief Output satisfies `PolygonalRegionConcept`.
     *
     * The regularized boolean operations are closed over exactly these —
     * `Rectangle`, `Triangle`, `Convex`, `Polygon`, `PolygonWithHoles`,
     * `PolygonSet` — and throw for every other alternative, so the boolean
     * properties select on this tag rather than catching the throw.
     */
    kRegion = 1u << 0,
    /**
     * @brief `pgl::Transformation` can be applied to the output.
     *
     * True for every alternative except `Rectangle` and `Disk`: a general
     * affine map turns a rectangle into a parallelogram and a disk into an
     * ellipse, neither of which those classes can represent, so applying one
     * throws. The transformation-invariance property still covers them through
     * the shape's own `rotated90` / translate / scale operators, which are
     * closed on every alternative.
     */
    kAffine = 1u << 1,
    /**
     * @brief The value does not depend on the orientation of the axes.
     *
     * True for every alternative but `MonotoneChain`, whose identity *is* the
     * lexicographic order of its vertices: it stores a weakly x-monotone chain,
     * and the quarter-turn or shear of an x-monotone chain is generally not
     * x-monotone, so the class cannot represent its own image. Rotating one
     * necessarily re-sorts the vertices and yields the chain *through the rotated
     * points*, which is a different curve from the rotation of the original
     * curve. That is inherent to the type rather than a defect, so the
     * rotation and shear invariance properties require this tag.
     *
     * Translation and scaling by a positive factor preserve the x-order, and
     * negation reverses it — which re-sorts to the reversed traversal, the same
     * curve. So those three stay applicable to a chain, and only the two maps
     * that genuinely change it are held back.
     */
    kAxisFree = 1u << 2,
    /**
     * @brief Output is a `Convex`.
     *
     * Lets a property that is only defined for convex operands be *selected* for
     * rather than skipped inside: a runtime `getIfConvex` guard would leave the
     * property vacuous at any reasonable case count, one pair in several hundred
     * being convex on both sides.
     */
    kConvexAlternative = 1u << 3,
};

/**
 * @brief One way of turning a list of lattice points into a shape.
 *
 * @see generators() for the table of them.
 */
struct Generator {
    /** @brief Name used in reports, witnesses and `--generator` filters. */
    const char* name;
    /** @brief Fewest points the builder reads. */
    std::size_t minPoints;
    /** @brief Most points the builder reads. */
    std::size_t maxPoints;
    /** @brief Mask of @ref GeneratorTag. */
    unsigned tags;
    /**
     * @brief Builds the shape, or refuses the input.
     *
     * @param points Between @ref minPoints and @ref maxPoints lattice points.
     * @param out Receives the shape when the return value is `true`.
     * @return `false` if @p points give an undefined or structurally invalid
     *         shape, in which case @p out is untouched.
     */
    bool (*build)(const std::vector<PointShape>& points, AnyShape& out);
};

namespace detail {

// The library types, aliased away from the names <wingdi.h> takes as functions.
// `Polygon`, `Rectangle` and `Polyline` cannot be spelled as themselves in a
// test translation unit; see tests/unit/windows_traps.hpp.
using SegmentShape = pgl::Segment<PointShape>;
using OrientedSegmentShape = pgl::OrientedSegment<PointShape>;
using LineShape = pgl::Line<PointShape>;
using OrientedLineShape = pgl::OrientedLine<PointShape>;
using RayShape = pgl::Ray<PointShape>;
using HalfplaneShape = pgl::Halfplane<PointShape>;
using RectangleShape = pgl::Rectangle<PointShape>;
using TriangleShape = pgl::Triangle<PointShape>;
using DiskShape = pgl::Disk<PointShape>;
using ConvexShape = pgl::Convex<PointShape>;
using MonotoneChainShape = pgl::MonotoneChain<PointShape>;
using PolylineShape = pgl::Polyline<PointShape>;
using PolygonShape = pgl::Polygon<PointShape>;
using HalfplaneIntersectionShape = pgl::HalfplaneIntersection<PointShape>;
using RegionShape = pgl::PolygonWithHoles<PointShape>;
using RegionSetShape = pgl::PolygonSet<PointShape>;

/**
 * @brief Accepts a shape unless it is undefined.
 *
 * The single choke point for the `isUndefined()` rejection every generator
 * ends with, so the rule is stated once.
 */
template <class T>
bool accept(const T& shape, AnyShape& out) {
    if (shape.isUndefined()) {
        return false;
    }
    out = shape;
    return true;
}

inline bool buildEmpty(const std::vector<PointShape>&, AnyShape& out) {
    out = pgl::EmptyShape<PointShape>();
    return true;
}

inline bool buildPoint(const std::vector<PointShape>& p, AnyShape& out) {
    out = p[0];
    return true;
}

inline bool buildSegment(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(SegmentShape(p[0], p[1]), out);
}

inline bool buildOrientedSegment(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(OrientedSegmentShape(p[0], p[1]), out);
}

inline bool buildLine(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(LineShape(p[0], p[1]), out);
}

inline bool buildOrientedLine(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(OrientedLineShape(p[0], p[1]), out);
}

inline bool buildRay(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(RayShape(p[0], p[1]), out);
}

inline bool buildHalfplane(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(HalfplaneShape(p[0], p[1]), out);
}

inline bool buildRectangle(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(RectangleShape(p[0], p[1]), out);
}

// The empty rectangle is a distinct generator rather than a rare draw of the
// one above: the empty set is the operand every predicate has a special clause
// for, so it deserves to appear in a fixed fraction of the cases.
inline bool buildEmptyRectangle(const std::vector<PointShape>&, AnyShape& out) {
    return accept(RectangleShape(), out);
}

inline bool buildTriangle(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(TriangleShape(p[0], p[1], p[2]), out);
}

inline bool buildDiskFromBoundary(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(DiskShape(p[0], p[1], p[2]), out);
}

// Radius taken from a coordinate rather than drawn separately, so that the
// shrinker reaches the zero-radius (single point) disk by the same coordinate
// reduction it uses everywhere else.
inline bool buildDiskFromRadius(const std::vector<PointShape>& p, AnyShape& out) {
    const Coord radius = p[1].x() < 0 ? -p[1].x() : p[1].x();
    return accept(DiskShape(p[0], radius), out);
}

inline bool buildConvex(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(ConvexShape(p), out);
}

inline bool buildMonotoneChain(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(MonotoneChainShape(p), out);
}

inline bool buildPolyline(const std::vector<PointShape>& p, AnyShape& out) {
    return accept(PolylineShape(p), out);
}

// A hull is a simple ring by construction, which is what `Polygon` needs and
// does not check. Degenerate hulls (all points collinear) collapse to a polygon
// that is a segment or a point -- defined, and worth generating.
inline bool buildPolygonFromHull(const std::vector<PointShape>& p, AnyShape& out) {
    const ConvexShape hull(p);
    if (hull.empty()) {
        return false;
    }
    return accept(hull.asPolygon(), out);
}

// The concave counterpart: take the points in the order drawn and keep the ring
// only when it happens to be simple. Rejection is frequent and cheap, and it is
// the only route here that produces reflex vertices at arbitrary angles.
inline bool buildPolygonSimpleRing(const std::vector<PointShape>& p, AnyShape& out) {
    const PolygonShape ring(p);
    if (ring.isUndefined() || !ring.isSimple()) {
        return false;
    }
    out = ring;
    return true;
}

/**
 * @brief Returns the free polyomino regions of up to seven cells.
 *
 * Rectilinear, always valid, and — from seven cells up — the cheapest source of
 * a region with a genuine hole. Enumerated once and cached; the enumeration is
 * deterministic, so indexing into it is reproducible.
 */
inline const std::vector<RegionShape>& polyominoCatalog() {
    static const std::vector<RegionShape> catalog = pgl::polyominoRegions<Coord>(1, 7);
    return catalog;
}

// Indexed by a coordinate so that shrinking a coordinate walks the catalog
// toward its small, simple entries.
inline bool buildPolyominoRegion(const std::vector<PointShape>& p, AnyShape& out) {
    const std::vector<RegionShape>& catalog = polyominoCatalog();
    if (catalog.empty()) {
        return false;
    }
    const Coord raw = p[0].x() < 0 ? -p[0].x() : p[0].x();
    const std::size_t pick = static_cast<std::size_t>(raw) % catalog.size();
    RegionShape region = catalog[pick];
    region += p[1];  // Translate, so the catalog is not pinned to the origin.
    return accept(region, out);
}

inline bool buildRegionFromRing(const std::vector<PointShape>& p, AnyShape& out) {
    const ConvexShape hull(p);
    if (hull.empty() || hull.isDegenerate()) {
        return false;
    }
    const RegionShape region(hull.asPolygon());
    if (!region.isValid()) {
        return false;
    }
    return accept(region, out);
}

// A rectangle with a rectangular bite taken out of it. Axis-aligned operands
// are the one case where a boolean operation stays on the integer lattice
// exactly, so an `int` result type here is not a truncation.
inline bool buildRegionFromDifference(const std::vector<PointShape>& p, AnyShape& out) {
    const RectangleShape outer(p[0], p[1]);
    const RectangleShape bite(p[2], p[3]);
    const RegionSetShape carved = outer.template difference<Coord>(bite);
    if (carved.componentCount() != 1) {
        return false;
    }
    const RegionShape region = carved.component(0);
    if (!region.isValid()) {
        return false;
    }
    return accept(region, out);
}

inline bool buildRegionSetFromUnion(const std::vector<PointShape>& p, AnyShape& out) {
    const RectangleShape first(p[0], p[1]);
    const RectangleShape second(p[2], p[3]);
    const RegionSetShape united = first.template regularizedUnion<Coord>(second);
    if (!united.isValid()) {
        return false;
    }
    return accept(united, out);
}

inline bool buildRegionSetFromDifference(const std::vector<PointShape>& p, AnyShape& out) {
    const RectangleShape outer(p[0], p[1]);
    const RectangleShape bite(p[2], p[3]);
    const RegionSetShape carved = outer.template difference<Coord>(bite);
    if (!carved.isValid()) {
        return false;
    }
    return accept(carved, out);
}

// Points are consumed in pairs, each pair one half-plane, so the region may
// come out bounded, unbounded or empty -- all three are wanted.
inline bool buildHalfplaneIntersection(const std::vector<PointShape>& p, AnyShape& out) {
    std::vector<HalfplaneShape> halfplanes;
    for (std::size_t i = 0; i + 1 < p.size(); i += 2) {
        const HalfplaneShape h(p[i], p[i + 1]);
        if (h.isUndefined()) {
            return false;
        }
        halfplanes.push_back(h);
    }
    if (halfplanes.empty()) {
        return false;
    }
    return accept(HalfplaneIntersectionShape(halfplanes), out);
}

inline bool buildHalfplaneIntersectionBox(const std::vector<PointShape>& p, AnyShape& out) {
    const RectangleShape box(p[0], p[1]);
    if (box.empty()) {
        return false;
    }
    return accept(HalfplaneIntersectionShape(box), out);
}

}  // namespace detail

/**
 * @brief Returns the generator table.
 *
 * Every alternative of `pgl::Shape`'s variant is reachable, most by more than
 * one route: a `Polygon` arrives as a hull, as an arbitrary simple ring or as a
 * polyomino, and the three have systematically different shapes (convex,
 * reflex-with-odd-angles, rectilinear-with-collinear-vertices). Coverage of the
 * *alternatives* is what the pair matrix needs; coverage of the *routes* is
 * what finds the cases where one construction path normalizes differently from
 * another.
 */
inline const std::vector<Generator>& generators() {
    static const std::vector<Generator> table = {
        {"EmptyShape", 0, 0, kAffine | kAxisFree, detail::buildEmpty},
        {"Point", 1, 1, kAffine | kAxisFree, detail::buildPoint},
        {"Segment", 2, 2, kAffine | kAxisFree, detail::buildSegment},
        {"OrientedSegment", 2, 2, kAffine | kAxisFree, detail::buildOrientedSegment},
        {"Line", 2, 2, kAffine | kAxisFree, detail::buildLine},
        {"OrientedLine", 2, 2, kAffine | kAxisFree, detail::buildOrientedLine},
        {"Ray", 2, 2, kAffine | kAxisFree, detail::buildRay},
        {"Halfplane", 2, 2, kAffine | kAxisFree, detail::buildHalfplane},
        {"Rectangle", 2, 2, kRegion | kAxisFree, detail::buildRectangle},
        {"Rectangle.empty", 0, 0, kRegion | kAxisFree, detail::buildEmptyRectangle},
        {"Triangle", 3, 3, kRegion | kAffine | kAxisFree, detail::buildTriangle},
        {"Disk.boundary", 3, 3, kNoTag | kAxisFree, detail::buildDiskFromBoundary},
        {"Disk.radius", 2, 2, kNoTag | kAxisFree, detail::buildDiskFromRadius},
        {"Convex", 1, 6, kRegion | kAffine | kAxisFree | kConvexAlternative, detail::buildConvex},
        {"MonotoneChain", 1, 6, kAffine, detail::buildMonotoneChain},
        {"Polyline", 1, 6, kAffine | kAxisFree, detail::buildPolyline},
        {"Polygon.hull", 1, 6, kRegion | kAffine | kAxisFree, detail::buildPolygonFromHull},
        {"Polygon.ring", 3, 7, kRegion | kAffine | kAxisFree, detail::buildPolygonSimpleRing},
        {"PolygonWithHoles.polyomino", 2, 2, kRegion | kAffine | kAxisFree, detail::buildPolyominoRegion},
        {"PolygonWithHoles.ring", 3, 6, kRegion | kAffine | kAxisFree, detail::buildRegionFromRing},
        {"PolygonWithHoles.carved", 4, 4, kRegion | kAffine | kAxisFree, detail::buildRegionFromDifference},
        {"PolygonSet.union", 4, 4, kRegion | kAffine | kAxisFree, detail::buildRegionSetFromUnion},
        {"PolygonSet.carved", 4, 4, kRegion | kAffine | kAxisFree, detail::buildRegionSetFromDifference},
        {"HalfplaneIntersection", 2, 8, kAffine | kAxisFree, detail::buildHalfplaneIntersection},
        {"HalfplaneIntersection.box", 2, 2, kAffine | kAxisFree, detail::buildHalfplaneIntersectionBox},
    };
    return table;
}

/**
 * @brief One operand: the generator that made it and the points it read.
 *
 * Kept together because the pair is what the shrinker rewrites and what the
 * report prints; the shape itself is rebuilt from them on demand.
 */
struct Operand {
    /** @brief Index into @ref generators(). */
    std::size_t generator = 0;
    /** @brief Points the generator reads. */
    std::vector<PointShape> points;
};

/**
 * @brief Rebuilds the shape an operand describes.
 *
 * @param operand Generator index and points.
 * @param out Receives the shape when the return value is `true`.
 * @return Whatever the generator answered; `false` after shrinking has pushed
 *         the points into a state the generator refuses.
 */
inline bool buildOperand(const Operand& operand, AnyShape& out) {
    const Generator& generator = generators()[operand.generator];
    if (operand.points.size() < generator.minPoints) {
        return false;
    }
    return generator.build(operand.points, out);
}

/**
 * @brief Draws a fresh operand from a generator.
 *
 * @param rng Random source.
 * @param generatorIndex Which generator to draw for.
 * @param grid Coordinates are drawn uniformly from `[-grid, grid]`.
 */
inline Operand drawOperand(Rng& rng, std::size_t generatorIndex, int grid) {
    const Generator& generator = generators()[generatorIndex];
    Operand operand;
    operand.generator = generatorIndex;
    const std::size_t count = generator.minPoints +
        rng.index(generator.maxPoints - generator.minPoints + 1);

    // Every so often, draw from a box a third the size. Two operands drawn from
    // the *same* box almost never nest, which leaves every property conditioned
    // on containment — `contains-implies-intersects`,
    // `interior-containment-meets-interiors` and the rest — starved of the inputs
    // it exists to judge. Mixing the scales makes one shape land inside the other
    // often enough to exercise them.
    const int box = rng.chance(30) ? std::max(1, grid / 3) : grid;

    operand.points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        // Drawn into locals on purpose. As two arguments of one call the order of
        // evaluation would be unspecified, and g++ and clang++ choose differently
        // -- which silently swaps x and y between the two builds and makes the
        // same seed explore a different sequence. A witness reported by one build
        // would then not reproduce under the other, which is exactly when one
        // needs it to.
        const int x = rng.inRange(-box, box);
        const int y = rng.inRange(-box, box);
        operand.points.emplace_back(x, y);
    }
    return operand;
}

/**
 * @brief Renders an operand as the C++ that reproduces it.
 *
 * Printed for every reported violation, so that a witness can be pasted
 * straight into a unit test.
 */
inline std::string describeOperand(const Operand& operand) {
    std::string text = generators()[operand.generator].name;
    text += "{";
    for (std::size_t i = 0; i < operand.points.size(); ++i) {
        if (i > 0) {
            text += ", ";
        }
        text += "(" + std::to_string(operand.points[i].x()) + "," +
                std::to_string(operand.points[i].y()) + ")";
    }
    text += "}";
    return text;
}

/** @brief Returns the name of the alternative a shape currently holds. */
inline const char* alternativeName(const AnyShape& shape) {
    if (shape.isPoint()) return "Point";
    if (shape.isSegment()) return "Segment";
    if (shape.isOrientedSegment()) return "OrientedSegment";
    if (shape.isLine()) return "Line";
    if (shape.isOrientedLine()) return "OrientedLine";
    if (shape.isRay()) return "Ray";
    if (shape.isHalfplane()) return "Halfplane";
    if (shape.isRectangle()) return "Rectangle";
    if (shape.isTriangle()) return "Triangle";
    if (shape.isDisk()) return "Disk";
    if (shape.isConvex()) return "Convex";
    if (shape.isMonotoneChain()) return "MonotoneChain";
    if (shape.isPolyline()) return "Polyline";
    if (shape.isPolygon()) return "Polygon";
    if (shape.isHalfplaneIntersection()) return "HalfplaneIntersection";
    if (shape.isPolygonWithHoles()) return "PolygonWithHoles";
    if (shape.isPolygonSet()) return "PolygonSet";
    return "EmptyShape";
}

namespace detail {

/**
 * @brief Maps a shape type over `PointShape` to the same shape over
 *        @ref ExactPoint.
 *
 * pgl's shape templates take their vertex type as the first parameter, so the
 * rebind is one pattern match per template arity: `EmptyShape<P>`,
 * `Segment<P, Label>` and friends, and `MonotoneChain<P, Label, Storage>` whose
 * storage has to be rebound too. `Point` is deliberately *not* covered — its
 * first parameter is a number, not a vertex — and is handled separately by the
 * one caller.
 */
template <class T>
struct Rebound;

/** @brief Rebinds a one-parameter shape template, i.e. `EmptyShape`. */
template <template <class> class Template, class Vertex>
struct Rebound<Template<Vertex>> {
    using type = Template<ExactPoint>;
};

/** @brief Rebinds the vertex-and-label shape templates, which is most of them. */
template <template <class, class> class Template, class Vertex, class Label>
struct Rebound<Template<Vertex, Label>> {
    using type = Template<ExactPoint, Label>;
};

/** @brief Rebinds `MonotoneChain`, whose storage names the vertex type again. */
template <template <class, class, class> class Template, class Vertex, class Label, class Storage>
struct Rebound<Template<Vertex, Label, Storage>> {
    using type = Template<ExactPoint, Label, std::vector<ExactPoint>>;
};

}  // namespace detail

/**
 * @brief Re-expresses a shape over exact rational coordinates.
 *
 * The constructions compute in @ref Exact, so comparing their results against
 * the operands needs the operands in the same coordinate type. Every shape has
 * a converting constructor from a compatible vertex type; this visits the
 * variant to reach it.
 */
inline ExactShape toExact(const AnyShape& shape) {
    return std::visit(
        [](const auto& value) -> ExactShape {
            using Alternative = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Alternative, PointShape>) {
                return ExactShape(ExactPoint(value));
            } else if constexpr (std::is_same_v<Alternative, pgl::EmptyShape<PointShape>>) {
                // The empty set carries no coordinates to convert.
                return ExactShape(pgl::EmptyShape<ExactPoint>());
            } else {
                return ExactShape(typename detail::Rebound<Alternative>::type(value));
            }
        },
        shape.variant());
}

}  // namespace pglprop
