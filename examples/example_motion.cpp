// Plans the shortest collision-free translation of a polygonal robot.
//
// A configuration is represented by the position of the robot's reference
// point (the origin of `robot`).  The reference point may be placed at x
// exactly when `robot + x` is contained in `room`.  That configuration space
// is therefore the Minkowski erosion `room - robot`.
//
// Shortest paths in that free space turn only at its vertices.  The reduced
// visibility graph keeps just the graph edges where a taut path can turn; its
// non-vertex endpoints still need edges to every vertex visible from them.
//
// The SVG shows the original room in gray, the free configuration space in
// green, its reduced visibility graph in blue, and the shortest robot motion
// in orange.  Translucent robot snapshots are spread along the route.
//
// Output: example_motion.svg

#include <iostream>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using ConfigurationPoint = pgl::Point<pgl::ERational>;
using Polygon = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using Segment = pgl::Segment<ConfigurationPoint>;

int main() {
    // A chamfered room with three staggered, slanted obstacles.  A point could
    // take much tighter routes here than the hexagonal robot below.
    const Polygon outer({0, 6, 8, 0, 112, 0, 120, 6, 120, 69, 112, 75, 8, 75, 0, 69});
    const std::vector<Polygon> holes{
        Polygon({20, 13, 33, 11, 35, 14, 35, 51, 32, 53, 19, 51, 19, 15}),
        Polygon({47, 3, 60, 2, 62, 5, 62, 61, 59, 64, 46, 62, 46, 5}),
        Polygon({92, 11, 95, 15, 95, 66, 92, 53, 78, 51, 78, 15}),
    };
    const Region room(outer, holes);

    // The robot's reference point is its center.  Giving its footprint
    // coordinates relative to the origin makes `robot + position` the robot
    // placed at a configuration-space point.  `Convex` states the useful
    // geometric fact about this footprint, rather than merely storing it as a
    // general polygon.
    const pgl::Convex<Point> robot({-4, 0, -2, -3, 2, -3, 4, 0, 2, 3, -2, 3});

    if (!room.isValid()) {
        std::cerr << "the room is not a valid polygon with holes\n";
        return 1;
    }

    // `minkowskiErosion` returns a PolygonSet because erosion can split a
    // region into several components.  Slanted boundaries can meet at rational
    // coordinates, so use its default exact rational result rather than round.
    const auto configurationSpace = room.minkowskiErosion(robot);
    if (configurationSpace.componentCount() != 1) {
        std::cerr << "this example expects one connected free space\n";
        return 1;
    }
    const auto& freeSpace = configurationSpace.component(0);

    // These are deliberately interior points, not free-space vertices.
    const ConfigurationPoint source(8, 8);
    const ConfigurationPoint target(110, 10);
    if (!freeSpace.contains(source) || !freeSpace.contains(target)) {
        std::cerr << "the source or target does not fit the robot in the room\n";
        return 1;
    }

    // This sparse graph has precisely the free-space vertices where a shortest
    // path may bend.  It is much smaller than the complete visibility graph.
    pgl::Graph<ConfigurationPoint> graph = freeSpace.reducedVisibilityGraph();

    // The reduced graph deliberately omits arbitrary first and final hops, so
    // join both non-vertex endpoints to every free-space vertex they can see.
    // Adding the vertices first also handles an endpoint with no visible corner.
    for (const ConfigurationPoint& endpoint : {source, target}) {
        graph.addVertex(endpoint);
        for (const ConfigurationPoint& vertex : freeSpace.visibleVertices(endpoint)) {
            graph.addEdge(endpoint, vertex);
        }
    }
    // If there is line of sight straight to the goal, it is itself a legal
    // path but has no graph vertex in its interior.
    if (freeSpace.contains(Segment(source, target))) {
        graph.addEdge(source, target);
    }

    const auto edgeLength = [](const ConfigurationPoint& a, const ConfigurationPoint& b) {
        return a.distance(b);
    };
    const std::vector<ConfigurationPoint> path = graph.shortestPath(source, target, edgeLength);
    if (path.empty()) {
        std::cerr << "no collision-free path from " << source << " to " << target << '\n';
        return 1;
    }

    double length = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        length += edgeLength(path[i - 1], path[i]);
    }

    std::cout << "configuration space: " << freeSpace.vertexCount() << " vertices, "
              << freeSpace.holeCount() << " holes\n";
    std::cout << "reduced visibility graph: " << graph.vertexCount() << " vertices, "
              << graph.edgeCount() << " edges\n";
    std::cout << "shortest motion (" << path.size() << " waypoints, length " << length << "): ";
    for (const ConfigurationPoint& waypoint : path) {
        std::cout << waypoint << ' ';
    }
    std::cout << '\n';

    pgl::Canvas canvas;

    // The workspace is useful context: its walls and obstacles are the things
    // the robot must not touch.
    canvas << pgl::stroke("#64748b") << pgl::strokeWidth("2px")
           << pgl::fill("#e2e8f0") << pgl::fillOpacity("0.8") << room;

    // The erosion's boundary is the locus of robot-center positions that just
    // touch a room boundary.  It is drawn before the graph so both remain clear.
    canvas << pgl::stroke("#059669") << pgl::strokeWidth("1.5px")
           << pgl::fill("#a7f3d0") << pgl::fillOpacity("0.35") << freeSpace;

    canvas << pgl::stroke("#93c5fd") << pgl::strokeWidth("1px")
           << pgl::strokeOpacity("0.9") << pgl::fill("none");
    for (const auto& [u, v] : graph.edges()) {
        canvas << Segment(u, v);
    }

    canvas << pgl::stroke("none") << pgl::fillOpacity("15%") << pgl::fill("#ea580c")
           << pgl::Polyline<ConfigurationPoint>(path).minkowskiSum(robot);
    canvas << pgl::stroke("#ea580c") << pgl::strokeWidth("3px") << pgl::fill("none")
           << pgl::Polyline<ConfigurationPoint>(path);

    // The footprints certify visually what the erosion means: each displayed
    // copy is inside the original room at its corresponding waypoint.
    canvas << pgl::stroke("#c2410c") << pgl::strokeWidth("1px")
           << pgl::fill("#fdba74") << pgl::fillOpacity("0.4");
    for (std::size_t i = 0; i < path.size(); ++i) {
        canvas << robot + path[i];
    }

    canvas << pgl::stroke("#1d4ed8") << pgl::fill("#1d4ed8") << pgl::pointRadius("5")
           << source;
    canvas << pgl::stroke("#7c2d12") << pgl::fill("#7c2d12") << pgl::pointRadius("5")
           << target;

    canvas.writeSVG("example_motion.svg");
    std::cout << "wrote example_motion.svg\n";
    return 0;
}
