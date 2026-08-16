#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "pgl.hpp"

namespace {

using Number = pgl::ERational;
using Point = pgl::EPoint;
using Segment = pgl::ESegment;
using Line = pgl::ELine;
using OrientedLine = pgl::EOrientedLine;
using Ray = pgl::ERay;
// Named PolygonShape, not Polygon: under MSVC, <windows.h> (pulled in
// transitively by doctest.h) injects a Win32 GDI function called `Polygon`
// into the global namespace, and an alias of the same name used from
// TEST_CASE bodies (global scope) resolves ambiguously against it.
using PolygonShape = pgl::EPolygon;
using Region = pgl::EPolygonWithHoles;
using Arrangement = pgl::Arrangement<Point>;
using VertexId = Arrangement::VertexId;
using HalfedgeId = Arrangement::HalfedgeId;
using FaceId = Arrangement::FaceId;

// The vertex type defaults to the exact one, since the crossings it has to hold
// are rational whatever the input coordinates are.
static_assert(std::is_same_v<pgl::Arrangement<>, Arrangement>);

Point P(int x, int y) {
    return Point(Number(x), Number(y));
}

Segment S(int ax, int ay, int bx, int by) {
    return Segment(P(ax, ay), P(bx, by));
}

bool hasInfinity(const Arrangement& arr) {
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        if (arr.source(HalfedgeId(i)).index() >= arr.vertexCount()) {
            return true;
        }
    }
    return false;
}

std::size_t topologicalVertexCount(const Arrangement& arr) {
    return arr.vertexCount() + (hasInfinity(arr) ? 1 : 0);
}

// Number of connected components of the arrangement seen as a graph, isolated
// vertices included. This is the C of Euler's formula V - E + F = 1 + C.
std::size_t componentCount(const Arrangement& arr) {
    std::vector<std::size_t> parent(topologicalVertexCount(arr));
    for (std::size_t i = 0; i < parent.size(); ++i) {
        parent[i] = i;
    }
    const auto root = [&parent](std::size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        parent[root(arr.source(h).index())] = root(arr.target(h).index());
    }
    std::set<std::size_t> roots;
    for (std::size_t i = 0; i < parent.size(); ++i) {
        roots.insert(root(i));
    }
    return roots.size();
}

// Everything that must hold of any arrangement whatsoever. Every case below
// runs it, so a structural mistake surfaces on the smallest input that has it.
void checkInvariants(const Arrangement& arr) {
    REQUIRE(arr.halfedgeCount() % 2 == 0);
    REQUIRE(arr.faceCount() >= 1);
    CHECK(arr.isUnbounded(FaceId(0)));

    bool hasUnboundedHalfedge = false;

    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        CHECK(arr.twin(arr.twin(h)) == h);
        CHECK(arr.twin(h) != h);
        CHECK(arr.face(arr.next(h)) == arr.face(h));
        CHECK(arr.source(arr.next(h)) == arr.target(h));
        const bool adjacentToInfinity =
            arr.isFictitious(arr.source(h)) || arr.isFictitious(arr.target(h));
        CHECK(arr.isUnbounded(h) == adjacentToInfinity);
        CHECK(arr.isUnbounded(h) == arr.isUnbounded(arr.twin(h)));
        hasUnboundedHalfedge = hasUnboundedHalfedge || arr.isUnbounded(h);
        const Arrangement::HalfedgeType geometry = arr[h];
        const Arrangement::HalfedgeType twinGeometry = arr[arr.twin(h)];
        if (const auto* segment =
                std::get_if<Arrangement::OrientedSegmentType>(&geometry)) {
            const auto& twinSegment =
                std::get<Arrangement::OrientedSegmentType>(twinGeometry);
            CHECK(segment->source() == arr[arr.source(h)]);
            CHECK(segment->target() == arr[arr.target(h)]);
            CHECK(segment->source() == twinSegment.target());
            CHECK(segment->target() == twinSegment.source());
        } else if (const auto* line =
                       std::get_if<Arrangement::OrientedLineType>(&geometry)) {
            const auto& twinLine = std::get<Arrangement::OrientedLineType>(twinGeometry);
            CHECK(arr.isFictitious(arr.source(h)));
            CHECK(arr.isFictitious(arr.target(h)));
            CHECK(line->source() == twinLine.target());
            CHECK(line->target() == twinLine.source());
        } else {
            const auto& ray = std::get<Arrangement::RayType>(geometry);
            CHECK(ray == std::get<Arrangement::RayType>(twinGeometry));
            CHECK(arr.isFictitious(arr.source(h)) != arr.isFictitious(arr.target(h)));
        }
        CHECK_FALSE(arr.originsOf(h).empty());
    }
    CHECK(arr.isUnbounded() == hasUnboundedHalfedge);

    // Every halfedge leaving a vertex says so, and the rotational fan reaches
    // all of them.
    std::map<VertexId, std::size_t> degree;
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        ++degree[arr.source(h)];
    }
    for (std::uint32_t i = 0; i < topologicalVertexCount(arr); ++i) {
        const VertexId v(i);
        const HalfedgeId start = arr.outgoing(v);
        const std::vector<HalfedgeId> outgoing = arr.outgoingHalfedges(v);
        if (!start.valid()) {
            CHECK(degree[v] == 0);
            CHECK(outgoing.empty());
            CHECK(arr.degree(v) == 0);
            CHECK(arr.originsOf(v).empty());
            continue;
        }
        REQUIRE_FALSE(outgoing.empty());
        CHECK(outgoing.front() == start);
        std::size_t seen = 0;
        HalfedgeId h = start;
        do {
            REQUIRE(seen < outgoing.size());
            CHECK(outgoing[seen] == h);
            CHECK(arr.source(h) == v);
            h = arr.next(arr.twin(h));
            ++seen;
            REQUIRE(seen <= arr.halfedgeCount());
        } while (h != start);
        CHECK(seen == degree[v]);
        CHECK(outgoing.size() == degree[v]);
        CHECK(arr.degree(v) == degree[v]);

        // The origins of a vertex are the origins of its incident edges, sorted
        // and without repetition.
        std::set<std::uint32_t> incidentOrigins;
        for (const HalfedgeId outgoingHalfedge : outgoing) {
            const auto edgeOrigins = arr.originsOf(outgoingHalfedge);
            incidentOrigins.insert(edgeOrigins.begin(), edgeOrigins.end());
        }
        CHECK(arr.originsOf(v) ==
              std::vector<std::uint32_t>(incidentOrigins.begin(), incidentOrigins.end()));
    }

    // The next cycles partition the halfedges, and each is listed exactly once
    // as the outer or an inner cycle of the face it bounds.
    std::set<std::uint32_t> visited;
    std::size_t cycles = 0;
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        if (visited.count(h.index()) != 0) {
            continue;
        }
        ++cycles;
        HalfedgeId walk = h;
        do {
            CHECK(visited.insert(walk.index()).second);
            walk = arr.next(walk);
        } while (walk != h);
    }
    CHECK(visited.size() == arr.halfedgeCount());

    std::size_t listed = 0;
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const FaceId f(i);
        const HalfedgeId outer = arr.outerCycle(f);
        CHECK(outer.valid() == !arr.isUnbounded(f));
        if (outer.valid()) {
            CHECK(arr.face(outer) == f);
            ++listed;
        }
        for (HalfedgeId inner : arr.innerCycles(f)) {
            CHECK(arr.face(inner) == f);
            ++listed;
        }
    }
    CHECK(listed == cycles);

    // The edge list is one variant per twin pair; boundedEdges is its segment
    // projection in the same order.
    const std::vector<Arrangement::EdgeType> edges = arr.edges();
    const std::vector<Segment> boundedEdges = arr.boundedEdges();
    REQUIRE(edges.size() == arr.edgeCount());
    std::size_t bounded = 0;
    for (std::uint32_t i = 0; i < arr.edgeCount(); ++i) {
        const Arrangement::HalfedgeType halfedge = arr[HalfedgeId(2 * i)];
        if (const auto* segment =
                std::get_if<Arrangement::OrientedSegmentType>(&halfedge)) {
            const Segment expected(segment->source(), segment->target());
            CHECK(std::get<Segment>(edges[i]) == expected);
            REQUIRE(bounded < boundedEdges.size());
            CHECK(boundedEdges[bounded++] == expected);
        } else if (const auto* line =
                       std::get_if<Arrangement::OrientedLineType>(&halfedge)) {
            CHECK(std::get<Line>(edges[i]) == Line(line->source(), line->target()));
        } else {
            CHECK(std::get<Ray>(edges[i]) == std::get<Ray>(halfedge));
        }
    }
    CHECK(bounded == boundedEdges.size());

    CHECK(arr.vertices().size() == arr.vertexCount());
    if (hasInfinity(arr)) {
        CHECK(arr.isFictitious(VertexId(static_cast<std::uint32_t>(arr.vertexCount()))));
    }

    // Euler's formula, over the whole complex.
    const std::size_t v = topologicalVertexCount(arr);
    const std::size_t e = arr.edgeCount();
    const std::size_t f = arr.faceCount();
    CHECK(v + f == e + 1 + componentCount(arr));

    // The graph view holds every vertex, the fictitious one included, and joins
    // them exactly as the halfedges do. It is simple, so its edge count only
    // matches the arrangement's when no line closes a loop at infinity and no
    // two edges share both of their endpoints.
    const pgl::Graph<VertexId> graph = arr.asGraph();
    CHECK(graph.vertexCount() == static_cast<int>(v));
    CHECK(graph.components().size() == componentCount(arr));
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        CHECK(graph.containsEdge(arr.source(h), arr.target(h)) ==
              (arr.source(h) != arr.target(h)));
    }
    for (std::uint32_t i = 0; i < v; ++i) {
        const VertexId vertex(i);
        REQUIRE(graph.containsVertex(vertex));
        CHECK(graph.degree(vertex) <= static_cast<int>(arr.degree(vertex)));
    }

    // Each bounded face's witness is where it says it is.
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const FaceId face(i);
        if (arr.isUnbounded(face)) {
            continue;
        }
        const Point witness = arr.witness(face);
        CHECK(arr.locateFace(witness) == face);
        CHECK(arr.polygonWithHoles(face).contains(witness));
    }
}

