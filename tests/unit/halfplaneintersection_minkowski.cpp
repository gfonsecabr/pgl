#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "pgl.hpp"

// The Minkowski sums whose result is unbounded, and so is a
// `HalfplaneIntersection`: an unbounded convex operand (`Halfplane`, `Line`,
// `OrientedLine`, `Ray`, `HalfplaneIntersection`) against another one or
// against a bounded convex shape. The bounded-shape-valued sums live in
// minkowski.cpp, and the region-valued non-convex ones in the three
// `*_minkowski.cpp` files named for their receivers.

using Point = pgl::Point<int>;
using QPoint = pgl::Point<pgl::ERational>;
using Region = pgl::HalfplaneIntersection<Point>;
using QRegion = pgl::HalfplaneIntersection<QPoint>;
using Halfplane = pgl::Halfplane<Point>;
using Segment = pgl::Segment<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using RectangleShape = pgl::Rectangle<Point>;

// The definition read as a query: `p ∈ A ⊕ B` exactly when `A` meets the
// reflected copy of `B` placed at `p`, since a 180° rotation about the origin
// *is* the negation. It settles unbounded operands as readily as bounded ones.
template <class A, class B, class P>
static bool inSumByDefinition(const A& a, const B& b, const P& p) {
    auto placed = b.rotated90(2);
    placed += p;
    return a.intersects(placed);
}

// Probes the sum against the definition on an integer grid, in both spellings.
template <class A, class B>
static void agreesWithDefinition(const A& a, const B& b, int range = 12) {
    const auto sum = a.minkowskiSum(b);
    const auto mirror = b.minkowskiSum(a);
    for (int x = -range; x <= range; ++x) {
        for (int y = -range; y <= range; ++y) {
            const Point p(x, y);
            const bool truth = inSumByDefinition(a, b, p);
            INFO("probe " << p << " in " << sum);
            CHECK(sum.contains(p) == truth);
            CHECK(mirror.contains(p) == truth);
        }
    }
}

TEST_CASE("A line and a bounded convex shape sum to the slab that sweeps it") {
    const pgl::Line<Point> xAxis(Point(0, 0), Point(1, 0));
    const Triangle triangle(Point(0, 0), Point(3, 0), Point(0, 2));

    const auto slab = xAxis.minkowskiSum(triangle);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(slab)>, Region>,
                  "an unbounded sum comes back as a region, over the operands' own coordinates");
    CHECK(slab.size() == 2);
    CHECK_FALSE(slab.isBounded());
    CHECK(slab == Region(std::vector<Halfplane>{Halfplane(Point(0, 0), Point(1, 0)),
                                                Halfplane(Point(0, 2), Point(-1, 2))}));
    CHECK(slab == triangle.minkowskiSum(xAxis));  // the sum commutes
    CHECK((xAxis + triangle) == slab);

    // Perpendicular extent is all the operand contributes, so a segment across
    // the same span gives the same slab and a parallel one gives none at all.
    CHECK(xAxis.minkowskiSum(Segment(1, 0, 4, 2)) == slab);
    const auto stillALine = xAxis.minkowskiSum(Segment(0, 0, 5, 0));
    CHECK(stillALine.isLine());
    CHECK(*stillALine.getIfLine() == pgl::Line<Point>(0, 0, 1, 0));

    agreesWithDefinition(xAxis, triangle);
    agreesWithDefinition(pgl::Line<Point>(Point(1, 1), Point(3, 2)), Convex({0, 0, 2, 1, 1, 3}));
}

TEST_CASE("A ray sweeps its operand along itself, and keeps its cap") {
    const pgl::Ray<Point> east(Point(0, 0), Point(1, 0));
    const pgl::Ray<Point> north(Point(0, 0), Point(0, 1));

    // Two rays from the origin span the quadrant between them.
    const auto quadrant = east.minkowskiSum(north);
    CHECK(quadrant.size() == 2);
    CHECK(quadrant == Region(std::vector<Halfplane>{Halfplane(Point(0, 0), Point(1, 0)),
                                                    Halfplane(Point(0, 0), Point(0, -1))}));

    // A ray and a segment across it sweep out a half-strip: two parallel sides
    // and the cap the ray's source contributes.
    const auto strip = east.minkowskiSum(Segment(0, 0, 0, 2));
    CHECK(strip.size() == 3);
    CHECK(strip.contains(Point(7, 1)));
    CHECK_FALSE(strip.contains(Point(-1, 1)));
    CHECK_FALSE(strip.contains(Point(7, 3)));

    // A collapsed operand is a translation, and the ray comes back as one: a
    // ray is three constraints, so the region reports it rather than a shape.
    const auto moved = east.minkowskiSum(Segment(3, 4, 3, 4));
    CHECK(moved.isRay());
    CHECK(*moved.template getIfRay<int>() == pgl::Ray<Point>(Point(3, 4), Point(4, 4)));

    agreesWithDefinition(east, Triangle(Point(0, 0), Point(2, 1), Point(-1, 3)));
    agreesWithDefinition(pgl::Ray<Point>(Point(1, -2), Point(-1, 1)), RectangleShape(0, 0, 3, 2));
    agreesWithDefinition(east, north);
    agreesWithDefinition(pgl::Ray<Point>(Point(2, 1), Point(3, 3)),
                         pgl::Ray<Point>(Point(-1, 0), Point(-3, 1)));
}

