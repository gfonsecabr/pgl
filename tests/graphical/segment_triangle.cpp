// g++ -std=c++23 -Iinclude tests/graphical/segment_triangle.cpp -o build/graphical/segment_triangle.exe

#include <random>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Triangle = pgl::Triangle<Point>;

std::vector<Triangle> randomTriangles(size_t n) {
    std::random_device rd;
    std::mt19937 rgen(rd());

    std::vector<Triangle> ret;
    static std::uniform_int_distribution<int> coord(-100, 100);

    for (size_t i = 0; i < n; i++) {
        Triangle triangle;
        do {
            triangle = Triangle(
                Point(coord(rgen), coord(rgen)),
                Point(coord(rgen), coord(rgen)),
                Point(coord(rgen), coord(rgen))
            );
        } while (triangle.isDegenerate());
        ret.push_back(triangle);
    }

    return ret;
}

template <class Function>
void run(Function f, const std::string& name) {
    const auto triangles = randomTriangles(15);
    pgl::Canvas canvas;
    canvas << pgl::strokeWidth("5");
    const Segment target(-30, -30, 30, 30);
    canvas << pgl::stroke("black") << target;
    canvas << pgl::strokeWidth("3");

    for (const auto& triangle : triangles) {
        if (f(triangle, target)) {
            canvas << pgl::stroke("red");
        } else {
            canvas << pgl::stroke("blue");
        }
        canvas << pgl::fill("none") << triangle;
    }

    canvas.writeSVG("segment_triangle_" + name + ".svg");
}

int main() {
    for (size_t i=0; i<20; i++) {
        run([](Triangle triangle, Segment segment) { return triangle.crosses(segment); }, "crosses");
        run([](Triangle triangle, Segment segment) { return triangle.intersects(segment); }, "intersects");
        run([](Triangle triangle, Segment segment) { return segment.separates(triangle); }, "separates_st");
        run([](Triangle triangle, Segment segment) { return triangle.separates(segment); }, "separates_ts");
        run([](Triangle triangle, Segment segment) { return triangle.contains(segment); }, "contains");
        //std::cout << "it" << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}