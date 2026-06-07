// g++ -Ofast -Iinclude -std=c++23 benchmark/support/doctable.cpp -o build/doctable_bench
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
// #define PGL_DISABLE_PROMOTION
#include "pgl.hpp"
#include "plf_nanotimer.h"


template<class Point>
std::vector<pgl::Segment<Point>> randomSegments(size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Segment<Point>> ret;
    static std::uniform_int_distribution<int> dist(-500,500);
    using Number =  std::remove_reference_t<decltype(Point().x())>;

    for(size_t i = 0; i < n; i++) {
        ret.emplace_back((Number)dist(rgen)/den,(Number)dist(rgen)/den,(Number)dist(rgen)/den,(Number)dist(rgen)/den);
    }
    return ret;
}

template<class Point>
void allPairs(const std::vector<pgl::Segment<Point>> &segs) {
    int n = 0;

    for(auto s : segs) {
        for(auto t : segs) {
            auto b = s.crosses(t);

            if(b) {
                n++;
            }
        }
    }

    std::cout << n << "\t";
    assert(n > 10); // To make sure code is not discarded
}

template<class Point>
void run(int den=1) {
    auto segs = randomSegments<Point>(10000,den);
    plf::nanotimer timer;
    timer.start();
    allPairs(segs);

    double ns = timer.get_elapsed_ns()/segs.size()/segs.size();
    std::cout << ns << std::endl;
}


int main() {
#ifdef PGL_DISABLE_PROMOTION
    std::cout << "Promotion disabled!\n";
#endif

    std::cout << std::setprecision(2)<< std::fixed;
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;


    std::cout << "crosses\t\tint16_t\t\t";
    run<pgl::Point<int16_t>>();

    std::cout << "crosses\t\tint32_t\t\t";
    run<pgl::Point<int32_t>>();

    std::cout << "crosses\t\tint64_t\t\t";
    run<pgl::Point<int64_t>>();

    std::cout << "crosses\t\t__int128_t\t";
    run<pgl::Point<__int128_t>>();

    std::cout << "crosses\t\tfloat\t\t";
    run<pgl::Point<float>>();

    std::cout << "crosses\t\tdouble\t\t";
    run<pgl::Point<double>>();

    std::cout << "crosses\t\tlong double\t";
    run<pgl::Point<long double>>();

    std::cout << "crosses\t\tRational i32\t";
    run<pgl::Point<pgl::Rational<int32_t>>>();

    std::cout << "crosses\t\tRational i64\t";
    run<pgl::Point<pgl::Rational<int64_t>>>();

    std::cout << "crosses\t\tRational i128\t";
    run<pgl::Point<pgl::Rational<__int128_t>>>();

    std::cout << "crosses\t\tRational i32/60\t";
    run<pgl::Point<pgl::Rational<int32_t>>>(60);

    std::cout << "crosses\t\tRational i64/60\t";
    run<pgl::Point<pgl::Rational<int64_t>>>(60);

    std::cout << "crosses\t\tRational 128/60\t";
    run<pgl::Point<pgl::Rational<__int128_t>>>(60);

    return 0;
}
