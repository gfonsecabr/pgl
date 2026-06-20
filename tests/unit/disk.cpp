#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "pgl.hpp"

static_assert(std::is_same_v<pgl::Disk<>, pgl::Disk<pgl::Point<int>>>);

TEST_CASE_TEMPLATE("Disk stores three boundary points and computes center and radius", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Disk = pgl::Disk<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Disk degenerate;
    CHECK(degenerate.a() == make_point(Number{}, Number{}, ""));
    CHECK(degenerate.b() == make_point(Number{}, Number{}, ""));
    CHECK(degenerate.c() == make_point(Number{}, Number{}, ""));
    CHECK(degenerate.center() == make_point(Number{}, Number{}, ""));
    CHECK(degenerate.radius() == 0);
    CHECK(degenerate.isDegenerate());

    const Disk disk(make_point(static_cast<Number>(2), static_cast<Number>(3), "c"), static_cast<Number>(-5));
    CHECK(disk.center().x() == Number(2));
    CHECK(disk.center().y() == Number(3));
    CHECK(disk.radius() == 5);
    CHECK(disk.squaredRadius() == Number(25));

    if constexpr (requires { disk.center().label(); }) {
        CHECK(disk.center().label().empty());
    }
}

TEST_CASE("Disk supports default template parameters and CTAD") {
    const pgl::Disk<> explicit_default({2, 3}, 4);
    pgl::Disk deduced(2, 3, 4);

    static_assert(std::is_same_v<decltype(deduced), pgl::Disk<pgl::Point<int>>>);

    CHECK(explicit_default.center() == pgl::Point<int>(2, 3));
    CHECK(explicit_default.radius() == 4);
    CHECK(deduced.center() == pgl::Point<int>(2, 3));
    CHECK(deduced.radius() == 4);
    CHECK(deduced.a() == pgl::Point<int>(-2, 3));
    CHECK(deduced.b() == pgl::Point<int>(6, 3));
    CHECK(deduced.c() == pgl::Point<int>(2, 7));
}

TEST_CASE("Disk streams, translates, scales, and exposes its bounding box") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk disk({2, 3}, 4);

    std::ostringstream stream;
    stream << disk;
    CHECK(stream.str() == "Disk((-2,3)(6,3)(2,7))");

    const auto translated = disk + Point(1, -1);
    const auto shifted = translated - Point(2, 1);
    const auto scaled = -2 * disk;

    CHECK(translated.center() == Point(3, 2));
    CHECK(shifted.center() == Point(1, 1));
    CHECK(scaled.center() == Point(-4, -6));
    CHECK(scaled.radius() == 8);

    CHECK(disk.fbox<float>().min() == pgl::Point<float>(-2.0f, -1.0f));
    CHECK(disk.fbox<float>().max() == pgl::Point<float>(6.0f, 7.0f));
}

TEST_CASE("Disk converts between labeled and unlabeled centers and supports hashing") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainDisk = pgl::Disk<PlainPoint>;
    using LabelDisk = pgl::Disk<LabelPoint, std::string>;

    const LabelDisk labeled(LabelPoint(2, 3, "center-point"), 4, "disk");
    const PlainDisk plain = labeled;
    const LabelDisk relabeled = plain;

    CHECK(plain.center() == PlainPoint(2, 3));
    CHECK(plain.radius() == 4);
    CHECK(relabeled.center() == LabelPoint(2, 3, ""));
    CHECK(relabeled.radius() == 4);
    CHECK(labeled.label() == "disk");
    CHECK(relabeled.label().empty());

    std::set<PlainDisk> ordered{plain, PlainDisk({2, 3}, 4), PlainDisk({2, 3}, 5)};
    CHECK(ordered.size() == 2);

    std::unordered_set<PlainDisk> hashed{plain, PlainDisk({2, 3}, 4), PlainDisk({2, 3}, 5)};
    CHECK(hashed.size() == 2);
}

TEST_CASE("Disk equality and hashing compare the represented circle") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk from_center(Point(0, 0), 5);
    const Disk from_boundary(Point(5, 0), Point(0, 5), Point(-5, 0));
    const Disk different(Point(1, 0), 5);

    CHECK(from_center == from_boundary);
    CHECK(from_center != different);

    std::set<Disk> ordered{from_center, from_boundary, different};
    CHECK(ordered.size() == 2);

    std::unordered_set<Disk> hashed{from_center, from_boundary, different};
    CHECK(hashed.size() == 2);
}

