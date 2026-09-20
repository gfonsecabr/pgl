#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <compare>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;

// The running examples: the unit square, the triangle below x + y <= 1, the
// first quadrant (a wedge), a horizontal strip, and assorted degenerate
// regions. Half-planes contain the points to the left of source -> target.
namespace {
const Halfplane yGE0(0, 0, 1, 0);   // y >= 0
const Halfplane xLE1(1, 0, 1, 1);   // x <= 1
const Halfplane yLE1(1, 1, 0, 1);   // y <= 1
const Halfplane xGE0(0, 1, 0, 0);   // x >= 0
const Halfplane diag(1, 0, 0, 1);   // x + y <= 1

Region unitSquare() {
    return Region({yGE0, xLE1, yLE1, xGE0});
}
Region triangle() {
    return Region({yGE0, xGE0, diag});
}
Region quadrant() {
    return Region({yGE0, xGE0});
}
Region strip() {
    return Region({yGE0, yLE1});
}
}  // namespace

TEST_CASE("Default construction is the whole plane") {
    const Region plane;
    CHECK(plane.isPlane());
    CHECK(!plane.empty());
    CHECK(!plane.isBounded());
    CHECK(!plane.isDegenerate());
    CHECK(plane.size() == 0);
    CHECK(plane.vertexCount() == 0);
    CHECK(plane.contains(Point(1000, -1000)));
    CHECK(plane.interiorContains(Point(0, 0)));
}

TEST_CASE("Range construction discards redundant half-planes") {
    const Region square = unitSquare();
    CHECK(square.size() == 4);
    CHECK(square.isBounded());
    CHECK(!square.isDegenerate());
    CHECK(square.vertexCount() == 4);

    SUBCASE("a redundant half-plane is discarded on insert") {
        Region k = unitSquare();
        CHECK(!k.insert(Halfplane(10, 0, 10, 1)));  // x <= 10 already implied
        CHECK(k.size() == 4);
        CHECK(!k.insert(Halfplane(1, 0, 1, 1)));  // exactly x <= 1 again
        CHECK(k.size() == 4);
    }

    SUBCASE("a touching half-plane that contains the region is redundant") {
        Region k = unitSquare();
        CHECK(!k.insert(Halfplane(-1, 1, 0, 0)));  // x + y >= 0, touches (0,0)
        CHECK(k.size() == 4);
    }

    SUBCASE("a cutting half-plane removes newly redundant constraints") {
        Region k = unitSquare();
        CHECK(k.insert(diag));  // x + y <= 1 makes x <= 1 and y <= 1 redundant
        CHECK(k == triangle());
    }

    SUBCASE("a same-direction more restrictive half-plane replaces the old one") {
        Region k = unitSquare();
        CHECK(k.insert(Halfplane(0, 0, 0, 1)));  // x <= 0 replaces x <= 1
        CHECK(k.size() == 4);
        CHECK(k.isDegenerate());  // the square collapses onto its left edge
        CHECK(k.contains(Point(0, 0)));
        CHECK(k.contains(Point(0, 1)));
        CHECK(!k.contains(Point(1, 0)));
    }
}

TEST_CASE("Insertion order does not matter for full-dimensional regions") {
    const Region a({yGE0, xLE1, yLE1, xGE0});
    const Region b({xGE0, yLE1, yGE0, xLE1});
    CHECK(a == b);
    CHECK((a <=> b) == std::strong_ordering::equal);
    CHECK(std::hash<Region>{}(a) == std::hash<Region>{}(b));
}

TEST_CASE("Range construction agrees with inserting one half-plane at a time") {
    // Small coordinates make parallel, coincident, touching and contradictory
    // constraints common, and the opposite of an earlier constraint pins the
    // region to a line, so the empty and degenerate outcomes (about half and a
    // fifth of the trials) are exercised alongside the full-dimensional ones.
    std::mt19937 rng(20260914);
    std::uniform_int_distribution<int> coordinate(-4, 4);
    std::uniform_int_distribution<int> count(0, 12);
    std::uniform_int_distribution<int> coin(0, 3);
    for (int trial = 0; trial < 4000; ++trial) {
        std::vector<Halfplane> constraints;
        const int n = count(rng);
        for (int i = 0; i < n; ++i) {
            if (!constraints.empty() && coin(rng) == 0) {
                std::uniform_int_distribution<std::size_t> pick(0, constraints.size() - 1);
                constraints.push_back(constraints[pick(rng)].opposite());
            } else {
                constraints.emplace_back(coordinate(rng), coordinate(rng), coordinate(rng), coordinate(rng));
            }
        }
        const Region built(constraints);
        Region inserted;
        for (const Halfplane& h : constraints) {
            inserted.insert(h);
        }
        REQUIRE(built.empty() == inserted.empty());
        REQUIRE(built.isDegenerate() == inserted.isDegenerate());
        if (!built.isDegenerate()) {
            CHECK(built == inserted);  // canonical for full-dimensional regions
        }
        for (int x = -5; x <= 5; ++x) {
            for (int y = -5; y <= 5; ++y) {
                const Point p(x, y);
                REQUIRE(built.contains(p) == inserted.contains(p));
                REQUIRE(built.interiorContains(p) == inserted.interiorContains(p));
            }
        }
    }
}

