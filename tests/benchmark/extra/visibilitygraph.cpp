// @desc: Visibility graphs of a spiral corridor (sparse sight) and of random
// simple polygons (dense sight), comparing triangular expansion against the
// angular sweep it replaced, plus the clear and reduced variants.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
#include "pgl.hpp"
#include "../plf_nanotimer.h"

// The angular sweep that Polygon::visibilityGraph used before triangular
// expansion, kept here verbatim as the baseline: one counterclockwise sweep per
// vertex over a distance-ordered set of the polygon edges crossing the ray.
// O(n^2 log n) whatever the polygon looks like.
namespace pgl {
template <class PointType>
static pgl::Graph<PointType> sweepVisibilityGraph(const pgl::Polygon<PointType>& polygon) {
    using NumberType = typename PointType::NumberType;
    using SweepNumber = detail::promoted_number_t<NumberType>;
    using WideNumber = detail::promoted_number_t<SweepNumber>;

    struct Direction {
        SweepNumber x{};
        SweepNumber y{};
    };

    Graph<PointType> result;
    const auto translatedVertices = polygon.vertices();
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
    if (n < 3 || polygon.isDegenerate() || polygon.isConvex()) {
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
        const PointType& sourcePoint = translatedVertices[source];
        const SweepNumber sourceX(sourcePoint.x());
        const SweepNumber sourceY(sourcePoint.y());
        const auto vectorTo = [&](std::size_t vertex) {
            return Direction{SweepNumber(translatedVertices[vertex].x()) - sourceX,
                             SweepNumber(translatedVertices[vertex].y()) - sourceY};
        };
        // Only for the *synthesized* sweep directions, which are already in the
        // promoted sweep type: handing those to pgl::crossSign would promote a
        // second time, and an int64_t sweep would land in BigInt. Directions
        // that are differences of two polygon vertices go through the shared
        // predicates on those vertices instead, which promotes exactly once.
        const auto cross = [](const Direction& a, const Direction& b) {
            return a.x * b.y - a.y * b.x;
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
            const auto side = orientationSign(sourcePoint, translatedVertices[a],
                                              translatedVertices[b]);
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
            const PointType& first = translatedVertices[angularOrder[begin]];
            while (end < angularOrder.size()) {
                const PointType& candidate = translatedVertices[angularOrder[end]];
                if (orientationSign(sourcePoint, first, candidate) != 0 ||
                    dotSign(sourcePoint, first, sourcePoint, candidate) <= 0) {
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
            const std::size_t fromVertex = angularOrder[groups[fromGroup].first];
            const std::size_t toVertex = angularOrder[groups[toGroup].first];
            const Direction from = vectorTo(fromVertex);
            const Direction to = vectorTo(toVertex);
            if (orientationSign(sourcePoint, translatedVertices[fromVertex],
                                translatedVertices[toVertex]) > 0) {
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

// Deterministic, portable generator: std::uniform_int_distribution is not
// stable across standard libraries, so we roll our own.
struct Rng {
    std::uint64_t state;
    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }
    int range(int lo, int hi) {
        return lo + static_cast<int>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
};

// A corridor of constant width winding `turns` times around the origin: almost
// every vertex is reflex and each sees only its own stretch of corridor, so the
// visibility graph is sparse and the expansion should stay near linear per
// source. Same construction as the triangulation benchmark.
template <class Number>
static pgl::Polygon<pgl::Point<Number>> makeSpiral(double r0, double k, double width,
                                                   int turns, int stepsPerTurn) {
    const double thetaMax = turns * 2.0 * M_PI;
    const int steps = turns * stepsPerTurn;
    std::vector<pgl::Point<Number>> verts;
    auto add = [&](double t, double extra) {
        const double r = r0 + k * t + extra;
        const int x = static_cast<int>(std::lround(r * std::cos(t)));
        const int y = static_cast<int>(std::lround(r * std::sin(t)));
        if (verts.empty() || verts.back() != pgl::Point<Number>(x, y)) {
            verts.emplace_back(x, y);
        }
    };
    for (int i = 0; i <= steps; ++i) {
        add(thetaMax * i / steps, 0.0);
    }
    for (int i = steps; i >= 0; --i) {
        add(thetaMax * i / steps, width);
    }
    return pgl::Polygon<pgl::Point<Number>>(verts);
}

// A near-convex polygon: a regular n-gon with every `notchEvery`-th vertex
// pulled towards the centre. The few reflex corners keep it off the convex fast
// path while nearly every pair of vertices still sees every other, so the answer
// is quadratic and this is the case where the sweep has least to lose.
template <class Number>
static pgl::Polygon<pgl::Point<Number>> makeNotchedDisk(int n, int notchEvery, double radius) {
    std::vector<pgl::Point<Number>> verts;
    for (int i = 0; i < n; ++i) {
        const double theta = 2.0 * M_PI * i / n;
        const double r = (i % notchEvery == 0) ? radius * 0.90 : radius;
        const int x = static_cast<int>(std::lround(r * std::cos(theta)));
        const int y = static_cast<int>(std::lround(r * std::sin(theta)));
        if (verts.empty() || verts.back() != pgl::Point<Number>(x, y)) {
            verts.emplace_back(x, y);
        }
    }
    return pgl::Polygon<pgl::Point<Number>>(verts);
}

// A random simple polygon by 2-opt uncrossing: a spiky, deeply indented shape
// where each vertex sees only a handful of others, so the answer is sparse and
// the output-sensitive expansion has the most to gain.
template <class Number>
static pgl::Polygon<pgl::Point<Number>> makeRandom(int n, int coord, std::uint64_t seed) {
    using Point = pgl::Point<Number>;
    Rng rng{seed};
    std::vector<Point> verts;
    while (static_cast<int>(verts.size()) < n) {
        Point candidate(Number(rng.range(0, coord)), Number(rng.range(0, coord)));
        if (std::find(verts.begin(), verts.end(), candidate) == verts.end()) {
            verts.push_back(candidate);
        }
    }
    for (int pass = 0; pass < 200; ++pass) {
        bool changed = false;
        for (std::size_t i = 0; i + 1 < verts.size(); ++i) {
            for (std::size_t j = i + 2; j < verts.size(); ++j) {
                if (i == 0 && j + 1 == verts.size()) {
                    continue;
                }
                if (pgl::Segment<Point>(verts[i], verts[i + 1])
                        .crosses(pgl::Segment<Point>(verts[j], verts[(j + 1) % verts.size()]))) {
                    std::reverse(verts.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                                 verts.begin() + static_cast<std::ptrdiff_t>(j) + 1);
                    changed = true;
                }
            }
        }
        if (!changed) {
            break;
        }
    }
    return pgl::Polygon<Point>(verts);
}

template <class Number>
static void run(const char* label, const pgl::Polygon<pgl::Point<Number>>& poly,
                const char* what, bool withSweep) {
    plf::nanotimer timer;

    if (withSweep) {
        timer.start();
        const auto graph = pgl::sweepVisibilityGraph(poly);
        const double elapsed = timer.get_elapsed_ms();
        std::cout << what << " sweep\t\t" << label << "\t\t" << graph.edgeCount() << "\t"
                  << elapsed << std::endl;
    }

    {
        timer.start();
        const auto graph = poly.visibilityGraph();
        const double elapsed = timer.get_elapsed_ms();
        std::cout << what << " expansion\t" << label << "\t\t" << graph.edgeCount() << "\t"
                  << elapsed << std::endl;
    }

    {
        timer.start();
        const auto graph = poly.clearVisibilityGraph();
        const double elapsed = timer.get_elapsed_ms();
        std::cout << what << " clear\t\t" << label << "\t\t" << graph.edgeCount() << "\t"
                  << elapsed << std::endl;
    }

    {
        timer.start();
        const auto graph = poly.reducedVisibilityGraph();
        const double elapsed = timer.get_elapsed_ms();
        std::cout << what << " reduced\t" << label << "\t\t" << graph.edgeCount() << "\t"
                  << elapsed << std::endl;
    }
}

// Per-query visibility from a point, which is what makes reducedVisibilityGraph
// usable: a route needs its endpoints joined to what they see, and paying for the
// whole visibility graph to learn that would defeat the reduction.
template <class Number>
static void runQueries(const char* label, const pgl::Polygon<pgl::Point<Number>>& poly,
                       const char* what) {
    using Point = pgl::Point<Number>;
    plf::nanotimer timer;
    const auto mesh = poly.triangulation();

    // Query points on the vertices themselves, which every routing endpoint that
    // is a corner of the domain reduces to.
    const auto corners = poly.vertices();
    const std::size_t queries = corners.size();

    {
        timer.start();
        std::size_t seen = 0;
        for (const Point& q : corners) {
            seen += mesh.visibleVertices(q).size();
        }
        std::cout << what << " visibleVertices\t" << label << "\t\t" << seen << "\t"
                  << timer.get_elapsed_ms() / static_cast<double>(queries) << std::endl;
    }

    {
        timer.start();
        std::size_t seen = 0;
        for (const Point& q : corners) {
            seen += mesh.template regularizedVisiblePolygon<double>(q).size();
        }
        std::cout << what << " visiblePolygon\t" << label << "\t\t" << seen << "\t"
                  << timer.get_elapsed_ms() / static_cast<double>(queries) << std::endl;
    }
}

int main() {
    std::cout << "Operation\t\tNumber\t\tResult\tTime(ms)" << std::endl;

    const auto spiral = makeSpiral<int>(50.0, 75.0, 220.0, 10, 100);
    run<int>("int", spiral, "spiral", true);

    const auto random400 = makeRandom<int>(400, 100000, 987654321ULL);
    run<int>("int", random400, "random400", true);

    const auto random1500 = makeRandom<int>(1500, 100000, 13579ULL);
    run<int>("int", random1500, "random1500", true);

    const auto disk = makeNotchedDisk<int>(1500, 25, 500000.0);
    run<int>("int", disk, "notched1500", true);

    // Averaged over one query per vertex, so these are directly comparable with
    // the whole-graph timings above.
    runQueries<int>("int", spiral, "spiral");
    runQueries<int>("int", random1500, "random1500");
    runQueries<int>("int", disk, "notched1500");

    // Exact rationals: the expansion never constructs a coordinate, so the
    // arithmetic stays at the width of the input.
    const auto spiralExact = makeSpiral<pgl::ERational>(50.0, 75.0, 220.0, 6, 100);
    run<pgl::ERational>("ERational", spiralExact, "spiral", true);

    return 0;
}
