#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "pgl.hpp"

namespace {

// Build a Point of the test's number type from integer coordinates.
template <class Point>
Point P(int x, int y) {
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;
    return Point(Number(x), Number(y));
}

// Number of (point, triangle) pairs where the point is STRICTLY inside the
// triangle's circumcircle. Zero is the defining property of a Delaunay
// triangulation, and the property we want preserved even under cocircularity.
template <class Tri, class Point>
int64_t strictInCircleViolations(const Tri& tri, const std::vector<Point>& pts) {
    int64_t viol = 0;
    for (const auto& t : tri.triangles()) {
        for (const auto& p : pts) {
            if (pgl::inCircleSign(t.a(), t.b(), t.c(), p) ==
                std::partial_ordering::greater) {
                ++viol;
            }
        }
    }
    return viol;
}

// True if every triangle is oriented counterclockwise.
template <class Tri>
bool allCounterClockwise(const Tri& tri) {
    for (const auto& t : tri.triangles()) {
        if (!(pgl::orientationSign(t.a(), t.b(), t.c()) ==
              std::partial_ordering::greater)) {
            return false;
        }
    }
    return true;
}

// Sum of twice-areas of all triangles (exact in the number type).
template <class Tri>
auto totalTwiceArea(const Tri& tri) {
    using Number = typename Tri::NumberType;
    Number sum = Number(0);
    for (const auto& t : tri.triangles()) {
        sum += t.twiceArea();
    }
    return sum;
}

// True if every input point appears as a vertex of some triangle (so no point
// — in particular no collinear one — was silently dropped).
template <class Tri, class Point>
bool everyPointIsAVertex(const Tri& tri, const std::vector<Point>& pts) {
    std::set<Point> used;
    for (const auto& t : tri.triangles()) {
        used.insert(t.a());
        used.insert(t.b());
        used.insert(t.c());
    }
    for (const auto& p : pts) {
        if (!used.count(p)) {
            return false;
        }
    }
    return true;
}

// True if the mesh is conforming: no input point lies in the relative interior
// of a triangle edge without being one of that edge's endpoints.
template <class Tri, class Point>
bool isConforming(const Tri& tri, const std::vector<Point>& pts) {
    for (const auto& t : tri.triangles()) {
        for (const auto& e : t.edges()) {
            for (const auto& p : pts) {
                if (p != e[0] && p != e[1] && e.contains(p)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// Each predicate of the triangulated domain must answer exactly what the same
// predicate of the polygon it triangulates answers for the same shape.
template <class Tri, class Point, class Shape>
void agreesWithPolygon(const Tri& tri, const pgl::Polygon<Point>& poly, const Shape& s) {
    CHECK(tri.contains(s) == poly.contains(s));
    CHECK(tri.intersects(s) == poly.intersects(s));
    CHECK(tri.interiorContains(s) == poly.interiorContains(s));
    CHECK(tri.interiorsIntersect(s) == poly.interiorsIntersect(s));
}

// True if some triangle is degenerate (zero area).
template <class Tri>
bool hasDegenerateTriangle(const Tri& tri) {
    using Number = typename Tri::NumberType;
    for (const auto& t : tri.triangles()) {
        if (t.twiceArea() == Number(0)) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE_TEMPLATE("Delaunay triangulation of a point set is exact and valid",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>,
                   pgl::Point<pgl::Rational<pgl::BigInt>>) {
    // Ten points in general position (no degenerate input).
    const std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(30, 5),  P<Point>(55, 0),  P<Point>(60, 30),
        P<Point>(50, 55), P<Point>(25, 60), P<Point>(0, 50),  P<Point>(20, 25),
        P<Point>(40, 20), P<Point>(35, 40),
    };
    pgl::Triangulation tri(pts);

    CHECK(tri.numVertices() == pts.size());
    CHECK_FALSE(tri.empty());
    CHECK(tri.checkInvariants());

    // Defining Delaunay property, all faces CCW, and a watertight tiling of the
    // convex hull (no gaps, no overlaps): triangle areas sum to the hull area.
    CHECK(strictInCircleViolations(tri, pts) == 0);
    CHECK(allCounterClockwise(tri));
    CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(pts).twiceArea());

    // Accessor consistency.
    CHECK(tri.triangles().size() == tri.numTriangles());
    CHECK(tri.edges().size() == tri.numEdges());
    for (const auto& t : tri.triangles()) {
        CHECK(tri.has(t));
    }
}

TEST_CASE_TEMPLATE("Degenerate point sets produce an empty triangulation",
                   Point, pgl::Point<int>, pgl::Point<pgl::Rational<int64_t>>) {
    SUBCASE("fewer than three points") {
        const std::vector<Point> pts = {P<Point>(0, 0), P<Point>(4, 1)};
        pgl::Triangulation tri(pts);
        CHECK(tri.empty());
        CHECK(tri.numTriangles() == 0);
    }
    SUBCASE("all points collinear") {
        const std::vector<Point> pts = {P<Point>(0, 0), P<Point>(1, 1),
                                        P<Point>(2, 2), P<Point>(5, 5)};
        pgl::Triangulation tri(pts);
        CHECK(tri.empty());
    }
}

TEST_CASE_TEMPLATE("Cocircular points still yield a valid Delaunay triangulation",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>,
                   pgl::Point<pgl::Rational<pgl::BigInt>>) {
    // The Delaunay triangulation is not unique when points are cocircular, but
    // whichever one we produce must keep the strict empty-circumcircle property
    // and tile the hull exactly.
    SUBCASE("square plus its center") {
        const std::vector<Point> pts = {P<Point>(0, 0), P<Point>(6, 0),
                                        P<Point>(6, 6), P<Point>(0, 6),
                                        P<Point>(3, 3)};
        pgl::Triangulation tri(pts);
        CHECK(tri.checkInvariants());
        CHECK(strictInCircleViolations(tri, pts) == 0);
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(pts).twiceArea());
    }
    SUBCASE("twelve points on a common circle") {
        // Integer points on the radius-5 circle: all twelve are cocircular and
        // in convex position, so any triangulation of the 12-gon is Delaunay.
        const std::vector<Point> pts = {
            P<Point>(5, 0),   P<Point>(4, 3),   P<Point>(3, 4),   P<Point>(0, 5),
            P<Point>(-3, 4),  P<Point>(-4, 3),  P<Point>(-5, 0),  P<Point>(-4, -3),
            P<Point>(-3, -4), P<Point>(0, -5),  P<Point>(3, -4),  P<Point>(4, -3),
        };
        pgl::Triangulation tri(pts);
        CHECK(tri.checkInvariants());
        CHECK(tri.numTriangles() == 10);  // convex k-gon -> k-2 triangles
        CHECK(strictInCircleViolations(tri, pts) == 0);
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(pts).twiceArea());
    }
}

TEST_CASE_TEMPLATE("Partially collinear point sets triangulate validly and conformingly",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>,
                   pgl::Point<pgl::Rational<pgl::BigInt>>) {
    // Collinear input is supported as int64_t as the points are not ALL collinear:
    // the collinear points must still be used (not absorbed into a flat triangle
    // or a spanning edge), and the result must stay conforming and Delaunay.
    SUBCASE("five collinear points on a hull edge, apex above") {
        const std::vector<Point> pts = {
            P<Point>(0, 0), P<Point>(1, 0), P<Point>(2, 0),
            P<Point>(3, 0), P<Point>(4, 0), P<Point>(2, 5),
        };
        pgl::Triangulation tri(pts);
        CHECK(tri.checkInvariants());
        CHECK_FALSE(hasDegenerateTriangle(tri));
        CHECK(strictInCircleViolations(tri, pts) == 0);
        CHECK(allCounterClockwise(tri));
        CHECK(everyPointIsAVertex(tri, pts));
        CHECK(isConforming(tri, pts));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(pts).twiceArea());
    }
    SUBCASE("crossing collinear triples, shared vertex interior to the hull") {
        // A rhombus with its center: both (0,0)-(3,0)-(6,0) and (3,-4)-(3,0)-(3,4)
        // are collinear, and (3,0) is strictly inside the hull.
        const std::vector<Point> pts = {
            P<Point>(0, 0),  P<Point>(6, 0),  P<Point>(3, 4),
            P<Point>(3, -4), P<Point>(3, 0),
        };
        pgl::Triangulation tri(pts);
        CHECK(tri.checkInvariants());
        CHECK_FALSE(hasDegenerateTriangle(tri));
        CHECK(strictInCircleViolations(tri, pts) == 0);
        CHECK(allCounterClockwise(tri));
        CHECK(everyPointIsAVertex(tri, pts));
        CHECK(isConforming(tri, pts));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(pts).twiceArea());
    }
}

TEST_CASE("Dense random point sets never yield a degenerate triangle") {
    // Small coordinate ranges make collinear triples (and points landing exactly
    // on a hull edge) common, which is what once made the incremental build
    // re-fan a zero-area triangle for some insertion orders. Sweep many such sets
    // and require every result to stay a valid, conforming Delaunay triangulation
    // with no degenerate triangle. Deterministic generator, so failures repro.
    using Point = pgl::Point<int>;
    std::uint64_t state = 0x243f6a8885a308d3ULL;
    const auto nextInt = [&state](int modulus) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int>((state >> 33) % static_cast<std::uint64_t>(modulus));
    };

    for (int instance = 0; instance < 250; ++instance) {
        const int range = 8 + (instance % 28);  // [8, 35]: dense, collinearity-prone
        std::vector<Point> pts;
        pts.reserve(120);
        for (int i = 0; i < 120; ++i) {
            pts.push_back(P<Point>(nextInt(range + 1), nextInt(range + 1)));
        }
        pgl::Triangulation tri(pts);
        REQUIRE(tri.checkInvariants());
        REQUIRE_FALSE(hasDegenerateTriangle(tri));
        REQUIRE(strictInCircleViolations(tri, pts) == 0);
        REQUIRE(allCounterClockwise(tri));
        REQUIRE(everyPointIsAVertex(tri, pts));
        REQUIRE(isConforming(tri, pts));
    }
}

TEST_CASE_TEMPLATE("Constrained Delaunay of a simple polygon tiles its interior",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    // A non-convex (arrow-head) polygon, given CCW.
    const std::vector<Point> verts = {
        P<Point>(0, 0),  P<Point>(40, 0),  P<Point>(40, 40),
        P<Point>(20, 20), P<Point>(0, 40),
    };
    const pgl::Polygon<Point> poly(verts);
    pgl::Triangulation tri(poly);

    CHECK(tri.checkInvariants());
    CHECK(tri.numTriangles() == verts.size() - 2);  // simple k-gon -> k-2 triangles
    CHECK(allCounterClockwise(tri));
    // The visible triangles cover exactly the polygon interior, not its hull.
    CHECK(totalTwiceArea(tri) == poly.twiceArea());
}

TEST_CASE_TEMPLATE("Constrained Delaunay of a polygon with interior points and segments",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Seg = pgl::Segment<Point>;
    // CCW square; every interior addition stays strictly inside it, so the
    // covered area must always equal the square regardless of the extra vertices.
    const pgl::Polygon<Point> poly(std::vector<Point>{
        P<Point>(0, 0), P<Point>(60, 0), P<Point>(60, 60), P<Point>(0, 60)});
    const auto squareArea = poly.twiceArea();

    SUBCASE("extra interior points become vertices, domain unchanged") {
        const std::vector<Point> pts = {P<Point>(20, 20), P<Point>(40, 30), P<Point>(25, 45)};
        pgl::Triangulation tri(poly, pts);
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 4 + pts.size());
        CHECK(allCounterClockwise(tri));
        CHECK(everyPointIsAVertex(tri, pts));
        CHECK(totalTwiceArea(tri) == squareArea);
    }
    SUBCASE("interior segments are constrained edges and add their endpoints") {
        const std::vector<Seg> segs = {Seg(P<Point>(10, 30), P<Point>(50, 30))};
        pgl::Triangulation tri(poly, segs);
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 4 + 2);
        CHECK(tri.isConstrained(Seg(P<Point>(10, 30), P<Point>(50, 30))));
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == squareArea);
    }
    SUBCASE("points and segments together, boundary stays constrained") {
        const std::vector<Point> pts = {P<Point>(30, 50)};
        const std::vector<Seg> segs = {Seg(P<Point>(15, 15), P<Point>(45, 15)),
                                       Seg(P<Point>(15, 15), P<Point>(30, 50))};
        pgl::Triangulation tri(poly, pts, segs);
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 4 + 3);  // (30,50) shared between pts and a segment
        CHECK(everyPointIsAVertex(tri, pts));
        CHECK(tri.isConstrained(Seg(P<Point>(15, 15), P<Point>(45, 15))));
        CHECK(tri.isConstrained(Seg(P<Point>(15, 15), P<Point>(30, 50))));
        CHECK(tri.isConstrained(Seg(P<Point>(0, 0), P<Point>(60, 0))));
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == squareArea);
    }
}

TEST_CASE("Polygon constraint segments carry their labels onto the edges") {
    using Point = pgl::Point<int>;
    using LabeledSegment = pgl::Segment<Point, std::string>;
    const pgl::Polygon<Point> poly(std::vector<Point>{
        P<Point>(0, 0), P<Point>(60, 0), P<Point>(60, 60), P<Point>(0, 60)});

    std::vector<LabeledSegment> segs;  // parallel, non-crossing interior constraints
    segs.emplace_back(P<Point>(10, 20), P<Point>(50, 20), "lower");
    segs.emplace_back(P<Point>(10, 40), P<Point>(50, 40), "upper");

    pgl::Triangulation tri(poly, segs);
    // CTAD carries the segment label type into the stored edge type.
    static_assert(std::is_same_v<decltype(tri)::SegmentType, LabeledSegment>);
    CHECK(tri.checkInvariants());
    CHECK(tri.isConstrained(LabeledSegment(P<Point>(10, 20), P<Point>(50, 20))));
    CHECK(tri.isConstrained(LabeledSegment(P<Point>(10, 40), P<Point>(50, 40))));

    std::string lower, upper;
    for (const auto& e : tri.edges()) {
        if (e.min() == P<Point>(10, 20) && e.max() == P<Point>(50, 20)) {
            lower = e.label();
        }
        if (e.min() == P<Point>(10, 40) && e.max() == P<Point>(50, 40)) {
            upper = e.label();
        }
    }
    CHECK(lower == "lower");
    CHECK(upper == "upper");

    // The label() accessor returns a reference into the triangulation's own
    // storage: assigning through it changes what every later accessor reports.
    const LabeledSegment lowerEdge(P<Point>(10, 20), P<Point>(50, 20));
    CHECK(tri.label(lowerEdge) == "lower");
    tri.label(lowerEdge) = "RELABELED";
    CHECK(tri.label(lowerEdge) == "RELABELED");
    bool sawRelabeled = false;
    for (const auto& e : tri.edges()) {
        if (e.min() == P<Point>(10, 20) && e.max() == P<Point>(50, 20)) {
            sawRelabeled = (e.label() == "RELABELED");
        }
    }
    CHECK(sawRelabeled);  // the change is visible through edges()
}

TEST_CASE("Triangle labels can be read and mutated in place") {
    using Point = pgl::Point<int>;
    using LabeledTriangle = pgl::Triangle<Point, std::string>;

    std::vector<LabeledTriangle> tris;
    tris.emplace_back(P<Point>(0, 0), P<Point>(40, 0), P<Point>(0, 40), "A");
    tris.emplace_back(P<Point>(40, 0), P<Point>(40, 40), P<Point>(0, 40), "B");

    pgl::Triangulation tri(tris);
    static_assert(std::is_same_v<decltype(tri)::TriangleLabel, std::string>);
    REQUIRE(tri.numTriangles() == 2);

    // Each stored triangle still carries the label it was built with, surfaced
    // both by triangles() and by the label() accessor.
    for (const auto& t : tri.triangles()) {
        CHECK((t.label() == "A" || t.label() == "B"));
        CHECK(tri.label(t) == t.label());
    }

    // Mutating through label() persists and is visible to later accessors.
    const auto first = tri.triangles().front();
    tri.label(first) = "MUTATED";
    CHECK(tri.label(first) == "MUTATED");
    bool sawMutated = false;
    for (const auto& t : tri.triangles()) {
        if (t.a() == first.a() && t.b() == first.b() && t.c() == first.c()) {
            sawMutated = (t.label() == "MUTATED");
        }
    }
    CHECK(sawMutated);
}

TEST_CASE("Locating a point returns the containing triangle") {
    using Point = pgl::Point<int>;
    const std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(60, 0),  P<Point>(60, 50),
        P<Point>(0, 50), P<Point>(25, 30), P<Point>(40, 15),
    };
    pgl::Triangulation tri(pts);

    const Point inside = P<Point>(20, 20);
    const auto located = tri.locate(inside);
    REQUIRE(located.has_value());
    CHECK(located->contains(inside));
    CHECK(tri.has(*located));

    // A point well outside the convex hull has no containing triangle.
    CHECK_FALSE(tri.locate(P<Point>(1000, 1000)).has_value());
}

