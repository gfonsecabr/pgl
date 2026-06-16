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

static std::vector<Triangle> randomTriangles(int count, int max_coord, int size) {
    std::mt19937 rng(5);
    std::uniform_int_distribution<int> coord(0, max_coord);
    std::uniform_int_distribution<int> offset(-size, size);
    std::vector<Triangle> triangles;
    while (static_cast<int>(triangles.size()) < count) {
        const int x = coord(rng);
        const int y = coord(rng);
        const Point a(x + offset(rng), y + offset(rng));
        const Point b(x + offset(rng), y + offset(rng));
        const Point c(x + offset(rng), y + offset(rng));
        Triangle triangle(a,b,c);
        if (!triangle.isDegenerate()) {
            triangles.emplace_back(a, b, c);
        }
    }
    return triangles;
}

static void draw(const std::string &filename,
                 const auto& tree,
                 const auto& shape,
                 const auto& shapes2,
                 const auto& shapes3) {
    pgl::Canvas canvas;
    canvas.size(800, 800).pointRadius(4);

    // The shape tree node boxes, lightly filled at 20% opacity.
    canvas << pgl::stroke("#2f9aff") << pgl::strokeWidth("1")
           << pgl::fill("#000000") << pgl::fillOpacity("0.2");
    canvas << tree;

    canvas << pgl::stroke("#1100ff") << pgl::fill("#1100ff") << pgl::fillOpacity(".5");
    canvas << shape;

    canvas << pgl::stroke("#10b305") << pgl::fill("#10b305") << pgl::fillOpacity(".5");
    for (auto& s : tree) {
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

    canvas.writeSVG(filename);
}

// Overload without the third shape group.
static void draw(const std::string &filename,
                 const auto& tree,
                 const auto& shape,
                 const auto& shapes2) {
    draw(filename, tree, shape, shapes2, std::vector<Point>{});
}

int main() {
    Triangle triangle(200,200,600,800,900,250);

    {   // 100 random points in a 1000x1000 box.
        std::vector<Point> points = randomPoints(100, 1000);
        pgl::ShapeTree<Point> tree(points, 3);
    
        auto contained = tree.reportContainedIn(triangle);
        draw("example_shapetree_points.svg", tree, triangle, contained);
    }

    {   // 100 random triangles of size up to 40 in the same box.
        std::vector<Triangle> triangles = randomTriangles(100, 1000, 50);
        pgl::ShapeTree<Triangle> tree(triangles, 3);

        auto contained = tree.reportContainedIn(triangle);
        auto intersecting = tree.reportIntersecting(triangle);
        draw("example_shapetree_triangles.svg", tree, triangle, contained, intersecting);
    }

    return 0;
}
