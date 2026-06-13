// g++ -Ofast -Iinclude -std=c++23 benchmark/convex/convex_convex.cpp
// @desc: Convex hulls of 100 random points in a radius-70 disk around a random
//        center (x,y in [-500,500]); degenerate hulls discarded.
#include <random>
#include <vector>
#include <iostream>
#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"


template<class Point>
std::vector<pgl::Polygon<Point>> randomConvexes(size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Polygon<Point>> ret;
    int radius = 70;
    static std::uniform_int_distribution<int> rradius(-radius, radius);
    static std::uniform_int_distribution<int> rcenter(-500,500);

    while(ret.size() < n) {
        std::vector<Point> vec;
        Point center(rcenter(rgen),rcenter(rgen));
        for (size_t j = 0; j < 100; j++) {
            Point v(rradius(rgen),rradius(rgen));
            if (v.squaredDistance(Point()) <= radius*radius) {
                vec.push_back(v+center);
            }
        }
        pgl::Convex<Point> c(vec);
        if (!c.isDegenerate()) {
            std::vector<Point> scaled;
            for (const Point& p : c / den) {
                scaled.emplace_back(pgl_benchmark::normalized(p.x()),
                                    pgl_benchmark::normalized(p.y()));
            }
            ret.emplace_back(scaled);
        }
    }
    return ret;
}

template<class Point, class Function>
void allPairs(const std::vector<pgl::Polygon<Point>> &polygons, Function f) {
    int n = 0;

    for(auto &s : polygons) {
        for(auto &t : polygons) {
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
    auto polygons = randomConvexes<Point>(300,den);
    plf::nanotimer timer;
    timer.start();
    allPairs(polygons, f);

    double ns = timer.get_elapsed_ns()/polygons.size()/polygons.size();
    std::cout << ns << std::endl;
}


int main() {
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;

    // if (pgl_benchmark::numberEnabled("int")) {
    //     std::cout << "crosses\t\tint\t\t";
    //     run<pgl::Point<int>>([](const pgl::Polygon<pgl::Point<int>>& s, const pgl::Polygon<pgl::Point<int>>& t){return s.crosses(t);});
    // }

    // if (pgl_benchmark::numberEnabled("double")) {
    //     std::cout << "crosses\t\tdouble\t\t";
    //     run<pgl::Point<double>>([](const pgl::Polygon<pgl::Point<double>>& s, const pgl::Polygon<pgl::Point<double>>& t){return s.crosses(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rational")) {
    //     std::cout << "crosses\t\tRational i64\t";
    //     run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.crosses(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rational60")) {
    //     std::cout << "crosses\t\tRational/60\t";
    //     run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.crosses(t);}, 60);
    // }

    // if (pgl_benchmark::numberEnabled("rationalbigint")) {
    //     std::cout << "crosses\t\tRational BigInt\t";
    //     run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.crosses(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rationalbigint60")) {
    //     std::cout << "crosses\t\tRational BigInt/60\t";
    //     run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.crosses(t);}, 60);
    // }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersects\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Polygon<pgl::Point<int>>& s, const pgl::Polygon<pgl::Point<int>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "intersects\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Polygon<pgl::Point<double>>& s, const pgl::Polygon<pgl::Point<double>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersects\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersects\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.intersects(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersects\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersects(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersects\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersects(t);}, 60);
    }


    // if (pgl_benchmark::numberEnabled("rational")) {
    //     std::cout << "intersection\tRational i64\t";
    //     run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.intersection(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rational60")) {
    //     std::cout << "intersection\tRational/60\t";
    //     run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.intersection(t);}, 60);
    // }

    // if (pgl_benchmark::numberEnabled("rationalbigint")) {
    //     std::cout << "intersection\tRational BigInt\t";
    //     run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersection(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rationalbigint60")) {
    //     std::cout << "intersection\tRational BigInt/60\t";
    //     run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.intersection(t);}, 60);
    // }


    // if (pgl_benchmark::numberEnabled("int")) {
    //     std::cout << "separates\tint\t\t";
    //     run<pgl::Point<int>>([](const pgl::Polygon<pgl::Point<int>>& s, const pgl::Polygon<pgl::Point<int>>& t){return s.separates(t);});
    // }

    // if (pgl_benchmark::numberEnabled("double")) {
    //     std::cout << "separates\tdouble\t\t";
    //     run<pgl::Point<double>>([](const pgl::Polygon<pgl::Point<double>>& s, const pgl::Polygon<pgl::Point<double>>& t){return s.separates(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rational")) {
    //     std::cout << "separates\tRational i64\t";
    //     run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.separates(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rational60")) {
    //     std::cout << "separates\tRational/60\t";
    //     run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.separates(t);}, 60);
    // }

    // if (pgl_benchmark::numberEnabled("rationalbigint")) {
    //     std::cout << "separates\tRational BigInt\t";
    //     run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.separates(t);});
    // }

    // if (pgl_benchmark::numberEnabled("rationalbigint60")) {
    //     std::cout << "separates\tRational BigInt/60\t";
    //     run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.separates(t);}, 60);
    // }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "interiorsIntersect\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Polygon<pgl::Point<int>>& s, const pgl::Polygon<pgl::Point<int>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "interiorsIntersect\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Polygon<pgl::Point<double>>& s, const pgl::Polygon<pgl::Point<double>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "interiorsIntersect\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "interiorsIntersect\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.interiorsIntersect(t);}, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "interiorsIntersect\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.interiorsIntersect(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "interiorsIntersect\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.interiorsIntersect(t);}, 60);
    }


    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "contains\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Polygon<pgl::Point<int>>& s, const pgl::Polygon<pgl::Point<int>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "contains\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Polygon<pgl::Point<double>>& s, const pgl::Polygon<pgl::Point<double>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "contains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "contains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.contains(t);},60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "contains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.contains(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "contains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.contains(t);},60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "interiorContains\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Polygon<pgl::Point<int>>& s, const pgl::Polygon<pgl::Point<int>>& t){return s.interiorContains(t);});
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "interiorContains\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Polygon<pgl::Point<double>>& s, const pgl::Polygon<pgl::Point<double>>& t){return s.interiorContains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "interiorContains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.interiorContains(t);});
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "interiorContains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<>>>& t){return s.interiorContains(t);},60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "interiorContains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.interiorContains(t);});
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "interiorContains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& s, const pgl::Polygon<pgl::Point<pgl::Rational<pgl::BigInt>>>& t){return s.interiorContains(t);},60);
    }

    return 0;
}
