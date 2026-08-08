#pragma once

#include "core/hash.hpp"
#include "core/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

/**
 * @file visibilitygraph.hpp
 * @brief Visibility graphs of simple polygons.
 */

namespace pgl {

template <class PointType, class LabelType>
Graph<PointType> Polygon<PointType, LabelType>::visibilityGraph() const {
    using NumberType = typename PointType::NumberType;
    using SweepNumber = detail::promoted_number_t<NumberType>;
    using WideNumber = detail::promoted_number_t<SweepNumber>;

    struct Direction {
        SweepNumber x{};
        SweepNumber y{};
    };

    Graph<PointType> result;
    const auto translatedVertices = vertices();
    const std::size_t n = translatedVertices.size();

    // Add vertices explicitly: Graph::addEdge deliberately ignores self-loops,
    // so an isolated vertex (notably a one-point polygon) would otherwise be
    // absent from the result.
    for (const auto& vertex : translatedVertices) {
        result.addVertex(vertex);
    }

    // A convex polygon, including one collapsed to a segment, contains every
    // segment between its vertices. Besides handling the documented
    // degeneracies in O(n^2), this avoids a sweep when the answer is known to
    // be the complete graph.
    if (n < 3 || isDegenerate() || isConvex()) {
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                result.addEdge(translatedVertices[i], translatedVertices[j]);
            }
        }
        return result;
    }

    // Polygon sides are always visible. Adding them up front also lets each
    // angular sweep concentrate on obstruction and vertex-cone tests.
    for (std::size_t i = 0; i < n; ++i) {
        result.addEdge(translatedVertices[i], translatedVertices[(i + 1) % n]);
    }

    // Whether the ray from vertex i towards vertex j enters the closed polygon
    // locally at i. Precomputing this O(n^2) table avoids repeating the same two
    // orientation tests both for endpoints and for collinear vertices met on a
    // candidate segment.
    std::vector<std::uint8_t> entersClosed(n * n);
    for (std::size_t i = 0; i < n; ++i) {
        const PointType& previous = translatedVertices[(i + n - 1) % n];
        const PointType& vertex = translatedVertices[i];
        const PointType& next = translatedVertices[(i + 1) % n];
        const auto turn = orientationSign(previous, vertex, next);
        for (std::size_t j = 0; j < n; ++j) {
            const auto incoming = orientationSign(previous, vertex, translatedVertices[j]);
            const auto outgoing = orientationSign(vertex, next, translatedVertices[j]);
            entersClosed[i * n + j] =
                turn > 0 ? incoming >= 0 && outgoing >= 0
              : turn < 0 ? incoming >= 0 || outgoing >= 0
                         : incoming >= 0;
        }
    }

    // Sweep all other vertices counterclockwise around each source. The active
    // set contains the polygon edges crossing the ray between two consecutive
    // vertex directions, ordered by their intersection distance from source.
    // Simple-polygon edges never cross, so that relative order cannot change
    // between events; only the two edges incident to an event vertex enter or
    // leave the set.
    for (std::size_t source = 0; source < n; ++source) {
        const SweepNumber sourceX(translatedVertices[source].x());
        const SweepNumber sourceY(translatedVertices[source].y());
        const auto vectorTo = [&](std::size_t vertex) {
            return Direction{SweepNumber(translatedVertices[vertex].x()) - sourceX,
                             SweepNumber(translatedVertices[vertex].y()) - sourceY};
        };
        const auto cross = [](const Direction& a, const Direction& b) {
            return a.x * b.y - a.y * b.x;
        };
        const auto dot = [](const Direction& a, const Direction& b) {
            return a.x * b.x + a.y * b.y;
        };
        const auto angularHalf = [&](std::size_t vertex) {
            const Direction direction = vectorTo(vertex);
            return direction.y > 0 || (direction.y == 0 && direction.x > 0) ? 0 : 1;
        };
        const auto squaredLength = [&](std::size_t vertex) {
            const Direction direction = vectorTo(vertex);
            return direction.x * direction.x + direction.y * direction.y;
        };

        std::vector<std::size_t> angularOrder;
        angularOrder.reserve(n - 1);
        for (std::size_t vertex = 0; vertex < n; ++vertex) {
            if (vertex != source) {
                angularOrder.push_back(vertex);
            }
        }
        std::sort(angularOrder.begin(), angularOrder.end(),
                  [&](std::size_t a, std::size_t b) {
            const int halfA = angularHalf(a);
            const int halfB = angularHalf(b);
            if (halfA != halfB) {
                return halfA < halfB;
            }
            const auto side = cross(vectorTo(a), vectorTo(b));
            if (side != 0) {
                return side > 0;
            }
            return squaredLength(a) < squaredLength(b);
        });

        // Vertices on the same ray are one event, nearest first. They require
        // no active-edge update between them; instead the local cone at every
        // nearer vertex decides whether visibility continues past it.
        std::vector<std::pair<std::size_t, std::size_t>> groups;
        for (std::size_t begin = 0; begin < angularOrder.size();) {
            std::size_t end = begin + 1;
            const Direction first = vectorTo(angularOrder[begin]);
            while (end < angularOrder.size()) {
                const Direction candidate = vectorTo(angularOrder[end]);
                if (cross(first, candidate) != 0 || dot(first, candidate) <= 0) {
                    break;
                }
                ++end;
            }
            groups.emplace_back(begin, end);
            begin = end;
        }

        // An exact direction strictly inside the CCW gap between two event
        // rays. Their sum lies in gaps shorter than pi; rotating the first ray
        // left lies in a gap of pi or more.
        const auto directionBetween = [&](std::size_t fromGroup, std::size_t toGroup) {
            const Direction from = vectorTo(angularOrder[groups[fromGroup].first]);
            const Direction to = vectorTo(angularOrder[groups[toGroup].first]);
            if (cross(from, to) > 0) {
                return Direction{from.x + to.x, from.y + to.y};
            }
            return Direction{-from.y, from.x};
        };

        Direction sweepDirection = directionBetween(groups.size() - 1, 0);

        // Parameter t of the intersection between source+t*direction and a
        // polygon edge, represented as numerator/positive denominator. No
        // division is performed, keeping integer and rational sweeps exact.
        const auto intersectionParameter =
            [&](std::size_t edge, const Direction& direction) {
            const PointType& a = translatedVertices[edge];
            const PointType& b = translatedVertices[(edge + 1) % n];
            const SweepNumber ax = SweepNumber(a.x()) - sourceX;
            const SweepNumber ay = SweepNumber(a.y()) - sourceY;
            const SweepNumber edgeX = SweepNumber(b.x()) - SweepNumber(a.x());
            const SweepNumber edgeY = SweepNumber(b.y()) - SweepNumber(a.y());
            SweepNumber numerator = ax * edgeY - ay * edgeX;
            SweepNumber denominator = direction.x * edgeY - direction.y * edgeX;
            if (denominator < 0) {
                numerator = -numerator;
                denominator = -denominator;
            }
            return std::pair{numerator, denominator};
        };

        const auto intersectsOpenRay = [&](std::size_t edge, const Direction& direction) {
            if (edge == source || (edge + 1) % n == source) {
                return false;
            }
            const Direction a = vectorTo(edge);
            const Direction b = vectorTo((edge + 1) % n);
            const auto sideA = cross(direction, a);
            const auto sideB = cross(direction, b);
            if (!((sideA < 0 && sideB > 0) || (sideA > 0 && sideB < 0))) {
                return false;
            }
            const auto [numerator, denominator] = intersectionParameter(edge, direction);
            return numerator > 0 && denominator > 0;
        };

        struct EdgeOrder {
            const decltype(intersectionParameter)* parameter;
            const Direction* direction;

            bool operator()(std::size_t a, std::size_t b) const {
                if (a == b) {
                    return false;
                }
                const auto [aNumerator, aDenominator] = (*parameter)(a, *direction);
                const auto [bNumerator, bDenominator] = (*parameter)(b, *direction);
                const auto left = WideNumber(aNumerator) * WideNumber(bDenominator);
                const auto right = WideNumber(bNumerator) * WideNumber(aDenominator);
                return left < right || (left == right && a < b);
            }
        };

        using ActiveSet = std::set<std::size_t, EdgeOrder>;
        ActiveSet active(EdgeOrder{&intersectionParameter, &sweepDirection});
        std::vector<std::optional<typename ActiveSet::iterator>> position(n);
        for (std::size_t edge = 0; edge < n; ++edge) {
            if (intersectsOpenRay(edge, sweepDirection)) {
                position[edge] = active.insert(edge).first;
            }
        }

        for (std::size_t group = 0; group < groups.size(); ++group) {
            const auto [begin, end] = groups[group];

            // Edges ending at this event do not block the event ray itself.
            // Remove by iterator so the distance comparator need not evaluate
            // an edge exactly at its endpoint.
            for (std::size_t at = begin; at < end; ++at) {
                const std::size_t vertex = angularOrder[at];
                for (const std::size_t edge : {vertex, (vertex + n - 1) % n}) {
                    if (position[edge]) {
                        active.erase(*position[edge]);
                        position[edge].reset();
                    }
                }
            }

            bool passesPreviousVertices = true;
            for (std::size_t at = begin; at < end; ++at) {
                const std::size_t target = angularOrder[at];
                bool blocked = false;
                if (!active.empty()) {
                    // With the target itself as direction, t<1 means the first
                    // active edge crosses the open source-target segment.
                    const Direction targetDirection = vectorTo(target);
                    const auto [numerator, denominator] =
                        intersectionParameter(*active.begin(), targetDirection);
                    blocked = numerator > 0 && numerator < denominator;
                }
                const bool boundaryNeighbors =
                    (source + 1) % n == target || (target + 1) % n == source;
                if (source < target &&
                    (boundaryNeighbors ||
                     (passesPreviousVertices && !blocked &&
                      entersClosed[source * n + target] &&
                      entersClosed[target * n + source]))) {
                    result.addEdge(translatedVertices[source], translatedVertices[target]);
                }

                if (at + 1 < end) {
                    // This vertex is strictly inside every later segment on the
                    // ray. Visibility continues only if the polygon is locally
                    // entered in both the backward and forward directions.
                    passesPreviousVertices =
                        passesPreviousVertices && entersClosed[target * n + source] &&
                        entersClosed[target * n + angularOrder[at + 1]];
                }
            }

            // Reorder at a representative direction after the event, then add
            // exactly the incident edges that cross that open ray. Existing
            // edges keep their order because polygon edges cannot cross.
            sweepDirection = directionBetween(group, (group + 1) % groups.size());
            for (std::size_t at = begin; at < end; ++at) {
                const std::size_t vertex = angularOrder[at];
                for (const std::size_t edge : {vertex, (vertex + n - 1) % n}) {
                    if (!position[edge] && intersectsOpenRay(edge, sweepDirection)) {
                        position[edge] = active.insert(edge).first;
                    }
                }
            }
        }
    }

    return result;
}

}  // namespace pgl
