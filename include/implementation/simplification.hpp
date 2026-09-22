#pragma once

#include "algorithm/emptypolygons.hpp"

/**
 * @file simplification.hpp
 * @brief Vertex-subset simplification of chains, rings and regions.
 *
 * Every `simplified(squaredTolerance)` keeps a subsequence of the shape's
 * vertices, so it never constructs a coordinate: the point type and the labels
 * come through unchanged, and an integral shape stays exact. The vertices go in
 * Douglas–Peucker order: a *shortcut* from `a` to `b` replaces the stretch of
 * boundary between them when every vertex of the stretch lies within the
 * tolerance of the segment `ab`, and otherwise the stretch is split at its
 * vertex farthest from `ab` and each half is tried in turn. Since the whole
 * stretch then lies in the tolerance neighbourhood of `ab`, and every point of
 * `ab` is within the tolerance of the stretch, the Hausdorff distance between
 * the result and the shape is at most the tolerance.
 *
 * A ring that must stay simple, and a region whose rings must keep their
 * nesting, go through a repair loop afterwards: a shortcut that touches any
 * other edge anywhere but at its own endpoints — which are vertices of the
 * input, and so were contacts in the input too — is split again, and so is
 * every shortcut of two rings whose nesting changed. Each round keeps at least
 * one more vertex, so the loop ends, at the latest on the input itself.
 */

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace pgl::detail {

/**
 * @brief The type the farthest-vertex search compares in: `|ab|²` times a
 *        squared distance is a polynomial of degree four in the coordinates,
 *        the degree of the in-circle determinant.
 */
template <class PointType>
using simplification_key_t = incircle_coordinate_t<typename PointType::NumberType>;

/**
 * @brief `|ab|²` times the squared distance from @p p to the segment `ab`, or
 *        the plain squared distance to `a` when `a == b`.
 *
 * Every point of one shortcut shares the factor `|ab|²`, so comparing keys
 * compares distances without dividing.
 */
template <class PointType>
simplification_key_t<PointType> shortcutKey(const PointType& a, const PointType& b,
                                            const PointType& p) {
    using Key = simplification_key_t<PointType>;
    const Point<Key> wa(a);
    const Point<Key> wb(b);
    const Point<Key> wp(p);
    const auto ab = wb - wa;
    const auto ap = wp - wa;
    const Key length = ab * ab;
    if (length == 0) {
        return ap * ap;
    }
    const Key along = ap * ab;
    if (along <= 0) {
        return (ap * ap) * length;
    }
    if (along >= length) {
        const auto bp = wp - wb;
        return (bp * bp) * length;
    }
    const Key cross = ab.x() * ap.y() - ab.y() * ap.x();
    return cross * cross;
}

/**
 * @brief Douglas–Peucker over index ranges of one vertex sequence, with the
 *        tolerance held in the exact result type of the coordinates.
 */
template <class PointType>
class Simplifier {
public:
    using NumberType = typename PointType::NumberType;
    using ToleranceType = division_result_t<NumberType>;

    template <class Tolerance>
    explicit Simplifier(const Tolerance& squaredTolerance)
        : tolerance_(static_cast<ToleranceType>(squaredTolerance)) {}

    /**
     * @brief The vertex strictly between @p first and @p last farthest from the
     *        segment joining them, the earliest of those tied.
     */
    std::size_t farthest(const std::vector<PointType>& points, std::size_t first,
                         std::size_t last) const {
        std::size_t best = first + 1;
        auto bestKey = shortcutKey(points[first], points[last], points[best]);
        for (std::size_t k = first + 2; k < last; ++k) {
            auto key = shortcutKey(points[first], points[last], points[k]);
            if (bestKey < key) {
                bestKey = std::move(key);
                best = k;
            }
        }
        return best;
    }

    /** @brief Whether @p p lies within the tolerance of the segment `ab`. */
    bool within(const PointType& a, const PointType& b, const PointType& p) const {
        using Plain = Point<NumberType>;
        return Segment<Plain>(Plain(a), Plain(b)).template squaredDistance<ToleranceType>(Plain(p)) <=
               tolerance_;
    }

    /**
     * @brief Marks in @p keep the vertices of `[first, last]` that
     *        Douglas–Peucker keeps, both ends included.
     */
    void run(const std::vector<PointType>& points, std::size_t first, std::size_t last,
             std::vector<char>& keep) const {
        std::vector<std::pair<std::size_t, std::size_t>> pending{{first, last}};
        while (!pending.empty()) {
            const auto [i, j] = pending.back();
            pending.pop_back();
            keep[i] = 1;
            keep[j] = 1;
            if (j - i < 2) {
                continue;
            }
            const std::size_t k = farthest(points, i, j);
            if (within(points[i], points[j], points[k])) {
                continue;
            }
            pending.emplace_back(k, j);
            pending.emplace_back(i, k);
        }
    }

    /**
     * @brief Splits the accepted shortcut from @p first to @p last at its
     *        farthest vertex, then runs each half again.
     */
    void refine(const std::vector<PointType>& points, std::size_t first, std::size_t last,
                std::vector<char>& keep) const {
        const std::size_t k = farthest(points, first, last);
        keep[k] = 1;
        run(points, first, k, keep);
        run(points, k, last, keep);
    }

private:
    ToleranceType tolerance_;
};

/** @brief The vertices of an open chain that Douglas–Peucker keeps. */
template <class PointType>
std::vector<PointType> simplifyChain(const std::vector<PointType>& points,
                                     const Simplifier<PointType>& simplifier) {
    const std::size_t n = points.size();
    if (n <= 2) {
        return points;
    }
    std::vector<char> keep(n, 0);
    simplifier.run(points, 0, n - 1, keep);
    std::vector<PointType> kept;
    for (std::size_t k = 0; k < n; ++k) {
        if (keep[k]) {
            kept.push_back(points[k]);
        }
    }
    return kept;
}

/**
 * @brief One closed ring being simplified: its vertices with the first
 *        repeated at the end, and which of them are kept.
 */
template <class PointType>
struct SimplifiedRing {
    std::vector<PointType> points;
    std::vector<char> keep;

    /**
     * @brief Starts from the ring's vertices, anchored at the first one and at
     *        the vertex farthest from it, with Douglas–Peucker run between.
     */
    template <std::ranges::input_range Range>
    SimplifiedRing(const Range& vertices, const Simplifier<PointType>& simplifier) {
        for (const auto& p : vertices) {
            points.push_back(p);
        }
        const std::size_t n = points.size();
        keep.assign(n + 1, 0);
        if (n == 0) {
            return;
        }
        points.push_back(points.front());
        if (n <= 2) {
            std::fill(keep.begin(), keep.end(), 1);
            return;
        }
        using Key = simplification_key_t<PointType>;
        const Point<Key> origin(points[0]);
        std::size_t anchor = 0;
        Key reach = 0;
        for (std::size_t k = 1; k < n; ++k) {
            const auto offset = Point<Key>(points[k]) - origin;
            Key distance = offset * offset;
            if (reach < distance) {
                reach = std::move(distance);
                anchor = k;
            }
        }
        if (anchor == 0) {
            std::fill(keep.begin(), keep.end(), 1);  // every vertex coincides
            return;
        }
        simplifier.run(points, 0, anchor, keep);
        simplifier.run(points, anchor, n, keep);
    }

    std::size_t size() const {
        return points.empty() ? 0 : points.size() - 1;
    }

    /** @brief Positions of the kept vertices, the closing copy excluded. */
    std::vector<std::size_t> kept() const {
        std::vector<std::size_t> result;
        for (std::size_t k = 0; k < size(); ++k) {
            if (keep[k]) {
                result.push_back(k);
            }
        }
        return result;
    }

    std::vector<PointType> keptPoints() const {
        std::vector<PointType> result;
        for (const std::size_t k : kept()) {
            result.push_back(points[k]);
        }
        return result;
    }

    /** @brief Splits every shortcut once; `false` if there was none. */
    bool refineAll(const Simplifier<PointType>& simplifier) {
        const std::vector<std::size_t> at = kept();
        bool refined = false;
        for (std::size_t t = 0; t < at.size(); ++t) {
            const std::size_t to = t + 1 < at.size() ? at[t + 1] : size();
            if (to - at[t] > 1) {
                simplifier.refine(points, at[t], to, keep);
                refined = true;
            }
        }
        return refined;
    }
};

/** @brief The pairs of positions in @p segments whose segments meet. */
template <class Seg>
std::vector<std::pair<std::size_t, std::size_t>> meetingSegmentPairs(const std::vector<Seg>& segments) {
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    if constexpr (std::is_floating_point_v<typename Seg::PointType::NumberType>) {
        visitXYSweepPairs(segments, [&segments, &pairs](std::size_t i, std::size_t j) {
            if (segments[i].intersects(segments[j])) {
                pairs.emplace_back(i, j);
            }
            return false;
        });
    } else {
        for (const auto& pair : pgl::findIntersections(segments)) {
            pairs.emplace_back(pair[0].label(), pair[1].label());
        }
    }
    return pairs;
}

/** @brief Whether @p other meets the relative interior of @p edge. */
template <class Seg>
bool meetsRelativeInterior(const Seg& edge, const Seg& other) {
    return edge.interiorsIntersect(other) || edge.interiorContains(other.min()) ||
           edge.interiorContains(other.max());
}

/**
 * @brief Splits shortcuts until none touches another edge beyond its own
 *        endpoints and @p nesting finds nothing wrong.
 *
 * @param nesting Called with the rings once the edges are clean; returns the
 *        positions of the rings whose every shortcut is to be split, empty when
 *        the rings sit as they should.
 */
template <class PointType, class Nesting>
void repairRings(std::vector<SimplifiedRing<PointType>>& rings, const Simplifier<PointType>& simplifier,
                 Nesting nesting) {
    using NumberType = typename PointType::NumberType;
    using Edge = Segment<Point<NumberType>, std::size_t>;
    struct Place {
        std::size_t ring, from, to;
    };

    while (true) {
        // A ring left with fewer than three vertices has no area and cannot be
        // simple; it is split before anything is asked of its edges.
        bool refined = false;
        for (auto& ring : rings) {
            if (ring.size() >= 3 && ring.kept().size() < 3) {
                refined = ring.refineAll(simplifier) || refined;
            }
        }
        if (refined) {
            continue;
        }

        std::vector<Place> places;
        std::vector<Edge> edges;
        for (std::size_t r = 0; r < rings.size(); ++r) {
            const std::vector<std::size_t> at = rings[r].kept();
            for (std::size_t t = 0; t < at.size(); ++t) {
                const std::size_t to = t + 1 < at.size() ? at[t + 1] : rings[r].size();
                edges.emplace_back(Point<NumberType>(rings[r].points[at[t]]),
                                   Point<NumberType>(rings[r].points[to]), places.size());
                places.push_back({r, at[t], to});
            }
        }

        std::vector<char> offending(edges.size(), 0);
        const auto shortcut = [&places](std::size_t e) { return places[e].to - places[e].from > 1; };
        for (const auto& [e, f] : meetingSegmentPairs(edges)) {
            if (shortcut(e) && meetsRelativeInterior(edges[e], edges[f])) {
                offending[e] = 1;
            }
            if (shortcut(f) && meetsRelativeInterior(edges[f], edges[e])) {
                offending[f] = 1;
            }
        }
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if (offending[e]) {
                simplifier.refine(rings[places[e].ring].points, places[e].from, places[e].to,
                                  rings[places[e].ring].keep);
                refined = true;
            }
        }
        if (refined) {
            continue;
        }

        for (const std::size_t r : nesting(rings)) {
            refined = rings[r].refineAll(simplifier) || refined;
        }
        if (!refined) {
            return;
        }
    }
}