TEST_CASE("Segment traversal reports exactly the triangles and edges it meets") {
    using Point = pgl::Point<int>;
    const std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(30, 5),  P<Point>(55, 0),  P<Point>(60, 30),
        P<Point>(50, 55), P<Point>(25, 60), P<Point>(0, 50),  P<Point>(20, 25),
        P<Point>(40, 20), P<Point>(35, 40),
    };
    pgl::Triangulation tri(pts);

    const pgl::OrientedSegment<Point> s(5, 10, 55, 45);

    const auto crossed = tri.trianglesIntersecting(s);
    CHECK_FALSE(crossed.empty());
    for (const auto& t : crossed) {
        CHECK(t.intersects(s));   // every reported triangle really meets s
        CHECK(tri.has(t));
    }

    const auto entered = tri.trianglesInteriorIntersecting(s);
    CHECK(entered.size() <= crossed.size());
    for (const auto& t : entered) {
        CHECK(t.interiorsIntersect(s));
    }

    const auto metEdges = tri.edgesIntersecting(s);
    CHECK_FALSE(metEdges.empty());
    for (const auto& e : metEdges) {
        CHECK(s.intersects(e));
    }
}

TEST_CASE("A segment running along a mesh edge stops at its target") {
    using Point = pgl::Point<int>;
    std::vector<Point> pts;
    for (int x = 0; x <= 40; x += 10) {
        for (int y = 0; y <= 40; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const auto all = tri.triangles();

    const auto check = [&](const pgl::OrientedSegment<Point>& s) {
        std::set<std::array<Point, 3>> got, ref;
        for (const auto& t : tri.trianglesIntersecting(s)) {
            got.insert({t[0], t[1], t[2]});
        }
        for (const auto& t : all) {
            if (t.intersects(s)) ref.insert({t[0], t[1], t[2]});
        }
        CHECK(got == ref);
    };

    // Every mesh edge here spans several lattice points, so it can be walked
    // along. A segment collinear with an edge but ending strictly inside it meets
    // only the two triangles sharing that edge: the walk must not hop on to the
    // far endpoint and visit its whole fan. Ending exactly at the far endpoint, it
    // does meet that fan.
    for (const auto& e : tri.edges()) {
        const int dx = e[1].x() - e[0].x();
        const int dy = e[1].y() - e[0].y();
        const int g = std::gcd(dx, dy);
        REQUIRE(g >= 4);
        const Point step(dx / g, dy / g);
        const Point from(e[0].x() + step.x(), e[0].y() + step.y());
        const Point inside(e[0].x() + 2 * step.x(), e[0].y() + 2 * step.y());
        check(pgl::OrientedSegment<Point>(from, inside));    // stops inside the edge
        check(pgl::OrientedSegment<Point>(inside, from));    // and the other way
        check(pgl::OrientedSegment<Point>(from, e[1]));      // stops at the far vertex
    }
}

TEST_CASE("Line, oriented-line, and ray traversal is ordered along the query") {
    using Point = pgl::Point<int>;
    using OSeg = pgl::OrientedSegment<Point>;
    std::vector<Point> pts;
    for (int x = 0; x <= 80; x += 10) {
        for (int y = 0; y <= 80; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const auto all = tri.triangles();

    // A directed query is traced in order along q[0]->q[1]. The reference is the
    // directed segment walk over the same supporting line, clipped to a segment
    // that spans the whole hull (`spanning`): the two must report exactly the
    // same triangles in exactly the same order, and the set must equal the
    // brute-force intersecting set.
    const auto compare = [&](const auto& query, const OSeg& spanning) {
        const auto got = tri.trianglesIntersecting(query);
        CHECK(got == tri.trianglesIntersecting(spanning));  // same triangles, same order
        std::set<std::array<Point, 3>> gotSet, refSet;
        for (const auto& t : got) {
            CHECK(t.intersects(query));
            gotSet.insert({t[0], t[1], t[2]});
        }
        for (const auto& t : all) {
            if (t.intersects(query)) refSet.insert({t[0], t[1], t[2]});
        }
        CHECK(gotSet == refSet);
        return got;
    };
    const int K = 80;  // enough to push a point well past the 80x80 mesh
    // A line/oriented line spans the hull when extended past both defining points.
    const auto checkLine = [&](const auto& line) {
        const Point p0 = line[0], p1 = line[1];
        const Point d(p1.x() - p0.x(), p1.y() - p0.y());
        return compare(line, OSeg(Point(p0.x() - K * d.x(), p0.y() - K * d.y()),
                                  Point(p1.x() + K * d.x(), p1.y() + K * d.y())));
    };
    // A ray keeps its source and extends only forwards.
    const auto checkRay = [&](const pgl::Ray<Point>& ray) {
        const Point s0 = ray[0], s1 = ray[1];
        const Point d(s1.x() - s0.x(), s1.y() - s0.y());
        return compare(ray, OSeg(s0, Point(s0.x() + K * d.x(), s0.y() + K * d.y())));
    };

    SUBCASE("a line with both defining points outside the hull") {
        const auto got = checkLine(pgl::Line<Point>(P<Point>(-8, 5), P<Point>(88, 74)));
        CHECK_FALSE(got.empty());
    }
    SUBCASE("an oriented line whose defining points are inside the hull") {
        // The key case: the line also extends backwards out of the hull past q[0],
        // so the ordered trace must begin at the back-side ghost, not at q[0].
        const auto got = checkLine(pgl::OrientedLine<Point>(P<Point>(30, 28), P<Point>(52, 55)));
        CHECK_FALSE(got.empty());
    }
    SUBCASE("reversing an oriented line reverses the order") {
        auto fwd = checkLine(pgl::OrientedLine<Point>(P<Point>(12, 9), P<Point>(70, 66)));
        auto rev = checkLine(pgl::OrientedLine<Point>(P<Point>(70, 66), P<Point>(12, 9)));
        CHECK_FALSE(fwd.empty());
        std::reverse(rev.begin(), rev.end());
        CHECK(fwd == rev);
    }
    SUBCASE("a ray whose source is inside the hull") {
        const auto got = checkRay(pgl::Ray<Point>(P<Point>(38, 41), P<Point>(73, 64)));
        CHECK_FALSE(got.empty());
    }
    SUBCASE("a ray whose source is outside the hull, pointing in") {
        const auto got = checkRay(pgl::Ray<Point>(P<Point>(-9, 6), P<Point>(11, 19)));
        CHECK_FALSE(got.empty());
    }
    SUBCASE("a ray pointing away from the mesh reports nothing") {
        CHECK(checkRay(pgl::Ray<Point>(P<Point>(120, 41), P<Point>(160, 46))).empty());
    }
    SUBCASE("a line missing the mesh reports nothing") {
        CHECK(tri.trianglesIntersecting(
                     pgl::Line<Point>(P<Point>(-10, 100), P<Point>(100, 130)))
                  .empty());
    }
}

TEST_CASE("Region traversal reports exactly the triangles a shape intersects") {
    using Point = pgl::Point<int>;
    // A 9x9 grid of points: a dense mesh so the flood has to navigate, not scan.
    std::vector<Point> pts;
    for (int x = 0; x <= 80; x += 10) {
        for (int y = 0; y <= 80; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const auto all = tri.triangles();

    // Compares trianglesIntersecting(shape) against the brute-force reference
    // { t : t.intersects(shape) } over every in-domain triangle.
    const auto check = [&](const auto& shape) {
        const auto got = tri.trianglesIntersecting(shape);
        std::set<std::array<Point, 3>> gotSet;
        for (const auto& t : got) {
            CHECK(t.intersects(shape));  // no false positives
            const bool fresh = gotSet.insert({t[0], t[1], t[2]}).second;
            CHECK(fresh);                // no duplicates
        }
        std::set<std::array<Point, 3>> refSet;
        for (const auto& t : all) {
            if (t.intersects(shape)) {
                refSet.insert({t[0], t[1], t[2]});
            }
        }
        CHECK(gotSet == refSet);         // exactly the intersecting set
    };

    SUBCASE("a point interior to the mesh") { check(P<Point>(25, 35)); }
    SUBCASE("a point on a mesh vertex") { check(P<Point>(30, 30)); }
    SUBCASE("a point outside the mesh") {
        CHECK(tri.trianglesIntersecting(P<Point>(-5, -5)).empty());
    }
    SUBCASE("a small triangle inside one cell") {
        check(pgl::Triangle<Point>(P<Point>(11, 11), P<Point>(18, 12), P<Point>(13, 18)));
    }
    SUBCASE("a triangle spanning several cells") {
        check(pgl::Triangle<Point>(P<Point>(5, 5), P<Point>(72, 18), P<Point>(33, 70)));
    }
    SUBCASE("a rectangle inside the mesh") {
        check(pgl::Rectangle<Point>(P<Point>(22, 17), P<Point>(58, 49)));
    }
    SUBCASE("a rectangle covering the whole mesh") {
        check(pgl::Rectangle<Point>(P<Point>(-20, -20), P<Point>(120, 120)));
    }
    SUBCASE("a rectangle straddling the boundary") {
        check(pgl::Rectangle<Point>(P<Point>(-15, 30), P<Point>(25, 55)));
    }
    SUBCASE("a rectangle entirely outside the mesh") {
        CHECK(tri.trianglesIntersecting(pgl::Rectangle<Point>(P<Point>(100, 100),
                                                              P<Point>(140, 140)))
                  .empty());
    }
    SUBCASE("a disk inside the mesh") {
        check(pgl::Disk<Point>(P<Point>(38, 42), 17));
    }
    SUBCASE("a disk covering the whole mesh") {
        check(pgl::Disk<Point>(P<Point>(40, 40), 120));
    }
    SUBCASE("a disk straddling the boundary") {
        check(pgl::Disk<Point>(P<Point>(-5, 35), 28));
    }
    SUBCASE("a disk entirely outside the mesh") {
        CHECK(tri.trianglesIntersecting(pgl::Disk<Point>(P<Point>(160, 160), 20)).empty());
    }
    SUBCASE("a sub-cell disk inside one triangle") {
        check(pgl::Disk<Point>(P<Point>(33, 33), 2));
    }
    SUBCASE("a line crossing the mesh") {
        check(pgl::Line<Point>(P<Point>(-10, 3), P<Point>(90, 77)));
    }
    SUBCASE("a line missing the mesh") {
        CHECK(tri.trianglesIntersecting(pgl::Line<Point>(P<Point>(-10, 100),
                                                         P<Point>(100, 130)))
                  .empty());
    }
    SUBCASE("a ray starting inside the mesh") {
        check(pgl::Ray<Point>(P<Point>(40, 40), P<Point>(95, 70)));
    }
    SUBCASE("a ray pointing away from the mesh") {
        check(pgl::Ray<Point>(P<Point>(120, 40), P<Point>(160, 45)));
    }
    SUBCASE("a half-plane cutting the mesh") {
        check(pgl::Halfplane<Point>(P<Point>(0, 35), P<Point>(80, 45)));
    }
    SUBCASE("a half-plane covering the whole mesh") {
        check(pgl::Halfplane<Point>(P<Point>(0, -50), P<Point>(80, -50)));
    }
}

TEST_CASE("Region traversal supports early-exit and visiting") {
    using Point = pgl::Point<int>;
    std::vector<Point> pts;
    for (int x = 0; x <= 60; x += 10) {
        for (int y = 0; y <= 60; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const pgl::Rectangle<Point> box(P<Point>(15, 15), P<Point>(45, 45));

    // A void visitor sees every triangle the materialized form lists.
    std::size_t visited = 0;
    const bool stoppedAll =
        tri.visitTrianglesIntersecting(box, [&](const pgl::Triangle<Point>&) { ++visited; });
    CHECK_FALSE(stoppedAll);
    CHECK(visited == tri.trianglesIntersecting(box).size());
    CHECK(visited > 0);

    // A visitor that returns true stops the walk after the first triangle.
    std::size_t seen = 0;
    const bool stopped = tri.visitTrianglesIntersecting(box, [&](const pgl::Triangle<Point>&) {
        ++seen;
        return true;
    });
    CHECK(stopped);
    CHECK(seen == 1);
}

TEST_CASE("Region traversal: interior-intersecting and edge variants match brute force") {
    using Point = pgl::Point<int>;
    std::vector<Point> pts;
    for (int x = 0; x <= 80; x += 10) {
        for (int y = 0; y <= 80; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const auto all = tri.triangles();
    std::set<pgl::Segment<Point>> allEdges;
    for (const auto& t : all) {
        for (const auto& e : t.edges()) {
            allEdges.insert(pgl::Segment<Point>(e[0], e[1]));
        }
    }

    // Each derived family query, for a region shape, must equal its brute-force
    // reference over the in-domain triangles / edges.
    const auto check = [&](const auto& shape) {
        std::set<std::array<Point, 3>> tg, tr;
        for (const auto& t : tri.trianglesInteriorIntersecting(shape)) {
            CHECK(t.interiorsIntersect(shape));
            tg.insert({t[0], t[1], t[2]});
        }
        for (const auto& t : all) {
            if (t.interiorsIntersect(shape)) tr.insert({t[0], t[1], t[2]});
        }
        CHECK(tg == tr);

        std::set<pgl::Segment<Point>> eg, er;
        for (const auto& e : tri.edgesIntersecting(shape)) {
            CHECK(shape.intersects(e));
            eg.insert(e);
        }
        for (const auto& e : allEdges) {
            if (shape.intersects(e)) er.insert(e);
        }
        CHECK(eg == er);

        std::set<pgl::Segment<Point>> ig, ir;
        for (const auto& e : tri.edgesInteriorIntersecting(shape)) {
            CHECK(shape.interiorsIntersect(e));
            ig.insert(e);
        }
        for (const auto& e : allEdges) {
            if (shape.interiorsIntersect(e)) ir.insert(e);
        }
        CHECK(ig == ir);
    };

    SUBCASE("triangle") {
        check(pgl::Triangle<Point>(P<Point>(8, 12), P<Point>(67, 22), P<Point>(30, 71)));
    }
    SUBCASE("rectangle") { check(pgl::Rectangle<Point>(P<Point>(17, 23), P<Point>(58, 61))); }
    SUBCASE("convex") {
        check(pgl::Convex<Point>(std::vector<Point>{P<Point>(12, 9), P<Point>(70, 18),
                                                    P<Point>(63, 64), P<Point>(20, 55)}));
    }
    SUBCASE("disk") { check(pgl::Disk<Point>(P<Point>(40, 38), 26)); }
    SUBCASE("disk straddling the boundary") { check(pgl::Disk<Point>(P<Point>(-4, 30), 25)); }
    SUBCASE("line") { check(pgl::Line<Point>(P<Point>(-8, 5), P<Point>(88, 74))); }
    SUBCASE("ray") { check(pgl::Ray<Point>(P<Point>(38, 41), P<Point>(92, 70))); }
    SUBCASE("half-plane") { check(pgl::Halfplane<Point>(P<Point>(0, 33), P<Point>(80, 47))); }
}

TEST_CASE("Chain traversal reports exactly the triangles and edges a chain meets") {
    using Point = pgl::Point<int>;
    std::vector<Point> pts;
    for (int x = 0; x <= 80; x += 10) {
        for (int y = 0; y <= 80; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const auto all = tri.triangles();
    std::set<pgl::Segment<Point>> allEdges;
    for (const auto& t : all) {
        for (const auto& e : t.edges()) {
            allEdges.insert(pgl::Segment<Point>(e[0], e[1]));
        }
    }

    // A chain is traced edge by edge: the triangles must be exactly the
    // brute-force intersecting set, each reported once, in the order the chain
    // first meets them (the concatenated per-edge segment walks, duplicates
    // dropped). The derived families must match their brute-force references too.
    const auto check = [&](const auto& chain) {
        const auto got = tri.trianglesIntersecting(chain);
        std::set<std::array<Point, 3>> gotSet, refSet;
        for (const auto& t : got) {
            CHECK(t.intersects(chain));                        // no false positives
            CHECK(gotSet.insert({t[0], t[1], t[2]}).second);   // no duplicates
        }
        for (const auto& t : all) {
            if (t.intersects(chain)) refSet.insert({t[0], t[1], t[2]});
        }
        CHECK(gotSet == refSet);

        std::vector<pgl::Triangle<Point>> ordered;
        std::set<std::array<Point, 3>> once;
        for (const auto& e : chain.orientedEdges()) {
            for (const auto& t : tri.trianglesIntersecting(e)) {
                if (once.insert({t[0], t[1], t[2]}).second) ordered.push_back(t);
            }
        }
        CHECK(got == ordered);  // same triangles, in chain order

        std::set<std::array<Point, 3>> ig, ir;
        for (const auto& t : tri.trianglesInteriorIntersecting(chain)) {
            CHECK(t.interiorsIntersect(chain));
            ig.insert({t[0], t[1], t[2]});
        }
        for (const auto& t : all) {
            if (t.interiorsIntersect(chain)) ir.insert({t[0], t[1], t[2]});
        }
        CHECK(ig == ir);

        std::set<pgl::Segment<Point>> eg, er;
        for (const auto& e : tri.edgesIntersecting(chain)) {
            CHECK(chain.intersects(e));
            eg.insert(e);
        }
        for (const auto& e : allEdges) {
            if (chain.intersects(e)) er.insert(e);
        }
        CHECK(eg == er);

        std::set<pgl::Segment<Point>> xg, xr;
        for (const auto& e : tri.edgesInteriorIntersecting(chain)) {
            CHECK(chain.interiorsIntersect(e));
            xg.insert(e);
        }
        for (const auto& e : allEdges) {
            if (chain.interiorsIntersect(e)) xr.insert(e);
        }
        CHECK(xg == xr);
        return got;
    };

    SUBCASE("a polyline zigzagging inside the mesh") {
        const pgl::Polyline<Point> pl(std::vector<Point>{P<Point>(5, 7), P<Point>(35, 62),
                                                         P<Point>(62, 13), P<Point>(74, 71)});
        CHECK_FALSE(check(pl).empty());
    }
    SUBCASE("a polyline leaving the hull and coming back") {
        // The middle vertex is well outside: the walk must exit and re-enter,
        // which a single connected flood fill could not do.
        const pgl::Polyline<Point> pl(std::vector<Point>{P<Point>(10, 15), P<Point>(120, 40),
                                                         P<Point>(20, 70)});
        CHECK_FALSE(check(pl).empty());
    }
    SUBCASE("a polyline turning back into triangles it already met") {
        // The last edge retraces the region of the first: every triangle stays
        // reported exactly once.
        const pgl::Polyline<Point> pl(std::vector<Point>{P<Point>(12, 12), P<Point>(63, 47),
                                                         P<Point>(66, 44), P<Point>(15, 9)});
        CHECK_FALSE(check(pl).empty());
    }
    SUBCASE("a polyline running along mesh edges and through vertices") {
        const pgl::Polyline<Point> pl(std::vector<Point>{P<Point>(20, 20), P<Point>(50, 20),
                                                         P<Point>(50, 60), P<Point>(30, 40)});
        CHECK_FALSE(check(pl).empty());
    }
    SUBCASE("a polyline entirely outside the mesh") {
        const pgl::Polyline<Point> pl(std::vector<Point>{P<Point>(-40, 100), P<Point>(20, 120),
                                                         P<Point>(90, 105)});
        CHECK(check(pl).empty());
    }
    SUBCASE("a single-vertex polyline is the point query") {
        const pgl::Polyline<Point> pl(std::vector<Point>{P<Point>(25, 35)});
        CHECK(tri.trianglesIntersecting(pl) ==
              tri.trianglesIntersecting(P<Point>(25, 35)));
        CHECK_FALSE(tri.trianglesIntersecting(pl).empty());
    }
    SUBCASE("a monotone chain crossing the mesh") {
        const pgl::MonotoneChain<Point> mc(std::vector<Point>{P<Point>(3, 9), P<Point>(27, 55),
                                                              P<Point>(48, 18), P<Point>(76, 66)});
        CHECK_FALSE(check(mc).empty());
    }
    SUBCASE("a monotone chain starting and ending outside the hull") {
        const pgl::MonotoneChain<Point> mc(std::vector<Point>{
            P<Point>(-30, 20), P<Point>(40, 45), P<Point>(115, 30)});
        CHECK_FALSE(check(mc).empty());
    }
    SUBCASE("a monotone chain entirely outside the mesh") {
        const pgl::MonotoneChain<Point> mc(
            std::vector<Point>{P<Point>(-40, 100), P<Point>(20, 130), P<Point>(90, 105)});
        CHECK(check(mc).empty());
    }
}

TEST_CASE("Chain traversal supports early-exit and visiting") {
    using Point = pgl::Point<int>;
    std::vector<Point> pts;
    for (int x = 0; x <= 60; x += 10) {
        for (int y = 0; y <= 60; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);
    const pgl::Polyline<Point> pl(
        std::vector<Point>{P<Point>(5, 8), P<Point>(48, 25), P<Point>(22, 55)});

    std::size_t visited = 0;
    const bool stoppedAll =
        tri.visitTrianglesIntersecting(pl, [&](const pgl::Triangle<Point>&) { ++visited; });
    CHECK_FALSE(stoppedAll);
    CHECK(visited == tri.trianglesIntersecting(pl).size());
    CHECK(visited > 0);

    // Stopping on the first triangle stops the whole chain, not just its edge.
    std::size_t seen = 0;
    const bool stopped = tri.visitTrianglesIntersecting(pl, [&](const pgl::Triangle<Point>&) {
        ++seen;
        return true;
    });
    CHECK(stopped);
    CHECK(seen == 1);

    // The edge variants stop early through the same chain walk.
    std::size_t edges = 0;
    const bool stoppedEdges = tri.visitEdgesIntersecting(pl, [&](const pgl::Segment<Point>&) {
        ++edges;
        return edges == 2;
    });
    CHECK(stoppedEdges);
    CHECK(edges == 2);
}

TEST_CASE("Navigation: adjacency and the segment-to-edge bridge are consistent") {
    using Point = pgl::Point<int>;
    const std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(40, 0),  P<Point>(40, 40),
        P<Point>(0, 40), P<Point>(18, 22),
    };
    pgl::Triangulation tri(pts);

    for (const auto& t : tri.triangles()) {
        // Each neighbor across a shared edge sees t back across the same edge.
        for (const auto& nb : tri.edgeAdjacentTriangles(t)) {
            bool mutual = false;
            for (const auto& edge : t.edges()) {
                const pgl::Segment<Point> e(edge[0], edge[1]);
                const auto other = tri.otherTriangle(t, e);
                if (other.has_value() && *other == nb) {
                    mutual = true;
                    const auto back = tri.otherTriangle(nb, e);
                    REQUIRE(back.has_value());
                    CHECK(*back == t);
                }
            }
            CHECK(mutual);
        }
    }

    // incidentTriangles(vertex) returns exactly the fan of triangles at each
    // vertex: every returned triangle has the vertex, and none is missed.
    for (const auto& p : pts) {
        std::set<pgl::Triangle<Point>> expected;
        for (const auto& t : tri.triangles()) {
            for (const auto& v : t.vertices()) {
                if (v == p) {
                    expected.insert(t);
                }
            }
        }
        const auto fan = tri.incidentTriangles(p);
        std::set<pgl::Triangle<Point>> got(fan.begin(), fan.end());
        CHECK(got.size() == fan.size());  // no duplicates
        CHECK(got == expected);
        for (const auto& t : fan) {
            bool hasVertex = false;
            for (const auto& v : t.vertices()) {
                hasVertex = hasVertex || (v == p);
            }
            CHECK(hasVertex);
        }
    }

    // A point that is not a vertex of the triangulation has an empty fan.
    CHECK(tri.incidentTriangles(P<Point>(100, 100)).empty());  // outside the hull
    CHECK(tri.incidentTriangles(P<Point>(20, 20)).empty());    // interior, not a vertex

    // vertexAdjacentTriangles(t) is exactly the set of other triangles sharing
    // a vertex with t, listed once each, and a superset of edgeAdjacentTriangles.
    for (const auto& t : tri.triangles()) {
        std::set<pgl::Triangle<Point>> expected;
        for (const auto& u : tri.triangles()) {
            if (u == t) continue;
            for (const auto& vu : u.vertices()) {
                for (const auto& vt : t.vertices()) {
                    if (vu == vt) {
                        expected.insert(u);
                    }
                }
            }
        }
        const auto around = tri.vertexAdjacentTriangles(t);
        std::set<pgl::Triangle<Point>> got(around.begin(), around.end());
        CHECK(got.size() == around.size());  // no duplicates
        CHECK(got == expected);
        for (const auto& nb : tri.edgeAdjacentTriangles(t)) {
            CHECK(got.count(nb) == 1);
        }
    }

    // A triangle not part of the mesh yields nothing.
    CHECK(tri.vertexAdjacentTriangles(
                 pgl::Triangle<Point>(P<Point>(0, 0), P<Point>(40, 0), P<Point>(40, 40)))
              .empty());
}

TEST_CASE("Flipping an interior edge yields the other diagonal and keeps the mesh valid") {
    using Point = pgl::Point<int>;
    // A convex quadrilateral (not cocircular): exactly one interior diagonal.
    const std::vector<Point> pts = {
        P<Point>(0, 0), P<Point>(10, 0), P<Point>(12, 7), P<Point>(2, 9),
    };
    pgl::Triangulation tri(pts);
    REQUIRE(tri.numTriangles() == 2);
    const auto areaBefore = totalTwiceArea(tri);

    // The single diagonal is the only edge with two incident triangles.
    std::optional<pgl::Segment<Point>> diagonal;
    for (const auto& e : tri.edges()) {
        if (tri.incidentTriangles(e).size() == 2) {
            diagonal = e;
        }
    }
    REQUIRE(diagonal.has_value());
    REQUIRE(tri.flippable(*diagonal));

    const auto flipped = tri.flip(*diagonal);
    REQUIRE(flipped.has_value());
    CHECK_FALSE(*flipped == *diagonal);   // it is the opposite diagonal
    CHECK(tri.checkInvariants());
    CHECK(tri.numTriangles() == 2);
    CHECK(totalTwiceArea(tri) == areaBefore);  // same region, retriangulated
    CHECK(tri.has(*flipped));
    CHECK_FALSE(tri.has(*diagonal));
}

TEST_CASE("Flipping a set of edges in parallel requires disjoint quadrilaterals") {
    using Point = pgl::Point<int>;
    using Seg = pgl::Segment<Point>;
    std::vector<Point> pts;
    for (int x = 0; x <= 50; x += 10) {
        for (int y = 0; y <= 50; y += 10) {
            pts.push_back(P<Point>(x, y));
        }
    }
    pgl::Triangulation tri(pts);

    SUBCASE("an empty set is trivially flippable and flips to nothing") {
        const std::vector<Seg> none;
        CHECK(tri.flippable(none));
        const auto res = tri.flip(none);
        REQUIRE(res.has_value());
        CHECK(res->empty());
    }

    SUBCASE("a maximal set of triangle-disjoint flippable edges flips at once") {
        // Greedily pick flippable edges whose quads share no triangle.
        std::vector<Seg> chosen;
        std::set<std::array<Point, 3>> usedTris;
        for (const auto& e : tri.edges()) {
            if (!tri.flippable(e)) continue;
            const auto inc = tri.incidentTriangles(e);
            if (inc.size() != 2) continue;
            const std::array<Point, 3> a{inc[0][0], inc[0][1], inc[0][2]};
            const std::array<Point, 3> b{inc[1][0], inc[1][1], inc[1][2]};
            if (usedTris.count(a) || usedTris.count(b)) continue;
            usedTris.insert(a);
            usedTris.insert(b);
            chosen.push_back(e);
        }
        REQUIRE(chosen.size() >= 2);
        CHECK(tri.flippable(chosen));

        const auto areaBefore = totalTwiceArea(tri);
        const auto res = tri.flip(chosen);
        REQUIRE(res.has_value());
        CHECK(res->size() == chosen.size());
        CHECK(tri.checkInvariants());
        CHECK(totalTwiceArea(tri) == areaBefore);  // same region, retriangulated
        for (std::size_t i = 0; i < chosen.size(); ++i) {
            CHECK_FALSE(tri.has(chosen[i]));  // original diagonal gone
            CHECK(tri.has((*res)[i]));        // replaced by the new diagonal
        }
    }

    SUBCASE("two edges sharing a triangle cannot flip in parallel") {
        // Two edges of one triangle: their quads share that triangle.
        std::vector<Seg> conflicting;
        for (const auto& t : tri.triangles()) {
            std::vector<Seg> f;
            for (const auto& e : t.edges()) {
                const Seg seg(e[0], e[1]);
                if (tri.flippable(seg)) f.push_back(seg);
            }
            if (f.size() >= 2) {
                conflicting = {f[0], f[1]};
                break;
            }
        }
        REQUIRE(conflicting.size() == 2);

        const auto areaBefore = totalTwiceArea(tri);
        CHECK_FALSE(tri.flippable(conflicting));
        CHECK_FALSE(tri.flip(conflicting).has_value());  // all-or-nothing: no change
        CHECK(totalTwiceArea(tri) == areaBefore);
        for (const auto& e : conflicting) {
            CHECK(tri.has(e));  // both edges still present
        }
    }

    SUBCASE("a repeated edge is rejected") {
        Seg some;
        for (const auto& e : tri.edges()) {
            if (tri.flippable(e)) {
                some = e;
                break;
            }
        }
        const std::vector<Seg> twice{some, some};
        CHECK_FALSE(tri.flippable(twice));
        CHECK_FALSE(tri.flip(twice).has_value());
    }
}

TEST_CASE_TEMPLATE("Inserting a point subdivides the containing triangle or edge",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Seg = pgl::Segment<Point>;
    // A square: its Delaunay triangulation is two triangles along one of the
    // diagonals (the corners are cocircular, so either diagonal may be picked;
    // the checks below hold for both).
    const std::vector<Point> pts = {P<Point>(0, 0), P<Point>(8, 0), P<Point>(8, 8),
                                    P<Point>(0, 8)};
    pgl::Triangulation tri(pts);
    const auto hullArea = pgl::Convex<Point>(pts).twiceArea();
    REQUIRE(tri.numTriangles() == 2);

    SUBCASE("strictly inside a triangle: one triangle becomes three") {
        // (2,1) is strictly inside the lower triangle of either diagonal.
        CHECK(tri.insert(P<Point>(2, 1)));
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 5);
        CHECK(tri.numTriangles() == 4);
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == hullArea);
        CHECK(tri.incidentTriangles(P<Point>(2, 1)).size() == 3);
        CHECK(tri.triangles().size() == tri.numTriangles());
        CHECK(tri.edges().size() == tri.numEdges());
    }

    SUBCASE("on an interior edge: both incident triangles split") {
        // (4,4) is the midpoint of both possible diagonals.
        CHECK(tri.insert(P<Point>(4, 4)));
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 5);
        CHECK(tri.numTriangles() == 4);
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == hullArea);
        CHECK(tri.incidentTriangles(P<Point>(4, 4)).size() == 4);
        // The split diagonal is gone, replaced by its two halves.
        CHECK_FALSE(tri.has(Seg(P<Point>(0, 0), P<Point>(8, 8))));
        CHECK_FALSE(tri.has(Seg(P<Point>(8, 0), P<Point>(0, 8))));
        const bool halvesOfEither =
            (tri.has(Seg(P<Point>(0, 0), P<Point>(4, 4))) &&
             tri.has(Seg(P<Point>(4, 4), P<Point>(8, 8)))) ||
            (tri.has(Seg(P<Point>(8, 0), P<Point>(4, 4))) &&
             tri.has(Seg(P<Point>(4, 4), P<Point>(0, 8))));
        CHECK(halvesOfEither);
    }

    SUBCASE("on a hull edge: the hull gains a vertex, one new triangle") {
        CHECK(tri.insert(P<Point>(4, 0)));
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 5);
        CHECK(tri.numTriangles() == 3);
        CHECK(allCounterClockwise(tri));
        CHECK(totalTwiceArea(tri) == hullArea);
        CHECK_FALSE(tri.has(Seg(P<Point>(0, 0), P<Point>(8, 0))));
        CHECK(tri.has(Seg(P<Point>(0, 0), P<Point>(4, 0))));
        CHECK(tri.has(Seg(P<Point>(4, 0), P<Point>(8, 0))));
        CHECK(tri.incidentTriangles(P<Point>(4, 0)).size() == 2);
    }

    SUBCASE("a duplicate vertex is rejected and changes nothing") {
        CHECK_FALSE(tri.insert(P<Point>(0, 0)));
        CHECK(tri.numVertices() == 4);
        CHECK(tri.numTriangles() == 2);
        CHECK(tri.checkInvariants());
        CHECK(totalTwiceArea(tri) == hullArea);
    }

    SUBCASE("outside the hull: one new triangle per visible hull edge") {
        // (12,-4) strictly sees two hull edges; the pocket is fanned to it.
        CHECK(tri.insert(P<Point>(12, -4)));
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 5);
        CHECK(tri.numTriangles() == 4);
        CHECK(allCounterClockwise(tri));
        std::vector<Point> grown = pts;
        grown.push_back(P<Point>(12, -4));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(grown).twiceArea());
        CHECK(tri.incidentTriangles(P<Point>(12, -4)).size() == 2);
        CHECK(tri.has(Seg(P<Point>(8, 0), P<Point>(12, -4))));  // spoke
        CHECK(tri.has(Seg(P<Point>(0, 0), P<Point>(12, -4))));  // new hull edge
        CHECK(tri.has(Seg(P<Point>(8, 8), P<Point>(12, -4))));  // new hull edge
    }

    SUBCASE("outside and collinear with a hull edge: no degenerate triangle") {
        // (12,0) extends the hull edge (0,0)-(8,0) beyond its endpoint: that
        // edge is not strictly visible, only (8,0)-(8,8) is.
        CHECK(tri.insert(P<Point>(12, 0)));
        CHECK(tri.checkInvariants());
        CHECK_FALSE(hasDegenerateTriangle(tri));
        CHECK(tri.numTriangles() == 3);
        CHECK(allCounterClockwise(tri));
        std::vector<Point> grown = pts;
        grown.push_back(P<Point>(12, 0));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(grown).twiceArea());
    }

    SUBCASE("flips and traversals still work after insertions") {
        REQUIRE(tri.insert(P<Point>(2, 1)));
        REQUIRE(tri.insert(P<Point>(4, 4)));
        bool flippedOne = false;
        for (const auto& e : tri.edges()) {
            if (tri.flippable(e)) {
                CHECK(tri.flip(e).has_value());
                flippedOne = true;
                break;
            }
        }
        CHECK(flippedOne);
        CHECK(tri.checkInvariants());
        CHECK(totalTwiceArea(tri) == hullArea);
        // A segment traversal across the mesh sees consistent triangles.
        std::size_t seen = 0;
        tri.visitTrianglesIntersecting(Seg(P<Point>(1, 1), P<Point>(7, 7)),
                                       [&](const auto& t) { (void)t; ++seen; });
        CHECK(seen > 0);
    }
}

TEST_CASE_TEMPLATE("insertDelaunay keeps the triangulation Delaunay",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    // General-position start; (0,0)-(40,2) stays a hull edge with the lattice
    // point (20,1) strictly inside it, exercising the hull-edge split.
    std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(40, 2),  P<Point>(38, 39), P<Point>(2, 35),
        P<Point>(17, 11), P<Point>(9, 27), P<Point>(28, 30),
    };
    pgl::Triangulation tri(pts);
    REQUIRE(strictInCircleViolations(tri, pts) == 0);

    // Interior points, a point on the hull edge (0,0)-(40,2), then points
    // outside the hull (growing it).
    const std::vector<Point> extra = {P<Point>(20, 20), P<Point>(13, 17),
                                      P<Point>(30, 12), P<Point>(24, 4),
                                      P<Point>(20, 1),  P<Point>(45, 20),
                                      P<Point>(-5, -6), P<Point>(20, -60)};
    for (const auto& q : extra) {
        CHECK(tri.insertDelaunay(q));
        pts.push_back(q);
        CHECK(tri.checkInvariants());
        CHECK(strictInCircleViolations(tri, pts) == 0);  // defining property
        CHECK(allCounterClockwise(tri));
        CHECK(everyPointIsAVertex(tri, pts));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(pts).twiceArea());
    }
    CHECK(tri.numVertices() == pts.size());
    // The duplicate rejection also holds for insertDelaunay.
    CHECK_FALSE(tri.insertDelaunay(P<Point>(20, 20)));  // already a vertex
    CHECK_FALSE(tri.insertDelaunay(P<Point>(45, 20)));  // a hull vertex now
}

TEST_CASE_TEMPLATE("Splitting a hull edge survives the ghost relocation",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    // A hull-edge split frees a real slot by relocating the lowest-indexed
    // ghost, which invalidates that ghost's id. Splitting the hull edge whose
    // ghost is exactly the relocated one used to alias the freed slot and
    // scramble the connectivity; here it takes four splits to line up.
    std::vector<Point> pts = {P<Point>(0, 0), P<Point>(2, 0), P<Point>(2, 2),
                              P<Point>(0, 2), P<Point>(1, 1)};
    pgl::Triangulation tri(pts);
    REQUIRE(tri.checkInvariants());
    const auto hullArea = totalTwiceArea(tri);

    for (const auto& q : {P<Point>(1, 0), P<Point>(2, 1), P<Point>(1, 2), P<Point>(0, 1)}) {
        CHECK(tri.insertDelaunay(q));
        pts.push_back(q);
        CHECK(tri.checkInvariants());
        CHECK(allCounterClockwise(tri));
        CHECK(everyPointIsAVertex(tri, pts));
        CHECK(totalTwiceArea(tri) == hullArea);  // an edge split adds no area
    }
    CHECK(tri.numVertices() == pts.size());
}

TEST_CASE("Insertion respects constrained edges and the polygon domain") {
    using Point = pgl::Point<int>;
    using Seg = pgl::Segment<Point>;
    // CCW polygon with a reflex vertex at (4,4): the region between the notch
    // and the hull edge (8,8)-(0,8) is triangulated but out of domain.
    const pgl::Polygon<Point> poly(std::vector<Point>{
        P<Point>(0, 0), P<Point>(8, 0), P<Point>(8, 8), P<Point>(4, 4), P<Point>(0, 8)});
    pgl::Triangulation tri(poly);
    const auto polyArea = poly.twiceArea();
    REQUIRE(totalTwiceArea(tri) == polyArea);

    SUBCASE("inserting inside the polygon keeps the domain tiling") {
        CHECK(tri.insert(P<Point>(4, 2)));
        CHECK(tri.checkInvariants());
        CHECK(totalTwiceArea(tri) == polyArea);
        CHECK(allCounterClockwise(tri));
    }

    SUBCASE("splitting a constrained edge keeps both halves constrained") {
        const Seg boundary(P<Point>(4, 4), P<Point>(0, 8));  // domain/fill border
        REQUIRE(tri.isConstrained(boundary));
        const auto before = tri.numTriangles();
        CHECK(tri.insert(P<Point>(2, 6)));
        CHECK(tri.checkInvariants());
        CHECK_FALSE(tri.has(boundary));
        CHECK(tri.isConstrained(Seg(P<Point>(4, 4), P<Point>(2, 6))));
        CHECK(tri.isConstrained(Seg(P<Point>(2, 6), P<Point>(0, 8))));
        CHECK(tri.numTriangles() == before + 1);  // the fill side stays hidden
        CHECK(totalTwiceArea(tri) == polyArea);
    }

    SUBCASE("insertDelaunay preserves every polygon boundary constraint") {
        CHECK(tri.insertDelaunay(P<Point>(3, 2)));
        CHECK(tri.insertDelaunay(P<Point>(6, 3)));
        CHECK(tri.checkInvariants());
        CHECK(totalTwiceArea(tri) == polyArea);
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const Seg e(poly[i], poly[(i + 1) % poly.size()]);
            CHECK(tri.has(e));
            CHECK(tri.isConstrained(e));
        }
    }
}

TEST_CASE("Insertion preserves surviving edge labels and propagates split ones") {
    using Point = pgl::Point<int>;
    using LabeledSegment = pgl::Segment<Point, std::string>;
    const pgl::Polygon<Point> poly(std::vector<Point>{
        P<Point>(0, 0), P<Point>(60, 0), P<Point>(60, 60), P<Point>(0, 60)});
    std::vector<LabeledSegment> segs;
    segs.emplace_back(P<Point>(10, 30), P<Point>(50, 30), "wall");
    pgl::Triangulation tri(poly, segs);
    const LabeledSegment wall(P<Point>(10, 30), P<Point>(50, 30));
    REQUIRE(tri.isConstrained(wall));
    REQUIRE(tri.label(wall) == "wall");

    // Splitting the labeled constrained edge: both halves keep flag and label.
    CHECK(tri.insert(P<Point>(30, 30)));
    CHECK(tri.checkInvariants());
    const LabeledSegment left(P<Point>(10, 30), P<Point>(30, 30));
    const LabeledSegment right(P<Point>(30, 30), P<Point>(50, 30));
    CHECK(tri.isConstrained(left));
    CHECK(tri.isConstrained(right));
    CHECK(tri.label(left) == "wall");
    CHECK(tri.label(right) == "wall");

    // An insertion elsewhere must not wipe the labels of edges it re-points.
    tri.label(left) = "left-wall";
    CHECK(tri.insert(P<Point>(20, 25)));  // strictly below the wall
    CHECK(tri.checkInvariants());
    CHECK(tri.label(left) == "left-wall");
    CHECK(tri.label(right) == "wall");
}

TEST_CASE_TEMPLATE("Conforming constrained Delaunay of points plus segments",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Seg = pgl::Segment<Point>;
    // Points in general position; the constraint's endpoints are extra vertices.
    const std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(40, 3),  P<Point>(37, 38), P<Point>(3, 34),
        P<Point>(18, 12), P<Point>(10, 25), P<Point>(27, 28),
    };
    const std::vector<Seg> segs = {Seg(P<Point>(8, 18), P<Point>(33, 17))};
    pgl::Triangulation tri(pts, segs);

    CHECK(tri.checkInvariants());
    CHECK(tri.numVertices() == pts.size() + 2);
    CHECK(tri.isConstrained(segs.front()));
    CHECK(allCounterClockwise(tri));
    // Nothing is carved away: the domain is the whole hull.
    std::vector<Point> all = pts;
    all.push_back(P<Point>(8, 18));
    all.push_back(P<Point>(33, 17));
    CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(all).twiceArea());

    // The constraint survives Delaunay insertions next to it, and hull growth
    // still works (the hull is unconstrained).
    CHECK(tri.insertDelaunay(P<Point>(20, 19)));
    CHECK(tri.insertDelaunay(P<Point>(20, -9)));
    CHECK(tri.checkInvariants());
    CHECK(tri.isConstrained(segs.front()));
}

