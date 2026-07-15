#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <compare>
#include <sstream>
#include <stdexcept>
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
    CHECK(!plane.isEmpty());
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

TEST_CASE("Emptiness is detected and sticky") {
    Region k({yGE0});
    CHECK(k.insert(Halfplane(1, -1, 0, -1)));  // y <= -1 contradicts y >= 0
    CHECK(k.isEmpty());
    CHECK(k.size() == 0);
    CHECK(!k.isPlane());
    CHECK(k.isBounded());  // the empty set is bounded
    CHECK(k.isDegenerate());
    CHECK(!k.contains(Point(0, 0)));
    CHECK(!k.insert(yGE0));  // inserting into the empty region changes nothing
    CHECK(k.isEmpty());

    SUBCASE("all empty regions are equal") {
        Region other({xGE0});
        other.insert(Halfplane(-1, 0, -1, 1));  // x <= -1
        CHECK(other.isEmpty());
        CHECK(k == other);
        CHECK(std::hash<Region>{}(k) == std::hash<Region>{}(other));
    }
}

TEST_CASE("Degenerate regions are tracked and behave as sets") {
    SUBCASE("two opposite half-planes give a line") {
        const Region line({yGE0, Halfplane(1, 0, 0, 0)});  // y >= 0 and y <= 0
        CHECK(line.isDegenerate());
        CHECK(!line.isEmpty());
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
        CHECK(k.asConvex() == convex);
    }

    SUBCASE("an empty convex polygon gives the empty region") {
        const Region k(pgl::Convex<Point>{});
        CHECK(k.isEmpty());
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
        const auto box = unitSquare().bbox();
        CHECK(box == pgl::Rectangle<Point>(Point(0, 0), Point(1, 1)));
    }

    SUBCASE("integer boxes round outward") {
        // y >= 0, x >= 0, x + 2y <= 1: y_max = 1/2 rounds up to 1.
        const Region k({yGE0, xGE0, Halfplane(1, 0, -1, 1)});
        CHECK(k.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(1, 1)));
        // Mirrored below the axes: y_min = -1/2 rounds down to -1.
        const Region m({Halfplane(0, 0, -1, 0), Halfplane(0, 0, 0, 1), Halfplane(-1, 0, 1, -1)});
        CHECK(m.bbox() == pgl::Rectangle<Point>(Point(-1, -1), Point(0, 0)));
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
    const auto cut = k.intersection(diag);
    CHECK(cut == triangle());
    // Cutting the whole thing away yields the empty region, not an optional.
    const auto gone = k.intersection(Halfplane(1, -1, 0, -1));  // y <= -1
    CHECK(gone.isEmpty());
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
