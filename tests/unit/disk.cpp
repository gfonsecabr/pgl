#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

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
    CHECK(degenerate.template center<Number>() == make_point(Number{}, Number{}, ""));
    CHECK(degenerate.radius() == 0);
    CHECK(degenerate.isDegenerate());

    const Disk disk(make_point(static_cast<Number>(2), static_cast<Number>(3), "c"), static_cast<Number>(-5));
    CHECK(disk.template center<Number>().x() == Number(2));
    CHECK(disk.template center<Number>().y() == Number(3));
    CHECK(disk.radius() == 5);
    CHECK(disk.template squaredRadius<Number>() == Number(25));

    if constexpr (requires { disk.template center<Number>().label(); }) {
        CHECK(disk.template center<Number>().label().empty());
    }
}

TEST_CASE("Disk supports default template parameters and CTAD") {
    const pgl::Disk<> explicit_default({2, 3}, 4);
    pgl::Disk deduced(2, 3, 4);

    static_assert(std::is_same_v<decltype(deduced), pgl::Disk<pgl::Point<int>>>);

    CHECK(explicit_default.center<int>() == pgl::Point<int>(2, 3));
    CHECK(explicit_default.radius() == 4);
    CHECK(deduced.center<int>() == pgl::Point<int>(2, 3));
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

    CHECK(translated.center<int>() == Point(3, 2));
    CHECK(shifted.center<int>() == Point(1, 1));
    CHECK(scaled.center<int>() == Point(-4, -6));
    CHECK(scaled.radius() == 8);

    CHECK(disk.fbox<float>().min() == pgl::Point<float>(-2.0f, -1.0f));
    CHECK(disk.fbox<float>().max() == pgl::Point<float>(6.0f, 7.0f));
}

TEST_CASE("Disk diameter supports exact rational endpoints for three-point disks") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    const Disk axis_aligned(Point(2, 3), 4);
    CHECK(axis_aligned.diameter<int>() == pgl::Segment<Point>(Point(-2, 3), Point(6, 3)));

    const Disk disk(Point(-2, -2), Point(-2, -1), Point(1, 0));
    CHECK(disk.diameter<Rational>()
          == RationalSegment(RationalPoint(Rational(-2), Rational(-2)),
                             RationalPoint(Rational(5, 3), Rational(-1))));
}

TEST_CASE("Disk converts between labeled and unlabeled centers and supports hashing") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainDisk = pgl::Disk<PlainPoint>;
    using LabelDisk = pgl::Disk<LabelPoint, std::string>;

    const LabelDisk labeled(LabelPoint(2, 3, "center-point"), 4, "disk");
    const PlainDisk plain = labeled;
    const LabelDisk relabeled = plain;

    CHECK(plain.center<int>() == PlainPoint(2, 3));
    CHECK(plain.radius() == 4);
    CHECK(relabeled.center<int>() == LabelPoint(2, 3, ""));
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
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Disk disk(Point(0, 0), 5);

    const Disk degenerate;
    CHECK(degenerate.contains(Point(0, 0)));
    CHECK_FALSE(degenerate.contains(Point(1, 0)));

    // A disk collapses to a point, never to a segment (doc/raw/shapes.md), so a
    // zero-radius disk holds its centre and nothing else. Three distinct
    // collinear points determine no circle and are isUndefined(), which carries
    // no contract, so nothing here asserts on such a disk.
    const Disk dot(Point(2, 0), 0);
    CHECK(dot.contains(Point(2, 0)));
    CHECK_FALSE(dot.contains(Point(3, 0)));
    CHECK_FALSE(dot.contains(Point(2, 1)));

    CHECK(disk.contains(Line(Point(3, 4), Point(3, 4))));
    CHECK_FALSE(disk.contains(Line(Point(6, 0), Point(6, 0))));
    CHECK_FALSE(disk.contains(Line(Point(-5, 0), Point(5, 0))));

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

    CHECK(disk.contains(Disk(Point(3, 4), 0)));
    CHECK_FALSE(disk.contains(Disk(Point(6, 0), 0)));

    CHECK(dot.contains(Disk(Point(2, 0), 0)));
    CHECK_FALSE(dot.contains(Disk(Point(2, 0), 1)));

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

    // Disk vs disk: a real overlap that truncation would have dropped.
    const pgl::Disk<Point> d3(Point(9, 0), Point(12, 2), Point(7, 5));
    const pgl::Disk<Point> d4(Point(6, 9), Point(2, 4), Point(7, 4));
    CHECK(d3.interiorsIntersect(d4) == true);
}

