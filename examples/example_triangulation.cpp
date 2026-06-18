#include <random>
#include <vector>
#include "pgl.hpp"

using Point = pgl::Point<int>;
using Triangle = pgl::Triangle<Point>;

static std::vector<Point> randomPoints(int count, int max_coord) {
    std::mt19937 rng(1);
    std::uniform_int_distribution<int> coord(0, max_coord);
    std::vector<Point> points;
    for (int i = 0; i < count; ++i) {
        points.emplace_back(coord(rng), coord(rng));
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
    pgl::OrientedSegment s(10,20,80,90);

    // 100 random points in a 100x100 box.
    std::vector<Point> points = randomPoints(100, 100);
    pgl::Triangulation triangulation(points); // Delaunay triangulation

    auto interiorIsec = triangulation.trianglesInteriorIntersecting(s);
    auto isec = triangulation.trianglesIntersecting(s);
    draw("example_triangulation.svg", triangulation, s, interiorIsec, isec);

    return 0;
}

