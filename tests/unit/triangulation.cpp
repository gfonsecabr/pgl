#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
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
long strictInCircleViolations(const Tri& tri, const std::vector<Point>& pts) {
    long viol = 0;
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
        CHECK(tri.contains(t));
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
    // Collinear input is supported as long as the points are not ALL collinear:
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
    CHECK(tri.contains(*located));

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
        CHECK(tri.contains(t));
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

TEST_CASE("Navigation: adjacency and the segment-to-edge bridge are consistent") {
    using Point = pgl::Point<int>;
    const std::vector<Point> pts = {
        P<Point>(0, 0),  P<Point>(40, 0),  P<Point>(40, 40),
        P<Point>(0, 40), P<Point>(18, 22),
    };
    pgl::Triangulation tri(pts);

    for (const auto& t : tri.triangles()) {
        // Each neighbor across a shared edge sees t back across the same edge.
        for (const auto& nb : tri.adjacentTriangles(t)) {
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
    CHECK(tri.contains(*flipped));
    CHECK_FALSE(tri.contains(*diagonal));
}
