#pragma once

#include "algorithm/arrangement.hpp"

/**
 * @file voronoi.hpp
 * @brief Voronoi, power and order-`k` diagrams as arrangements.
 *
 * Two free functions cover the family: @ref pgl::voronoiDiagram over a container
 * of @ref pgl::Point sites and @ref pgl::powerDiagram over a container of
 * @ref pgl::Disk sites. Each returns the @ref pgl::Arrangement the sites
 * subdivide the plane into, with every face labeled by the sites that own it.
 *
 * Both read a site through the same lens: the power distance of a point `x` to a
 * disk of center `c` and radius `r` is `|x - c|^2 - r^2`, and a point site is
 * the disk of radius zero, whose power distance is the ordinary squared
 * distance. The weights are the whole of the difference between the two, and
 * they are one implementation.
 *
 * The **order** `k` says how many sites own a face: the cell of a `k`-element
 * set of sites is the region where exactly those `k` are the nearest ones, ties
 * excluded. `k = 1` is the ordinary diagram, one site per face.
 *
 * Everything is exact. The bisector of two sites is a line whatever the weights
 * are, so the diagram is a straight-line arrangement and needs no arithmetic
 * beyond the rational coordinates an arrangement already uses.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pgl {

namespace detail {

/**
 * @brief A site as the power distance sees it.
 *
 * `power(x) = |x - center|^2 - weight`, so a point site is a zero weight and a
 * disk site is its squared radius. @ref lifted is `|center|^2 - weight`, the
 * constant term left once the `|x|^2` common to every site is dropped: two
 * sites compare by the affine function `-2 center . x + lifted` alone.
 */
template <class Number>
struct VoronoiSite {
    Point<Number> center{};
    Number weight{};
    Number lifted{};
};

/** @brief The site of a point: its position, with no weight. */
template <class Number, PointConcept P>
VoronoiSite<Number> voronoiSiteOf(const P& point) {
    VoronoiSite<Number> site;
    site.center = Point<Number>(point);
    site.weight = Number{};
    site.lifted = site.center.x() * site.center.x() + site.center.y() * site.center.y();
    return site;
}

/** @brief The site of a disk: its center, weighted by its squared radius. */
template <class Number, DiskConcept D>
VoronoiSite<Number> voronoiSiteOf(const D& disk) {
    VoronoiSite<Number> site;
    site.center = Point<Number>(disk.template center<Number>());
    site.weight = disk.template squaredRadius<Number>();
    site.lifted =
        site.center.x() * site.center.x() + site.center.y() * site.center.y() - site.weight;
    return site;
}

/** @brief The power distance from @p site to @p query. */
template <class Number>
Number voronoiPower(const VoronoiSite<Number>& site, const Point<Number>& query) {
    const Number dx = query.x() - site.center.x();
    const Number dy = query.y() - site.center.y();
    return dx * dx + dy * dy - site.weight;
}

/**
 * @brief How fast the power distance to @p site grows leaving @p query along
 *        @p direction, up to the positive factor 2 that every site shares.
 *
 * Only the ordering of these matters, so the factor is dropped.
 */
template <class Number>
Number voronoiPowerSlope(const VoronoiSite<Number>& site, const Point<Number>& query,
                         const Point<Number>& direction) {
    const Number dx = query.x() - site.center.x();
    const Number dy = query.y() - site.center.y();
    return dx * direction.x() + dy * direction.y();
}

/** @brief A bisector line, as a point on it and a direction along it. */
template <class Number>
struct VoronoiBisector {
    Point<Number> origin{};
    Point<Number> direction{};
};

/**
 * @brief The bisector of two sites: where their power distances agree.
 *
 * Equating `-2 a.center . x + a.lifted` with the same expression for `b` leaves
 * `(a.center - b.center) . x = (a.lifted - b.lifted) / 2`, a line whatever the
 * two weights are. Two sites sharing a center have no bisector at all — either
 * they are the same site, or one is closer than the other everywhere — and are
 * reported as nothing.
 */
template <class Number>
std::optional<VoronoiBisector<Number>> voronoiBisector(const VoronoiSite<Number>& a,
                                                       const VoronoiSite<Number>& b) {
    const Number zero{};
    const Number ux = a.center.x() - b.center.x();
    const Number uy = a.center.y() - b.center.y();
    if (ux == zero && uy == zero) {
        return std::nullopt;
    }
    const Number two = static_cast<Number>(2);
    const Number offset = (a.lifted - b.lifted) / two;
    const Number norm = ux * ux + uy * uy;
    VoronoiBisector<Number> bisector;
    bisector.origin = Point<Number>(ux * offset / norm, uy * offset / norm);
    bisector.direction = Point<Number>(-uy, ux);
    return bisector;
}

