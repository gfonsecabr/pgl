#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <optional>
#include <vector>
#include "pgl.hpp"

// Cross-shape tests for the degeneracy classification trio: isPoint/getIfPoint,
// isSegment/getIfSegment, and isUndefined. Every shape that can degenerate is
// exactly one of point, segment, or undefined -- or none of them when it is
// non-degenerate.
//
// The HalfplaneIntersection cases at the bottom cover the same family of
// shape-recognition predicates for regions: point, segment, ray, line, half-plane.

TEST_CASE_TEMPLATE("Segments classify degeneracy", Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Point p(3, 4);

    SUBCASE("degenerate segment is a point") {
        const Segment s(p, p);
        CHECK(s.isPoint());
        CHECK(s.getIfPoint() == std::optional<Point>(p));
        CHECK_FALSE(s.isUndefined());

        const OrientedSegment o(p, p);
        CHECK(o.isPoint());
        CHECK(o.getIfPoint() == std::optional<Point>(p));
        CHECK_FALSE(o.isUndefined());
    }

    SUBCASE("proper segment is nothing degenerate") {
        const Segment s(Point(0, 0), Point(2, 2));
        CHECK_FALSE(s.isPoint());
        CHECK_FALSE(s.getIfPoint().has_value());
        CHECK_FALSE(s.isUndefined());

        const OrientedSegment o(Point(0, 0), Point(2, 2));
        CHECK_FALSE(o.isPoint());
        CHECK_FALSE(o.getIfPoint().has_value());
        CHECK_FALSE(o.isUndefined());
    }
}

TEST_CASE_TEMPLATE("Rectangles classify degeneracy", Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("zero width and height is a point") {
        const Point p(3, 4);
        const Rectangle r(p, p);
        CHECK(r.isPoint());
        CHECK(r.getIfPoint() == std::optional<Point>(p));
        CHECK_FALSE(r.isSegment());
        CHECK_FALSE(r.getIfSegment().has_value());
        CHECK_FALSE(r.isUndefined());
    }

    SUBCASE("zero width is a vertical segment") {
        const Rectangle r(Point(1, 1), Point(1, 5));
        CHECK(r.isSegment());
        CHECK(r.getIfSegment() == std::optional<Segment>(Segment(Point(1, 1), Point(1, 5))));
        CHECK_FALSE(r.isPoint());
        CHECK_FALSE(r.isUndefined());
    }

    SUBCASE("zero height is a horizontal segment") {
        const Rectangle r(Point(1, 1), Point(5, 1));
        CHECK(r.isSegment());
        CHECK(r.getIfSegment() == std::optional<Segment>(Segment(Point(1, 1), Point(5, 1))));
    }

    SUBCASE("proper rectangle is nothing degenerate") {
        const Rectangle r(Point(0, 0), Point(2, 3));
        CHECK_FALSE(r.isPoint());
        CHECK_FALSE(r.isSegment());
        CHECK_FALSE(r.isUndefined());
    }
}

TEST_CASE_TEMPLATE("Triangles classify degeneracy", Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("three equal vertices are a point") {
        const Point p(3, 4);
        const Triangle t(p, p, p);
        CHECK(t.isPoint());
        CHECK(t.getIfPoint() == std::optional<Point>(p));
        CHECK_FALSE(t.isSegment());
        CHECK_FALSE(t.isUndefined());
    }

    SUBCASE("collinear vertices span a segment") {
        const Triangle t(Point(0, 0), Point(1, 1), Point(3, 3));
        CHECK(t.isSegment());
        CHECK(t.getIfSegment() == std::optional<Segment>(Segment(Point(0, 0), Point(3, 3))));
        CHECK_FALSE(t.isPoint());
        CHECK_FALSE(t.isUndefined());
    }

    SUBCASE("a repeated vertex still spans a segment") {
        const Triangle t(Point(0, 0), Point(0, 0), Point(2, 2));
        CHECK(t.isSegment());
        CHECK(t.getIfSegment() == std::optional<Segment>(Segment(Point(0, 0), Point(2, 2))));
    }

    SUBCASE("proper triangle is nothing degenerate") {
        const Triangle t(Point(0, 0), Point(4, 0), Point(0, 4));
        CHECK_FALSE(t.isPoint());
        CHECK_FALSE(t.isSegment());
        CHECK_FALSE(t.isUndefined());
    }
}