TEST_CASE("Emptiness is detected and sticky") {
    Region k({yGE0});
    CHECK(k.insert(Halfplane(1, -1, 0, -1)));  // y <= -1 contradicts y >= 0
    CHECK(k.empty());
    CHECK(k.size() == 0);
    CHECK(!k.isPlane());
    CHECK(k.isBounded());  // the empty set is bounded
    CHECK(k.isDegenerate());
    CHECK(!k.contains(Point(0, 0)));
    CHECK(!k.insert(yGE0));  // inserting into the empty region changes nothing
    CHECK(k.empty());

    SUBCASE("all empty regions are equal") {
        Region other({xGE0});
        other.insert(Halfplane(-1, 0, -1, 1));  // x <= -1
        CHECK(other.empty());
        CHECK(k == other);
        CHECK(std::hash<Region>{}(k) == std::hash<Region>{}(other));
    }
}

TEST_CASE("Degenerate regions are tracked and behave as sets") {
    SUBCASE("two opposite half-planes give a line") {
        const Region line({yGE0, Halfplane(1, 0, 0, 0)});  // y >= 0 and y <= 0
        CHECK(line.isDegenerate());
        CHECK(!line.empty());
        CHECK(!line.isBounded());
        CHECK(line.vertexCount() == 0);
        CHECK(line.contains(Point(7, 0)));
        CHECK(line.boundaryContains(Point(7, 0)));
        CHECK(!line.interiorContains(Point(7, 0)));
        CHECK(!line.contains(Point(0, 1)));
    }

    SUBCASE("three touching half-planes give a point") {
        const Region point({yGE0, xGE0, Halfplane(0, 0, -1, 1)});  // x + y <= 0
        CHECK(point.isDegenerate());
        CHECK(point.isBounded());
        CHECK(point.contains(Point(0, 0)));
        CHECK(point.boundaryContains(Point(0, 0)));
        CHECK(!point.contains(Point(1, 0)));
        CHECK(!point.contains(Point(0, 1)));
    }
}

TEST_CASE("Constructors from other shapes") {
    SUBCASE("a single half-plane") {
        const Region k(yGE0);
        CHECK(k.size() == 1);
        CHECK(!k.isBounded());
        CHECK(k.contains(Point(5, 3)));
        CHECK(!k.contains(Point(5, -3)));
    }

    SUBCASE("a rectangle") {
        const Region k(pgl::Rectangle<Point>(Point(0, 0), Point(2, 1)));
        CHECK(k.size() == 4);
        CHECK(k.isBounded());
        CHECK(k.contains(Point(2, 1)));
        CHECK(k.contains(Point(1, 1)));
        CHECK(!k.contains(Point(3, 0)));
    }

    SUBCASE("a triangle") {
        const pgl::Triangle<Point> t(Point(0, 0), Point(2, 0), Point(0, 2));
        const Region k(t);
        CHECK(k.size() == 3);
        CHECK(k.isBounded());
        CHECK(k.contains(Point(1, 1)));
        CHECK(k.interiorContains(Point(1, 1)) == false);  // on the hypotenuse
        CHECK(k.contains(Point(0, 0)));
        CHECK(!k.contains(Point(2, 2)));
    }

    SUBCASE("a convex polygon round-trips through asConvex") {
        const pgl::Convex<Point> convex({Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)});
        const Region k(convex);
        CHECK(k.size() == 4);
        CHECK(k.asConvex<int>() == convex);
        CHECK(k.convexHull<int>() == convex);
    }

    SUBCASE("an empty convex polygon gives the empty region") {
        const Region k(pgl::Convex<Point>{});
        CHECK(k.empty());
    }

    SUBCASE("a one-point convex polygon gives a degenerate point region") {
        const Region k(pgl::Convex<Point>({Point(3, 4)}));
        CHECK(k.isDegenerate());
        CHECK(k.contains(Point(3, 4)));
        CHECK(!k.contains(Point(3, 5)));
        CHECK(!k.contains(Point(4, 4)));
    }

    SUBCASE("a two-point convex polygon gives a degenerate segment region") {
        const Region k(pgl::Convex<Point>({Point(0, 0), Point(2, 2)}));
        CHECK(k.isDegenerate());
        CHECK(k.isBounded());
        CHECK(k.contains(Point(0, 0)));
        CHECK(k.contains(Point(1, 1)));
        CHECK(k.contains(Point(2, 2)));
        CHECK(!k.contains(Point(3, 3)));
        CHECK(!k.contains(Point(1, 0)));
    }
}

TEST_CASE("Boundedness of the standard examples") {
    CHECK(unitSquare().isBounded());
    CHECK(triangle().isBounded());
    CHECK(!quadrant().isBounded());
    CHECK(!strip().isBounded());
    CHECK(!Region(yGE0).isBounded());
}

