// @desc: Regularized union of two polygons with n vertices or n/3 triangles;
// the result is the total number of boundary vertices.
// @dataset large + large: Two independent random simple polygons. Each one's
// vertices have integer coordinates drawn uniformly from a disk of diameter
// 5,000 whose centre is itself drawn from a disk of that size, so the two
// usually overlap; joined in the order drawn, its ring is untangled into a
// simple polygon by flipping crossing edges, dropping the rare vertex that only
// touches another edge.
// @dataset triangles: Distinct triangles with integer coordinates. One vertex
// is drawn uniformly from a disk of diameter 5,000 and the other two are offset
// from it by vectors drawn uniformly from a disk of the same size, so the
// triangles overlap heavily.
// @dataset fpg: The polygon with n vertices of the Salzburg Database of
// Polygonal Data's fpg set (triangulation perturbation), and the same polygon
// turned a quarter turn about the centre of its bounding box, since the
// database has one polygon of each size. Its coordinates, in [-1500, 1500]²,
// are scaled by 1,000 and rounded to integers. The database has polygons of
// only some sizes; the sweep takes the nearest.
// @dataset spg: The polygon with n vertices of the Salzburg Database of
// Polygonal Data's spg set (line sweep and 2-opt moves on random points), and
// the same polygon turned a quarter turn about the centre of its bounding box.
// Its coordinates, in the unit square with six decimals, are scaled by
// 1,000,000. The database has polygons of only some sizes; the sweep takes the
// nearest.
// @dataset fpg-holes: A polygon with holes with n vertices in all, from the
// Salzburg Database of Polygonal Data's fpg set with holes (triangulation
// perturbation), and the same region turned a quarter turn about the centre of
// its bounding box.
// The database has several polygons whose outer ring has a given number of
// vertices, with different numbers of holes; this is the one with the most.
// Its coordinates, in [-1500, 1500]², are scaled by 100,000 and rounded to
// integers. The database has polygons of only some sizes; the sweep takes the
// nearest.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <type_traits>
#include <vector>

namespace {

constexpr const char* kCategory  = "Regularized union";
constexpr const char* kProblem   = "union";
constexpr const char* kAlgorithm = "boundary overlay";

void twoPolygons(const bench::Options& opt) {
    constexpr const char* dataset = "large + large";
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, kProblem)) return;

    for (const int n : bench::sweep(bench::kUnionPair, opt)) {
        const pgl::EPolygon a(bench::randomPolygon(n, 1));
        const pgl::EPolygon b(bench::randomPolygon(n, 2));
        long long result = 0;
        const double us = bench::timeOnce(result,
            [&] { return static_cast<long long>(a.regularizedUnion(b).vertexCount()); });
        bench::emit(kCategory, dataset, kProblem, kAlgorithm, number, n, result, us);
    }
}

// A polygon or region of the database and its own quarter turn.
template <class Load>
void sbpdPolygons(const bench::Options& opt, const char* dataset, Load load) {
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, kProblem)) return;

    for (const int n : bench::sbpdSizes(dataset, bench::sweep(bench::kUnionPair, opt))) {
        const auto source = load(dataset, n);
        using Exact = bench::retyped_t<std::remove_cvref_t<decltype(source)>, pgl::ERational>;
        const Exact a(source);
        const Exact b(bench::quarterTurn(source));
        long long result = 0;
        const double us = bench::timeOnce(result,
            [&] { return static_cast<long long>(a.regularizedUnion(b).vertexCount()); });
        bench::emit(kCategory, dataset, kProblem, kAlgorithm, number, n, result, us);
    }
}

void manyTriangles(const bench::Options& opt) {
    constexpr const char* dataset = "triangles";
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, kProblem)) return;

    for (const int n : bench::sweep(bench::kUnionTriangles, opt)) {
        const auto triangles =
            bench::convert<pgl::ETriangle>(bench::largeTriangles(n / 3));
        long long result = 0;
        const double us = bench::timeOnce(result, [&] {
            return static_cast<long long>(
                pgl::regularizedUnionOf<pgl::EPoint>(triangles).vertexCount());
        });
        bench::emit(kCategory, dataset, kProblem, kAlgorithm, number, n, result, us);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "ERational")) {
        twoPolygons(opt);
        manyTriangles(opt);
        for (const char* dataset : bench::kSbpdDatasets) {
            sbpdPolygons(opt, dataset, bench::sbpdPolygon);
        }
        for (const char* dataset : bench::kSbpdRegionDatasets) {
            sbpdPolygons(opt, dataset, bench::sbpdRegion);
        }
    }
    return 0;
}
