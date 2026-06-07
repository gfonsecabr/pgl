// g++ -Ofast -Iinclude -std=c++23 tests/graphical/segment_shape.cpp

#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"

using Point = pgl::Point<pgl::Rational<>>;
using Segment = pgl::Segment<Point>;
using Rectangle = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Shape = pgl::Shape<Point>;

std::vector<Shape> randomShapes(size_t n) {
    std::mt19937 rgen(3);
    std::vector<Shape> ret;
    static std::uniform_int_distribution<int> base(-30,30);
    static std::uniform_int_distribution<int> len(-3,12);

    for(size_t i = 0; i < n; i++) {
        if (i % 4 == 0) {
            Point p(base(rgen), base(rgen));
            Point q(p.x() + len(rgen), p.y() + len(rgen));
            if (p.x() != q.x() && p.y() != q.y()) {
                ret.push_back(Rectangle(p,q));
            }
        }
        else if (i % 4 == 1) {
            Point p(base(rgen), base(rgen));
            Point q(p.x() + len(rgen), p.y() + len(rgen));
            Point r(p.x() + len(rgen), p.y() + len(rgen));
            if (p != q && p != r && q != r) {
                ret.push_back(Triangle(p,q,r));
            }
        }
        else {
            Point p(base(rgen), base(rgen));
            Point q(p.x() + len(rgen), p.y() + len(rgen));
            if (p != q) {
                ret.push_back(Segment(p,q));
            }
        }
    }

    return ret;
}

template<class Function>
void run(Function f, std::string name) {
    auto shapes = randomShapes(120);
    pgl::Canvas canvas;
    canvas << pgl::strokeWidth("5");
    Point p(-27,30);
    Segment t(p, p + Point(40,-40));
    canvas << t;
    canvas << pgl::strokeWidth("3");

    for(auto s : shapes) {
        auto b = f(s,t);

        if(b) {
            canvas << pgl::stroke("red");
        }
        else {
            canvas << pgl::stroke("blue");
        }

        canvas << s;
    }

    canvas.writeSVG("segment_shape_"+name+".svg");
}


int main() {

    run([](Shape s, Segment t){return s.crosses(t);}, "crosses");

    run([](Shape s, Segment t){return s.intersects(t);}, "intersects");

    run([](Shape s, Segment t){return s.separates(t);}, "separates_st");

    //run([](Shape s, Segment t){ return separates(t, s); }, "separates_ts");

    run([](Shape s, Segment t){return s.interiorsIntersect(t);}, "interiorsintersect");

    return 0;
}
