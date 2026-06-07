// g++ -Ofast -Iinclude -std=c++23 benchmark/segment/segment_segment.cpp -o build/segment_segment_bench

#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "visualization/canvas.hpp"

using Point = pgl::Point<pgl::Rational<>>;
using Segment = pgl::Segment<Point>;

std::vector<Segment> randomSegments(size_t n) {
    std::mt19937 rgen(1);
    std::vector<Segment> ret;
    static std::uniform_int_distribution<int> base(-10,10);
    static std::uniform_int_distribution<int> len(-3,3);

    for(size_t i = 0; i < n; i++) {
        Point p(base(rgen),base(rgen));
        Point q(p.x()+ len(rgen), p.y() + len(rgen));
        if (p != q)
            ret.emplace_back(p,q);
    }

    return ret;
}

template<class Function>
void run(Function f, std::string name) {
    auto segs = randomSegments(100);
    pgl::Canvas canvas;
    canvas << pgl::strokeWidth("5");
    Segment t(-7,-9,7,5);
    canvas << t;
    canvas << pgl::strokeWidth("3");

    for(auto s : segs) {
        auto b = f(s,t);

        if(b) {
            canvas << pgl::stroke("red");
        }
        else {
            canvas << pgl::stroke("blue");
        }
        canvas << s;
    }

    canvas.writeSVG("segment_segment_"+name+".svg");
}


int main() {

    run([](Segment s, Segment t){return s.crosses(t);}, "crosses");

    run([](Segment s, Segment t){return s.intersects(t);}, "intersects");

    run([](Segment s, Segment t){return s.separates(t);}, "separates_st");

    run([](Segment s, Segment t){return t.separates(s);}, "separates_ts");

    run([](Segment s, Segment t){return s.interiorsIntersect(t);},"interiorsintersect");

    run([](Segment s, Segment t){return t.contains(s);}, "contains");

    return 0;
}
