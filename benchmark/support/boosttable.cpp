// g++ -Ofast -Iinclude -std=c++23 benchmark/support/boosttable.cpp -lgmp
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
// GMP via Boost.Multiprecision. The native gmpxx mpz_class/mpq_class cannot be
// used here: their expression templates hold references to temporaries, so the
// `auto x = a - b;` pattern in the orientation predicate dangles. Boost's GMP
// backend wraps the same libgmp with expression templates that copy temporaries.
#include <boost/multiprecision/gmp.hpp>

#define PGL_DISABLE_PROMOTION
#include "pgl.hpp"
#include "plf_nanotimer.h"
#include "filter.hpp"

template<class Point>
std::vector<pgl::Segment<Point>> randomSegments(size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Segment<Point>> ret;
    using Number =  std::remove_reference_t<decltype(Point().x())>;

    auto random = [&rgen, den]() {
        std::uniform_int_distribution<int> dist(-500,500);
        pgl::Rational<int> r(dist(rgen),den); // To force simplification
        return (Number) r.numerator() /  (Number) r.denominator();
    };

    for(size_t i = 0; i < n; i++) {
        ret.emplace_back(random(),random(),random(),random());
    }
    return ret;
}

template<class Point>
void allPairs(const std::vector<pgl::Segment<Point>> &segs) {
    int n = 0;

    for(const auto& s : segs) {
        for(const auto& t : segs) {
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
    std::cout << "Operation\tNumber\t\t\tResult\t\tTime(ns)" << std::endl;

    std::cout << "crosses\t\t__int128_t\t\t";
    run<pgl::Point<__int128_t>>();

    std::cout << "crosses\t\tBigInt\t\t\t";
    run<pgl::Point<pgl::BigInt>>();

    std::cout << "crosses\t\tboost int128_t\t\t";
    run<pgl::Point<boost::multiprecision::int128_t>>();

    std::cout << "crosses\t\tboost cpp_int\t\t";
    run<pgl::Point<boost::multiprecision::cpp_int>>();

    std::cout << "crosses\t\tGMP mpz_int\t\t";
    run<pgl::Point<boost::multiprecision::mpz_int>>();

    std::cout << "crosses\t\tpgl Rational int64_t\t";
    run<pgl::Point<pgl::Rational<int64_t>>>();

    std::cout << "crosses\t\tpgl Rational BigInt\t";
    run<pgl::Point<pgl::Rational<pgl::BigInt>>>();

    std::cout << "crosses\t\tboost rational int64_t\t";
    run<pgl::Point<boost::rational<int64_t>>>();

    std::cout << "crosses\t\tboost rational cpp_int\t";
    run<pgl::Point<boost::rational<boost::multiprecision::cpp_int>>>();

    std::cout << "crosses\t\tGMP mpq_rational\t";
    run<pgl::Point<boost::multiprecision::mpq_rational>>();

    std::cout << "Dividing coordinates by 60:\n";

    std::cout << "crosses\t\tpgl Rational int64_t\t";
    run<pgl::Point<pgl::Rational<int64_t>>>(60);

    std::cout << "crosses\t\tpgl Rational BigInt\t";
    run<pgl::Point<pgl::Rational<pgl::BigInt>>>(60);

    std::cout << "crosses\t\tboost rational int64_t\t";
    run<pgl::Point<boost::rational<int64_t>>>(60);

    std::cout << "crosses\t\tboost rational cpp_int\t";
    run<pgl::Point<boost::rational<boost::multiprecision::cpp_int>>>(60);

    std::cout << "crosses\t\tGMP mpq_rational\t";
    run<pgl::Point<boost::multiprecision::mpq_rational>>(60);

    return 0;
}
