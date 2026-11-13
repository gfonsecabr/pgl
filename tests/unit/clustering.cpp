#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstddef>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "pgl.hpp"

namespace {

template <class Number>
using IdPoint = pgl::Point<Number, int>;

template <class Number>
using IdClusters = std::vector<std::vector<IdPoint<Number>>>;

static_assert(std::is_same_v<decltype(pgl::maximumSpacingClusters(
                                 std::declval<const std::vector<IdPoint<int>>&>(), 2)),
                             IdClusters<int>>);
static_assert(std::is_same_v<decltype(pgl::proximityClusters(
                                 std::declval<const std::vector<pgl::Point<double>>&>(), 2.0)),
                             std::vector<std::vector<pgl::Point<double>>>>);
static_assert(std::is_same_v<decltype(pgl::minimumDiameterBipartition(
                                 std::declval<const std::vector<IdPoint<int>>&>())),
                             IdClusters<int>>);
static_assert(std::is_same_v<decltype(pgl::kMeansClustering(
                                 std::declval<const std::vector<IdPoint<int>>&>(), 2)),
                             IdClusters<int>>);

template <class Number>
std::vector<IdPoint<Number>> labeled(const std::vector<std::pair<int, int>>& coordinates) {
    std::vector<IdPoint<Number>> points;
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        points.emplace_back(Number(coordinates[i].first), Number(coordinates[i].second),
                            static_cast<int>(i));
    }
    return points;
}

// Small coordinates, so that copies, ties and collinear runs are common.
template <class Number>
std::vector<IdPoint<Number>> randomPoints(std::mt19937& rng, std::size_t n, int range) {
    std::uniform_int_distribution<int> coordinate(0, range);
    std::vector<std::pair<int, int>> coordinates;
    for (std::size_t i = 0; i < n; ++i) {
        coordinates.emplace_back(coordinate(rng), coordinate(rng));
    }
    return labeled<Number>(coordinates);
}

template <class Number>
pgl::ERational exactSquaredDistance(const IdPoint<Number>& a, const IdPoint<Number>& b) {
    return a.template squaredDistance<pgl::ERational>(b);
}

// The clusters as label sets, in a canonical order.
template <class Number>
std::vector<std::vector<int>> labelsOf(const IdClusters<Number>& clusters) {
    std::vector<std::vector<int>> result;
    for (const auto& cluster : clusters) {
        std::vector<int> labels;
        for (const auto& p : cluster) {
            labels.push_back(p.label());
        }
        std::sort(labels.begin(), labels.end());
        result.push_back(std::move(labels));
    }
    std::sort(result.begin(), result.end());
    return result;
}

// Checks the shared output contract: a partition of the input, the points
// copied with their labels, clusters largest first and each in input order.
template <class Number>
void checkClusterContract(const std::vector<IdPoint<Number>>& points,
                          const IdClusters<Number>& clusters) {
    std::vector<int> seen(points.size(), 0);
    for (std::size_t c = 0; c < clusters.size(); ++c) {
        REQUIRE_FALSE(clusters[c].empty());
        if (c > 0) {
            CHECK(clusters[c - 1].size() >= clusters[c].size());
        }
        for (std::size_t j = 0; j < clusters[c].size(); ++j) {
            const auto& p = clusters[c][j];
            REQUIRE(p.label() >= 0);
            REQUIRE(static_cast<std::size_t>(p.label()) < points.size());
            CHECK(p == points[static_cast<std::size_t>(p.label())]);
            ++seen[static_cast<std::size_t>(p.label())];
            if (j > 0) {
                CHECK(clusters[c][j - 1].label() < p.label());
            }
        }
    }
    CHECK(std::all_of(seen.begin(), seen.end(), [](int count) { return count == 1; }));
}

template <class Number>
std::vector<std::size_t> clusterOfEachPoint(std::size_t n, const IdClusters<Number>& clusters) {
    std::vector<std::size_t> owner(n);
    for (std::size_t c = 0; c < clusters.size(); ++c) {
        for (const auto& p : clusters[c]) {
            owner[static_cast<std::size_t>(p.label())] = c;
        }
    }
    return owner;
}

