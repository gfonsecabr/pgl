#include <random>
#include <vector>
#include "pgl.hpp"

using Point = pgl::Point<int>;
using Triangle = pgl::Triangle<Point>;

static std::vector<Point> randomPoints(int count, int max_coord) {
    std::mt19937 rng(1);
    std::uniform_int_distribution<int> coord(0, max_coord);
    std::vector<Point> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        points.emplace_back(coord(rng), coord(rng));
    }
    return points;
}

static std::vector<Triangle> randomTriangles(int count, int max_coord, int size) {
    std::mt19937 rng(1);
    std::uniform_int_distribution<int> coord(0, max_coord);
    std::uniform_int_distribution<int> offset(-size, size);
    std::vector<Triangle> triangles;
    triangles.reserve(count);
    while (static_cast<int>(triangles.size()) < count) {
        const int x = coord(rng);
        const int y = coord(rng);
        const Point a(x + offset(rng), y + offset(rng));
        const Point b(x + offset(rng), y + offset(rng));
        const Point c(x + offset(rng), y + offset(rng));
        if (pgl::orientationSign(a, b, c) == 0) {
            continue;  // Collinear/coincident: degenerate triangles are UB.
        }
        triangles.emplace_back(a, b, c);
    }
    return triangles;
}

template <typename Shape>
static void draw(const pgl::ShapeTree<Shape>& tree,
                               const std::vector<Shape>& shapes,
                               const std::string &filename) {
    pgl::Canvas canvas;
    canvas.size(800, 800).pointRadius(4);

    // The shape tree node boxes, lightly filled at 20% opacity.
    canvas << pgl::stroke("#2f9aff") << pgl::strokeWidth("1")
           << pgl::fill("#000000") << pgl::fillOpacity("0.2");
    canvas << tree;

    // The shapes on top.
    canvas << pgl::stroke("#c62828") << pgl::fill("white") << pgl::fillOpacity(".5");
    for (const Shape& s : shapes) {
        canvas << s;
    }

    canvas.writeSVG(filename);
}

int main() {
    // 100 random points in a 1000x1000 box.
    std::vector<Point> points = randomPoints(100, 1000);
    pgl::ShapeTree<Point> point_tree(points, 3);
    draw(point_tree, points, "example_shapetree_points.svg");

    // 100 random triangles of size up to 40 in the same box.
    std::vector<Triangle> triangles = randomTriangles(100, 1000, 50);
    pgl::ShapeTree<Triangle> triangle_tree(triangles, 3);
    draw(triangle_tree, triangles, "example_shapetree_triangles.svg");

    return 0;
}
