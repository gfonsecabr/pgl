// @desc: Regularized union of two polygons with n vertices or n/3 triangles;
// the result is the total number of boundary vertices.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

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
    }
    return 0;
}