TEST_CASE("Two half-planes sum to a half-plane when they face the same way, and to the plane otherwise") {
    const Halfplane up(Point(0, 0), Point(1, 0));    // y >= 0
    const Halfplane up3(Point(5, 3), Point(7, 3));   // y >= 3

    const auto parallel = up.minkowskiSum(up3);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(parallel)>, Region>);
    CHECK(parallel.isHalfplane());
    CHECK(*parallel.getIfHalfplane() == Halfplane(Point(0, 3), Point(1, 3)));

    // Anti-parallel and crossing boundaries both leave nothing to constrain:
    // the two half-planes between them reach every point of the plane.
    CHECK(up.minkowskiSum(Halfplane(Point(0, 9), Point(-1, 9))).isPlane());
    CHECK(up.minkowskiSum(Halfplane(Point(4, 0), Point(4, 1))).isPlane());

    // A line or a ray along the boundary direction still bounds nothing else.
    CHECK(up.minkowskiSum(pgl::Line<Point>(Point(0, 5), Point(3, 5))) ==
          Region(Halfplane(Point(0, 5), Point(1, 5))));
    CHECK(up.minkowskiSum(pgl::Ray<Point>(Point(0, 5), Point(3, 5))) ==
          Region(Halfplane(Point(0, 5), Point(1, 5))));
    CHECK(up.minkowskiSum(pgl::Line<Point>(Point(0, 5), Point(3, 6))).isPlane());

    agreesWithDefinition(up, Halfplane(Point(1, 2), Point(-1, 3)));
    agreesWithDefinition(up, pgl::Ray<Point>(Point(1, 2), Point(-1, 3)));
    agreesWithDefinition(up, pgl::Line<Point>(Point(1, 2), Point(-1, 3)));
    agreesWithDefinition(Halfplane(Point(0, 0), Point(2, 1)), Triangle(0, 0, 3, 1, 1, 4));
}

TEST_CASE("A region sums with a bounded convex shape, over rational coordinates") {
    const Region box{RectangleShape(0, 0, 2, 2)};
    const Triangle triangle(Point(0, 0), Point(3, 0), Point(0, 2));

    const auto grown = box.minkowskiSum(triangle);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(grown)>, QRegion>,
                  "a region's own vertices are line crossings, so its sums are rational");
    // Bounded on both sides, so the answer is the bounded convex sum, spelled
    // as a region.
    CHECK(grown.isBounded());
    CHECK(grown.template asConvex<int>() == Convex({0, 0, 2, 0, 2, 2, 0, 2}).minkowskiSum(triangle));
    CHECK(grown == triangle.minkowskiSum(box));

    // An unbounded region keeps what it recedes in and moves its constraints to
    // the operand's support points.
    Region wedge;
    wedge.insert(Halfplane(Point(0, 0), Point(1, 0)));   // y >= 0
    wedge.insert(Halfplane(Point(0, 0), Point(0, -1)));  // x >= 0
    const auto pushed = wedge.minkowskiSum(RectangleShape(1, 1, 4, 3));
    CHECK(pushed == QRegion(std::vector<pgl::Halfplane<QPoint>>{
                        pgl::Halfplane<QPoint>(QPoint(1, 1), QPoint(2, 1)),
                        pgl::Halfplane<QPoint>(QPoint(1, 1), QPoint(1, 0))}));

    agreesWithDefinition(wedge, triangle);
    agreesWithDefinition(wedge, Segment(-2, 1, 1, -3));
    agreesWithDefinition(box, Convex({0, 0, 2, 1, 1, 3}));
}