// Smallest distance between points in different clusters.
template <class Number>
pgl::ERational spacing(const std::vector<IdPoint<Number>>& points, const IdClusters<Number>& clusters) {
    const auto owner = clusterOfEachPoint(points.size(), clusters);
    bool found = false;
    pgl::ERational best(0);
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            if (owner[i] != owner[j]) {
                const pgl::ERational d = exactSquaredDistance(points[i], points[j]);
                if (!found || d < best) {
                    best = d;
                    found = true;
                }
            }
        }
    }
    return best;
}

// Largest distance between points in one cluster.
template <class Number>
pgl::ERational largestDiameter(const std::vector<IdPoint<Number>>& points,
                               const std::vector<std::size_t>& owner) {
    pgl::ERational best(0);
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            if (owner[i] == owner[j]) {
                best = std::max(best, exactSquaredDistance(points[i], points[j]));
            }
        }
    }
    return best;
}

// Single linkage over every pair, stopped at k components.
template <class Number>
std::vector<std::size_t> bruteSingleLinkage(const std::vector<IdPoint<Number>>& points,
                                            std::size_t k) {
    const std::size_t n = points.size();
    std::vector<std::size_t> component(n);
    for (std::size_t i = 0; i < n; ++i) {
        component[i] = i;
    }
    std::vector<std::pair<pgl::ERational, std::pair<std::size_t, std::size_t>>> pairs;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            pairs.push_back({exactSquaredDistance(points[i], points[j]), {i, j}});
        }
    }
    std::sort(pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::size_t components = n;
    for (const auto& [d, pair] : pairs) {
        (void)d;
        if (components <= k) {
            break;
        }
        const std::size_t from = component[pair.second];
        const std::size_t to = component[pair.first];
        if (from != to) {
            std::replace(component.begin(), component.end(), from, to);
            --components;
        }
    }
    return component;
}

template <class Number>
IdClusters<Number> asClusters(const std::vector<IdPoint<Number>>& points,
                              const std::vector<std::size_t>& owner) {
    IdClusters<Number> clusters(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        clusters[owner[i]].push_back(points[i]);
    }
    clusters.erase(std::remove_if(clusters.begin(), clusters.end(),
                                  [](const auto& c) { return c.empty(); }),
                   clusters.end());
    return clusters;
}

template <class Number>
std::size_t distinctCount(std::vector<IdPoint<Number>> points) {
    std::sort(points.begin(), points.end());
    return static_cast<std::size_t>(std::unique(points.begin(), points.end()) - points.begin());
}

}  // namespace

TEST_CASE("maximumSpacingClusters splits at the widest gaps") {
    const auto points = labeled<int>({{0, 0}, {1, 0}, {10, 0}, {11, 1}, {12, 0}, {50, 50}});
    const auto clusters = pgl::maximumSpacingClusters(points, 3);
    checkClusterContract(points, clusters);
    CHECK(labelsOf(clusters) == std::vector<std::vector<int>>{{0, 1}, {2, 3, 4}, {5}});
    CHECK(clusters[0].size() == 3);
    CHECK(clusters[2].size() == 1);

    CHECK(pgl::maximumSpacingClusters(points, 1).size() == 1);
    CHECK(pgl::maximumSpacingClusters(points, 6).size() == 6);
}

TEST_CASE("maximumSpacingClusters handles collinear points and copies") {
    // Collinear: the Delaunay triangulation is empty.
    const auto line = labeled<int>({{6, 3}, {0, 0}, {2, 1}, {10, 5}, {4, 2}});
    CHECK(labelsOf(pgl::maximumSpacingClusters(line, 2)) ==
          std::vector<std::vector<int>>{{0, 1, 2, 4}, {3}});

    // Copies stay together until k exceeds the distinct points, then split.
    const auto copies = labeled<int>({{1, 1}, {5, 5}, {1, 1}, {5, 5}, {1, 1}});
    CHECK(labelsOf(pgl::maximumSpacingClusters(copies, 2)) ==
          std::vector<std::vector<int>>{{0, 2, 4}, {1, 3}});
    const auto four = pgl::maximumSpacingClusters(copies, 4);
    checkClusterContract(copies, four);
    CHECK(four.size() == 4);

    const auto single = labeled<int>({{3, 4}});
    CHECK(labelsOf(pgl::maximumSpacingClusters(single, 1)) == std::vector<std::vector<int>>{{0}});
}

