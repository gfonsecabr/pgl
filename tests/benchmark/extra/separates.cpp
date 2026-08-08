// @desc: Cut predicates (separates / crosses) over the cell decomposition of two overlapping combed regions with holes.
//
// These are the predicates whose engine is the arrangement of both operands'
// boundaries: `separates` asks whether removing one operand leaves the other in
// two or more pieces, and `crosses` asks it both ways. The interesting cost is
// entirely in that decomposition, so the operands here are built to make it
// large and awkward rather than random: two combs whose teeth interlock, each
// punched with a row of square holes, so that the arrangement is full of
// crossings and each region carries holes, pinches and shared vertices.
//
// The disk direction is measured too. It is the one caller left that still
// triangulates, so it is the one to watch when that changes.
#include <cstdint>
#include <iostream>
#include <vector>
#include "pgl.hpp"
#include "../plf_nanotimer.h"

constexpr int kUnit = 12;
constexpr int kTeeth[] = {4, 8, 16, 32};

// A comb: a rectangular body with `teeth` square teeth hanging off its bottom
// edge, and one square hole under each tooth. Given counterclockwise, which is
// what Polygon's canonical form wants anyway.
template <class Number>
pgl::PolygonWithHoles<pgl::Point<Number>> comb(int teeth, int unit, int shiftX, int shiftY,
                                               bool flipped) {
    using Point = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;

    const int height = 6 * unit;
    const int width = 4 * teeth * unit;
    const auto place = [&](int x, int y) {
        return Point(Number(shiftX + x), Number(shiftY + (flipped ? height - y : y)));
    };

    std::vector<Point> outer;
    for (int i = 0; i < teeth; ++i) {
        const int base = 4 * i * unit;
        outer.push_back(place(base, 0));
        outer.push_back(place(base + unit, -2 * unit));
        outer.push_back(place(base + 3 * unit, -2 * unit));
        outer.push_back(place(base + 4 * unit, 0));
    }
    outer.push_back(place(width, height));
    outer.push_back(place(0, height));

    std::vector<Polygon> holes;
    for (int i = 0; i < teeth; ++i) {
        const int base = 4 * i * unit;
        holes.emplace_back(std::vector<Point>{place(base + unit, 2 * unit),
                                              place(base + 2 * unit, 2 * unit),
                                              place(base + 2 * unit, 3 * unit),
                                              place(base + unit, 3 * unit)});
    }
    if (flipped) {
        std::reverse(outer.begin(), outer.end());
    }
    return pgl::PolygonWithHoles<Point>(Polygon(outer), holes);
}

template <class Number>
void run(const char* label) {
    using Point = pgl::Point<Number>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    plf::nanotimer timer;
    for (const int teeth : kTeeth) {
        // The second comb is turned over and pushed halfway into the first, so
        // the teeth interlock and every tooth cuts the other body.
        const auto a = comb<Number>(teeth, kUnit, 0, 0, false);
        const auto b = comb<Number>(teeth, kUnit, 2 * kUnit, -3 * kUnit, true);
        // A crosscut of the body, above the holes and clear of every edge of
        // the region, so it severs the comb whatever its size — the direction
        // where a one-dimensional operand is the remover, and every tooth of
        // the comb ends up in a piece of its own.
        const Segment slash(Point(Number(0), Number(4 * kUnit)),
                            Point(Number(4 * teeth * kUnit), Number(4 * kUnit)));
        const Disk disk(Point(Number(2 * teeth * kUnit), Number(2 * kUnit)),
                        Number(2 * teeth * kUnit));

        const auto report = [&](const char* what, bool result, double microseconds) {
            std::cout << what << "\tteeth=" << teeth << "\t" << label << "\t" << (result ? 1 : 0)
                      << "\t" << microseconds << std::endl;
        };

        timer.start();
        const bool separates = a.separates(b);
        report("separates region/region", separates, timer.get_elapsed_us());

        timer.start();
        const bool crosses = a.crosses(b);
        report("crosses region/region\t", crosses, timer.get_elapsed_us());

        timer.start();
        const bool cut = slash.separates(a);
        report("separates segment/region", cut, timer.get_elapsed_us());

        timer.start();
        const bool bitten = a.separates(disk);
        report("separates region/disk\t", bitten, timer.get_elapsed_us());
    }
}

int main() {
    std::cout << "Operation\t\t\tNumber\t\tResult\tTime(μs)" << std::endl;

    run<int>("int");
    run<pgl::Rational<>>("Rational");

    return 0;
}