TEST_CASE("A region sums with another unbounded shape") {
    Region wedge;
    wedge.insert(Halfplane(Point(0, 0), Point(1, 0)));   // y >= 0
    wedge.insert(Halfplane(Point(0, 0), Point(0, -1)));  // x >= 0

    // A half-plane the wedge already recedes into absorbs it entirely.
    CHECK(wedge.minkowskiSum(Halfplane(Point(0, -4), Point(1, -4))) ==
          QRegion(pgl::Halfplane<QPoint>(QPoint(0, -4), QPoint(1, -4))));
    // One facing the other way leaves nothing at all.
    CHECK(wedge.minkowskiSum(Halfplane(Point(0, 4), Point(-1, 4))).isPlane());

    agreesWithDefinition(wedge, pgl::Ray<Point>(Point(1, 1), Point(-1, 2)));
    agreesWithDefinition(wedge, pgl::Line<Point>(Point(1, 1), Point(-1, 2)));
    agreesWithDefinition(wedge, Halfplane(Point(2, -1), Point(1, 2)));

    Region slab;
    slab.insert(Halfplane(Point(0, 1), Point(1, 1)));
    slab.insert(Halfplane(Point(0, 4), Point(-1, 4)));
    agreesWithDefinition(slab, wedge);
    agreesWithDefinition(slab, Triangle(0, 0, 2, 1, 1, 3));
}

TEST_CASE("Empty and full operands absorb") {
    const pgl::Line<Point> xAxis(Point(0, 0), Point(1, 0));
    const Triangle triangle(Point(0, 0), Point(3, 0), Point(0, 2));

    // An empty convex polygon has no point to add.
    CHECK(xAxis.minkowskiSum(Convex()).empty());

    Region empty;
    empty.insert(Halfplane(Point(0, 0), Point(1, 0)));
    empty.insert(Halfplane(Point(0, -1), Point(-1, -1)));
    REQUIRE(empty.empty());
    CHECK(empty.minkowskiSum(triangle).empty());
    CHECK(empty.minkowskiSum(xAxis).empty());
    CHECK(triangle.minkowskiSum(empty).empty());

    // The whole plane absorbs every non-empty operand.
    CHECK(Region().minkowskiSum(triangle).isPlane());
    CHECK(Region().minkowskiSum(xAxis).isPlane());
    CHECK(Region().minkowskiSum(Region()).isPlane());
    CHECK(Region().minkowskiSum(Convex()).empty());

    // The empty shape still outranks everything.
    CHECK(xAxis.minkowskiSum(pgl::EmptyShape<Point>{}) == pgl::EmptyShape<Point>{});
}

TEST_CASE("A degenerate region behaves like the point set it covers") {
    const Triangle triangle(Point(0, 0), Point(3, 0), Point(0, 2));

    // A point region is a translation.
    const Region point{Point(2, 1)};
    REQUIRE(point.isDegenerate());
    CHECK(point.minkowskiSum(triangle).template asConvex<int>() ==
          Convex({2, 1, 5, 1, 2, 3}));

    // A segment region sums like the segment it is.
    const Region segment{Segment(0, 0, 2, 0)};
    CHECK(segment.minkowskiSum(triangle).template asConvex<int>() ==
          Segment(0, 0, 2, 0).minkowskiSum(triangle));

    // A line region and a segment across it sweep out a slab, which has area
    // again: a degenerate operand does not make the sum degenerate.
    const Region line{pgl::Line<Point>(0, 0, 1, 1)};
    const auto slab = line.minkowskiSum(Segment(0, 0, 0, 2));
    CHECK_FALSE(slab.isDegenerate());
    CHECK(slab.contains(Point(1, 2)));
    CHECK_FALSE(slab.contains(Point(1, 4)));

    agreesWithDefinition(point, triangle);
    agreesWithDefinition(segment, pgl::Ray<Point>(Point(0, 0), Point(1, 2)));
    agreesWithDefinition(line, Triangle(0, 0, 2, 1, 1, 3));
}

TEST_CASE("Oriented operands sum as the point sets they are") {
    const pgl::OrientedLine<Point> line(Point(0, 0), Point(1, 0));
    const pgl::OrientedSegment<Point> segment(Point(0, 2), Point(0, 0));

    const auto slab = line.minkowskiSum(segment);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(slab)>, Region>);
    CHECK(slab == pgl::Line<Point>(0, 0, 1, 0).minkowskiSum(pgl::Segment<Point>(0, 0, 0, 2)));
    CHECK((line + segment) == slab);
    CHECK(segment.minkowskiSum(line) == slab);
}

