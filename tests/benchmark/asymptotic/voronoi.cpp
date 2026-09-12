// @desc: Voronoi diagram of random points in a disk, at orders 1, 2 and 4, and
// the farthest-point diagram.
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
// One axis, the problem: the order of the diagram the caller asked for — 1, 2
// and 4 — or the farthest-point diagram, which is its own call.
//
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
    // the diagram is a cell, so counting them is counting the diagram's cells.
    // Order 1 has exactly n of them, one per site, and the farthest-point
    // diagram one per hull vertex, which the sweep checks as it goes through
    // @p check; the higher orders grow with k(n - k) and the count is the record
    // of it.
    const auto sweepProblem = [&](std::span<const int> sizes, int order,
                                  const char* problem, const char* algorithm,
                                  auto&& compute, auto&& check) {
        if (!bench::matches(opt.problem, problem)) return;
        for (const int n : bench::sweep(sizes, opt)) {
            if (n < order) continue;
            const auto points = bench::convert<Point>(bench::points(n));
            long long faces = 0;
            const double us = bench::timeOnce(faces, [&] { return compute(points); });
            bench::emit(kCategory, kDataset, problem, algorithm, number, n, faces, us);
            check(points, faces);
        }
    };
    const auto unchecked = [](const std::vector<Point>&, long long) {};

    // The free function, one list per order: an order costs enough more than
    // the one below it that a single list would either stop the order-1 curve
    // early or run the order-4 one for minutes. Order 1 is the Delaunay dual —
    // which is how the call computes it — and every order above it is Lee's
    // refinement of the order below.
    //
    // Order 1 asks for it by leaving the order off, which is the call a caller
    // wanting the ordinary diagram makes: it labels each face with the one site
    // that owns it, where the order-k overload would label it with a
    // one-element vector, so it is a different return type and not k = 1.
    const auto diagram = [](int order) {
        return [order](const std::vector<Point>& points) {
            return pgl::voronoiDiagram(points, order).faceCount();
        };
    };
    sweepProblem(bench::kVoronoiOrder1, 1, "order 1", "Delaunay dual",
                 [](const std::vector<Point>& points) {
                     return pgl::voronoiDiagram(points).faceCount();
                 },
                 [](const std::vector<Point>& points, long long faces) {
                     bench::require(faces == static_cast<long long>(points.size()),
                                    "the order-1 diagram does not have one face per site");
                 });
    sweepProblem(bench::kVoronoiOrder2, 2, "order 2", "Lee's refinement", diagram(2), unchecked);
    sweepProblem(bench::kVoronoiOrder4, 4, "order 4", "Lee's refinement", diagram(4), unchecked);

    // The farthest-point diagram gives only the hull vertices a cell, so its
    // output is h faces for h ~ n^(1/3) over sites in a disk: the hull, not the
    // diagram, is what grows with n. It sweeps the order-1 list, to share the
    // chart's x axis with the orders.
    sweepProblem(bench::kVoronoiOrder1, 1, "farthest", "hull bisectors",
                 [](const std::vector<Point>& points) {
                     return pgl::farthestVoronoiDiagram(points).faceCount();
                 },
                 [](const std::vector<Point>& points, long long faces) {
                     bench::require(faces == static_cast<long long>(pgl::convexHull(points).size()),
                                    "the farthest-point diagram does not have one face per hull vertex");
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
