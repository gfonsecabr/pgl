// Visualizes the Convex::interiorsIntersect(Segment) failure.
//
// Pentagon `convex1` from tests/integration/segment_convex.cpp. Each of its
// own edges lies on the boundary, so convex.interiorsIntersect(edge) must be
// FALSE (the interiors only meet through the polygon's interior, never along a
// boundary edge). The cyclicMaxOrPositive support-triangle implementation
// reports TRUE for some edges instead.
//
// Edges are drawn green when the predicate is correct (false) and red+thick
// when it is wrong (true). The thin diagonals are shown for context.
//
// Build: g++ -std=c++23 -Iinclude -o /tmp/iibug tests/graphical/interiorsintersect_bug.cpp
#include "pgl.hpp"
#include <iostream>

using namespace pgl;
using P = Point<int>;
using S = Segment<P>;
using C = Convex<P>;

int main() {
    C convex({{0, 0}, {8, 8}, {16, 8}, {24, 0}, {12, -8}});
    convex += P(36, 24);

    Canvas canvas;
    canvas.size(800, 600).pointRadius(4);

    // Polygon, lightly filled.
    canvas << stroke("#1f4e79") << strokeWidth("1") << fill("#cfe2f3") << fillOpacity("0.5");
    canvas << convex;

    // Each boundary edge: interiorsIntersect must be false. Red where it lies.
    std::cout << "edge  interiorsIntersect (must be false):\n";
    for (const auto& edge : convex.edges()) {
        const bool ii = convex.interiorsIntersect(edge);
        if (ii) {
            canvas << stroke("#c62828") << strokeWidth("5") << fill("none");  // wrong
        } else {
            canvas << stroke("#2e7d32") << strokeWidth("3") << fill("none");  // correct
        }
        canvas << edge;
        std::cout << "  " << edge << " -> " << (ii ? "TRUE  <-- BUG" : "false (ok)") << "\n";
    }

    // Vertices for reference.
    canvas << stroke("#1f4e79") << fill("#1f4e79") << fillOpacity("1");
    for (std::size_t i = 0; i < convex.size(); ++i) {
        canvas << convex[i];
    }

    const std::string path = "tests/graphical/interiorsintersect_bug.svg";
    canvas.writeSVG(path);
    std::cout << "\nWrote " << path
              << "\n(red, thick = boundary edge wrongly reported as interior-intersecting)\n";
    return 0;
}
