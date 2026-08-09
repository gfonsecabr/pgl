// @desc: IntervalTree exact intersection counting on 10k long random segments
// whose x projections all overlap, compared with a direct vector scan.
#include <cstdint>
#include <iostream>
#include <vector>

#include "pgl.hpp"
#include "../plf_nanotimer.h"


#ifndef PGL_INTERVALTREE_SEGMENTS
#  define PGL_INTERVALTREE_SEGMENTS 10000
#endif

#ifndef PGL_INTERVALTREE_QUERIES
#  define PGL_INTERVALTREE_QUERIES 1000
#endif

constexpr int kSegments = PGL_INTERVALTREE_SEGMENTS;
constexpr int kQueries = PGL_INTERVALTREE_QUERIES;

struct Rng {
    std::uint64_t state;

    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }

    int range(int low, int high) {
        return low + static_cast<int>(next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;

std::vector<Segment> longSegments(int count, std::uint64_t seed) {
    Rng rng{seed};
    std::vector<Segment> segments;
    segments.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        segments.emplace_back(Point(rng.range(0, 100), rng.range(0, 10000)),
                              Point(rng.range(9900, 10000), rng.range(0, 10000)));
    }
    return segments;
}

int main() {
    const auto segments = longSegments(kSegments, 0x6a09e667f3bcc909ULL);
    const auto queries = longSegments(kQueries, 0xbb67ae8584caa73bULL);

    std::cout << "Operation\t\tNumber\t\tResult\tTime(μs)\n";

    plf::nanotimer timer;
    timer.start();
    const pgl::IntervalTree<Segment> tree(segments);
    std::cout << "build\t\t\tint\t\t" << tree.size() << '\t' << timer.get_elapsed_us() << '\n';

    // Every stored and query segment spans nearly the full x range, so the
    // tree's x-projection pruning rejects essentially no candidates.
    timer.start();
    std::size_t treeHits = 0;
    for (const Segment& query : queries) {
        treeHits += tree.countIntersecting(query);
    }
    std::cout << "countIntersecting\tint\t\t" << treeHits << '\t'
              << timer.get_elapsed_us() / queries.size() << '\n';

    timer.start();
    std::size_t vectorHits = 0;
    for (const Segment& query : queries) {
        for (const Segment& segment : segments) {
            if (segment.intersects(query)) {
                ++vectorHits;
            }
        }
    }
    std::cout << "countIntersectsVector\tint\t\t" << vectorHits << '\t'
              << timer.get_elapsed_us() / queries.size() << '\n';

    return treeHits == vectorHits ? 0 : 1;
}