Arrangement arrangementOf(const std::vector<Segment>& segments) {
    Arrangement arr(segments);
    checkInvariants(arr);
    return arr;
}

// The bounded faces as regions, in a canonical order, so a case can state the
// whole subdivision it expects.
std::vector<Region> boundedFaces(const Arrangement& arr) {
    std::vector<Region> regions;
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const FaceId f(i);
        if (!arr.isUnbounded(f)) {
            regions.push_back(arr.polygonWithHoles(f));
        }
    }
    std::sort(regions.begin(), regions.end());
    return regions;
}

Region regionOf(const std::vector<Point>& ring) {
    return Region(PolygonShape(ring));
}

// The public face boundary is a flat sequence, but its cycles remain evident:
// each supplied start begins a consecutive `next` walk in the returned order.
void checkBoundaryOf(const Arrangement& arr, FaceId face,
                     const std::vector<HalfedgeId>& starts) {
    std::vector<HalfedgeId> expected;
    for (HalfedgeId start : starts) {
        HalfedgeId h = start;
        do {
            expected.push_back(h);
            h = arr.next(h);
        } while (h != start);
    }
    CHECK(arr.boundaryOf(face) == expected);
}

std::vector<HalfedgeId> cycleFrom(const Arrangement& arr, HalfedgeId start) {
    std::vector<HalfedgeId> cycle;
    HalfedgeId h = start;
    do {
        cycle.push_back(h);
        h = arr.next(h);
    } while (h != start);
    return cycle;
}

// The four sides of an axis-parallel square, appended to `out`.
void square(std::vector<Segment>& out, int left, int low, int right, int high) {
    out.push_back(S(left, low, right, low));
    out.push_back(S(right, low, right, high));
    out.push_back(S(right, high, left, high));
    out.push_back(S(left, high, left, low));
}

// One big square holding a `cells` x `cells` grid of squares, each of which holds
// a smaller square of its own. Every ring is a connected component of its own, so
// the number of rings is the number of questions the face nesting has to ask, and
// the answers are nested two deep — a ring's place is the face that really holds
// it, not the outermost one that contains it.
std::vector<Segment> nestedCells(int cells) {
    std::vector<Segment> segments;
    square(segments, 0, 0, 10 * cells, 10 * cells);
    for (int i = 0; i < cells; ++i) {
        for (int j = 0; j < cells; ++j) {
            square(segments, 10 * i + 1, 10 * j + 1, 10 * i + 7, 10 * j + 7);
            square(segments, 10 * i + 3, 10 * j + 3, 10 * i + 5, 10 * j + 5);
        }
    }
    return segments;
}

// Everything the arrangement of nestedCells(cells) must look like.
void checkNestedCells(const Arrangement& arr, int cells) {
    const std::size_t rings = 1 + 2 * static_cast<std::size_t>(cells) * cells;
    CHECK(arr.vertexCount() == 4 * rings);
    CHECK(arr.edgeCount() == 4 * rings);
    REQUIRE(arr.faceCount() == 1 + rings);

    // The unbounded face is held off by the big square alone, and the big square
    // holds one ring per cell — not the inner rings, which are two deep.
    CHECK(arr.innerCycles(FaceId(0)).size() == 1);
    CHECK(arr.innerCycles(arr.locateFace(P(0, 0) + Point(Number(1, 2), Number(1, 2)))).size() ==
          static_cast<std::size_t>(cells) * cells);
    for (int i = 0; i < cells; ++i) {
        for (int j = 0; j < cells; ++j) {
            const FaceId ring = arr.locateFace(P(10 * i + 2, 10 * j + 2));
            const FaceId inside = arr.locateFace(P(10 * i + 4, 10 * j + 4));
            CHECK(ring != inside);
            CHECK(arr.innerCycles(ring).size() == 1);
            CHECK(arr.innerCycles(inside).empty());
        }
    }
}

}  // namespace

TEST_CASE("empty arrangement has only the unbounded face") {
    const Arrangement empty;
    checkInvariants(empty);
    CHECK(empty.vertexCount() == 0);
    CHECK(empty.halfedgeCount() == 0);
    CHECK(empty.faceCount() == 1);
    CHECK_FALSE(empty.isUnbounded());
    CHECK(empty.locateFace(P(3, 7)) == FaceId(0));
    CHECK(empty.innerCycles(FaceId(0)).empty());
    CHECK(empty.boundaryOf(FaceId(0)).empty());
    CHECK(empty.outerBoundaryOf(FaceId(0)).empty());
    CHECK(empty.innerBoundariesOf(FaceId(0)).empty());
    CHECK_THROWS_AS(static_cast<void>(empty.polygonWithHoles(FaceId(0))), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(empty.polygonWithHoles(FaceId())), std::logic_error);

    const Arrangement fromNothing{std::vector<Segment>{}};
    checkInvariants(fromNothing);
    CHECK(fromNothing.faceCount() == 1);
}

TEST_CASE("boundaryOf visits outer and inner face cycles in order") {
    SUBCASE("the unbounded face lists its inner cycle") {
        const Arrangement arr = arrangementOf(
            {S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4), S(0, 4, 0, 0)});
        const FaceId unbounded(0);
        REQUIRE(arr.innerCycles(unbounded).size() == 1);
        checkBoundaryOf(arr, unbounded, {arr.innerCycles(unbounded).front()});
        CHECK(arr.outerBoundaryOf(unbounded).empty());
        CHECK(arr.innerBoundariesOf(unbounded) ==
              std::vector<std::vector<HalfedgeId>>{
                  cycleFrom(arr, arr.innerCycles(unbounded).front())});
    }

    SUBCASE("a bounded face without holes lists its outer cycle") {
        const Arrangement arr = arrangementOf(
            {S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4), S(0, 4, 0, 0)});
        const FaceId inside = arr.locateFace(P(1, 1));
        REQUIRE(arr.outerCycle(inside).valid());
        CHECK(arr.innerCycles(inside).empty());
        checkBoundaryOf(arr, inside, {arr.outerCycle(inside)});
        CHECK(arr.outerBoundaryOf(inside) == cycleFrom(arr, arr.outerCycle(inside)));
        CHECK(arr.innerBoundariesOf(inside).empty());
    }

    SUBCASE("a bounded face with a hole lists its outer cycle then its inner cycle") {
        const Arrangement arr = arrangementOf({S(0, 0, 6, 0), S(6, 0, 6, 6), S(6, 6, 0, 6),
                                               S(0, 6, 0, 0), S(2, 2, 4, 2), S(4, 2, 4, 4),
                                               S(4, 4, 2, 4), S(2, 4, 2, 2)});
        const FaceId ring = arr.locateFace(P(1, 1));
        REQUIRE(arr.outerCycle(ring).valid());
        REQUIRE(arr.innerCycles(ring).size() == 1);
        checkBoundaryOf(arr, ring, {arr.outerCycle(ring), arr.innerCycles(ring).front()});
        CHECK(arr.outerBoundaryOf(ring) == cycleFrom(arr, arr.outerCycle(ring)));
        CHECK(arr.innerBoundariesOf(ring) ==
              std::vector<std::vector<HalfedgeId>>{
                  cycleFrom(arr, arr.innerCycles(ring).front())});
    }
}

