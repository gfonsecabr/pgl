// g++ -Ofast -Iinclude -std=c++23 benchmark/rectangle/rectangle_rectangle.cpp -o build/rectangle_rectangle_bench
#include <iostream>
#include <random>
#include <vector>

#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"

template <class Point>
std::vector<pgl::Rectangle<Point>> randomRectangles(std::size_t n, int den) {
    std::mt19937 rgen(1);
    std::vector<pgl::Rectangle<Point>> ret;
    ret.reserve(n);
    static std::uniform_int_distribution<int> dist(-500, 500);
    using Number = std::remove_reference_t<decltype(Point().x())>;

    for (std::size_t i = 0; i < n; ++i) {
        ret.emplace_back(
            static_cast<Number>(dist(rgen)) / den,
            static_cast<Number>(dist(rgen)) / den,
            static_cast<Number>(dist(rgen)) / den,
            static_cast<Number>(dist(rgen)) / den
        );
    }

    return ret;
}

template <class Point, class Function>
void allPairs(const std::vector<pgl::Rectangle<Point>>& rectangles, Function f) {
    int n = 0;

    for (auto first : rectangles) {
        for (auto second : rectangles) {
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
    const auto rectangles = randomRectangles<Point>(5000, den);
    plf::nanotimer timer;
    timer.start();
    allPairs(rectangles, f);

    const double ns = timer.get_elapsed_ns() / rectangles.size() / rectangles.size();
    std::cout << ns << std::endl;
}

int main() {
    std::cout << "Operation\tNumber\t\tResult\tTime(ns)" << std::endl;

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "contains\tint\t\t";
        run<pgl::Point<int>>([](pgl::Rectangle<pgl::Point<int>> first, pgl::Rectangle<pgl::Point<int>> second) { return first.contains(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "contains\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Rectangle<pgl::Point<double>> first, pgl::Rectangle<pgl::Point<double>> second) { return first.contains(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "contains\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.contains(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "contains\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.contains(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "contains\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.contains(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "contains\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.contains(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "intersects\tint\t\t";
        run<pgl::Point<int>>([](pgl::Rectangle<pgl::Point<int>> first, pgl::Rectangle<pgl::Point<int>> second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "intersects\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Rectangle<pgl::Point<double>> first, pgl::Rectangle<pgl::Point<double>> second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "intersects\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "intersects\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.intersects(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "intersects\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.intersects(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "intersects\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.intersects(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "interiorsIntersect\tint\t\t";
        run<pgl::Point<int>>([](pgl::Rectangle<pgl::Point<int>> first, pgl::Rectangle<pgl::Point<int>> second) { return first.interiorsIntersect(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "interiorsIntersect\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Rectangle<pgl::Point<double>> first, pgl::Rectangle<pgl::Point<double>> second) { return first.interiorsIntersect(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "interiorsIntersect\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.interiorsIntersect(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "interiorsIntersect\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.interiorsIntersect(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "interiorsIntersect\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.interiorsIntersect(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "interiorsIntersect\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.interiorsIntersect(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "crosses\t\tint\t\t";
        run<pgl::Point<int>>([](pgl::Rectangle<pgl::Point<int>> first, pgl::Rectangle<pgl::Point<int>> second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "crosses\t\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Rectangle<pgl::Point<double>> first, pgl::Rectangle<pgl::Point<double>> second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "crosses\t\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "crosses\t\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.crosses(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "crosses\t\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.crosses(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "crosses\t\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.crosses(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "separates\tint\t\t";
        run<pgl::Point<int>>([](pgl::Rectangle<pgl::Point<int>> first, pgl::Rectangle<pgl::Point<int>> second) { return first.separates(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "separates\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Rectangle<pgl::Point<double>> first, pgl::Rectangle<pgl::Point<double>> second) { return first.separates(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "separates\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.separates(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "separates\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.separates(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "separates\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.separates(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "separates\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.separates(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("int")) {
        std::cout << "sqDistance\tint\t\t";
        run<pgl::Point<int>>([](pgl::Rectangle<pgl::Point<int>> first, pgl::Rectangle<pgl::Point<int>> second) { return first.squaredDistance(second); });
    }

    if (pgl_benchmark::numberEnabled("double")) {
        std::cout << "sqDistance\tdouble\t\t";
        run<pgl::Point<double>>([](pgl::Rectangle<pgl::Point<double>> first, pgl::Rectangle<pgl::Point<double>> second) { return first.squaredDistance(second); });
    }

    if (pgl_benchmark::numberEnabled("rational")) {
        std::cout << "sqDistance\tRational i64\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.squaredDistance(second); });
    }

    if (pgl_benchmark::numberEnabled("rational60")) {
        std::cout << "sqDistance\tRational/60\t";
        run<pgl::Point<pgl::Rational<>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<>>> second) { return first.squaredDistance(second); }, 60);
    }

    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        std::cout << "sqDistance\tRational BigInt\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.squaredDistance(second); });
    }

    if (pgl_benchmark::numberEnabled("rationalbigint60")) {
        std::cout << "sqDistance\tRational BigInt/60\t";
        run<pgl::Point<pgl::Rational<pgl::BigInt>>>([](pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> first, pgl::Rectangle<pgl::Point<pgl::Rational<pgl::BigInt>>> second) { return first.squaredDistance(second); }, 60);
    }

    return 0;
}