// Disk vs Disk: intersects is exact via Rational<BigInt>. separates is always
// false (a disk minus a disk stays connected -- a crescent or an annulus), so
// the two disks never cross either.
TEST_CASE("Disk intersects, separates, and crosses another Disk") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk d(0, 0, 5);

    SUBCASE("two overlapping disks intersect but neither separates nor crosses") {
        const Disk over(6, 0, 5);  // centres 6 apart < 5 + 5
        CHECK(d.intersects(over));
        CHECK(over.intersects(d));
        CHECK_FALSE(d.separates(over));
        CHECK_FALSE(over.separates(d));
        CHECK_FALSE(d.crosses(over));
        CHECK_FALSE(over.crosses(d));
    }

    SUBCASE("externally tangent disks touch (closed contact)") {
        const Disk tangent(10, 0, 5);  // centres exactly 5 + 5 apart
        CHECK(d.intersects(tangent));
        CHECK(tangent.intersects(d));
        CHECK_FALSE(d.separates(tangent));
        CHECK_FALSE(d.crosses(tangent));
    }

    SUBCASE("a disk contained in another intersects it") {
        const Disk inner(1, 0, 2);
        CHECK(d.intersects(inner));
        CHECK(inner.intersects(d));
        CHECK_FALSE(d.separates(inner));   // remainder is an annulus, still connected
        CHECK_FALSE(d.crosses(inner));
    }

    SUBCASE("two disjoint disks meet in nothing") {
        const Disk away(20, 0, 5);
        CHECK_FALSE(d.intersects(away));
        CHECK_FALSE(away.intersects(d));
        CHECK_FALSE(d.separates(away));
        CHECK_FALSE(d.crosses(away));
    }
}

TEST_CASE("Disk::squaredDistance to other shapes returns the squared exterior gap") {
    using Point = pgl::Point<int>;
    // Closed disk centred at the origin with radius 2 (exact center+radius form).
    const pgl::Disk<Point> disk(Point(0, 0), 2);

    // For every disjoint shape the closest feature is at x = 5, so the gap is
    // 5 - 2 = 3 and the squared distance is 9. Intersecting shapes give 0.
    const double gap = (5.0 - 2.0) * (5.0 - 2.0);

    // Segment, both orientations, and the symmetric forwarder on the shape side.
    const pgl::Segment<Point> seg(Point(5, -1), Point(5, 1));
    CHECK(disk.squaredDistance<int>(seg) == doctest::Approx(gap));
    CHECK(seg.squaredDistance<int>(disk) == doctest::Approx(gap));
    const pgl::Segment<Point> segHit(Point(-5, 0), Point(5, 0));
    CHECK(disk.squaredDistance<int>(segHit) == doctest::Approx(0.0));
    CHECK(segHit.squaredDistance<int>(disk) == doctest::Approx(0.0));

    const pgl::OrientedSegment<Point> oseg(Point(5, -1), Point(5, 1));
    CHECK(disk.squaredDistance<int>(oseg) == doctest::Approx(gap));
    CHECK(oseg.squaredDistance<int>(disk) == doctest::Approx(gap));

    // Line / OrientedLine at x = 5.
    const pgl::Line<Point> line(Point(5, 0), Point(5, 7));
    CHECK(disk.squaredDistance<int>(line) == doctest::Approx(gap));
    CHECK(line.squaredDistance<int>(disk) == doctest::Approx(gap));
    CHECK(disk.squaredDistance<int>(pgl::Line<Point>(Point(-3, 0), Point(3, 0)))
          == doctest::Approx(0.0));

    const pgl::OrientedLine<Point> oline(Point(5, 0), Point(5, 7));
    CHECK(disk.squaredDistance<int>(oline) == doctest::Approx(gap));
    CHECK(oline.squaredDistance<int>(disk) == doctest::Approx(gap));

    // Ray whose source (5,0) is the nearest point.
    const pgl::Ray<Point> ray(Point(5, 0), Point(5, 7));
    CHECK(disk.squaredDistance<int>(ray) == doctest::Approx(gap));
    CHECK(ray.squaredDistance<int>(disk) == doctest::Approx(gap));

    // Halfplane with boundary line x = 5 and interior x >= 5 (origin is outside);
    // the directed boundary (5,7) -> (5,0) puts the interior to its left.
    const pgl::Halfplane<Point> hp(Point(5, 7), Point(5, 0));
    CHECK(disk.squaredDistance<int>(hp) == doctest::Approx(gap));
    CHECK(hp.squaredDistance<int>(disk) == doctest::Approx(gap));

    // Rectangle, disjoint and containing.
    const pgl::Rectangle<Point> rect(Point(5, -1), Point(8, 1));
    CHECK(disk.squaredDistance<int>(rect) == doctest::Approx(gap));
    CHECK(rect.squaredDistance<int>(disk) == doctest::Approx(gap));
    CHECK(disk.squaredDistance<int>(pgl::Rectangle<Point>(Point(-3, -3), Point(3, 3)))
          == doctest::Approx(0.0));

    // Triangle whose nearest edge lies on x = 5.
    const pgl::Triangle<Point> tri(Point(5, -2), Point(8, 0), Point(5, 2));
    CHECK(disk.squaredDistance<int>(tri) == doctest::Approx(gap));
    CHECK(tri.squaredDistance<int>(disk) == doctest::Approx(gap));

    // Disk vs disk: centres 10 apart, radii 2 and 2 -> gap 6, squared 36.
    const pgl::Disk<Point> disk2(Point(10, 0), 2);
    CHECK(disk.squaredDistance<int>(disk2) == doctest::Approx(36.0));
    CHECK(disk2.squaredDistance<int>(disk) == doctest::Approx(36.0));
    // Overlapping disks give 0.
    const pgl::Disk<Point> disk3(Point(2, 0), 2);
    CHECK(disk.squaredDistance<int>(disk3) == doctest::Approx(0.0));
}