TEST_CASE("Vertices and edges") {
    SUBCASE("triangle vertices are exact with rational coordinates") {
        const Region k = triangle();
        REQUIRE(k.vertexCount() == 3);
        using Q = pgl::Rational<int>;
        std::vector<pgl::Point<Q>> vs = k.vertices<Q>();
        REQUIRE(vs.size() == 3);
        // Vertices in pair-index order, CCW for a bounded region.
        CHECK(std::count(vs.begin(), vs.end(), pgl::Point<Q>(Q(0), Q(0))) == 1);
        CHECK(std::count(vs.begin(), vs.end(), pgl::Point<Q>(Q(1), Q(0))) == 1);
        CHECK(std::count(vs.begin(), vs.end(), pgl::Point<Q>(Q(0), Q(1))) == 1);
    }

    SUBCASE("non-integer vertices from integer half-planes") {
        // y >= 0, x >= 0, x + 2y <= 1: vertices (0,0), (1,0), (0,1/2).
        const Region k({yGE0, xGE0, Halfplane(1, 0, -1, 1)});
        using Q = pgl::Rational<int>;
        const auto vs = k.vertices<Q>();
        REQUIRE(vs.size() == 3);
        CHECK(std::count(vs.begin(), vs.end(), pgl::Point<Q>(Q(0), Q(1, 2))) == 1);
    }

    SUBCASE("a wedge has one vertex and two ray edges") {
        const Region k = quadrant();
        CHECK(k.vertexCount() == 1);
        using Q = pgl::Rational<int>;
        bool sawRay = false;
        for (std::size_t i = 0; i < k.size(); ++i) {
            const auto e = k.edge<Q>(i);
            if (std::holds_alternative<pgl::Ray<pgl::Point<Q>>>(e)) {
                sawRay = true;
                CHECK(std::get<pgl::Ray<pgl::Point<Q>>>(e).source() == pgl::Point<Q>(Q(0), Q(0)));
            }
        }
        CHECK(sawRay);
    }

    SUBCASE("a strip has two full-line edges") {
        const Region k = strip();
        CHECK(k.vertexCount() == 0);
        using Q = pgl::Rational<int>;
        CHECK(std::holds_alternative<pgl::Line<pgl::Point<Q>>>(k.edge<Q>(0)));
        CHECK(std::holds_alternative<pgl::Line<pgl::Point<Q>>>(k.edge<Q>(1)));
    }

    SUBCASE("a bounded region has segment edges") {
        const Region k = unitSquare();
        using Q = pgl::Rational<int>;
        for (std::size_t i = 0; i < k.size(); ++i) {
            CHECK(std::holds_alternative<pgl::Segment<pgl::Point<Q>>>(k.edge<Q>(i)));
        }
    }
}

TEST_CASE("Bounding boxes") {
    SUBCASE("exact box of the unit square") {
        const auto box = unitSquare().bbox<int>();
        CHECK(box == pgl::Rectangle<Point>(Point(0, 0), Point(1, 1)));
    }

    SUBCASE("integer boxes round outward") {
        // y >= 0, x >= 0, x + 2y <= 1: y_max = 1/2 rounds up to 1.
        const Region k({yGE0, xGE0, Halfplane(1, 0, -1, 1)});
        CHECK(k.bbox<int>() == pgl::Rectangle<Point>(Point(0, 0), Point(1, 1)));
        // Mirrored below the axes: y_min = -1/2 rounds down to -1.
        const Region m({Halfplane(0, 0, -1, 0), Halfplane(0, 0, 0, 1), Halfplane(-1, 0, 1, -1)});
        CHECK(m.bbox<int>() == pgl::Rectangle<Point>(Point(-1, -1), Point(0, 0)));
    }

    SUBCASE("rational and floating boxes are exact") {
        const Region k({yGE0, xGE0, Halfplane(1, 0, -1, 1)});
        using Q = pgl::Rational<int>;
        const auto box = k.bbox<Q>();
        CHECK(box.max() == pgl::Point<Q>(Q(1), Q(1, 2)));
        const auto fb = k.fbox();
        CHECK(fb.max().y() == doctest::Approx(0.5));
    }

    SUBCASE("unbounded and empty regions throw") {
        CHECK_THROWS_AS((void)quadrant().bbox(), std::logic_error);
        CHECK_THROWS_AS((void)strip().bbox(), std::logic_error);
        CHECK_THROWS_AS((void)Region().bbox(), std::logic_error);
        Region empty({yGE0});
        empty.insert(Halfplane(1, -1, 0, -1));
        CHECK_THROWS_AS((void)empty.bbox(), std::logic_error);
    }
}

TEST_CASE("convexHull mirrors asConvex: throws only when unbounded") {
    CHECK_THROWS_AS((void)quadrant().convexHull(), std::logic_error);
    CHECK_THROWS_AS((void)strip().convexHull(), std::logic_error);
    CHECK_THROWS_AS((void)Region().convexHull(), std::logic_error);
    // Unlike bbox, an empty (but bounded) region has a well-defined empty hull.
    CHECK(Region(pgl::Convex<Point>{}).convexHull().empty());
}

TEST_CASE("asConvex") {
    SUBCASE("bounded regions convert") {
        const auto convex = triangle().asConvex();
        CHECK(convex.size() == 3);
        CHECK(convex.contains(Point(0, 0)));
    }
    SUBCASE("the empty region gives the empty convex polygon") {
        Region empty({yGE0});
        empty.insert(Halfplane(1, -1, 0, -1));
        CHECK(empty.asConvex().size() == 0);
    }
    SUBCASE("unbounded regions throw") {
        CHECK_THROWS_AS((void)quadrant().asConvex(), std::logic_error);
    }
}

TEST_CASE("Accessors") {
    const Region k = unitSquare();
    CHECK(k[0] == yGE0);  // sorted by boundary pseudo-angle from the +x axis
    CHECK(k.get(-1) == k[3]);
    CHECK(k.get(4) == k[0]);
    CHECK(k.index(xLE1) == 1);
    CHECK(k.index(diag) == -1);
    CHECK(k.halfplanes().size() == 4);
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& h : k) {
        ++count;
    }
    CHECK(count == 4);
}

TEST_CASE("Streaming") {
    std::ostringstream stream;
    stream << quadrant();
    CHECK(stream.str().find("HalfplaneIntersection[") == 0);

    std::ostringstream planeStream;
    planeStream << Region();
    CHECK(planeStream.str() == "HalfplaneIntersection[plane]");

    Region empty({yGE0});
    empty.insert(Halfplane(1, -1, 0, -1));
    std::ostringstream emptyStream;
    emptyStream << empty;
    CHECK(emptyStream.str() == "HalfplaneIntersection[empty]");
}

