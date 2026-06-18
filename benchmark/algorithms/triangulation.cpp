// g++ -DNDEBUG -Ofast -Iinclude -std=c++23 benchmark/algorithms/triangulation.cpp
// @desc: Triangulation benchmark over 10k random points in [0,10000]^2 and random query segments in [0,10000]^2.
#include <cstdint>
#include <iostream>
#include <vector>
#include "pgl.hpp"
#include "../support/plf_nanotimer.h"
#include "../support/filter.hpp"


constexpr int kShapes = 10000;

// Deterministic, portable generator: std::uniform_int_distribution is not stable
// across standard libraries, so we roll our own.
struct Rng {
    std::uint64_t state;
    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }
    int range(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

template <class Number>
std::vector<pgl::Point<Number>> randomPoints(int n) {
    using Point = pgl::Point<Number>;
    Rng rng{1};
    std::vector<Point> v;
    v.reserve(static_cast<std::size_t>(n));
    while (static_cast<int>(v.size()) < n) {
        const int x = rng.range(0, 10000);
        const int y = rng.range(0, 10000);
        v.emplace_back(Number(x), Number(y));
    }
    return v;
}

template <class Number>
std::vector<pgl::OrientedSegment<pgl::Point<Number>>> querySegments(int n) {
    using Seg = pgl::OrientedSegment<pgl::Point<Number>>;
    std::vector<Seg> w;
    Rng rng{1234};
    for (int i = 0; i < n; ++i) {
        const int x1 = rng.range(0, 10000);
        const int y1 = rng.range(0, 10000);
        const int x2 = rng.range(0, 10000);
        const int y2 = rng.range(0, 10000);
        w.emplace_back(Number(x1), Number(y1), Number(x2), Number(y2));
    }
    return w;
}


template <class Number>
void run(const char* label) {
    using Point = pgl::Point<Number>;
    const std::vector<Point> pts = randomPoints<Number>(kShapes);
    const auto segs = querySegments<Number>(1000);

    // Construction.
    plf::nanotimer timer;
    timer.start();
    pgl::Triangulation triangulation(pts);
    const double buildus = timer.get_elapsed_us();
    std::cout << "build (Delaunay)\t\t" << label << "\t\t" << triangulation.triangles().size() << "\t" << buildus << std::endl;

    {
        timer.start();
        std::size_t hits = 0;
        for (const auto& q : segs) {
            hits += triangulation.locate(q[0]).has_value() ? 1 : 0;
        }
        std::cout << "locate\t\t\t\t" << label << "\t\t" << hits << "\t"
                << timer.get_elapsed_us() / segs.size() << std::endl;
    }

    {
        timer.start();
        std::size_t hits = 0;
        for (const auto& q : segs) {
            hits += triangulation.trianglesIntersecting(q).size();
        }
        std::cout << "trianglesIntersecting\t\t" << label << "\t\t" << hits << "\t"
                << timer.get_elapsed_us() / segs.size() << std::endl;
    }

    {
        timer.start();
        std::size_t hits = 0;
        for (const auto& q : segs) {
            hits += triangulation.trianglesInteriorIntersecting(q).size();
        }
        std::cout << "trianglesInteriorIntersecting\t" << label << "\t\t" << hits << "\t"
                << timer.get_elapsed_us() / segs.size() << std::endl;
    }

    {
        timer.start();
        std::size_t hits = 0;
        for (const auto& q : segs) {
            hits += triangulation.edgesIntersecting(q).size();
        }
        std::cout << "edgesIntersecting\t\t" << label << "\t\t" << hits << "\t"
                << timer.get_elapsed_us() / segs.size() << std::endl;
    }

    {
        timer.start();
        std::size_t hits = 0;
        for (const auto& q : segs) {
            hits += triangulation.edgesInteriorIntersecting(q).size();
        }
        std::cout << "edgesInteriorIntersecting\t" << label << "\t\t" << hits << "\t"
                << timer.get_elapsed_us() / segs.size() << std::endl;
    }

}


int main() {
    std::cout << "Operation\t\t\tNumber\t\tResult\tTime(μs)" << std::endl;

    if (pgl_benchmark::numberEnabled("int")) {
        run<int>("int");
    }
    if (pgl_benchmark::numberEnabled("double")) {
        run<double>("double");
    }
    if (pgl_benchmark::numberEnabled("rational")) {
        run<pgl::Rational<>>("Rational i64");
    }
    if (pgl_benchmark::numberEnabled("rationalbigint")) {
        run<pgl::Rational<pgl::BigInt>>("Rational BigInt");
    }

    return 0;
}
