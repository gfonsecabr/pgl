//g++ -std=c++23 -I../../include/ -Wall bentleyottmann.cpp -o bentleyottmann
#include <iostream>
#include <limits>
#include <queue>
#include <utility>
#include <array>
#include <queue>
#include <set>
#include <map>
#include <random>
#include "../include/pgl.hpp"

template<class Point>
std::vector<pgl::Segment<Point>> randomSegments(size_t n1, size_t n2, size_t seed) {
    std::mt19937 rgen(seed); // Seed
    std::set<pgl::Segment<Point>> ret_set;
    static std::uniform_int_distribution<int> base(-20,20);
    static std::uniform_int_distribution<int> len(-4,4);

    for (size_t i = 0; i < n1; i++) {
        Point p(base(rgen),base(rgen));
        Point q(p.x()+ 5*len(rgen), p.y() + 5*len(rgen));
        if (p != q)
            ret_set.emplace(p,q);
    }

    static std::uniform_int_distribution<int> base2(-15,15);
    static std::uniform_int_distribution<int> len2(-1,5);
    static std::uniform_int_distribution<int> k2(0,2);
    Point center(base2(rgen),base2(rgen));
    for (size_t i = 0; i < n2; i++) {
        Point v(5*len2(rgen), 5*len2(rgen));
        Point p1 = center + v;
        Point p2 = center - v*k2(rgen);
        if (p1 != p2)
            ret_set.emplace(p1,p2);
    }

    return std::vector<pgl::Segment<Point>>(ret_set.begin(),ret_set.end());
}

template<class Point>
void draw(const std::string &fn, const std::vector<pgl::Segment<Point>> &segments, const std::vector<std::array<pgl::Segment<Point>,2>> &crossings = {}) {
    pgl::Canvas canvas;
    for (pgl::Segment<Point> s : segments) {
        canvas << pgl::stroke("black") << s;
        canvas << pgl::stroke("lightgrey") << s.min() << s.max();
    }
    using Rational = pgl::Rational<pgl::int128>;
    using RPoint = pgl::Point<Rational>;

    std::map<RPoint,int> count;

    for (auto pair : crossings) {
        auto isec = pair[0].template intersection<Rational>(pair[1]);
        if (isec) {
            if (std::holds_alternative<RPoint>(*isec)) {
                RPoint p =  std::get<RPoint>(*isec);
                count[p]++;
                static const std::vector<std::string> color = {"purple", "red", "orange", "yellow", "green"};
                canvas << pgl::stroke(color[count[p]%5]);

                canvas << p;
            }
            else if (std::holds_alternative<pgl::Segment<RPoint>>(*isec)) {
                canvas << pgl::stroke("red");
                canvas << std::get<pgl::Segment<RPoint>>(*isec);
            }
            else {
                std::cout << "ERROR: No point nor segment???\n";
                assert(false);
            }
        }
        else {
            std::cout << "ERROR: " << pair[0] << ' ' << pair[1] << std::endl;
            assert(false);
        }
    }

    canvas.writeSVG(fn);
}

int main() {
    using Point = pgl::Point<int>;
    auto segments = randomSegments<Point>(25, 8, 2);
    draw("bo_segments.svg", segments);

    // std::cout << "From " << segments.size() << " to ";
    // segments = nonDegenerate(segments);
    // std::cout << segments.size() << " segments\n";

    auto crossingsBF = pgl::bruteForceCrossings(segments);
    draw("bo_crossings_bruteforce.svg", segments, crossingsBF);
    auto isecsBF= pgl::bruteForceIntersections(segments);
    draw("bo_isecs_bruteforce.svg", segments, isecsBF);

    auto crossings = pgl::findCrossings(segments);
    draw("bo_crossings.svg", segments, crossings);
    auto isecs = pgl::findIntersections(segments);
    draw("bo_isecs.svg", segments, isecs);

    std::sort(crossings.begin(),crossings.end());
    std::sort(crossingsBF.begin(),crossingsBF.end());
    std::cout << "cross: " << crossings.size() << " " << crossingsBF.size() << std::endl;
    if (crossings != crossingsBF)
        std::cout << "ERROR!!!\n";

    std::sort(isecs.begin(),isecs.end());
    std::sort(isecsBF.begin(),isecsBF.end());
    std::cout << "isec: " << isecs.size() << " " << isecsBF.size() << std::endl;
    if (isecs != isecsBF)
        std::cout << "ERROR!!!\n";

    return isecs == isecsBF && crossings == crossingsBF;
}
