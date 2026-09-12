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
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
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

/**
 * @brief The power distance from @p site to @p query, less the `|query|^2` that
 *        every site's shares.
 *
 * Only the order of these ever matters, and the term dropped is the same for
 * every site, so the order is the power distances' own. What is left is affine
 * in @p query — `site.lifted - 2 query . site.center` — which over a wide
 * rational query is two multiplications by a narrow site coordinate rather than
 * two squarings of a difference as wide as the query.
 */
template <class Number>
Number voronoiRelativePower(const VoronoiSite<Number>& site, const Point<Number>& query) {
    const Number two = static_cast<Number>(2);
    return site.lifted - two * (query.x() * site.center.x() + query.y() * site.center.y());
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

/**
 * @brief A bisector line, as the points `(base + t * direction) / denominator`.
 *
 * The denominator is kept out of the point rather than divided into it because
 * every site of this library's own input has whole coordinates, and so does
 * every one of `base`, `direction` and `denominator` then: the conditions that
 * cut the line, and the comparisons that intersect what they leave, are whole
 * numbers throughout, and no common factor has to be found and divided out
 * until an endpoint is finally reported. Over rationals of arbitrary width that
 * is most of the arithmetic.
 */
template <class Number>
struct VoronoiBisector {
    Point<Number> base{};
    Point<Number> direction{};
    Number denominator{};
};

/**
 * @brief The bisector of two sites: where their power distances agree.
 *
 * Equating `-2 a.center . x + a.lifted` with the same expression for `b` leaves
 * `(a.center - b.center) . x = (a.lifted - b.lifted) / 2`, a line whatever the
 * two weights are. Two sites sharing a center have no bisector at all — either
 * they are the same site, or one is closer than the other everywhere — and are
 * reported as nothing.
 *
 * The point reported on it is the one midway between the centers, slid along
 * `u = a.center - b.center` by however much the weights differ: writing the
 * point as `midpoint + s u` and solving leaves `s = (b.weight - a.weight) /
 * (2 |u|^2)`, which is zero for two point sites and leaves the midpoint over a
 * denominator of two. Reading the line off its own equation instead would put
 * `|a.center|^2` over `|u|^2` in a base point the midpoint states in the
 * coordinates themselves, and every distance measured from it afterwards is
 * that much wider a number.
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
    VoronoiBisector<Number> bisector;
    bisector.direction = Point<Number>(-uy, ux);
    if (a.weight == b.weight) {
        bisector.base = Point<Number>(a.center.x() + b.center.x(), a.center.y() + b.center.y());
        bisector.denominator = two;
        return bisector;
    }
    const Number norm = ux * ux + uy * uy;
    const Number slid = b.weight - a.weight;
    bisector.base = Point<Number>(norm * (a.center.x() + b.center.x()) + slid * ux,
                                  norm * (a.center.y() + b.center.y()) + slid * uy);
    bisector.denominator = two * norm;
    return bisector;
}

/**
 * @brief Reports the pieces of the bisector of sites @p i and @p j that are
 *        edges of the order-`k` diagram.
 *
 * A point of the bisector lies on such an edge exactly when `i` and `j` are the
 * `k`-th and `(k+1)`-th nearest sites there, that is when exactly `k - 1` other
 * sites are strictly nearer. Along the bisector, parametrized by `t` as
 * @ref VoronoiBisector says, the amount by which another site `m` exceeds `i`
 * is the affine function `alpha * t + beta`, so each `m` is nearer on a halfline
 * (or on all of the line, or on none of it) and the count of nearer sites is a
 * step function of `t`. Its level sets are what this reports.
 *
 * A site `m` that is tied with the pair along the whole bisector — which needs
 * weights, two point sites being tied along a line only if they are the same
 * point — puts every pair drawn from `{i, j, m, ...}` on that one line, seeing
 * one nearer-count and so reporting one set of runs. The two smallest of the
 * tied group report them and the rest report nothing, so the line is covered
 * once rather than several times over.
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
    const Point<Number>& base = bisector->base;
    const Point<Number>& direction = bisector->direction;

    // alpha * t + beta is how much site m's power distance exceeds site i's at
    // parameter t, times the bisector's positive denominator; m is strictly
    // nearer exactly where it is negative, which the factor does not move.
    const auto coefficients = [&](std::size_t m) {
        const Number vx = sites[m].center.x() - sites[i].center.x();
        const Number vy = sites[m].center.y() - sites[i].center.y();
        const Number alpha = -two * (vx * direction.x() + vy * direction.y());
        const Number beta = -two * (vx * base.x() + vy * base.y()) +
                            (sites[m].lifted - sites[i].lifted) * bisector->denominator;
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
                if (beta == zero && m < j) {
                    return;  // a smaller pair of the tied group reports this line
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
            } else if (beta == zero && m < j) {
                return;  // a smaller pair of the tied group reports this line
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
        if (inRun) {
            // A run is cut at every `at` it meets, not only at the one it ends
            // at. A count that leaves `k - 1` and is back by the time the next
            // interval starts is one site handing over to another, both of them
            // tied with the pair at that single `t` — four sites on a circle —
            // and the bisector of the two that swapped is another edge of the
            // diagram crossing this one there. Ending the run at the crossing
            // is what keeps the diagram's edges meeting only at endpoints.
            emit(*bisector, from, std::optional<Number>(at));
            from = at;
            inRun = count == k - 1;
        } else if (count == k - 1) {
            from = at;
            inRun = true;
        }
    }
    if (inRun) {
        emit(*bisector, from, std::nullopt);
    }
}

/**
 * @brief Appends the run of @p bisector between @p from and @p to to @p curves.
 *
 * An end left as `std::nullopt` runs to infinity, so a run bounded at neither
 * end is the whole bisector line, one bounded at one end a ray, and one bounded
 * at both a segment.
 */
template <class Number>
void voronoiAppendRun(std::vector<Shape<Point<Number>>>& curves,
                      const VoronoiBisector<Number>& bisector, const std::optional<Number>& from,
                      const std::optional<Number>& to) {
    using ResultPoint = Point<Number>;
    const ResultPoint& base = bisector.base;
    const ResultPoint& direction = bisector.direction;
    const Number& denominator = bisector.denominator;
    const auto pointAt = [&](const Number& t) {
        return ResultPoint((base.x() + t * direction.x()) / denominator,
                           (base.y() + t * direction.y()) / denominator);
    };
    if (!from && !to) {
        const ResultPoint origin(base.x() / denominator, base.y() / denominator);
        curves.emplace_back(Line<ResultPoint>(
            origin, ResultPoint(origin.x() + direction.x(), origin.y() + direction.y())));
    } else if (!from) {
        const ResultPoint end = pointAt(*to);
        curves.emplace_back(
            Ray<ResultPoint>(end, ResultPoint(end.x() - direction.x(), end.y() - direction.y())));
    } else if (!to) {
        const ResultPoint start = pointAt(*from);
        curves.emplace_back(Ray<ResultPoint>(
            start, ResultPoint(start.x() + direction.x(), start.y() + direction.y())));
    } else {
        curves.emplace_back(Segment<ResultPoint>(pointAt(*from), pointAt(*to)));
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
template <class Number, class Label, class Element>
void labelVoronoiFaces(Arrangement<Point<Number>, Label>& diagram,
                       const std::vector<VoronoiSite<Number>>& sites,
                       const std::vector<Element>& elements, int k) {
    using Diagram = Arrangement<Point<Number>, Label>;
    // A diagram labeled by the bare element is the ordinary one, whose faces
    // have a single owner each; every other order labels by the vector.
    constexpr bool singleOwner = std::same_as<Label, Element>;
    static_assert(singleOwner || std::same_as<Label, std::vector<Element>>,
                  "a Voronoi face is labeled by its owner or by the vector of them");
    const std::size_t wanted = static_cast<std::size_t>(k);
    assert((!singleOwner || wanted == 1) && "a single-owner label needs order 1");

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
            keys[s] = {voronoiRelativePower(sites[s], query),
                   voronoiPowerSlope(sites[s], query, inward)};
        }
        std::iota(order.begin(), order.end(), std::size_t{0});
        const auto cut = order.begin() + static_cast<std::ptrdiff_t>(wanted);
        std::partial_sort(order.begin(), cut, order.end(), nearer);
        std::sort(order.begin(), cut);
        if constexpr (singleOwner) {
            diagram.label(face) = elements[order[0]];
        } else {
            Label owners;
            owners.reserve(wanted);
            for (std::size_t s = 0; s < wanted; ++s) {
                owners.push_back(elements[order[s]]);
            }
            diagram.label(face) = std::move(owners);
        }
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

/**
 * @brief The site of @p among nearest to @p query, ties settled toward @p inward.
 *
 * @p query is a point of the boundary of the face being labeled, so a tie there
 * may well be a tie only there: the site whose power distance grows slower
 * leaving @p query along @p inward is the nearer one just inside the face. Two
 * sites agreeing in both are ordered by index, which is what
 * @ref labelVoronoiFaces argues is free to choose.
 */
template <class Number>
std::uint32_t voronoiNearestAmong(const std::vector<VoronoiSite<Number>>& sites,
                                  const std::vector<std::uint32_t>& among,
                                  const Point<Number>& query, const Point<Number>& inward) {
    std::uint32_t best = among.front();
    Number bestPower = voronoiRelativePower(sites[best], query);
    std::optional<Number> bestSlope;
    for (std::size_t c = 1; c < among.size(); ++c) {
        const std::uint32_t site = among[c];
        const Number power = voronoiRelativePower(sites[site], query);
        if (bestPower < power) {
            continue;
        }
        if (power < bestPower) {
            best = site;
            bestPower = power;
            bestSlope.reset();
            continue;
        }
        if (!bestSlope) {
            bestSlope = voronoiPowerSlope(sites[best], query, inward);
        }
        const Number slope = voronoiPowerSlope(sites[site], query, inward);
        if (slope < *bestSlope) {
            best = site;
            bestSlope = slope;
        }
    }
    return best;
}

/**
 * @brief Hash of a cell's sorted owner indices, so a set of owners can name a cell.
 */
struct VoronoiOwnersHash {
    /** @brief Mixes the indices in order; two cells never share a sorted name. */
    std::size_t operator()(const std::vector<std::uint32_t>& owners) const noexcept {
        std::size_t mixed = owners.size();
        for (const std::uint32_t site : owners) {
            mixed ^= std::size_t{site} + 0x9e3779b97f4a7c15ULL + (mixed << 6) + (mixed >> 2);
        }
        return mixed;
    }
};

/**
 * @brief Reports the edges the next order's diagram has inside one cell of this
 *        one.
 *
 * The cell of the `k - 1` element set @p owners is subdivided by the ordinary
 * Voronoi diagram of the sites that are not in it, so a piece of the bisector of
 * two of those is an edge of the order-`k` diagram exactly where it is inside
 * the cell and no third of them is nearer. Both halves of that come to a
 * handful of affine conditions along the bisector, in the sites' own
 * coordinates:
 *
 *   - the pair is no farther than any site in @p neighbors, which
 *     @ref VoronoiCells::neighbors is every site that reaches into the cell at
 *     all;
 *   - every site of @p owners is no farther than the pair, which with the
 *     condition above puts every owner ahead of every neighbor, and that is the
 *     cell — its sides are bisectors of an owner against a neighbor, no other
 *     site of the plane having a side of its own to contribute.
 *
 * An intersection of halflines is one interval, so there is no sorting and no
 * pass over the sites: this is the whole of what Lee's refinement saves over
 * cutting every bisector against everything.
 *
 * Inside that interval the conditions all hold strictly, since an affine
 * function that vanishes inside an interval it is non-positive on vanishes
 * throughout it, which would put a third site's bisector on top of this one and
 * so make it one of the two. An edge reported here therefore meets another only
 * at an endpoint, and the cells being convex with disjoint interiors keeps the
 * edges of two different ones apart the same way.
 *
 * @param sites Every site.
 * @param owners The `k - 1` sites of the cell.
 * @param neighbors The sites the cell can gain; see @ref VoronoiCells.
 * @param emit Called as `emit(bisector, from, to, p, q)` once per edge, with
 *        `std::nullopt` for an end that runs to infinity.
 */
template <class Number, class Emit>
void voronoiCellEdges(const std::vector<VoronoiSite<Number>>& sites,
                      const std::vector<std::uint32_t>& owners,
                      const std::vector<std::uint32_t>& neighbors, Emit&& emit) {
    using ResultPoint = Point<Number>;
    const Number zero{};
    const Number two = static_cast<Number>(2);

    for (std::size_t a = 0; a + 1 < neighbors.size(); ++a) {
        for (std::size_t b = a + 1; b < neighbors.size(); ++b) {
            const std::uint32_t p = neighbors[a];
            const std::uint32_t q = neighbors[b];
            const auto bisector = voronoiBisector(sites[p], sites[q]);
            if (!bisector) {
                continue;
            }
            const ResultPoint& base = bisector->base;
            const ResultPoint& direction = bisector->direction;

            // The interval of parameters still allowed, narrowed one condition
            // at a time and held as a fraction per end so that nothing has to
            // be reduced until the run is reported. An end left as nullopt runs
            // to infinity; the denominators are kept positive, so two ends
            // compare by crossing the fractions.
            std::optional<std::pair<Number, Number>> from;
            std::optional<std::pair<Number, Number>> to;
            bool empty = false;
            const auto atMost = [&](const Number& alpha, const Number& beta) {
                if (alpha == zero) {
                    empty = empty || zero < beta;
                    return;
                }
                if (zero < alpha) {
                    if (!to || -beta * to->second < to->first * alpha) {
                        to = {-beta, alpha};
                    }
                } else if (!from || beta * from->second > from->first * -alpha) {
                    from = {beta, -alpha};
                }
                empty = empty || (from && to && !(from->first * to->second <
                                                  to->first * from->second));
            };

            // How much site m's power distance exceeds the pair's at parameter
            // t, times the positive denominator, as the affine function
            // `alpha * t + beta` the conditions are written in. Both are linear
            // in m's center and lifted value, so the pair's own share of them
            // is taken out of the loop over the sites and added back.
            const Number alongBase = two * (sites[p].center.x() * base.x() +
                                            sites[p].center.y() * base.y()) -
                                     sites[p].lifted * bisector->denominator;
            const Number alongDirection = two * (sites[p].center.x() * direction.x() +
                                                 sites[p].center.y() * direction.y());
            const auto exceeds = [&](std::uint32_t m) {
                return std::pair<Number, Number>(
                    alongDirection - two * (sites[m].center.x() * direction.x() +
                                            sites[m].center.y() * direction.y()),
                    alongBase - two * (sites[m].center.x() * base.x() +
                                       sites[m].center.y() * base.y()) +
                        sites[m].lifted * bisector->denominator);
            };

            for (std::size_t c = 0; c < neighbors.size() && !empty; ++c) {
                if (c != a && c != b) {
                    const auto [alpha, beta] = exceeds(neighbors[c]);
                    atMost(-alpha, -beta);
                }
            }
            for (std::size_t o = 0; o < owners.size() && !empty; ++o) {
                const auto [alpha, beta] = exceeds(owners[o]);
                atMost(alpha, beta);
            }

            if (!empty) {
                const auto ratio = [](const std::optional<std::pair<Number, Number>>& end) {
                    return end ? std::optional<Number>(end->first / end->second) : std::nullopt;
                };
                emit(*bisector, ratio(from), ratio(to), p, q);
            }
        }
    }
}

/**
 * @brief The cells of one order, and the sites each of them can gain at the next.
 *
 * A cell is named by the sorted indices of the sites that own it, and that name
 * is what the refinement hands back and forth: an order-`k` cell is an
 * order-`k - 1` cell's owners plus one more site, so the whole family is
 * enumerated without ever laying the diagram out in the plane. Only the last
 * order is built as an @ref pgl::Arrangement, and @ref labelVoronoiCells matches
 * its faces back to these names.
 *
 * @ref neighbors is the set @ref voronoiCellEdges needs: the sites a cell can
 * gain, which are the ones its edge-adjacent cells own. It is accumulated from
 * the edges themselves, an edge reported between the owners `A + p` and `A + q`
 * being exactly the statement that those two cells are adjacent.
 */
struct VoronoiCells {
    /** @brief Sorted site indices of each cell. */
    std::vector<std::vector<std::uint32_t>> owners;
    /** @brief Sorted site indices a cell can gain, one set per cell. */
    std::vector<std::vector<std::uint32_t>> neighbors;
    /** @brief Which cell a set of owners names. */
    std::unordered_map<std::vector<std::uint32_t>, std::uint32_t, VoronoiOwnersHash> numbering;

    /** @brief The number of cells. */
    [[nodiscard]] std::size_t size() const {
        return owners.size();
    }

    /** @brief The cell @p set names, created if this is the first sight of it. */
    std::uint32_t cellOf(const std::vector<std::uint32_t>& set) {
        const auto seen = numbering.find(set);
        if (seen != numbering.end()) {
            return seen->second;
        }
        const auto fresh = static_cast<std::uint32_t>(owners.size());
        numbering.emplace(set, fresh);
        owners.push_back(set);
        neighbors.emplace_back();
        return fresh;
    }
};

/**
 * @brief The order-1 cells, read off a Delaunay triangulation.
 *
 * One cell per site, and a cell's neighbors are the site's Delaunay neighbors.
 * That is a superset of the sites whose cells actually share an edge with this
 * one — four sites on a circle leave the triangulation a choice of diagonal,
 * and the one it takes dualizes to a Voronoi edge of no length — which is all
 * @ref voronoiCellEdges asks of it.
 *
 * The cells are laid out directly rather than through @ref VoronoiCells::cellOf,
 * one per site and in the sites' own order, so @ref VoronoiCells::numbering is
 * filled alongside them: @ref labelVoronoiCells reads a cell back by its name,
 * and at order 1 these are the cells it reads.
 *
 * @param triangulation A Delaunay triangulation of @p plain.
 * @param plain The sites, in the order their indices count them.
 * @return The cells, or nothing if a site is not a vertex of the triangulation.
 */
template <class Mesh, class PlainPoint>
std::optional<VoronoiCells> voronoiDelaunayCells(const Mesh& triangulation,
                                                 const std::vector<PlainPoint>& plain) {
    constexpr std::uint32_t none = ~std::uint32_t{};
    std::unordered_map<PlainPoint, std::uint32_t> indexOf;
    indexOf.reserve(plain.size());
    for (std::size_t s = 0; s < plain.size(); ++s) {
        indexOf.emplace(plain[s], static_cast<std::uint32_t>(s));
    }

    std::vector<std::uint32_t> siteAt(triangulation.vertexIndexBound(), none);
    for (const auto vertex : triangulation.vertexIds()) {
        const auto named = indexOf.find(triangulation[vertex]);
        if (named == indexOf.end()) {
            return std::nullopt;
        }
        siteAt[vertex.index()] = named->second;
    }

    VoronoiCells cells;
    cells.owners.resize(plain.size());
    cells.neighbors.resize(plain.size());
    cells.numbering.reserve(plain.size());
    for (std::size_t s = 0; s < plain.size(); ++s) {
        cells.owners[s] = {static_cast<std::uint32_t>(s)};
        cells.numbering.emplace(cells.owners[s], static_cast<std::uint32_t>(s));
    }
    for (const auto triangle : triangulation.triangleIds()) {
        const auto corners = triangulation.vertices(triangle);
        for (std::size_t i = 0; i < 3; ++i) {
            const std::uint32_t a = siteAt[corners[i].index()];
            const std::uint32_t b = siteAt[corners[(i + 1) % 3].index()];
            if (a == none || b == none) {
                return std::nullopt;
            }
            cells.neighbors[a].push_back(b);
            cells.neighbors[b].push_back(a);
        }
    }
    for (std::vector<std::uint32_t>& list : cells.neighbors) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
    return cells;
}

/**
 * @brief Lee's refinement: the cells of the next order, and their edges.
 *
 * Every edge of the order-`k - 1` diagram disappears at order `k`, and every
 * edge that appears lies strictly inside one of its cells. The first because an
 * edge between the cells of `A + p` and `A + q` has `A + p + q` as the `k`
 * nearest sites on both sides of it, and so has nothing left to separate. The
 * second by counting at a point of an edge's relative interior: an edge of the
 * order-`j` diagram is where two sites tie with exactly `j - 1` sites strictly
 * nearer, and one point cannot do that for one pair and the same with `j - 2`
 * for another — the second pair's members would both be strictly nearer than
 * the first pair's, which makes `j` of them.
 *
 * So the cells are refined one at a time and independently, each by
 * @ref voronoiCellEdges, and an edge reported inside the cell of `A` for the
 * pair `p, q` both names the two cells `A + p` and `A + q` and makes them
 * neighbors. Every edge of the new diagram is reported exactly once this way,
 * so every cell with a boundary is named and every cell's neighbors are
 * complete.
 *
 * @param sites Every site.
 * @param cells The order-`k - 1` cells.
 * @param emit Called as `emit(bisector, from, to)` once per edge. The
 *        intermediate orders pass a callback that does nothing: only the last
 *        order's edges are ever laid out.
 * @return The order-`k` cells.
 */
template <class Number, class Emit>
VoronoiCells voronoiRefineCells(const std::vector<VoronoiSite<Number>>& sites,
                                const VoronoiCells& cells, Emit&& emit) {
    VoronoiCells refined;
    std::vector<std::uint32_t> grown;
    const auto cellWith = [&](const std::vector<std::uint32_t>& owners, std::uint32_t site) {
        grown.assign(owners.begin(), owners.end());
        grown.insert(std::lower_bound(grown.begin(), grown.end(), site), site);
        return refined.cellOf(grown);
    };

    for (std::size_t c = 0; c < cells.size(); ++c) {
        const std::vector<std::uint32_t>& owners = cells.owners[c];
        voronoiCellEdges(sites, owners, cells.neighbors[c],
                         [&](const VoronoiBisector<Number>& bisector,
                             const std::optional<Number>& from, const std::optional<Number>& to,
                             std::uint32_t p, std::uint32_t q) {
                             // Both cells before either push: naming the second
                             // one can reallocate the first one's neighbors.
                             const std::uint32_t kept = cellWith(owners, p);
                             const std::uint32_t other = cellWith(owners, q);
                             refined.neighbors[kept].push_back(q);
                             refined.neighbors[other].push_back(p);
                             emit(bisector, from, to);
                         });
    }
    for (std::vector<std::uint32_t>& list : refined.neighbors) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
    return refined;
}

/**
 * @brief The @p wanted nearest sites at @p query, in index order.
 *
 * A pass over every site, for the one face of each connected piece of a diagram
 * that has no labeled neighbor to take its answer from. @p inward settles a tie
 * the way @ref labelVoronoiFaces settles it.
 */
template <class Number>
std::vector<std::uint32_t> voronoiNearestSites(const std::vector<VoronoiSite<Number>>& sites,
                                               const Point<Number>& query,
                                               const Point<Number>& inward, std::size_t wanted) {
    std::vector<std::pair<Number, Number>> keys(sites.size());
    for (std::size_t s = 0; s < sites.size(); ++s) {
        keys[s] = {voronoiRelativePower(sites[s], query),
                   voronoiPowerSlope(sites[s], query, inward)};
    }
    std::vector<std::uint32_t> order(sites.size());
    std::iota(order.begin(), order.end(), std::uint32_t{0});
    const auto cut = order.begin() + static_cast<std::ptrdiff_t>(wanted);
    std::partial_sort(order.begin(), cut, order.end(),
                      [&](std::uint32_t left, std::uint32_t right) {
                          if (keys[left].first != keys[right].first) {
                              return keys[left].first < keys[right].first;
                          }
                          if (keys[left].second != keys[right].second) {
                              return keys[left].second < keys[right].second;
                          }
                          return left < right;
                      });
    order.resize(wanted);
    std::sort(order.begin(), order.end());
    return order;
}

/**
 * @brief The site of @p among farthest from @p query.
 *
 * Called at a point of an edge's relative interior, where the owners of the face
 * on either side are one site tied with the site across the edge and `k - 1`
 * that are strictly nearer than both. So the farthest owner is the one the face
 * across gives up, and no tie can reach it.
 */
template <class Number>
std::uint32_t voronoiFarthestAmong(const std::vector<VoronoiSite<Number>>& sites,
                                   const std::vector<std::uint32_t>& among,
                                   const Point<Number>& query) {
    std::uint32_t worst = among.front();
    Number reach = voronoiRelativePower(sites[worst], query);
    for (std::size_t c = 1; c < among.size(); ++c) {
        Number power = voronoiRelativePower(sites[among[c]], query);
        if (reach < power) {
            worst = among[c];
            reach = std::move(power);
        }
    }
    return worst;
}

/**
 * @brief Labels every face of @p diagram with the cell of @p cells it is.
 *
 * The faces are walked rather than looked up. Crossing an edge swaps one site
 * for another, and both of them are a handful of power distances away at a point
 * of that edge: the site coming in is the nearest of what the face can gain, and
 * the one going out is the farthest the face owns. So a face named names its
 * neighbors, and one face of each connected piece — in practice one face, the
 * cells of a diagram tiling the plane — is named by a pass over the sites.
 *
 * @param diagram The order-`k` diagram, its faces unlabeled.
 * @param sites Every site.
 * @param elements The sites as the caller handed them over, for the labels.
 * @param cells The order-`k` cells.
 * @param k Order of the diagram.
 * @return Whether every face was named; `false` asks the caller for the general
 *         construction instead.
 */
template <class Number, class Element>
bool labelVoronoiCells(Arrangement<Point<Number>, std::vector<Element>>& diagram,
                       const std::vector<VoronoiSite<Number>>& sites,
                       const std::vector<Element>& elements, const VoronoiCells& cells,
                       std::size_t k) {
    using ResultPoint = Point<Number>;
    using Diagram = Arrangement<ResultPoint, std::vector<Element>>;
    using HalfedgeId = typename Diagram::HalfedgeId;
    constexpr std::uint32_t none = ~std::uint32_t{};

    if (diagram.halfedgeCount() == 0) {
        // One face, the whole plane, which happens only once every site owns
        // it: below that, a site left out is nearest at its own position and
        // the nearest set is not the same everywhere.
        if (k != sites.size()) {
            return false;
        }
        diagram.label(typename Diagram::FaceId(0)) = elements;
        return true;
    }

    std::vector<std::vector<HalfedgeId>> boundary(diagram.faceCount());
    for (std::size_t h = 0; h < diagram.halfedgeCount(); ++h) {
        const HalfedgeId halfedge(static_cast<std::uint32_t>(h));
        boundary[diagram.face(halfedge).index()].push_back(halfedge);
    }

    std::vector<std::uint32_t> cellAt(diagram.faceCount(), none);
    std::vector<std::uint32_t> swapped;
    std::vector<std::size_t> pending;
    const auto name = [&](std::size_t face, std::uint32_t cell) {
        cellAt[face] = cell;
        std::vector<Element> owners;
        owners.reserve(cells.owners[cell].size());
        for (const std::uint32_t site : cells.owners[cell]) {
            owners.push_back(elements[site]);
        }
        diagram.label(typename Diagram::FaceId(static_cast<std::uint32_t>(face))) =
            std::move(owners);
        pending.push_back(face);
    };

    for (std::size_t seed = 0; seed < diagram.faceCount(); ++seed) {
        if (cellAt[seed] != none || boundary[seed].empty()) {
            continue;
        }
        {
            const HalfedgeId halfedge = boundary[seed].front();
            const ResultPoint query = diagram.template witness<Number>(halfedge);
            const ResultPoint along = voronoiHalfedgeDirection(diagram, halfedge);
            // The face of a halfedge is the one on its left.
            const auto found = cells.numbering.find(voronoiNearestSites(
                sites, query, ResultPoint(-along.y(), along.x()), k));
            if (found == cells.numbering.end()) {
                return false;
            }
            name(seed, found->second);
        }

        while (!pending.empty()) {
            const std::size_t here = pending.back();
            pending.pop_back();
            const std::uint32_t cell = cellAt[here];
            const std::vector<std::uint32_t>& owners = cells.owners[cell];
            const std::vector<std::uint32_t>& neighbors = cells.neighbors[cell];
            for (const HalfedgeId halfedge : boundary[here]) {
                const std::size_t across = diagram.face(diagram.twin(halfedge)).index();
                if (cellAt[across] != none) {
                    continue;
                }
                if (neighbors.empty()) {
                    return false;
                }
                const ResultPoint query = diagram.template witness<Number>(halfedge);
                const ResultPoint along = voronoiHalfedgeDirection(diagram, halfedge);
                // The face across is the one on the halfedge's right.
                const ResultPoint inward(along.y(), -along.x());
                const std::uint32_t gained =
                    voronoiNearestAmong(sites, neighbors, query, inward);
                const std::uint32_t given = voronoiFarthestAmong(sites, owners, query);
                swapped.clear();
                for (const std::uint32_t site : owners) {
                    if (site != given) {
                        swapped.push_back(site);
                    }
                }
                swapped.insert(std::lower_bound(swapped.begin(), swapped.end(), gained), gained);
                const auto found = cells.numbering.find(swapped);
                if (found == cells.numbering.end()) {
                    return false;
                }
                name(across, found->second);
            }
        }
    }

    for (const std::uint32_t cell : cellAt) {
        if (cell == none) {
            return false;
        }
    }
    return true;
}

/**
 * @brief The order-@p k diagram of point sites, by Lee's refinement.
 *
 * The order-1 cells come from a Delaunay triangulation and @ref voronoiRefineCells
 * raises them one order at a time. Only the last order's edges are laid out and
 * overlaid into an @ref pgl::Arrangement; the orders below it exist as names and
 * adjacencies alone.
 *
 * @param diagram Where to put the diagram; untouched unless the return is true.
 * @param sites Every site.
 * @param elements The sites as the caller handed them over, for the labels.
 * @param k Order of the diagram.
 * @return Whether the refinement stands. It declines sites that are not in
 *         general enough position for it: sites with no triangle to dualize,
 *         which are all equal or all collinear, and repeated sites, which share
 *         a cell no bisector ever splits and so are invisible to a refinement.
 */
template <class Number, class Element>
bool voronoiByRefinement(Arrangement<Point<Number>, std::vector<Element>>& diagram,
                         const std::vector<VoronoiSite<Number>>& sites,
                         const std::vector<Element>& elements, int k) {
    using ResultPoint = Point<Number>;
    using Diagram = Arrangement<ResultPoint, std::vector<Element>>;
    using PlainPoint = Point<typename Element::NumberType>;

    if (k > 1) {
        std::vector<ResultPoint> centers;
        centers.reserve(sites.size());
        for (const VoronoiSite<Number>& site : sites) {
            centers.push_back(site.center);
        }
        std::sort(centers.begin(), centers.end());
        if (std::adjacent_find(centers.begin(), centers.end()) != centers.end()) {
            return false;
        }
    }

    std::vector<PlainPoint> plain;
    plain.reserve(elements.size());
    for (const Element& element : elements) {
        plain.emplace_back(element.x(), element.y());
    }
    const Triangulation triangulation(plain);
    if (triangulation.numTriangles() == 0) {
        return false;
    }

    std::optional<VoronoiCells> cells = voronoiDelaunayCells(triangulation, plain);
    if (!cells) {
        return false;
    }

    if (k == 1) {
        // The faces are named by the same walk every higher order uses. Locating
        // each site instead would be a handful of predicates apiece, but it has
        // to index the arrangement to do it, and building that index costs more
        // than everything else here together — for nothing, since crossing an
        // edge of the order-1 diagram swaps one Delaunay neighbor for another
        // and the cells already carry that adjacency.
        Diagram level(triangulation.template voronoiEdges<Number>(), true);
        if (!labelVoronoiCells(level, sites, elements, *cells, 1)) {
            return false;
        }
        diagram = std::move(level);
        return true;
    }

    std::vector<Shape<ResultPoint>> curves;
    for (int order = 2; order <= k; ++order) {
        if (order < k) {
            const auto keepNothing = [](const auto&, const auto&, const auto&) {};
            *cells = voronoiRefineCells(sites, *cells, keepNothing);
        } else {
            *cells = voronoiRefineCells(
                sites, *cells,
                [&](const VoronoiBisector<Number>& bisector, const std::optional<Number>& from,
                    const std::optional<Number>& to) {
                    voronoiAppendRun(curves, bisector, from, to);
                });
        }
        // Nothing left to refine below the last order means the cells stopped
        // covering the plane, which they never do; hand the input back rather
        // than build a diagram out of what is left.
        if (cells->size() == 0 && order < k) {
            return false;
        }
    }

    // Every edge of the diagram comes out of exactly one cell, reported open at
    // both ends: there is nothing here for the overlay to cut.
    Diagram next(curves, true);
    if (!labelVoronoiCells(next, sites, elements, *cells, static_cast<std::size_t>(k))) {
        return false;
    }
    diagram = std::move(next);
    return true;
}

/** @brief The coordinate type the diagram functions compute in. */
template <class ResultNumber, class Element>
using voronoi_number_t =
    std::conditional_t<std::is_void_v<ResultNumber>,
                       division_result_t<typename Element::NumberType>, ResultNumber>;

/** @brief The arrangement the order-`k` @ref pgl::voronoiDiagram and @ref pgl::powerDiagram return. */
template <class ResultNumber, class SiteRange>
using voronoi_diagram_t =
    Arrangement<Point<voronoi_number_t<ResultNumber, std::ranges::range_value_t<SiteRange>>>,
                std::vector<std::ranges::range_value_t<SiteRange>>>;

/** @brief The arrangement the ordinary @ref pgl::voronoiDiagram returns. */
template <class ResultNumber, class SiteRange>
using voronoi_dual_t =
    Arrangement<Point<voronoi_number_t<ResultNumber, std::ranges::range_value_t<SiteRange>>>,
                std::ranges::range_value_t<SiteRange>>;

/**
 * @brief The ordinary diagram: the Delaunay dual, borrowed from a triangulation
 *        of the sites; see @ref pgl::voronoiDiagram for the contract.
 *
 * A point is interior to its own cell and the cells tile the plane, so the
 * ordinary diagram *is* the dual of the Delaunay triangulation, and the way to
 * compute one is to build the other and dualize it — which @ref
 * pgl::Triangulation::voronoiDiagram already does, down to labeling each face
 * with the site whose cell it is. Building the triangulation from the caller's
 * own points rather than from copies stripped of their labels is what makes that
 * label the caller's element: a point's label is metadata that equality,
 * ordering and hashing all ignore, so it rides along without reaching any
 * predicate the triangulation runs.
 *
 * Sites with no triangle to dualize — fewer than three of them, or all equal or
 * all collinear — have no dual to borrow, and take the bisector construction of
 * the order-`k` entry point at `k = 1` instead.
 */
template <class ResultNumber, std::ranges::input_range SiteRange>
    requires PointConcept<std::ranges::range_value_t<SiteRange>>
voronoi_dual_t<ResultNumber, SiteRange> ordinaryDiagram(const SiteRange& sites) {
    using Element = std::ranges::range_value_t<SiteRange>;
    using Number = voronoi_number_t<ResultNumber, Element>;
    using Diagram = voronoi_dual_t<ResultNumber, SiteRange>;

    std::vector<Element> elements(std::ranges::begin(sites), std::ranges::end(sites));
    if (elements.empty()) {
        throw std::invalid_argument("pgl::voronoiDiagram: no sites to make a diagram of");
    }

    const Triangulation<Triangle<Element>> triangulation(elements);
    if (triangulation.numTriangles() != 0) {
        return triangulation.template voronoiDiagram<Number>();
    }

    std::vector<VoronoiSite<Number>> lifted;
    lifted.reserve(elements.size());
    for (const Element& element : elements) {
        lifted.push_back(voronoiSiteOf<Number>(element));
    }

    std::vector<Shape<Point<Number>>> curves;
    std::vector<std::pair<Number, int>> events;
    const auto emit = [&](const VoronoiBisector<Number>& bisector,
                          const std::optional<Number>& from, const std::optional<Number>& to) {
        voronoiAppendRun(curves, bisector, from, to);
    };
    for (std::size_t i = 0; i + 1 < lifted.size(); ++i) {
        for (std::size_t j = i + 1; j < lifted.size(); ++j) {
            voronoiPairEdges(lifted, i, j, 1, events, emit);
        }
    }

    Diagram diagram(curves, true);
    labelVoronoiFaces(diagram, lifted, elements, 1);
    return diagram;
}

/**
 * @brief The diagram both public entry points compute; see them for the
 *        contract.
 *
 * Point sites go through @ref voronoiByRefinement, which is Lee's refinement
 * over a Delaunay triangulation. What is left here is the general construction,
 * which every disk site takes and a point site takes only when the refinement
 * declines the input: it cuts each of the `O(n^2)` bisectors against every site
 * and keeps the runs where the pair is the `k`-th and `(k+1)`-th nearest.
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

    if constexpr (PointConcept<Element>) {
        Diagram refined;
        if (detail::voronoiByRefinement(refined, lifted, elements, k)) {
            return refined;
        }
    }

    std::vector<Shape<ResultPoint>> curves;
    const auto emit = [&](const detail::VoronoiBisector<Number>& bisector,
                          const std::optional<Number>& from, const std::optional<Number>& to) {
        detail::voronoiAppendRun(curves, bisector, from, to);
    };
    std::vector<std::pair<Number, int>> events;
    for (std::size_t i = 0; i + 1 < lifted.size(); ++i) {
        for (std::size_t j = i + 1; j < lifted.size(); ++j) {
            detail::voronoiPairEdges(lifted, i, j, k, events, emit);
        }
    }

    // Two edges of the diagram have different nearest pairs throughout their
    // relative interiors, so they meet only where a third site joins the tie,
    // which is an endpoint of both: there is nothing for the overlay to cut.
    Diagram diagram(curves, true);
    detail::labelVoronoiFaces(diagram, lifted, elements, k);
    return diagram;
}

}  // namespace detail

/**
 * @brief Computes the Voronoi diagram of a set of points.
 *
 * Every face of the result is labeled with the one site nearest to it, as an
 * element of the input container. The arrangement's edge labels are
 * default-constructed and have no meaning.
 *
 * This is the dual of the Delaunay triangulation of the sites, and it is
 * computed that way: the triangulation is built and
 * @ref Triangulation::voronoiDiagram dualizes it, so a caller who already holds
 * a triangulation of the same points should call that directly and skip
 * building a second one. Sites with no triangle to dualize — fewer than three of
 * them, or all equal or all collinear — fall back to the bisector construction
 * the order-`k` overload describes, at `k = 1`.
 *
 * Complexity: `O(n log n)`.
 *
 * @tparam ResultNumber Coordinate type of the arrangement vertices. The default
 *         is exact and overflow-free for integral input.
 * @tparam SiteRange Range of @ref pgl::Point.
 * @param sites Sites of the diagram. Repeated sites share a cell, which then
 *        carries one of them.
 * @return The unbounded arrangement of the diagram, every face labeled with its
 *         site.
 * @throws std::invalid_argument if there are no sites.
 * @see The overload below for the order-`k` diagram, and @ref powerDiagram for
 *      weighted sites.
 */
template <class ResultNumber = void, std::ranges::input_range SiteRange>
    requires PointConcept<std::ranges::range_value_t<SiteRange>>
[[nodiscard]] detail::voronoi_dual_t<ResultNumber, SiteRange> voronoiDiagram(
    const SiteRange& sites) {
    return detail::ordinaryDiagram<ResultNumber>(sites);
}

/**
 * @brief Computes the order-`k` diagram of a set of points.
 *
 * Every face of the result is labeled with the `k` sites nearest to it, as
 * elements of the input container and ordered by their position in it — a
 * one-element vector when `k` is 1, where the overload above computes the same
 * diagram and labels each face with the site itself. The arrangement's edge
 * labels are default-constructed and have no meaning.
 *
 * A face of the order-`k` diagram is the region where one set of `k` sites is
 * nearer than every other site; the sites of neighboring faces differ by a
 * single swap. Cells that are empty do not appear, so the number of faces is
 * generally far below the number of `k`-element subsets.
 *
 * Complexity: `O(k^2 n log n)` for sites whose cells have a bounded number of
 * neighbors each, which is the usual case; refining a cell with `d` of them
 * costs `O(d^3)`. The order-1 cells are the Delaunay triangulation's, and each
 * order after them is Lee's refinement of the one below: every cell of the
 * order-`k - 1` diagram is subdivided by the ordinary diagram of the sites it
 * does not own, which is a few conditions per neighboring pair rather than a
 * pass over the sites. Only the order asked for is ever laid out in the plane —
 * the orders below it are cells named by their sites and nothing else — and
 * since two edges of one diagram meet only at a shared endpoint, the one
 * @ref Arrangement that is built needs no splitting step.
 *
 * Repeated sites, and sites that are all collinear, have no refinement to take
 * and fall back to the general construction, which cuts each of the `O(n^2)`
 * bisectors against every site at `O(n^3 log n)`.
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
    const SiteRange& sites, int k) {
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
 * Labels and order are as in @ref voronoiDiagram, but neither the Delaunay dual
 * nor Lee's refinement is available: a disk's center need not lie in its own
 * cell, and a light disk can own a region buried inside a heavy one's cell,
 * which no neighbor of that cell names. So every order, `k` of 1 included, cuts
 * each of the `O(n^2)` bisectors against every site, at `O(n^3 log n)`.
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