TEST_CASE("Constrained hull edges do not block hull growth of a point-set triangulation") {
    using Point = pgl::Point<int>;
    using Seg = pgl::Segment<Point>;
    const std::vector<Point> corners = {P<Point>(0, 0), P<Point>(8, 0), P<Point>(8, 8),
                                        P<Point>(0, 8)};

    SUBCASE("all hull edges constrained via the points-plus-segments constructor") {
        std::vector<Seg> boundary;
        for (std::size_t i = 0; i < corners.size(); ++i) {
            boundary.emplace_back(corners[i], corners[(i + 1) % corners.size()]);
        }
        pgl::Triangulation tri(corners, boundary);
        REQUIRE(tri.numTriangles() == 2);
        for (const auto& e : boundary) {
            CHECK(tri.isConstrained(e));
        }

        // (12,4) strictly sees only the constrained hull edge (8,0)-(8,8):
        // growth proceeds and the edge stays constrained, now interior.
        CHECK(tri.insert(P<Point>(12, 4)));
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 5);
        CHECK(tri.numTriangles() == 3);
        CHECK(tri.isConstrained(Seg(P<Point>(8, 0), P<Point>(8, 8))));
        CHECK_FALSE(tri.flippable(Seg(P<Point>(8, 0), P<Point>(8, 8))));
        std::vector<Point> grown = corners;
        grown.push_back(P<Point>(12, 4));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(grown).twiceArea());
        CHECK(tri.insert(P<Point>(3, 4)));  // interior insertion still fine
        CHECK(tri.checkInvariants());
    }

    SUBCASE("one hull edge constrained by hand, pocket spanning it and an open edge") {
        pgl::Triangulation tri(corners);
        const Seg bottom(P<Point>(0, 0), P<Point>(8, 0));
        tri.setConstrained(bottom);
        REQUIRE(tri.isConstrained(bottom));

        // (12,-4) strictly sees the constrained bottom and the open right edge.
        CHECK(tri.insertDelaunay(P<Point>(12, -4)));
        CHECK(tri.checkInvariants());
        CHECK(tri.numVertices() == 5);
        CHECK(tri.isConstrained(bottom));  // kept, now an interior edge
        std::vector<Point> grown = corners;
        grown.push_back(P<Point>(12, -4));
        CHECK(totalTwiceArea(tri) == pgl::Convex<Point>(grown).twiceArea());
        CHECK(allCounterClockwise(tri));
    }
}