// KNOWN BUG: several disk predicates evaluate the disk's circumcenter and
// squared circumradius at an integer result type (`promoted_number_t<int>` is
// `int64_t`, or the default `NumberType`). Both are computed by division, so for
// a disk built from three integer boundary points -- whose center and squared
// radius are generally rational -- the result is TRUNCATED and the predicate can
// return the wrong answer in either direction.
//
// Each expected value below is the true geometric answer, verified independently
// with exact `Rational<BigInt>` arithmetic and a floating-point cross-check, so
// these CHECKs FAIL against the current implementation. They are written against
// correct behavior on purpose so they pass once the predicates compute
// center/radius exactly -- the way Disk::intersects(Disk) /
// interiorsIntersect(Disk) already do via `Rational<BigInt>`.
TEST_CASE("Disk::contains(Disk) is exact for three-point integer disks") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // A = circle through (3,-4),(5,1),(4,4); B = circle through
    // (-5,-5),(2,-5),(3,0). Exactly, B is NOT contained in A (the distance
    // between centers plus B's radius exceeds A's radius).
    const Disk a(Point(3, -4), Point(5, 1), Point(4, 4));
    const Disk b(Point(-5, -5), Point(2, -5), Point(3, 0));
    CHECK(a.contains(b) == false);
}

TEST_CASE("Disk::interiorContains(Disk) is exact for three-point integer disks") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // A's open interior does NOT contain B.
    const Disk a(Point(-2, 4), Point(5, -2), Point(3, 3));
    const Disk b(Point(-5, 1), Point(3, -1), Point(2, 4));
    CHECK(a.interiorContains(b) == false);
}

TEST_CASE("Halfplane::contains(Disk) is exact for three-point integer disks") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // The disk lies inside the closed halfplane left of (5,0)->(0,1): the signed
    // distance of its center (~4.31) exceeds its radius (~3.73).
    const Halfplane h(Point(5, 0), Point(0, 1));
    const Disk d(Point(-4, 1), Point(0, -4), Point(-3, 1));
    CHECK(h.contains(d) == true);
}