TEST_CASE("halfplaneIntersection extracts the outer face constraints") {
    SUBCASE("a bounded face keeps its outer boundary and ignores its hole") {
        const Arrangement arr = arrangementOf({S(0, 0, 6, 0), S(6, 0, 6, 6),
                                               S(6, 6, 0, 6), S(0, 6, 0, 0),
                                               S(2, 2, 4, 2), S(4, 2, 4, 4),
                                               S(4, 4, 2, 4), S(2, 4, 2, 2)});
        const FaceId ring = arr.locateFace(P(1, 1));
        const pgl::HalfplaneIntersection<Point> expected(
            pgl::Rectangle<Point>(P(0, 0), P(6, 6)));
        CHECK(arr.halfplaneIntersection(ring) == expected);
        CHECK_THROWS_AS(static_cast<void>(arr.halfplaneIntersection(FaceId())),
                        std::logic_error);
    }

    SUBCASE("an unbounded face ignores bounded holes") {
        const Arrangement empty;
        CHECK(empty.halfplaneIntersection(FaceId(0)).isPlane());

        const Arrangement square = arrangementOf(
            {S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4), S(0, 4, 0, 0)});
        CHECK(square.halfplaneIntersection(FaceId(0)).isPlane());

        const Arrangement slit(std::vector<Ray>{Ray(P(0, 0), P(1, 0))});
        CHECK(slit.halfplaneIntersection(FaceId(0)).isPlane());
    }

    SUBCASE("the two faces of a line become opposite half-planes") {
        const Arrangement arr(std::vector<Line>{Line(P(-1, 0), P(1, 0))});
        const auto upper = arr.halfplaneIntersection(arr.locateFace(P(0, 1)));
        const auto lower = arr.halfplaneIntersection(arr.locateFace(P(0, -1)));
        REQUIRE(upper.isHalfplane());
        REQUIRE(lower.isHalfplane());
        CHECK(upper.contains(P(0, 1)));
        CHECK_FALSE(upper.contains(P(0, -1)));
        CHECK(lower.contains(P(0, -1)));
        CHECK_FALSE(lower.contains(P(0, 1)));
    }

    SUBCASE("an unbounded wedge keeps every boundary through infinity") {
        const Arrangement arr(std::vector<Line>{Line(P(-1, 0), P(1, 0)),
                                                Line(P(0, -1), P(0, 1))});
        const auto upperRight = arr.halfplaneIntersection(arr.locateFace(P(1, 1)));
        REQUIRE(upperRight.size() == 2);
        CHECK(upperRight.contains(P(1, 1)));
        CHECK_FALSE(upperRight.contains(P(-1, 1)));
        CHECK_FALSE(upperRight.contains(P(1, -1)));
    }

    SUBCASE("the result coordinate type can be integral") {
        using IntegerPoint = pgl::Point<int>;
        using IntegerSegment = pgl::Segment<IntegerPoint>;
        const Arrangement arr(std::vector<IntegerSegment>{
            IntegerSegment(IntegerPoint(0, 0), IntegerPoint(4, 0)),
            IntegerSegment(IntegerPoint(4, 0), IntegerPoint(4, 4)),
            IntegerSegment(IntegerPoint(4, 4), IntegerPoint(0, 4)),
            IntegerSegment(IntegerPoint(0, 4), IntegerPoint(0, 0))});
        const auto region = arr.halfplaneIntersection<int>(arr.locateFace(P(1, 1)));
        static_assert(std::is_same_v<decltype(region),
                                     const pgl::HalfplaneIntersection<IntegerPoint>>);
        CHECK(region == pgl::HalfplaneIntersection<IntegerPoint>(
                            pgl::Rectangle<IntegerPoint>(IntegerPoint(0, 0),
                                                         IntegerPoint(4, 4))));
    }
}

TEST_CASE("a single segment is two vertices, one edge and no bounded face") {
    const Arrangement arr = arrangementOf({S(0, 0, 4, 2)});
    CHECK(arr.vertexCount() == 2);
    CHECK(arr.edgeCount() == 1);
    CHECK(arr.faceCount() == 1);
    CHECK_FALSE(arr.isUnbounded());
    CHECK_FALSE(arr.isUnbounded(HalfedgeId(0)));
    CHECK_FALSE(arr.isUnbounded(HalfedgeId(1)));
    // The two halfedges form a single cycle, an inner cycle of the outer face.
    CHECK(arr.innerCycles(FaceId(0)).size() == 1);
    CHECK(arr.locateFace(P(2, 1)) == FaceId(0));

    const HalfedgeId h = arr.outgoing(VertexId(0));
    CHECK(arr.witness(h) == P(2, 1));
    const auto segment = std::get<Arrangement::OrientedSegmentType>(arr[h]);
    CHECK(segment.source() == P(0, 0));
    CHECK(segment.target() == P(4, 2));
}

TEST_CASE("crossing segments are split at their crossing") {
    const Arrangement arr = arrangementOf({S(-2, 0, 2, 0), S(0, -2, 0, 2)});
    CHECK(arr.vertexCount() == 5);
    CHECK(arr.edgeCount() == 4);
    CHECK(arr.faceCount() == 1);
    // The crossing is the only vertex of degree four.
    std::size_t crossings = 0;
    for (std::uint32_t i = 0; i < arr.vertexCount(); ++i) {
        const VertexId v(i);
        std::size_t degree = 0;
        HalfedgeId h = arr.outgoing(v);
        const HalfedgeId start = h;
        do {
            ++degree;
            h = arr.next(arr.twin(h));
        } while (h != start);
        if (degree == 4) {
            ++crossings;
            CHECK(arr[v] == P(0, 0));
        }
    }
    CHECK(crossings == 1);
}

TEST_CASE("a square encloses one bounded face") {
    const Arrangement arr =
        arrangementOf({S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4), S(0, 4, 0, 0)});
    CHECK(arr.vertexCount() == 4);
    CHECK(arr.edgeCount() == 4);
    REQUIRE(arr.faceCount() == 2);
    CHECK(arr.polygonWithHoles(FaceId(1)) == regionOf({P(0, 0), P(4, 0), P(4, 4), P(0, 4)}));
    CHECK(arr.locateFace(P(1, 1)) == FaceId(1));
    CHECK(arr.locateFace(P(5, 1)) == FaceId(0));
    CHECK(arr.locateFace(P(-1, 2)) == FaceId(0));

    // Only the unbounded face is unbounded, and it is the one holding the square.
    CHECK(arr.innerCycles(FaceId(0)).size() == 1);
    CHECK(arr.innerCycles(FaceId(1)).empty());
}

TEST_CASE("a diagonal cuts a square into two faces") {
    const Arrangement arr = arrangementOf(
        {S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4), S(0, 4, 0, 0), S(0, 0, 4, 4)});
    REQUIRE(arr.faceCount() == 3);
    const std::vector<Region> faces = boundedFaces(arr);
    std::vector<Region> expected{regionOf({P(0, 0), P(4, 0), P(4, 4)}),
                                 regionOf({P(0, 0), P(4, 4), P(0, 4)})};
    std::sort(expected.begin(), expected.end());
    CHECK(faces == expected);
}

TEST_CASE("a square inside a square is a face with a hole") {
    const Arrangement arr = arrangementOf({S(0, 0, 6, 0), S(6, 0, 6, 6), S(6, 6, 0, 6),
                                           S(0, 6, 0, 0), S(2, 2, 4, 2), S(4, 2, 4, 4),
                                           S(4, 4, 2, 4), S(2, 4, 2, 2)});
    REQUIRE(arr.faceCount() == 3);

    const FaceId ring = arr.locateFace(P(1, 1));
    const FaceId inside = arr.locateFace(P(3, 3));
    CHECK(ring != inside);
    CHECK_FALSE(arr.isUnbounded(ring));
    CHECK_FALSE(arr.isUnbounded(inside));

    CHECK(arr.innerCycles(ring).size() == 1);
    CHECK(arr.innerCycles(inside).empty());

    const Region holed(PolygonShape({P(0, 0), P(6, 0), P(6, 6), P(0, 6)}),
                        std::vector<PolygonShape>{PolygonShape({P(2, 2), P(4, 2), P(4, 4), P(2, 4)})});
    CHECK(arr.polygonWithHoles(ring) == holed);
    CHECK(arr.polygonWithHoles(inside) == regionOf({P(2, 2), P(4, 2), P(4, 4), P(2, 4)}));
    // The witness of the ring-shaped face must miss the hole.
    CHECK(holed.contains(arr.witness(ring)));
}