TEST_CASE("Traversal reports every triangle met, however the query meets the hull") {
    using Point = pgl::Point<int>;
    // Vertices sit on the hull boundary and mesh edges run along the axes, so these
    // queries enter through hull *vertices*, run along mesh edges, and pass beyond
    // their own defining points — the cases a walk steered "towards b" gets wrong.
    const pgl::Polygon<Point> poly({P<Point>(0, 0), P<Point>(6, 0), P<Point>(6, 2),
                                    P<Point>(2, 2), P<Point>(2, 4), P<Point>(6, 4),
                                    P<Point>(6, 6), P<Point>(0, 6)});
    pgl::Triangulation tri(poly);

    // Brute force: the walk must report exactly the triangles the query intersects.
    const auto reportsExactly = [&](const auto& query) {
        auto walked = tri.trianglesIntersecting(query);
        std::sort(walked.begin(), walked.end());
        std::vector<pgl::Triangle<Point>> expected;
        for (const auto& t : tri.triangles()) {
            if (t.intersects(query)) {
                expected.push_back(t);
            }
        }
        std::sort(expected.begin(), expected.end());
        return walked == expected;
    };

    // Entering the hull exactly at a hull vertex, not through an edge's interior.
    CHECK(reportsExactly(pgl::Segment<Point>(P<Point>(4, 8), P<Point>(7, 5))));
    CHECK(reportsExactly(pgl::Ray<Point>(P<Point>(8, 2), P<Point>(6, 2))));
    // Running along a mesh edge, and along the hull boundary itself.
    CHECK(reportsExactly(pgl::Line<Point>(P<Point>(2, -1), P<Point>(2, 0))));
    CHECK(reportsExactly(pgl::Line<Point>(P<Point>(6, 0), P<Point>(6, 1))));
    CHECK(reportsExactly(pgl::Segment<Point>(P<Point>(0, 0), P<Point>(6, 0))));
    // Continuing past the second defining point: forward is the direction a->b.
    CHECK(reportsExactly(pgl::Line<Point>(P<Point>(-2, 6), P<Point>(2, 4))));
    CHECK(reportsExactly(pgl::Ray<Point>(P<Point>(8, 2), P<Point>(3, 2))));
    CHECK(reportsExactly(pgl::Ray<Point>(P<Point>(6, 4), P<Point>(2, 2))));
    // Missing the hull entirely, and grazing a single hull vertex.
    CHECK(reportsExactly(pgl::Segment<Point>(P<Point>(8, 8), P<Point>(9, 9))));
    CHECK(reportsExactly(pgl::Line<Point>(P<Point>(6, 8), P<Point>(8, 6))));

    // The walk is a const query: it must be deterministic (it caches a hint).
    const pgl::Ray<Point> ray(P<Point>(8, 2), P<Point>(3, 2));
    CHECK(tri.trianglesIntersecting(ray) == tri.trianglesIntersecting(ray));
}