/**
 * @brief Reports the pieces of the bisector of sites @p i and @p j that are
 *        edges of the order-`k` diagram.
 *
 * A point of the bisector lies on such an edge exactly when `i` and `j` are the
 * `k`-th and `(k+1)`-th nearest sites there, that is when exactly `k - 1` other
 * sites are strictly nearer. Along the bisector, parametrized as
 * `origin + t * direction`, the amount by which another site `m` exceeds `i` is
 * the affine function `alpha * t + beta`, so each `m` is nearer on a halfline
 * (or on all of the line, or on none of it) and the count of nearer sites is a
 * step function of `t`. Its level sets are what this reports.
 *
 * @param sites Every site.
 * @param i,j The pair whose bisector to cut.
 * @param k Order of the diagram.
 * @param events Scratch buffer, reused across pairs.
 * @param emit Called as `emit(bisector, from, to)` once per maximal run, with
 *        `std::nullopt` for an end that runs to infinity.
 */
template <class Number, class Emit>
void voronoiPairEdges(const std::vector<VoronoiSite<Number>>& sites, std::size_t i, std::size_t j,
                      int k, std::vector<std::pair<Number, int>>& events, Emit&& emit) {
    const auto bisector = voronoiBisector(sites[i], sites[j]);
    if (!bisector) {
        return;
    }
    const Number zero{};
    const Number two = static_cast<Number>(2);
    const Point<Number>& origin = bisector->origin;
    const Point<Number>& direction = bisector->direction;

    // alpha * t + beta is how much site m's power distance exceeds site i's at
    // origin + t * direction; m is strictly nearer exactly where it is negative.
    const auto coefficients = [&](std::size_t m) {
        const Number vx = sites[m].center.x() - sites[i].center.x();
        const Number vy = sites[m].center.y() - sites[i].center.y();
        const Number alpha = -two * (vx * direction.x() + vy * direction.y());
        const Number beta =
            -two * (vx * origin.x() + vy * origin.y()) + (sites[m].lifted - sites[i].lifted);
        return std::pair<Number, Number>(alpha, beta);
    };

    if (k == 1) {
        // The one run wanted is where no site is nearer, an intersection of
        // halflines and hence a single interval. Tracking it directly skips the
        // sort, and the interval usually empties after a handful of sites.
        std::optional<Number> from;
        std::optional<Number> to;
        for (std::size_t m = 0; m < sites.size(); ++m) {
            if (m == i || m == j) {
                continue;
            }
            const auto [alpha, beta] = coefficients(m);
            if (alpha == zero) {
                if (beta < zero) {
                    return;
                }
                continue;
            }
            const Number crossing = -beta / alpha;
            if (alpha > zero) {
                // Nearer for t < crossing, so the run starts there.
                if (!from || crossing > *from) {
                    from = crossing;
                }
            } else if (!to || crossing < *to) {
                to = crossing;
            }
            if (from && to && !(*from < *to)) {
                return;
            }
        }
        emit(*bisector, from, to);
        return;
    }

    events.clear();
    int count = 0;  // sites nearer than the pair at t = -infinity
    for (std::size_t m = 0; m < sites.size(); ++m) {
        if (m == i || m == j) {
            continue;
        }
        const auto [alpha, beta] = coefficients(m);
        if (alpha == zero) {
            if (beta < zero) {
                ++count;
            }
            continue;
        }
        const Number crossing = -beta / alpha;
        if (alpha > zero) {
            ++count;
            events.emplace_back(crossing, -1);
        } else {
            events.emplace_back(crossing, 1);
        }
    }
    std::sort(events.begin(), events.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });

    std::optional<Number> from;  // nullopt while the current run starts at -infinity
    bool inRun = count == k - 1;
    for (std::size_t e = 0; e < events.size();) {
        const Number& at = events[e].first;
        while (e < events.size() && events[e].first == at) {
            count += events[e].second;
            ++e;
        }
        if (inRun && count != k - 1) {
            emit(*bisector, from, std::optional<Number>(at));
            inRun = false;
        } else if (!inRun && count == k - 1) {
            from = at;
            inRun = true;
        }
    }
    if (inRun) {
        emit(*bisector, from, std::nullopt);
    }
}