TEST_CASE("Hashing and use in unordered containers") {
    std::unordered_set<Region> set;
    set.insert(unitSquare());
    set.insert(triangle());
    set.insert(unitSquare());
    CHECK(set.size() == 2);
}

TEST_CASE("Transformations") {
    SUBCASE("translation") {
        Region k = triangle();
        k += Point(10, 20);
        CHECK(k.contains(Point(10, 20)));
        CHECK(k.contains(Point(11, 20)));
        CHECK(!k.contains(Point(0, 0)));
        k -= Point(10, 20);
        CHECK(k == triangle());
    }

    SUBCASE("uniform scaling, including negative factors") {
        Region k = triangle();
        k *= 2;
        CHECK(k.contains(Point(2, 0)));
        CHECK(!k.contains(Point(2, 1)));
        Region m = triangle();
        m *= -1;
        CHECK(m.contains(Point(-1, 0)));
        CHECK(m.contains(Point(0, -1)));
        CHECK(!m.contains(Point(1, 0)));
        CHECK(m.isBounded());
    }

    SUBCASE("rotation by 90 degrees") {
        const Region k = quadrant().rotated90();
        CHECK(k.contains(Point(-1, 1)));
        CHECK(!k.contains(Point(1, 1)));
    }

    SUBCASE("axis scaling, including the reflecting negative case") {
        Region k = triangle();
        k.scaleUpX(3);
        CHECK(k.contains(Point(3, 0)));
        CHECK(!k.contains(Point(4, 0)));
        Region m = triangle();
        m.scaleUpX(-1);
        CHECK(m.contains(Point(-1, 0)));
        CHECK(m.contains(Point(0, 1)));
        CHECK(!m.contains(Point(1, 0)));
    }
}

TEST_CASE("Intersection with a half-plane stays exact and typed") {
    const Region k = unitSquare();
    const auto cut = k.intersection<int>(diag);
    CHECK(cut == triangle());
    // Cutting the whole thing away yields the empty region, not an optional.
    const auto gone = k.intersection<int>(Halfplane(1, -1, 0, -1));  // y <= -1
    CHECK(gone.empty());
}

TEST_CASE("Exact rational regions work end to end") {
    using EPoint = pgl::EPoint;
    using ER = pgl::ERational;
    pgl::EHalfplaneIntersection k;
    k.insert(pgl::EHalfplane(EPoint(ER(0), ER(0)), EPoint(ER(1), ER(0))));
    k.insert(pgl::EHalfplane(EPoint(ER(0), ER(1)), EPoint(ER(0), ER(0))));
    k.insert(pgl::EHalfplane(EPoint(ER(1), ER(0)), EPoint(ER(0), ER(1))));
    CHECK(k.size() == 3);
    CHECK(k.isBounded());
    CHECK(k.contains(EPoint(ER(1, 4), ER(1, 4))));
    CHECK(!k.contains(EPoint(ER(1), ER(1))));
    CHECK(k.bbox() == pgl::ERectangle(EPoint(ER(0), ER(0)), EPoint(ER(1), ER(1))));
}

TEST_CASE("Cross-type conversion") {
    const Region k = triangle();
    const pgl::HalfplaneIntersection<pgl::Point<long long>> wide(k);
    CHECK(wide.size() == 3);
    CHECK(wide.contains(pgl::Point<long long>(0, 0)));
    CHECK(!wide.contains(pgl::Point<long long>(1, 1)));
}

// ---------------------------------------------------------------------------
// Self pair: region against region.

namespace {
Region plane() {
    return Region();
}
Region emptyRegion() {
    return Region({yGE0, Halfplane(1, -1, 0, -1)});  // y >= 0 and y <= -1
}
Region upper() {
    return Region(yGE0);  // y >= 0
}
Region hslab(int lo, int hi) {
    return Region({Halfplane(0, lo, 1, lo), Halfplane(1, hi, 0, hi)});  // lo <= y <= hi
}
Region vslab(int lo, int hi) {
    return Region({Halfplane(lo, 1, lo, 0), Halfplane(hi, 0, hi, 1)});  // lo <= x <= hi
}
Region box(int lo, int hi) {
    return Region(pgl::Rectangle<Point>({lo, lo}, {hi, hi}));
}
Region lineY(int y) {
    return Region({Halfplane(0, y, 1, y), Halfplane(1, y, 0, y)});  // the line y == const
}
}  // namespace

TEST_CASE("Region contains another region") {
    CHECK(plane().contains(plane()));
    CHECK(plane().contains(upper()));
    CHECK(plane().contains(box(0, 6)));
    CHECK(!upper().contains(plane()));
    CHECK(upper().contains(hslab(0, 3)));
    CHECK(upper().contains(quadrant()));
    CHECK(!hslab(0, 3).contains(upper()));
    CHECK(hslab(0, 5).contains(hslab(1, 3)));
    CHECK(!hslab(1, 3).contains(hslab(0, 5)));
    CHECK(upper().contains(box(0, 6)));
    CHECK(!box(0, 6).contains(upper()));
    CHECK(box(0, 6).contains(box(1, 5)));

    // The empty region is contained everywhere and contains nothing else.
    CHECK(emptyRegion().empty());
    CHECK(box(0, 6).contains(emptyRegion()));
    CHECK(emptyRegion().contains(emptyRegion()));
    CHECK(!emptyRegion().contains(box(0, 6)));
    CHECK(!emptyRegion().contains(plane()));

    // Degenerate regions are ordinary point sets.
    CHECK(upper().contains(lineY(0)));
    CHECK(upper().contains(lineY(2)));
    CHECK(!upper().contains(lineY(-1)));
    CHECK(lineY(0).contains(lineY(0)));
    CHECK(!lineY(0).contains(lineY(1)));
}

