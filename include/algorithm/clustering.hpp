#pragma once

#include "implementation/simplification.hpp"

/**
 * @file clustering.hpp
 * @brief Partitions of a point set into clusters.
 *
 * Every function here takes a container of points and returns its clusters as
 * a `std::vector<std::vector<Point>>`: a partition of the input, each point
 * copied with its label, the clusters largest first and the points of each in
 * input order. Copies of one point count separately, so a point that occurs
 * twice in the input occurs twice in the output.
 *
 * Squared distances are compared exactly, one promotion above the coordinate
 * type as in the sign predicates, so integer and rational input never meets a
 * rounding error.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

template <class Container>
using cluster_input_point_t =
    std::remove_cvref_t<decltype(*std::begin(std::declval<const Container&>()))>;

/** @brief Type the squared distance between two input points is computed in. */
template <class PointType>
using cluster_distance_t = detail::promoted_number_t<typename PointType::NumberType>;

template <class PointType>
using Clusters = std::vector<std::vector<PointType>>;

/** @brief Disjoint sets over `0..n-1`, with union by size and path halving. */
class ClusterUnionFind {
  public:
    explicit ClusterUnionFind(std::size_t n) : parent_(n), size_(n, 1), components_(n) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }

    [[nodiscard]] std::size_t find(std::size_t v) {
        while (parent_[v] != v) {
            parent_[v] = parent_[parent_[v]];
            v = parent_[v];
        }
        return v;
    }

    bool unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a == b) {
            return false;
        }
        if (size_[a] < size_[b]) {
            std::swap(a, b);
        }
        parent_[b] = a;
        size_[a] += size_[b];
        --components_;
        return true;
    }

    [[nodiscard]] std::size_t components() const { return components_; }

  private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> size_;
    std::size_t components_;
};

/**
 * @brief Groups the points by cluster number, dropping empty clusters.
 *
 * Points keep their input order inside a cluster, and clusters of equal size
 * keep the order of their first points.
 */
template <class PointType>
[[nodiscard]] Clusters<PointType> clustersFromAssignment(const std::vector<PointType>& points,
                                                         const std::vector<std::size_t>& cluster,
                                                         std::size_t count) {
    Clusters<PointType> grouped(count);
    for (std::size_t i = 0; i < points.size(); ++i) {
        grouped[cluster[i]].push_back(points[i]);
    }
    // Number the clusters by first appearance, so the order among equal sizes
    // does not depend on how the caller numbered them.
    std::vector<std::size_t> rank(count, count);
    std::size_t next = 0;
    for (std::size_t c : cluster) {
        if (rank[c] == count) {
            rank[c] = next++;
        }
    }
    Clusters<PointType> result(next);
    for (std::size_t c = 0; c < count; ++c) {
        if (rank[c] != count) {
            result[rank[c]] = std::move(grouped[c]);
        }
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const auto& a, const auto& b) { return a.size() > b.size(); });
    return result;
}

template <class PointType>
[[nodiscard]] Clusters<PointType> clustersFromUnionFind(const std::vector<PointType>& points,
                                                        ClusterUnionFind& sets) {
    std::vector<std::size_t> cluster(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        cluster[i] = sets.find(i);
    }
    return clustersFromAssignment(points, cluster, points.size());
}

template <class Distance>
struct ClusterEdge {
    std::size_t first;
    std::size_t second;
    Distance squaredLength;
};

/**
 * @brief Edges, between input indices, that contain a Euclidean minimum
 *        spanning tree of the input.
 *
 * Copies of one point are chained by edges of length zero, and the distinct
 * points are joined by the edges of their Delaunay triangulation, which
 * contains a minimum spanning tree of them. When the distinct points are
 * collinear the triangulation is empty, and the path through them in
 * lexicographic order, which is their order along the line, takes its place.
 */