TEST_CASE("nested rings are assigned to the face that really holds them") {
    SUBCASE("three concentric squares") {
        std::vector<Segment> squares;
        for (int side : {2, 4, 6}) {
            squares.push_back(S(-side, -side, side, -side));
            squares.push_back(S(side, -side, side, side));
            squares.push_back(S(side, side, -side, side));
            squares.push_back(S(-side, side, -side, -side));
        }
        const Arrangement arr = arrangementOf(squares);
        REQUIRE(arr.faceCount() == 4);
        // Each ring holds exactly the next one in, and the innermost holds none.
        CHECK(arr.innerCycles(FaceId(0)).size() == 1);
        CHECK(arr.innerCycles(arr.locateFace(P(0, 5))).size() == 1);
        CHECK(arr.innerCycles(arr.locateFace(P(0, 3))).size() == 1);
        CHECK(arr.innerCycles(arr.locateFace(P(0, 1))).empty());
        CHECK(arr.polygonWithHoles(arr.locateFace(P(0, 3))).holeCount() == 1);
    }

    SUBCASE("many rings, nested two deep") {
        // Placing a ring is a question about what lies to the left of it, and
        // there are two ways to answer the whole batch: a scan over the edges per
        // question, or one sweep answering all of them. Which one runs is decided
        // on the counts, so the same figure is built at two sizes — three cells
        // wide is answered by the scans, ten cells wide by the sweep — and both
        // have to produce the same nesting.
        checkNestedCells(arrangementOf(nestedCells(3)), 3);
        checkNestedCells(arrangementOf(nestedCells(10)), 10);
    }

    SUBCASE("a hole level with a spike of its neighbour") {
        // The horizontal ray cast to place the small square's ring meets the
        // triangle's apex, where two edges leave upwards at once: the ring
        // belongs to the face beyond the rightmost of them, not to the triangle.
        const Arrangement arr = arrangementOf({S(0, 0, 8, 0), S(8, 0, 8, 8), S(8, 8, 0, 8),
                                               S(0, 8, 0, 0), S(2, 2, 1, 6), S(1, 6, 3, 6),
                                               S(3, 6, 2, 2), S(4, 2, 6, 2), S(6, 2, 6, 4),
                                               S(6, 4, 4, 4), S(4, 4, 4, 2)});
        REQUIRE(arr.faceCount() == 4);
        const FaceId around = arr.locateFace(P(7, 1));
        CHECK(arr.innerCycles(around).size() == 2);
        CHECK(arr.polygonWithHoles(around).holeCount() == 2);
        CHECK(arr.innerCycles(arr.locateFace(P(2, 5))).empty());   // the triangle
        CHECK(arr.innerCycles(arr.locateFace(P(5, 3))).empty());   // the small square
    }
}

TEST_CASE("two triangles meeting at a point stay two faces") {
    // A bowtie: the pinch vertex has degree four, and the rotational order is
    // what keeps the two sides apart.
    const Arrangement arr = arrangementOf({S(-2, -2, -2, 2), S(-2, 2, 0, 0), S(0, 0, -2, -2),
                                           S(2, -2, 2, 2), S(2, 2, 0, 0), S(0, 0, 2, -2)});
    REQUIRE(arr.faceCount() == 3);
    const std::vector<Region> faces = boundedFaces(arr);
    std::vector<Region> expected{regionOf({P(-2, -2), P(0, 0), P(-2, 2)}),
                                 regionOf({P(0, 0), P(2, -2), P(2, 2)})};
    std::sort(expected.begin(), expected.end());
    CHECK(faces == expected);
    // The unbounded face's inner cycle walks the whole bowtie, passing the pinch
    // vertex twice.
    CHECK(arr.innerCycles(FaceId(0)).size() == 1);
}

TEST_CASE("duplicated and overlapping input becomes one edge each") {
    SUBCASE("exact duplicates") {
        const Arrangement arr = arrangementOf({S(0, 0, 4, 0), S(4, 0, 0, 0), S(0, 0, 4, 0)});
        CHECK(arr.vertexCount() == 2);
        CHECK(arr.edgeCount() == 1);
        const HalfedgeId h = arr.outgoing(VertexId(0));
        const std::vector<std::uint32_t> origins(arr.originsOf(h).begin(),
                                                 arr.originsOf(h).end());
        CHECK(origins == std::vector<std::uint32_t>{0, 1, 2});
    }

    SUBCASE("collinear overlap") {
        const Arrangement arr = arrangementOf({S(0, 0, 4, 0), S(2, 0, 6, 0)});
        CHECK(arr.vertexCount() == 4);
        CHECK(arr.edgeCount() == 3);
        // Only the shared stretch has two origins.
        std::map<Segment, std::size_t> origins;
        for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
            const HalfedgeId h(i);
            const auto segment = std::get<Arrangement::OrientedSegmentType>(arr[h]);
            origins[Segment(segment.source(), segment.target())] = arr.originsOf(h).size();
        }
        CHECK(origins.size() == 3);
        CHECK(origins[S(0, 0, 2, 0)] == 1);
        CHECK(origins[S(2, 0, 4, 0)] == 2);
        CHECK(origins[S(4, 0, 6, 0)] == 1);
    }
}

TEST_CASE("a dangling edge does not disturb the face it hangs in") {
    SUBCASE("attached to the boundary") {
        const Arrangement arr = arrangementOf({S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4),
                                               S(0, 4, 0, 0), S(0, 0, 2, 2)});
        CHECK(arr.vertexCount() == 5);
        CHECK(arr.edgeCount() == 5);
        REQUIRE(arr.faceCount() == 2);
        // The face's boundary walks down the spike and back; its polygon is the
        // regularized face, so the spike is gone.
        CHECK(arr.polygonWithHoles(FaceId(1)) == regionOf({P(0, 0), P(4, 0), P(4, 4), P(0, 4)}));
        CHECK(arr.locateFace(P(3, 1)) == FaceId(1));
    }

    SUBCASE("floating inside") {
        const Arrangement arr = arrangementOf({S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4),
                                               S(0, 4, 0, 0), S(1, 2, 3, 2)});
        CHECK(arr.vertexCount() == 6);
        CHECK(arr.edgeCount() == 5);
        REQUIRE(arr.faceCount() == 2);
        // The floating segment is an inner cycle of the square's face.
        CHECK(arr.innerCycles(FaceId(1)).size() == 1);
        CHECK(arr.polygonWithHoles(FaceId(1)) == regionOf({P(0, 0), P(4, 0), P(4, 4), P(0, 4)}));
    }
}

TEST_CASE("isolated points are vertices of the face holding them") {
    std::vector<pgl::EShape> shapes;
    for (const Segment& s : {S(0, 0, 4, 0), S(4, 0, 4, 4), S(4, 4, 0, 4), S(0, 4, 0, 0)}) {
        shapes.emplace_back(s);
    }
    shapes.emplace_back(P(2, 2));  // inside
    shapes.emplace_back(P(9, 9));  // outside
    shapes.emplace_back(P(2, 0));  // on an edge, which it therefore splits

    const Arrangement arr(shapes);
    checkInvariants(arr);
    CHECK(arr.vertexCount() == 7);
    CHECK(arr.edgeCount() == 5);
    REQUIRE(arr.faceCount() == 2);

    std::size_t isolated = 0;
    for (std::uint32_t i = 0; i < arr.vertexCount(); ++i) {
        const VertexId v(i);
        if (!arr.outgoing(v).valid()) {
            ++isolated;
            CHECK((arr[v] == P(2, 2) || arr[v] == P(9, 9)));
        }
    }
    CHECK(isolated == 2);
    CHECK(arr.locateFace(P(2, 2)) == FaceId(1));
    CHECK(arr.locateFace(P(9, 9)) == FaceId(0));
    CHECK(arr.witness(VertexId(0)) == arr[VertexId(0)]);
}

TEST_CASE("many segments through one point") {
    // A star of six chords of a hexagon through the origin: the radial order at
    // the centre has to be exact and total for the six wedges to come out.
    const Arrangement arr = arrangementOf({S(-6, 0, 6, 0), S(-3, -5, 3, 5), S(-3, 5, 3, -5),
                                           S(-6, 0, -3, 5), S(-3, 5, 3, 5), S(3, 5, 6, 0),
                                           S(6, 0, 3, -5), S(3, -5, -3, -5), S(-3, -5, -6, 0)});
    CHECK(arr.vertexCount() == 7);
    CHECK(arr.edgeCount() == 12);
    CHECK(arr.faceCount() == 7);
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const FaceId f(i);
        if (!arr.isUnbounded(f)) {
            CHECK(arr.polygonWithHoles(f).outer().size() == 3);
        }
    }
}