TEST_CASE("Region interior contains another region") {
    CHECK(plane().interiorContains(upper()));
    CHECK(upper().interiorContains(hslab(1, 3)));
    CHECK(!upper().interiorContains(hslab(0, 3)));  // touches the boundary y = 0
    CHECK(!upper().interiorContains(lineY(0)));
    CHECK(upper().interiorContains(lineY(1)));
    CHECK(box(0, 6).interiorContains(box(1, 5)));
    CHECK(!box(0, 6).interiorContains(box(0, 5)));

    // A degenerate region has empty interior, so it only contains the empty region.
    CHECK(lineY(0).interiorContains(emptyRegion()));
    CHECK(!lineY(0).interiorContains(lineY(0)));

    CHECK(upper().interiorContains(emptyRegion()));
    CHECK(!emptyRegion().interiorContains(lineY(0)));
}

TEST_CASE("Region boundary contains another region") {
    // The boundary of the upper half-plane region is the line y = 0.
    CHECK(upper().boundaryContains(lineY(0)));
    CHECK(!upper().boundaryContains(lineY(1)));
    CHECK(!upper().boundaryContains(hslab(0, 1)));  // full-dimensional
    CHECK(!upper().boundaryContains(upper()));
    CHECK(upper().boundaryContains(emptyRegion()));

    // A degenerate region is its own boundary.
    CHECK(lineY(0).boundaryContains(lineY(0)));
    CHECK(!lineY(0).boundaryContains(lineY(1)));

    // The whole plane has empty boundary.
    CHECK(!plane().boundaryContains(lineY(0)));
    CHECK(plane().boundaryContains(emptyRegion()));
}

TEST_CASE("Region intersects another region") {
    CHECK(plane().intersects(plane()));
    CHECK(plane().intersects(upper()));
    CHECK(hslab(0, 3).intersects(vslab(0, 3)));
    CHECK(upper().intersects(quadrant()));
    CHECK(box(0, 6).intersects(box(4, 9)));
    CHECK(!box(0, 6).intersects(box(7, 9)));
    CHECK(!hslab(0, 1).intersects(hslab(3, 4)));

    // Boundary touch counts as intersecting.
    CHECK(hslab(0, 1).intersects(hslab(1, 2)));
    CHECK(upper().intersects(lineY(0)));

    CHECK(!upper().intersects(emptyRegion()));
    CHECK(!emptyRegion().intersects(emptyRegion()));
    CHECK(!emptyRegion().intersects(plane()));
}

TEST_CASE("Region interiors intersect another region's") {
    CHECK(plane().interiorsIntersect(upper()));
    CHECK(hslab(0, 3).interiorsIntersect(vslab(0, 3)));
    CHECK(box(0, 6).interiorsIntersect(box(4, 9)));

    // Touching along a boundary line or point is not an interior meeting.
    CHECK(!hslab(0, 1).interiorsIntersect(hslab(1, 2)));
    CHECK(!box(0, 2).interiorsIntersect(box(2, 4)));

    // Degenerate regions have empty interiors.
    CHECK(!lineY(0).interiorsIntersect(upper()));
    CHECK(!upper().interiorsIntersect(lineY(1)));
    CHECK(!emptyRegion().interiorsIntersect(plane()));
}

TEST_CASE("Region intersection with another region stays a region") {
    const Region cell = hslab(0, 3).intersection<int>(vslab(0, 2));
    CHECK(cell.isBounded());
    CHECK(cell.twiceArea<long long>() == 12);  // 2 x 3

    // Intersecting with the whole plane is the identity.
    CHECK(plane().intersection<int>(hslab(0, 3)) == hslab(0, 3));
    CHECK(hslab(0, 3).intersection<int>(plane()) == hslab(0, 3));

    // Unbounded results stay representable.
    const Region wedgeCut = upper().intersection<int>(quadrant());
    CHECK(wedgeCut == quadrant());
    CHECK(!wedgeCut.isBounded());

    // Disjoint and empty operands produce the canonical empty region.
    CHECK(hslab(0, 1).intersection<int>(hslab(3, 4)).empty());
    CHECK(upper().intersection<int>(emptyRegion()).empty());
    CHECK(emptyRegion().intersection<int>(upper()).empty());
    CHECK(hslab(0, 1).intersection<int>(hslab(3, 4)) == emptyRegion());

    // Exactness: the result type promotes on request.
    const auto exact = hslab(0, 3).intersection<long long>(vslab(0, 2));
    CHECK(exact.twiceArea<long long>() == 12);
}