TEST_CASE("Integral operands stay integral, and only a region asks for fractions") {
    const pgl::Ray<Point> ray(Point(0, 0), Point(1, 0));
    static_assert(std::is_same_v<decltype(ray.minkowskiSum(Triangle(0, 0, 1, 0, 0, 1)))::NumberType,
                  int>);
    static_assert(std::is_same_v<decltype(ray.minkowskiSum(Halfplane(0, 0, 1, 1)))::NumberType,
                  int>);
    static_assert(std::is_same_v<decltype(Region().minkowskiSum(ray))::NumberType, pgl::ERational>);

    // A wider operand widens the result, exactly as the bounded sums promote.
    const pgl::Line<pgl::Point<double>> line(pgl::Point<double>(0, 0), pgl::Point<double>(1, 0));
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(line.minkowskiSum(Triangle(0, 0, 1, 0, 0, 1)))>,
                       pgl::HalfplaneIntersection<pgl::Point<double>>>);

    // A vertex of the region falls between the lattice points, and the sum
    // reports it exactly rather than rounding it.
    Region wedge;
    wedge.insert(Halfplane(Point(0, 0), Point(2, 1)));
    wedge.insert(Halfplane(Point(1, 0), Point(1, 3)));
    const auto pushed = wedge.minkowskiSum(Segment(0, 0, 0, 1));
    CHECK(pushed.contains(QPoint(pgl::ERational(1), pgl::ERational(1, 2))));
    CHECK_FALSE(pushed.contains(QPoint(pgl::ERational(1), pgl::ERational(1, 4))));
}

TEST_CASE("The wrapper dispatches on the stored alternatives") {
    const pgl::Shape<Point> ray = pgl::Ray<Point>(Point(0, 0), Point(1, 0));
    const pgl::Shape<Point> triangle = Triangle(Point(0, 0), Point(2, 1), Point(-1, 3));

    const auto sum = ray.minkowskiSum(triangle);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(sum)>, pgl::Shape<Point>>);
    REQUIRE(sum.isHalfplaneIntersection());
    CHECK(*sum.getIfHalfplaneIntersection() ==
          pgl::Ray<Point>(Point(0, 0), Point(1, 0))
              .minkowskiSum(Triangle(Point(0, 0), Point(2, 1), Point(-1, 3))));

    // A region alternative has an answer, but a rational one, which a wrapper
    // over integer points cannot hold — so this pair is refused at run time.
    const pgl::Shape<Point> region = Region{RectangleShape(0, 0, 2, 2)};
    CHECK_THROWS_AS(static_cast<void>(region.minkowskiSum(triangle)), std::logic_error);

    // Over rational points there is nothing left to refuse.
    const pgl::Shape<QPoint> qregion = QRegion(Region{RectangleShape(0, 0, 2, 2)});
    const pgl::Shape<QPoint> qtriangle = pgl::Triangle<QPoint>(QPoint(0, 0), QPoint(3, 0), QPoint(0, 2));
    CHECK(qregion.minkowskiSum(qtriangle).isHalfplaneIntersection());
}

TEST_CASE("Random unbounded sums agree with the definition") {
    std::mt19937 rng(20260810);
    std::uniform_int_distribution<int> coordinate(-4, 4);
    std::uniform_int_distribution<int> count(0, 3);
    const auto c = [&]() { return coordinate(rng); };
    const auto direction = [&]() {
        while (true) {
            const Point d(c(), c());
            if (d != Point(0, 0)) {
                return d;
            }
        }
    };

    for (int trial = 0; trial < 40; ++trial) {
        const Point source(c(), c());
        const Point step = direction();
        const Point target(source.x() + step.x(), source.y() + step.y());
        const pgl::Ray<Point> ray(source, target);
        const pgl::Line<Point> line(source, target);
        const Halfplane halfplane(source, target);

        const Triangle triangle(Point(c(), c()), Point(c(), c()), Point(c(), c()));
        std::vector<Point> vertices;
        for (int i = 0, n = count(rng) + 1; i < n; ++i) {
            vertices.emplace_back(c(), c());
        }
        const Convex convex(vertices);

        Region region;
        for (int i = 0, n = count(rng); i < n; ++i) {
            const Point on(c(), c());
            const Point along = direction();
            region.insert(Halfplane(on, Point(on.x() + along.x(), on.y() + along.y())));
        }
        if (region.empty()) {
            continue;
        }

        agreesWithDefinition(ray, triangle, 9);
        agreesWithDefinition(line, convex, 9);
        agreesWithDefinition(halfplane, ray, 9);
        agreesWithDefinition(region, triangle, 9);
        agreesWithDefinition(region, line, 9);
        agreesWithDefinition(region, region, 9);
    }
}