TEST_CASE_TEMPLATE("Vertex-list shapes classify degeneracy", Point, pgl::Point<int>,
                   pgl::Point<double>, pgl::Point<pgl::Rational<int64_t>>) {
    using Segment = pgl::Segment<Point>;

    const Point p(3, 4);
    const std::vector<Point> none{};
    const std::vector<Point> repeated{p, p, p};
    const std::vector<Point> collinear{Point(0, 0), Point(1, 1), Point(2, 2)};
    const std::vector<Point> proper{Point(0, 0), Point(4, 0), Point(0, 4)};
    const Segment spanned(Point(0, 0), Point(2, 2));

    const auto check = [&](const auto& empty_shape, const auto& point_shape,
                           const auto& segment_shape, const auto& proper_shape) {
        CHECK(empty_shape.isUndefined());
        CHECK_FALSE(empty_shape.isPoint());
        CHECK_FALSE(empty_shape.isSegment());
        CHECK_FALSE(empty_shape.getIfPoint().has_value());
        CHECK_FALSE(empty_shape.getIfSegment().has_value());

        CHECK(point_shape.isPoint());
        CHECK(point_shape.getIfPoint() == std::optional<Point>(p));
        CHECK_FALSE(point_shape.isSegment());
        CHECK_FALSE(point_shape.isUndefined());

        CHECK(segment_shape.isSegment());
        CHECK(segment_shape.getIfSegment() == std::optional<Segment>(spanned));
        CHECK_FALSE(segment_shape.isPoint());
        CHECK_FALSE(segment_shape.isUndefined());

        CHECK_FALSE(proper_shape.isPoint());
        CHECK_FALSE(proper_shape.isSegment());
        CHECK_FALSE(proper_shape.isUndefined());
    };

    SUBCASE("Convex") {
        check(pgl::Convex<Point>(none), pgl::Convex<Point>(repeated),
              pgl::Convex<Point>(collinear), pgl::Convex<Point>(proper));
    }

    SUBCASE("Polygon") {
        check(pgl::Polygon<Point>(none), pgl::Polygon<Point>(repeated),
              pgl::Polygon<Point>(collinear), pgl::Polygon<Point>(proper));
    }

    SUBCASE("Polyline") {
        check(pgl::Polyline<Point>(none), pgl::Polyline<Point>(repeated),
              pgl::Polyline<Point>(collinear), pgl::Polyline<Point>(proper));
    }

    SUBCASE("MonotoneChain") {
        // A monotone chain must be x-monotone, so the proper case zig-zags in y.
        const std::vector<Point> monotone_proper{Point(0, 0), Point(1, 3), Point(2, 0)};
        check(pgl::MonotoneChain<Point>(none), pgl::MonotoneChain<Point>(repeated),
              pgl::MonotoneChain<Point>(collinear), pgl::MonotoneChain<Point>(monotone_proper));
    }
}

TEST_CASE("Polygon with zero area but non-collinear vertices is undefined") {
    using Point = pgl::Point<int>;
    // The boundary retraces itself, so the signed area cancels even though the
    // vertices are not collinear: neither a point nor a segment.
    const pgl::Polygon<Point> bowtie(
        std::vector<Point>{Point(0, 0), Point(2, 0), Point(1, 1), Point(2, 0)});

    REQUIRE(bowtie.isDegenerate());
    CHECK(bowtie.isUndefined());
    CHECK_FALSE(bowtie.isPoint());
    CHECK_FALSE(bowtie.isSegment());
}

TEST_CASE_TEMPLATE("Disks classify degeneracy", Point, pgl::Point<int>, pgl::Point<double>,
                   pgl::Point<pgl::Rational<int64_t>>) {
    using Disk = pgl::Disk<Point>;

    SUBCASE("three equal boundary points are a point") {
        const Point p(3, 4);
        const Disk d(p, p, p);
        CHECK(d.isPoint());
        CHECK(d.getIfPoint() == std::optional<Point>(p));
        CHECK_FALSE(d.isUndefined());
    }

    SUBCASE("three distinct collinear boundary points determine no circle") {
        const Disk d(Point(0, 0), Point(1, 1), Point(2, 2));
        CHECK(d.isUndefined());
        CHECK_FALSE(d.isPoint());
        CHECK_FALSE(d.getIfPoint().has_value());
    }

    SUBCASE("a repeated boundary point leaves the circle underdetermined") {
        // Only two distinct points: collinear and not all equal, so undefined
        // even though infinitely many circles pass through them.
        const Disk d(Point(0, 0), Point(0, 0), Point(2, 2));
        CHECK(d.isUndefined());
        CHECK_FALSE(d.isPoint());
        CHECK_FALSE(d.getIfPoint().has_value());
    }

    SUBCASE("proper disk is nothing degenerate") {
        const Disk d(Point(0, 0), Point(4, 0), Point(0, 4));
        CHECK_FALSE(d.isPoint());
        CHECK_FALSE(d.isUndefined());
    }
}

