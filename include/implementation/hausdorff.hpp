#pragma once

#include "implementation/distancelinf.hpp"

/**
 * @file hausdorff.hpp
 * @brief L1 and LInf Hausdorff distances between bounded polygonal shapes that
 *        are not both convex.
 *
 * The Hausdorff distance between two compact sets is the larger of the two
 * directed distances `h(A, B) = max_{a in A} d(a, B)`. distancel1.hpp and
 * distancelinf.hpp answer the pairs of convex shapes, where `d(., B)` is convex
 * and `h(A, B)` is therefore attained at a vertex of `A`. This file answers every
 * other pair of @ref BoundedPolygonalConcept shapes: a @ref MonotoneChain, a
 * @ref Polyline, a @ref Polygon, a @ref PolygonWithHoles or a @ref PolygonSet
 * against each other and against the convex ones.
 *
 * ### The distance to one edge is a maximum of six affine functions
 *
 * Both norms are polyhedral: `|v| = max_k nu_k . v` over the four outer normals
 * `nu_k` of the unit ball, scaled so that `nu_k . w = 1` on the ball's facet
 * `k`. The distance from `p` to a convex polygon `C` is then
 * `max_n (n . p - h_C(n)) / h_D(n)` over the edge normals `n` of `C ⊕ D`, where
 * `D` is the unit ball and `h` a support function. For a segment `C = [a, b]`
 * those normals are the ball's four and the segment's two, so the distance to a
 * segment is the maximum of six affine functions of `p` with rational
 * coefficients, and every quantity below stays exact in a `ResultNumber` closed
 * under division.
 *
 * ### Where the directed distance is attained
 *
 * Call the edges of the target `B` its *sites* (a vertex with no edge is a
 * degenerate site). Off `B`, `d(p, B) = g(p) = min_s d_s(p)`, a lower envelope of
 * convex piecewise-linear functions; inside a region `B` it is zero. The maximum
 * of `d(., B)` over the source `A` is attained at one of
 *
 * 1. a vertex of `A`;
 * 2. a breakpoint of `g` along an edge of `A` — within a stretch where one site
 *    is nearest, `g` is convex along the edge and peaks at the stretch's ends;
 * 3. when `A` has an interior, a vertex of the cell of a site `s` inside the
 *    region `P` where one affine piece `l` of `d_s` is the largest.
 *
 * The first two are a one-dimensional lower envelope along each edge of `A`. The
 * third is one too, which is what makes it tractable. Pick a unit vector `u`
 * with `grad(l) . u = 1` (a vertex of the unit ball). Every other distance is
 * 1-Lipschitz, so moving a point of the cell by `-u` lowers `l` by exactly the
 * step and every `d_t` by at most as much: the cell is closed under that move.
 * In a frame with `u` pointing up, the set where `l <= d_t` for every other site
 * `t` is therefore everything below a ceiling `rho(sigma) = min_t rho_t(sigma)`,
 * where `rho_t` is where `d_t` first drops below `l` on the vertical line at
 * `sigma`: a convex piecewise-linear function on an open interval and `+inf`
 * outside it. The maximum of `l` over the cell is at a breakpoint of that
 * ceiling, where it meets the floor of `P`, or at one end of either.
 *
 * Every candidate is a point whose distance is recomputed from all the sites
 * before it counts, so a candidate can only ever report a distance the target
 * really has. Sites are fed to each envelope nearest first, and the feed stops
 * as soon as no remaining site can reach below what the envelope already
 * allows, which in practice leaves a handful per envelope.
 */

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace pgl {

namespace detail {

/**
 * @brief The L1 unit ball, as the Hausdorff search reads a polyhedral norm.
 *
 * `facets` are the outer normals `nu_k`, scaled so that `nu_k . w = 1` on the
 * facet they bound, and `corners` are the ball's vertices; the norm of a vector
 * is its largest product with a facet normal. `combine` turns the absolute
 * coordinate differences of a vector into its norm.
 */
struct HausdorffNormL1 {
    static constexpr int facets[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
    static constexpr int corners[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    template <class W>
    static constexpr W combine(const W& dx, const W& dy) {
        return dx + dy;
    }
};

/** @brief The LInf unit ball; see @ref HausdorffNormL1. */
struct HausdorffNormLInf {
    static constexpr int facets[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    static constexpr int corners[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};

    template <class W>
    static constexpr W combine(const W& dx, const W& dy) {
        return dx < dy ? dy : dx;
    }
};

/**
 * @brief Coordinate type the Hausdorff search computes in.
 *
 * The requested `ResultNumber` when it is closed under division, so that a
 * floating-point request computes in floating point; otherwise the exact
 * division type of the operands, whose result is converted at the very end. An
 * integral `ResultNumber` would truncate the intermediate crossings, not just
 * the answer.
 */
template <class ResultNumber, class Self, class Other>
using hausdorffWork_t = std::conditional_t<
    std::floating_point<ResultNumber> || RationalConcept<ResultNumber>, ResultNumber,
    division_result_t<std::common_type_t<typename Self::NumberType, typename Other::NumberType>>>;

/** @brief `a x + b y + c`. */
template <class W>
struct HausdorffAffine {
    W a{};
    W b{};
    W c{};

    constexpr W at(const Point<W>& p) const {
        return a * p.x() + b * p.y() + c;
    }

    friend constexpr bool operator==(const HausdorffAffine&, const HausdorffAffine&) = default;
};

/**
 * @brief One edge of the target, with its distance function as affine pieces.
 *
 * The distance from a point to the edge is the largest of the pieces. The box is
 * the edge's bounding box, whose distance in the norm bounds the edge's from
 * below.
 */
template <class W>
struct HausdorffSite {
    std::array<HausdorffAffine<W>, 6> pieces{};
    std::size_t count = 0;
    W minX{};
    W maxX{};
    W minY{};
    W maxY{};

    constexpr W distance(const Point<W>& p) const {
        W result = pieces[0].at(p);
        for (std::size_t i = 1; i < count; ++i) {
            const W value = pieces[i].at(p);
            if (result < value) {
                result = value;
            }
        }
        return result;
    }
};

/** @brief The distance pieces of the segment from @p first to @p second. */
template <class W, class Norm>
constexpr HausdorffSite<W> makeHausdorffSite(const Point<W>& first, const Point<W>& second) {
    HausdorffSite<W> site;
    const auto add = [&site](const HausdorffAffine<W>& piece) {
        for (std::size_t i = 0; i < site.count; ++i) {
            if (site.pieces[i] == piece) {
                return;
            }
        }
        site.pieces[site.count++] = piece;
    };
    for (const auto& facet : Norm::facets) {
        const W nx(facet[0]);
        const W ny(facet[1]);
        const W atFirst = nx * first.x() + ny * first.y();
        const W atSecond = nx * second.x() + ny * second.y();
        add({nx, ny, -(atFirst < atSecond ? atSecond : atFirst)});
    }
    if (first != second) {
        // The segment's own normal, scaled to support 1 on the unit ball.
        W nx = first.y() - second.y();
        W ny = second.x() - first.x();
        W support = nx * W(Norm::corners[0][0]) + ny * W(Norm::corners[0][1]);
        for (const auto& corner : Norm::corners) {
            const W value = nx * W(corner[0]) + ny * W(corner[1]);
            if (support < value) {
                support = value;
            }
        }
        nx = nx / support;
        ny = ny / support;
        const W c = -(nx * first.x() + ny * first.y());
        add({nx, ny, c});
        add({-nx, -ny, -c});
    }
    site.minX = first.x() < second.x() ? first.x() : second.x();
    site.maxX = first.x() < second.x() ? second.x() : first.x();
    site.minY = first.y() < second.y() ? first.y() : second.y();
    site.maxY = first.y() < second.y() ? second.y() : first.y();
    return site;
}

/**
 * @brief A bounded polygonal shape as the Hausdorff search reads it.
 *
 * Its edges, the vertices that no edge covers, and whether it is a region — in
 * which case the edges are closed rings bounding it under the even-odd rule, so
 * a ring nested in another is a hole and one nested in a hole an island.
 */
template <class W>
struct HausdorffOperand {
    std::vector<std::array<Point<W>, 2>> edges;
    std::vector<Point<W>> points;
    bool region = false;
    W minX{};
    W maxX{};
    W minY{};
    W maxY{};

    /** @brief Whether the closed region contains @p p. */
    constexpr bool regionContains(const Point<W>& p) const {
        bool inside = false;
        for (const auto& [a, b] : edges) {
            const W cross = (b.x() - a.x()) * (p.y() - a.y()) - (b.y() - a.y()) * (p.x() - a.x());
            const W zero{};
            if (cross == zero) {
                const bool withinX = !(p.x() < a.x() && p.x() < b.x()) && !(a.x() < p.x() && b.x() < p.x());
                const bool withinY = !(p.y() < a.y() && p.y() < b.y()) && !(a.y() < p.y() && b.y() < p.y());
                if (withinX && withinY) {
                    return true;
                }
            }
            // The edge crosses the rightward ray from p when it spans p's height
            // (half-open, so a vertex on the ray counts once) and p is on the
            // side of the edge the ray leaves through.
            if (!(p.y() < a.y()) && p.y() < b.y() && zero < cross) {
                inside = !inside;
            } else if (!(p.y() < b.y()) && p.y() < a.y() && cross < zero) {
                inside = !inside;
            }
        }
        return inside;
    }
};

/** @brief @p shape as a @ref HausdorffOperand over `W`. */
template <class W, class ShapeType>
constexpr HausdorffOperand<W> hausdorffOperand(const ShapeType& shape) {
    HausdorffOperand<W> operand;
    const auto convert = [](const auto& p) {
        return Point<W>(convertCoordinate<W>(p.x()), convertCoordinate<W>(p.y()));
    };
    if constexpr (PointConcept<ShapeType>) {
        operand.points.push_back(convert(shape));
    } else {
        operand.region = !(SegmentConcept<ShapeType> || OrientedSegmentConcept<ShapeType> ||
                           MonotoneChainConcept<ShapeType> || PolylineConcept<ShapeType>);
        for (const auto& edge : shape.edges()) {
            operand.edges.push_back({convert(edge[0]), convert(edge[1])});
        }
        if (operand.edges.empty()) {
            for (const auto& vertex : shape.vertices()) {
                operand.points.push_back(convert(vertex));
            }
        }
    }
    // Neither shape may be empty: the empty set has no Hausdorff distance to
    // anything.
    PGL_ASSERT(!operand.edges.empty() || !operand.points.empty());
    bool seeded = false;
    const auto grow = [&](const Point<W>& p) {
        if (!seeded) {
            operand.minX = operand.maxX = p.x();
            operand.minY = operand.maxY = p.y();
            seeded = true;
            return;
        }
        if (p.x() < operand.minX) operand.minX = p.x();
        if (operand.maxX < p.x()) operand.maxX = p.x();
        if (p.y() < operand.minY) operand.minY = p.y();
        if (operand.maxY < p.y()) operand.maxY = p.y();
    };
    for (const auto& [a, b] : operand.edges) {
        grow(a);
        grow(b);
    }
    for (const auto& p : operand.points) {
        grow(p);
    }
    return operand;
}

/** @brief `slope x + intercept`, or `+inf` everywhere. */
template <class W>
struct HausdorffLine {
    W slope{};
    W intercept{};
    bool infinite = true;

    constexpr W at(const W& x) const {
        return slope * x + intercept;
    }

    constexpr bool sameAs(const HausdorffLine& other) const {
        if (infinite || other.infinite) {
            return infinite && other.infinite;
        }
        return slope == other.slope && intercept == other.intercept;
    }
};

/** @brief A value that may be `+inf`. */
template <class W>
struct HausdorffValue {
    W value{};
    bool infinite = true;
};

template <class W>
constexpr HausdorffValue<W> hausdorffMin(const HausdorffValue<W>& first, const HausdorffValue<W>& second) {
    if (first.infinite) {
        return second;
    }
    if (second.infinite) {
        return first;
    }
    return second.value < first.value ? second : first;
}

/**
 * @brief A piecewise-linear function on a closed interval, with jumps.
 *
 * `xs` are the breakpoints, the interval's ends included; `values` holds the
 * function *at* each breakpoint and `lines[i]` the function on the open interval
 * `(xs[i], xs[i + 1])`. Keeping the value at a breakpoint apart from the pieces
 * on either side is what lets a ceiling that jumps to `+inf` at the end of an
 * open interval, as the one of a single site does, be represented exactly.
 */
template <class W>
struct HausdorffEnvelope {
    std::vector<W> xs;
    std::vector<HausdorffValue<W>> values;
    std::vector<HausdorffLine<W>> lines;

    /** @brief `+inf` everywhere on `[lo, hi]`. */
    static constexpr HausdorffEnvelope unbounded(const W& lo, const W& hi) {
        HausdorffEnvelope envelope;
        envelope.xs = {lo, hi};
        envelope.values = {HausdorffValue<W>{}, HausdorffValue<W>{}};
        envelope.lines = {HausdorffLine<W>{}};
        return envelope;
    }

    /** @brief Appends @p line up to the breakpoint @p x, where the function is @p value. */
    constexpr void close(const HausdorffLine<W>& line, const W& x, const HausdorffValue<W>& value) {
        lines.push_back(line);
        xs.push_back(x);
        values.push_back(value);
    }

    /** @brief Line @p interval evaluated at @p x. */
    constexpr HausdorffValue<W> lineValue(std::size_t interval, const W& x) const {
        const auto& line = lines[interval];
        return line.infinite ? HausdorffValue<W>{} : HausdorffValue<W>{line.at(x), false};
    }

    /** @brief Whether the function is `+inf` anywhere. */
    constexpr bool unboundedSomewhere() const {
        for (const auto& value : values) {
            if (value.infinite) {
                return true;
            }
        }
        for (const auto& line : lines) {
            if (line.infinite) {
                return true;
            }
        }
        return false;
    }
};

/** @brief Drops the breakpoints across which the function continues unchanged. */
template <class W>
constexpr HausdorffEnvelope<W> hausdorffCompress(const HausdorffEnvelope<W>& envelope) {
    HausdorffEnvelope<W> result;
    result.xs.push_back(envelope.xs.front());
    result.values.push_back(envelope.values.front());
    for (std::size_t i = 0; i < envelope.lines.size(); ++i) {
        const auto& line = envelope.lines[i];
        if (!result.lines.empty() && result.lines.back().sameAs(line)) {
            const auto& joint = result.values.back();
            const bool continuous = line.infinite
                                        ? joint.infinite
                                        : (!joint.infinite && joint.value == line.at(result.xs.back()));
            if (continuous) {
                result.xs.back() = envelope.xs[i + 1];
                result.values.back() = envelope.values[i + 1];
                continue;
            }
        }
        result.close(line, envelope.xs[i + 1], envelope.values[i + 1]);
    }
    return result;
}

/** @brief The pointwise minimum of two envelopes over the same interval. */
template <class W>
constexpr HausdorffEnvelope<W> hausdorffLower(const HausdorffEnvelope<W>& first,
                                              const HausdorffEnvelope<W>& second) {
    HausdorffEnvelope<W> result;
    result.xs.push_back(first.xs.front());
    result.values.push_back(hausdorffMin(first.values.front(), second.values.front()));
    const W zero{};
    std::size_t i = 0;
    std::size_t j = 0;
    W x = first.xs.front();
    while (i < first.lines.size() && j < second.lines.size()) {
        const W& firstEnd = first.xs[i + 1];
        const W& secondEnd = second.xs[j + 1];
        const bool firstEnds = !(secondEnd < firstEnd);
        const bool secondEnds = !(firstEnd < secondEnd);
        const W next = firstEnds ? firstEnd : secondEnd;
        const auto& a = first.lines[i];
        const auto& b = second.lines[j];
        const auto atNext = hausdorffMin(firstEnds ? first.values[i + 1] : first.lineValue(i, next),
                                         secondEnds ? second.values[j + 1] : second.lineValue(j, next));
        if (a.infinite || b.infinite) {
            result.close(a.infinite ? b : a, next, atNext);
        } else {
            const W startGap = a.at(x) - b.at(x);
            const W endGap = a.at(next) - b.at(next);
            if (!(zero < startGap) && !(zero < endGap)) {
                result.close(a, next, atNext);
            } else if (!(startGap < zero) && !(endGap < zero)) {
                result.close(b, next, atNext);
            } else {
                // The lines cross strictly inside the interval.
                const W cross = (b.intercept - a.intercept) / (a.slope - b.slope);
                const auto& before = startGap < zero ? a : b;
                const auto& after = startGap < zero ? b : a;
                if (x < cross && cross < next) {
                    result.close(before, cross, HausdorffValue<W>{a.at(cross), false});
                    result.close(after, next, atNext);
                } else {
                    // Only rounding puts the crossing outside the interval, next
                    // to one of its ends; the line lower in between wins.
                    const W middle = (x + next) / W(2);
                    result.close(b.at(middle) < a.at(middle) ? b : a, next, atNext);
                }
            }
        }
        if (firstEnds) {
            ++i;
        }
        if (secondEnds) {
            ++j;
        }
        x = next;
    }
    return hausdorffCompress(result);
}

/** @brief The upper envelope of finite @p lines over `[lo, hi]`, `lo < hi`. */
template <class W>
constexpr HausdorffEnvelope<W> hausdorffUpper(const std::vector<HausdorffLine<W>>& lines, const W& lo,
                                              const W& hi) {
    std::size_t current = 0;
    for (std::size_t k = 1; k < lines.size(); ++k) {
        const W value = lines[k].at(lo);
        const W best = lines[current].at(lo);
        if (best < value || (value == best && lines[current].slope < lines[k].slope)) {
            current = k;
        }
    }
    HausdorffEnvelope<W> envelope;
    envelope.xs.push_back(lo);
    envelope.values.push_back(HausdorffValue<W>{lines[current].at(lo), false});
    W x = lo;
    while (true) {
        bool found = false;
        std::size_t following = current;
        W switchAt = hi;
        for (std::size_t k = 0; k < lines.size(); ++k) {
            if (!(lines[current].slope < lines[k].slope)) {
                continue;
            }
            const W cross = (lines[current].intercept - lines[k].intercept) / (lines[k].slope - lines[current].slope);
            if (!(x < cross) || !(cross < hi)) {
                continue;
            }
            if (!found || cross < switchAt || (cross == switchAt && lines[following].slope < lines[k].slope)) {
                found = true;
                following = k;
                switchAt = cross;
            }
        }
        if (!found) {
            envelope.close(lines[current], hi, HausdorffValue<W>{lines[current].at(hi), false});
            return envelope;
        }
        envelope.close(lines[current], switchAt, HausdorffValue<W>{lines[current].at(switchAt), false});
        current = following;
        x = switchAt;
    }
}

/**
 * @brief The directed Hausdorff distance from a source to a target operand.
 *
 * See the file comment for the three candidate families and why they suffice.
 */
template <class W, class Norm>
class HausdorffSearch {
public:
    constexpr HausdorffSearch(const HausdorffOperand<W>& source, const HausdorffOperand<W>& target)
        : source_(source), target_(target) {
        for (const auto& [a, b] : target.edges) {
            sites_.push_back(makeHausdorffSite<W, Norm>(a, b));
        }
        for (const auto& p : target.points) {
            sites_.push_back(makeHausdorffSite<W, Norm>(p, p));
        }
    }

    /**
     * @brief `max_{p in source} d(p, target)`.
     *
     * With @p targetConvex only the source's vertices are measured, which is the
     * whole answer when the distance to the target is convex.
     */
    constexpr W run(bool targetConvex) {
        for (const auto& p : source_.points) {
            offer(distance(p));
        }
        std::vector<std::array<W, 2>> ends;
        ends.reserve(source_.edges.size());
        for (const auto& [a, b] : source_.edges) {
            ends.push_back({distance(a), distance(b)});
            offer(ends.back()[0]);
            offer(ends.back()[1]);
        }
        if (targetConvex) {
            return best_;
        }
        for (std::size_t i = 0; i < source_.edges.size(); ++i) {
            scanEdge(source_.edges[i][0], source_.edges[i][1], ends[i][0], ends[i][1]);
        }
        if (source_.region) {
            for (std::size_t s = 0; s < sites_.size(); ++s) {
                scanSite(s);
            }
        }
        return best_;
    }

private:
    using Envelope = HausdorffEnvelope<W>;
    using Line = HausdorffLine<W>;

    const HausdorffOperand<W>& source_;
    const HausdorffOperand<W>& target_;
    std::vector<HausdorffSite<W>> sites_;
    W best_{};

    static constexpr W absolute(const W& value) {
        return value < W{} ? -value : value;
    }

    // The distance in the norm between two boxes; at most that between any two
    // points in them.
    static constexpr W boxGap(const W& minX, const W& maxX, const W& minY, const W& maxY,
                              const HausdorffSite<W>& site) {
        const W zero{};
        W gapX = zero;
        if (maxX < site.minX) {
            gapX = site.minX - maxX;
        } else if (site.maxX < minX) {
            gapX = minX - site.maxX;
        }
        W gapY = zero;
        if (maxY < site.minY) {
            gapY = site.minY - maxY;
        } else if (site.maxY < minY) {
            gapY = minY - site.maxY;
        }
        return Norm::combine(gapX, gapY);
    }

    constexpr void offer(const W& value) {
        if (best_ < value) {
            best_ = value;
        }
    }

    // Distance to the nearest site, skipping the sites whose box is already
    // farther than the nearest one found.
    constexpr W siteDistance(const Point<W>& p) const {
        W result{};
        bool seeded = false;
        for (const auto& site : sites_) {
            if (seeded && !(boxGap(p.x(), p.x(), p.y(), p.y(), site) < result)) {
                continue;
            }
            const W value = site.distance(p);
            if (!seeded || value < result) {
                result = value;
                seeded = true;
            }
        }
        return result;
    }

    constexpr W distance(const Point<W>& p) const {
        if (target_.region && target_.regionContains(p)) {
            return W{};
        }
        return siteDistance(p);
    }

    // Takes the best candidate that the target really is at the claimed distance
    // from. `candidates` hold a claimed distance and a point; `admissible` says
    // whether a point may count at all.
    template <class Admissible>
    constexpr void settle(std::vector<std::pair<W, Point<W>>>& candidates, Admissible&& admissible) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& first, const auto& second) { return second.first < first.first; });
        for (const auto& [claimed, p] : candidates) {
            if (!(best_ < claimed)) {
                return;
            }
            if (!admissible(p)) {
                continue;
            }
            const W measured = siteDistance(p);
            if constexpr (!std::floating_point<W>) {
                PGL_ASSERT(measured == claimed);
            }
            offer(measured);
            return;
        }
    }

    // Sites in the order of their box distance from a box, nearest first.
    constexpr std::vector<std::pair<W, std::size_t>> nearestFirst(const W& minX, const W& maxX, const W& minY,
                                                                  const W& maxY, std::size_t skip) const {
        std::vector<std::pair<W, std::size_t>> order;
        order.reserve(sites_.size());
        for (std::size_t t = 0; t < sites_.size(); ++t) {
            if (t != skip) {
                order.emplace_back(boxGap(minX, maxX, minY, maxY, sites_[t]), t);
            }
        }
        std::sort(order.begin(), order.end(),
                  [](const auto& first, const auto& second) { return first.first < second.first; });
        return order;
    }

    // Candidate family 2: the breakpoints of the distance along the edge ab.
    constexpr void scanEdge(const Point<W>& a, const Point<W>& b, const W& atA, const W& atB) {
        const W dx = b.x() - a.x();
        const W dy = b.y() - a.y();
        // The distance is 1-Lipschitz, so it stays below this bound on the edge.
        const W two(2);
        if (!(best_ < (atA + atB + Norm::combine(absolute(dx), absolute(dy))) / two)) {
            return;
        }
        const W zero{};
        const W one(1);
        const auto order = nearestFirst(a.x() < b.x() ? a.x() : b.x(), a.x() < b.x() ? b.x() : a.x(),
                                        a.y() < b.y() ? a.y() : b.y(), a.y() < b.y() ? b.y() : a.y(), sites_.size());
        Envelope envelope = Envelope::unbounded(zero, one);
        bool bounded = false;
        W top{};
        std::vector<Line> lines;
        for (const auto& [gap, t] : order) {
            // A site at least `top` away from the edge cannot go below it.
            if (bounded && !(gap < top)) {
                break;
            }
            lines.clear();
            const auto& site = sites_[t];
            for (std::size_t i = 0; i < site.count; ++i) {
                const auto& piece = site.pieces[i];
                lines.push_back(Line{piece.a * dx + piece.b * dy, piece.at(a), false});
            }
            envelope = hausdorffLower(envelope, hausdorffUpper(lines, zero, one));
            bounded = true;
            top = envelope.values.front().value;
            for (const auto& value : envelope.values) {
                if (top < value.value) {
                    top = value.value;
                }
            }
            if (!(best_ < top)) {
                return;
            }
        }
        std::vector<std::pair<W, Point<W>>> candidates;
        for (std::size_t k = 0; k < envelope.xs.size(); ++k) {
            const W& claimed = envelope.values[k].value;
            if (best_ < claimed) {
                const W& sigma = envelope.xs[k];
                candidates.emplace_back(claimed, Point<W>(a.x() + sigma * dx, a.y() + sigma * dy));
            }
        }
        settle(candidates, [this](const Point<W>& p) { return !(target_.region && target_.regionContains(p)); });
    }

    // The frame of one affine piece: `u` is a unit-ball corner the piece rises
    // along at rate one, `v` completes it, and `p = sigma v + h u`.
    struct Frame {
        W ux, uy, vx, vy, det;

        constexpr Point<W> point(const W& sigma, const W& h) const {
            return Point<W>(sigma * vx + h * ux, sigma * vy + h * uy);
        }

        constexpr W sigmaOf(const W& x, const W& y) const {
            return (x * uy - y * ux) * det;
        }

        // The coefficients of `piece` as `A sigma + B h + C`.
        constexpr std::array<W, 3> express(const W& a, const W& b, const W& c) const {
            return {a * vx + b * vy, a * ux + b * uy, c};
        }
    };

    static constexpr Frame frameOf(const HausdorffAffine<W>& piece) {
        std::size_t chosen = 0;
        W rise = piece.a * W(Norm::corners[0][0]) + piece.b * W(Norm::corners[0][1]);
        for (std::size_t k = 1; k < 4; ++k) {
            const W value = piece.a * W(Norm::corners[k][0]) + piece.b * W(Norm::corners[k][1]);
            if (rise < value) {
                rise = value;
                chosen = k;
            }
        }
        const int ux = Norm::corners[chosen][0];
        const int uy = Norm::corners[chosen][1];
        const int vx = uy != 0 ? 1 : 0;
        const int vy = uy != 0 ? 0 : 1;
        return Frame{W(ux), W(uy), W(vx), W(vy), W(vx * uy - vy * ux)};
    }

    // Where the site first comes strictly nearer than `piece` on each vertical
    // line of the frame, over [lo, hi].
    constexpr Envelope ceilingOf(const HausdorffSite<W>& site, const HausdorffAffine<W>& piece,
                                 const Frame& frame, const W& lo, const W& hi) const {
        const W zero{};
        bool hasLow = false;
        bool hasHigh = false;
        W low{};
        W high{};
        std::vector<Line> rises;
        for (std::size_t i = 0; i < site.count; ++i) {
            const auto& other = site.pieces[i];
            const auto [a, b, c] = frame.express(other.a - piece.a, other.b - piece.b, other.c - piece.c);
            if (b < zero) {
                // The gap `a sigma + b h + c` turns negative above this height.
                const W down = -b;
                rises.push_back(Line{a / down, c / down, false});
            } else if (a == zero) {
                // A gap constant along the whole line: never negative here.
                if (!(c < zero)) {
                    return Envelope::unbounded(lo, hi);
                }
            } else {
                const W root = -c / a;
                if (zero < a) {
                    if (!hasHigh || root < high) {
                        high = root;
                        hasHigh = true;
                    }
                } else if (!hasLow || low < root) {
                    low = root;
                    hasLow = true;
                }
            }
        }
        const bool openStart = hasLow && !(low < lo);
        const bool openEnd = hasHigh && !(hi < high);
        const W start = openStart ? low : lo;
        const W end = openEnd ? high : hi;
        // Every distance falls somewhere along `-u`, so some piece rises; an empty
        // list would mean the site is nearer than `piece` on the whole line.
        PGL_ASSERT(!rises.empty());
        if (!(start < end) || rises.empty()) {
            return Envelope::unbounded(lo, hi);
        }
        Envelope inner = hausdorffUpper(rises, start, end);
        if (openStart) {
            inner.values.front() = HausdorffValue<W>{};
        }
        if (openEnd) {
            inner.values.back() = HausdorffValue<W>{};
        }
        Envelope result;
        result.xs.push_back(lo);
        if (lo < start) {
            result.values.push_back(HausdorffValue<W>{});
            result.close(Line{}, start, inner.values.front());
        } else {
            result.values.push_back(inner.values.front());
        }
        for (std::size_t k = 0; k < inner.lines.size(); ++k) {
            result.close(inner.lines[k], inner.xs[k + 1], inner.values[k + 1]);
        }
        if (end < hi) {
            result.close(Line{}, hi, HausdorffValue<W>{});
        }
        return result;
    }

    // Calls `visit(sigma, h)` for every candidate top of the cell: a point on
    // the ceiling at a breakpoint of it or of the floor, or where the two meet,
    // taking both one-sided limits at a jump, and on or above the floor. Walks
    // the ceiling's intervals in order, so no global sort is needed.
    template <class Visit>
    constexpr void forEachTop(const Envelope& ceiling, const std::vector<Line>& floor, Visit&& visit) const {
        std::vector<W> bends;
        for (std::size_t f = 0; f < floor.size(); ++f) {
            for (std::size_t g = f + 1; g < floor.size(); ++g) {
                if (!(floor[f].slope == floor[g].slope)) {
                    bends.push_back((floor[g].intercept - floor[f].intercept) / (floor[f].slope - floor[g].slope));
                }
            }
        }
        const auto floorAt = [&floor](const W& sigma, W& bottom) {
            bool hasFloor = false;
            for (const auto& line : floor) {
                const W value = line.at(sigma);
                if (!hasFloor || bottom < value) {
                    bottom = value;
                    hasFloor = true;
                }
            }
            return hasFloor;
        };
        const auto offer = [&](const W& sigma, const HausdorffValue<W>& top) {
            W bottom{};
            if (!top.infinite && (!floorAt(sigma, bottom) || !(top.value < bottom))) {
                visit(sigma, top.value);
            }
        };
        std::vector<W> inside;
        for (std::size_t k = 0; k < ceiling.xs.size(); ++k) {
            const W& sigma = ceiling.xs[k];
            offer(sigma, ceiling.values[k]);
            if (k > 0) {
                offer(sigma, ceiling.lineValue(k - 1, sigma));
            }
            if (k == ceiling.lines.size()) {
                break;
            }
            offer(sigma, ceiling.lineValue(k, sigma));

            // Inside the open interval the ceiling is one line: offer it where
            // the floor bends and where it crosses the floor.
            const W& next = ceiling.xs[k + 1];
            const auto& line = ceiling.lines[k];
            if (line.infinite) {
                continue;
            }
            inside.clear();
            for (const W& bend : bends) {
                if (sigma < bend && bend < next) {
                    inside.push_back(bend);
                }
            }
            for (const auto& bottom : floor) {
                if (!(bottom.slope == line.slope)) {
                    const W cross = (bottom.intercept - line.intercept) / (line.slope - bottom.slope);
                    if (sigma < cross && cross < next) {
                        inside.push_back(cross);
                    }
                }
            }
            for (const W& at : inside) {
                offer(at, HausdorffValue<W>{line.at(at), false});
            }
        }
    }

    // Candidate family 3: the vertices of the cell of site `s`, one affine
    // piece of its distance at a time.
    constexpr void scanSite(std::size_t s) {
        const auto& site = sites_[s];
        const W zero{};
        const std::array<Point<W>, 4> box{Point<W>(source_.minX, source_.minY), Point<W>(source_.maxX, source_.minY),
                                          Point<W>(source_.maxX, source_.maxY), Point<W>(source_.minX, source_.maxY)};
        std::vector<std::pair<W, std::size_t>> order;
        bool ordered = false;
        for (std::size_t j = 0; j < site.count; ++j) {
            const auto& piece = site.pieces[j];
            const Frame frame = frameOf(piece);

            // Over the source's box the piece peaks at a corner.
            W boxBound = piece.at(box[0]);
            W lo = frame.sigmaOf(box[0].x(), box[0].y());
            W hi = lo;
            for (std::size_t k = 1; k < 4; ++k) {
                const W value = piece.at(box[k]);
                if (boxBound < value) {
                    boxBound = value;
                }
                const W sigma = frame.sigmaOf(box[k].x(), box[k].y());
                if (sigma < lo) {
                    lo = sigma;
                }
                if (hi < sigma) {
                    hi = sigma;
                }
            }
            if (!(best_ < boxBound)) {
                continue;
            }

            // The region where this piece is the site's distance: a floor below
            // which another piece takes over, and a sigma range.
            std::vector<Line> floor;
            bool empty = false;
            for (std::size_t i = 0; i < site.count && !empty; ++i) {
                if (i == j) {
                    continue;
                }
                const auto& other = site.pieces[i];
                const auto [a, b, c] = frame.express(piece.a - other.a, piece.b - other.b, piece.c - other.c);
                if (zero < b) {
                    floor.push_back(Line{-a / b, -c / b, false});
                } else if (a == zero) {
                    empty = c < zero;
                } else {
                    const W root = -c / a;
                    if (zero < a) {
                        if (lo < root) {
                            lo = root;
                        }
                    } else if (root < hi) {
                        hi = root;
                    }
                }
            }
            if (empty || !(lo < hi)) {
                continue;
            }

            if (!ordered) {
                order = nearestFirst(site.minX, site.maxX, site.minY, site.maxY, s);
                ordered = true;
            }
            // The piece in the frame is `sigma across + h + c`.
            const W across = piece.a * frame.vx + piece.b * frame.vy;
            Envelope ceiling = Envelope::unbounded(lo, hi);
            W bound = boxBound;
            for (const auto& [gap, t] : order) {
                // A site that far cannot come nearer than this one anywhere the
                // piece is at most `bound`.
                if (!(gap < bound + bound)) {
                    break;
                }
                ceiling = hausdorffLower(ceiling, ceilingOf(sites_[t], piece, frame, lo, hi));
                if (!ceiling.unboundedSomewhere()) {
                    bool any = false;
                    W top{};
                    forEachTop(ceiling, floor, [&](const W& sigma, const W& h) {
                        const W value = sigma * across + h + piece.c;
                        if (!any || top < value) {
                            top = value;
                            any = true;
                        }
                    });
                    if (!any) {
                        bound = best_;
                    } else if (top < bound) {
                        bound = top;
                    }
                }
                if (!(best_ < bound)) {
                    break;
                }
            }
            if (!(best_ < bound)) {
                continue;
            }
            std::vector<std::pair<W, Point<W>>> candidates;
            forEachTop(ceiling, floor, [&](const W& sigma, const W& h) {
                const Point<W> p = frame.point(sigma, h);
                const W value = piece.at(p);
                if (best_ < value) {
                    candidates.emplace_back(value, p);
                }
            });
            settle(candidates, [this](const Point<W>& p) {
                return source_.regionContains(p) && !(target_.region && target_.regionContains(p));
            });
        }
    }
};

/**
 * @brief The Hausdorff distance between two bounded polygonal shapes under
 *        `Norm`, at least one of them not convex.
 */
template <class ResultNumber, class Norm, class Self, class Other>
constexpr ResultNumber hausdorffPolygonal(const Self& self, const Other& other) {
    using W = hausdorffWork_t<ResultNumber, Self, Other>;
    const auto first = hausdorffOperand<W>(self);
    const auto second = hausdorffOperand<W>(other);
    const W forward = HausdorffSearch<W, Norm>(first, second).run(BoundedConvexConcept<Other>);
    const W backward = HausdorffSearch<W, Norm>(second, first).run(BoundedConvexConcept<Self>);
    return static_cast<ResultNumber>(forward < backward ? backward : forward);
}

}  // namespace detail

// -----------------------------------------------------------------------------
// MonotoneChain

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, point);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<MonotoneChain<PointType, LabelType, Storage>>)
constexpr auto MonotoneChain<PointType, LabelType, Storage>::hausdorffDistanceL1(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, point);
}