template <class PointType>
[[nodiscard]] std::vector<ClusterEdge<cluster_distance_t<PointType>>>
clusterCandidateEdges(const std::vector<PointType>& points) {
    using Number = typename PointType::NumberType;
    using Plain = Point<Number>;
    using Distance = cluster_distance_t<PointType>;

    const std::size_t n = points.size();
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return points[a] < points[b]; });

    std::vector<ClusterEdge<Distance>> edges;
    std::vector<Plain> distinct;           // lexicographically sorted
    std::vector<std::size_t> representative;  // input index of each distinct point
    for (std::size_t k = 0; k < n; ++k) {
        const PointType& p = points[order[k]];
        if (k > 0 && p == points[order[k - 1]]) {
            edges.push_back({order[k - 1], order[k], Distance(0)});
        } else {
            distinct.emplace_back(p.x(), p.y());
            representative.push_back(order[k]);
        }
    }

    const std::size_t m = distinct.size();
    const auto join = [&](std::size_t i, std::size_t j) {
        edges.push_back({representative[i], representative[j],
                         distinct[i].template squaredDistance<Distance>(distinct[j])});
    };
    const auto indexOf = [&](const Plain& p) {
        return static_cast<std::size_t>(
            std::lower_bound(distinct.begin(), distinct.end(), p) - distinct.begin());
    };

    bool triangulated = false;
    if (m >= 3) {
        const Triangulation<Triangle<Plain>> mesh(distinct);
        if (mesh.numTriangles() > 0) {
            triangulated = true;
            mesh.visitEdges([&](const auto& edge) { join(indexOf(edge.min()), indexOf(edge.max())); });
        }
    }
    if (!triangulated) {
        for (std::size_t i = 1; i < m; ++i) {
            join(i - 1, i);
        }
    }
    return edges;
}

/** @brief Whether `na / da < nb / db`, for positive denominators. */
template <class W>
[[nodiscard]] bool clusterFractionLess(const W& na, const W& da, const W& nb, const W& db) {
    if constexpr (extended_integral<W>) {
        // Cross-multiplying would square the magnitudes; comparing the integer
        // parts first leaves only remainders, each below its denominator.
        const W qa = na / da;
        const W qb = nb / db;
        if (qa != qb) {
            return qa < qb;
        }
        return (na % da) * db < (nb % db) * da;
    } else {
        return na * db < nb * da;
    }
}

}  // namespace detail

/**
 * @brief Partitions points into @p k clusters that are as far apart as
 *        possible.
 *
 * The result maximizes the smallest distance between two points in different
 * clusters: it is the partition single-linkage clustering reaches when @p k
 * clusters remain. Ties between equally good partitions are broken
 * arbitrarily.
 *
 * Complexity: the construction time of the Delaunay triangulation of the
 * distinct points, plus @f$O(n \log n)@f$.
 *
 * @tparam Container Range of `pgl::Point`.
 * @param input The points.
 * @param k Number of clusters.
 * @pre `1 <= k <= n`, for the @f$n@f$ points of @p input.
 * @return The @p k clusters, largest first, each point in input order.
 */
template <class Container>
[[nodiscard]] detail::Clusters<detail::cluster_input_point_t<Container>>
maximumSpacingClusters(const Container& input, std::size_t k) {
    using InputPoint = detail::cluster_input_point_t<Container>;
    static_assert(PointConcept<InputPoint>,
                  "maximumSpacingClusters requires a container of pgl::Point values");

    std::vector<InputPoint> points(std::begin(input), std::end(input));
    PGL_ASSERT(k >= 1 && k <= points.size());

    auto edges = detail::clusterCandidateEdges(points);
    std::sort(edges.begin(), edges.end(),
              [](const auto& a, const auto& b) { return a.squaredLength < b.squaredLength; });
    detail::ClusterUnionFind sets(points.size());
    for (const auto& edge : edges) {
        if (sets.components() <= k) {
            break;
        }
        sets.unite(edge.first, edge.second);
    }
    return detail::clustersFromUnionFind(points, sets);
}

/**
 * @brief Partitions points into the clusters of points chained by short
 *        steps.
 *
 * Two points share a cluster when a sequence of input points leads from one to
 * the other with every step at squared distance at most @p squaredThreshold:
 * the clusters are the connected components of the graph joining every such
 * pair. A point with no other point that close forms a cluster of its own.
 *
 * Complexity: the construction time of the Delaunay triangulation of the
 * distinct points, plus @f$O(n \log n)@f$.
 *
 * @tparam Container Range of `pgl::Point`.
 * @tparam Threshold Number type comparable with squared distances.
 * @param input The points.
 * @param squaredThreshold Square of the longest step allowed.
 * @return The clusters, largest first, each point in input order.
 */
