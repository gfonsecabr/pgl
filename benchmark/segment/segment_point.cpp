// g++ -Ofast -Iinclude -std=c++23 benchmark/segment/segment_point.cpp
// @desc: Point-containment tests of random points against segments between two
//        random endpoints with x,y in [-500,500].
#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"


template<class Point>
std::vector<pgl::Segment<Point>> randomShapes(size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Segment<Point>> ret;
    static std::uniform_int_distribution<int> dist(-500,500);
    using Number = std::remove_reference_t<decltype(Point().x())>;

    for(size_t i = 0; i < n; i++) {
        ret.emplace_back(pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den));
    }
    return ret;
}

template<class Point>
std::vector<Point> randomPoints(size_t n, int den) {
    std::mt19937 rgen(2);
    std::vector<Point> ret;
    ret.reserve(n);
    static std::uniform_int_distribution<int> dist(-500,500);
    using Number = std::remove_reference_t<decltype(Point().x())>;

    for(size_t i = 0; i < n; i++) {
        ret.emplace_back(pgl_benchmark::normalized((Number)dist(rgen)/den),
                         pgl_benchmark::normalized((Number)dist(rgen)/den));
    }
    return ret;
}

template<class S, class Point, class Function>
void allPairs(const std::vector<S> &shapes, const std::vector<Point> &points, Function f) {
    int n = 0;

    for(const auto &s : shapes) {
        for(const auto &p : points) {
            if(f(s,p)) {
                n++;
            }
        }
    }

    std::cout << n << "\t";
}

template<class Point, class Function>
void run(Function f, int den=1) {
    auto shapes = randomShapes<Point>(5000,den);
    auto points = randomPoints<Point>(5000,den);
    plf::nanotimer timer;
    timer.start();
    allPairs(shapes, points, f);

    double ns = timer.get_elapsed_ns()/shapes.size()/points.size();
    std::cout << ns << std::endl;
}


int main() {
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "contains\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Segment<pgl::Point<int>>& s, const pgl::Point<int>& p){return s.contains(p);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "contains\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Segment<pgl::Point<double>>& s, const pgl::Point<double>& p){return s.contains(p);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "contains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.contains(p);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "contains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.contains(p);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "contains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.contains(p);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "contains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.contains(p);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "interiorContains\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Segment<pgl::Point<int>>& s, const pgl::Point<int>& p){return s.interiorContains(p);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "interiorContains\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Segment<pgl::Point<double>>& s, const pgl::Point<double>& p){return s.interiorContains(p);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "interiorContains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.interiorContains(p);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "interiorContains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.interiorContains(p);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "interiorContains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.interiorContains(p);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "interiorContains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.interiorContains(p);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "boundaryContains\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Segment<pgl::Point<int>>& s, const pgl::Point<int>& p){return s.boundaryContains(p);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "boundaryContains\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Segment<pgl::Point<double>>& s, const pgl::Point<double>& p){return s.boundaryContains(p);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "boundaryContains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.boundaryContains(p);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "boundaryContains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.boundaryContains(p);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "boundaryContains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.boundaryContains(p);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "boundaryContains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.boundaryContains(p);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "verticesContain\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Segment<pgl::Point<int>>& s, const pgl::Point<int>& p){return s.verticesContain(p);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "verticesContain\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Segment<pgl::Point<double>>& s, const pgl::Point<double>& p){return s.verticesContain(p);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "verticesContain\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.verticesContain(p);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "verticesContain\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Segment<pgl::Point<pgl::Rational<>>>& s, const pgl::Point<pgl::Rational<>>& p){return s.verticesContain(p);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "verticesContain\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.verticesContain(p);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "verticesContain\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Segment<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Point<pgl::Rational<pgl::BigInt>>& p){return s.verticesContain(p);}, 60);
    }

    return 0;
}
