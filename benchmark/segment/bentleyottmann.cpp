// g++ -DNDEBUG -Ofast -Iinclude -std=c++23 benchmark/segment/bentleyottmann.cpp
#include <random>
#include <vector>
#include <iostream>
#include "algorithm/intersections.hpp"
#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"


template<class Point>
std::vector<pgl::Segment<Point>> randomSegments(size_t n) {
    std::mt19937 rgen(1);
    std::set<pgl::Segment<Point>> ret_set;
    static std::uniform_int_distribution<int> base(-1000,1000);
    static std::uniform_int_distribution<int> len(-5,5);

    for (size_t i = 0; i < n; i++) {
        Point p(base(rgen),base(rgen));
        Point q(p.x()+ len(rgen), p.y() + len(rgen));
        if (p != q)
            ret_set.emplace(p,q);
    }

    return std::vector<pgl::Segment<Point>>(ret_set.begin(),ret_set.end());
}
template<class Point>
void run(bool crossingsOnly, bool bruteForce) {
    auto segs = randomSegments<Point>(100000);
    plf::nanotimer timer;
    timer.start();
    std::vector<std::array<pgl::Segment<Point>,2>> v;
    if (crossingsOnly) {
        if (bruteForce) v = pgl::bruteForceCrossings(segs);
        else v = pgl::findCrossings(segs);
    }
    else {
        if (bruteForce) v = pgl::bruteForceIntersections(segs);
        else v = pgl::findIntersections(segs);
    }

    std::cout << v.size() << "\t";

    double ns = timer.get_elapsed_ms();
    std::cout << ns << std::endl;
}


int main() {
    std::cout << "Operation\t\tNumber\t\tResult\tTime(ms)" << std::endl;


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersections\t\tint\t\t";
        run<pgl::Point<int>>(false, false);
    }
    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersectionsBF\t\tint\t\t";
        run<pgl::Point<int>>(false, true);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "crossings\t\tint\t\t";
        run<pgl::Point<int>>(true, false);
    }
    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "crossingsBF\t\tint\t\t";
        run<pgl::Point<int>>(true, true);
    }

    return 0;
}