TEST_CASE("Region separates another region") {
    // Removing a slab from the plane leaves two half-planes.
    CHECK(hslab(0, 1).separates(plane()));
    CHECK(!plane().separates(hslab(0, 1)));

    // Removing a half-plane always leaves a convex (connected) set.
    CHECK(!upper().separates(plane()));
    CHECK(!upper().separates(hslab(-3, 3)));

    // Crossing slabs cut each other.
    CHECK(hslab(0, 1).separates(vslab(0, 1)));
    CHECK(vslab(0, 1).separates(hslab(0, 1)));

    // A vertical slab cuts the upper half-plane in two.
    CHECK(vslab(0, 1).separates(upper()));
    CHECK(!upper().separates(vslab(0, 1)));

    // A slab inside a wider parallel slab splits it.
    CHECK(hslab(1, 2).separates(hslab(-5, 5)));
    CHECK(!hslab(-5, 5).separates(hslab(1, 2)));

    // A quadrant never splits the plane or a half-plane.
    CHECK(!quadrant().separates(plane()));
    CHECK(!quadrant().separates(upper()));

    // A bounded region splits an unbounded one only when it blocks the full
    // width: the box spanning the slab exactly cuts it, a taller slab or a
    // half-plane flows around it.
    CHECK(!box(0, 6).separates(upper()));
    CHECK(box(0, 6).separates(hslab(0, 6)));
    CHECK(!box(0, 6).separates(hslab(-7, 7)));

    // An unbounded remover can split a bounded region.
    CHECK(vslab(2, 4).separates(box(0, 6)));
    CHECK(!vslab(2, 4).separates(box(2, 4)));

    // Degenerate regions: a line region splits the plane and a crossing slab.
    CHECK(lineY(0).separates(plane()));
    CHECK(lineY(0).separates(vslab(0, 1)));
    CHECK(!lineY(0).separates(upper()));  // leaves one open half-plane
    CHECK(!upper().separates(lineY(1)));

    CHECK(!emptyRegion().separates(plane()));
    CHECK(!plane().separates(emptyRegion()));
}

TEST_CASE("Region crosses another region") {
    CHECK(hslab(0, 1).crosses(vslab(0, 1)));
    CHECK(vslab(0, 1).crosses(hslab(0, 1)));
    CHECK(!hslab(0, 1).crosses(plane()));      // the plane does not separate the slab
    CHECK(!hslab(1, 2).crosses(hslab(-5, 5)));
    CHECK(!vslab(0, 1).crosses(upper()));
    CHECK(box(0, 6).crosses(vslab(2, 4)));   // mutual full-width cuts
    CHECK(!vslab(0, 6).crosses(box(2, 4)));  // the box is strictly inside the slab
    CHECK(!emptyRegion().crosses(plane()));
}

TEST_CASE("Region distances to another region") {
    using Rational = pgl::Rational<long long>;

    // Parallel vertical half-planes: x <= -5 and x >= 7.
    const Region left(Halfplane(-5, 0, -5, 1));
    const Region right(Halfplane(7, 1, 7, 0));
    CHECK(left.squaredDistance<int>(right) == 144);
    CHECK(right.squaredDistance<int>(left) == 144);
    CHECK(left.distanceL1<int>(right) == 12);
    CHECK(left.distanceLInf<int>(right) == 12);

    // Opposite quadrants: closest points (0, 0) and (-3, -4).
    const Region q1 = quadrant();
    const Region q3({Halfplane(0, -4, -1, -4), Halfplane(-3, -5, -3, -4)});  // y <= -4 and x <= -3
    CHECK(q1.squaredDistance<int>(q3) == 25);
    CHECK(q1.distanceL1<int>(q3) == 7);
    CHECK(q1.distanceLInf<int>(q3) == 4);
    CHECK(q1.squaredDistance<Rational>(q3) == Rational(25));

    // Intersecting regions are at distance zero.
    CHECK(hslab(0, 3).squaredDistance<int>(vslab(0, 3)) == 0);
    CHECK(plane().squaredDistance<int>(upper()) == 0);
    CHECK(hslab(0, 1).distanceL1<int>(hslab(1, 2)) == 0);  // boundary touch

    // Disjoint parallel slabs: the gap between y <= 1 and y >= 3 sides.
    CHECK(hslab(0, 1).squaredDistance<int>(hslab(3, 4)) == 4);
    CHECK(hslab(0, 1).distanceL1<int>(hslab(3, 4)) == 2);
    CHECK(hslab(0, 1).distanceLInf<int>(hslab(3, 4)) == 2);
}

TEST_CASE("Region Hausdorff distances to another region") {
    using Exact = pgl::ERational;
    const Region square = unitSquare();
    const Region tri = triangle();

    // The square holds the triangle, so only the square's corner (1,1) counts;
    // its nearest triangle point is the middle of the hypotenuse.
    CHECK(square.squaredHausdorffDistance(tri) == Exact(1, 2));
    CHECK(square.hausdorffDistanceL1(tri) == Exact(1));
    CHECK(square.hausdorffDistanceLInf(tri) == Exact(1, 2));

    // The distance is symmetric, and zero exactly on equal point sets.
    CHECK(tri.squaredHausdorffDistance(square) == Exact(1, 2));
    CHECK(tri.hausdorffDistanceL1(square) == Exact(1));
    CHECK(tri.hausdorffDistanceLInf(square) == Exact(1, 2));
    CHECK(square.squaredHausdorffDistance(square) == Exact(0));
    CHECK(square.hausdorffDistanceL1(square) == Exact(0));
    CHECK(square.hausdorffDistanceLInf(square) == Exact(0));

    // The default result type is exact; a requested one is used as asked.
    static_assert(std::is_same_v<decltype(square.squaredHausdorffDistance(tri)), Exact>);
    CHECK(square.squaredHausdorffDistance<double>(tri) == doctest::Approx(0.5));
    CHECK(square.squaredHausdorffDistance<int>(tri) == 0);   // truncated

    // Either operand being unbounded leaves the distance infinite.
    CHECK_THROWS_AS((void)quadrant().squaredHausdorffDistance(square), std::logic_error);
    CHECK_THROWS_AS((void)square.squaredHausdorffDistance(quadrant()), std::logic_error);
    CHECK_THROWS_AS((void)strip().hausdorffDistanceL1(square), std::logic_error);
    CHECK_THROWS_AS((void)square.hausdorffDistanceLInf(quadrant()), std::logic_error);
}

