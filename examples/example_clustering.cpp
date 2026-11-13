// Clusters one point set four ways and draws the results side by side.
//
// The points form two round blobs, a small tight one, a long arc made of
// evenly spaced points, and a few stragglers. Each panel colors the clusters
// one function returns and shades their convex hulls:
//
//  - maximumSpacingClusters(V, 7) keeps the 7 groups farthest apart: the four
//    groups and the three stragglers. It follows the arc around its bend, since
//    no gap along it is as wide as the gaps between the groups.
//  - kMeansClustering(V, 4) looks for compact groups around their centroids
//    instead, so it cuts the arc in two and hands the small blob and the
//    stragglers to whichever centroid is nearest.
//  - proximityClusters(V, 36) links every two points at distance at most 6;
//    a straggler with nothing that close stays alone.
//  - minimumDiameterBipartition(V) splits the set into the two halves whose
//    larger diameter is as small as possible.
//
// Squared distances of integer points are compared exactly, so none of the
// results depend on rounding.
//
// Output: example_clustering.svg

#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Clusters = std::vector<std::vector<Point>>;

// Appends n points drawn from a round normal distribution, rounded to integers.
void addBlob(std::vector<Point>& points, std::mt19937& rng, Point center, double sigma,
             std::size_t n) {
    std::normal_distribution<double> offset(0.0, sigma);
    for (std::size_t i = 0; i < n; ++i) {
        points.emplace_back(center.x() + static_cast<int>(std::lround(offset(rng))),
                            center.y() + static_cast<int>(std::lround(offset(rng))));
    }
}

// Appends n points evenly spaced along the lower half of a circle.
void addArc(std::vector<Point>& points, Point center, double radius, std::size_t n) {
    const double pi = std::acos(-1.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double angle = pi + pi * static_cast<double>(i) / static_cast<double>(n - 1);
        points.emplace_back(center.x() + static_cast<int>(std::lround(radius * std::cos(angle))),
                            center.y() + static_cast<int>(std::lround(radius * std::sin(angle))));
    }
}

// Draws one panel: each cluster's hull and points in its own color, shifted by
// offset, with a title above.
void drawPanel(pgl::Canvas& canvas, const Clusters& clusters, Point offset,
               const std::string& title) {
    const std::vector<std::string> colors = {
        "#2563eb", "#dc2626", "#16a34a", "#d97706", "#9333ea", "#0891b2", "#db2777",
    };
    for (std::size_t i = 0; i < clusters.size(); ++i) {
        // A cluster of one point is drawn grey: it has nothing close to it.
        const std::string color = clusters[i].size() == 1 ? "#94a3b8" : colors[i % colors.size()];
        std::vector<Point> shifted;
        for (const Point& p : clusters[i]) {
            shifted.push_back(p + offset);
        }
        canvas << pgl::stroke(color) << pgl::fill(color) << pgl::fillOpacity("15%")
               << pgl::Convex<Point>(shifted);
        canvas << pgl::fillOpacity("100%") << pgl::stroke("white") << shifted;
    }
    canvas << pgl::stroke("#1e293b")
           << pgl::Text(title, offset + Point(50, 108), 7.0);
}

int main() {
    std::mt19937 rng(7);
    std::vector<Point> points;
    addBlob(points, rng, Point(20, 82), 5.0, 25);
    addBlob(points, rng, Point(78, 80), 6.0, 25);
    addBlob(points, rng, Point(50, 52), 2.5, 10);
    addArc(points, Point(50, 42), 36.0, 24);
    points.insert(points.end(), {Point(48, 98), Point(4, 6), Point(97, 8)});

    const Clusters spacing = pgl::maximumSpacingClusters(points, 7);
    const Clusters kMeans = pgl::kMeansClustering(points, 4);
    const Clusters proximity = pgl::proximityClusters(points, 36);
    const Clusters bipartition = pgl::minimumDiameterBipartition(points);

    pgl::Canvas canvas;
    canvas << pgl::pointRadius("4");
    drawPanel(canvas, spacing, Point(0, 130), "maximumSpacingClusters(V, 7)");
    drawPanel(canvas, kMeans, Point(130, 130), "kMeansClustering(V, 4)");
    drawPanel(canvas, proximity, Point(0, 0), "proximityClusters(V, 36)");
    drawPanel(canvas, bipartition, Point(130, 0), "minimumDiameterBipartition(V)");

    canvas.writeSVG("example_clustering.svg");

    const auto sizes = [](const Clusters& clusters) {
        std::string text;
        for (const auto& cluster : clusters) {
            text += (text.empty() ? "" : " ") + std::to_string(cluster.size());
        }
        return text;
    };
    std::cout << points.size() << " points\n"
              << "maximum spacing:   " << sizes(spacing) << '\n'
              << "k-means:           " << sizes(kMeans) << '\n'
              << "proximity:         " << sizes(proximity) << '\n'
              << "min-diameter split: " << sizes(bipartition) << '\n'
              << "wrote example_clustering.svg\n";
    return 0;
}