TEST_CASE("vertical and horizontal segments in a grid") {
    std::vector<Segment> grid;
    for (int i = 0; i <= 2; ++i) {
        grid.push_back(S(0, i, 2, i));
        grid.push_back(S(i, 0, i, 2));
    }
    const Arrangement arr = arrangementOf(grid);
    CHECK(arr.vertexCount() == 9);
    CHECK(arr.edgeCount() == 12);
    REQUIRE(arr.faceCount() == 5);
    Number total(0);
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const FaceId f(i);
        if (!arr.isUnbounded(f)) {
            total += arr.polygonWithHoles(f).twiceArea();
        }
    }
    CHECK(total == Number(8));  // twice the area of the 2x2 square
}

TEST_CASE("crossings off the input lattice") {
    // The two diagonals of a unit square meet at (1/2, 1/2), so the arrangement
    // needs a coordinate type the input does not have.
    const Arrangement arr = arrangementOf({S(0, 0, 1, 1), S(0, 1, 1, 0)});
    CHECK(arr.vertexCount() == 5);
    CHECK(arr.edgeCount() == 4);
    bool foundCentre = false;
    for (const Point& vertex : arr.vertices()) {
        if (vertex == Point(Number(1, 2), Number(1, 2))) {
            foundCentre = true;
        }
    }
    CHECK(foundCentre);
}

TEST_CASE("integer-valued rational segment endpoints retain exact crossings") {
    const auto integer = [](std::int64_t numerator, std::int64_t denominator) {
        return Number(pgl::BigInt(numerator), pgl::BigInt(denominator));
    };
    const Point zero(integer(0, 7), integer(0, 11));
    const Point one(integer(13, 13), integer(17, 17));
    const std::vector<Segment> diagonals{
        Segment(zero, one),
        Segment(Point(integer(0, 19), integer(23, 23)),
                Point(integer(29, 29), integer(0, 31)))};

    const Arrangement arr(diagonals);
    CHECK(arr.vertexCount() == 5);
    CHECK(arr.edgeCount() == 4);
    CHECK(std::ranges::find(arr.vertices(), Point(Number(1, 2), Number(1, 2))) !=
          arr.vertices().end());
}

TEST_CASE("integer segment endpoints outside int64 retain exact crossings") {
    const pgl::BigInt beyond =
        pgl::BigInt(std::numeric_limits<std::int64_t>::max()) + pgl::BigInt(1);
    const Point low(Number(beyond), Number(0));
    const Point high(Number(beyond + pgl::BigInt(1)), Number(1));
    const std::vector<Segment> diagonals{
        Segment(low, high),
        Segment(Point(Number(beyond), Number(1)),
                Point(Number(beyond + pgl::BigInt(1)), Number(0)))};

    const Arrangement arr(diagonals);
    CHECK(arr.vertexCount() == 5);
    CHECK(arr.edgeCount() == 4);
    const Point centre(Number(beyond * pgl::BigInt(2) + pgl::BigInt(1), pgl::BigInt(2)),
                       Number(1, 2));
    CHECK(std::ranges::find(arr.vertices(), centre) != arr.vertices().end());
}

TEST_CASE("polygonal input contributes its boundary") {
    SUBCASE("polygons") {
        const std::vector<PolygonShape> polygons{PolygonShape({P(0, 0), P(4, 0), P(4, 4), P(0, 4)}),
                                                  PolygonShape({P(2, 2), P(6, 2), P(6, 6), P(2, 6)})};
        const Arrangement arr(polygons);
        checkInvariants(arr);
        // The two squares overlap in a third one, so three bounded faces.
        CHECK(arr.faceCount() == 4);
        CHECK(arr.polygonWithHoles(arr.locateFace(P(3, 3))) == regionOf({P(2, 2), P(4, 2), P(4, 4), P(2, 4)}));
    }

    SUBCASE("a region, holes included") {
        const Region holed(PolygonShape({P(0, 0), P(6, 0), P(6, 6), P(0, 6)}),
                            std::vector<PolygonShape>{PolygonShape({P(2, 2), P(4, 2), P(4, 4), P(2, 4)})});
        const Arrangement arr(std::vector<Region>{holed});
        checkInvariants(arr);
        REQUIRE(arr.faceCount() == 3);
        CHECK(arr.polygonWithHoles(arr.locateFace(P(1, 1))) == holed);
        CHECK(arr.polygonWithHoles(arr.locateFace(P(3, 3))) == regionOf({P(2, 2), P(4, 2), P(4, 4), P(2, 4)}));
    }

    SUBCASE("triangles and rectangles") {
        std::vector<pgl::EShape> shapes;
        shapes.emplace_back(pgl::ETriangle(P(0, 0), P(4, 0), P(0, 4)));
        shapes.emplace_back(pgl::ERectangle(P(1, 1), P(5, 5)));
        const Arrangement arr(shapes);
        checkInvariants(arr);
        CHECK(arr.faceCount() == 4);
    }

    SUBCASE("an open chain") {
        const pgl::EPolyline chain(std::vector<Point>{P(0, 0), P(4, 0), P(4, 4)});
        const Arrangement arr(std::vector<pgl::EPolyline>{chain});
        checkInvariants(arr);
        CHECK(arr.vertexCount() == 3);
        CHECK(arr.edgeCount() == 2);
        CHECK(arr.faceCount() == 1);
    }
}

TEST_CASE("edges inherit the label of the shape that produced them") {
    using LabeledSegment = pgl::Segment<Point, int>;
    using LabeledArrangement = pgl::Arrangement<Point, int>;
    const std::vector<LabeledSegment> segments{
        LabeledSegment(P(0, 0), P(4, 0), 7), LabeledSegment(P(2, -2), P(2, 2), 9)};

    LabeledArrangement arr(segments);
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const LabeledArrangement::HalfedgeId h(i);
        const std::uint32_t source = arr.originsOf(h).front();
        CHECK(arr.label(h) == (source == 0 ? 7 : 9));
        CHECK(arr.label(h) == std::visit([](const auto& edge) { return edge.label(); }, arr[h]));
    }

    // The edge list carries the same labels, one entry per twin pair.
    const std::vector<LabeledSegment> edges = arr.boundedEdges();
    REQUIRE(edges.size() == arr.edgeCount());
    std::multiset<int> labels;
    for (const LabeledSegment& edge : edges) {
        labels.insert(edge.label());
    }
    CHECK(labels == std::multiset<int>{7, 7, 9, 9});

    const std::vector<LabeledArrangement::EdgeType> allEdges = arr.edges();
    REQUIRE(allEdges.size() == arr.edgeCount());
    std::multiset<int> variantLabels;
    for (const auto& edge : allEdges) {
        variantLabels.insert(
            std::visit([](const auto& geometry) { return geometry.label(); }, edge));
    }
    CHECK(variantLabels == labels);

    // A face's label is the caller's to set.
    const LabeledArrangement::FaceId unbounded(0);
    CHECK(arr.label(unbounded) == 0);
    arr.label(unbounded) = 5;
    CHECK(arr.label(unbounded) == 5);
}

TEST_CASE("integer coordinates suffice when segments meet only at endpoints") {
    using IntPoint = pgl::Point<int>;
    using IntSegment = pgl::Segment<IntPoint>;
    using IntArrangement = pgl::Arrangement<IntPoint>;
    const std::vector<IntSegment> square{
        IntSegment(IntPoint(0, 0), IntPoint(4, 0)), IntSegment(IntPoint(4, 0), IntPoint(4, 4)),
        IntSegment(IntPoint(4, 4), IntPoint(0, 4)), IntSegment(IntPoint(0, 4), IntPoint(0, 0))};
    const IntArrangement arr(square);
    REQUIRE(arr.faceCount() == 2);
    CHECK(arr.locateFace(IntPoint(2, 2)) == IntArrangement::FaceId(1));
    CHECK(arr.locateFace(IntPoint(9, 2)) == IntArrangement::FaceId(0));
    // The witness is asked for in a type that can hold it.
    CHECK(arr.polygonWithHoles<pgl::Rational<int>>(IntArrangement::FaceId(1))
              .contains(arr.witness<pgl::Rational<int>>(IntArrangement::FaceId(1))));
}

TEST_CASE("integer coordinates suffice when orthogonal segments cross") {
    // Unlike the diagonals in "crossings off the input lattice", a horizontal and
    // a vertical segment meet at a point both their coordinates already have, so
    // int coordinates carry the crossing without promotion to a rational type.
    using IntPoint = pgl::Point<int>;
    using IntSegment = pgl::Segment<IntPoint>;
    const std::vector<IntSegment> cross{IntSegment(IntPoint(-2, 0), IntPoint(2, 0)),
                                        IntSegment(IntPoint(0, -2), IntPoint(0, 2))};
    const pgl::Arrangement<IntPoint> arr(cross);
    CHECK(arr.vertexCount() == 5);
    CHECK(arr.edgeCount() == 4);
    CHECK(arr.faceCount() == 1);
    bool foundCentre = false;
    for (const IntPoint& vertex : arr.vertices()) {
        if (vertex == IntPoint(0, 0)) {
            foundCentre = true;
        }
    }
    CHECK(foundCentre);
}

