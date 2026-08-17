#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "pgl.hpp"

namespace {

// An integral point type can hold an arrangement whose crossings are all
// integral, which axis-parallel input guarantees just as input meeting only at
// its endpoints does. What these tests pin down is that it is then exact — the
// same cells, the same locations, the same traversals as the rational
// arrangement over the same coordinates — at magnitudes where the quantities
// behind a crossing and behind the point-location scan are cubic in the
// coordinates and so overflow the coordinate type unless they are widened.

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using SegmentLine = pgl::Line<Point>;
using Ray = pgl::Ray<Point>;
using Shape = pgl::Shape<Point>;
using Arrangement = pgl::Arrangement<Point>;

using ExactPoint = pgl::EPoint;
using ExactSegment = pgl::ESegment;
using ExactRay = pgl::ERay;
using ExactShape = pgl::EShape;
using ExactArrangement = pgl::Arrangement<ExactPoint>;

// The magnitudes worth checking: everything past the third is beyond the cube
// root of int's range, where a cubic quantity no longer fits a single
// coordinate. The top of each list is what keeps the widest coordinate the test
// itself forms — `10 * scale` here, `40 * scale` for the input below that
// reaches to 30 — inside int, so that the test's own arithmetic never overflows
// while the library's is under examination.
const std::vector<int> scales = {1, 37, 1000, 10000, 1000000, 100000000};
const std::vector<int> narrowerScales = {1, 37, 1000, 10000, 1000000, 20000000};

// One axis-parallel input, in plain ints, instantiated into either point type.
struct RawSegment {
    int ax, ay, bx, by;
};

// Orthogonal input with every degeneracy the arrangement has to survive: shared
// endpoints, T-joints, collinear overlaps, proper crossings, a dangling edge,
// two rays and three isolated points, one of them on a vertex.
std::vector<RawSegment> rawSegments() {
    return {{-4, -4, 4, -4}, {4, -4, 4, 4}, {4, 4, -4, 4},   {-4, 4, -4, -4},
            {-4, 0, 4, 0},   {0, -4, 0, 4}, {-2, -4, -2, 0}, {-6, 0, 2, 0},
            {2, 0, 6, 0},    {2, 2, 2, 6},  {-4, -4, -4, 4}};
}

std::vector<RawSegment> rawRays() {
    return {{0, 6, 0, 9}, {-6, -6, -9, -6}};
}

std::vector<std::pair<int, int>> rawPoints() {
    return {{3, 3}, {-3, -1}, {0, 0}};
}

// The queries: a grid over the input, and rays entering it from outside in both
// axis directions and diagonally. A slanted query against an orthogonal
// arrangement still meets nothing but arrangement cells, so its answers stay
// integral too.
const int gridRadius = 8;

template <class P>
std::vector<P> queryPoints(int scale) {
    using N = typename P::NumberType;
    std::vector<P> queries;
    for (int y = -gridRadius; y <= gridRadius; ++y) {
        for (int x = -gridRadius; x <= gridRadius; ++x) {
            queries.push_back(P(N(x * scale), N(y * scale)));
        }
    }
    return queries;
}

template <class R, class P>
std::vector<R> queryRays(int scale) {
    using N = typename P::NumberType;
    const int outside = (gridRadius + 2) * scale;
    std::vector<R> rays;
    for (int c = -gridRadius; c <= gridRadius; ++c) {
        const int t = c * scale;
        rays.push_back(R(P(N(t), N(-outside)), P(N(t), N(-outside + scale))));
        rays.push_back(R(P(N(-outside), N(t)), P(N(-outside + scale), N(t))));
        rays.push_back(R(P(N(-outside), N(t)), P(N(-outside + scale), N(t + scale))));
    }
    return rays;
}

// Cells are named by the geometry they stand for rather than by their handles,
// so that the two arrangements can be compared without assuming they number
// anything the same way.
std::string coordinate(const pgl::ERational& value) {
    std::ostringstream out;
    out << value.numerator() << "/" << value.denominator();
    return out.str();
}

template <class P>
std::string position(const P& p) {
    return "(" + coordinate(pgl::ERational(p.x())) + "," +
           coordinate(pgl::ERational(p.y())) + ")";
}

// Every cell of an arrangement and every answer it gives to the queries above,
// as one comparable list.
template <class A, class P, class R>
std::vector<std::string> report(const A& arrangement, const std::vector<P>& queries,
                                const std::vector<R>& rays) {
    using VertexId = typename A::VertexId;
    using HalfedgeId = typename A::HalfedgeId;
    using FaceId = typename A::FaceId;

    const auto vertexName = [&](VertexId v) {
        return v.index() >= arrangement.vertexCount() ? std::string("infinity")
                                                      : position(arrangement[v]);
    };
    const auto edgeName = [&](HalfedgeId h) {
        const HalfedgeId even(h.index() & ~std::uint32_t{1});
        std::string source = vertexName(arrangement.source(even));
        std::string target = vertexName(arrangement.target(even));
        if (target < source) {
            std::swap(source, target);
        }
        return "E" + source + target;
    };

    std::vector<std::string> lines;
    lines.push_back("counts " + std::to_string(arrangement.vertexCount()) + " " +
                    std::to_string(arrangement.halfedgeCount()) + " " +
                    std::to_string(arrangement.faceCount()));
    for (std::uint32_t v = 0; v < arrangement.vertexCount(); ++v) {
        lines.push_back("V" + position(arrangement[VertexId(v)]));
    }
    for (std::uint32_t h = 0; h < arrangement.halfedgeCount(); h += 2) {
        lines.push_back(edgeName(HalfedgeId(h)));
    }

    // A face is named by the first query that landed in it, which identifies it
    // across the two arrangements without depending on face numbering.
    std::vector<std::string> faceName(arrangement.faceCount());
    for (std::size_t i = 0; i < queries.size(); ++i) {
        const FaceId f = arrangement.locateFace(queries[i]);
        REQUIRE(f.index() < faceName.size());
        if (faceName[f.index()].empty()) {
            faceName[f.index()] = "F" + position(queries[i]);
        }
        lines.push_back("locateFace " + position(queries[i]) + " " +
                        faceName[f.index()]);
    }
    for (const P& q : queries) {
        const auto cell = arrangement.locateCell(q);
        if (const auto* v = std::get_if<VertexId>(&cell)) {
            lines.push_back("locateCell " + position(q) + " V" + vertexName(*v));
        } else if (const auto* h = std::get_if<HalfedgeId>(&cell)) {
            lines.push_back("locateCell " + position(q) + " " + edgeName(*h));
        } else {
            lines.push_back("locateCell " + position(q) + " " +
                            faceName[std::get<FaceId>(cell).index()]);
        }
    }
    for (const R& r : rays) {
        std::string trail = "shoot ";
        for (const auto& id : arrangement.reportIntersecting(r)) {
            if (const auto* v = std::get_if<VertexId>(&id)) {
                trail += "V" + vertexName(*v) + " ";
            } else {
                trail += edgeName(std::get<HalfedgeId>(id)) + " ";
            }
        }
        lines.push_back(trail);
    }
    return lines;
}

// The whole comparison at one coordinate scale, with and without the
// point-location index. The query grid is spread over @p queryScale, which the
// caller widens when the input reaches further than the input scale.
void checkIntegralMatchesExact(const std::vector<RawSegment>& rawEdges,
                              const std::vector<RawSegment>& rays,
                              const std::vector<std::pair<int, int>>& isolated,
                              int scale, int queryScale) {
    std::vector<Shape> shapes;
    std::vector<ExactShape> exactShapes;
    for (const RawSegment& s : rawEdges) {
        shapes.push_back(Segment(Point(s.ax * scale, s.ay * scale),
                                 Point(s.bx * scale, s.by * scale)));
        exactShapes.push_back(ExactSegment(ExactPoint(s.ax * scale, s.ay * scale),
                                           ExactPoint(s.bx * scale, s.by * scale)));
    }
    for (const RawSegment& r : rays) {
        shapes.push_back(
            Ray(Point(r.ax * scale, r.ay * scale), Point(r.bx * scale, r.by * scale)));
        exactShapes.push_back(ExactRay(ExactPoint(r.ax * scale, r.ay * scale),
                                       ExactPoint(r.bx * scale, r.by * scale)));
    }
    std::vector<Point> points;
    std::vector<ExactPoint> exactPoints;
    for (const auto& p : isolated) {
        points.push_back(Point(p.first * scale, p.second * scale));
        exactPoints.push_back(ExactPoint(p.first * scale, p.second * scale));
    }

    Arrangement arrangement(shapes, points);
    ExactArrangement exact(exactShapes, exactPoints);

    const std::vector<Point> queries = queryPoints<Point>(queryScale);
    const std::vector<ExactPoint> exactQueries = queryPoints<ExactPoint>(queryScale);
    const std::vector<Ray> shots = queryRays<Ray, Point>(queryScale);
    const std::vector<ExactRay> exactShots = queryRays<ExactRay, ExactPoint>(queryScale);

    const std::vector<std::string> scanned = report(arrangement, queries, shots);
    const std::vector<std::string> expected = report(exact, exactQueries, exactShots);
    REQUIRE(scanned.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CAPTURE(scale);
        CAPTURE(i);
        CHECK(scanned[i] == expected[i]);
    }

    // A face witness has to fall inside the face it names. The boundary sweep
    // behind one compares hits along a ray by cross-multiplying two quadratic
    // parameters, so it is the quartic corner of the same question.
    for (std::uint32_t i = 0; i < arrangement.faceCount(); ++i) {
        const Arrangement::FaceId f(i);
        if (arrangement.isUnbounded(f)) {
            continue;
        }
        CAPTURE(scale);
        CAPTURE(i);
        CHECK(arrangement.polygonWithHoles<pgl::ERational>(f).contains(
            arrangement.witness<pgl::ERational>(f)));
    }

    // The trapezoidal index has to give the integral arrangement the same
    // answers its own scan does.
    std::mt19937 generator(1729);
    arrangement.buildPointLocation(generator);
    REQUIRE(arrangement.hasPointLocation());
    const std::vector<std::string> indexed = report(arrangement, queries, shots);
    REQUIRE(indexed.size() == scanned.size());
    for (std::size_t i = 0; i < scanned.size(); ++i) {
        CAPTURE(scale);
        CAPTURE(i);
        CHECK(indexed[i] == scanned[i]);
    }
}

}  // namespace

