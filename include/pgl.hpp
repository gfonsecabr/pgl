#pragma once

/**
 * @file pgl.hpp
 * @brief Convenience umbrella header for the PGL library.
 */

#if defined(_MSVC_LANG)
#  define PGL_CPLUSPLUS _MSVC_LANG
#else
#  define PGL_CPLUSPLUS __cplusplus
#endif

#if PGL_CPLUSPLUS < 202002L
#  error "PGL requires C++20 or newer. Compile with -std=c++20 (or later)."
#endif

#undef PGL_CPLUSPLUS

// This umbrella simply includes every header in dependency order. Each header is
// self-contained: it includes its immediate predecessor in that order, which
// transitively pulls in everything it depends on. As a result a user may include
// any single header directly and get the same result as including pgl.hpp, while
// each header still parses cleanly on its own (no recursion through the umbrella).
#include "core/forward.hpp"
#include "core/numeric.hpp"
#include "core/bigint.hpp"
#include "core/rational.hpp"
#include "core/transformation.hpp"
#include "shape/point.hpp"
#include "shape/emptyshape.hpp"
#include "implementation/orientation.hpp"
#include "shape/segment.hpp"
#include "shape/orientedsegment.hpp"
#include "shape/line.hpp"
#include "shape/orientedline.hpp"
#include "shape/ray.hpp"
#include "shape/halfplane.hpp"
#include "shape/rectangle.hpp"
#include "shape/triangle.hpp"
#include "shape/disk.hpp"
#include "shape/convex.hpp"
#include "shape/monotonechain.hpp"
#include "shape/polyline.hpp"
#include "shape/polygon.hpp"
#include "shape/halfplaneintersection.hpp"
#include "shape/shape.hpp"
#include "implementation/io.hpp"
#include "implementation/transformations.hpp"
#include "implementation/measures.hpp"
#include "implementation/bounding.hpp"
#include "implementation/duality.hpp"
#include "implementation/predicates.hpp"
#include "implementation/atxy.hpp"
#include "implementation/intersection.hpp"
#include "implementation/distance.hpp"
#include "implementation/distancel1.hpp"
#include "implementation/distancelinf.hpp"
#include "visualization/canvas.hpp"
#include "core/hash.hpp"
#include "algorithm/intersections.hpp"
#include "algorithm/convexhull.hpp"
#include "algorithm/shapetree.hpp"
#include "algorithm/sortpoints.hpp"
#include "algorithm/polyominoes.hpp"
#include "algorithm/triangulation.hpp"

// -----------------------------------------------------------------------------
// Exact convenience aliases
//
// "E"-prefixed shorthands for the exact, overflow-free configuration: rationals
// backed by arbitrary-precision integers (Rational<BigInt>) over labelless
// shapes. Reach for these when coordinates may grow without bound (repeated
// intersections, constructions, duality) and correctness matters more than the
// speed of fixed-width arithmetic.
namespace pgl {

using ERational        = Rational<BigInt>;
using ETransformation  = Transformation<ERational>;
using EPoint           = Point<ERational>;
using EEmptyShape      = EmptyShape<EPoint>;
using ESegment         = Segment<EPoint>;
using EOrientedSegment = OrientedSegment<EPoint>;
using ELine            = Line<EPoint>;
using EOrientedLine    = OrientedLine<EPoint>;
using ERay             = Ray<EPoint>;
using EHalfplane       = Halfplane<EPoint>;
using ERectangle       = Rectangle<EPoint>;
using ETriangle        = Triangle<EPoint>;
using EDisk            = Disk<EPoint>;
using EConvex          = Convex<EPoint>;
using EPolygon         = Polygon<EPoint>;
using EMonotoneChain   = MonotoneChain<EPoint>;
using EPolyline        = Polyline<EPoint>;
using EHalfplaneIntersection = HalfplaneIntersection<EPoint>;
using EShape           = Shape<EPoint>;

}  // namespace pgl