/** @brief No nesting to check: a single ring. */
struct NoNesting {
    template <class Rings>
    std::vector<std::size_t> operator()(const Rings&) const {
        return {};
    }
};

/**
 * @brief The rings of regions whose nesting went wrong: a hole outside its
 *        outer boundary, two holes overlapping, two regions overlapping.
 *
 * @p starts holds, per region, the position of its outer ring; its holes
 * follow up to the next region's outer ring.
 */
template <class PointType>
struct RegionNesting {
    std::vector<std::size_t> starts;

    std::vector<std::size_t> operator()(const std::vector<SimplifiedRing<PointType>>& rings) const {
        using PolygonType = Polygon<PointType>;
        using Region = PolygonWithHoles<PointType>;
        std::vector<char> broken(rings.size(), 0);
        std::vector<Region> regions;
        for (std::size_t c = 0; c < starts.size(); ++c) {
            const std::size_t begin = starts[c];
            const std::size_t end = c + 1 < starts.size() ? starts[c + 1] : rings.size();
            const PolygonType outer(rings[begin].keptPoints());
            std::vector<PolygonType> holes;
            for (std::size_t r = begin + 1; r < end; ++r) {
                holes.emplace_back(rings[r].keptPoints());
                if (!outer.contains(holes.back())) {
                    broken[begin] = broken[r] = 1;
                }
            }
            anyIntersectingBoxPair(
                holes.size(), [&holes](std::size_t i) { return holes[i].bbox(); },
                [&](std::size_t i, std::size_t j) {
                    if (holes[i].interiorsIntersect(holes[j])) {
                        broken[begin + 1 + i] = broken[begin + 1 + j] = 1;
                    }
                    return false;
                });
            regions.emplace_back(outer, holes);
        }
        anyIntersectingBoxPair(
            regions.size(), [&regions](std::size_t i) { return regions[i].bbox(); },
            [&](std::size_t i, std::size_t j) {
                if (regions[i].interiorsIntersect(regions[j])) {
                    for (const std::size_t c : {i, j}) {
                        const std::size_t end = c + 1 < starts.size() ? starts[c + 1] : rings.size();
                        for (std::size_t r = starts[c]; r < end; ++r) {
                            broken[r] = 1;
                        }
                    }
                }
                return false;
            });
        std::vector<std::size_t> result;
        for (std::size_t r = 0; r < rings.size(); ++r) {
            if (broken[r]) {
                result.push_back(r);
            }
        }
        return result;
    }
};

}  // namespace pgl::detail

