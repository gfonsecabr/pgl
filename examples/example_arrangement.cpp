// The arrangement of a few crossing segments, drawn on a single canvas: every
// edge, then every vertex, then the segments bounding one face — the largest
// bounded one — in a color of their own.
//
// The input is integral, but the segments cross at non-lattice points, so the
// arrangement holds its vertices as exact rationals (pgl::EPoint, the default
// vertex type). Nothing here rounds: the canvas converts to double only when it
// emits the SVG.
//
// Output: example_arrangement.svg

#include <vector>
#include "pgl.hpp"

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;

// The bounded face of largest area, which is the one this example picks out.
static pgl::Arrangement<>::FaceId largestFace(const pgl::Arrangement<>& arr) {
    pgl::Arrangement<>::FaceId largest;
    pgl::ERational largestArea(0);
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const pgl::Arrangement<>::FaceId f(i);
        if (arr.isUnbounded(f)) {
            continue;
        }
        const pgl::ERational twiceArea = arr.polygonWithHoles(f).twiceArea();
        if (!largest.valid() || twiceArea > largestArea) {
            largest = f;
            largestArea = twiceArea;
        }
    }
    return largest;
}

int main() {
    // A square, its two diagonals, and two chords crossing them.
    const std::vector<Segment> segments = {
        Segment(3, 3, 12, 7),  Segment(1, 0, 15, 12), Segment(12, 2, 0, 12),
        Segment(4, 15, 8, 3),  Segment(0, 2, 13, 12), Segment(0, 2, 12, 0),
        Segment(13, 9, 5, 9),  Segment(4, 0, 9, 12),
    };

    const pgl::Arrangement<> arr(segments);

    pgl::Canvas canvas;

    // Every edge of the subdivision, one entry per twin pair.
    canvas << pgl::stroke("#334155") << pgl::strokeWidth("1.5px") << pgl::fill("none");
    for (const auto& edge : arr.edges()) {
        canvas << edge;
    }

    // Every vertex: the segment endpoints together with every crossing.
    canvas << pgl::stroke("#1d4ed8") << pgl::fill("#1d4ed8") << pgl::pointRadius("4");
    for (const auto& vertex : arr.vertices()) {
        canvas << vertex;
    }

    const pgl::Arrangement<>::FaceId face = largestFace(arr);

    // The segments bounding the chosen face, on top and in another color.
    canvas << pgl::stroke("#e11d48") << pgl::strokeWidth("3px") << pgl::fill("none");
    for (pgl::Arrangement<>::HalfedgeId h : arr.boundaryOf(face)) {
        canvas << arr[h];
    }

    canvas.writeSVG("example_arrangement.svg");

    std::cout << arr.vertexCount() << " vertices, " << arr.edgeCount() << " edges, "
              << arr.faceCount() << " faces (the unbounded one included)\n"
              << "highlighted face " << face.index() << ", of area "
              << arr.polygonWithHoles(face).area<double>() << ", bounded by "
              << arr.boundaryOf(face).size() << " edges\n";

    return 0;
}
