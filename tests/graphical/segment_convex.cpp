// g++ -std=c++23 -Iinclude tests/graphical/segment_convex.cpp

#include <random>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Convex = pgl::Convex<Point>;

std::vector<Convex> randomConvexes(size_t n) {
    std::mt19937 rgen(1);

    std::vector<Convex> ret;
    int r = 8;
    static std::uniform_int_distribution<int> coord(-30, 30), displacement(-r, r);

    for (size_t i = 0; i < n; i++) {
        Point center(coord(rgen), coord(rgen));
        Convex convex;
        do {
            std::set<Point> pts;
            while (pts.size() < 10) {
                Point p(displacement(rgen), displacement(rgen));
                if(p.squaredDistance(Point(0, 0)) > r*r) {
                    continue;
                }
                pts.insert(center+p);
            }
            convex = Convex(pts);
        } while (convex.isDegenerate());
        ret.push_back(convex);
    }

    return ret;
}

template <class Function>
void run(Function f, const std::string& name) {
    const auto Convexes = randomConvexes(50);
    pgl::Canvas canvas;
    canvas << pgl::strokeWidth("5");
    const Segment target(-22, -12, 15, 25);
    canvas << pgl::stroke("black") << target;
    canvas << pgl::strokeWidth("3");

    for (const auto& convex : Convexes) {
        if (f(convex, target)) {
            canvas << pgl::stroke("red");
        } else {
            canvas << pgl::stroke("blue");
        }
        canvas << pgl::fill("none") << convex;
    }

    canvas.writeSVG("segment_convex_" + name + ".svg");
}

int main() {
    for (size_t i=0; i<20; i++) {
        run([](Convex convex, Segment segment) { return convex.crosses(segment); }, "crosses");
        run([](Convex convex, Segment segment) { return convex.intersects(segment); }, "intersects");
        run([](Convex convex, Segment segment) { return convex.interiorsIntersect(segment); }, "interiorsintersect");
        run([](Convex convex, Segment segment) { return segment.separates(convex); }, "separates_st");
        run([](Convex convex, Segment segment) { return convex.separates(segment); }, "separates_ts");
        //std::cout << "it" << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}