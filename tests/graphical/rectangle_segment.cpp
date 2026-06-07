// g++ -Ofast -Iinclude -std=c++23 benchmark/segment/segment_segment.cpp -o build/segment_segment_bench

#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "visualization/canvas.hpp"

using Point = pgl::Point<>;
using Segment = pgl::Segment<Point>;
using Rectangle = pgl::Rectangle<Point>;

std::vector<Segment> randomSegments(size_t n) {
    std::mt19937 rgen(1);
    std::vector<Segment> ret;
    static std::uniform_int_distribution<int> base(-10,10);
    static std::uniform_int_distribution<int> len(-3,3);

    for(size_t i = 0; i < 3*n/4; i++) {
        Point p(base(rgen),base(rgen));
        Point q(p.x()+ len(rgen), p.y() + len(rgen));
        if (p != q)
            ret.emplace_back(p,q);
    }

    for(size_t i = 0; i < n/4; i++) {
        Point p(base(rgen),base(rgen));
        Point q(base(rgen),base(rgen));
        if (p != q)
            ret.emplace_back(p,q);
    }

    return ret;
}

template<class Function>
void run(Function f, std::string name) {
    auto segs = randomSegments(50);
    pgl::Canvas canvas;
    canvas << pgl::strokeWidth("5");
    Rectangle t(-4,-8,7,5);
    canvas << t;
    canvas << pgl::strokeWidth("3");

    for(auto s : segs) {
        auto b = f(t,s);

        if(b) {
            canvas << pgl::stroke("red");
        }
        else {
            canvas << pgl::stroke("blue");
        }
        canvas << s;
    }

    canvas.writeSVG("rectangle_segment"+name+".svg");
}


int main() {

    run([](Rectangle s, Segment t){return s.crosses(t);}, "crosses");

    run([](Rectangle s, Segment t){return s.intersects(t);}, "intersects");

    // run([](Rectangle s, Segment t){return t.separates(s);}, "separates_ts");

    run([](Rectangle s, Segment t){return s.interiorsIntersect(t);},"interiorsintersect");


    return 0;
}
