#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "pgl.hpp"

namespace {

using Number = pgl::ERational;
using Point = pgl::EPoint;
using Segment = pgl::ESegment;
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

// Number of connected components of the arrangement seen as a graph, isolated
// vertices included. This is the C of Euler's formula V - E + F = 1 + C.
std::size_t componentCount(const Arrangement& arr) {
    std::vector<std::size_t> parent(arr.vertexCount());
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

    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        CHECK(arr.twin(arr.twin(h)) == h);
        CHECK(arr.twin(h) != h);
        CHECK(arr.face(arr.next(h)) == arr.face(h));
        CHECK(arr.source(arr.next(h)) == arr.target(h));
        CHECK(arr.source(h) != arr.target(h));
        CHECK_FALSE(arr.isFictitious(h));
        // A halfedge indexes to the directed segment between the topology's
        // source and target; its twin has the reverse direction.
        CHECK(arr[h].source() == arr[arr.source(h)]);
        CHECK(arr[h].target() == arr[arr.target(h)]);
        CHECK(arr[h].source() == arr[arr.twin(h)].target());
        CHECK(arr[h].target() == arr[arr.twin(h)].source());
        CHECK_FALSE(arr.originsOf(h).empty());
    }

    // Every halfedge leaving a vertex says so, and the rotational fan reaches
    // all of them.
    std::map<VertexId, std::size_t> degree;
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        ++degree[arr.source(h)];
    }
    for (std::uint32_t i = 0; i < arr.vertexCount(); ++i) {
        const VertexId v(i);
        const HalfedgeId start = arr.outgoing(v);
        const std::vector<HalfedgeId> outgoing = arr.outgoingHalfedges(v);
        if (!start.valid()) {
            CHECK(degree[v] == 0);
            CHECK(outgoing.empty());
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

    // The edge list is one entry per twin pair, and agrees with indexing either
    // halfedge of the pair.
    const std::vector<Segment> edges = arr.edges();
    REQUIRE(edges.size() == arr.edgeCount());
    for (std::uint32_t i = 0; i < arr.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        CHECK(edges[h.index() / 2] == Segment(arr[h].source(), arr[h].target()));
    }
    CHECK(arr.vertices().size() == arr.vertexCount());

    // Euler's formula, over the whole complex.
    const std::size_t v = arr.vertexCount();
    const std::size_t e = arr.edgeCount();
    const std::size_t f = arr.faceCount();
    CHECK(v + f == e + 1 + componentCount(arr));

    // Each bounded face's witness is where it says it is.
    for (std::uint32_t i = 0; i < arr.faceCount(); ++i) {
        const FaceId face(i);
        if (arr.isUnbounded(face)) {
            continue;
        }
        const Point witness = arr.witness(face);
        CHECK(arr.locate(witness) == face);
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
    CHECK(arr.innerCycles(arr.locate(P(0, 0) + Point(Number(1, 2), Number(1, 2)))).size() ==
          static_cast<std::size_t>(cells) * cells);
    for (int i = 0; i < cells; ++i) {
        for (int j = 0; j < cells; ++j) {
            const FaceId ring = arr.locate(P(10 * i + 2, 10 * j + 2));
            const FaceId inside = arr.locate(P(10 * i + 4, 10 * j + 4));
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
    CHECK(empty.locate(P(3, 7)) == FaceId(0));
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
        const FaceId inside = arr.locate(P(1, 1));
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
        const FaceId ring = arr.locate(P(1, 1));
        REQUIRE(arr.outerCycle(ring).valid());
        REQUIRE(arr.innerCycles(ring).size() == 1);
        checkBoundaryOf(arr, ring, {arr.outerCycle(ring), arr.innerCycles(ring).front()});
        CHECK(arr.outerBoundaryOf(ring) == cycleFrom(arr, arr.outerCycle(ring)));
        CHECK(arr.innerBoundariesOf(ring) ==
              std::vector<std::vector<HalfedgeId>>{
                  cycleFrom(arr, arr.innerCycles(ring).front())});
    }
}

TEST_CASE("a single segment is two vertices, one edge and no bounded face") {
    const Arrangement arr = arrangementOf({S(0, 0, 4, 2)});
    CHECK(arr.vertexCount() == 2);
    CHECK(arr.edgeCount() == 1);
    CHECK(arr.faceCount() == 1);
    // The two halfedges form a single cycle, an inner cycle of the outer face.
    CHECK(arr.innerCycles(FaceId(0)).size() == 1);
    CHECK(arr.locate(P(2, 1)) == FaceId(0));

    const HalfedgeId h = arr.outgoing(VertexId(0));
    CHECK(arr.witness(h) == P(2, 1));
    CHECK(arr[h].source() == P(0, 0));
    CHECK(arr[h].target() == P(4, 2));
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
    CHECK(arr.locate(P(1, 1)) == FaceId(1));
    CHECK(arr.locate(P(5, 1)) == FaceId(0));
    CHECK(arr.locate(P(-1, 2)) == FaceId(0));

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

    const FaceId ring = arr.locate(P(1, 1));
    const FaceId inside = arr.locate(P(3, 3));
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
        CHECK(arr.innerCycles(arr.locate(P(0, 5))).size() == 1);
        CHECK(arr.innerCycles(arr.locate(P(0, 3))).size() == 1);
        CHECK(arr.innerCycles(arr.locate(P(0, 1))).empty());
        CHECK(arr.polygonWithHoles(arr.locate(P(0, 3))).holeCount() == 1);
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
        const FaceId around = arr.locate(P(7, 1));
        CHECK(arr.innerCycles(around).size() == 2);
        CHECK(arr.polygonWithHoles(around).holeCount() == 2);
        CHECK(arr.innerCycles(arr.locate(P(2, 5))).empty());   // the triangle
        CHECK(arr.innerCycles(arr.locate(P(5, 3))).empty());   // the small square
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
            origins[Segment(arr[h].source(), arr[h].target())] = arr.originsOf(h).size();
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
        CHECK(arr.locate(P(3, 1)) == FaceId(1));
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
    CHECK(arr.locate(P(2, 2)) == FaceId(1));
    CHECK(arr.locate(P(9, 9)) == FaceId(0));
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

TEST_CASE("polygonal input contributes its boundary") {
    SUBCASE("polygons") {
        const std::vector<PolygonShape> polygons{PolygonShape({P(0, 0), P(4, 0), P(4, 4), P(0, 4)}),
                                                  PolygonShape({P(2, 2), P(6, 2), P(6, 6), P(2, 6)})};
        const Arrangement arr(polygons);
        checkInvariants(arr);
        // The two squares overlap in a third one, so three bounded faces.
        CHECK(arr.faceCount() == 4);
        CHECK(arr.polygonWithHoles(arr.locate(P(3, 3))) == regionOf({P(2, 2), P(4, 2), P(4, 4), P(2, 4)}));
    }

    SUBCASE("a region, holes included") {
        const Region holed(PolygonShape({P(0, 0), P(6, 0), P(6, 6), P(0, 6)}),
                            std::vector<PolygonShape>{PolygonShape({P(2, 2), P(4, 2), P(4, 4), P(2, 4)})});
        const Arrangement arr(std::vector<Region>{holed});
        checkInvariants(arr);
        REQUIRE(arr.faceCount() == 3);
        CHECK(arr.polygonWithHoles(arr.locate(P(1, 1))) == holed);
        CHECK(arr.polygonWithHoles(arr.locate(P(3, 3))) == regionOf({P(2, 2), P(4, 2), P(4, 4), P(2, 4)}));
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
        CHECK(arr.label(h) == arr[h].label());
    }

    // The edge list carries the same labels, one entry per twin pair.
    const std::vector<LabeledSegment> edges = arr.edges();
    REQUIRE(edges.size() == arr.edgeCount());
    std::multiset<int> labels;
    for (const LabeledSegment& edge : edges) {
        labels.insert(edge.label());
    }
    CHECK(labels == std::multiset<int>{7, 7, 9, 9});

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
    CHECK(arr.locate(IntPoint(2, 2)) == IntArrangement::FaceId(1));
    CHECK(arr.locate(IntPoint(9, 2)) == IntArrangement::FaceId(0));
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