/**
 * @brief The direction a halfedge runs in, as a vector.
 *
 * The two halfedges of a ray report the same @ref pgl::Ray, so the one whose
 * source is the vertex at infinity is the one running against it.
 */
template <class Diagram>
typename Diagram::PointType voronoiHalfedgeDirection(const Diagram& diagram,
                                                     typename Diagram::HalfedgeId halfedge) {
    using ResultPoint = typename Diagram::PointType;
    return std::visit(
        [&](const auto& geometry) -> ResultPoint {
            const ResultPoint step(geometry[1].x() - geometry[0].x(),
                                   geometry[1].y() - geometry[0].y());
            if constexpr (RayConcept<std::remove_cvref_t<decltype(geometry)>>) {
                if (diagram.isFictitious(diagram.source(halfedge))) {
                    return ResultPoint(-step.x(), -step.y());
                }
            }
            return step;
        },
        diagram[halfedge]);
}

/**
 * @brief Labels every face of @p diagram with the @p k sites nearest to it.
 *
 * A face is identified from a boundary halfedge rather than from an interior
 * point, which an unbounded face has no constructive answer for: the midpoint of
 * the halfedge is on the face's boundary, and the face is the side the halfedge's
 * left normal points to. Each site's power distance is affine along that normal,
 * so ordering the sites by (value, rate of growth) at the midpoint orders them
 * as they are ordered just inside the face, with no step length to choose.
 *
 * The two orders differ only if two sites agree in both, which on a line
 * crossing the boundary means they are tied along the whole of it. Such a pair
 * is separated by a bisector perpendicular to the halfedge there; were that
 * bisector a boundary of the order-`k` diagram, the midpoint would be a vertex
 * rather than interior to an edge, so the pair is either inside the `k` nearest
 * or outside it, never split by it.
 */
template <class Number, class Element>
void labelVoronoiFaces(Arrangement<Point<Number>, std::vector<Element>>& diagram,
                       const std::vector<VoronoiSite<Number>>& sites,
                       const std::vector<Element>& elements, int k) {
    using Diagram = Arrangement<Point<Number>, std::vector<Element>>;
    const std::size_t wanted = static_cast<std::size_t>(k);

    std::vector<std::pair<Number, Number>> keys(sites.size());
    std::vector<std::size_t> order(sites.size());
    const auto nearer = [&](std::size_t left, std::size_t right) {
        if (keys[left].first != keys[right].first) {
            return keys[left].first < keys[right].first;
        }
        if (keys[left].second != keys[right].second) {
            return keys[left].second < keys[right].second;
        }
        return left < right;
    };
    const auto labelAt = [&](typename Diagram::FaceId face, const Point<Number>& query,
                             const Point<Number>& inward) {
        for (std::size_t s = 0; s < sites.size(); ++s) {
            keys[s] = {voronoiPower(sites[s], query), voronoiPowerSlope(sites[s], query, inward)};
        }
        std::iota(order.begin(), order.end(), std::size_t{0});
        const auto cut = order.begin() + static_cast<std::ptrdiff_t>(wanted);
        std::partial_sort(order.begin(), cut, order.end(), nearer);
        std::sort(order.begin(), cut);
        std::vector<Element> owners;
        owners.reserve(wanted);
        for (std::size_t s = 0; s < wanted; ++s) {
            owners.push_back(elements[order[s]]);
        }
        diagram.label(face) = std::move(owners);
    };

    if (diagram.halfedgeCount() == 0) {
        // No boundary anywhere: one face, the whole plane, owned by the same
        // sites throughout. Any query point answers for it.
        labelAt(typename Diagram::FaceId(0), sites[0].center,
                Point<Number>(static_cast<Number>(1), Number{}));
        return;
    }

    std::vector<bool> labeled(diagram.faceCount(), false);
    for (std::size_t h = 0; h < diagram.halfedgeCount(); ++h) {
        const typename Diagram::HalfedgeId halfedge(static_cast<std::uint32_t>(h));
        const typename Diagram::FaceId face = diagram.face(halfedge);
        if (labeled[face.index()]) {
            continue;
        }
        labeled[face.index()] = true;
        const Point<Number> query = diagram.template witness<Number>(halfedge);
        const Point<Number> along = voronoiHalfedgeDirection(diagram, halfedge);
        // The face of a halfedge is the one on its left.
        labelAt(face, query, Point<Number>(-along.y(), along.x()));
    }
}

/** @brief The coordinate type the diagram functions compute in. */
template <class ResultNumber, class Element>
using voronoi_number_t =
    std::conditional_t<std::is_void_v<ResultNumber>,
                       division_result_t<typename Element::NumberType>, ResultNumber>;

