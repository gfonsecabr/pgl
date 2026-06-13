// g++ -std=c++23 -Iinclude tests/graphical/convex_convex.cpp

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
    int n = (name.find("ntersec") != std::string::npos) ? 20 : 50;
    const auto Convexes = randomConvexes(n);
    pgl::Canvas canvas;
   
    canvas << pgl::strokeWidth("3");
    std::set<Convex> red;

    for (size_t i=0; i<Convexes.size(); i++) {
        const auto& convex = Convexes[i];
        for (size_t j=i+1; j<Convexes.size(); j++) {
            const auto& other = Convexes[j];
          if (f(convex, other)) {
            red.insert(convex);
            red.insert(other);
          }
 
        }
    }

    for (const auto& convex : Convexes) {
        if (red.contains(convex)) {
            canvas << pgl::stroke("red");
        } else {
            canvas << pgl::stroke("blue");
        }
        canvas << pgl::fill("none") << convex;
    }

    canvas.writeSVG("convex_convex_" + name + ".svg");
}

int main() {
    run([](Convex convex, Convex other) { return convex.crosses(other); }, "crosses");
    run([](Convex convex, Convex other) { return convex.intersects(other); }, "intersects");
    run([](Convex convex, Convex other) { return convex.interiorsIntersect(other); }, "interiorsintersect");
    run([](Convex convex, Convex other) { return other.separates(convex) || convex.separates(other); }, "separates");
    run([](Convex convex, Convex other) { return convex.intersection<pgl::Rational<>>(other).has_value(); }, "intersection");
    
    return 0;
}