// ---------------------------------------------------------------------------
// HalfplaneIntersection shape recognition

TEST_CASE("HalfplaneIntersection recognizes the shape it describes") {
    using Point = pgl::Point<int>;
    using Region = pgl::HalfplaneIntersection<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Segment = pgl::Segment<Point>;

    // Exactly which of the five predicates should hold, in order.
    const auto expect = [](const Region& r, bool point, bool segment, bool ray, bool line,
                           bool halfplane) {
        CHECK(r.isPoint() == point);
        CHECK(r.isSegment() == segment);
        CHECK(r.isRay() == ray);
        CHECK(r.isLine() == line);
        CHECK(r.isHalfplane() == halfplane);
        CHECK(r.getIfPoint().has_value() == point);
        CHECK(r.getIfSegment().has_value() == segment);
        CHECK(r.getIfRay().has_value() == ray);
        CHECK(r.getIfLine().has_value() == line);
        CHECK(r.getIfHalfplane().has_value() == halfplane);
    };

    SUBCASE("the whole plane is none of them") {
        expect(Region{}, false, false, false, false, false);
    }

    SUBCASE("the empty region is none of them") {
        Region r{Point(0, 0)};
        r.insert(Halfplane(Point(1, 1), Point(1, 0)));  // x >= 1, misses the origin
        REQUIRE(r.isEmpty());
        expect(r, false, false, false, false, false);
    }

    SUBCASE("a single constraint is a half-plane") {
        Region r;
        const Halfplane h(Point(0, 0), Point(1, 0));
        r.insert(h);
        expect(r, false, false, false, false, true);
        CHECK(*r.getIfHalfplane() == h);
    }

    SUBCASE("a redundant parallel constraint still leaves a half-plane") {
        const Halfplane tight(Point(0, 0), Point(1, 0));    // y >= 0
        const Halfplane loose(Point(0, -5), Point(1, -5));  // y >= -5

        // Either insertion order must collapse to the tighter constraint alone.
        Region tight_first;
        tight_first.insert(tight);
        tight_first.insert(loose);
        CHECK(tight_first.size() == 1);
        expect(tight_first, false, false, false, false, true);
        CHECK(*tight_first.getIfHalfplane() == tight);

        Region loose_first;
        loose_first.insert(loose);
        loose_first.insert(tight);
        CHECK(loose_first.size() == 1);
        expect(loose_first, false, false, false, false, true);
        CHECK(*loose_first.getIfHalfplane() == tight);
    }

    SUBCASE("a slab and a wedge are none of them") {
        Region slab;
        slab.insert(Halfplane(Point(0, 0), Point(1, 0)));
        slab.insert(Halfplane(Point(1, 5), Point(0, 5)));
        REQUIRE_FALSE(slab.isDegenerate());
        expect(slab, false, false, false, false, false);

        Region wedge;
        wedge.insert(Halfplane(Point(0, 0), Point(1, 0)));
        wedge.insert(Halfplane(Point(0, 0), Point(0, -1)));
        REQUIRE_FALSE(wedge.isDegenerate());
        expect(wedge, false, false, false, false, false);
    }

    SUBCASE("two opposite constraints are a line") {
        const Region r{pgl::Line<Point>(Point(0, 0), Point(3, 1))};
        expect(r, false, false, false, true, false);
        const auto line = r.getIfLine();
        REQUIRE(line);
        CHECK(line->contains(Point(0, 0)));
        CHECK(line->contains(Point(3, 1)));
    }

    // Halfplane(a, b) is the side left of the directed line a -> b, so the
    // clamp below points along +y and keeps x <= 0.
    SUBCASE("a clamped line is a ray") {
        Region r{pgl::Line<Point>(Point(0, 0), Point(1, 0))};
        r.insert(Halfplane(Point(0, 0), Point(0, 1)));  // clamp to x <= 0
        REQUIRE(r.isDegenerate());
        REQUIRE_FALSE(r.isEmpty());
        expect(r, false, false, true, false, false);

        const auto ray = r.getIfRay();
        REQUIRE(ray);
        // The region is the negative x-axis: source at the origin, running -x.
        CHECK(ray->source() == Point(0, 0));
        CHECK(ray->contains(Point(-5, 0)));
        CHECK_FALSE(ray->contains(Point(5, 0)));
        CHECK_FALSE(ray->contains(Point(0, 5)));
    }

    SUBCASE("a ray clamped the other way runs the other way") {
        Region r{pgl::Line<Point>(Point(0, 0), Point(1, 0))};
        r.insert(Halfplane(Point(0, 1), Point(0, 0)));  // clamp to x >= 0
        expect(r, false, false, true, false, false);

        const auto ray = r.getIfRay();
        REQUIRE(ray);
        CHECK(ray->source() == Point(0, 0));
        CHECK(ray->contains(Point(5, 0)));
        CHECK_FALSE(ray->contains(Point(-5, 0)));
    }

    SUBCASE("a point slab is a point") {
        const Region r{Point(3, 4)};
        expect(r, true, false, false, false, false);
        CHECK(*r.getIfPoint() == Point(3, 4));
    }

    SUBCASE("a point pinned by three constraints with no opposite pair") {
        // x >= 0, y >= 0, x + y <= 0 meet only at the origin.
        Region r;
        r.insert(Halfplane(Point(0, 0), Point(0, 1)));
        r.insert(Halfplane(Point(0, 0), Point(-1, 0)));
        r.insert(Halfplane(Point(0, 0), Point(1, -1)));
        REQUIRE(r.size() == 3);
        expect(r, true, false, false, false, false);
        CHECK(*r.getIfPoint() == Point(0, 0));
    }

    SUBCASE("a segment slab is a segment") {
        const Region r{Segment(Point(0, 0), Point(4, 2))};
        expect(r, false, true, false, false, false);
        CHECK(*r.getIfSegment() == Segment(Point(0, 0), Point(4, 2)));
    }
}

