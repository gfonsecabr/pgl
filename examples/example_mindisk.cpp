// Computes the smallest disk enclosing a set of integral points and draws the
// result. All coordinates are even because smallestEnclosingDisk divides by two
// when two points support the result and retains the integral coordinate type.
//
// Output: example_mindisk.svg

#include <iostream>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;

int main() {
    const std::vector<Point> points = {
        {0, 2}, {4, 12}, {10, 4}, {16, 14}, {22, 6},
        {18, -2}, {8, -4}, {12, 8}, {6, 2}, {16, 4},
    };

    const auto disk = pgl::smallestEnclosingDisk(points);

    pgl::Canvas canvas;

    // Draw the disk first so that the points remain visible on top of its fill.
    canvas << pgl::stroke("#2563eb")
           << pgl::fill("#93c5fd")
           << pgl::fillOpacity("25%")
           << disk;

    canvas << pgl::stroke("#991b1b")
           << pgl::fill("#dc2626")
           << pgl::fillOpacity("100%")
           << points;

    canvas.writeSVG("example_mindisk.svg");

    std::cout << "center: " << disk.center<double>()
              << ", squared radius: " << disk.squaredRadius<double>() << '\n'
              << "wrote example_mindisk.svg\n";
    return 0;
}