TEST_CASE("A region measures like the shape it was built from") {
    using Exact = pgl::ERational;
    const pgl::Triangle<Point> source(Point(0, 0), Point(4, 0), Point(0, 3));
    const pgl::Rectangle<Point> other(Point(6, 1), Point(8, 5));
    const Region built(source);

    CHECK(built.squaredHausdorffDistance(other) == source.squaredHausdorffDistance<Exact>(other));
    CHECK(built.hausdorffDistanceL1(other) == source.hausdorffDistanceL1<Exact>(other));
    CHECK(built.hausdorffDistanceLInf(other) == source.hausdorffDistanceLInf<Exact>(other));

    // Both operands as regions give the same answers.
    const Region otherRegion(other);
    CHECK(built.squaredHausdorffDistance(otherRegion) == source.squaredHausdorffDistance<Exact>(other));
    CHECK(built.hausdorffDistanceL1(otherRegion) == source.hausdorffDistanceL1<Exact>(other));
    CHECK(built.hausdorffDistanceLInf(otherRegion) == source.hausdorffDistanceLInf<Exact>(other));
}

TEST_CASE("Region self-pair works with rational coordinates") {
    using RPoint = pgl::Point<pgl::Rational<long long>>;
    using RHalfplane = pgl::Halfplane<RPoint>;
    using RRegion = pgl::HalfplaneIntersection<RPoint>;

    const RRegion a({RHalfplane(RPoint(0, 0), RPoint(1, 0))});   // y >= 0
    const RRegion b({RHalfplane(RPoint(0, 1), RPoint(0, 0)),     // x >= 0
                     RHalfplane(RPoint(2, 0), RPoint(2, 1))});   // x <= 2
    CHECK(a.contains(a));
    CHECK(a.intersects(b));
    CHECK(b.separates(a));
    CHECK(!a.separates(b));
    const RRegion meet = a.intersection<int>(b);
    CHECK(!meet.isBounded());
    CHECK(meet.contains(RPoint(1, 5)));
    CHECK(!meet.contains(RPoint(1, -1)));
}

TEST_CASE("HalfplaneIntersection latticePoints needs a bounded region") {
    CHECK(unitSquare().latticePoints()
          == std::vector<Point>{{0, 0}, {0, 1}, {1, 0}, {1, 1}});
    CHECK(triangle().latticePoints() == std::vector<Point>{{0, 0}, {0, 1}, {1, 0}});

    // The vertices of a region are crossings, so they need not be lattice
    // points; the ones it covers are found all the same.
    const Region halved({yGE0, xGE0, Halfplane(3, 0, 0, 3), Halfplane(0, 0, 1, 0)});
    for (const Point& point : halved.latticePoints()) {
        CHECK(halved.contains(point));
    }

    // Unbounded regions have infinitely many, so they are refused rather than
    // truncated -- as every vertex list of one is.
    CHECK_THROWS_AS(static_cast<void>(quadrant().latticePoints()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(strip().latticePoints()), std::logic_error);
    CHECK_THROWS_AS(static_cast<void>(Region().latticePoints()), std::logic_error);
    CHECK(emptyRegion().latticePoints().empty());
}

namespace {
// A strictly convex lattice polygon: the primitive vectors of [-r, r]^2 laid
// end to end in direction order. Its vertex count grows like r^2.
std::vector<Point> latticeRing(int r) {
    std::vector<Point> steps;
    for (int dx = -r; dx <= r; ++dx) {
        for (int dy = -r; dy <= r; ++dy) {
            if ((dx != 0 || dy != 0) && std::gcd(dx, dy) == 1) {
                steps.emplace_back(dx, dy);
            }
        }
    }
    std::sort(steps.begin(), steps.end(), [](const Point& u, const Point& v) {
        return pgl::detail::minkowskiDirectionOrder(u, v) < 0;
    });
    std::vector<Point> ring;
    Point at(0, 0);
    for (const Point& step : steps) {
        ring.push_back(at);
        at = Point(at.x() + step.x(), at.y() + step.y());
    }
    return ring;
}

std::vector<Halfplane> edgeHalfplanes(const std::vector<Point>& ring) {
    std::vector<Halfplane> edges;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        edges.emplace_back(ring[i], ring[(i + 1) % ring.size()]);
    }
    return edges;
}

Halfplane randomHalfplane(std::mt19937& rng, int c) {
    std::uniform_int_distribution<int> coordinate(-c, c);
    for (;;) {
        const Point a(coordinate(rng), coordinate(rng));
        const Point b(coordinate(rng), coordinate(rng));
        if (a != b) {
            return Halfplane(a, b);
        }
    }
}
}  // namespace

