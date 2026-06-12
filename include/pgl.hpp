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

#include "core/forward.hpp"
#include "core/numeric.hpp"
#include "core/bigint.hpp"
#include "core/rational.hpp"
#include "geometry/point.hpp"
#include "geometry/emptyshape.hpp"
#include "implementation/orientation.hpp"
#include "geometry/segment.hpp"
#include "geometry/orientedsegment.hpp"
#include "geometry/line.hpp"
#include "geometry/orientedline.hpp"
#include "geometry/ray.hpp"
#include "geometry/halfplane.hpp"
#include "geometry/rectangle.hpp"
#include "geometry/triangle.hpp"
#include "geometry/disk.hpp"
#include "geometry/convex.hpp"
#include "geometry/polygon.hpp"
#include "geometry/shape.hpp"
#include "implementation/io.hpp"
#include "implementation/transformations.hpp"
#include "implementation/measures.hpp"
#include "implementation/bounding.hpp"
#include "implementation/duality.hpp"
#include "implementation/predicates.hpp"
#include "implementation/atxy.hpp"
#include "implementation/intersection.hpp"
#include "implementation/distance.hpp"
#include "visualization/canvas.hpp"
#include "core/hash.hpp"
#include "algorithm/intersections.hpp"
#include "algorithm/convexhull.hpp"