template <class Container, class Threshold>
[[nodiscard]] detail::Clusters<detail::cluster_input_point_t<Container>>
proximityClusters(const Container& input, const Threshold& squaredThreshold) {
    using InputPoint = detail::cluster_input_point_t<Container>;
    static_assert(PointConcept<InputPoint>,
                  "proximityClusters requires a container of pgl::Point values");

    std::vector<InputPoint> points(std::begin(input), std::end(input));
    detail::ClusterUnionFind sets(points.size());
    // Any MST is a minimum bottleneck spanning tree, so two points within the
    // threshold are joined by a path of candidate edges within it too.
    for (const auto& edge : detail::clusterCandidateEdges(points)) {
        if (!(squaredThreshold < edge.squaredLength)) {
            sets.unite(edge.first, edge.second);
        }
    }
    return detail::clustersFromUnionFind(points, sets);
}

/**
 * @brief Splits points into two clusters minimizing the larger diameter.
 *
 * The result minimizes the largest distance between two points in the same
 * cluster. Ties between equally good splits are broken arbitrarily. A single
 * point forms one cluster and an empty input gives none; from two points on
 * there are always two clusters.
 *
 * Complexity: @f$O(n^2)@f$ time and @f$O(n)@f$ space.
 *
 * @tparam Container Range of `pgl::Point`.
 * @param input The points.
 * @return The clusters, largest first, each point in input order.
 */
template <class Container>
[[nodiscard]] detail::Clusters<detail::cluster_input_point_t<Container>>
minimumDiameterBipartition(const Container& input) {
    using InputPoint = detail::cluster_input_point_t<Container>;
    using Distance = detail::cluster_distance_t<InputPoint>;
    static_assert(PointConcept<InputPoint>,
                  "minimumDiameterBipartition requires a container of pgl::Point values");

    std::vector<InputPoint> points(std::begin(input), std::end(input));
    const std::size_t n = points.size();
    if (n == 0) {
        return {};
    }

    // Two-color a maximum spanning tree, grown by Prim. Two points of one
    // color close an odd cycle with the tree path between them, and every edge
    // of that cycle is at least as long as the pair (cycle property), so any
    // split has a pair at least that far apart within one cluster.
    const auto distance = [&](std::size_t a, std::size_t b) {
        return points[a].template squaredDistance<Distance>(points[b]);
    };
    std::vector<Distance> farthest(n);
    std::vector<std::size_t> parent(n, 0);
    std::vector<bool> inTree(n, false);
    std::vector<std::size_t> side(n, 0);
    inTree[0] = true;
    for (std::size_t v = 1; v < n; ++v) {
        farthest[v] = distance(0, v);
    }
    for (std::size_t step = 1; step < n; ++step) {
        std::size_t u = n;
        for (std::size_t v = 0; v < n; ++v) {
            if (!inTree[v] && (u == n || farthest[u] < farthest[v])) {
                u = v;
            }
        }
        inTree[u] = true;
        side[u] = 1 - side[parent[u]];
        for (std::size_t v = 0; v < n; ++v) {
            if (!inTree[v]) {
                Distance d = distance(u, v);
                if (farthest[v] < d) {
                    farthest[v] = std::move(d);
                    parent[v] = u;
                }
            }
        }
    }
    return detail::clustersFromAssignment(points, side, 2);
}