TEST_CASE_TEMPLATE("Domain predicates agree with the polygon triangulated", Point,
                   pgl::Point<int>, pgl::Point<pgl::Rational<long>>) {
    // A "C": the notch x in [2,6], y in [2,4] is outside the polygon but inside
    // its convex hull, so it is covered by out-of-domain hull-fill triangles —
    // the triangles the domain predicates must treat as exterior.
    const pgl::Polygon<Point> poly({P<Point>(0, 0), P<Point>(6, 0), P<Point>(6, 2),
                                    P<Point>(2, 2), P<Point>(2, 4), P<Point>(6, 4),
                                    P<Point>(6, 6), P<Point>(0, 6)});
    pgl::Triangulation tri(poly);

    // Exhaustive agreement over a grid reaching outside the hull: points,
    // segments, and the rectangles they span.
    for (int x1 = -1; x1 <= 7; ++x1) {
        for (int y1 = -1; y1 <= 7; ++y1) {
            const Point a = P<Point>(x1, y1);
            agreesWithPolygon(tri, poly, a);
            for (int x2 = -1; x2 <= 7; ++x2) {
                for (int y2 = -1; y2 <= 7; ++y2) {
                    const Point b = P<Point>(x2, y2);
                    if (a == b) {
                        continue;
                    }
                    agreesWithPolygon(tri, poly, pgl::Segment<Point>(a, b));
                    if (a.x() != b.x() && a.y() != b.y()) {  // a flat rectangle is degenerate
                        agreesWithPolygon(tri, poly, pgl::Rectangle<Point>(a, b));
                    }
                }
            }
        }
    }

    // The shapes taking three points, plus the unbounded ones, over a coarser grid.
    for (int x1 = -1; x1 <= 7; x1 += 2) {
        for (int y1 = -1; y1 <= 7; y1 += 2) {
            for (int x2 = -1; x2 <= 7; x2 += 2) {
                for (int y2 = -1; y2 <= 7; y2 += 2) {
                    const Point a = P<Point>(x1, y1);
                    const Point b = P<Point>(x2, y2);
                    if (a == b) {
                        continue;
                    }
                    agreesWithPolygon(tri, poly, pgl::OrientedSegment<Point>(a, b));
                    agreesWithPolygon(tri, poly, pgl::Line<Point>(a, b));
                    agreesWithPolygon(tri, poly, pgl::Ray<Point>(a, b));
                    agreesWithPolygon(tri, poly, pgl::Halfplane<Point>(a, b));
                    for (int x3 = -1; x3 <= 7; x3 += 2) {
                        for (int y3 = -1; y3 <= 7; y3 += 2) {
                            const Point c = P<Point>(x3, y3);
                            if (pgl::orientationSign(a, b, c) == 0) {
                                continue;  // degenerate triangle/disk/polygon
                            }
                            agreesWithPolygon(tri, poly, pgl::Triangle<Point>(a, b, c));
                            agreesWithPolygon(tri, poly, pgl::Disk<Point>(a, b, c));
                            agreesWithPolygon(tri, poly,
                                              pgl::Convex<Point>(std::vector<Point>{a, b, c}));
                            agreesWithPolygon(tri, poly,
                                              pgl::Polygon<Point>(std::vector<Point>{a, b, c}));
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("Containment is closed: the domain boundary is contained, the fill is not") {
    using Point = pgl::Point<int>;
    using Seg = pgl::Segment<Point>;
    const pgl::Polygon<Point> poly({P<Point>(0, 0), P<Point>(6, 0), P<Point>(6, 2),
                                    P<Point>(2, 2), P<Point>(2, 4), P<Point>(6, 4),
                                    P<Point>(6, 6), P<Point>(0, 6)});
    pgl::Triangulation tri(poly);

    // On the boundary: contained, even though the walk sweeps the hull-fill
    // triangles on the far side of these edges and the fans of their vertices.
    CHECK(tri.contains(Seg(P<Point>(2, 2), P<Point>(6, 2))));  // a polygon edge
    CHECK(tri.contains(Seg(P<Point>(3, 2), P<Point>(5, 2))));  // inside one
    CHECK(tri.contains(Seg(P<Point>(2, 2), P<Point>(2, 4))));  // the notch's back wall
    CHECK(tri.contains(Seg(P<Point>(0, 3), P<Point>(2, 2))));  // ends on a reflex vertex
    CHECK(tri.contains(Seg(P<Point>(1, 1), P<Point>(1, 5))));  // strictly interior
    // Through a reflex vertex and back inside: the fan there is half hull-fill.
    CHECK(tri.contains(Seg(P<Point>(0, 4), P<Point>(2, 2))));

    // Leaving the domain, in each of the ways the walk can see it happen.
    CHECK_FALSE(tri.contains(Seg(P<Point>(1, 3), P<Point>(5, 3))));  // through the notch
    CHECK_FALSE(tri.contains(Seg(P<Point>(2, 2), P<Point>(6, 4))));  // along a fill diagonal
    CHECK_FALSE(tri.contains(Seg(P<Point>(6, 2), P<Point>(6, 4))));  // along the notch's mouth
    CHECK_FALSE(tri.contains(Seg(P<Point>(1, 1), P<Point>(9, 1))));  // out through the hull
    CHECK_FALSE(tri.contains(P<Point>(4, 3)));                       // in the notch
    CHECK_FALSE(tri.contains(P<Point>(9, 9)));                       // outside the hull

    // A triangle spanning the notch has all three vertices in the domain.
    CHECK_FALSE(tri.contains(pgl::Triangle<Point>(P<Point>(2, 2), P<Point>(6, 2), P<Point>(6, 4))));

    // Chains: contained iff every edge is.
    CHECK(tri.contains(pgl::Polyline<Point>(
        std::vector<Point>{P<Point>(1, 1), P<Point>(1, 5), P<Point>(2, 4)})));
    CHECK_FALSE(tri.contains(pgl::Polyline<Point>(
        std::vector<Point>{P<Point>(1, 1), P<Point>(1, 5), P<Point>(5, 3)})));
}

TEST_CASE("The domain predicates split boundary contact from interior contact") {
    using Point = pgl::Point<int>;
    using Seg = pgl::Segment<Point>;
    const pgl::Polygon<Point> poly({P<Point>(0, 0), P<Point>(6, 0), P<Point>(6, 2),
                                    P<Point>(2, 2), P<Point>(2, 4), P<Point>(6, 4),
                                    P<Point>(6, 6), P<Point>(0, 6)});
    pgl::Triangulation tri(poly);

    SUBCASE("a segment on a polygon edge is contained, but not by the interior") {
        const Seg onEdge(P<Point>(2, 2), P<Point>(6, 2));
        CHECK(tri.contains(onEdge));
        CHECK(tri.intersects(onEdge));
        CHECK_FALSE(tri.interiorContains(onEdge));    // it lies in the boundary
        CHECK_FALSE(tri.interiorsIntersect(onEdge));  // and so does its interior
    }

    SUBCASE("a segment ending on the boundary keeps its interior inside") {
        const Seg toEdge(P<Point>(1, 3), P<Point>(0, 3));
        CHECK(tri.contains(toEdge));
        CHECK(tri.intersects(toEdge));
        CHECK_FALSE(tri.interiorContains(toEdge));  // the endpoint touches ∂D
        CHECK(tri.interiorsIntersect(toEdge));      // its relative interior does not
    }

    SUBCASE("strictly inside satisfies all four") {
        const Seg inside(P<Point>(1, 1), P<Point>(1, 5));
        CHECK(tri.contains(inside));
        CHECK(tri.intersects(inside));
        CHECK(tri.interiorContains(inside));
        CHECK(tri.interiorsIntersect(inside));
    }

    SUBCASE("vertices are in the domain but never in its interior") {
        CHECK(tri.contains(P<Point>(2, 2)));  // the reflex vertex
        CHECK(tri.intersects(P<Point>(2, 2)));
        CHECK_FALSE(tri.interiorContains(P<Point>(2, 2)));
        CHECK_FALSE(tri.interiorsIntersect(P<Point>(2, 2)));
        CHECK(tri.interiorContains(P<Point>(1, 1)));  // strictly inside
        CHECK(tri.interiorsIntersect(P<Point>(1, 1)));
    }

    SUBCASE("crossing the notch: met, but not contained") {
        const Seg through(P<Point>(1, 3), P<Point>(5, 3));
        CHECK_FALSE(tri.contains(through));
        CHECK(tri.intersects(through));
        CHECK(tri.interiorsIntersect(through));
        CHECK_FALSE(tri.interiorContains(through));
    }

    SUBCASE("shapes that miss the domain entirely") {
        const Seg away(P<Point>(7, 7), P<Point>(9, 9));
        CHECK_FALSE(tri.intersects(away));
        CHECK_FALSE(tri.interiorsIntersect(away));
        CHECK_FALSE(tri.contains(away));
        CHECK_FALSE(tri.interiorContains(away));
        const Seg inTheNotch(P<Point>(3, 3), P<Point>(5, 3));  // inside the hull, outside the polygon
        CHECK_FALSE(tri.intersects(inTheNotch));
        CHECK_FALSE(tri.interiorsIntersect(inTheNotch));
    }

    SUBCASE("unbounded and empty shapes") {
        const pgl::Line<Point> line(P<Point>(0, 1), P<Point>(1, 1));  // cuts the polygon
        CHECK(tri.intersects(line));
        CHECK(tri.interiorsIntersect(line));
        CHECK_FALSE(tri.contains(line));
        CHECK_FALSE(tri.interiorContains(line));

        const pgl::Shape<Point> empty;
        CHECK(tri.contains(empty));
        CHECK(tri.interiorContains(empty));
        CHECK_FALSE(tri.intersects(empty));
        CHECK_FALSE(tri.interiorsIntersect(empty));
    }
}

TEST_CASE("Containment on a hull domain, and of unbounded and empty shapes") {
    using Point = pgl::Point<int>;
    const std::vector<Point> pts = {P<Point>(0, 0), P<Point>(8, 0), P<Point>(8, 8),
                                    P<Point>(0, 8), P<Point>(4, 3)};
    pgl::Triangulation tri(pts);  // the domain is the convex hull: no fill triangles

    CHECK(tri.contains(pgl::Segment<Point>(P<Point>(0, 0), P<Point>(8, 8))));  // a diagonal
    CHECK(tri.contains(pgl::Segment<Point>(P<Point>(0, 0), P<Point>(8, 0))));  // a hull edge
    CHECK(tri.contains(pgl::Segment<Point>(P<Point>(1, 1), P<Point>(7, 2))));
    CHECK_FALSE(tri.contains(pgl::Segment<Point>(P<Point>(4, 4), P<Point>(9, 4))));  // out
    CHECK(tri.contains(pgl::Rectangle<Point>(P<Point>(1, 1), P<Point>(7, 7))));
    CHECK_FALSE(tri.contains(pgl::Rectangle<Point>(P<Point>(1, 1), P<Point>(9, 7))));

    // Unbounded shapes never fit in a bounded domain; the empty shape always does.
    CHECK_FALSE(tri.contains(pgl::Line<Point>(P<Point>(1, 1), P<Point>(2, 2))));
    CHECK_FALSE(tri.contains(pgl::Ray<Point>(P<Point>(1, 1), P<Point>(2, 2))));
    CHECK_FALSE(tri.contains(pgl::Halfplane<Point>(P<Point>(1, 1), P<Point>(2, 2))));
    CHECK(tri.contains(pgl::Shape<Point>()));

    // The variant dispatches to the same answers as the concrete shapes.
    const pgl::Shape<Point> inside(pgl::Segment<Point>(P<Point>(1, 1), P<Point>(7, 2)));
    const pgl::Shape<Point> outside(pgl::Segment<Point>(P<Point>(4, 4), P<Point>(9, 4)));
    CHECK(tri.contains(inside));
    CHECK_FALSE(tri.contains(outside));

    // An empty triangulation contains nothing.
    const pgl::Triangulation<pgl::Triangle<Point>> none;
    CHECK_FALSE(none.contains(P<Point>(0, 0)));
    CHECK_FALSE(none.contains(pgl::Segment<Point>(P<Point>(0, 0), P<Point>(1, 1))));
}

TEST_CASE("Triangulation predicates dispatch concrete compound region queries") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using Set = pgl::PolygonSet<Point>;
    using Intersection = pgl::HalfplaneIntersection<Point>;

    const Polygon domain({0, 0, 10, 0, 10, 10, 0, 10});
    const auto tri = domain.triangulation();
    const Region region(Polygon({1, 1, 5, 1, 5, 5, 1, 5}),
                        std::vector{Polygon({2, 2, 4, 2, 4, 4, 2, 4})});
    const Set set(std::vector{region,
                              Region(Polygon({6, 6, 9, 6, 9, 9, 6, 9}))});
    const Intersection bounded(pgl::Rectangle<Point>(P<Point>(1, 1), P<Point>(5, 5)));

    const auto checkInside = [&]<class Q>(const Q& query) {
        CHECK(tri.contains(query));
        CHECK(tri.interiorContains(query));
        CHECK(tri.intersects(query));
        CHECK(tri.interiorsIntersect(query));

        // The benchmark calls the concrete overload. Shape erasure must reach
        // the same overload instead of visiting back into itself.
        const pgl::Shape<Point> erased(query);
        CHECK(tri.contains(erased));
        CHECK(tri.interiorContains(erased));
        CHECK(tri.intersects(erased));
        CHECK(tri.interiorsIntersect(erased));
    };
    checkInside(region);
    checkInside(set);
    checkInside(bounded);

    const Intersection wholePlane;
    CHECK_FALSE(tri.contains(wholePlane));
    CHECK_FALSE(tri.interiorContains(wholePlane));
    CHECK(tri.intersects(wholePlane));
    CHECK(tri.interiorsIntersect(wholePlane));
}

// ---------------------------------------------------------------------------
// Regions with holes. The domain is the part of the region that has area, so
// the in-domain triangles tile it exactly: their areas sum to the region's, no
// triangle escapes it, and no hole is covered.

// Every ring goes in as constrained edges and the hole interiors are carved out,
// so the triangles tile the region exactly.
template <class Region>
static void tilesTheRegion(const Region& region) {
    using Q = pgl::Rational<long long>;
    REQUIRE(region.isValid());
    const auto tri = region.triangulation();
    typename Region::NumberType twice = 0;
    for (const auto& t : tri.triangles()) {
        CHECK_FALSE(t.isDegenerate());
        twice += t.twiceArea();
        for (const auto& edge : t.edges()) {
            CHECK(region.contains(edge));
        }
        CHECK(region.interiorContains(t.template pointInside<Q>()));
    }
    CHECK(twice == region.twiceArea());
}

TEST_CASE("A region with holes triangulates to its material only") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    const Polygon outer({0, 0, 8, 0, 8, 8, 0, 8});

    SUBCASE("an annulus") {
        const Region region(outer, std::vector{Polygon({2, 2, 6, 2, 6, 6, 2, 6})});
        const auto tri = region.triangulation();
        CHECK(tri.numTriangles() == 8);  // one quad ring, two triangles per side
        CHECK_FALSE(tri.contains(P<Point>(4, 4)));   // the hole is not in the domain
        CHECK(tri.contains(P<Point>(1, 1)));         // the material is
        CHECK(tri.contains(P<Point>(2, 2)));         // and so is the hole's boundary
        tilesTheRegion(region);
    }

    SUBCASE("two holes") {
        tilesTheRegion(Region(outer, std::vector{Polygon({1, 1, 3, 1, 3, 3, 1, 3}),
                                                 Polygon({5, 5, 7, 5, 7, 7, 5, 7})}));
    }

    SUBCASE("a hole touching the outer ring at one point") {
        tilesTheRegion(Region(outer, std::vector{Polygon({4, 0, 6, 2, 2, 2})}));
    }

    SUBCASE("two holes meeting at a corner") {
        tilesTheRegion(Region(outer, std::vector{Polygon({1, 1, 4, 1, 4, 4, 1, 4}),
                                                 Polygon({4, 4, 7, 4, 7, 7, 4, 7})}));
    }

    SUBCASE("holes sharing a whole edge") {
        tilesTheRegion(Region(outer, std::vector{Polygon({1, 1, 4, 1, 4, 7, 1, 7}),
                                                 Polygon({4, 1, 7, 1, 7, 7, 4, 7})}));
    }

    SUBCASE("a nonconvex outer ring") {
        tilesTheRegion(Region(Polygon({0, 0, 8, 0, 8, 8, 4, 4, 0, 8}),
                              std::vector{Polygon({1, 1, 3, 1, 3, 3, 1, 3})}));
    }

    SUBCASE("extra interior points and constraint segments") {
        const Region region(outer, std::vector{Polygon({2, 2, 6, 2, 6, 6, 2, 6})});
        const std::vector<Point> points = {P<Point>(1, 4)};
        const std::vector<pgl::Segment<Point>> segments = {
            pgl::Segment<Point>(P<Point>(7, 1), P<Point>(7, 7))};
        const pgl::Triangulation withPoints(region, points);
        const pgl::Triangulation withSegments(region, segments);
        const pgl::Triangulation withBoth(region, points, segments);
        CHECK(withPoints.numVertices() == 9);
        CHECK(withSegments.isConstrained(segments[0]));
        CHECK(withBoth.numVertices() == 11);
        CHECK(withBoth.isConstrained(segments[0]));
    }
}

// A slit — a hole ring running along another ring — belongs to the region but
// has no area, so it carries no triangle: the domain is the closure of the
// region's interior rather than the region itself.
TEST_CASE("Slits belong to the region but not to the triangulated domain") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    const Polygon outer({0, 0, 8, 0, 8, 8, 0, 8});

    SUBCASE("a hole sharing two edges leaves an L") {
        const Region region(outer, std::vector{Polygon({0, 0, 4, 0, 4, 4, 0, 4})});
        const auto tri = region.triangulation();
        tilesTheRegion(region);
        CHECK(tri.numTriangles() == 4);
        CHECK_FALSE(tri.contains(P<Point>(2, 2)));  // inside the hole
        CHECK(tri.contains(P<Point>(4, 4)));        // the hole's free corner
        CHECK(tri.contains(P<Point>(6, 6)));        // the material
        // The slit is in the region, but no triangle covers it.
        CHECK(region.contains(P<Point>(2, 0)));
        CHECK_FALSE(tri.contains(P<Point>(2, 0)));
    }

    SUBCASE("a hole spanning the region splits it in two") {
        const Region region(outer, std::vector{Polygon({0, 3, 8, 3, 8, 5, 0, 5})});
        const auto tri = region.triangulation();
        tilesTheRegion(region);
        CHECK(tri.contains(P<Point>(4, 1)));
        CHECK(tri.contains(P<Point>(4, 7)));
        CHECK_FALSE(tri.contains(P<Point>(4, 4)));
        CHECK(region.contains(P<Point>(0, 4)));      // a whisker of the region
        CHECK_FALSE(tri.contains(P<Point>(0, 4)));   // with no area to triangulate
    }
}

TEST_CASE("Domain predicates on a region agree with the region itself") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    // No slit anywhere, so the region equals the closure of its interior and the
    // two must answer identically. The hole touches the outer ring at (4,0),
    // which keeps the pinch cases in the sweep.
    const Region region(Polygon({0, 0, 6, 0, 6, 6, 0, 6}),
                        std::vector{Polygon({4, 0, 5, 3, 2, 3})});
    const auto tri = region.triangulation();

    for (int x1 = -1; x1 <= 7; ++x1) {
        for (int y1 = -1; y1 <= 7; ++y1) {
            const Point a = P<Point>(x1, y1);
            CHECK(tri.contains(a) == region.contains(a));
            CHECK(tri.intersects(a) == region.intersects(a));
            CHECK(tri.interiorContains(a) == region.interiorContains(a));
            for (int x2 = -1; x2 <= 7; ++x2) {
                for (int y2 = -1; y2 <= 7; ++y2) {
                    const Point b = P<Point>(x2, y2);
                    if (a == b) {
                        continue;
                    }
                    const pgl::Segment<Point> s(a, b);
                    CHECK(tri.contains(s) == region.contains(s));
                    CHECK(tri.intersects(s) == region.intersects(s));
                    CHECK(tri.interiorContains(s) == region.interiorContains(s));
                    CHECK(tri.interiorsIntersect(s) == region.interiorsIntersect(s));
                }
            }
        }
    }

    // The unbounded operands, over a coarser grid.
    for (int x1 = -1; x1 <= 7; x1 += 2) {
        for (int y1 = -1; y1 <= 7; y1 += 2) {
            for (int x2 = -1; x2 <= 7; x2 += 2) {
                for (int y2 = -1; y2 <= 7; y2 += 2) {
                    const Point a = P<Point>(x1, y1);
                    const Point b = P<Point>(x2, y2);
                    if (a == b) {
                        continue;
                    }
                    for (const auto& shape : {pgl::Line<Point>(a, b)}) {
                        CHECK(tri.intersects(shape) == region.intersects(shape));
                        CHECK(tri.interiorsIntersect(shape) == region.interiorsIntersect(shape));
                    }
                    const pgl::Ray<Point> ray(a, b);
                    CHECK(tri.intersects(ray) == region.intersects(ray));
                    CHECK(tri.interiorsIntersect(ray) == region.interiorsIntersect(ray));
                }
            }
        }
    }
}

TEST_CASE("A boundary inside a holed domain can still enclose a hole") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    // The reduction "a closed boundary inside the domain encloses nothing
    // outside it" needs a simply connected domain; a region is not one, and the
    // hole witnesses are what restore the answer.
    const Region region(Polygon({0, 0, 10, 0, 10, 10, 0, 10}),
                        std::vector{Polygon({4, 4, 6, 4, 6, 6, 4, 6})});
    const auto tri = region.triangulation();

    const pgl::Rectangle<Point> around(P<Point>(1, 1), P<Point>(9, 9));
    CHECK(tri.intersects(around));
    CHECK_FALSE(tri.contains(around));  // its edges are in the material, the hole is not

    const pgl::Rectangle<Point> clear(P<Point>(1, 1), P<Point>(3, 3));
    CHECK(tri.contains(clear));

    const pgl::Triangle<Point> overHole(P<Point>(1, 1), P<Point>(9, 1), P<Point>(5, 9));
    CHECK_FALSE(tri.contains(overHole));

    const pgl::Polygon<Point> ring({1, 1, 9, 1, 9, 9, 1, 9});
    CHECK_FALSE(tri.contains(ring));

    // Compound regions take the same witness path: their own boundary can be
    // clear while their interior still covers the domain hole.
    const Region holedQuery(ring);
    CHECK_FALSE(tri.contains(holedQuery));
    CHECK_FALSE(tri.interiorContains(holedQuery));
    const pgl::HalfplaneIntersection<Point> convexQuery(around);
    CHECK_FALSE(tri.contains(convexQuery));
    CHECK_FALSE(tri.interiorContains(convexQuery));

    // A polygon threading around the hole keeps clear of it and is contained.
    const pgl::Polygon<Point> beside({1, 1, 3, 1, 3, 9, 1, 9});
    CHECK(tri.contains(beside));
}

TEST_CASE("A hole can split the domain, and every query must see both pieces") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    // A band hole leaves a bottom slab [0,12]x[0,4] and a top slab [0,12]x[8,12].
    // The queries below have their whole boundary inside the hole or outside the
    // square, so no edge of theirs meets the domain and the answer comes from the
    // fallback — which used to consult whichever triangle came first, and so
    // spoke for one slab only.
    const Region split(Polygon({0, 0, 12, 0, 12, 12, 0, 12}),
                       std::vector{Polygon({0, 4, 12, 4, 12, 8, 0, 8})});
    const auto tri = split.triangulation();

    const pgl::Rectangle<Point> overTop(P<Point>(-2, 7), P<Point>(14, 14));
    const pgl::Rectangle<Point> overBottom(P<Point>(-2, -2), P<Point>(14, 5));
    for (const auto& shape : {overTop, overBottom}) {
        CHECK(tri.intersects(shape));
        CHECK(tri.interiorsIntersect(shape));
        CHECK_FALSE(tri.contains(shape));  // the domain does not hold the query
    }

    const pgl::Polygon<Point> polygonOverTop({-2, 7, 14, 7, 14, 14, -2, 14});
    CHECK(tri.intersects(polygonOverTop));
    CHECK(tri.interiorsIntersect(polygonOverTop));

    const pgl::Triangle<Point> triangleOverTop(P<Point>(-30, 7), P<Point>(40, 7), P<Point>(5, 60));
    CHECK(tri.intersects(triangleOverTop));
    CHECK(tri.interiorsIntersect(triangleOverTop));

    // A query lying strictly inside the hole meets neither slab.
    const pgl::Rectangle<Point> inTheHole(P<Point>(2, 5), P<Point>(10, 7));
    CHECK_FALSE(tri.intersects(inTheHole));
    CHECK_FALSE(tri.interiorsIntersect(inTheHole));

    // Containment the other way round still works: a shape inside one slab is
    // held by the domain.
    CHECK(tri.contains(pgl::Rectangle<Point>(P<Point>(1, 1), P<Point>(11, 3))));
}

TEST_CASE("Points-plus-segments constructor keeps segment labels") {
    using Point = pgl::Point<int>;
    using LabeledSegment = pgl::Segment<Point, std::string>;
    const std::vector<Point> pts = {P<Point>(0, 0), P<Point>(60, 0), P<Point>(60, 60),
                                    P<Point>(0, 60)};
    std::vector<LabeledSegment> segs;
    segs.emplace_back(P<Point>(10, 20), P<Point>(50, 20), "wall");
    pgl::Triangulation tri(pts, segs);
    static_assert(std::is_same_v<decltype(tri)::SegmentType, LabeledSegment>);
    const LabeledSegment wall(P<Point>(10, 20), P<Point>(50, 20));
    CHECK(tri.isConstrained(wall));
    CHECK(tri.label(wall) == "wall");
}

// ---------------------------------------------------------------------------
// convexPartition

namespace {

// Every piece is convex, non-degenerate, and inside the shape; together they
// cover exactly its area. Area is the decisive check: pieces with disjoint
// interiors summing to the shape's own area can neither miss part of it nor
// overlap, and cannot stray outside a shape they are contained in.
template <class Shape, class Pieces>
void checkPartitions(const Shape& shape, const Pieces& pieces) {
    REQUIRE_FALSE(pieces.empty());
    auto total = shape.twiceArea();
    total -= total;  // a zero of the shape's own area type
    for (const auto& piece : pieces) {
        // Re-hulling must be the identity: the pieces are built `trusted`, so
        // this is what says they really are in canonical convex-hull form.
        using PiecePoint = std::remove_cvref_t<decltype(*piece.begin())>;
        const std::vector<PiecePoint> verts(piece.begin(), piece.end());
        CHECK(pgl::Convex<PiecePoint>(verts) == piece);
        CHECK_FALSE(piece.isDegenerate());
        CHECK(shape.contains(piece));
        total += piece.twiceArea();
    }
    CHECK(total == shape.twiceArea());
}

}  // namespace

TEST_CASE_TEMPLATE("convexPartition covers a polygon with convex pieces",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    SUBCASE("a convex polygon is one piece") {
        const pgl::Polygon<Point> square(std::vector<Point>{
            P<Point>(0, 0), P<Point>(4, 0), P<Point>(4, 4), P<Point>(0, 4)});
        const auto pieces = square.convexPartition();
        CHECK(pieces.size() == 1);
        checkPartitions(square, pieces);
    }

    SUBCASE("an L has one reflex vertex and splits in two") {
        const pgl::Polygon<Point> ell(std::vector<Point>{
            P<Point>(0, 0), P<Point>(3, 0), P<Point>(3, 1),
            P<Point>(1, 1), P<Point>(1, 3), P<Point>(0, 3)});
        const auto pieces = ell.convexPartition();
        CHECK(pieces.size() == 2);  // one reflex vertex needs one cut
        checkPartitions(ell, pieces);
    }

    SUBCASE("a comb keeps fewer pieces than the triangulation") {
        // Four teeth pointing down from a bar: 8 reflex vertices.
        std::vector<Point> verts{P<Point>(0, 0)};
        for (int i = 0; i < 4; ++i) {
            const int x = 4 * i;
            verts.push_back(P<Point>(x + 1, 0));
            verts.push_back(P<Point>(x + 1, 3));
            verts.push_back(P<Point>(x + 3, 3));
            verts.push_back(P<Point>(x + 3, 0));
        }
        verts.push_back(P<Point>(16, 0));
        verts.push_back(P<Point>(16, 6));
        verts.push_back(P<Point>(0, 6));
        const pgl::Polygon<Point> comb(verts);
        const auto pieces = comb.convexPartition();
        checkPartitions(comb, pieces);
        CHECK(pieces.size() < comb.triangulation().numTriangles());
    }
}

TEST_CASE_TEMPLATE("convexPartition covers a region without covering its holes",
                   Point, pgl::Point<int>, pgl::Point<pgl::Rational<int64_t>>) {
    using Poly = pgl::Polygon<Point>;
    const Poly outer(std::vector<Point>{P<Point>(0, 0), P<Point>(9, 0),
                                        P<Point>(9, 9), P<Point>(0, 9)});
    std::vector<Poly> holes{Poly(std::vector<Point>{P<Point>(3, 3), P<Point>(6, 3),
                                                    P<Point>(6, 6), P<Point>(3, 6)})};
    const pgl::PolygonWithHoles<Point> region(outer, holes);
    const auto pieces = region.convexPartition();
    checkPartitions(region, pieces);
    // The hole is where there is no piece.
    const Point inHole = P<Point>(4, 4);
    for (const auto& piece : pieces) {
        CHECK_FALSE(piece.interiorContains(inHole));
    }
}

TEST_CASE("convexPartition respects constrained edges") {
    using Point = pgl::Point<int>;
    using Seg = pgl::Segment<Point>;
    // A square that would be one convex piece, cut in half by a constraint.
    const pgl::Polygon<Point> square({0, 0, 4, 0, 4, 4, 0, 4});
    CHECK(square.convexPartition().size() == 1);
    const std::vector<Seg> wall{Seg(Point(0, 0), Point(4, 4))};
    const auto pieces = square.triangulation(wall).convexPartition();
    CHECK(pieces.size() == 2);  // the constraint is never merged across
    checkPartitions(square, pieces);
}

TEST_CASE("convexPartition of an empty triangulation is empty") {
    using Point = pgl::Point<int>;
    const pgl::Triangulation<pgl::Triangle<Point>> tri;
    CHECK(tri.convexPartition().empty());
}

// ---------------------------------------------------------------------------
// convexCovering

namespace {

template <class Shape, class Triangulation, class Pieces>
void checkCovers(const Shape& shape, const Triangulation& triangulation,
                 const Pieces& pieces) {
    REQUIRE_FALSE(pieces.empty());
    const auto triangles = triangulation.triangles();

    for (const auto& piece : pieces) {
        using PiecePoint = std::remove_cvref_t<decltype(*piece.begin())>;
        const std::vector<PiecePoint> vertices(piece.begin(), piece.end());
        CHECK(pgl::Convex<PiecePoint>(vertices) == piece);
        CHECK_FALSE(piece.isDegenerate());
        CHECK(shape.contains(piece));
    }
    for (const auto& triangle : triangles) {
        CHECK(std::ranges::any_of(pieces, [&](const auto& piece) {
            return piece.contains(triangle);
        }));
    }

    // Redundancy removal leaves every selected piece with a triangle that no
    // other selected piece covers.
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        CHECK(std::ranges::any_of(triangles, [&](const auto& triangle) {
            if (!pieces[i].contains(triangle)) {
                return false;
            }
            for (std::size_t j = 0; j < pieces.size(); ++j) {
                if (j != i && pieces[j].contains(triangle)) {
                    return false;
                }
            }
            return true;
        }));
    }
}

}  // namespace