namespace pgl {

template <class PointType_, class TLabel>
template <class Tolerance>
Polyline<PointType_, TLabel> Polyline<PointType_, TLabel>::simplified(const Tolerance& squaredTolerance) const {
    const detail::Simplifier<PointType> simplifier(squaredTolerance);
    Polyline result(detail::simplifyChain(vertices(), simplifier));
    result.label_ = label_;
    return result;
}

template <class PointType_, class TLabel>
template <class Tolerance>
void Polyline<PointType_, TLabel>::simplify(const Tolerance& squaredTolerance) {
    *this = simplified(squaredTolerance);
}

template <class PointType, class LabelType, class Storage>
template <class Tolerance>
typename MonotoneChain<PointType, LabelType, Storage>::OwningChain
MonotoneChain<PointType, LabelType, Storage>::simplified(const Tolerance& squaredTolerance) const {
    const detail::Simplifier<PointType> simplifier(squaredTolerance);
    const std::vector<PointType> points(begin(), end());
    OwningChain result(detail::simplifyChain(points, simplifier), trusted);
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = label();
    }
    return result;
}

template <class PointType, class LabelType, class Storage>
template <class Tolerance>
void MonotoneChain<PointType, LabelType, Storage>::simplify(const Tolerance& squaredTolerance)
    requires detail::ownsChainStorage<Storage, PointType>
{
    *this = simplified(squaredTolerance);
}

