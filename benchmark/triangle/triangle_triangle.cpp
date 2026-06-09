// g++ -Ofast -Iinclude -std=c++23 benchmark/triangle/triangle_triangle.cpp -o build/triangle_triangle_bench
#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"


template<class Point>
std::vector<pgl::Triangle<Point>> randomTriangles(size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Triangle<Point>> ret;
    static std::uniform_int_distribution<int> dist(-500,500);
    using Number =  std::remove_reference_t<decltype(Point().x())>;

    while(ret.size() < n) {
        Point a((Number)dist(rgen)/den,(Number)dist(rgen)/den);
        Point b((Number)dist(rgen)/den,(Number)dist(rgen)/den);
        Point c((Number)dist(rgen)/den,(Number)dist(rgen)/den);
        pgl::Triangle<Point> tri(a,b,c);
        if (!tri.isDegenerate()) {
            ret.push_back(tri);
        }
    }
    return ret;
}

template<class Point, class Function>
void allPairs(const std::vector<pgl::Triangle<Point>> &tris, Function f) {
    int n = 0;

    for(auto s : tris) {
        for(auto t : tris) {
            auto b = f(s,t);

            if(b) {
                n++;
            }
        }
    }

    std::cout << n << "\t";
}

template<class Point, class Function>
void run(Function f, int den=1) {
    auto tris = randomTriangles<Point>(2000,den);
    plf::nanotimer timer;
    timer.start();
    allPairs(tris, f);

    double ns = timer.get_elapsed_ns()/tris.size()/tris.size();
    std::cout << ns << std::endl;
}


int main() {
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "crosses\t\tint\t\t";
        run<pgl::Point<int>>([](pgl::Triangle<pgl::Point<int>> s, pgl::Triangle<pgl::Point<int>> t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "crosses\t\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Triangle<pgl::Point<double>> s, pgl::Triangle<pgl::Point<double>> t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "crosses\t\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "crosses\t\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.crosses(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "crosses\t\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "crosses\t\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.crosses(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersects\tint\t\t";
        run<pgl::Point<int>>([](pgl::Triangle<pgl::Point<int>> s, pgl::Triangle<pgl::Point<int>> t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "intersects\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Triangle<pgl::Point<double>> s, pgl::Triangle<pgl::Point<double>> t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersects\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersects\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.intersects(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersects\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersects\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.intersects(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersection\tint\t\t";
        run<pgl::Point<int>>([](pgl::Triangle<pgl::Point<int>> s, pgl::Triangle<pgl::Point<int>> t){return s.intersection(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "intersection\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Triangle<pgl::Point<double>> s, pgl::Triangle<pgl::Point<double>> t){return s.intersection(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersection\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.intersection(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersection\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.intersection(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersection\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.intersection(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersection\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.intersection(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "separates\tint\t\t";
        run<pgl::Point<int>>([](pgl::Triangle<pgl::Point<int>> s, pgl::Triangle<pgl::Point<int>> t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "separates\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Triangle<pgl::Point<double>> s, pgl::Triangle<pgl::Point<double>> t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "separates\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "separates\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.separates(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "separates\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "separates\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.separates(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "interiorsInter\tint\t\t";
        run<pgl::Point<int>>([](pgl::Triangle<pgl::Point<int>> s, pgl::Triangle<pgl::Point<int>> t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "interiorsInter\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Triangle<pgl::Point<double>> s, pgl::Triangle<pgl::Point<double>> t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "interiorsInter\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "interiorsInter\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.interiorsIntersect(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "interiorsInter\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "interiorsInter\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.interiorsIntersect(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "contains\tint\t\t";
        run<pgl::Point<int>>([](pgl::Triangle<pgl::Point<int>> s, pgl::Triangle<pgl::Point<int>> t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "contains\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Triangle<pgl::Point<double>> s, pgl::Triangle<pgl::Point<double>> t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "contains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "contains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Triangle<pgl::Point<pgl::Rational<>>> s, pgl::Triangle<pgl::Point<pgl::Rational<>>> t){return s.contains(t);},60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "contains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "contains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> s, pgl::Triangle<pgl::Point<pgl::Rational<pgl::BigInt>>> t){return s.contains(t);},60);
    }

    return 0;
}
