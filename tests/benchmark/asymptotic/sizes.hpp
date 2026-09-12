#pragma once
//
// The x axis of every asymptotic benchmark chart.
//
// These lists are fixed constants, calibrated once by hand and then left alone.
// They are deliberately *not* probed at runtime: if a driver derived its sizes
// from how fast the machine it happens to be running on turned out to be, then
// two runs of the same commit — or the same run on a slower CI box — would
// measure different x values, and overlaying one run's curve on another's,
// which is the entire point of the page, would be meaningless. A slower machine
// simply takes longer to measure the same sizes.
//
// Each list is generated from one number: its maximum. That maximum is the
// calibrated part — established by running the driver on the reference dev
// machine, with the note on each list saying what set it — and then written
// down; the 32 sample points are spaced evenly from a
// small floor up to it, which is what a linear-linear chart wants. Writing the
// maximum rather than 32 literals keeps the calibrated quantity visible and the
// list impossible to typo — the values are just as fixed either way, and change
// only when someone edits a maximum on purpose.
//
// **Anchoring.** Within a category the list is anchored to the expensive
// one-shot construction, and every cheaper problem in that category reuses it.
// A flat query curve beside a steep build curve is the intended picture, not a
// miscalibration: pushing a point-location query to a second on its own would
// need an input no machine can build. Categories with no single dominant
// construction get one list per problem or per dataset instead — several
// independent constructions (point constructions), or datasets whose cost
// differs by orders of magnitude at the same n (segment intersections, segment
// search, Minkowski sum, union).
//
// **What actually sets the ceiling.** Three different things do, and the notes
// below say which applies where:
//   * the anchor operation reaching about a second — the ordinary case;
//   * quadratic output — the segments are scattered over a disk of *fixed*
//     radius, so sweeping n sweeps the density too and the number of crossings
//     grows with n²; those categories are dominated by the output term, not by
//     their n log n structure, and their drivers record the output size in the
//     Output column so the term can be read from the data;
//   * dataset generation — a simple polygon of n vertices is built by
//     untangling n random points, which is superpolynomial in practice, so the
//     polygon datasets cap where generating one dominates the run — seconds of
//     untangling against milliseconds of algorithm — well before the algorithm
//     under test would have run out of room on its own.
//
// Those are the reasons a maximum was chosen, not a promise about where it sits
// today. The library keeps getting faster, so a list calibrated to the
// one-second anchor drifts under it until someone deliberately recalibrates;
// several already sit well under, and that is a thing to fix on purpose rather
// than a discrepancy to read as a bug.
//
// **Where the numbers are.** Not here. Every measured time, and the output size
// behind it, is recorded per commit in the benchmark history and drawn on the
// page. A time copied into a comment is wrong again the moment the library gets
// faster, and the ones that used to be here had gone stale by more than an
// order of magnitude. So the notes below carry only what does not perish: what
// sets each ceiling, what the list is anchored to, and which lists are tied to
// each other — plus the one cost no record holds, dataset generation, which
// happens outside the timed region and shows up only as wall clock.
//
// To recalibrate a maximum, sweep candidate sizes without recording them:
//
//     python3 tests/benchmark/run_asymptotic.py pointsearch --sizes 60000,90000
//
// A run that passed --sizes is refused by to_history.py, so calibration can
// never reach the charts.
//
#include <array>