TEST_CASE("handles are distinct, ordered and hashable types") {
    CHECK(sizeof(VertexId) == sizeof(std::uint32_t));
    CHECK_FALSE(std::is_convertible_v<VertexId, FaceId>);
    CHECK_FALSE(std::is_convertible_v<std::uint32_t, VertexId>);
    CHECK_FALSE(std::is_same_v<VertexId, pgl::Arrangement<Point, int>::VertexId>);

    const FaceId invalid;
    CHECK_FALSE(invalid.valid());
    CHECK_FALSE(static_cast<bool>(invalid));
    CHECK(FaceId(3).valid());
    CHECK(FaceId(3).index() == 3);
    CHECK(FaceId(2) < FaceId(3));
    CHECK(FaceId(3) == FaceId(3));
    CHECK(FaceId(3) != invalid);

    const std::set<HalfedgeId> ordered{HalfedgeId(4), HalfedgeId(1)};
    CHECK(ordered.begin()->index() == 1);
    CHECK(std::hash<VertexId>{}(VertexId(6)) ==
          std::hash<VertexId>{}(VertexId(6)));
}

TEST_CASE("a ray ends at the symbolic vertex at infinity") {
    const Arrangement arr(std::vector<Ray>{Ray(P(0, 0), P(1, 0))});
    checkInvariants(arr);
    REQUIRE(arr.vertexCount() == 1);
    REQUIRE(arr.edgeCount() == 1);
    REQUIRE(arr.faceCount() == 1);
    CHECK(arr.isUnbounded());
    CHECK(arr.isUnbounded(HalfedgeId(0)));
    CHECK(arr.isUnbounded(HalfedgeId(1)));

    const VertexId infinity(1);
    CHECK(arr.isFictitious(infinity));
    CHECK_THROWS_AS(static_cast<void>(arr[infinity]), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(arr.witness(infinity)), std::logic_error);
    CHECK(arr.source(HalfedgeId(0)) == VertexId(0));
    CHECK(arr.target(HalfedgeId(0)) == infinity);
    CHECK(std::get<Ray>(arr[HalfedgeId(0)]) == std::get<Ray>(arr[HalfedgeId(1)]));
    CHECK(arr.boundedEdges().empty());
    REQUIRE(arr.edges().size() == 1);
    CHECK(std::holds_alternative<Ray>(arr.edges().front()));
}

TEST_CASE("a line is a loop through infinity with oppositely oriented twins") {
    const Arrangement arr(std::vector<Line>{Line(P(-1, 0), P(1, 0))});
    checkInvariants(arr);
    REQUIRE(arr.vertexCount() == 0);
    REQUIRE(arr.edgeCount() == 1);
    REQUIRE(arr.faceCount() == 2);
    CHECK(arr.isUnbounded());
    CHECK(arr.isUnbounded(HalfedgeId(0)));
    CHECK(arr.isUnbounded(HalfedgeId(1)));
    CHECK(arr.isUnbounded(FaceId(0)));
    CHECK(arr.isUnbounded(FaceId(1)));

    const auto forward = std::get<OrientedLine>(arr[HalfedgeId(0)]);
    const auto backward = std::get<OrientedLine>(arr[HalfedgeId(1)]);
    CHECK(forward.source() == backward.target());
    CHECK(forward.target() == backward.source());
    CHECK(arr.locateFace(P(0, 1)) != arr.locateFace(P(0, -1)));
    CHECK(arr.locateFace(P(0, 0)) == arr.locateFace(P(0, 1)));
    CHECK(arr.boundedEdges().empty());
    REQUIRE(arr.edges().size() == 1);
    CHECK(std::holds_alternative<Line>(arr.edges().front()));
}

TEST_CASE("parallel lines form an ordered fan at infinity") {
    const Arrangement arr(std::vector<Line>{Line(P(0, -1), P(1, -1)),
                                            Line(P(0, 1), P(1, 1))});
    checkInvariants(arr);
    REQUIRE(arr.vertexCount() == 0);
    REQUIRE(arr.edgeCount() == 2);
    REQUIRE(arr.faceCount() == 3);
    const FaceId below = arr.locateFace(P(0, -2));
    const FaceId middle = arr.locateFace(P(0, 0));
    const FaceId above = arr.locateFace(P(0, 2));
    CHECK(below != middle);
    CHECK(middle != above);
    CHECK(below != above);
    CHECK(arr.locateFace(P(0, -1)) == middle);
    CHECK(arr.locateFace(P(0, 1)) == above);
}

TEST_CASE("crossing lines are split into four rays") {
    const Arrangement arr(std::vector<Line>{Line(P(-1, 0), P(1, 0)),
                                            Line(P(0, -1), P(0, 1))});
    checkInvariants(arr);
    CHECK(arr.vertexCount() == 1);
    CHECK(arr.edgeCount() == 4);
    CHECK(arr.faceCount() == 4);
    for (std::uint32_t h = 0; h < arr.halfedgeCount(); ++h) {
        CHECK(std::holds_alternative<Ray>(arr[HalfedgeId(h)]));
    }
    std::set<FaceId> quadrants{arr.locateFace(P(-1, -1)), arr.locateFace(P(-1, 1)),
                               arr.locateFace(P(1, -1)), arr.locateFace(P(1, 1))};
    CHECK(quadrants.size() == 4);
}

TEST_CASE("three lines in general position form seven faces") {
    const Arrangement arr(std::vector<Line>{Line(P(-2, 0), P(2, 0)),
                                            Line(P(0, -2), P(0, 2)),
                                            Line(P(-2, -1), P(2, 3))});
    checkInvariants(arr);
    CHECK(arr.vertexCount() == 3);
    CHECK(arr.edgeCount() == 9);
    CHECK(arr.faceCount() == 7);
}

TEST_CASE("concurrent lines make one vertex of high degree") {
    // Three lines through the origin, and a fourth one that misses it.
    const Arrangement arr(std::vector<Line>{Line(P(-1, 0), P(1, 0)),
                                            Line(P(0, -1), P(0, 1)),
                                            Line(P(-1, -1), P(1, 1)),
                                            Line(P(3, 0), P(0, 3))});
    checkInvariants(arr);
    REQUIRE(arr.vertexCount() == 4);

    std::size_t concurrent = 0;
    for (std::uint32_t i = 0; i < arr.vertexCount(); ++i) {
        const VertexId v(i);
        if (arr[v] != P(0, 0)) {
            // An ordinary crossing of two of the lines.
            CHECK(arr.degree(v) == 4);
            CHECK(arr.originsOf(v).size() == 2);
            continue;
        }
        ++concurrent;
        CHECK(arr.degree(v) == 6);  // Three lines, two halfedges each.
        CHECK(arr.originsOf(v) == std::vector<std::uint32_t>{0, 1, 2});
    }
    CHECK(concurrent == 1);
}

TEST_CASE("overlapping line and ray retain all contributing origins") {
    std::vector<pgl::EShape> shapes{Line(P(-1, 0), P(1, 0)), Ray(P(0, 0), P(1, 0))};
    const Arrangement arr(shapes);
    checkInvariants(arr);
    REQUIRE(arr.edgeCount() == 2);
    std::vector<std::size_t> originCounts;
    for (std::uint32_t edge = 0; edge < arr.edgeCount(); ++edge) {
        originCounts.push_back(arr.originsOf(HalfedgeId(2 * edge)).size());
    }
    std::sort(originCounts.begin(), originCounts.end());
    CHECK(originCounts == std::vector<std::size_t>{1, 2});
}

TEST_CASE("lines rays and bounded edges share exact finite vertices") {
    std::vector<pgl::EShape> shapes{Line(P(-2, 0), P(2, 0)), Ray(P(0, -2), P(0, 1)),
                                    S(-1, -1, 1, 1)};
    const Arrangement arr(shapes);
    checkInvariants(arr);
    CHECK(std::ranges::count(arr.vertices(), P(0, 0)) == 1);
    CHECK(arr.locateFace(P(100, 1)) != arr.locateFace(P(100, -1)));
    CHECK(arr.locateFace(P(-100, 1)) != arr.locateFace(P(-100, -1)));
    CHECK(arr.boundedEdges().size() == 3);
}

