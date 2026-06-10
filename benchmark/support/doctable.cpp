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
        ret.emplace_back(pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den),pgl_benchmark::normalized((Number)dist(rgen)/den));
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
}

template<class Point>
void run(const char* label, int den=1) {
    using Number = std::remove_reference_t<decltype(Point().x())>;
#ifndef PGL_DISABLE_PROMOTION
    // With promotion enabled, skip types that do not actually promote: the
    // result would be identical to the no-promotion run, so there is no point
    // spending time on it (and the table cell is left empty).
    if constexpr (std::is_same_v<pgl::detail::promoted_number_t<Number>, Number>) {
        return;
    }
#endif
    std::cout << label;
    auto segs = randomSegments<Point>(5000,den);
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


    run<pgl::Point<int16_t>>("crosses\t\tint16_t\t\t");
    run<pgl::Point<int32_t>>("crosses\t\tint32_t\t\t");
    run<pgl::Point<int64_t>>("crosses\t\tint64_t\t\t");
    run<pgl::Point<pgl::int128>>("crosses\t\tpgl::int128\t");
    run<pgl::Point<pgl::BigInt>>("crosses\t\tpgl::BigInt\t");
    run<pgl::Point<float>>("crosses\t\tfloat\t\t");
    run<pgl::Point<double>>("crosses\t\tdouble\t\t");
    run<pgl::Point<long double>>("crosses\t\tlong double\t");
    run<pgl::Point<pgl::Rational<int32_t>>>("crosses\t\tRational i32\t");
    run<pgl::Point<pgl::Rational<int64_t>>>("crosses\t\tRational i64\t");
    run<pgl::Point<pgl::Rational<pgl::int128>>>("crosses\t\tRational i128\t");
    run<pgl::Point<pgl::Rational<pgl::BigInt>>>("crosses\t\tRational BigInt\t");
    run<pgl::Point<pgl::Rational<int32_t>>>("crosses\t\tRational i32/60\t", 60);
    run<pgl::Point<pgl::Rational<int64_t>>>("crosses\t\tRational i64/60\t", 60);
    run<pgl::Point<pgl::Rational<pgl::int128>>>("crosses\t\tRational 128/60\t", 60);
    run<pgl::Point<pgl::Rational<pgl::BigInt>>>("crosses\t\tRational BigInt/60\t", 60);

    return 0;
}
