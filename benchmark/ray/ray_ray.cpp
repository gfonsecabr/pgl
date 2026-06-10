// g++ -Ofast -Iinclude -std=c++23 benchmark/ray/ray_ray.cpp -o build/ray_ray_bench
// @desc: Rays from a random source through a random target, x,y in [-500,500];
//        degenerate (zero-length) rays discarded.
#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"


template<class Point>
std::vector<pgl::Ray<Point>> randomRays(size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Ray<Point>> ret;
    static std::uniform_int_distribution<int> dist(-500,500);
    using Number =  std::remove_reference_t<decltype(Point().x())>;

    while(ret.size() < n) {
        Point source(pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den));
        Point target(pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den));
        pgl::Ray<Point> ray(source,target);
        if (!ray.isDegenerate()) {
            ret.push_back(ray);
        }
    }
    return ret;
}

template<class Point, class Function>
void allPairs(const std::vector<pgl::Ray<Point>> &rays, Function f) {
    int n = 0;

    for(const auto& s : rays) {
        for(const auto& t : rays) {
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
    auto rays = randomRays<Point>(5000,den);
    plf::nanotimer timer;
    timer.start();
    allPairs(rays, f);

    double ns = timer.get_elapsed_ns()/rays.size()/rays.size();
    std::cout << ns << std::endl;
}


int main() {
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "crosses\t\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Ray<pgl::Point<int>>& s, const pgl::Ray<pgl::Point<int>>& t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "crosses\t\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Ray<pgl::Point<double>>& s, const pgl::Ray<pgl::Point<double>>& t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "crosses\t\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "crosses\t\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.crosses(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "crosses\t\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.crosses(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "crosses\t\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.crosses(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersects\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Ray<pgl::Point<int>>& s, const pgl::Ray<pgl::Point<int>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "intersects\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Ray<pgl::Point<double>>& s, const pgl::Ray<pgl::Point<double>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersects\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersects\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.intersects(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersects\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersects\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersects(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersection\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.intersection(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersection\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.intersection(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersection\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersection(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersection\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersection(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "separates\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Ray<pgl::Point<int>>& s, const pgl::Ray<pgl::Point<int>>& t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "separates\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Ray<pgl::Point<double>>& s, const pgl::Ray<pgl::Point<double>>& t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "separates\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "separates\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.separates(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "separates\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.separates(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "separates\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.separates(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "interiorsIntersect\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Ray<pgl::Point<int>>& s, const pgl::Ray<pgl::Point<int>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "interiorsIntersect\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Ray<pgl::Point<double>>& s, const pgl::Ray<pgl::Point<double>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "interiorsIntersect\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "interiorsIntersect\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.interiorsIntersect(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "interiorsIntersect\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "interiorsIntersect\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.interiorsIntersect(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "contains\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Ray<pgl::Point<int>>& s, const pgl::Ray<pgl::Point<int>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "contains\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Ray<pgl::Point<double>>& s, const pgl::Ray<pgl::Point<double>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "contains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "contains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Ray<pgl::Point<pgl::Rational<>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<>>>& t){return s.contains(t);},60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "contains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "contains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Ray<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.contains(t);},60);
    }

    return 0;
}
