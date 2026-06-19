#include <cmath>
#include <random>
#include <vector>
#include "pgl.hpp"

using Point = pgl::Point<int>;
using Triangle = pgl::Triangle<Point>;
using Polygon = pgl::Polygon<Point>;

// Builds a simple spiral polygon: a corridor of constant width that winds
// around the origin `turns` times. The boundary runs outward along the inner
// wall (radius r0 + k*theta) and back along the outer wall (the same plus
// `width`), so it never self-intersects as long as width < pitch (= 2*pi*k).
static Polygon makeSpiral(double r0, double k,
                          double width, int turns, int stepsPerTurn) {
    const double thetaMax = turns * 2.0 * M_PI;
    const int steps = turns * stepsPerTurn;
    std::vector<Point> verts;
    auto add = [&](double t, double extra) {
        const double r = r0 + k * t + extra;
        const int x = static_cast<int>(std::lround(r * std::cos(t)));
        const int y = static_cast<int>(std::lround(r * std::sin(t)));
        // Skip repeated integer vertices so no edge collapses to zero length.
        if (verts.empty() || verts.back() != Point(x, y)) {
            verts.emplace_back(x, y);
        }
    };
    for (int i = 0; i <= steps; ++i) {        // outward along the inner wall
        add(thetaMax * i / steps, 0.0);
    }
    for (int i = steps; i >= 0; --i) {        // back along the outer wall
        add(thetaMax * i / steps, width);
    }
    return Polygon(verts);
}

static std::vector<Point> randomPointsInside(const Polygon &poly, size_t count, int max_coord) {
    std::mt19937 rng(1);
    std::uniform_int_distribution<int> coord(-max_coord, max_coord);
    std::vector<Point> points;
    while (points.size() < count) {
        Point p(coord(rng), coord(rng));
        if (poly.interiorContains(p)) {
            points.emplace_back(p);
        }
    }
    return points;
}

static void draw(const std::string &filename,
                 const auto& triangulation,
                 const auto& shape,
                 const auto& shapes2,
                 const auto& shapes3) {
    pgl::Canvas canvas;

    // The shape tree node boxes, lightly filled at 20% opacity.
    canvas << pgl::stroke("#2f9aff") << pgl::strokeWidth("1")
           << pgl::fill("#000000") << pgl::fillOpacity("0.2");
    canvas << triangulation;

    canvas << pgl::stroke("#10b305") << pgl::fill("#10b305") << pgl::fillOpacity(".5");
    for (auto& s : triangulation.triangles()) {
        canvas << s;
    }

    canvas << pgl::stroke("#ff0000") << pgl::fill("#ff0000") << pgl::fillOpacity(".5");
    for (auto& s : shapes2) {
        canvas << s;
    }

    canvas << pgl::stroke("#ffff00") << pgl::fill("#ffff00") << pgl::fillOpacity(".3");
    for (auto& s : shapes3) {
        canvas << s;
    }

    canvas << pgl::stroke("#1100ff") << pgl::fill("#1100ff") << pgl::fillOpacity(".5");
    canvas << shape;

    canvas.writeSVG(filename);
}

int main() {
    pgl::OrientedSegment s(-47,-36,53,64);

    Polygon poly = makeSpiral(12.0, 3.5, 15.0, 3, 40);

    if (!poly.isSimple()) {
        throw std::runtime_error("Polygon is not simple");
    }

    std::vector<Point> points = randomPointsInside(poly, 100, 100);
    pgl::Triangulation triangulation(poly, points); // Delaunay triangulation

    auto interiorIsec = triangulation.trianglesInteriorIntersecting(s);
    auto isec = triangulation.trianglesIntersecting(s);
    draw("example_triangulation2.svg", triangulation, s, interiorIsec, isec);

    return 0;
}

