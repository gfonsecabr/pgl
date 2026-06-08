// g++ -Ofast -Iinclude -std=c++23 benchmark/support/boosttable.cpp
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#define PGL_DISABLE_PROMOTION
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

// Note: pgl no longer needs a hand-rolled global operator<=> for number types
// that lack one (e.g. Boost.Multiprecision). The comparison operators route
// coordinate ordering through pgl::detail::strongOrder / threeWay, which fall
// back to operator< when operator<=> is unavailable. A catch-all operator<=>
// here would also recurse infinitely for types like pgl::BigInt that rely on
// their own operator<=> (the rewritten `a < b` would re-select this template).

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
    std::cout << "Operation\tNumber\t\t\tResult\t\tTime(ns)" << std::endl;

    std::cout << "crosses\t\t__int128_t\t\t";
    run<pgl::Point<__int128_t>>();

    std::cout << "crosses\t\tBigInt\t\t\t";
    run<pgl::Point<pgl::BigInt>>();

    std::cout << "crosses\t\tboost int128_t\t\t";
    run<pgl::Point<boost::multiprecision::int128_t>>();

    std::cout << "crosses\t\tboost int256_t\t\t";
    run<pgl::Point<boost::multiprecision::int256_t>>();

    std::cout << "crosses\t\tboost cpp_int\t\t";
    run<pgl::Point<boost::multiprecision::cpp_int>>();

    std::cout << "crosses\t\tpgl Rational int64_t\t";
    run<pgl::Point<pgl::Rational<int64_t>>>();

    std::cout << "crosses\t\tpgl Rational BigInt\t";
    run<pgl::Point<pgl::Rational<pgl::BigInt>>>();

    std::cout << "crosses\t\tboost rational int64_t\t";
    run<pgl::Point<boost::rational<int64_t>>>();

    std::cout << "crosses\t\tboost rational int256_t\t";
    run<pgl::Point<boost::rational<boost::multiprecision::int256_t>>>();

    std::cout << "crosses\t\tboost rational cpp_int\t";
    run<pgl::Point<boost::rational<boost::multiprecision::cpp_int>>>();

    std::cout << "Dividing coordinates by 60:\n";

    std::cout << "crosses\t\tpgl Rational int64_t\t";
    run<pgl::Point<pgl::Rational<int64_t>>>(60);

    std::cout << "crosses\t\tpgl Rational BigInt\t";
    run<pgl::Point<pgl::Rational<pgl::BigInt>>>(60);

    std::cout << "crosses\t\tboost rational int64_t\t";
    run<pgl::Point<boost::rational<int64_t>>>(60);

    std::cout << "crosses\t\tboost rational int256_t\t";
    run<pgl::Point<boost::rational<boost::multiprecision::int256_t>>>(60);

    std::cout << "crosses\t\tboost rational cpp_int\t";
    run<pgl::Point<boost::rational<boost::multiprecision::cpp_int>>>(60);

    return 0;
}