template <class PointType_, class TLabel>
template <class Tolerance>
Convex<PointType_, TLabel> Convex<PointType_, TLabel>::simplified(const Tolerance& squaredTolerance) const {
    const detail::Simplifier<PointType> simplifier(squaredTolerance);
    const detail::SimplifiedRing<PointType> ring(vertices(), simplifier);
    Convex result(ring.keptPoints(), trusted);
    result.label_ = label_;
    return result;
}

template <class PointType_, class TLabel>
template <class Tolerance>
void Convex<PointType_, TLabel>::simplify(const Tolerance& squaredTolerance) {
    *this = simplified(squaredTolerance);
}

template <class PointType_, class TLabel>
template <class Tolerance>
Polygon<PointType_, TLabel> Polygon<PointType_, TLabel>::simplified(const Tolerance& squaredTolerance) const {
    const detail::Simplifier<PointType> simplifier(squaredTolerance);
    std::vector<detail::SimplifiedRing<PointType>> rings;
    rings.emplace_back(vertices(), simplifier);
    detail::repairRings(rings, simplifier, detail::NoNesting{});
    Polygon result(rings.front().keptPoints());
    result.label_ = label_;
    return result;
}

template <class PointType_, class TLabel>
template <class Tolerance>
void Polygon<PointType_, TLabel>::simplify(const Tolerance& squaredTolerance) {
    *this = simplified(squaredTolerance);
}

