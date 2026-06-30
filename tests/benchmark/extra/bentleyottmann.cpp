// @desc: 100k short segments: a random endpoint with x,y in [-1000,1000] plus a
//        per-axis offset of up to +/-5. Sweep-line vs brute force. Polygons made of 100k
//        random points in [-1000,1000].
#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "../plf_nanotimer.h"


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

template <class Rational>
void run(bool crossingsOnly, bool bruteForce) {
    using Point = pgl::Point<int>;

    auto segs = randomSegments<Point>(100000);
    plf::nanotimer timer;
    timer.start();
    std::vector<std::array<pgl::Segment<Point>,2>> v;
    if (crossingsOnly) {
        if (bruteForce) v = pgl::bruteForceCrossings(segs);
        else v = pgl::findCrossings<Rational>(segs);
    }
    else {
        if (bruteForce) v = pgl::bruteForceIntersections(segs);
        else v = pgl::findIntersections<Rational>(segs);
    }

    std::cout << v.size() << "\t";

    double t = timer.get_elapsed_ms();
    std::cout << t << std::endl;
}

template<class Point>
std::vector<Point> randomPoints(size_t n) {
    std::mt19937 rgen(1);
    std::set<Point> ret_set;
    static std::uniform_int_distribution<int> base(-100000,100000);

    for (size_t i = 0; i < n; i++) {
        Point p(base(rgen),base(rgen));
        ret_set.insert(p);
    }

    return std::vector<Point>(ret_set.begin(),ret_set.end());
}

void simplicity(bool star) {
    using Point = pgl::Point<int>;

    auto points = randomPoints<Point>(1000);
    if (star) {
        pgl::sortAround(points, Point());
    }
    else {
        std::mt19937 rgen(1);
        std::shuffle(points.begin(),points.end(), rgen);
    }
    pgl::Polygon<Point> poly(points);

    plf::nanotimer timer;
    timer.start();
    bool result = poly.isSimple();

    std::cout << result << "\t";

    double t = timer.get_elapsed_ms();
    std::cout << t << std::endl;
}


int main() {
    std::cout << "Operation\t\tNumber\t\tResult\tTime(ms)" << std::endl;

    std::cout << "intersectionsBF\t\tint\t\t";
    run<int>(false, true);
    std::cout << "intersections\t\tERational\t\t";
    run<pgl::ERational>(false, false);
    std::cout << "intersections\t\tRational\t\t";
    run<pgl::Rational<>>(false, false);

    std::cout << "crossingsBF\t\tint\t\t";
    run<int>(true, true);
    std::cout << "crossings\t\tERational\t\t";
    run<pgl::ERational>(true, false);
    std::cout << "crossings\t\tRational\t\t";
    run<pgl::Rational<>>(true, false);

    std::cout << "star.isSimple\t\tERational\t\t";
    simplicity(true);
    std::cout << "random.isSimple\t\tERational\t\t";
    simplicity(false);

    return 0;
}
