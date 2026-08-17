// The Minkowski sum of a nonconvex polygon and a convex polygon.
//
// The sum A + B is the set of all sums of a point of A and a point of B. When
// the origin lies inside B, every translate A + b covers a copy of A, so the
// sum grows A outward and the picture is the union of one translated copy of B
// per point of A. It is enough to place a copy at each *vertex*: the sum's
// boundary is swept by those copies, and every other translate lands inside
// their union together with A itself.
//
// Output: example_minkowskisum.svg

#include <iostream>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Convex = pgl::Convex<Point>;

int main() {
    const PolygonShape polygon({0, 0, 100, 0, 100, 45, 75, 45, 75, 25, 25, 25, 25, 75, 75, 75, 100, 100, 0, 100});
    const Convex convex({8, 0, 4, 7, -4, 7, -8, 0, -4, -7, 4, -7});

    const auto sum = polygon.minkowskiSum(convex);

    std::cout << "polygon area " << polygon.area<double>()
              << ", sum area " << sum.area<double>()
              << ", holes in the sum: " << sum.holeCount() << '\n';

    pgl::Canvas canvas;

    // The sum underneath, as a pale filled region.
    canvas << pgl::stroke("#1d4ed8") << pgl::strokeWidth("1.5")
           << pgl::fill("#93c5fd") << pgl::fillOpacity("0.35");
    canvas << sum;

    // One copy of the convex per polygon vertex; together they sweep out the
    // difference between the polygon and the sum.
    canvas << pgl::stroke("#375cc4") << pgl::strokeWidth("0.5")
           << pgl::fill("#77a1d1") << pgl::fillOpacity("0.35");
    for (const Point& vertex : polygon.vertices()) {
        canvas << convex + vertex;
    }

    // The polygon itself on top.
    canvas << pgl::stroke("#111827") << pgl::strokeWidth("1.5")
           << pgl::fill("#6b7280") << pgl::fillOpacity("0.5");
    canvas << polygon;

    canvas.writeSVG("example_minkowskisum.svg");

    return 0;
}