TEST_CASE("HalfplaneIntersection::isPoint is exact for non-representable coordinates") {
    using Point = pgl::Point<int>;
    using Region = pgl::HalfplaneIntersection<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rational = pgl::Rational<int64_t>;

    // The lines x + y = 1 and y = x meet only at (1/2, 1/2), which is not
    // representable as a Point<int>. The predicate must still say "point".
    Region r;
    r.insert(Halfplane(Point(0, 1), Point(1, 0)));
    r.insert(Halfplane(Point(1, 0), Point(0, 1)));
    r.insert(Halfplane(Point(0, 0), Point(1, 1)));
    r.insert(Halfplane(Point(1, 1), Point(0, 0)));

    REQUIRE(r.isDegenerate());
    CHECK(r.isPoint());
    CHECK_FALSE(r.isSegment());
    CHECK_FALSE(r.isLine());
    CHECK_FALSE(r.isHalfplane());

    const auto exact = r.getIfPoint<Rational>();
    REQUIRE(exact);
    CHECK(exact->x() == Rational(1, 2));
    CHECK(exact->y() == Rational(1, 2));
}

TEST_CASE("HalfplaneIntersection::getIfRay is exact for a non-representable source") {
    using Point = pgl::Point<int>;
    using Region = pgl::HalfplaneIntersection<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;

    // The line y = x clamped by x + y >= 1: a ray sourced at (1/2, 1/2), which
    // is not representable as a Point<int>.
    Region r{pgl::Line<Point>(Point(0, 0), Point(1, 1))};
    r.insert(Halfplane(Point(0, 1), Point(1, 0)));

    REQUIRE(r.isRay());
    CHECK_FALSE(r.isLine());
    CHECK_FALSE(r.isPoint());
    CHECK_FALSE(r.isSegment());

    const auto ray = r.getIfRay<Rational>();
    REQUIRE(ray);
    CHECK(ray->source() == RationalPoint(Rational(1, 2), Rational(1, 2)));
    CHECK(ray->contains(RationalPoint(Rational(3), Rational(3))));
    CHECK_FALSE(ray->contains(RationalPoint(Rational(0), Rational(0))));
}

TEST_CASE("HalfplaneIntersection recognition works with rational coordinates") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using Region = pgl::HalfplaneIntersection<Point>;
    using Segment = pgl::Segment<Point>;

    const Point a(Rational(0), Rational(0));
    const Point b(Rational(1, 3), Rational(1, 3));
    const Region r{Segment(a, b)};

    CHECK(r.isSegment());
    CHECK_FALSE(r.isPoint());
    CHECK(*r.getIfSegment<Rational>() == Segment(a, b));
}
