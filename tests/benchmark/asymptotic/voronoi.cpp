// @desc: Voronoi diagram of random points in a field, at orders 1, 2 and 4.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>

namespace {

constexpr const char* kCategory = "Voronoi diagram";
constexpr const char* kDataset  = "points";

// What the category is a cube of.
//
// The problem axis is the order of the diagram: 1, 2 and 4. Every row measures
// a whole diagram — the curves *and* the Arrangement they are overlaid into,
// which is what the call returns and what a caller has in hand afterwards. How
// the library arrives at them is its own business and not an axis here.
//
// The remaining axis is which call the caller reaches for, and at order 1 there
// are two that answer the same question:
//
//   voronoiDiagram — the free function over the sites, which is the only one of
//     the two that has an order at all.
//
//   Triangulation::voronoiDiagram — the dual of a Delaunay triangulation the
//     caller builds from the same points, order 1 by construction. Measured
//     from the points, triangulation included, so the two rows start from the
//     same thing and end with the same thing.
//
// There is no number axis. A Voronoi vertex is the circumcentre of three sites,
// so the diagram is computed in exact rationals however the sites themselves
// are stored, and an `int` column runs the identical arithmetic at the
// identical speed — it measured within 3% of this one at every n of every
// order — so there is nothing for a second column to compare.
void run(const bench::Options& opt) {
    using Point = pgl::EPoint;
    const char* number = bench::numberName<pgl::ERational>;

    // One measured row.
    //
    // The number of faces is both the signature and the output size: a face of
    // the order-k diagram is a cell, so counting them is counting the diagram's
    // cells. Order 1 has exactly n of them, one per site, which the row checks
    // as it goes — and checks of both calls, so the two paths are held to the
    // same diagram and not merely timed side by side; the higher orders grow
    // with k(n - k) and the count is the record of it.
    const auto row = [&](int n, int order, const char* problem, const char* path,
                         auto&& compute) {
        long long faces = 0;
        const double us = bench::timeOnce(faces, compute);
        bench::emit(kCategory, kDataset, problem, path, number, n, faces, us);
        if (order == 1) {
            bench::require(faces == n, "the order-1 diagram does not have one face per site");
        }
    };

    // One list per order, since an order costs enough more than the one below
    // it that a single list would either stop the order-1 curve early or run
    // the order-4 one for minutes.
    const auto sweepOrder = [&](std::span<const int> sizes, int order, const char* problem) {
        if (!bench::matches(opt.problem, problem)) return;
        for (const int n : bench::sweep(sizes, opt)) {
            if (n < order) continue;
            const auto points = bench::convert<Point>(bench::points(n));
            row(n, order, problem, "voronoiDiagram",
                [&] { return pgl::voronoiDiagram(points, order).faceCount(); });
            if (order == 1) {
                row(n, order, problem, "Triangulation::voronoiDiagram", [&] {
                    return pgl::Triangulation<pgl::Triangle<Point>>(points)
                        .voronoiDiagram()
                        .faceCount();
                });
            }
        }
    };
    sweepOrder(bench::kVoronoiOrder1, 1, "order 1");
    sweepOrder(bench::kVoronoiOrder2, 2, "order 2");
    sweepOrder(bench::kVoronoiOrder4, 4, "order 4");
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
