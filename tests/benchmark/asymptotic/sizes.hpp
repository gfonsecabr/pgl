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
// calibrated part, measured on the reference dev machine (see the per-list
// notes) and then written down; the 32 sample points are spaced evenly from a
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
//   * quadratic output — the segments are scattered over a field of *fixed*
//     width, so sweeping n sweeps the density too and the number of crossings
//     grows with n²; those categories are dominated by the output term, not by
//     their n log n structure, and their drivers record the output size in the
//     Result column so the term can be read from the data;
//   * dataset generation — a simple polygon of n vertices is built by
//     untangling n random points, which is superpolynomial in practice, so the
//     polygon datasets cap where generating one costs about a second, well
//     before the algorithm under test would have.
//
// Measurements below are g++ -O2 -DNDEBUG on the reference machine, at the
// listed maximum, for the slowest number type the category measures.
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
// Anchored to buildPointLocation, not to the Delaunay build: the two are both
// constructions, but the index costs about thirty times the triangulation it
// indexes (at 100,000 points, 4.2 s against 0.12 s), so it is the one that
// decides how far the sweep can go. 30,000 puts it at ~1.0 s. The Delaunay
// build is measured out to the full 100,000 ceiling in its own right under
// point constructions, so nothing is lost by anchoring here.
constexpr auto kTriangulation = linearSizes(30000);

// ── 2. Arrangement ──────────────────────────────────────────────────────────
// Quadratic output: at 10,000 small segments the arrangement already has 63,338
// vertices, against 20,845 at 5,000. Anchored to buildPointLocation again
// (0.92 s at 10,000, against 0.56 s for the arrangement itself). Large segments
// span half the field instead of a tenth of it, so they cross far more often
// and reach a bigger arrangement at an eighth of the n: 45,845 vertices at
// 1,200, where the index takes 1.17 s. That is the same 1,200 the large-segment
// list in category 3 stops at, and for the same reason — the dataset, not the
// operation, is what runs out of room.
constexpr auto kArrangement      = linearSizes(10000);
constexpr auto kArrangementLarge = linearSizes(1200);

// ── 3. Intersection of line segments ────────────────────────────────────────
// One list per dataset: the three differ by orders of magnitude in output at
// the same n. Anchored to Bentley–Ottmann over ERational coordinates, the
// slowest cell of each.
//   small segments — quadratic output (43,354 crossings at n = 10,000); 0.86 s.
//   large segments — large ones span half the field, so they cross far more
//     often: 29,813 crossings at n = 1,000 already, and ~1.0 s at 1,200.
//   polygon edges  — a simple polygon's edges cross nowhere, so this is the
//     same sweep with no output term, and it runs in 33 ms at 6,000. Its
//     ceiling is generation: 6,400 is about a second of untangling.
constexpr auto kSegmentsSmall   = linearSizes(10000);
constexpr auto kSegmentsLarge   = linearSizes(1200);
constexpr auto kSegmentsPolygon = linearSizes(6400);

// ── 4. Constructions over a set of points ───────────────────────────────────
// No dominant construction: each is its own one-shot build, so each is anchored
// to itself. The first four are far under a second even at 100,000 (ERational:
// closest pair 72 ms, hull 63 ms, sort 76 ms, Delaunay 0.57 s) and stop there
// because 100,000 is the ceiling this benchmark imposes, not because they run
// out of budget. The kd-tree build is the expensive one — 3.0 s for 100,000
// ERational points, three times the anchor the rest of the suite is calibrated
// to — and runs to the ceiling anyway, so that all five constructions are
// compared over the same range.
constexpr auto kClosestPair   = linearSizes(100000);
constexpr auto kConvexHull    = linearSizes(100000);
constexpr auto kSortAround    = linearSizes(100000);
constexpr auto kDelaunayBuild = linearSizes(100000);
constexpr auto kPointTree     = linearSizes(100000);

// ── 5. Geometric search over points ─────────────────────────────────────────
// The same ShapeTree build as kPointTree, but capped where that one used to be:
// this list carries three query problems as well as the build, and the queries
// have nothing to gain from a range the build alone can afford.
constexpr auto kPointSearch = linearSizes(40000);

// ── 6. Geometric search over segments ───────────────────────────────────────
// Anchored to the ShapeTree build, the slower of the two structures by roughly
// four times. n independent small segments, reaching the full ceiling — 1.7 s
// at 100,000 ERational segments, the one list here that sits deliberately above
// the one-second mark rather than giving up the ceiling for it.
constexpr auto kSegmentSearch = linearSizes(100000);

// ── 7. Visibility ───────────────────────────────────────────────────────────
// Capped by generation at 6,400. The visibility graph turns out to be neither
// dense nor quadratic on this input — a polygon untangled from random points is
// extremely spiky, so at 6,400 vertices the graph has 28,415 edges, about 4.4
// per vertex, and takes 70 ms to build. visibleVertices is the expensive
// problem here rather than the graph, which is why it uses a much shorter query
// batch than the rest of the suite.
constexpr auto kVisibility = linearSizes(6400);

// ── 8. Minkowski sum ────────────────────────────────────────────────────────
// pgl decomposes both operands into convex pieces and unions the pairwise sums,
// so cost climbs steeply in the number of pieces — n vertices in each operand,
// whichever dataset. One list for both, so that the two curves span the same
// range and the only thing separating them at a given n is how much of the
// plane the second operand covers: at 180 vertices each, two large operands
// take 1.65 s and a large with a small one 1.09 s.
constexpr auto kMinkowski = linearSizes(180);

// ── 9. Union ────────────────────────────────────────────────────────────────
// The pair union is its own anchor at 0.67 s for two 3,200-vertex polygons
// (6,400 takes 3.0 s).
//
// One problem, two shapes of input, each anchored to itself. Two polygons of
// n vertices: 0.67 s at 3,200 each (6,400 takes 3.0 s). n large triangles: the
// pieces are trivial but they overlap heavily, so the cost is in how many
// boundaries meet — 0.85 s at 800, and 1.4 s at 1,000.
constexpr auto kUnionPair      = linearSizes(3200);
constexpr auto kUnionTriangles = linearSizes(800);

}  // namespace bench