template <class PointType_, class TLabel>
template <class Tolerance>
PolygonWithHoles<PointType_, TLabel>
PolygonWithHoles<PointType_, TLabel>::simplified(const Tolerance& squaredTolerance) const {
    const detail::Simplifier<PointType> simplifier(squaredTolerance);
    std::vector<detail::SimplifiedRing<PointType>> rings;
    rings.emplace_back(outer_.vertices(), simplifier);
    for (const auto& hole : holes_) {
        rings.emplace_back(hole.vertices(), simplifier);
    }
    detail::repairRings(rings, simplifier, detail::RegionNesting<PointType>{{0}});
    std::vector<PolygonType> holes;
    for (std::size_t r = 1; r < rings.size(); ++r) {
        holes.emplace_back(rings[r].keptPoints());
    }
    PolygonWithHoles result(PolygonType(rings.front().keptPoints()), holes);
    result.label_ = label_;
    return result;
}

template <class PointType_, class TLabel>
template <class Tolerance>
void PolygonWithHoles<PointType_, TLabel>::simplify(const Tolerance& squaredTolerance) {
    *this = simplified(squaredTolerance);
}

template <class PointType_, class TLabel>
template <class Tolerance>
PolygonSet<PointType_, TLabel> PolygonSet<PointType_, TLabel>::simplified(const Tolerance& squaredTolerance) const {
    using PolygonType = typename ComponentType::PolygonType;
    const detail::Simplifier<PointType> simplifier(squaredTolerance);
    std::vector<detail::SimplifiedRing<PointType>> rings;
    detail::RegionNesting<PointType> nesting;
    for (const auto& component : components_) {
        nesting.starts.push_back(rings.size());
        rings.emplace_back(component.outer().vertices(), simplifier);
        for (const auto& hole : component.holes()) {
            rings.emplace_back(hole.vertices(), simplifier);
        }
    }
    detail::repairRings(rings, simplifier, nesting);
    std::vector<ComponentType> components;
    for (std::size_t c = 0; c < nesting.starts.size(); ++c) {
        const std::size_t begin = nesting.starts[c];
        const std::size_t end = c + 1 < nesting.starts.size() ? nesting.starts[c + 1] : rings.size();
        std::vector<PolygonType> holes;
        for (std::size_t r = begin + 1; r < end; ++r) {
            holes.emplace_back(rings[r].keptPoints());
        }
        components.emplace_back(PolygonType(rings[begin].keptPoints()), holes);
    }
    PolygonSet result(components);
    result.label_ = label_;
    return result;
}

template <class PointType_, class TLabel>
template <class Tolerance>
void PolygonSet<PointType_, TLabel>::simplify(const Tolerance& squaredTolerance) {
    *this = simplified(squaredTolerance);
}

}  // namespace pgl