TEST_CASE("Insert drops a long run of redundant half-planes at once") {
    const std::vector<Point> ring = latticeRing(12);
    const std::vector<Halfplane> edges = edgeHalfplanes(ring);
    int low = ring.front().y();
    int high = low;
    int left = ring.front().x();
    int right = left;
    for (const Point& p : ring) {
        low = std::min(low, p.y());
        high = std::max(high, p.y());
        left = std::min(left, p.x());
        right = std::max(right, p.x());
    }
    const int cut = low + (high - low) / 2;
    const int xcut = left + (right - left) / 2;
    // Cutting either half away removes a contiguous run of about n/2 stored
    // constraints, and the run wraps around the end of the storage for the
    // lower cut (the stored order starts at direction 0).
    for (const Halfplane& knife : {Halfplane(Point(1, cut), Point(0, cut)), Halfplane(Point(0, cut), Point(1, cut)),
                                   Halfplane(Point(xcut, 0), Point(xcut, 1)), Halfplane(Point(xcut, 1), Point(xcut, 0))}) {
        Region inserted(edges);
        const std::size_t before = inserted.size();
        CHECK(inserted.insert(knife));
        std::vector<Halfplane> all = edges;
        all.push_back(knife);
        const Region built(all);
        CHECK(inserted == built);
        CHECK(inserted.size() < before / 2 + 3);
        for (const Point& p : ring) {
            CHECK(inserted.contains(p) == (Region(edges).contains(p) && knife.contains(p)));
        }
    }
}

TEST_CASE("Insert agrees with the range constructor on random constraints") {
    std::mt19937 rng(20260915);
    for (int trial = 0; trial < 3000; ++trial) {
        std::vector<Halfplane> constraints;
        const int count = static_cast<int>(rng() % 14);
        for (int i = 0; i < count; ++i) {
            constraints.push_back(randomHalfplane(rng, 1 + static_cast<int>(rng() % 6)));
        }
        Region inserted;
        for (const Halfplane& h : constraints) {
            inserted.insert(h);
        }
        const Region built(constraints);
        CHECK(inserted.empty() == built.empty());
        CHECK(inserted.isDegenerate() == built.isDegenerate());
        CHECK(inserted.samePointSet(built));
        if (!inserted.isDegenerate()) {
            CHECK(inserted == built);
        }
        // Already sorted input takes the linear path and must agree with it.
        std::sort(constraints.begin(), constraints.end(),
                  [](const Halfplane& a, const Halfplane& b) { return pgl::detail::directionLess(a, b); });
        const Region sorted(constraints);
        CHECK(sorted == built);
        CHECK(sorted.isDegenerate() == built.isDegenerate());
    }
}

TEST_CASE("insertChanges answers what insert would, without inserting") {
    std::mt19937 rng(7);
    for (int trial = 0; trial < 3000; ++trial) {
        Region region;
        const int count = static_cast<int>(rng() % 10);
        for (int i = 0; i < count; ++i) {
            region.insert(randomHalfplane(rng, 4));
        }
        if (trial % 5 == 1) {
            region = Region(Point(static_cast<int>(rng() % 5) - 2, static_cast<int>(rng() % 5) - 2));
        } else if (trial % 5 == 2) {
            region = Region(pgl::Segment<Point>(Point(0, 0), Point(static_cast<int>(rng() % 5) - 2, 2)));
        } else if (trial % 5 == 3) {
            region = Region(pgl::Line<Point>(Point(0, 0), Point(static_cast<int>(rng() % 5) - 2, 1)));
        }
        for (int q = 0; q < 8; ++q) {
            const Halfplane h = randomHalfplane(rng, 5);
            Region copy = region;
            const bool changed = copy.insert(h);
            CHECK(region.insertChanges(h) == changed);
            // The reverse predicates that read it, against the copy-and-insert
            // definition they used to spell out.
            const auto inside = [&region](const Halfplane& g) {
                Region scratch = region;
                return !scratch.insert(g);
            };
            CHECK(h.contains(region) == inside(h));
            const pgl::Line<Point> line(h.source(), h.target());
            CHECK(line.contains(region) == (inside(h) && inside(h.opposite())));
            if (!region.empty() && !region.isDegenerate()) {
                CHECK(line.separates(region) == (!inside(h) && !inside(h.opposite())));
                CHECK(region.crosses(line) == line.crosses(region));
            }
            CHECK(h.source().contains(region) ==
                  (inside(Halfplane(h.source(), Point(h.source().x() + 1, h.source().y()))) &&
                   inside(Halfplane(Point(h.source().x() + 1, h.source().y()), h.source())) &&
                   inside(Halfplane(h.source(), Point(h.source().x(), h.source().y() + 1))) &&
                   inside(Halfplane(Point(h.source().x(), h.source().y() + 1), h.source()))));
        }
    }
}

TEST_CASE("asConvex matches the hull of the vertices") {
    std::mt19937 rng(11);
    for (int trial = 0; trial < 3000; ++trial) {
        Region region;
        const int count = 3 + static_cast<int>(rng() % 8);
        for (int i = 0; i < count; ++i) {
            region.insert(randomHalfplane(rng, 5));
        }
        if (trial % 4 == 1) {
            region = Region(pgl::Segment<Point>(Point(static_cast<int>(rng() % 5), 1), Point(-2, static_cast<int>(rng() % 5))));
        } else if (trial % 4 == 2) {
            region = Region(Point(1, 2));
        }
        if (region.empty() || !region.isBounded()) {
            continue;
        }
        using EPoint = pgl::Point<pgl::ERational>;
        CHECK(region.asConvex<pgl::ERational>() == pgl::Convex<EPoint>(region.vertices<pgl::ERational>()));
        CHECK(region.asConvex<int>() == pgl::Convex<Point>(region.vertices<int>()));
        CHECK(region.asConvex<double>() == pgl::Convex<pgl::Point<double>>(region.vertices<double>()));
    }
    const std::vector<Point> ring = latticeRing(6);
    const Region polygon(edgeHalfplanes(ring));
    CHECK(polygon.asConvex<pgl::ERational>() ==
          pgl::Convex<pgl::Point<pgl::ERational>>(polygon.vertices<pgl::ERational>()));
}
