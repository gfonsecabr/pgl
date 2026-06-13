// g++ -Ofast -Iinclude -std=c++23 benchmark/segment/segment_segment.cpp -o build/segment_segment_bench

#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"

using Point = pgl::Point<pgl::Rational<>, std::string>;
using Line = pgl::Line<Point>;

std::vector<Point> randomPoints(size_t n) {
    const int radius = 100;
    std::mt19937 rgen(2);
    std::vector<Point> ret;
    static std::uniform_int_distribution<int> base(-radius, radius);

    while(ret.size() != n) {
        Point p(base(rgen), base(rgen));
        if(p.squaredDistance(Point{}) < radius*radius) {
            p /= 40;
            ret.push_back(p);
        }
    }

    return ret;
}

void run(bool polarity) {
    static const std::vector<std::string> colors = {
        "black",
        "navy",
        "blue",
        "deepskyblue",
        "cyan",
        "teal",
        "green",
        "limegreen",
        "yellowgreen",
        "gold",
        "orange",
        "red",
        "grey",
        "crimson",
        "magenta",
        "purple"
    };


    auto pts = randomPoints(16);
    pgl::Canvas canvas;


    for(int i = 0; auto p : pts) {
        auto color = colors[i++%colors.size()];
        p.label() = color;
        canvas << pgl::stroke(color);
        canvas << p;
        auto l = polarity ? p.polar() : p.dual();
        Point l0 = l[0];
        l0.label() = color;
        Line l2 = Line(l0, l[1]);
        canvas << l2;

    }

    if (polarity)
        canvas.writeSVG("polarity.svg");
    else
        canvas.writeSVG("duality.svg");
}


int main() {

    run(0);
    run(1);

    return 0;
}
