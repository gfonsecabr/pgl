// Builds the Delaunay triangulation of a point set, hands it over as a graph,
// and grows the Euclidean minimum spanning tree of the points with Prim's
// algorithm.
//
// The triangulation is where the shortcut is: the Euclidean MST of a point set
// is a subgraph of its Delaunay triangulation, so the tree of that O(n)-edge
// graph is the tree of the complete one. Squaring the distances loses nothing
// either — squaring is increasing on non-negative numbers, so it orders the
// edges exactly as the distances do — and it keeps every weight an exact
// integer, with no square root and no rounding anywhere in the run.
//
// The drawing shows the triangulation in grey behind the tree in green.
//
// Output: example_mst.svg

#include <iostream>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;

int main() {
    const std::vector<Point> points{
        {8, 12},  {24, 38}, {45, 10}, {68, 22}, {84, 48}, {58, 68}, {31, 70},
        {47, 43}, {72, 54}, {14, 58}, {90, 12}, {60, 90}, {20, 88}, {38, 20},
    };

    const pgl::Triangulation triangulation(points);
    const pgl::Graph<Point> graph = triangulation.asGraph();

    // Prim's algorithm, with the weight function choosing its own number type:
    // long keeps the squared distances exact for coordinates of this size.
    const pgl::Graph<Point> mst = graph.spanningTree(
        [](const Point& a, const Point& b) { return a.squaredDistance<long>(b); });

    pgl::Canvas canvas;
    canvas << pgl::stroke("#cbd5e1") << pgl::strokeWidth("1.5px") << pgl::fill("none")
           << triangulation.edges();

    canvas << pgl::stroke("#078e07") << pgl::strokeWidth("3px");
    for (const auto& [u, v] : mst.edges()) {  // each undirected edge once
        canvas << Segment(u, v);
    }

    canvas << pgl::stroke("#1d4ed8") << pgl::fill("#1d4ed8") << pgl::pointRadius("5")
           << mst.vertices();

    canvas.writeSVG("example_mst.svg");
    std::cout << "wrote example_mst.svg\n";
    return 0;
}
