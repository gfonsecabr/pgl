// Iterated midpoint polygon.
//
// Starting from the convex hull of random points sampled inside a disk, this
// repeatedly replaces the polygon by its "midpoint polygon" -- the convex whose
// vertices are the midpoints of the current polygon's edges -- and draws every
// iteration to an SVG with cycling colors.
//
// Each iteration shrinks toward the centroid of the original vertices (drawn as
// a black dot), which is invariant under the midpoint map. After renormalizing,
// the shape converges to an affine-regular polygon
//
// Output: midpoint_polygon.svg

#include <random>
#include <vector>
#include "pgl.hpp"

using Number = double;
using Point = pgl::Point<Number>;

// Returns n random integer-coordinate points lying inside the disk of radius r
// centered at the origin.
std::vector<Point> randomPointsInDisk(size_t n, int r) {
    static std::mt19937 rng(12345);
    std::vector<Point> points;
    std::uniform_int_distribution<int> coord(-r, r);
    pgl::Disk<Point> disk(Point(), r);   
    while (points.size() < n) {
        Point p(coord(rng), coord(rng));
        if (disk.contains(p)) {              // reject points outside the disk
            points.push_back(p);
        }
    }
    return points;
}

// Returns the midpoint polygon: the convex through the midpoints of c's edges.
pgl::Convex<Point> midpointPolygon(const pgl::Convex<Point>& c) {
    std::vector<Point> midpoints;
    for (const auto& edge : c.edges()) {
        midpoints.push_back(edge.midpoint());   // exact rational midpoint
    }
    return pgl::Convex<Point>(midpoints);
}

int main() {
    const std::vector<std::string> colors = {
        "crimson", "darkorange", "gold", "yellowgreen", "seagreen",
        "teal", "steelblue", "royalblue", "slateblue", "purple", "magenta",
    };

    pgl::Convex<Point> convex = pgl::Convex<Point>(randomPointsInDisk(30, 100));
    Point center = convex.verticesCentroid();

    pgl::Canvas canvas;

    for (int i = 0; i <= 30; ++i) {
        canvas << pgl::stroke(colors[i % colors.size()]);
        canvas << pgl::fill(colors[i % colors.size()]);
        canvas << pgl::fillOpacity("10%");
        canvas << convex;
        convex = midpointPolygon(convex);
    }

    canvas << pgl::stroke("black");
    canvas << pgl::fill("black");
    canvas << pgl::fillOpacity("100%");
    canvas << center;

    canvas.writeSVG("midpoint_polygon.svg");

    return 0;
}