/**
 * @brief Partitions points into @p k clusters by Lloyd's k-means iteration.
 *
 * The result is a fixed point of the iteration: no point is strictly closer to
 * the centroid of another cluster than to the centroid of its own, and no
 * cluster is empty. It is a local optimum of the sum of squared distances to
 * the centroids, not necessarily the global one. The starting centers are
 * input points drawn by k-means++ seeding from @p generator.
 *
 * Integer and rational coordinates are compared exactly, two promotions above
 * the coordinate type, so the iteration provably stops at a fixed point.
 * Floating-point coordinates are compared in their promoted floating-point type
 * (`long double` for `double`), which rounds: the fixed point then holds only
 * up to rounding, and the iteration also stops at the first round that fails
 * to lower the computed sum of squared distances, keeping the partition before
 * it.
 *
 * The seeding draws its weights in `double` whatever the coordinate type. That
 * only perturbs the probabilities: a point coinciding with a center already
 * chosen is never drawn.
 *
 * Complexity: @f$O(nk)@f$ time per round. The number of rounds can be
 * exponential in @f$n@f$.
 *
 * @tparam Container Range of `pgl::Point`.
 * @tparam UniformRandomBitGenerator Random engine for the seeding.
 * @param input The points.
 * @param k Number of clusters.
 * @param generator Random engine.
 * @pre `k >= 1` and @p input holds at least @p k distinct points.
 * @return The @p k clusters, largest first, each point in input order.
 */
