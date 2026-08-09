// Finding the collinear points of a random point set through duality.
//
// The dual of a point (a, b) is the line y = a x - b, and the dual of a line
// y = m x - c is the point (m, c). Duality preserves incidence: a point lies on
// a line exactly when the dual line contains the dual point. So k points of the
// plane are collinear exactly when their k dual lines meet at a single point,
// and that point is the dual of the line through them.
//
// Building the arrangement of the dual lines therefore turns "which points are
// collinear?" into "which arrangement vertices have more than two lines through
// them?". A vertex where k lines meet has 2k halfedges leaving it, so the
// interesting vertices are the ones of degree greater than four; each one hands
// back both the line (its own dual) and the points on it (the input lines that
// produced the edges around it).
//
// The one collinear family duality cannot see is a vertical one: the duals of
// (k, y1) and (k, y2) are two lines of the same slope k, and parallel lines have
// no crossing to find.
//
// The input points are integral, but the dual lines cross at rational points, so
// the arrangement keeps its vertices as exact rationals (pgl::EPoint, the
// default). Nothing here rounds.
//
// Output: example_dual_arrangement_primal.svg, example_dual_arrangement_dual.svg

#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <vector>
#include "pgl.hpp"

using Point = pgl::Point<int>;
using Window = pgl::Rectangle<Point>;    // The part of the plane a drawing shows.
using Arrangement = pgl::Arrangement<>;  // Vertices of type pgl::EPoint.

// The points are drawn from the grid [-gridRadius, gridRadius]^2, centered on
// the origin, which is also the window of the primal drawing.
static constexpr int gridRadius = 3;

// One maximal set of collinear points, as read off a single arrangement vertex.
struct Collinear {
    pgl::EPoint dualVertex;               // Where the dual lines meet: (m, c).
    pgl::ELine line;                      // Its dual, the primal line y = m x - c.
    std::vector<std::uint32_t> onLine;    // Indices of the points lying on it.
};

// Distinct random points on a small integer grid, where collinearities abound.
static std::vector<Point> randomPoints(int count, int radius, unsigned seed) {
    std::mt19937 generator(seed);
    std::uniform_int_distribution<int> coordinate(-radius, radius);

    std::set<Point> distinct;  // Shapes are values: usable in a set as they are.
    while (static_cast<int>(distinct.size()) < count) {
        const int x = coordinate(generator);  // Drawn one at a time: the order of
        const int y = coordinate(generator);  // argument evaluation is unspecified.
        distinct.emplace(x, y);
    }
    return {distinct.begin(), distinct.end()};
}

// The vertices of the arrangement where more than two dual lines meet.
static std::vector<Collinear> collinearFamilies(const Arrangement& arr) {
    std::vector<Collinear> families;

    for (std::uint32_t i = 0; i < arr.vertexCount(); ++i) {
        const Arrangement::VertexId v(i);
        if (arr.degree(v) <= 4) {
            continue;  // Two lines crossing: an ordinary vertex, nothing to report.
        }

        // A vertex remembers which input shapes pass through it, and the input
        // shape of index i is the dual line of the point of index i.
        const pgl::EPoint vertex = arr[v];
        families.push_back({vertex, vertex.dual<pgl::ERational>(), arr.originsOf(v)});
    }
    return families;
}

// One color per collinear family, reused by both drawings.
static const char* familyColor(std::size_t family) {
    static const char* const palette[] = {"#e11d48", "#2563eb", "#16a34a", "#d97706",
                                          "#9333ea", "#0891b2", "#db2777", "#65a30d"};
    return palette[family % (sizeof(palette) / sizeof(palette[0]))];
}

// The points themselves, with every collinear family in its own color.
static void drawPrimal(const std::vector<Point>& points, const std::vector<Collinear>& families) {
    pgl::Canvas canvas;
    canvas.margin(40);
    // The grid the points came from, and nothing else.
    canvas.view(Window(-gridRadius, -gridRadius, gridRadius, gridRadius));

    canvas << pgl::strokeWidth("1.5px") << pgl::fill("none");
    for (std::size_t i = 0; i < families.size(); ++i) {
        canvas << pgl::stroke(familyColor(i)) << families[i].line;
    }

    canvas << pgl::stroke("#334155") << pgl::fill("#334155") << pgl::pointRadius("4");
    for (const Point& p : points) {
        canvas << p;
    }

    // On top, the points of each family in the color of their line. A point on
    // two of the lines takes the color of the family drawn last.
    canvas << pgl::pointRadius("6");
    for (std::size_t i = 0; i < families.size(); ++i) {
        canvas << pgl::stroke(familyColor(i)) << pgl::fill(familyColor(i));
        for (const std::uint32_t index : families[i].onLine) {
            canvas << points[index];
        }
    }

    canvas.writeSVG("example_dual_arrangement_primal.svg");
}

// The arrangement of the dual lines, with the concurrent vertices colored to
// match the primal lines they are dual to.
static void drawDual(const Arrangement& arr, const std::vector<Collinear>& families) {
    pgl::Canvas canvas;
    canvas.margin(40);
    // Two dual lines of nearly equal slope cross very far away, so fitting the
    // whole arrangement would leave the interesting middle unreadably small.
    canvas.view(Window(-4, -4, 4, 4));

    canvas << pgl::stroke("#94a3b8") << pgl::strokeWidth("1px") << pgl::fill("none");
    canvas << arr.edges();

    // Every crossing of two dual lines, that is, every line through two points.
    canvas << pgl::stroke("#334155") << pgl::fill("#334155") << pgl::pointRadius("3");
    canvas << arr.vertices();

    // The ones where three or more dual lines meet.
    canvas << pgl::pointRadius("7");
    for (std::size_t i = 0; i < families.size(); ++i) {
        canvas << pgl::stroke(familyColor(i)) << pgl::fill(familyColor(i))
               << families[i].dualVertex;
    }

    canvas.writeSVG("example_dual_arrangement_dual.svg");
}

int main() {
    const std::vector<Point> points = randomPoints(12, gridRadius, 9);

    std::vector<pgl::ELine> duals;
    for (const Point& p : points) {
        duals.push_back(p.dual<pgl::ERational>());
    }

    const Arrangement arr(duals);
    const std::vector<Collinear> families = collinearFamilies(arr);

    drawPrimal(points, families);
    drawDual(arr, families);

    std::cout << points.size() << " points, dual arrangement with " << arr.vertexCount()
              << " vertices, " << arr.edgeCount() << " edges and " << arr.faceCount()
              << " faces\n"
              << families.size() << " lines through more than two points:\n";
    for (const Collinear& family : families) {
        // The vertex (m, c) is the line y = m x - c, written here with its sign.
        const pgl::ERational slope = family.dualVertex.x();
        const pgl::ERational intercept = -family.dualVertex.y();
        std::cout << "  y = " << slope << " x " << (intercept < 0 ? "- " : "+ ")
                  << (intercept < 0 ? -intercept : intercept) << " through";
        for (const std::uint32_t index : family.onLine) {
            std::cout << ' ' << points[index];
        }

        // A direct scan over the points confirms the dual's answer, and that
        // the family is maximal: no further point lies on the line.
        std::size_t onLine = 0;
        for (const Point& p : points) {
            onLine += family.line.contains(pgl::EPoint(p)) ? 1 : 0;
        }
        std::cout << (onLine == family.onLine.size() ? "  [confirmed]\n" : "  [MISMATCH]\n");
    }

    return 0;
}