TEST_CASE_TEMPLATE("maximumSpacingClusters matches single linkage over every pair", Number, int,
                   long long, double, pgl::ERational) {
    std::mt19937 rng(11);
    for (int trial = 0; trial < 300; ++trial) {
        const std::size_t n = 1 + static_cast<std::size_t>(trial % 14);
        const auto points = randomPoints<Number>(rng, n, trial % 3 == 0 ? 3 : 9);
        const std::size_t k = 1 + std::uniform_int_distribution<std::size_t>(0, n - 1)(rng);
        CAPTURE(trial);
        const auto clusters = pgl::maximumSpacingClusters(points, k);
        checkClusterContract(points, clusters);
        REQUIRE(clusters.size() == k);
        if (k > 1) {
            const auto reference = asClusters(points, bruteSingleLinkage(points, k));
            CHECK(spacing(points, clusters) == spacing(points, reference));
        }
    }
}

TEST_CASE("proximityClusters joins chains of short steps") {
    const auto points = labeled<int>({{0, 0}, {3, 0}, {6, 0}, {6, 4}, {20, 20}, {20, 20}});
    CHECK(labelsOf(pgl::proximityClusters(points, 9)) ==
          std::vector<std::vector<int>>{{0, 1, 2}, {3}, {4, 5}});
    CHECK(labelsOf(pgl::proximityClusters(points, 16)) ==
          std::vector<std::vector<int>>{{0, 1, 2, 3}, {4, 5}});
    CHECK(labelsOf(pgl::proximityClusters(points, 8)) ==
          std::vector<std::vector<int>>{{0}, {1}, {2}, {3}, {4, 5}});
    CHECK(labelsOf(pgl::proximityClusters(points, 0)) ==
          std::vector<std::vector<int>>{{0}, {1}, {2}, {3}, {4, 5}});

    // A rational threshold between two squared lengths.
    CHECK(labelsOf(pgl::proximityClusters(points, pgl::ERational(17, 2))) ==
          labelsOf(pgl::proximityClusters(points, 8)));

    CHECK(pgl::proximityClusters(std::vector<pgl::Point<int>>{}, 1).empty());
}

TEST_CASE_TEMPLATE("proximityClusters matches the components of every close pair", Number, int,
                   long long, double, pgl::ERational) {
    std::mt19937 rng(23);
    for (int trial = 0; trial < 300; ++trial) {
        const std::size_t n = static_cast<std::size_t>(trial % 16);
        const auto points = randomPoints<Number>(rng, n, trial % 3 == 0 ? 3 : 12);
        const int threshold = std::uniform_int_distribution<int>(0, 20)(rng);
        CAPTURE(trial);
        const auto clusters = pgl::proximityClusters(points, Number(threshold));
        checkClusterContract(points, clusters);

        std::vector<std::size_t> component(n);
        for (std::size_t i = 0; i < n; ++i) {
            component[i] = i;
        }
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                if (exactSquaredDistance(points[i], points[j]) <= pgl::ERational(threshold)) {
                    const std::size_t from = component[j];
                    const std::size_t to = component[i];
                    std::replace(component.begin(), component.end(), from, to);
                }
            }
        }
        CHECK(labelsOf(clusters) == labelsOf(asClusters(points, component)));
    }
}

TEST_CASE("minimumDiameterBipartition small cases") {
    CHECK(pgl::minimumDiameterBipartition(std::vector<IdPoint<int>>{}).empty());
    const auto one = labeled<int>({{2, 2}});
    CHECK(labelsOf(pgl::minimumDiameterBipartition(one)) == std::vector<std::vector<int>>{{0}});
    const auto two = labeled<int>({{2, 2}, {2, 2}});
    CHECK(labelsOf(pgl::minimumDiameterBipartition(two)) ==
          std::vector<std::vector<int>>{{0}, {1}});

    // Two far groups: each becomes a cluster.
    const auto groups = labeled<int>({{0, 0}, {100, 0}, {1, 1}, {101, 2}, {0, 2}, {99, 1}});
    const auto clusters = pgl::minimumDiameterBipartition(groups);
    checkClusterContract(groups, clusters);
    CHECK(labelsOf(clusters) == std::vector<std::vector<int>>{{0, 2, 4}, {1, 3, 5}});
}

