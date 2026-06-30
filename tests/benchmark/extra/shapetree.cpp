// g++ -DNDEBUG -Ofast -Iinclude -std=c++23 benchmark/algorithus/shapetree.cpp
// @desc: ShapeTree over 10k random triangles with vertices in [0,100]^2  translated by [0,10k]^2. Average time for query rectangles (intersect/contain) and nearest-neighbor query points in [0,10k]^2 compared against Brute Force.
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>
#include "pgl.hpp"
#include "../plf_nanotimer.h"


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
std::vector<pgl::Triangle<pgl::Point<Number>>> randomTriangles(int n) {
    using Point = pgl::Point<Number>;
    using Triangle = pgl::Triangle<Point>;
    const int sz = 100;
    Rng rng{1};
    std::vector<Triangle> v;
    v.reserve(static_cast<std::size_t>(n));
    while (static_cast<int>(v.size()) < n) {
        const int x = rng.range(0, 10000);
        const int y = rng.range(0, 10000);
        const Point a(Number(x + rng.range(0, sz)), Number(y + rng.range(0, sz)));
        const Point b(Number(x + rng.range(0, sz)), Number(y + rng.range(0, sz)));
        const Point c(Number(x + rng.range(0, sz)), Number(y + rng.range(0, sz)));
        if (pgl::orientationSign(a, b, c) == 0) {
            continue;
        }
        v.emplace_back(a, b, c);
    }
    return v;
}

template <class Number>
std::vector<pgl::Rectangle<pgl::Point<Number>>> queryWindows(int n) {
    using Rect = pgl::Rectangle<pgl::Point<Number>>;
    std::vector<Rect> w;
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
std::vector<pgl::Point<Number>> queryPoints(int n) {
    using Point = pgl::Point<Number>;
    std::vector<Point> p;
    Rng rng{5678};
    for (int i = 0; i < n; ++i) {
        p.emplace_back(Number(rng.range(0, 10000)), Number(rng.range(0, 10000)));
    }
    return p;
}


template <class Number>
void run(const char* label) {
    using Triangle = pgl::Triangle<pgl::Point<Number>>;

    const auto tris = randomTriangles<Number>(kShapes);
    const auto windows = queryWindows<Number>(1000);

    // Construction.
    plf::nanotimer timer;
    timer.start();
    pgl::ShapeTree<Triangle> tree(tris);
    const double buildus = timer.get_elapsed_us();
    std::cout << "build\t\t\t" << label << "\t\t" << tree.size() << "\t" << buildus << std::endl;

    // Tree-pruned intersection counting across all query windows.
    timer.start();
    std::size_t hits = 0;
    for (const auto& q : windows) {
        hits += tree.countIntersecting(q);
    }
    std::cout << "countIntersecting\t" << label << "\t\t" << hits << "\t"
              << timer.get_elapsed_us() / windows.size() << std::endl;

    // Brute-force baseline for the same counts.
    timer.start();
    std::size_t bfHits = 0;
    for (const auto& q : windows) {
        for (const auto& t : tris) {
            if (q.intersects(t)) {
                bfHits++;
            }
        }
    }
    std::cout << "countIntersectsBF\t" << label << "\t\t" << bfHits << "\t"
              << timer.get_elapsed_us()  / windows.size() << std::endl;

    // Tree-pruned containment counting (element contained in the query window).
    timer.start();
    std::size_t inHits = 0;
    for (const auto& q : windows) {
        inHits += tree.countContainedIn(q);
    }
    std::cout << "countContainedIn\t" << label << "\t\t" << inHits << "\t"
              << timer.get_elapsed_us()  / windows.size() << std::endl;

    //Brute-force baseline for the same counts.
    timer.start();
    std::size_t bfHits2 = 0;
    for (const auto& q : windows) {
        for (const auto& t : tris) {
            if (q.contains(t)) {
                bfHits2++;
            }
        }
    }
    std::cout << "countContainsBF\t" << label << "\t\t" << bfHits2 << "\t"
              << timer.get_elapsed_us()  / windows.size() << std::endl;

    // Nearest neighbor is skipped for integer numbers: a triangle's nearest
    // point can fall on an edge interior, where the squared distance divides and
    // truncates, so the integer "nearest" is only approximate and the tree and
    // brute-force results need not agree.
    if constexpr (!std::is_integral_v<Number>) {
        const auto points = queryPoints<Number>(1000);

        // Tree branch-and-bound nearest neighbor across all query points. The
        // checksum sums each found shape's lex-min x to defeat dead-code elimination.
        timer.start();
        Number nnSum{};
        for (const auto& p : points) {
            nnSum = nnSum + tree.nearestNeighbor(p).bbox().min().x();
        }
        std::cout << "nearestNeighbor\t\t" << label << "\t\t" << nnSum << "\t"
                  << timer.get_elapsed_us() / points.size() << std::endl;

        // Brute-force baseline: scan every triangle for the closest one.
        timer.start();
        Number nnSumBF{};
        for (const auto& p : points) {
            const Triangle* best = &tris[0];
            auto bestDist = p.template squaredDistance<Number>(tris[0]);
            for (const auto& t : tris) {
                const auto d = p.template squaredDistance<Number>(t);
                if (d < bestDist) {
                    bestDist = d;
                    best = &t;
                }
            }
            nnSumBF = nnSumBF + best->bbox().min().x();
        }
        std::cout << "nearestNeighborBF\t" << label << "\t\t" << nnSumBF << "\t"
                  << timer.get_elapsed_us() / points.size() << std::endl;
    }

}


int main() {
    std::cout << "Operation\t\tNumber\t\tResult\tTime(μs)" << std::endl;

    run<int>("int");
    run<double>("double");
    run<pgl::Rational<>>("Rational");
    run<pgl::ERational>("ERational");

    return 0;
}