TEST_CASE("an orthogonal crossing is exact in an integral coordinate type") {
    for (int scale : scales) {
        CAPTURE(scale);
        const Segment horizontal(Point(-7 * scale, 3 * scale), Point(5 * scale, 3 * scale));
        const Segment vertical(Point(2 * scale, -4 * scale), Point(2 * scale, 6 * scale));
        const auto crossing = horizontal.intersection<int>(vertical);
        REQUIRE(crossing);
        REQUIRE(std::holds_alternative<Point>(*crossing));
        CHECK(std::get<Point>(*crossing) == Point(2 * scale, 3 * scale));

        // A crossing that is integral without either carrier being axis-parallel:
        // exactness is a property of the crossing, not of the slopes.
        const Segment rising(Point(-2 * scale, -2 * scale), Point(6 * scale, 6 * scale));
        const Segment falling(Point(-2 * scale, 6 * scale), Point(6 * scale, -2 * scale));
        const auto diagonal = rising.intersection<int>(falling);
        REQUIRE(diagonal);
        REQUIRE(std::holds_alternative<Point>(*diagonal));
        CHECK(std::get<Point>(*diagonal) == Point(2 * scale, 2 * scale));
    }
}

TEST_CASE("an orthogonal crossing of carriers is exact in an integral type") {
    for (int scale : scales) {
        CAPTURE(scale);
        const SegmentLine horizontal(Point(-7 * scale, 3 * scale), Point(-5 * scale, 3 * scale));
        const SegmentLine vertical(Point(2 * scale, -4 * scale), Point(2 * scale, -3 * scale));
        const auto crossing = horizontal.intersection<int>(vertical);
        REQUIRE(crossing);
        REQUIRE(std::holds_alternative<Point>(*crossing));
        CHECK(std::get<Point>(*crossing) == Point(2 * scale, 3 * scale));
    }
}