TEST_CASE("bounded faces and holes coexist with the infinity fan") {
    SUBCASE("a line outside a square") {
        std::vector<pgl::EShape> shapes{Line(P(-1, -2), P(1, -2)), S(-1, -1, 1, -1),
                                        S(1, -1, 1, 1), S(1, 1, -1, 1), S(-1, 1, -1, -1)};
        const Arrangement arr(shapes);
        checkInvariants(arr);
        REQUIRE(arr.faceCount() == 3);
        std::size_t bounded = 0;
        for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
            bounded += !arr.isUnbounded(FaceId(i));
        }
        CHECK(bounded == 1);
        CHECK(arr.polygonWithHoles(arr.locateFace(P(0, 0))) ==
              regionOf({P(-1, -1), P(1, -1), P(1, 1), P(-1, 1)}));
    }

    SUBCASE("a line cuts a square into two bounded faces") {
        std::vector<pgl::EShape> shapes{Line(P(-2, 0), P(2, 0)), S(-1, -1, 1, -1),
                                        S(1, -1, 1, 1), S(1, 1, -1, 1), S(-1, 1, -1, -1)};
        const Arrangement arr(shapes);
        checkInvariants(arr);
        REQUIRE(arr.faceCount() == 4);
        const Point upper(Number(0), Number(1) / Number(2));
        const Point lower(Number(0), Number(-1) / Number(2));
        CHECK_FALSE(arr.isUnbounded(arr.locateFace(upper)));
        CHECK_FALSE(arr.isUnbounded(arr.locateFace(lower)));
        CHECK(arr.locateFace(upper) != arr.locateFace(lower));
    }
}

TEST_CASE("two rays sharing their finite source bound two unbounded faces") {
    const Arrangement arr(std::vector<Ray>{Ray(P(0, 0), P(1, 1)),
                                           Ray(P(0, 0), P(1, -1))});
    checkInvariants(arr);
    CHECK(arr.vertexCount() == 1);
    CHECK(arr.edgeCount() == 2);
    CHECK(arr.faceCount() == 2);
    CHECK(arr.locateFace(P(2, 0)) != arr.locateFace(P(-2, 0)));
}

TEST_CASE("parallel rays only sharing infinity do not enclose a face") {
    const Arrangement arr(std::vector<Ray>{Ray(P(0, 0), P(1, 0)),
                                           Ray(P(0, 1), P(1, 1))});
    checkInvariants(arr);
    CHECK(arr.vertexCount() == 2);
    CHECK(arr.edgeCount() == 2);
    CHECK(arr.faceCount() == 1);
}

TEST_CASE("arrangement intersection traversal is ordered and suppresses incident edges") {
    using IntersectionId = Arrangement::IntersectionId;
    const std::vector<Segment> shapes{S(0, 0, 10, 0), S(3, -2, 3, 2),
                                      S(7, -2, 7, 2)};
    Arrangement arr(shapes);

    const auto vertexAt = [&](const Point& point) {
        const auto found = std::ranges::find(arr.vertices(), point);
        REQUIRE(found != arr.vertices().end());
        return VertexId(static_cast<std::uint32_t>(found - arr.vertices().begin()));
    };
    const auto edgeThrough = [&](const Point& point) {
        for (std::uint32_t edge = 0; edge < arr.edgeCount(); ++edge) {
            const HalfedgeId h(2 * edge);
            if (std::visit([&](const auto& value) { return value.contains(point); }, arr[h])) {
                return h;
            }
        }
        return HalfedgeId();
    };

    const HalfedgeId at3 = edgeThrough(P(3, 1));
    const HalfedgeId at7 = edgeThrough(P(7, 1));
    REQUIRE(at3.valid());
    REQUIRE(at7.valid());
    const pgl::EOrientedLine forward(P(-1, 1), P(1, 1));
    CHECK(arr.reportIntersecting(forward) ==
          std::vector<IntersectionId>{IntersectionId(at3), IntersectionId(at7)});
    CHECK(arr.reportIntersecting(pgl::EOrientedLine(P(1, 1), P(-1, 1))) ==
          std::vector<IntersectionId>{IntersectionId(at7), IntersectionId(at3)});
    CHECK(arr.reportIntersecting(pgl::EOrientedSegment(P(-1, 1), P(5, 1))) ==
          std::vector<IntersectionId>{IntersectionId(at3)});
    CHECK(arr.reportIntersecting(Ray(P(-1, 1), P(0, 1))) ==
          std::vector<IntersectionId>{IntersectionId(at3), IntersectionId(at7)});

    const auto onArrangement =
        arr.reportIntersecting(pgl::EOrientedSegment(P(-1, 0), P(11, 0)));
    const std::vector<IntersectionId> expectedVertices{
        vertexAt(P(0, 0)), vertexAt(P(3, 0)), vertexAt(P(7, 0)), vertexAt(P(10, 0))};
    CHECK(onArrangement == expectedVertices);

    std::size_t visited = 0;
    CHECK(arr.visitIntersecting(forward, [&](const IntersectionId& id) {
        ++visited;
        CHECK(id == IntersectionId(at3));
        return true;
    }));
    CHECK(visited == 1);
    CHECK(arr.firstIntersecting(forward) == IntersectionId(at3));
    CHECK_FALSE(arr.emptyIntersecting(forward));
    CHECK(arr.emptyIntersecting(pgl::EOrientedSegment(P(-1, 5), P(11, 5))));
    std::vector<IntersectionId> visitedAll;
    CHECK_FALSE(arr.visitIntersecting(forward, [&](const IntersectionId& id) {
        visitedAll.push_back(id);
        return false;
    }));
    CHECK(visitedAll == arr.reportIntersecting(forward));

    const auto withoutIndex = arr.reportIntersecting(forward);
    arr.buildPointLocation();
    CHECK(arr.reportIntersecting(forward) == withoutIndex);
}

TEST_CASE("arrangement intersection traversal follows chains and reports cells once") {
    using IntersectionId = Arrangement::IntersectionId;
    Arrangement arr(std::vector<Segment>{S(0, 0, 10, 0), S(3, -2, 3, 2),
                                         S(7, -2, 7, 2)});
    const pgl::EPolyline polyline({P(-1, 1), P(5, 1), P(5, -1), P(9, -1),
                                   P(5, -1), P(5, 1)});
    const auto report = arr.reportIntersecting(polyline);
    REQUIRE(report.size() == 3);
    CHECK(std::holds_alternative<HalfedgeId>(report[0]));
    CHECK(std::holds_alternative<HalfedgeId>(report[1]));
    CHECK(std::holds_alternative<HalfedgeId>(report[2]));
    CHECK(std::visit([&](const auto& value) { return value.contains(P(3, 1)); },
                     arr[std::get<HalfedgeId>(report[0])]));
    CHECK(std::visit([&](const auto& value) { return value.contains(P(5, 0)); },
                     arr[std::get<HalfedgeId>(report[1])]));
    CHECK(std::visit([&](const auto& value) { return value.contains(P(7, -1)); },
                     arr[std::get<HalfedgeId>(report[2])]));

    const pgl::EMonotoneChain chain({P(-1, 1), P(5, 1), P(9, -1)});
    const auto chainReport = arr.reportIntersecting(chain);
    REQUIRE(chainReport.size() == 2);
    CHECK(std::holds_alternative<HalfedgeId>(chainReport[0]));
    CHECK(chainReport[1] == IntersectionId(
        VertexId(static_cast<std::uint32_t>(std::ranges::find(arr.vertices(), P(7, 0)) -
                                            arr.vertices().begin()))));
}

TEST_CASE("trapezoidal intersection traversal matches the linear traversal") {
    const std::vector<pgl::EShape> shapes{
        S(-8, -5, 9, 7), S(-9, 6, 8, -4), S(-7, 1, 10, 1),
        S(-2, -8, -2, 9), Line(P(-3, -6), P(4, 8)), Ray(P(1, -3), P(5, -1))};
    const Arrangement linear(shapes);
    Arrangement indexed = linear;
    indexed.buildPointLocation();

    const auto compare = [&](const auto& query) {
        CHECK(indexed.reportIntersecting(query) == linear.reportIntersecting(query));
        CHECK(indexed.firstIntersecting(query) == linear.firstIntersecting(query));
        CHECK(indexed.emptyIntersecting(query) == linear.emptyIntersecting(query));
    };
    for (int ax = -12; ax <= 12; ax += 4) {
        for (int ay = -11; ay <= 11; ay += 3) {
            int bx = 7 - ay;
            int by = ax + 5;
            if (ax == bx && ay == by) {
                ++bx;
            }
            compare(pgl::EOrientedSegment(P(ax, ay), P(bx, by)));
            compare(pgl::EOrientedLine(P(ax, ay), P(bx, by)));
            compare(Ray(P(ax, ay), P(bx, by)));
            compare(pgl::EPolyline({P(ax, ay), P(bx, by), P(bx + 3, by - 5)}));
        }
    }

    // The index is built on sheared coordinates, where an axis-parallel query
    // is no longer axis-parallel, so those directions get their own sweep.
    for (int a = -12; a <= 12; ++a) {
        compare(Ray(P(a, -13), P(a, -12)));       // straight up
        compare(Ray(P(a, 13), P(a, 12)));         // straight down
        compare(Ray(P(-13, a), P(-12, a)));       // straight right
        compare(Ray(P(13, a), P(12, a)));         // straight left
        compare(pgl::EOrientedLine(P(a, 0), P(a, 1)));
        compare(pgl::EOrientedLine(P(0, a), P(1, a)));
        compare(pgl::EOrientedSegment(P(a, -13), P(a, 13)));
        compare(pgl::EOrientedSegment(P(-13, a), P(13, a)));
    }
}