TEST_CASE("Disk boundaryContains detects points on the circle") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Disk disk(Point(0, 0), 5);

    CHECK(disk.boundaryContains(Point(5, 0)));
    CHECK(disk.boundaryContains(Point(0, -5)));
    CHECK_FALSE(disk.boundaryContains(Point(0, 0)));
    CHECK_FALSE(disk.boundaryContains(Point(6, 0)));

    const Disk degenerate;
    CHECK(degenerate.boundaryContains(Point(0, 0)));
    CHECK_FALSE(degenerate.boundaryContains(Point(1, 0)));

    const Disk collinear(Point(0, 0), Point(2, 0), Point(4, 0));
    CHECK(collinear.boundaryContains(Point(2, 0)));
    CHECK(collinear.boundaryContains(Point(3, 0)));
    CHECK(collinear.boundaryContains(Point(5, 0)));
    CHECK_FALSE(collinear.boundaryContains(Point(2, 1)));

    CHECK(disk.boundaryContains(Segment(Point(5, 0), Point(5, 0))));
    CHECK_FALSE(disk.boundaryContains(Segment(Point(-5, 0), Point(5, 0))));
    CHECK_FALSE(disk.boundaryContains(Segment(Point(0, 0), Point(0, 0))));

    CHECK(disk.boundaryContains(Line(Point(5, 0), Point(5, 0))));
    CHECK_FALSE(disk.boundaryContains(Line(Point(-5, 0), Point(5, 0))));
    CHECK(disk.boundaryContains(Ray(Point(5, 0), Point(5, 0))));
    CHECK_FALSE(disk.boundaryContains(Ray(Point(5, 0), Point(6, 0))));
    CHECK(disk.boundaryContains(Halfplane(Point(5, 0), Point(5, 0))));
    CHECK_FALSE(disk.boundaryContains(Halfplane(Point(5, 0), Point(6, 0))));

    CHECK(disk.boundaryContains(Disk(Point(0, 0), 5)));
    CHECK_FALSE(disk.boundaryContains(Disk(Point(0, 0), 4)));

    CHECK(collinear.boundaryContains(Segment(Point(1, 0), Point(3, 0))));
    CHECK(collinear.boundaryContains(Segment(Point(1, 0), Point(5, 0))));
    CHECK_FALSE(collinear.boundaryContains(Segment(Point(1, 0), Point(2, 1))));

    CHECK(disk.boundaryContains(pgl::Shape<Point>(Point(5, 0))));
    CHECK_FALSE(disk.boundaryContains(pgl::Shape<Point>(Segment(Point(-5, 0), Point(5, 0)))));
}