template <class Container, class UniformRandomBitGenerator>
[[nodiscard]] detail::Clusters<detail::cluster_input_point_t<Container>>
kMeansClustering(const Container& input, std::size_t k, UniformRandomBitGenerator&& generator) {
    using InputPoint = detail::cluster_input_point_t<Container>;
    using Number = typename InputPoint::NumberType;
    using Distance = detail::cluster_distance_t<InputPoint>;
    // Coordinate sums and the scaled differences m p - S, two promotions up.
    using Wide = detail::promoted_number_t<detail::promoted_number_t<Number>>;
    static_assert(PointConcept<InputPoint>,
                  "kMeansClustering requires a container of pgl::Point values");

    std::vector<InputPoint> points(std::begin(input), std::end(input));
    const std::size_t n = points.size();
    PGL_ASSERT(k >= 1 && k <= n);

    // k-means++ seeding: each new center is an input point drawn with
    // probability proportional to its squared distance to the nearest center
    // so far. The weights are drawn in double, which only perturbs the
    // probabilities; a point at distance zero is never drawn.
    std::vector<std::size_t> centers;
    centers.reserve(k);
    centers.push_back(std::uniform_int_distribution<std::size_t>(0, n - 1)(generator));
    std::vector<Distance> nearest(n);
    for (std::size_t i = 0; i < n; ++i) {
        nearest[i] = points[i].template squaredDistance<Distance>(points[centers[0]]);
    }
    std::vector<double> cumulative(n);
    while (centers.size() < k) {
        double total = 0;
        for (std::size_t i = 0; i < n; ++i) {
            total += static_cast<double>(nearest[i]);
            cumulative[i] = total;
        }
        std::size_t chosen = n;
        if (total > 0 && std::isfinite(total)) {
            const double r = std::uniform_real_distribution<double>(0.0, total)(generator);
            chosen = static_cast<std::size_t>(
                std::upper_bound(cumulative.begin(), cumulative.end(), r) - cumulative.begin());
        }
        if (chosen >= n || !(Distance(0) < nearest[chosen])) {
            chosen = 0;
            while (chosen < n && !(Distance(0) < nearest[chosen])) {
                ++chosen;
            }
        }
        PGL_ASSERT(chosen < n);  // fewer than k distinct points
        centers.push_back(chosen);
        for (std::size_t i = 0; i < n; ++i) {
            Distance d = points[i].template squaredDistance<Distance>(points[chosen]);
            if (d < nearest[i]) {
                nearest[i] = std::move(d);
            }
        }
    }

    // A cluster is kept as the sum S of its points and their count m. The
    // squared distance from p to the centroid S / m is |m p - S|^2 / m^2,
    // compared as that fraction so that no centroid is ever divided out.
    struct Sum {
        Wide x{0};
        Wide y{0};
        std::size_t count = 0;
    };
    struct Key {
        Wide numerator;
        Wide denominator;
    };
    std::vector<Sum> sums(k);
    const auto keyOf = [&](std::size_t i, const Sum& sum) {
        const Wide m(static_cast<long long>(sum.count));
        const Wide dx = m * detail::asNumber<Wide>(points[i].x()) - sum.x;
        const Wide dy = m * detail::asNumber<Wide>(points[i].y()) - sum.y;
        return Key{dx * dx + dy * dy, m * m};
    };
    const auto less = [](const Key& a, const Key& b) {
        return detail::clusterFractionLess(a.numerator, a.denominator, b.numerator, b.denominator);
    };
    const auto add = [&](Sum& sum, std::size_t i) {
        sum.x += detail::asNumber<Wide>(points[i].x());
        sum.y += detail::asNumber<Wide>(points[i].y());
        ++sum.count;
    };
    const auto remove = [&](Sum& sum, std::size_t i) {
        sum.x -= detail::asNumber<Wide>(points[i].x());
        sum.y -= detail::asNumber<Wide>(points[i].y());
        --sum.count;
    };

    // Each center is at distance zero from itself and positive from the
    // others, so every first cluster holds at least its center.
    for (std::size_t c = 0; c < k; ++c) {
        add(sums[c], centers[c]);
    }
    std::vector<std::size_t> assignment(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t best = 0;
        Key bestKey = keyOf(i, sums[0]);
        for (std::size_t c = 1; c < k; ++c) {
            Key key = keyOf(i, sums[c]);
            if (less(key, bestKey)) {
                best = c;
                bestKey = std::move(key);
            }
        }
        assignment[i] = best;
    }

    constexpr bool exact = !std::is_floating_point_v<Wide>;
    [[maybe_unused]] Wide lowestCost(0);
    [[maybe_unused]] std::vector<std::size_t> lowestAssignment;
    for (std::size_t round = 0;; ++round) {
        sums.assign(k, Sum{});
        for (std::size_t i = 0; i < n; ++i) {
            add(sums[assignment[i]], i);
        }
        // Refill an empty cluster with the point farthest from its own
        // centroid. That point is at positive distance, since otherwise every
        // cluster would hold copies of a single point, and moving it lowers the
        // cost, so the iteration still cannot cycle.
        for (std::size_t c = 0; c < k; ++c) {
            if (sums[c].count != 0) {
                continue;
            }
            std::size_t worst = n;
            Key worstKey{};
            for (std::size_t i = 0; i < n; ++i) {
                if (sums[assignment[i]].count < 2) {
                    continue;
                }
                Key key = keyOf(i, sums[assignment[i]]);
                if (worst == n || less(worstKey, key)) {
                    worst = i;
                    worstKey = std::move(key);
                }
            }
            PGL_ASSERT(worst < n);  // fewer than k distinct points
            remove(sums[assignment[worst]], worst);
            assignment[worst] = c;
            add(sums[c], worst);
        }
        // Rounding can let a round undo the previous one, so floating-point
        // coordinates also require the computed cost to fall strictly. A
        // strictly decreasing sequence of floating-point values is finite, so
        // this stops; exact arithmetic never fails the test before the
        // assignment settles.
        if constexpr (!exact) {
            Wide cost(0);
            for (std::size_t i = 0; i < n; ++i) {
                const Key key = keyOf(i, sums[assignment[i]]);
                cost += key.numerator / key.denominator;
            }
            if (round > 0 && !(cost < lowestCost)) {
                assignment = std::move(lowestAssignment);
                break;
            }
            lowestCost = cost;
            lowestAssignment = assignment;
        }

        bool changed = false;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t current = assignment[i];
            std::size_t best = current;
            Key bestKey = keyOf(i, sums[current]);
            for (std::size_t c = 0; c < k; ++c) {
                if (c == current) {
                    continue;
                }
                Key key = keyOf(i, sums[c]);
                if (less(key, bestKey)) {
                    best = c;
                    bestKey = std::move(key);
                }
            }
            if (best != current) {
                assignment[i] = best;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }
    return detail::clustersFromAssignment(points, assignment, k);
}

/**
 * @brief Partitions points into @p k clusters by Lloyd's k-means iteration,
 *        seeded from a fixed generator.
 *
 * Same as the generator-taking overload, with a fixed seed, so the same input
 * always yields the same clusters.
 */
template <class Container>
[[nodiscard]] detail::Clusters<detail::cluster_input_point_t<Container>>
kMeansClustering(const Container& input, std::size_t k) {
    std::mt19937 generator(0x50474cU);  // "PGL"
    return kMeansClustering(input, k, generator);
}

}  // namespace pgl