TEST_CASE("Halfplane::interiorContains(Disk) is exact for three-point integer disks") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // The disk does NOT fit in the open halfplane left of (3,-3)->(2,2): the
    // signed distance of its center (~2.086) is below its radius (~2.137).
    const Halfplane h(Point(3, -3), Point(2, 2));
    const Disk d(Point(-1, -4), Point(3, -3), Point(0, -1));
    CHECK(h.interiorContains(d) == false);
}


TEST_CASE("A disk of radius zero answers as the point it covers") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Ray = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;

    // The one degenerate disk the library defines: three equal boundary points,
    // covering exactly a(). Every predicate here reached the in-circle
    // formulation before, where three equal points make every determinant
    // vanish and the tests come out true whatever they are asked.
    const Disk zero(Point(5, 5), 0);
    REQUIRE(zero.isPoint());
    REQUIRE(zero.isDegenerate());
    REQUIRE(!zero.isUndefined());

    SUBCASE("against a segment") {
        CHECK(!zero.intersects(Segment(Point(100, 100), Point(200, 200))));
        CHECK(zero.intersects(Segment(Point(0, 0), Point(10, 10))));   // through (5,5)
        CHECK(zero.intersects(Segment(Point(5, 5), Point(9, 1))));     // from (5,5)
        CHECK(!zero.intersects(Segment(Point(0, 1), Point(10, 11))));  // parallel, misses
    }

    SUBCASE("against a line, a ray, and a half-plane") {
        CHECK(!zero.intersects(Line(Point(0, 100), Point(1, 100))));
        CHECK(zero.intersects(Line(Point(0, 0), Point(1, 1))));
        CHECK(!zero.intersects(Ray(Point(0, 0), Point(-1, -1))));
        CHECK(zero.intersects(Ray(Point(0, 0), Point(1, 1))));
        CHECK(zero.intersects(Halfplane(Point(0, 0), Point(1, 0))));    // y >= 0
        CHECK(!zero.intersects(Halfplane(Point(1, 0), Point(0, 0))));   // y <= 0
    }

    SUBCASE("against shapes bounded by edges") {
        CHECK(zero.intersects(Rectangle(Point(0, 0), Point(10, 10))));
        CHECK(!zero.intersects(Rectangle(Point(20, 20), Point(30, 30))));
        const pgl::Polygon<Point> square({0, 0, 10, 0, 10, 10, 0, 10});
        const pgl::Polygon<Point> faraway({20, 20, 30, 20, 30, 30, 20, 30});
        CHECK(square.intersects(zero));
        CHECK(!faraway.intersects(zero));
        CHECK(!faraway.contains(zero));
    }

    SUBCASE("a collapsed shape is entirely boundary") {
        // doc/raw/shapes.md: boundaryContains coincides with contains on a
        // shape that has dropped below its natural dimension.
        CHECK(zero.contains(Point(5, 5)));
        CHECK(zero.boundaryContains(Point(5, 5)));
        CHECK(!zero.boundaryContains(Point(5, 6)));
        CHECK(!zero.interiorContains(Point(5, 5)));
        CHECK(!zero.boundaryContains(Segment(Point(0, 0), Point(10, 10))));
        CHECK(zero.boundaryContains(Segment(Point(5, 5), Point(5, 5))));
        CHECK(!zero.boundaryContains(Line(Point(0, 0), Point(1, 1))));
        CHECK(!zero.boundaryContains(Rectangle(Point(0, 0), Point(10, 10))));
        CHECK(zero.boundaryContains(Rectangle(Point(5, 5), Point(5, 5))));
    }

    SUBCASE("distances measure from the point") {
        CHECK(zero.squaredDistance<int>(Point(5, 8)) == doctest::Approx(9.0));
        CHECK(zero.squaredDistance<int>(Segment(Point(8, 0), Point(8, 10))) == doctest::Approx(9.0));
        CHECK(zero.squaredDistance<int>(Segment(Point(0, 0), Point(10, 10))) == doctest::Approx(0.0));
    }

    SUBCASE("a disk with area is unaffected") {
        const Disk d(Point(5, 5), 2);
        CHECK(!d.intersects(Segment(Point(100, 100), Point(200, 200))));
        CHECK(d.intersects(Segment(Point(0, 5), Point(10, 5))));
        CHECK(d.boundaryContains(Point(7, 5)));
        CHECK(!d.boundaryContains(Point(5, 5)));
        CHECK(d.interiorContains(Point(5, 5)));
    }
}
