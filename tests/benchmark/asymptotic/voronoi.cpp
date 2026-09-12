// @desc: Voronoi diagram of random points in a disk, at orders 1, 2 and 4.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Voronoi diagram";
constexpr const char* kDataset  = "points";

// What the category is a cube of.
//
// One axis, the problem: what the caller asked for. Three of the four are the
// free function's orders — 1, 2 and 4 — and the fourth is the other call that
// reaches an order-1 diagram:
//
//   Triangulation::voronoiDiagram — the dual of a Delaunay triangulation the
//     caller builds from the same points, order 1 by construction. Measured
//     from the points, triangulation included, so it starts from the same thing
//     as `order 1` and ends with the same thing.
//
// It is a problem of its own and not a second algorithm under `order 1`,
// because it is not a second algorithm: at order 1 the free function computes
// the Delaunay dual too, and the two rows are one computation reached two ways.
// Every row measures a whole diagram — the curves *and* the Arrangement they
// are overlaid into, which is what the call returns and what a caller has in
// hand afterwards.
//
// There is no number axis. A Voronoi vertex is the circumcentre of three sites,
// so the diagram is computed in exact rationals however the sites themselves
// are stored, and an `int` column runs the identical arithmetic at the
// identical speed — it measured within 3% of this one at every n of every
// order — so there is nothing for a second column to compare.
void run(const bench::Options& opt) {
    using Point = pgl::EPoint;
    const char* number = bench::numberName<pgl::ERational>;

    // One sweep of one problem: its own size list, one row per n.
    //
    // The number of faces is both the signature and the output size: a face of
    // the order-k diagram is a cell, so counting them is counting the diagram's
    // cells. Order 1 has exactly n of them, one per site, which the sweep checks
    // as it goes — and checks of both problems that compute one, so the two
    // paths are held to the same diagram and not merely timed side by side; the
    // higher orders grow with k(n - k) and the count is the record of it.
    const auto sweepProblem = [&](std::span<const int> sizes, int order,
                                  const char* problem, const char* algorithm,
                                  auto&& compute) {
        if (!bench::matches(opt.problem, problem)) return;
        for (const int n : bench::sweep(sizes, opt)) {
            if (n < order) continue;
            const auto points = bench::convert<Point>(bench::points(n));
            long long faces = 0;
            const double us = bench::timeOnce(faces, [&] { return compute(points); });
            bench::emit(kCategory, kDataset, problem, algorithm, number, n, faces, us);
            if (order == 1) {
                bench::require(faces == n,
                               "the order-1 diagram does not have one face per site");
            }
        }
    };

    // The free function, one list per order: an order costs enough more than
    // the one below it that a single list would either stop the order-1 curve
    // early or run the order-4 one for minutes. Order 1 is the Delaunay dual —
    // which is how the call computes it — and every order above it is Lee's
    // refinement of the order below.
    const auto diagram = [](int order) {
        return [order](const std::vector<Point>& points) {
            return pgl::voronoiDiagram(points, order).faceCount();
        };
    };
    sweepProblem(bench::kVoronoiOrder1, 1, "order 1", "Delaunay dual", diagram(1));
    sweepProblem(bench::kVoronoiOrder2, 2, "order 2", "Lee's refinement", diagram(2));
    sweepProblem(bench::kVoronoiOrder4, 4, "order 4", "Lee's refinement", diagram(4));

    // The member, over the order-1 list, so the two reach the same diagram at
    // the same n.
    sweepProblem(bench::kVoronoiOrder1, 1, "Triangulation::voronoiDiagram",
                 "Delaunay dual", [](const std::vector<Point>& points) {
                     return pgl::Triangulation<pgl::Triangle<Point>>(points)
                         .voronoiDiagram()
                         .faceCount();
                 });
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.dataset, kDataset) && bench::matches(opt.type, "ERational")) {
        run(opt);
    }
    return 0;
}