// Two unrelated graphs answer to the name. The detail helper exercised below is
// the paper's dual construction behind convexCovering(): its vertices are
// *triangles*, joined when one is fully visible from the other. The public
// Triangulation::visibilityGraph is the ordinary one, over *vertices*. Pin the
// difference down so neither is reached for by mistake.
static_assert(std::same_as<decltype(pgl::detail::ConvexCoverBuilder::visibilityGraph(
                               std::declval<const pgl::Triangulation<
                                   pgl::Triangle<pgl::Point<int>>>&>())),
                           pgl::Graph<pgl::Triangle<pgl::Point<int>>>>);
static_assert(std::same_as<decltype(std::declval<const pgl::Triangulation<
                                        pgl::Triangle<pgl::Point<int>>>&>()
                                        .visibilityGraph()),
                           pgl::Graph<pgl::Point<int>>>);

TEST_CASE("Triangulation visibility graph follows fully visible dual components") {
    using Point = pgl::Point<int>;

    const pgl::Triangulation<pgl::Triangle<Point>> empty;
    CHECK(pgl::detail::ConvexCoverBuilder::visibilityGraph(empty).vertexCount() == 0);

    const pgl::Polygon<Point> ell(std::vector<Point>{
        Point(0, 0), Point(4, 0), Point(4, 1),
        Point(1, 1), Point(1, 4), Point(0, 4)});
    const auto triangulation = ell.triangulation();
    const auto triangles = triangulation.triangles();
    const auto graph = pgl::detail::ConvexCoverBuilder::visibilityGraph(triangulation);

    CHECK(graph.vertexCount() == static_cast<int>(triangles.size()));
    for (const auto& triangle : triangles) {
        CHECK(graph.containsVertex(triangle));
    }

    for (std::size_t i = 0; i < triangles.size(); ++i) {
        for (std::size_t j = i + 1; j < triangles.size(); ++j) {
            const auto& a = triangles[i];
            const auto& b = triangles[j];
            const std::array<Point, 6> vertices{
                a.a(), a.b(), a.c(), b.a(), b.b(), b.c()
            };
            const bool fullyVisible = triangulation.contains(pgl::Convex<Point>(vertices));
            // The paper's fast construction returns a subgraph: an edge is
            // always a full-visibility certificate, but a pair outside the
            // source's fully visible dual component may be omitted.
            if (graph.containsEdge(a, b)) {
                CHECK(fullyVisible);
            }
        }
    }

    // In a simple polygon (no holes), every triangle clique has a valid hull.
    for (const auto& clique : graph.cliqueCover()) {
        std::vector<Point> vertices;
        for (const auto& triangle : clique) {
            vertices.push_back(triangle.a());
            vertices.push_back(triangle.b());
            vertices.push_back(triangle.c());
        }
        CHECK(ell.contains(pgl::Convex<Point>(vertices)));
    }

    // In a convex domain every triangle is fully visible from every source, so
    // each dual BFS reaches the whole triangulation and the graph is complete.
    const pgl::Polygon<Point> rectangle(std::vector<Point>{
        Point(0, 0), Point(6, 0), Point(6, 4), Point(4, 4),
        Point(2, 4), Point(0, 4)});
    const auto convexTriangulation = rectangle.triangulation();
    const auto convexGraph =
        pgl::detail::ConvexCoverBuilder::visibilityGraph(convexTriangulation);
    const int triangleCount = static_cast<int>(convexTriangulation.numTriangles());
    CHECK(convexGraph.vertexCount() == triangleCount);
    CHECK(convexGraph.edgeCount() == triangleCount * (triangleCount - 1) / 2);

    // The fast variant intentionally need not return the complete visibility
    // graph: these triangles have one fully visible pair separated in the dual
    // graph by a triangle that is not fully visible from the same source.
    const pgl::Polygon<Point> winding(std::vector<Point>{
        Point(675, 0), Point(373, 373), Point(0, 916), Point(-569, 569),
        Point(-704, 0), Point(-366, -366), Point(0, -979), Point(583, -583)});
    const auto windingTriangulation = winding.triangulation();
    const auto windingTriangles = windingTriangulation.triangles();
    const auto windingGraph =
        pgl::detail::ConvexCoverBuilder::visibilityGraph(windingTriangulation);
    int fullEdgeCount = 0;
    for (std::size_t i = 0; i < windingTriangles.size(); ++i) {
        for (std::size_t j = i + 1; j < windingTriangles.size(); ++j) {
            const auto& a = windingTriangles[i];
            const auto& b = windingTriangles[j];
            const std::array<Point, 6> vertices{
                a.a(), a.b(), a.c(), b.a(), b.b(), b.c()
            };
            const bool fullyVisible =
                windingTriangulation.contains(pgl::Convex<Point>(vertices));
            fullEdgeCount += fullyVisible;
            if (windingGraph.containsEdge(a, b)) {
                CHECK(fullyVisible);
            }
        }
    }
    CHECK(windingGraph.edgeCount() < fullEdgeCount);
}

