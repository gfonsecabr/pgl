// Finds a shortest obstacle-avoiding path across a polygonal room.
//
// A shortest path inside a polygon with holes is a polygonal chain whose
// interior vertices are all corners of the region: it can only bend where an
// obstacle forces it to. So the whole continuous problem collapses onto the
// visibility graph and a shortest path in that graph, with Euclidean edge
// lengths, is a shortest path in the room.
//
// The drawing shows the region in grey, the visibility graph behind it in pale
// blue, and the shortest path from the lower left to the upper right in orange.
//
// Output: example_visibility.svg

#include <iostream>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Ring = pgl::Polygon<Point>;
using Segment = pgl::Segment<Point>;

int main() {
    // A rectangular room with three blocks in it, staggered so that no two
    // consecutive gaps line up and the direct line is blocked several times.
    const Ring outer({0, 0, 100, 0, 100, 60, 0, 60});
    const std::vector<Ring> holes{
        Ring({20, 12, 32, 54, 20, 54}),
        Ring({50, 8, 62, 8, 62, 40, 50, 40}),
        Ring({78, 20, 90, 20, 90, 45}),
    };
    const pgl::PolygonWithHoles<Point> room(outer, holes);

    if (!room.isValid()) {   // holes inside the outer ring, interiors disjoint
        std::cerr << "the region is not a valid polygon with holes\n";
        return 1;
    }

    const pgl::Graph<Point> visibility = room.visibilityGraph();
    const Point source = outer[1];
    const Point target = outer[3];
    const std::vector<Point> path = visibility.shortestPath(
        source, target, [](const Point& a, const Point& b) { return a.distance(b); });

    if (path.empty()) {
        std::cerr << "no path from " << source << " to " << target << '\n';
        return 1;
    }

    double length = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        length += path[i - 1].distance(path[i]);
    }

    std::cout << "visibility graph: " << visibility.vertexCount() << " vertices, "
              << visibility.edgeCount() << " edges\n";
    std::cout << "shortest path (" << path.size() << " vertices, length " << length << "): ";
    for (const Point& p : path) {
        std::cout << p << ' ';
    }
    std::cout << '\n';

    pgl::Canvas canvas;

    canvas << pgl::stroke("#94a3b8") << pgl::fill("#e2e8f0") << pgl::strokeWidth("2px") << room;

    canvas << pgl::stroke("#bfdbfe") << pgl::strokeWidth("1px") << pgl::fill("none");
    for (const auto& [u, v] : visibility.edges()) {   // each undirected edge once
        canvas << Segment(u, v);
    }

    canvas << pgl::stroke("#ea580c") << pgl::strokeWidth("3px")
           << pgl::Polyline<Point>(path);

    canvas << pgl::stroke("#1d4ed8") << pgl::fill("#1d4ed8") << pgl::pointRadius("5")
           << source << target;

    canvas.writeSVG("example_visibility.svg");
    std::cout << "wrote example_visibility.svg\n";
    return 0;
}
