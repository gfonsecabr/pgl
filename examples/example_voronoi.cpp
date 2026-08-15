// Builds the Delaunay triangulation of a point set, takes its Voronoi dual, and
// uses the generating-site face labels for a handful of nearest-site queries.
// The arrows run from each red query point to the blue site labeling the face
// returned by Arrangement::locateFace.
//
// Output: example_voronoi.svg

#include <iostream>
#include <type_traits>
#include <variant>
#include <vector>

#include "pgl.hpp"

using Site = pgl::Point<int>;
using ExactPoint = pgl::EPoint;
using QueryToSite = pgl::OrientedSegment<ExactPoint>;

int main() {
    pgl::Canvas canvas;
    const std::vector<Site> sites{
        {8, 12},  {24, 38}, {45, 10}, {68, 22}, {84, 48},
        {58, 68}, {31, 70}, {47, 43}, {72, 54}, {14, 58},
    };
    canvas << pgl::stroke("#1d4ed8") << pgl::fill("#1d4ed8") << pgl::pointRadius("6") << sites;

    const pgl::Triangulation triangulation(sites);
    auto diagram = triangulation.voronoiDiagram();
    diagram.buildPointLocation();

    // The unbounded Voronoi edges are clipped to the fitted SVG viewport by
    // Canvas; no artificial bounding rectangle is part of the Arrangement.
    canvas << pgl::stroke("#64748b") << pgl::strokeWidth("2px") << pgl::fill("none") << diagram.edges();

    const std::vector<ExactPoint> queries{
        {16, 20}, {35, 27}, {52, 53}, {79, 34}, {21, 45},
    };
    canvas << pgl::stroke("#f19e9e") << pgl::fill("#f19e9e") << pgl::pointRadius("3") << queries;

    canvas << pgl::stroke("#f59e0b") << pgl::strokeWidth("2px") << pgl::fill("none");
    for (const ExactPoint& query : queries) {
        const auto face = diagram.locateFace(query);
        const Site& site = diagram.label(face);
        QueryToSite queryToSite(query, ExactPoint(site));
        std::cout << queryToSite << std::endl;
        canvas << queryToSite;
    }

    canvas.writeSVG("example_voronoi.svg");
    std::cout << "wrote example_voronoi.svg\n";
    return 0;
}