TEST_CASE_TEMPLATE("minimumDiameterBipartition matches every split", Number, int, long long,
                   double, pgl::ERational) {
    std::mt19937 rng(37);
    for (int trial = 0; trial < 200; ++trial) {
        const std::size_t n = 2 + static_cast<std::size_t>(trial % 10);
        const auto points = randomPoints<Number>(rng, n, trial % 3 == 0 ? 3 : 10);
        CAPTURE(trial);
        const auto clusters = pgl::minimumDiameterBipartition(points);
        checkClusterContract(points, clusters);
        REQUIRE(clusters.size() == 2);

        pgl::ERational best(-1);
        for (std::size_t mask = 0; mask < (std::size_t{1} << (n - 1)); ++mask) {
            std::vector<std::size_t> owner(n);
            for (std::size_t i = 0; i < n; ++i) {
                owner[i] = (mask >> i) & 1;
            }
            const pgl::ERational d = largestDiameter(points, owner);
            if (best < pgl::ERational(0) || d < best) {
                best = d;
            }
        }
        CHECK(largestDiameter(points, clusterOfEachPoint(n, clusters)) == best);
    }
}

TEST_CASE_TEMPLATE("kMeansClustering reaches a fixed point of Lloyd's iteration", Number, int,
                   long long, pgl::ERational) {
    std::mt19937 rng(41);
    for (int trial = 0; trial < 150; ++trial) {
        const std::size_t n = 1 + static_cast<std::size_t>(trial % 30);
        const auto points = randomPoints<Number>(rng, n, trial % 3 == 0 ? 4 : 30);
        const std::size_t distinct = distinctCount(points);
        const std::size_t k = 1 + std::uniform_int_distribution<std::size_t>(0, distinct - 1)(rng);
        CAPTURE(trial);
        std::mt19937 generator(static_cast<unsigned>(trial));
        const auto clusters = pgl::kMeansClustering(points, k, generator);
        checkClusterContract(points, clusters);
        REQUIRE(clusters.size() == k);

        std::vector<pgl::EPoint> centroids;
        for (const auto& cluster : clusters) {
            pgl::ERational x(0), y(0);
            for (const auto& p : cluster) {
                x += pgl::ERational(p.x());
                y += pgl::ERational(p.y());
            }
            const pgl::ERational m(static_cast<long long>(cluster.size()));
            centroids.emplace_back(x / m, y / m);
        }
        for (std::size_t c = 0; c < clusters.size(); ++c) {
            for (const auto& p : clusters[c]) {
                const pgl::EPoint q(pgl::ERational(p.x()), pgl::ERational(p.y()));
                const pgl::ERational own = q.squaredDistance(centroids[c]);
                for (const auto& other : centroids) {
                    CHECK_FALSE(q.squaredDistance(other) < own);
                }
            }
        }
    }
}

TEST_CASE("kMeansClustering special cases") {
    const auto points = labeled<int>({{0, 0}, {1, 0}, {0, 1}, {40, 40}, {41, 40}, {40, 41}});
    const auto two = pgl::kMeansClustering(points, 2);
    checkClusterContract(points, two);
    CHECK(labelsOf(two) == std::vector<std::vector<int>>{{0, 1, 2}, {3, 4, 5}});
    // The default seed is fixed.
    CHECK(pgl::kMeansClustering(points, 3) == pgl::kMeansClustering(points, 3));

    CHECK(labelsOf(pgl::kMeansClustering(points, 1)) ==
          std::vector<std::vector<int>>{{0, 1, 2, 3, 4, 5}});

    // As many clusters as distinct points: copies share a cluster.
    const auto copies = labeled<int>({{3, 3}, {7, 1}, {3, 3}, {7, 1}, {0, 9}});
    CHECK(labelsOf(pgl::kMeansClustering(copies, 3)) ==
          std::vector<std::vector<int>>{{0, 2}, {1, 3}, {4}});

    // Floating-point coordinates follow the same contract.
    std::mt19937 rng(5);
    const auto floating = randomPoints<double>(rng, 40, 50);
    const auto clusters = pgl::kMeansClustering(floating, 4);
    checkClusterContract(floating, clusters);
    CHECK(clusters.size() == 4);
}