template <class PointType, class LabelType, class Storage>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<MonotoneChain<PointType, LabelType, Storage>>)
constexpr auto MonotoneChain<PointType, LabelType, Storage>::hausdorffDistanceLInf(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, other);
}

// -----------------------------------------------------------------------------
// Polyline

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polyline<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<Polyline<PointType, LabelType>>)
constexpr auto Polyline<PointType, LabelType>::hausdorffDistanceL1(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, other);
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polyline<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<Polyline<PointType, LabelType>>)
constexpr auto Polyline<PointType, LabelType>::hausdorffDistanceLInf(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, other);
}

// -----------------------------------------------------------------------------
// Polygon

template <class PointType_, class TLabel>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polygon<PointType_, TLabel>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, point);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<Polygon<PointType_, TLabel>>)
constexpr auto Polygon<PointType_, TLabel>::hausdorffDistanceL1(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polygon<PointType_, TLabel>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, point);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<Polygon<PointType_, TLabel>>)
constexpr auto Polygon<PointType_, TLabel>::hausdorffDistanceLInf(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, other);
}

// -----------------------------------------------------------------------------
// PolygonWithHoles

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto PolygonWithHoles<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<PolygonWithHoles<PointType, LabelType>>)
constexpr auto PolygonWithHoles<PointType, LabelType>::hausdorffDistanceL1(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, other);
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto PolygonWithHoles<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape> &&
             detail::shapeRank<OtherShape> <= detail::shapeRank<PolygonWithHoles<PointType, LabelType>>)
constexpr auto PolygonWithHoles<PointType, LabelType>::hausdorffDistanceLInf(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, other);
}

// -----------------------------------------------------------------------------
// PolygonSet

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto PolygonSet<PointType, LabelType>::hausdorffDistanceL1(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape>)
constexpr auto PolygonSet<PointType, LabelType>::hausdorffDistanceL1(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormL1>(*this, other);
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto PolygonSet<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires(!PointConcept<OtherShape>)
constexpr auto PolygonSet<PointType, LabelType>::hausdorffDistanceLInf(const OtherShape& other) const {
    return detail::hausdorffPolygonal<ResultNumber, detail::HausdorffNormLInf>(*this, other);
}

}  // namespace pgl