TEST_CASE("trapezoidal traversal matches the linear one on random arrangements") {
    std::mt19937 random(20260814);
    std::uniform_int_distribution<int> coordinate(-14, 14);
    for (int trial = 0; trial < 6; ++trial) {
        std::vector<Segment> shapes;
        while (shapes.size() < 7) {
            const Point a = P(coordinate(random), coordinate(random));
            const Point b = P(coordinate(random), coordinate(random));
            if (!(a == b)) {
                shapes.emplace_back(a, b);
            }
        }
        const Arrangement linear(shapes);
        Arrangement indexed = linear;
        indexed.buildPointLocation();

        for (int query = 0; query < 40; ++query) {
            const Point a = P(coordinate(random), coordinate(random));
            const Point b = P(coordinate(random), coordinate(random));
            if (a == b) {
                continue;
            }
            // Vertical and horizontal queries stay in the mix on purpose.
            for (const Point& target :
                 {b, Point(a.x(), b.y()), Point(b.x(), a.y())}) {
                if (target == a) {
                    continue;
                }
                CHECK(indexed.reportIntersecting(Ray(a, target)) ==
                      linear.reportIntersecting(Ray(a, target)));
                CHECK(indexed.reportIntersecting(pgl::EOrientedSegment(a, target)) ==
                      linear.reportIntersecting(pgl::EOrientedSegment(a, target)));
                CHECK(indexed.reportIntersecting(pgl::EOrientedLine(a, target)) ==
                      linear.reportIntersecting(pgl::EOrientedLine(a, target)));
            }
        }
    }
}

TEST_CASE("a chain touching an endpoint still reports the edge it later crosses") {
    using IntersectionId = Arrangement::IntersectionId;
    Arrangement arr(std::vector<Segment>{S(3, -2, 3, 2)});
    const HalfedgeId edge(0);
    const VertexId low(static_cast<std::uint32_t>(
        std::ranges::find(arr.vertices(), P(3, -2)) - arr.vertices().begin()));
    REQUIRE(low.index() < arr.vertexCount());

    // The first piece stops on the edge's lower endpoint, so that piece reports
    // the vertex alone. The third piece crosses the interior at (3,1), which no
    // vertex stands for, so the edge is still reported.
    const std::vector<IntersectionId> touchThenCross{IntersectionId(low),
                                                     IntersectionId(edge)};
    CHECK(arr.reportIntersecting(
              pgl::EPolyline({P(0, -2), P(3, -2), P(0, 1), P(6, 1)})) ==
          touchThenCross);
    CHECK(arr.reportIntersecting(
              pgl::EPolyline({P(6, 1), P(0, 1), P(3, -2), P(0, -2)})) ==
          std::vector<IntersectionId>{IntersectionId(edge), IntersectionId(low)});

    // A chain meeting the edge only at that endpoint reports the vertex alone.
    CHECK(arr.reportIntersecting(pgl::EPolyline({P(0, -2), P(3, -2), P(0, -4)})) ==
          std::vector<IntersectionId>{IntersectionId(low)});

    arr.buildPointLocation();
    CHECK(arr.reportIntersecting(
              pgl::EPolyline({P(0, -2), P(3, -2), P(0, 1), P(6, 1)})) ==
          touchThenCross);
}

TEST_CASE("arrangement intersection traversal accepts mixed coordinate types") {
    using IntPoint = pgl::Point<int>;
    using IntSegment = pgl::Segment<IntPoint>;
    using IntArrangement = pgl::Arrangement<IntPoint>;
    const std::vector<IntSegment> shapes{IntSegment(IntPoint(0, 0), IntPoint(6, 0)),
                                         IntSegment(IntPoint(2, -2), IntPoint(2, 2))};
    IntArrangement arr(shapes);
    const pgl::OrientedLine<pgl::Point<long>> query(pgl::Point<long>(-3, 1),
                                                     pgl::Point<long>(5, 1));
    const auto expected = arr.reportIntersecting(query);
    REQUIRE(expected.size() == 1);
    CHECK(std::holds_alternative<IntArrangement::HalfedgeId>(expected.front()));
    arr.buildPointLocation();
    CHECK(arr.reportIntersecting(query) == expected);
}

TEST_CASE("the graph view carries the vertices and the incidences") {
    // A cross, plus a point on nothing: the crossing vertex has degree four,
    // the four endpoints degree one, and the isolated point a component of its
    // own.
    const Arrangement arr(std::vector<Segment>{S(-2, 0, 2, 0), S(0, -2, 0, 2)},
                          std::vector<Point>{P(5, 5)});
    checkInvariants(arr);
    const pgl::Graph<VertexId> graph = arr.asGraph();
    REQUIRE(graph.vertexCount() == 6);
    CHECK(graph.edgeCount() == static_cast<int>(arr.edgeCount()));
    CHECK(graph.maxDegree() == 4);

    const auto vertexAt = [&arr](const Point& p) {
        const auto found = std::ranges::find(arr.vertices(), p);
        REQUIRE(found != arr.vertices().end());
        return VertexId(static_cast<std::uint32_t>(found - arr.vertices().begin()));
    };
    const VertexId center = vertexAt(P(0, 0));
    CHECK(graph.degree(center) == 4);
    CHECK(graph.containsEdge(center, vertexAt(P(2, 0))));
    CHECK_FALSE(graph.containsEdge(vertexAt(P(2, 0)), vertexAt(P(0, 2))));
    CHECK(graph.degree(vertexAt(P(5, 5))) == 0);
    CHECK(graph.components().size() == 2);
    CHECK(graph.bfs(center).size() == 5);

    // Vertices of a bounded arrangement are exactly its finite ones.
    for (const VertexId v : graph) {
        CHECK_FALSE(arr.isFictitious(v));
    }
}

TEST_CASE("the graph view holds the fictitious vertex of an unbounded arrangement") {
    // A ray and a line: the ray joins its source to infinity, and the line runs
    // from infinity back to it, so it is a self-loop the simple graph drops.
    const std::vector<pgl::Shape<Point>> shapes{pgl::Shape<Point>(Ray(P(0, 0), P(1, 0))),
                                                pgl::Shape<Point>(Line(P(0, 5), P(1, 5)))};
    const Arrangement arr(shapes);
    checkInvariants(arr);
    REQUIRE(arr.isUnbounded());
    const VertexId infinity(static_cast<std::uint32_t>(arr.vertexCount()));
    REQUIRE(arr.isFictitious(infinity));

    const pgl::Graph<VertexId> graph = arr.asGraph();
    CHECK(graph.vertexCount() == static_cast<int>(arr.vertexCount()) + 1);
    CHECK(graph.containsVertex(infinity));
    CHECK(arr.edgeCount() == 2);
    CHECK(graph.edgeCount() == 1);  // the line's self-loop is dropped
    CHECK(graph.degree(infinity) == 1);
    CHECK(graph.containsEdge(infinity, VertexId(0)));
    CHECK(graph.components().size() == 1);

    // Two rays leaving the same source meet again at infinity; the parallel
    // edges coalesce, so both endpoints keep degree one.
    const Arrangement fan(std::vector<Ray>{Ray(P(0, 0), P(1, 1)), Ray(P(0, 0), P(1, -1))});
    checkInvariants(fan);
    const pgl::Graph<VertexId> fanGraph = fan.asGraph();
    CHECK(fan.edgeCount() == 2);
    CHECK(fanGraph.vertexCount() == 2);
    CHECK(fanGraph.edgeCount() == 1);
    CHECK(fanGraph.maxDegree() == 1);
}

TEST_CASE("the graph of a line is one isolated fictitious vertex") {
    const Arrangement arr(std::vector<Line>{Line(P(0, 0), P(1, 1))});
    checkInvariants(arr);
    const pgl::Graph<VertexId> graph = arr.asGraph();
    CHECK(arr.vertexCount() == 0);
    CHECK(graph.vertexCount() == 1);
    CHECK(graph.edgeCount() == 0);
    CHECK(arr.isFictitious(*graph.begin()));

    const Arrangement empty;
    checkInvariants(empty);
    CHECK(empty.asGraph().vertexCount() == 0);
    CHECK(empty.asGraph().edgeCount() == 0);
}