/** @brief The arrangement @ref pgl::voronoiDiagram and @ref pgl::powerDiagram return. */
template <class ResultNumber, class SiteRange>
using voronoi_diagram_t =
    Arrangement<Point<voronoi_number_t<ResultNumber, std::ranges::range_value_t<SiteRange>>>,
                std::vector<std::ranges::range_value_t<SiteRange>>>;

/**
 * @brief The diagram both public entry points compute; see them for the
 *        contract.
 *
 * @param sites Point or disk sites.
 * @param k Order of the diagram.
 * @param name The caller's name, for the message of a rejected order.
 */
template <class ResultNumber, std::ranges::input_range SiteRange>
    requires(PointConcept<std::ranges::range_value_t<SiteRange>> ||
             DiskConcept<std::ranges::range_value_t<SiteRange>>)
voronoi_diagram_t<ResultNumber, SiteRange> diagramOf(const SiteRange& sites, int k,
                                                     const char* name) {
    using Element = std::ranges::range_value_t<SiteRange>;
    using Number = detail::voronoi_number_t<ResultNumber, Element>;
    using ResultPoint = Point<Number>;
    using Diagram = Arrangement<ResultPoint, std::vector<Element>>;

    std::vector<Element> elements(std::ranges::begin(sites), std::ranges::end(sites));
    if (k < 1 || static_cast<std::size_t>(k) > elements.size()) {
        throw std::invalid_argument(std::string(name) +
                                    ": the order must be between 1 and the number of sites");
    }

    std::vector<detail::VoronoiSite<Number>> lifted;
    lifted.reserve(elements.size());
    for (const Element& element : elements) {
        lifted.push_back(detail::voronoiSiteOf<Number>(element));
    }

    std::vector<Shape<ResultPoint>> curves;

    // Point sites with k = 1 are the Delaunay dual, whose edges come out of the
    // triangulation in O(n log n) instead of one pass per bisector. Sites that
    // are all collinear, or all equal, leave no triangle to dualize and fall
    // through.
    bool fromDelaunay = false;
    if constexpr (PointConcept<Element>) {
        if (k == 1) {
            std::vector<Point<typename Element::NumberType>> plain;
            plain.reserve(elements.size());
            for (const Element& element : elements) {
                plain.emplace_back(element.x(), element.y());
            }
            const Triangulation triangulation(plain);
            if (triangulation.numTriangles() != 0) {
                fromDelaunay = true;
                curves = triangulation.template voronoiEdges<Number>();
            }
        }
    }

    if (!fromDelaunay) {
        const auto pointAt = [](const detail::VoronoiBisector<Number>& bisector, const Number& t) {
            return ResultPoint(bisector.origin.x() + t * bisector.direction.x(),
                               bisector.origin.y() + t * bisector.direction.y());
        };
        const auto emit = [&](const detail::VoronoiBisector<Number>& bisector,
                              const std::optional<Number>& from, const std::optional<Number>& to) {
            const ResultPoint& direction = bisector.direction;
            if (!from && !to) {
                curves.emplace_back(Line<ResultPoint>(
                    bisector.origin, ResultPoint(bisector.origin.x() + direction.x(),
                                                 bisector.origin.y() + direction.y())));
            } else if (!from) {
                const ResultPoint end = pointAt(bisector, *to);
                curves.emplace_back(Ray<ResultPoint>(
                    end, ResultPoint(end.x() - direction.x(), end.y() - direction.y())));
            } else if (!to) {
                const ResultPoint start = pointAt(bisector, *from);
                curves.emplace_back(Ray<ResultPoint>(
                    start, ResultPoint(start.x() + direction.x(), start.y() + direction.y())));
            } else {
                curves.emplace_back(
                    Segment<ResultPoint>(pointAt(bisector, *from), pointAt(bisector, *to)));
            }
        };

        std::vector<std::pair<Number, int>> events;
        for (std::size_t i = 0; i + 1 < lifted.size(); ++i) {
            for (std::size_t j = i + 1; j < lifted.size(); ++j) {
                detail::voronoiPairEdges(lifted, i, j, k, events, emit);
            }
        }
    }

    Diagram diagram(curves);

    if constexpr (PointConcept<Element>) {
        if (k == 1) {
            // A point site is interior to its own cell, so locating it is both
            // cheaper than scanning the sites once per face and exactly how
            // Triangulation::voronoiDiagram attributes its own faces.
            diagram.buildPointLocation();
            for (std::size_t s = 0; s < lifted.size(); ++s) {
                diagram.label(diagram.locateFace(lifted[s].center)) =
                    std::vector<Element>{elements[s]};
            }
            diagram.clearPointLocation();
            return diagram;
        }
    }

    detail::labelVoronoiFaces(diagram, lifted, elements, k);
    return diagram;
}

}  // namespace detail