namespace bench {

// Sample points per sweep.
constexpr int kSamples = 32;

// `kSamples` sizes rising evenly from a floor to `max`. The floor is one step
// below the second sample, so the sweep starts small enough to show the low-n
// behaviour without wasting samples on inputs too small to mean anything.
constexpr std::array<int, kSamples> linearSizes(int max) {
    const int floor = max / kSamples < 4 ? 4 : max / kSamples;
    std::array<int, kSamples> sizes{};
    for (int i = 0; i < kSamples; ++i) {
        sizes[static_cast<std::size_t>(i)] =
            floor + static_cast<int>((static_cast<long long>(max - floor) * i) / (kSamples - 1));
    }
    return sizes;
}

// ── 1. Triangulation ────────────────────────────────────────────────────────
// Anchored to the incremental build, the one construction here; both locate
// problems and the preprocessed point-location index reuse its list.
constexpr auto kTriangulation = linearSizes(100000);

// ── 2. Arrangement ──────────────────────────────────────────────────────────
// Quadratic output, so the arrangement outgrows its input. Anchored to
// buildPointLocation, which costs more than building the arrangement it
// indexes. Large segments span half the disk instead of a tenth of it, so they
// cross far more often and reach a comparable arrangement at a fraction of the
// n — hence a second, much shorter list rather than one shared ceiling.
constexpr auto kArrangement      = linearSizes(10000);
constexpr auto kArrangementLarge = linearSizes(2000);

// ── 3. Intersection of line segments ────────────────────────────────────────
// One list per dataset: the three differ by orders of magnitude in output at
// the same n. Anchored to Bentley–Ottmann over ERational coordinates, the
// slowest cell of each.
//   small segments — capped by its quadratic output.
//   large segments — large ones span half the disk, so they cross far more
//     often, and a sixth of the n already yields more crossings than the small
//     sweep reaches at its own ceiling.
//   polygon edges  — a simple polygon's edges cross nowhere, so this is the
//     same sweep with no output term, and much the cheapest of the three. Its
//     ceiling is generation, not the sweep: untangling the polygon costs
//     seconds at the top of the list, the measured sweep milliseconds.
constexpr auto kSegmentsSmall   = linearSizes(10000);
constexpr auto kSegmentsLarge   = linearSizes(1600);
constexpr auto kSegmentsPolygon = linearSizes(10000);

// ── 4. Constructions over a set of points ───────────────────────────────────
// No dominant construction: each is its own one-shot build, so each is anchored
// to itself. The first four are well inside the budget at this ceiling and stop
// there because it is the ceiling this benchmark imposes, not because they ran
// out of room. The kd-tree build is the most expensive of the five and is taken
// to the same ceiling anyway, so that all five constructions are compared over
// one range.
constexpr auto kClosestPair   = linearSizes(100000);
constexpr auto kConvexHull    = linearSizes(100000);
constexpr auto kSortAround    = linearSizes(100000);
constexpr auto kDelaunayBuild = linearSizes(100000);
constexpr auto kPointTree     = linearSizes(100000);

// ── 5. Geometric search over points ─────────────────────────────────────────
// The same ShapeTree build as kPointTree, deliberately over the same range, so
// the two categories measure that build at the same n and their curves can be
// read against each other. The three query problems ride the list and are flat
// beside the build, which is the anchoring the header describes.
constexpr auto kPointSearch = linearSizes(100000);

// ── 6. Geometric search over segments ───────────────────────────────────────
// Anchored to the ShapeTree build, the slower of the two structures. n
// independent small segments — no polygon to untangle and no output term, so
// nothing caps this one short of the ceiling the point categories use.
constexpr auto kSegmentSearch = linearSizes(100000);

// ── 7. Visibility ───────────────────────────────────────────────────────────
// Capped by generation, and by a wide margin: untangling the polygon takes
// tens of times longer than everything this driver measures at that n put
// together. The visibility graph turns out to be neither dense nor quadratic on
// this input — a polygon untangled from random points is extremely spiky, so
// the graph stays sparse at a handful of edges per vertex however far the sweep
// goes. visibleVertices is the expensive problem here rather than the graph,
// which is why it uses a much shorter query batch than the rest of the suite.
constexpr auto kVisibility = linearSizes(10000);

// ── 8. Minkowski sum ────────────────────────────────────────────────────────
// pgl decomposes both operands into convex pieces and unions the pairwise sums,
// so cost climbs steeply in the number of pieces — n vertices in each operand,
// whichever dataset — which is why this ceiling is two orders of magnitude
// below the rest of the suite. One list for both, so that the two curves span
// the same range and the only thing separating them at a given n is how much of
// the plane the second operand covers.
constexpr auto kMinkowski = linearSizes(200);

// ── 9. Union ────────────────────────────────────────────────────────────────
// One problem, two shapes of input, each anchored to itself. Two polygons of n
// vertices, where generation is again what runs out first — untangling the pair
// costs orders of magnitude more than unioning them. Or n / 3 large triangles,
// where the pieces are trivial but overlap heavily, so the cost is in how many
// boundaries meet rather than in the size of the input. The triangle sweep is a
// multiple of three throughout, so n always denotes the total number of input
// vertices.
constexpr auto kUnionPair      = linearSizes(10000);
constexpr auto kUnionTriangles = linearSizes(10000);

// ── 10. Voronoi diagram ─────────────────────────────────────────────────────
// Every row here is a whole diagram — `voronoiDiagram` returning the assembled
// Arrangement — and the lists are separated by the order of the diagram, which
// is the only thing about the call that changes. An order costs more than the
// one below it by more than the sites suggest, because every order carries a
// bigger diagram: over random sites the order-k diagram comes out with (2k - 1)n
// cells, so order 2 assembles three times what order 1 does and order 4 seven
// times. The cost is linear in n at every order and flat per cell, which is why
// the maxima fall roughly as 1/(2k - 1). No one maximum could serve all three —
// order 4 is out of room long before order 1 is — so there is one list per
// order, each anchored to itself. The order-1 list carries the category's real
// range.
//
// The farthest-point diagram reuses the order-1 list rather than an anchor of
// its own. Only the h hull vertices own a cell, so at 16000 it is nowhere near
// the one-second anchor and could run orders of magnitude further — but it
// shares a chart with the three orders, and a list that long would crush them
// against its left edge.
//
// Exact rationals are the only column: the diagram is computed in them whatever
// the sites are stored as, so an `int` sweep would measure the same work twice.
constexpr auto kVoronoiOrder1 = linearSizes(16000);
constexpr auto kVoronoiOrder2 = linearSizes(8000);
constexpr auto kVoronoiOrder4 = linearSizes(4000);

}  // namespace bench