TEST_CASE_TEMPLATE("convexCovering covers every original triangle",
                   Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    SUBCASE("a convex polygon needs one covering piece") {
        const pgl::Polygon<Point> square(std::vector<Point>{
            P<Point>(0, 0), P<Point>(4, 0), P<Point>(4, 4), P<Point>(0, 4)});
        const auto triangulation = square.triangulation();
        const auto pieces = triangulation.convexCovering();
        CHECK(pieces.size() == 1);
        CHECK(square.convexCovering() == pieces);
        checkCovers(square, triangulation, pieces);
    }

    SUBCASE("a non-convex polygon is covered by irredundant convexes") {
        const pgl::Polygon<Point> ell(std::vector<Point>{
            P<Point>(0, 0), P<Point>(3, 0), P<Point>(3, 1),
            P<Point>(1, 1), P<Point>(1, 3), P<Point>(0, 3)});
        const auto triangulation = ell.triangulation();
        const auto grownPieces = triangulation.convexCovering();
        CHECK(grownPieces.size() <= triangulation.convexPartition().size());
        checkCovers(ell, triangulation, grownPieces);

        const auto cliquePieces = ell.convexCovering();
        checkCovers(ell, triangulation, cliquePieces);
    }
}

TEST_CASE("convexCovering may use overlapping candidates") {
    using Point = pgl::Point<int>;
    const pgl::Polygon<Point> star(std::vector<Point>{
        Point(8, 0), Point(9, 9), Point(0, 9), Point(-4, 4),
        Point(-5, 0), Point(-5, -5), Point(0, -2), Point(8, -8)});
    const auto triangulation = star.triangulation();
    const auto pieces = triangulation.convexCovering();
    checkCovers(star, triangulation, pieces);

    auto coveredTwiceArea = star.twiceArea();
    coveredTwiceArea -= coveredTwiceArea;
    for (const auto& piece : pieces) {
        coveredTwiceArea += piece.twiceArea();
    }
    CHECK(coveredTwiceArea > star.twiceArea());
}

TEST_CASE_TEMPLATE("convexCovering does not cover a region's holes",
                   Point, pgl::Point<int>, pgl::Point<pgl::Rational<int64_t>>) {
    using Poly = pgl::Polygon<Point>;
    const Poly outer(std::vector<Point>{P<Point>(0, 0), P<Point>(9, 0),
                                        P<Point>(9, 9), P<Point>(0, 9)});
    const std::vector<Poly> holes{Poly(std::vector<Point>{
        P<Point>(3, 3), P<Point>(6, 3), P<Point>(6, 6), P<Point>(3, 6)})};
    const pgl::PolygonWithHoles<Point> region(outer, holes);
    const auto triangulation = region.triangulation();
    const auto pieces = triangulation.convexCovering();
    CHECK(region.convexCovering() == pieces);
    checkCovers(region, triangulation, pieces);
    for (const auto& piece : pieces) {
        CHECK_FALSE(piece.interiorContains(P<Point>(4, 4)));
    }
}

TEST_CASE("convexCovering respects constrained edges") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    const pgl::Polygon<Point> square({0, 0, 4, 0, 4, 4, 0, 4});
    const std::vector<Segment> wall{Segment(Point(0, 0), Point(4, 4))};
    const auto triangulation = square.triangulation(wall);
    const auto pieces = triangulation.convexCovering();
    CHECK(pieces.size() == 2);
    checkCovers(square, triangulation, pieces);
}

TEST_CASE("convexCovering of an empty triangulation is empty") {
    using Point = pgl::Point<int>;
    const pgl::Triangulation<pgl::Triangle<Point>> triangulation;
    CHECK(triangulation.convexCovering().empty());
}

TEST_CASE("asGraph is the 1-skeleton of the mesh") {
    using Point = pgl::Point<int>;

    const pgl::Triangulation<pgl::Triangle<Point>> empty;
    CHECK(empty.asGraph().vertexCount() == 0);
    CHECK(empty.asGraph().edgeCount() == 0);

    // A square with its center: four boundary edges, four spokes, and one
    // diagonal-free interior vertex of degree four.
    const std::vector<Point> pts{Point(0, 0), Point(4, 0), Point(4, 4),
                                 Point(0, 4), Point(2, 2)};
    const pgl::Triangulation<pgl::Triangle<Point>> tri(pts);
    const pgl::Graph<Point> graph = tri.asGraph();
    CHECK(graph.vertexCount() == static_cast<int>(tri.numVertices()));
    CHECK(graph.edgeCount() == static_cast<int>(tri.numEdges()));
    CHECK(graph.degree(Point(2, 2)) == 4);
    CHECK(graph.components().size() == 1);
    for (const auto& p : pts) {
        CHECK(graph.containsVertex(p));
    }
    for (const auto& e : tri.edges()) {
        CHECK(graph.containsEdge(e[0], e[1]));
    }
    // The ghost vertex closing the mesh is internal, and its stored point is
    // the default-constructed one, which is a real vertex here.
    CHECK(graph.degree(Point(0, 0)) == 3);

    // Collinear points carry no triangle and hence no edge, but they are still
    // vertices of the triangulation and so of its graph.
    const pgl::Triangulation<pgl::Triangle<Point>> flat(
        std::vector<Point>{Point(0, 0), Point(1, 1), Point(2, 2)});
    const pgl::Graph<Point> flatGraph = flat.asGraph();
    CHECK(flat.numTriangles() == 0);
    CHECK(flatGraph.vertexCount() == 3);
    CHECK(flatGraph.edgeCount() == 0);
    CHECK(flatGraph.components().size() == 3);
}

TEST_CASE("asGraph sees only the triangulated domain of a polygon") {
    using Point = pgl::Point<int>;

    // The reflex corner of an L makes the hull triangles outside the polygon
    // out-of-domain, so the graph must not hold the chord that closes the hull.
    const pgl::Polygon<Point> ell(std::vector<Point>{
        Point(0, 0), Point(4, 0), Point(4, 1),
        Point(1, 1), Point(1, 4), Point(0, 4)});
    const auto triangulation = ell.triangulation();
    const pgl::Graph<Point> graph = triangulation.asGraph();
    CHECK(graph.vertexCount() == static_cast<int>(triangulation.numVertices()));
    CHECK(graph.edgeCount() == static_cast<int>(triangulation.numEdges()));
    CHECK(graph.vertexCount() == 6);
    CHECK_FALSE(graph.containsEdge(Point(4, 0), Point(0, 4)));
    CHECK_FALSE(graph.containsEdge(Point(4, 1), Point(1, 4)));
    for (const auto& e : ell.edges()) {
        CHECK(graph.containsEdge(e[0], e[1]));
    }

    // Every mesh edge of the graph is an edge of the mesh, and conversely.
    const auto edges = triangulation.edges();
    for (const Point& u : graph) {
        for (const Point& v : graph.neighbors(u)) {
            CHECK(triangulation.has(pgl::Segment<Point>(u, v)));
        }
    }
    CHECK(2 * static_cast<int>(edges.size()) ==
          [&] {
              int sum = 0;
              for (const Point& u : graph) {
                  sum += graph.degree(u);
              }
              return sum;
          }());
}

TEST_CASE("asArrangement has the mesh edges and triangle face IDs") {
    using Point = pgl::EPoint;
    using Segment = pgl::Segment<Point>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;

    const Mesh empty;
    const auto emptyArrangement = empty.asArrangement();
    CHECK(emptyArrangement.edgeCount() == 0);
    REQUIRE(emptyArrangement.faceCount() == 1);
    CHECK_FALSE(emptyArrangement.label(decltype(emptyArrangement)::FaceId(0)).valid());

    const Mesh mesh(std::vector<Point>{P<Point>(0, 0), P<Point>(4, 0), P<Point>(4, 4),
                                       P<Point>(0, 4), P<Point>(2, 2)});
    const auto arrangement = mesh.asArrangement();

    CHECK(arrangement.edgeCount() == mesh.numEdges());
    std::set<Segment> arrangementEdges;
    for (const auto& edge : arrangement.boundedEdges()) {
        arrangementEdges.emplace(edge[0], edge[1]);
    }
    const std::vector<Segment> meshEdgeList = mesh.edges();
    std::set<Segment> meshEdges(meshEdgeList.begin(), meshEdgeList.end());
    CHECK(arrangementEdges == meshEdges);

    std::set<Mesh::TriId> labeledTriangles;
    for (std::size_t i = 0; i < arrangement.faceCount(); ++i) {
        const decltype(arrangement)::FaceId face(static_cast<std::uint32_t>(i));
        const Mesh::TriId label = arrangement.label(face);
        if (arrangement.isUnbounded(face)) {
            CHECK_FALSE(label.valid());
            continue;
        }
        CHECK(mesh.has(label));
        labeledTriangles.insert(label);
    }
    const std::vector<Mesh::TriId> triangleIds = mesh.triangleIds();
    CHECK(labeledTriangles == std::set<Mesh::TriId>(triangleIds.begin(), triangleIds.end()));

    for (const Mesh::TriId triangle : triangleIds) {
        const auto face = arrangement.locateFace(mesh[triangle].pointInside());
        CHECK(arrangement.label(face) == triangle);
    }
}

// A mesh large enough that buildPointLocation indexes a strict sample of the
// vertices rather than all of them, with one query strictly inside every
// triangle. Coordinates are multiples of three, which makes each triangle's
// centroid a point of the coordinate type itself -- so the queries reach the
// index, which only serves a query in the mesh's own number type.
template <class Point>
std::vector<Point> sampledIndexMesh(int count) {
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;
    std::vector<Point> points;
    std::set<std::pair<int, int>> seen;
    std::uint32_t state = 0x1234567u;
    const auto next = [&state] {
        state = state * 1664525u + 1013904223u;
        return static_cast<int>((state >> 13) % 200) * 3;
    };
    while (static_cast<int>(points.size()) < count) {
        const std::pair<int, int> xy{next(), next()};
        if (seen.insert(xy).second) {
            points.emplace_back(Number(xy.first), Number(xy.second));
        }
    }
    return points;
}

TEST_CASE_TEMPLATE("The point-location index answers as the bare walk does",
                   Point, pgl::Point<int>, pgl::EPoint) {
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    // 400 vertices against an index of at most 64 cells: the walk finishes
    // what the index starts, over cells of several triangles each.
    Mesh mesh(sampledIndexMesh<Point>(400));
    REQUIRE(mesh.numTriangles() > 64);

    std::vector<Point> queries;
    for (const auto& triangle : mesh.triangles()) {
        queries.push_back((triangle.a() + triangle.b() + triangle.c()) / Number(3));
    }
    // Outside the hull, and on a vertex, where any incident triangle answers.
    queries.push_back(P<Point>(-50, -50));
    queries.push_back(P<Point>(5000, 17));
    const Point vertex = mesh[mesh.vertexIds().front()];

    std::vector<typename Mesh::TriId> walked;
    for (const Point& query : queries) {
        walked.push_back(mesh.locateId(query));
    }

    mesh.buildPointLocation();
    CHECK(mesh.hasPointLocation());
    for (std::size_t i = 0; i < queries.size(); ++i) {
        // Each query is strictly inside one triangle, or outside the hull, so
        // the answer is the triangle's identity and not a matter of where the
        // walk happened to start.
        CHECK(mesh.locateId(queries[i]) == walked[i]);
    }
    CHECK(mesh.has(mesh.locateId(vertex)));
}

TEST_CASE_TEMPLATE("A stale point-location index answers exactly as none does",
                   Point, pgl::Point<int>, pgl::EPoint) {
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    // Index a 400-vertex mesh, then grow it by half as much again without
    // rebuilding: the coarsening now knows two thirds of the vertices, and
    // every cell's seed was chosen against a mesh that no longer exists.
    //
    // The corners of the generator's range go in first, so every later point
    // falls inside the hull and the insertions exercise splitting rather than
    // hull growth.
    std::vector<Point> points{P<Point>(0, 0), P<Point>(597, 0), P<Point>(0, 597),
                              P<Point>(597, 597)};
    for (const Point& point : sampledIndexMesh<Point>(600)) {
        if (std::find(points.begin(), points.end(), point) == points.end()) {
            points.push_back(point);
        }
    }
    points.resize(600);
    Mesh mesh(std::vector<Point>(points.begin(), points.begin() + 400));
    mesh.buildPointLocation();
    REQUIRE(mesh.hasCurrentPointLocation());
    for (auto it = points.begin() + 400; it != points.end(); ++it) {
        mesh.insertDelaunay(*it);
    }
    REQUIRE(mesh.hasPointLocation());
    REQUIRE_FALSE(mesh.hasCurrentPointLocation());

    std::vector<Point> queries;
    for (const auto& triangle : mesh.triangles()) {
        queries.push_back((triangle.a() + triangle.b() + triangle.c()) / Number(3));
    }
    queries.push_back(P<Point>(-50, -50));
    queries.push_back(P<Point>(5000, 17));

    std::vector<typename Mesh::TriId> seeded;
    for (const Point& query : queries) {
        seeded.push_back(mesh.locateId(query));
    }

    mesh.clearPointLocation();
    for (std::size_t i = 0; i < queries.size(); ++i) {
        CHECK(mesh.locateId(queries[i]) == seeded[i]);
    }
}

TEST_CASE("The point-location index respects a domain with holes") {
    using Point = pgl::Point<int>;
    using PolygonShape = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;

    // The sample the index is drawn on is a triangulation of the vertices
    // alone, so its cells cross the holes; a query in a hole must still come
    // back empty, which is the walk's answer and not the index's. Every
    // coordinate here is a multiple of three, the holes' included, so that a
    // triangle's centroid stays a point of the mesh's own number type.
    const Region region(PolygonShape({0, 0, 600, 0, 600, 600, 0, 600}),
                        std::vector{PolygonShape({99, 99, 201, 99, 201, 201, 99, 201}),
                                    PolygonShape({399, 399, 501, 399, 501, 501, 399, 501})});
    std::vector<Point> interior;
    for (const Point& point : sampledIndexMesh<Point>(300)) {
        if (region.interiorContains(point)) {  // the holes are not the domain
            interior.push_back(point);
        }
    }
    Mesh mesh(region, interior);
    REQUIRE(mesh.numVertices() > 64);

    std::vector<Point> queries{Point(150, 150), Point(450, 450),  // the two holes
                               Point(50, 550),  Point(300, 50),   // material
                               Point(-20, 300)};                  // outside
    for (const auto& triangle : mesh.triangles()) {
        queries.push_back((triangle.a() + triangle.b() + triangle.c()) / 3);
    }

    std::vector<typename Mesh::TriId> walked;
    for (const Point& query : queries) {
        walked.push_back(mesh.locateId(query));
    }
    CHECK_FALSE(walked[0].valid());
    CHECK_FALSE(walked[1].valid());
    CHECK(walked[2].valid());
    CHECK(walked[3].valid());
    CHECK_FALSE(walked[4].valid());

    mesh.buildPointLocation();
    REQUIRE(mesh.hasPointLocation());
    for (std::size_t i = 0; i < queries.size(); ++i) {
        CHECK(mesh.locateId(queries[i]) == walked[i]);
    }
}

TEST_CASE("Triangulation uses and invalidates its arrangement point location") {
    using Point = pgl::EPoint;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;

    Mesh mesh(std::vector<Point>{P<Point>(0, 0), P<Point>(4, 0), P<Point>(4, 4),
                                 P<Point>(0, 4), P<Point>(2, 2)});
    const std::vector<Point> queries{P<Point>(2, 1), P<Point>(3, 2), P<Point>(2, 3),
                                     P<Point>(1, 2), P<Point>(8, 8)};
    std::vector<Mesh::TriId> walked;
    for (const Point& point : queries) {
        walked.push_back(mesh.locateId(point));
    }

    CHECK_FALSE(mesh.hasPointLocation());
    mesh.buildPointLocation();
    CHECK(mesh.hasPointLocation());
    for (std::size_t i = 0; i < queries.size(); ++i) {
        CHECK(mesh.locateId(queries[i]) == walked[i]);
    }

    // A query on a mesh vertex is seeded like any other, and the walk resolves
    // it to one of the incident triangles.
    CHECK(mesh.has(mesh.locateId(P<Point>(2, 2))));

    // A constraint flag changes no geometry a walk can see, so it leaves the
    // index not merely in place but current.
    mesh.setConstrained(mesh.triangleIds().front(), 0);
    CHECK(mesh.hasPointLocation());
    CHECK(mesh.hasCurrentPointLocation());

    // An edit does move the mesh on from the index, and the index survives it:
    // a seed is a triangle of this mesh however the mesh has changed, and the
    // walk answers from there.
    REQUIRE(mesh.insertDelaunay(P<Point>(1, 2)));
    CHECK(mesh.hasPointLocation());
    CHECK_FALSE(mesh.hasCurrentPointLocation());
    CHECK(mesh.has(mesh.locateId(P<Point>(1, 2))));
    for (std::size_t i = 0; i < queries.size(); ++i) {
        CHECK(mesh.locateId(queries[i]).valid() == walked[i].valid());
    }

    // Redrawing it against the mesh as it now stands is the owner's call.
    mesh.buildPointLocation();
    CHECK(mesh.hasCurrentPointLocation());
    CHECK(mesh.has(mesh.locateId(P<Point>(1, 2))));

    // And giving it up is the only thing that takes it away.
    mesh.clearPointLocation();
    CHECK_FALSE(mesh.hasPointLocation());
    CHECK(mesh.has(mesh.locateId(P<Point>(1, 2))));
}

