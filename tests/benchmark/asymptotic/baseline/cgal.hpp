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
// cube's selectable values: it is not an algorithm the user picks and it has no
// place on the number-type axis, so its rows carry the CGAL entry point in the
// algorithm column and EPECK in the number column, and the dashboard overlays
// them on whichever algorithm and type happen to be selected.
//
#include "../harness.hpp"
#include "../datasets.hpp"

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

#include <sstream>
#include <string>
#include <vector>

namespace bench::cgal {

using Kernel      = CGAL::Exact_predicates_exact_constructions_kernel;
using Point       = Kernel::Point_2;
using PolygonType = CGAL::Polygon_2<Kernel>;

// The number column of every baseline row: CGAL's exact-predicates,
// exact-constructions kernel, the closest thing it has to pgl's ERational.
constexpr const char* kNumber = "EPECK";

inline Point point(const IntPoint& p) {
    return Point(p.x(), p.y());
}

inline std::vector<Point> points(const std::vector<IntPoint>& in) {
    std::vector<Point> out;
    out.reserve(in.size());
    for (const auto& p : in) {
        out.push_back(point(p));
    }
    return out;
}

// pgl stores a polygon's vertices in canonical form, which may be clockwise;
// CGAL's Boolean and Minkowski operations want a counter-clockwise ring, so the
// orientation is fixed here rather than in each driver.
inline PolygonType polygon(const IntPolygon& in) {
    PolygonType out;
    for (const auto& v : in.vertices()) {
        out.push_back(point(v));
    }
    if (out.is_clockwise_oriented()) {
        out.reverse_orientation();
    }
    return out;
}

/** Total number of vertices over a CGAL region's outer ring and holes. */
inline long long vertexCount(const CGAL::Polygon_with_holes_2<Kernel>& region) {
    long long total = static_cast<long long>(region.outer_boundary().size());
    for (const auto& hole : region.holes()) {
        total += static_cast<long long>(hole.size());
    }
    return total;
}

// An exact pgl coordinate, handed to CGAL through its decimal numerator and
// denominator rather than through a double, so nothing is lost on the way in.
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