TEST_CASE("an integral orthogonal arrangement equals the rational one") {
    for (int scale : scales) {
        checkIntegralMatchesExact(rawSegments(), rawRays(), rawPoints(), scale, scale);
    }
}

TEST_CASE("an integral arrangement of input meeting only at its endpoints") {
    // The other case an integral point type is adequate for. Nothing crosses
    // transversally, so every vertex is an input endpoint — but the collinear
    // overlap still makes the construction cut a slanted carrier at a parameter,
    // which is where two coordinate differences get multiplied.
    // Two collinear halves of `y = x/2` overlapping along their middle, a
    // vertical stem rising from the point where the second one starts, and a
    // return path that touches the rest only at (0,0) and (30,15).
    const std::vector<RawSegment> slanted = {{0, 0, 20, 10},
                                             {10, 5, 30, 15},
                                             {30, 15, 30, -5},
                                             {30, -5, 0, 0},
                                             {10, 5, 10, 25}};
    for (int scale : narrowerScales) {
        checkIntegralMatchesExact(slanted, {}, {}, scale, 4 * scale);
    }
}

TEST_CASE("integral point location survives coordinates spread over the range") {
    // Random axis-parallel input at a magnitude that fills a good part of int's
    // range, where nothing but a widened comparison keeps the scan and the index
    // agreeing with each other.
    std::mt19937 generator(20260817);
    std::uniform_int_distribution<int> coordinate(-500000000, 500000000);
    std::uniform_int_distribution<int> length(1, 500000000);
    std::vector<Shape> shapes;
    for (int i = 0; i < 60; ++i) {
        const int x = coordinate(generator);
        const int y = coordinate(generator);
        const int l = length(generator);
        shapes.push_back(i % 2 == 0
                             ? Shape(Segment(Point(x, y), Point(x + l, y)))
                             : Shape(Segment(Point(x, y), Point(x, y + l))));
    }

    Arrangement arrangement(shapes);
    std::vector<Point> queries;
    for (int y = -4; y <= 4; ++y) {
        for (int x = -4; x <= 4; ++x) {
            queries.push_back(Point(x * 125000000, y * 125000000));
        }
    }

    std::vector<Arrangement::FaceId> scanned;
    for (const Point& q : queries) {
        scanned.push_back(arrangement.locateFace(q));
    }
    std::mt19937 order(1729);
    arrangement.buildPointLocation(order);
    for (std::size_t i = 0; i < queries.size(); ++i) {
        CAPTURE(queries[i]);
        CHECK(arrangement.locateFace(queries[i]) == scanned[i]);
    }
}
