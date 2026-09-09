#pragma once
//
// Shared plumbing for the CGAL baseline drivers.
//
// The baseline exists to do two things the benchmark cannot do for itself:
// check that pgl's answers are right, by comparing a numeric result signature
// against an independent implementation rather than against more pgl; and put a
// reference curve on the chart. It is opt-in (`run_asymptotic.py --baseline`),
// never part of a recorded run, and stores a single overwritten snapshot rather
// than a history — CGAL is not guaranteed present on a CI box or on every dev
// machine, and a reference point is not something to track commit over commit.
//
// The operands come from ../datasets.hpp — the same generators, the same seeds,
// the same shapes the pgl drivers measured. A CGAL comparison on a differently
// generated input measures nothing at all, so the conversions below are the
// whole point of this header: they take the integer dataset and hand CGAL
// exactly what pgl got.
//
// Rows are keyed on (category, dataset, problem) alone. CGAL is not one of the
// cube's selectable values: it is not an algorithm the user picks, so its rows
// carry the CGAL entry point in the algorithm column and the kernel's name in
// the number column, and the dashboard overlays them on whichever algorithm is
// selected.
//
// ---------------------------------------------------------------------------
// Two kernels, and which one a driver is entitled to
// ---------------------------------------------------------------------------
//
// pgl's number axis has an `int` column and an `ERational` column, and charging
// both against one CGAL kernel is not a fair comparison in either direction.
// So the baseline offers both of CGAL's, and the rule for which a driver may
// use is the rule CGAL itself states.
//
// EPECK — exact predicates, exact constructions — is the analogue of pgl's
// ERational, and it is *required*, not merely safer, wherever CGAL constructs
// new geometry and then feeds it back into a predicate: the intersection points
// of an arrangement or a sweep, the boundary of a Minkowski sum or a Boolean
// union, the window endpoints of a visibility region. Rounding those to double
// can make the algorithm's own decisions inconsistent, so those drivers use
// EPECK for every column, `int` included.
//
// EPICK — exact predicates, inexact constructions — is the analogue of pgl's
// `int`, and is CGAL's canonical kernel for the predicate-only structures:
// convex hulls, Delaunay triangulations, kd-trees, AABB trees. Nothing there
// constructs a point that is later tested; every decision is an orientation,
// an in-circle, a coordinate comparison or a squared distance evaluated on the
// input coordinates themselves. Those are exact under EPICK's filters whenever
// the operands are exactly representable as doubles, which the datasets are by
// construction: ../../randomshapes.hpp draws integer coordinates in
// [-10000, 10000], so an orientation determinant stays under 10^9 and a squared
// distance under 10^9 — both far inside double's exact range, so the filter
// never even has to fall back.
//
// The difference the choice makes is not a few percent of predicate cost. An
// EPECK `Search_traits_2` or `AABB_traits_2` stores `Lazy_exact_nt`
// coordinates: every point in the kd-tree and every primitive in the AABB tree
// carries an interval and a DAG node, against two machine words for pgl's `int`
// tree. Measuring pgl's `int` column against that was measuring CGAL paying for
// exactness the row never asked for.
//
#include "../harness.hpp"
#include "../datasets.hpp"

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

#include <sstream>
#include <string>
#include <vector>

namespace bench::cgal {

using Kernel      = CGAL::Exact_predicates_exact_constructions_kernel;
using Inexact     = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point       = Kernel::Point_2;
using PolygonType = CGAL::Polygon_2<Kernel>;

// The number column of a baseline row: the kernel that produced it. EPECK is
// the default because a driver that constructs geometry has no other choice;
// see the header comment for which drivers may also run EPICK.
template <class K> struct KernelName;
template <> struct KernelName<Kernel>  { static constexpr const char* value = "EPECK"; };
template <> struct KernelName<Inexact> { static constexpr const char* value = "EPICK"; };

template <class K>
constexpr const char* numberName = KernelName<K>::value;

constexpr const char* kNumber = numberName<Kernel>;

// The pgl number type a kernel is the reference for. Only the dual-kernel
// drivers consult this: it is what lets `--type int` narrow the baseline the
// same way it narrows a pgl driver, so a development loop over one column does
// not have to run the other kernel to see its reference.
template <class K> struct KernelFor;
template <> struct KernelFor<Kernel>  { static constexpr const char* value = "ERational"; };
template <> struct KernelFor<Inexact> { static constexpr const char* value = "int"; };

/** Whether `--type` (a pgl type name, or a kernel name) selects this kernel. */
template <class K>
inline bool selected(const Options& opt) {
    return matches(opt.type, numberName<K>) || matches(opt.type, KernelFor<K>::value);
}

template <class K = Kernel>
inline typename K::Point_2 point(const IntPoint& p) {
    return typename K::Point_2(p.x(), p.y());
}

template <class K = Kernel>
inline std::vector<typename K::Point_2> points(const std::vector<IntPoint>& in) {
    std::vector<typename K::Point_2> out;
    out.reserve(in.size());
    for (const auto& p : in) {
        out.push_back(point<K>(p));
    }
    return out;
}

// pgl stores a polygon's vertices in canonical form, which may be clockwise;
// CGAL's Boolean and Minkowski operations want a counter-clockwise ring, so the
// orientation is fixed here rather than in each driver.
template <class K = Kernel>
inline CGAL::Polygon_2<K> polygon(const IntPolygon& in) {
    CGAL::Polygon_2<K> out;
    for (const auto& v : in.vertices()) {
        out.push_back(point<K>(v));
    }
    if (out.is_clockwise_oriented()) {
        out.reverse_orientation();
    }
    return out;
}

/** Total number of vertices over a CGAL region's outer ring and holes. */
template <class K>
inline long long vertexCount(const CGAL::Polygon_with_holes_2<K>& region) {
    long long total = static_cast<long long>(region.outer_boundary().size());
    for (const auto& hole : region.holes()) {
        total += static_cast<long long>(hole.size());
    }
    return total;
}

// An exact pgl coordinate, handed to CGAL through its decimal numerator and
// denominator rather than through a double, so nothing is lost on the way in.
// Exact constructions only: a kernel that rounds has nothing to do with an
// operand that was kept exact all the way here.
inline Kernel::FT exact(const pgl::ERational& value) {
    std::ostringstream numerator, denominator;
    numerator << value.numerator();
    denominator << value.denominator();
    return Kernel::FT(Kernel::FT::ET(numerator.str() + "/" + denominator.str()));
}

inline Point exactPoint(const pgl::EPoint& p) {
    return Point(exact(p.x()), exact(p.y()));
}

}  // namespace bench::cgal
