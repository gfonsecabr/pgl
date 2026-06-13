// g++ -Ofast -Iinclude -std=c++23 benchmark/segment/segment_segment.cpp -o build/segment_segment_bench

#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"

using Point = pgl::Point<pgl::Rational<>>;
using Segment = pgl::Segment<Point>;
using Rectangle = pgl::Rectangle<Point>;

std::vector<Rectangle> randomRectangles(size_t n) {
    std::mt19937 rgen(1); //i changed the seed temporarly to try a new one
    std::vector<Rectangle> ret;
    static std::uniform_int_distribution<int> base(-100,100);
    static std::uniform_int_distribution<int> len(5,30);

    for(size_t i = 0; i < n; i++) {
        Point p(base(rgen), base(rgen));
        Point q(p.x() + len(rgen), p.y() + len(rgen));
        ret.emplace_back(p,q);
    }

    return ret;
}

template<class Function>
void run(Function f, std::string name) {
    auto rects = randomRectangles(50);
    pgl::Canvas canvas;
    canvas << pgl::strokeWidth("5");
    Segment t(-68,-88,70,50);
    canvas << t;
    canvas << pgl::strokeWidth("3");

    for(auto s : rects) {
        auto b = f(s,t);

        if(b) {
            canvas << pgl::stroke("red");
        }
        else {
            canvas << pgl::stroke("blue");
        }
        canvas << s;
    }

    canvas.writeSVG("segment_rectangle_"+name+".svg");
}


int main() {

    run([](Rectangle s, Segment t){return s.crosses(t);}, "crosses");

    run([](Rectangle s, Segment t){return s.intersects(t);}, "intersects");

    run([](Rectangle s, Segment t){ return t.separates(s); }, "separates_st");

    run([](Rectangle s, Segment t){ return s.separates(t); }, "separates_ts");

    run([](Rectangle s, Segment t){return s.interiorsIntersect(t);},"interiorsintersect");


    return 0;
}
