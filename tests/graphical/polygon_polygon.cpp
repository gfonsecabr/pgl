// g++ -Wall -std=c++23 -Iinclude tests/graphical/polygon_polygon.cpp

#include <random>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Polygon = pgl::Polygon<Point>;

std::vector<Polygon> randomPolygons(size_t n) {
    std::mt19937 rgen(1);

    std::vector<Polygon> ret;
    static std::uniform_int_distribution<int> coord(-50, 50);
    static std::uniform_int_distribution<int> scale(1, 4);

    auto polyominoes = pgl::polyominoes(1,6);
    std::shuffle(polyominoes.begin(), polyominoes.end(), rgen);
    if (n < polyominoes.size()) {
        polyominoes.resize(n);
    }

    for (size_t i = 0; i < n; i++) {
        Point center(coord(rgen), coord(rgen));
        int sc = scale(rgen)==1 ? 4 : 1;
        Polygon poly = center + sc*polyominoes[i%polyominoes.size()];
        ret.push_back(poly);
    }

    return ret;
}

template <class Function>
void run(Function f, const std::string& name) {
    int n = 120;
    const auto Polygones = randomPolygons(n);
    pgl::Canvas canvas;
   
    canvas << pgl::strokeWidth("3");
    std::set<Polygon> red;

    for (size_t i=0; i<Polygones.size(); i++) {
        const auto& poly = Polygones[i];
        for (size_t j=i+1; j<Polygones.size(); j++) {
            const auto& other = Polygones[j];
          if (f(poly, other)) {
            red.insert(poly);
            red.insert(other);
          }
 
        }
    }

    for (const auto& poly : Polygones) {
        if (red.contains(poly)) {
            canvas << pgl::stroke("red");
            canvas << pgl::fill("red") << pgl::fillOpacity("50%");
        } else {
            canvas << pgl::stroke("blue");
            canvas << pgl::fill("blue") << pgl::fillOpacity("50%");
        }
        canvas << poly;
    }

    canvas.writeSVG("polygon_polygon_" + name + ".svg");
}

int main() {
    // run([](Polygon poly, Polygon other) { return poly.crosses(other); }, "crosses");
    run([](Polygon poly, Polygon other) { return poly.intersects(other); }, "intersects");
    run([](Polygon poly, Polygon other) { return poly.interiorsIntersect(other); }, "interiorsintersect");
    run([](Polygon poly, Polygon other) { return poly.contains(other) || other.contains(poly); }, "contains");
    run([](Polygon poly, Polygon other) { return poly.interiorContains(other) || other.interiorContains(poly); }, "interiorContains");
    // run([](Polygon poly, Polygon other) { return other.separates(poly) || poly.separates(other); }, "separates");
    // run([](Polygon poly, Polygon other) { return poly.intersection<pgl::Rational<>>(other).size(); }, "intersection");

    return 0;
}