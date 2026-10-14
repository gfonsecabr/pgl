// Convex layers (onion peeling) of a set of points.
//
// The points go into a DynamicConvexHull. The first layer is the convex
// hull; erasing its vertices exposes the next one, and so on until the set
// is empty. Each erase takes O(log^2 n) time, so peeling every layer of n
// points takes O(n log^2 n) time in total.
//
// The hull is strictly convex, so a point in the middle of a hull edge is not
// one of its vertices and is peeled with a later layer.
//
// Output: example_convexlayers.svg

#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;

// Returns n random points with integer coordinates inside the disk of radius r
// centered at the origin.
std::vector<Point> randomPointsInDisk(std::size_t n, int r) {
    std::mt19937 rng(2026);
    std::uniform_int_distribution<int> coord(-r, r);
    const pgl::Disk<Point> disk(Point(0, 0), r);
    std::vector<Point> points;
    while (points.size() < n) {
        const Point p(coord(rng), coord(rng));
        if (disk.contains(p)) {
            points.push_back(p);
        }
    }
    return points;
}

int main() {
    const std::vector<std::string> colors = {
        "crimson", "darkorange", "gold", "yellowgreen", "seagreen",
        "teal", "steelblue", "royalblue", "slateblue", "purple", "magenta",
    };

    const std::vector<Point> points = randomPointsInDisk(150, 100);
    pgl::DynamicConvexHull<Point> hull(points);

    std::vector<pgl::Convex<Point>> layers;
    while (!hull.empty()) {
        layers.push_back(hull.convexHull());
        for (const Point& p : hull.vertices()) {
            hull.erase(p);
        }
    }

    pgl::Canvas canvas;
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const std::string& color = colors[i % colors.size()];
        canvas << pgl::stroke(color) << pgl::fill(color) << pgl::fillOpacity("8%")
               << layers[i];
    }

    canvas << pgl::stroke("white") << pgl::fill("black") << pgl::fillOpacity("100%")
           << points;

    canvas.writeSVG("example_convexlayers.svg");

    std::cout << points.size() << " points, " << layers.size() << " convex layers\n"
              << "wrote example_convexlayers.svg\n";
    return 0;
}