/**
 * @brief Computes the Voronoi diagram, or the order-`k` diagram, of a set of
 *        points.
 *
 * Every face of the result is labeled with the `k` sites nearest to it, as
 * elements of the input container and ordered by their position in it — a
 * one-element vector when `k` is 1, which is the ordinary Voronoi diagram and
 * reproduces @ref Triangulation::voronoiDiagram, and in fact computes it that
 * way. The arrangement's edge labels are default-constructed and have no
 * meaning.
 *
 * A face of the order-`k` diagram is the region where one set of `k` sites is
 * nearer than every other site; the sites of neighboring faces differ by a
 * single swap. Cells that are empty do not appear, so the number of faces is
 * generally far below the number of `k`-element subsets.
 *
 * Complexity: finding the edges takes `O(n log n)` when `k` is 1, those edges
 * being @ref Triangulation::voronoiEdges, and `O(n^3 log n)` otherwise — one
 * pass over the sites for each of the `O(n^2)` bisectors, the `k = 1` case
 * skipping the logarithmic factor and abandoning most bisectors after a few
 * sites. Assembling them into the @ref Arrangement is quadratic in their number
 * on top of that, an unbounded diagram being exactly the input that sends the
 * construction down its carrier overlay, and when `k` is 1 it is what the
 * running time consists of.
 *
 * @tparam ResultNumber Coordinate type of the arrangement vertices. The default
 *         is exact and overflow-free for integral input.
 * @tparam SiteRange Range of @ref pgl::Point.
 * @param sites Sites of the diagram. Repeated sites are allowed and share a
 *        cell, which then carries only one of them.
 * @param k Order of the diagram: how many sites own each face.
 * @return The unbounded arrangement of the diagram, every face labeled.
 * @throws std::invalid_argument if `k` is below 1 or above the number of sites,
 *         which includes every `k` when there are no sites.
 * @see powerDiagram for the same thing over weighted sites.
 */
template <class ResultNumber = void, std::ranges::input_range SiteRange>
    requires PointConcept<std::ranges::range_value_t<SiteRange>>
[[nodiscard]] detail::voronoi_diagram_t<ResultNumber, SiteRange> voronoiDiagram(
    const SiteRange& sites, int k = 1) {
    return detail::diagramOf<ResultNumber>(sites, k, "pgl::voronoiDiagram");
}

/**
 * @brief Computes the power diagram, or the order-`k` power diagram, of a set
 *        of disks.
 *
 * The power (Laguerre) diagram is @ref voronoiDiagram with the squared distance
 * to a site replaced by the power distance `|x - center|^2 - radius^2`, which
 * is what a radius weighs a disk by. Its bisectors are still lines and its
 * cells still convex polygons, but a disk that its neighbors swallow may own no
 * cell at all, and a disk's center may lie outside its own cell — neither of
 * which a point site can do. Disks of equal radius give the Voronoi diagram of
 * their centers.
 *
 * Labels, order and complexity are as in @ref voronoiDiagram, except that there
 * is no Delaunay route to take: finding the edges costs `O(n^3)` even when `k`
 * is 1.
 *
 * @tparam ResultNumber Coordinate type of the arrangement vertices. The default
 *         is exact and overflow-free for integral input.
 * @tparam SiteRange Range of @ref pgl::Disk.
 * @param sites Sites of the diagram. Disks sharing a center are allowed; the
 *        heavier one then owns everything the lighter one would have.
 * @param k Order of the diagram: how many sites own each face.
 * @return The unbounded arrangement of the diagram, every face labeled.
 * @throws std::invalid_argument if `k` is below 1 or above the number of sites,
 *         which includes every `k` when there are no sites.
 */
template <class ResultNumber = void, std::ranges::input_range SiteRange>
    requires DiskConcept<std::ranges::range_value_t<SiteRange>>
[[nodiscard]] detail::voronoi_diagram_t<ResultNumber, SiteRange> powerDiagram(
    const SiteRange& sites, int k = 1) {
    return detail::diagramOf<ResultNumber>(sites, k, "pgl::powerDiagram");
}

}  // namespace pgl
