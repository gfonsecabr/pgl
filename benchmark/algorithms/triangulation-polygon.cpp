// g++ -DNDEBUG -Ofast -Iinclude -std=c++23 benchmark/algorithms/triangulation.cpp
// @desc: Triangulation benchmark over a spiral with 10k points and 10 tours in [0,10000]^2 and random query segments in [0,10000]^2.
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

// Builds a simple spiral polygon: a corridor of constant width that winds
// around the origin `turns` times. The boundary runs outward along the inner
// wall (radius r0 + k*theta) and back along the outer wall (the same plus
// `width`), so it never self-intersects as long as width < pitch (= 2*pi*k).
template <class Number>
static pgl::Polygon<pgl::Point<Number>> makeSpiral(double r0, double k,
                                       double width, int turns, int stepsPerTurn) {
    const double thetaMax = turns * 2.0 * M_PI;
    const int steps = turns * stepsPerTurn;
    std::vector<pgl::Point<Number>> verts;
    auto add = [&](double t, double extra) {
        const double r = r0 + k * t + extra;
        const int x = static_cast<int>(std::lround(r * std::cos(t)));
        const int y = static_cast<int>(std::lround(r * std::sin(t)));
        // Skip repeated integer vertices so no edge collapses to zero length.
        if (verts.empty() || verts.back() != pgl::Point<Number>(x, y)) {
            verts.emplace_back(x, y);
        }
    };
    for (int i = 0; i <= steps; ++i) {        // outward along the inner wall
        add(thetaMax * i / steps, 0.0);
    }
    for (int i = steps; i >= 0; --i) {        // back along the outer wall
        add(thetaMax * i / steps, width);
    }

    auto poly = pgl::Polygon<pgl::Point<Number>>(verts);

    if (!poly.isSimple()) {
        std::cerr << "Spiral polygon is not simple!" << std::endl;
        std::exit(1);
    }
    // else {
    //     std::cout << "Spiral polygon is simple, vertices = " << poly.size() << std::endl;
    // }

    return poly;
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
    using Segment = pgl::OrientedSegment<Point>;
    const auto poly = makeSpiral<Number>(50.0, 75.0, 220.0, 10, 500) + Point(5000, 5000);
    const std::vector<Segment> segs = querySegments<Number>(1000);

    // Construction.
    plf::nanotimer timer;
    timer.start();
    pgl::Triangulation triangulation(poly);
    const double buildus = timer.get_elapsed_us();
    std::cout << "build (constrained Delaunay)\t" << label << "\t\t" << triangulation.triangles().size() << "\t" << buildus << std::endl;

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
