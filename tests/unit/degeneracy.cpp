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

    SUBCASE("collinear boundary points define no circle") {
        const Disk d(Point(0, 0), Point(1, 1), Point(2, 2));
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