TEST_CASE("Disk contains detects points inside or on the circle") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Disk disk(Point(0, 0), 5);

    CHECK(disk.contains(Point(0, 0)));
    CHECK(disk.contains(Point(3, 4)));
    CHECK(disk.contains(Point(5, 0)));
    CHECK_FALSE(disk.contains(Point(6, 0)));

    const Disk degenerate;
    CHECK(degenerate.contains(Point(0, 0)));
    CHECK_FALSE(degenerate.contains(Point(1, 0)));

    const Disk collinear(Point(0, 0), Point(2, 0), Point(4, 0));
    CHECK(collinear.contains(Point(2, 0)));
    CHECK(collinear.contains(Point(3, 0)));
    CHECK_FALSE(collinear.contains(Point(5, 0)));
    CHECK_FALSE(collinear.contains(Point(2, 1)));

    CHECK(disk.contains(Line(Point(3, 4), Point(3, 4))));
    CHECK_FALSE(disk.contains(Line(Point(6, 0), Point(6, 0))));
    CHECK_FALSE(disk.contains(Line(Point(-5, 0), Point(5, 0))));

    CHECK(disk.contains(OrientedLine(Point(3, 4), Point(3, 4))));
    CHECK_FALSE(disk.contains(OrientedLine(Point(6, 0), Point(6, 0))));
    CHECK_FALSE(disk.contains(OrientedLine(Point(-5, 0), Point(5, 0))));

    CHECK(disk.contains(Ray(Point(3, 4), Point(3, 4))));
    CHECK_FALSE(disk.contains(Ray(Point(6, 0), Point(6, 0))));
    CHECK_FALSE(disk.contains(Ray(Point(0, 0), Point(1, 0))));

    CHECK(disk.contains(Halfplane(Point(3, 4), Point(3, 4))));
    CHECK_FALSE(disk.contains(Halfplane(Point(6, 0), Point(6, 0))));
    CHECK_FALSE(disk.contains(Halfplane(Point(0, 0), Point(1, 0))));

    CHECK(disk.contains(Disk(Point(0, 0), 5)));
    CHECK(disk.contains(Disk(Point(1, 0), 4)));
    CHECK_FALSE(disk.contains(Disk(Point(1, 0), 5)));
    CHECK_FALSE(disk.contains(Disk(Point(6, 0), 1)));

    CHECK(disk.contains(Disk(Point(0, 0), Point(2, 0), Point(4, 0))));
    CHECK_FALSE(disk.contains(Disk(Point(0, 0), Point(5, 0), Point(6, 0))));

    CHECK(collinear.contains(Disk(Point(1, 0), Point(2, 0), Point(3, 0))));
    CHECK_FALSE(collinear.contains(Disk(Point(2, 0), 1)));

    CHECK(disk.contains(pgl::Convex<Point>(std::vector<Point>{{0, 0}, {3, 0}, {0, 4}})));
    CHECK_FALSE(disk.contains(pgl::Convex<Point>(std::vector<Point>{{0, 0}, {5, 0}, {6, 0}})));

    CHECK(disk.contains(pgl::Shape<Point>(Point(3, 4))));
    CHECK(disk.contains(pgl::Shape<Point>(pgl::Segment<Point>(Point(0, 0), Point(3, 4)))));
    CHECK_FALSE(disk.contains(pgl::Shape<Point>(Point(6, 0))));
    CHECK_FALSE(disk.contains(pgl::Shape<Point>(Ray(Point(0, 0), Point(1, 0)))));
}

// Disk::interiorsIntersect(...) and Convex::interiorsIntersect(Disk) compute the
// disk centre via center<R>() with R an exact field type (Rational<BigInt>), so a
// disk defined by three integer boundary points keeps its rational circumcentre
// instead of truncating it. The cases below use the geometrically correct answer
// (verified with an exact Rational oracle); they previously failed because the
// centre was computed in a promoted integer type and truncated by integer
// division — that bug is now fixed.
TEST_CASE("Disk interior predicates use the exact circumcentre of a 3-point disk") {
    using Point = pgl::Point<int>;

    // Disk (8,9)(8,12)(3,9): true centre (11/2, 21/2), r^2 = 17/2. A truncating
    // center<int>() would round it to (5, 10) and get these cases wrong.
    const pgl::Disk<Point> d(Point(8, 9), Point(8, 12), Point(3, 9));

    // The triangle's interior does not reach within the radius of the true
    // centre, so the interiors are disjoint.
    const pgl::Triangle<Point> t(Point(0, 1), Point(5, 3), Point(2, 10));
    CHECK(d.interiorsIntersect(t) == false);

    // Same disk vs a segment whose interior stays outside the true open disk.
    const pgl::Segment<Point> s(Point(6, 5), Point(1, 11));
    CHECK(d.interiorsIntersect(s) == false);

    // Convex::interiorsIntersect(Disk) shares the same exact centre.
    const pgl::Disk<Point> d2(Point(6, 4), Point(11, 2), Point(10, 7));
    const pgl::Convex<Point> convex(std::vector<Point>{
        Point(1, 4), Point(2, 3), Point(5, 1), Point(7, 11), Point(6, 11)});
    CHECK(convex.interiorsIntersect(d2) == false);

    // Disk vs disk: a real overlap that truncation would have dropped.
    const pgl::Disk<Point> d3(Point(9, 0), Point(12, 2), Point(7, 5));
    const pgl::Disk<Point> d4(Point(6, 9), Point(2, 4), Point(7, 4));
    CHECK(d3.interiorsIntersect(d4) == true);
}