TEST_CASE("handles navigate the same mesh as the value interface") {
    using Point = pgl::Point<int>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;
    using TriId = Mesh::TriId;
    using VertexId = Mesh::VertexId;

    const std::vector<Point> pts{Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10),
                                 Point(4, 5), Point(7, 2),  Point(2, 8)};
    const Mesh mesh(pts);

    // getId and getShape are inverse, and the handles of distinct triangles are
    // distinct (so they can key a hash set).
    std::set<TriId> ids;
    for (const auto& t : mesh.triangles()) {
        const TriId id = mesh.getId(t);
        CHECK(id.valid());
        CHECK(mesh.has(id));
        CHECK(mesh.getShape(id) == t);
        CHECK(mesh[id] == t);
        CHECK(ids.insert(id).second);
    }
    CHECK(ids.size() == mesh.numTriangles());

    for (const TriId id : ids) {
        const pgl::Triangle<Point> t = mesh[id];

        // vertices(id) is the triangle's own vertex order, handle by handle.
        const std::array<VertexId, 3> vs = mesh.vertices(id);
        for (int i = 0; i < 3; ++i) {
            CHECK(mesh.has(vs[i]));
            CHECK(mesh[vs[i]] == t[i]);
            CHECK(mesh.getId(mesh[vs[i]]) == vs[i]);
        }

        // The two adjacency queries agree with their value counterparts.
        std::set<pgl::Triangle<Point>> byHandle;
        for (const TriId n : mesh.edgeAdjacentTriangles(id)) {
            byHandle.insert(mesh[n]);
        }
        const auto edgeAdjacent = mesh.edgeAdjacentTriangles(t);
        CHECK(byHandle == std::set<pgl::Triangle<Point>>(edgeAdjacent.begin(),
                                                         edgeAdjacent.end()));
        byHandle.clear();
        for (const TriId n : mesh.vertexAdjacentTriangles(id)) {
            byHandle.insert(mesh[n]);
        }
        const auto vertexAdjacent = mesh.vertexAdjacentTriangles(t);
        CHECK(byHandle == std::set<pgl::Triangle<Point>>(vertexAdjacent.begin(),
                                                         vertexAdjacent.end()));

        // Crossing each edge by its endpoint handles lands where crossing it by
        // its segment does.
        for (int i = 0; i < 3; ++i) {
            const VertexId a = vs[(i + 1) % 3];
            const VertexId b = vs[(i + 2) % 3];
            const std::optional<TriId> across = mesh.otherTriangle(id, a, b);
            const auto expected =
                mesh.otherTriangle(t, pgl::Segment<Point>(mesh[a], mesh[b]));
            REQUIRE(across.has_value() == expected.has_value());
            if (across) {
                CHECK(mesh[*across] == *expected);
            }
        }
    }

    // locateId answers the same query as locate, one handle instead of a value.
    for (int x = 0; x <= 10; ++x) {
        for (int y = 0; y <= 10; ++y) {
            const Point q(x, y);
            const TriId found = mesh.locateId(q);
            const std::optional<pgl::Triangle<Point>> value = mesh.locate(q);
            REQUIRE(found.valid() == value.has_value());
            if (found.valid()) {
                CHECK(mesh.has(found));
                CHECK(mesh[found] == *value);
                CHECK(mesh.getId(*value) == found);
            }
        }
    }
    CHECK_FALSE(mesh.locateId(Point(-5, -5)).valid());  // outside the hull
    CHECK_FALSE(Mesh().locateId(Point(0, 0)).valid());  // empty triangulation

    // The fan of a vertex handle is the fan of its point.
    for (const auto& p : pts) {
        const VertexId v = mesh.getId(p);
        REQUIRE(mesh.has(v));
        std::set<pgl::Triangle<Point>> byHandle;
        for (const TriId f : mesh.incidentTriangles(v)) {
            byHandle.insert(mesh[f]);
        }
        const auto fan = mesh.incidentTriangles(p);
        CHECK_FALSE(byHandle.empty());
        CHECK(byHandle == std::set<pgl::Triangle<Point>>(fan.begin(), fan.end()));
    }
}

TEST_CASE("handles of foreign cells are invalid and navigate to nothing") {
    using Point = pgl::Point<int>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;

    const Mesh mesh(std::vector<Point>{Point(0, 0), Point(4, 0), Point(4, 4), Point(0, 4)});

    CHECK_FALSE(mesh.getId(pgl::Triangle<Point>(Point(0, 0), Point(1, 0), Point(0, 1))).valid());
    CHECK_FALSE(mesh.getId(Point(1, 3)).valid());   // inside, but not a vertex
    CHECK_FALSE(mesh.getId(Point(9, 9)).valid());   // outside the hull
    CHECK_FALSE(mesh.has(Mesh::TriId()));
    CHECK_FALSE(mesh.has(Mesh::VertexId()));
    CHECK_FALSE(mesh.has(Mesh::TriId(1u << 20)));   // never handed out
    CHECK_FALSE(mesh.has(Mesh::VertexId(1u << 20)));
    CHECK(mesh.edgeAdjacentTriangles(Mesh::TriId()).empty());
    CHECK(mesh.vertexAdjacentTriangles(Mesh::TriId()).empty());
    CHECK(mesh.incidentTriangles(Mesh::VertexId()).empty());
    CHECK_FALSE(mesh.otherTriangle(Mesh::TriId(), Mesh::VertexId(), Mesh::VertexId()).has_value());

    // Two vertices of the mesh that are not joined by an edge of the triangle
    // they are asked about are not a crossable edge either.
    const Mesh::TriId some = mesh.getId(mesh.triangles().front());
    const auto vs = mesh.vertices(some);
    CHECK_FALSE(mesh.otherTriangle(some, vs[0], vs[0]).has_value());
    CHECK_FALSE(mesh.otherTriangle(some, vs[0], Mesh::VertexId()).has_value());
}

TEST_CASE("handles reach the labels of a polygon's triangulation") {
    using Point = pgl::Point<int>;
    using LabeledTriangle = pgl::Triangle<Point, int>;
    using Mesh = pgl::Triangulation<LabeledTriangle>;

    // An L, so the triangles between the polygon and its hull are out of domain
    // and must stay unreachable through handles.
    const pgl::Polygon<Point> ell(std::vector<Point>{
        Point(0, 0), Point(4, 0), Point(4, 1),
        Point(1, 1), Point(1, 4), Point(0, 4)});
    Mesh mesh(ell);

    int next = 1;
    for (const auto& t : mesh.triangles()) {
        mesh.label(mesh.getId(t)) = next++;
    }
    CHECK(static_cast<std::size_t>(next - 1) == mesh.numTriangles());
    for (const auto& t : mesh.triangles()) {
        const Mesh::TriId id = mesh.getId(t);
        CHECK(mesh.label(id) == mesh.label(t));
        for (const Mesh::TriId n : mesh.edgeAdjacentTriangles(id)) {
            CHECK(mesh.has(n));                    // never leaves the domain
            CHECK(mesh.label(n) != mesh.label(id));
        }
    }

    // The chord closing the hull is not an edge of the domain, so the triangle
    // on its far side is not reachable from the domain by handle.
    for (const auto& t : mesh.triangles()) {
        const auto vs = mesh.vertices(mesh.getId(t));
        for (int i = 0; i < 3; ++i) {
            const auto across =
                mesh.otherTriangle(mesh.getId(t), vs[(i + 1) % 3], vs[(i + 2) % 3]);
            if (across) {
                CHECK(mesh.has(*across));
            }
        }
    }
}

TEST_CASE("handles enumerate the mesh and index side tables") {
    using Point = pgl::Point<int>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;
    using TriId = Mesh::TriId;
    using VertexId = Mesh::VertexId;

    const Mesh empty;
    CHECK(empty.triangleIds().empty());
    CHECK(empty.vertexIds().empty());
    CHECK(empty.triangleIndexBound() == 0);
    CHECK(empty.vertexIndexBound() == 0);

    // An L, so the mesh also stores the hull-fill triangles the domain carved
    // away: they take index slots but are not triangles of the triangulation.
    const pgl::Polygon<Point> ell(std::vector<Point>{
        Point(0, 0), Point(4, 0), Point(4, 1),
        Point(1, 1), Point(1, 4), Point(0, 4)});
    const Mesh mesh(ell);

    const std::vector<TriId> ids = mesh.triangleIds();
    CHECK(ids.size() == mesh.numTriangles());
    std::set<pgl::Triangle<Point>> byHandle;
    for (const TriId id : ids) {
        CHECK(mesh.has(id));
        byHandle.insert(mesh[id]);
    }
    const auto values = mesh.triangles();
    CHECK(byHandle == std::set<pgl::Triangle<Point>>(values.begin(), values.end()));

    // visitTriangles gives handles to a callable that takes only handles, values
    // to one that takes a triangle (a generic callable takes both: values), and
    // stops early for either when the callable says so.
    std::size_t handleCount = 0;
    std::size_t valueCount = 0;
    std::size_t genericCount = 0;
    mesh.visitTriangles([&](TriId id) { handleCount += mesh.has(id) ? 1 : 0; });
    mesh.visitTriangles([&](const pgl::Triangle<Point>& t) { valueCount += mesh.has(t) ? 1 : 0; });
    mesh.visitTriangles([&](auto t) { genericCount += mesh.has(t) ? 1 : 0; });
    CHECK(handleCount == mesh.numTriangles());
    CHECK(valueCount == mesh.numTriangles());
    CHECK(genericCount == mesh.numTriangles());
    std::size_t seen = 0;
    CHECK(mesh.visitTriangles([&](TriId) { return ++seen == 2; }));
    CHECK(seen == 2);

    // Every handle indexes a side table sized from the public API.
    CHECK(mesh.triangleIndexBound() >= mesh.numTriangles());
    std::vector<int> triangleTable(mesh.triangleIndexBound(), 0);
    for (const TriId id : ids) {
        REQUIRE(id.index() < triangleTable.size());
        CHECK(triangleTable[id.index()] == 0);
        triangleTable[id.index()] = 1;
    }

    // Vertex indices are dense over [1, vertexIndexBound()), so index() - 1 is
    // the tight slot of a table of numVertices() entries.
    const std::vector<VertexId> vertexIds = mesh.vertexIds();
    CHECK(vertexIds.size() == mesh.numVertices());
    CHECK(mesh.vertexIndexBound() == mesh.numVertices() + 1);
    std::vector<int> vertexTable(mesh.numVertices(), 0);
    for (const VertexId v : vertexIds) {
        REQUIRE(v.index() >= 1);
        REQUIRE(v.index() < mesh.vertexIndexBound());
        CHECK(mesh.has(v));
        vertexTable[v.index() - 1] = 1;
    }
    for (const int filled : vertexTable) {
        CHECK(filled == 1);  // dense: no slot left over
    }
    std::set<Point> points;
    for (const VertexId v : vertexIds) {
        points.insert(mesh[v]);
    }
    const auto corners = ell.vertices();  // one container: begin/end of the same object
    CHECK(points == std::set<Point>(corners.begin(), corners.end()));
}

TEST_CASE("a side index names an edge of a triangle handle") {
    using Point = pgl::Point<int>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;

    const pgl::Polygon<Point> ell(std::vector<Point>{
        Point(0, 0), Point(4, 0), Point(4, 1),
        Point(1, 1), Point(1, 4), Point(0, 4)});
    Mesh mesh(ell);

    const auto boundary = ell.edges();  // one container: begin/end of the same object
    const std::set<pgl::Segment<Point>> polygonEdges(boundary.begin(), boundary.end());

    for (const Mesh::TriId id : mesh.triangleIds()) {
        const pgl::Triangle<Point> t = mesh[id];
        const auto vs = mesh.vertices(id);
        for (int s = 0; s < 3; ++s) {
            // Side s is the edge from vertex s to vertex s + 1, which is exactly
            // what the triangle value calls its edge s.
            const pgl::Segment<Point> e = t.edges()[s];
            CHECK(e == pgl::Segment<Point>(mesh[vs[s]], mesh[vs[(s + 1) % 3]]));

            const std::optional<Mesh::TriId> across = mesh.otherTriangle(id, s);
            const auto expected = mesh.otherTriangle(t, e);
            REQUIRE(across.has_value() == expected.has_value());
            if (across) {
                CHECK(mesh[*across] == *expected);
                CHECK(mesh.otherTriangle(id, vs[s], vs[(s + 1) % 3]) == across);
            }

            // The constrained flag answers "is this shared edge one of the
            // polygon's own edges" with no set of segments on the side.
            CHECK(mesh.isConstrained(id, s) == mesh.isConstrained(e));
            CHECK(mesh.isConstrained(id, s) == (polygonEdges.count(e) > 0));
        }
    }

    // Constraining through a handle is seen from the other side of the edge and
    // by the segment-keyed accessor, and releasing it undoes exactly that.
    for (const Mesh::TriId id : mesh.triangleIds()) {
        for (int s = 0; s < 3; ++s) {
            const std::optional<Mesh::TriId> across = mesh.otherTriangle(id, s);
            if (mesh.isConstrained(id, s) || !across) {
                continue;
            }
            const pgl::Segment<Point> e = mesh[id].edges()[s];
            mesh.setConstrained(id, s);
            CHECK(mesh.isConstrained(id, s));
            CHECK(mesh.isConstrained(e));
            bool seenFromNeighbor = false;
            for (int q = 0; q < 3; ++q) {
                if (mesh[*across].edges()[q] == e) {
                    seenFromNeighbor = mesh.isConstrained(*across, q);
                }
            }
            CHECK(seenFromNeighbor);
            mesh.setConstrained(id, s, false);
            CHECK_FALSE(mesh.isConstrained(id, s));
            CHECK_FALSE(mesh.isConstrained(e));
        }
    }
}

TEST_CASE("range-search visitors follow the same handle-or-value rule") {
    using Point = pgl::Point<int>;
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;
    using TriangleShape = pgl::Triangle<Point>;

    std::vector<Point> pts;
    for (int x = 0; x < 6; ++x) {
        for (int y = 0; y < 6; ++y) {
            pts.push_back(Point(x * 3, y * 3 + (x % 2)));  // no big cocircular families
        }
    }
    const Mesh mesh(pts);

    // Whatever the query shape — directed, region, chain or point — a handle
    // visitor sees exactly the triangles a value visitor sees.
    const auto sameBothWays = [&](const auto& query) {
        std::set<TriangleShape> byHandle, byValue, interiorByHandle, interiorByValue;
        mesh.visitTrianglesIntersecting(query, [&](Mesh::TriId id) { byHandle.insert(mesh[id]); });
        mesh.visitTrianglesIntersecting(query, [&](const TriangleShape& t) { byValue.insert(t); });
        mesh.visitTrianglesInteriorIntersecting(
            query, [&](Mesh::TriId id) { interiorByHandle.insert(mesh[id]); });
        mesh.visitTrianglesInteriorIntersecting(
            query, [&](const TriangleShape& t) { interiorByValue.insert(t); });
        CHECK(byHandle == byValue);
        CHECK(interiorByHandle == interiorByValue);
        const auto listed = mesh.trianglesIntersecting(query);
        CHECK(byHandle == std::set<TriangleShape>(listed.begin(), listed.end()));
        return byHandle.size();
    };

    CHECK(sameBothWays(pgl::Segment<Point>(Point(0, 0), Point(15, 15))) > 0);
    CHECK(sameBothWays(pgl::OrientedSegment<Point>(Point(1, 1), Point(14, 4))) > 0);
    CHECK(sameBothWays(pgl::Line<Point>(Point(0, 2), Point(15, 9))) > 0);
    CHECK(sameBothWays(pgl::Ray<Point>(Point(2, 2), Point(15, 12))) > 0);
    CHECK(sameBothWays(TriangleShape(Point(1, 1), Point(9, 2), Point(4, 8))) > 0);
    CHECK(sameBothWays(pgl::Rectangle<Point>(Point(2, 2), Point(11, 9))) > 0);
    CHECK(sameBothWays(pgl::Disk<Point>(Point(6, 6), 16)) > 0);
    CHECK(sameBothWays(Point(3, 3)) > 0);
    CHECK(sameBothWays(pgl::Polyline<Point>(std::vector<Point>{
              Point(0, 0), Point(9, 3), Point(3, 12), Point(15, 15)})) > 0);

    // A handle visitor stops the walk early on true, like a value one.
    std::size_t seen = 0;
    CHECK(mesh.visitTrianglesIntersecting(pgl::Segment<Point>(Point(0, 0), Point(15, 15)),
                                          [&](Mesh::TriId) { return ++seen == 2; }));
    CHECK(seen == 2);
}
