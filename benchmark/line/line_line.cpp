// g++ -Ofast -Iinclude -std=c++23 benchmark/line/line_line.cpp -o build/line_line_bench
// @desc: Lines through two random points with x,y in [-500,500].
#include <iostream>
#include <random>
#include <vector>

#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"

template <class Point>
std::vector<pgl::Line<Point>> randomLines(std::size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Line<Point>> ret;
    ret.reserve(n);
    static std::uniform_int_distribution<int> dist(-500, 500);
    using Number = std::remove_reference_t<decltype(Point().x())>;

    for (std::size_t i = 0; i < n; ++i) {
        ret.emplace_back(
            pgl_benchmark::normalized(static_cast<Number>(dist(rgen)) / den),
            pgl_benchmark::normalized(static_cast<Number>(dist(rgen)) / den),
            pgl_benchmark::normalized(static_cast<Number>(dist(rgen)) / den),
            pgl_benchmark::normalized(static_cast<Number>(dist(rgen)) / den)
        );
    }

    return ret;
}

template <class Point, class Function>
void allPairs(const std::vector<pgl::Line<Point>>& lines, Function f) {
    int n = 0;

    for (const auto& first : lines) {
        for (const auto& second : lines) {
            const auto value = f(first, second);
            if constexpr (std::is_same_v<decltype(value), bool>) {
                if (value) {
                    ++n;
                }
            } else {
                if (value != decltype(value){}) {
                    ++n;
                }
            }
        }
    }

    std::cout << n << "\t";
    assert(n > 10);
}

template <class Point, class Function>
void run(Function f, int den = 1) {
    const auto lines = randomLines<Point>(5000, den);
    plf::nanotimer timer;
    timer.start();
    allPairs(lines, f);

    const double ns = timer.get_elapsed_ns() / lines.size() / lines.size();
    std::cout << ns << std::endl;
}

int main() {
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "parallel\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Line<pgl::Point<int>>& first, const pgl::Line<pgl::Point<int>>& second) { return first.parallel(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "parallel\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Line<pgl::Point<double>>& first, const pgl::Line<pgl::Point<double>>& second) { return first.parallel(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "parallel\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.parallel(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "parallel\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.parallel(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "parallel\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.parallel(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "parallel\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.parallel(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersects\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Line<pgl::Point<int>>& first, const pgl::Line<pgl::Point<int>>& second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "intersects\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Line<pgl::Point<double>>& first, const pgl::Line<pgl::Point<double>>& second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersects\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersects\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.intersects(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersects\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersects\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.intersects(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "crosses\t\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Line<pgl::Point<int>>& first, const pgl::Line<pgl::Point<int>>& second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "crosses\t\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Line<pgl::Point<double>>& first, const pgl::Line<pgl::Point<double>>& second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "crosses\t\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "crosses\t\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.crosses(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "crosses\t\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "crosses\t\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.crosses(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersection\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.intersection(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersection\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](const pgl::Line<pgl::Point<pgl::Rational<>>>& first, const pgl::Line<pgl::Point<pgl::Rational<>>>& second) { return first.intersection(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersection\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.intersection(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersection\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& first, const pgl::Line<pgl::Point<pgl::Rational<pgl::BigInt>>>& second) { return first.intersection(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "sqDistance\tint\t\t";
        run<pgl::Point<int>>([](const pgl::Line<pgl::Point<int>>& first, const pgl::Line<pgl::Point<int>>& second) { return first.squaredDistance(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "sqDistance\tdouble\t\t";
        run<pgl::Point<double>>([](const pgl::Line<pgl::Point<double>>& first, const pgl::Line<pgl::Point<double>>& second) { return first.squaredDistance(second); });
    }

    return 0;
}
