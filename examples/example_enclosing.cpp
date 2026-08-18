// Computes the smallest disk and rectangle enclosing a set of integral
// points and draws the result. All coordinates are even because
// smallestEnclosingDisk divides by two when two points support the
// result and retains the integral coordinate type. The rectangle is
// tilted, so its corners are fractional, but its four supporting lines
// are not: smallestEnclosingRectangle returns them as a half-plane
// intersection, exact in the integral coordinate type.
//
// Output: example_enclosing.svg

#include <iostream>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;

int main() {
    const std::vector<Point> points = {
        {0, 2}, {4, 12}, {10, 4}, {16, 14}, {22, 6},
        {18, -2}, {8, -4}, {12, 8}, {6, 2}, {16, 4},
    };

    const pgl::Disk<Point> disk = pgl::smallestEnclosingDisk(points);
    const pgl::Convex<Point> convex(points);
    const pgl::HalfplaneIntersection<Point> rect = convex.smallestEnclosingRectangle();

    pgl::Canvas canvas;

    canvas << pgl::stroke("#2563eb")
           << pgl::fill("#93c5fd")
           << pgl::fillOpacity("25%")
           << disk;

    // Drawn through its corners: a region is drawn together with the points
    // defining its boundary lines, which for this one lie outside it.
    canvas << pgl::stroke("#2dc535")
           << pgl::fill("#95fd93")
           << pgl::fillOpacity("25%")
           << rect.asConvex<pgl::ERational>();

    canvas << pgl::stroke("#991b1b")
           << pgl::fill("#dc2626")
           << pgl::fillOpacity("100%")
           << points;

    canvas.writeSVG("example_enclosing.svg");

    std::cout << "center: " << disk.center<double>()
              << ", squared radius: " << disk.squaredRadius<double>() << '\n'
              << "rectangle: " << rect << '\n'
              << "its corners: " << rect.asConvex<pgl::ERational>() << '\n'
              << "wrote example_enclosing.svg\n";
    return 0;
}
