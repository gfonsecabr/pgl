#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <variant>
#include <vector>

using ERational = pgl::ERational;
using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
// Named PolygonShape, not Polygon: under MSVC, <windows.h> (pulled in
// transitively by doctest.h) injects a Win32 GDI function called `Polygon`
// into the global namespace, and an alias of the same name used from
// TEST_CASE bodies (global scope) resolves ambiguously against it.
using PolygonShape = pgl::Polygon<Point>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
Region vslab() {
    return Region({Halfplane(0, 1, 0, 0), Halfplane(3, 0, 3, 1)});
}
PolygonShape square(int lo, int hi) {
    return PolygonShape(std::vector<Point>{{lo, lo}, {hi, lo}, {hi, hi}, {lo, hi}});
}
// A reflex, C-shaped polygon opening to the right.
PolygonShape cShape() {
    return PolygonShape(std::vector<Point>{{0, 0}, {5, 0}, {5, 2}, {2, 2},
                                      {2, 4}, {5, 4}, {5, 6}, {0, 6}});
}
// A U with two prongs, whose gap a horizontal slab can fall into.
PolygonShape uShape() {
    return PolygonShape(std::vector<Point>{{0, 0}, {6, 0}, {6, 6}, {4, 6},
                                           {4, 2}, {2, 2}, {2, 6}, {0, 6}});
}
}  // namespace

TEST_CASE("Region contains a polygon") {
    const Region k = box6();
    CHECK(k.contains(square(1, 3)));
    CHECK(k.interiorContains(square(1, 3)));
    CHECK(k.contains(cShape()));
    CHECK(!k.contains(square(4, 8)));
}

TEST_CASE("A polygon contains a region") {
    CHECK(square(-1, 7).contains(box6()));
    CHECK(!square(-1, 7).contains(vslab()));
    // The reflex notch of the C excludes a box that reaches into it.
    CHECK(!cShape().contains(box6()));
}

TEST_CASE("Region intersects a polygon") {
    const Region k = box6();
    CHECK(k.intersects(square(3, 9)));
    CHECK(k.interiorsIntersect(square(3, 9)));
    CHECK(!k.intersects(square(8, 10)));
}

TEST_CASE("Separation and crossing with a polygon") {
    const Region k = box6();
    const PolygonShape band(std::vector<Point>{{-1, 2}, {7, 2}, {7, 4}, {-1, 4}});
    CHECK(band.separates(k));
    CHECK(k.separates(band));
    CHECK(k.crosses(band));
    CHECK(!square(2, 4).separates(k));
}

TEST_CASE("An unbounded slab is cut by a spanning polygon") {
    const Region s = vslab();
    const PolygonShape spanning(std::vector<Point>{{-1, 8}, {4, 8}, {4, 10}, {-1, 10}});
    CHECK(spanning.separates(s));
    const PolygonShape narrow(std::vector<Point>{{0, 8}, {1, 8}, {1, 10}, {0, 10}});
    CHECK(!narrow.separates(s));
}

TEST_CASE("Distance to a polygon") {
    const Region k = box6();
    // A square three units to the right of the box, overlapping it in y.
    const PolygonShape right(std::vector<Point>{{9, 2}, {11, 2}, {11, 4}, {9, 4}});
    CHECK(k.squaredDistance<double>(right) == doctest::Approx(9.0));
    CHECK(k.distanceL1<double>(right) == doctest::Approx(3.0));
    CHECK(k.squaredDistance<double>(square(2, 4)) == doctest::Approx(0.0));
}

// -----------------------------------------------------------------------------
// intersection
//
// The region's vertices are crossings of constraint lines, so every check here
// asks for exact rational coordinates. The pieces come back in no particular
// order, which is what the little counting helpers below are for.

namespace {
using EPoint = pgl::Point<ERational>;
using EPolyline = pgl::Polyline<EPoint>;
using EPolygon = pgl::Polygon<EPoint>;
using Piece = std::variant<EPoint, EPolyline, EPolygon>;

template <class T>
std::vector<T> piecesOfType(const std::vector<Piece>& pieces) {
    std::vector<T> out;
    for (const Piece& piece : pieces) {
        if (const T* value = std::get_if<T>(&piece)) {
            out.push_back(*value);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// The exact-arithmetic twin of a polygon written with integer coordinates.
EPolygon exactly(const PolygonShape& polygon) {
    return EPolygon(polygon);
}
}  // namespace

TEST_CASE("Region intersection with a polygon: the region covers it") {
    // A polygon inside the region comes back whole, as a single filled piece.
    const auto pieces = box6().intersection<ERational>(square(1, 3));
    REQUIRE(pieces.size() == 1);
    const auto polygons = piecesOfType<EPolygon>(pieces);
    REQUIRE(polygons.size() == 1);
    CHECK(polygons[0] == exactly(square(1, 3)));

    // The whole plane is the identity of the operation.
    const auto all = Region().intersection<ERational>(cShape());
    REQUIRE(all.size() == 1);
    CHECK(piecesOfType<EPolygon>(all)[0] == exactly(cShape()));
}

TEST_CASE("Region intersection with a polygon: a genuine clip") {
    // The unbounded half-plane {x <= 3} cuts the C in two: its two arms reach
    // past x = 3 only through the notch, which the clip closes.
    const Region left(Halfplane(3, 0, 3, 1));
    REQUIRE(!left.isBounded());
    const auto pieces = left.intersection<ERational>(cShape());
    const auto polygons = piecesOfType<EPolygon>(pieces);
    REQUIRE(pieces.size() == 1);
    REQUIRE(polygons.size() == 1);
    CHECK(polygons[0] == exactly(PolygonShape(std::vector<Point>{
                             {0, 0}, {3, 0}, {3, 2}, {2, 2}, {2, 4}, {3, 4}, {3, 6}, {0, 6}})));

    // A non-convex polygon can come apart into several pieces even against a
    // convex region: the horizontal slab 3 <= y <= 5 falls into the U's gap and
    // meets only its two prongs.
    Region slab(Halfplane(0, 3, 1, 3));
    slab.insert(Halfplane(1, 5, 0, 5));
    const auto cut = slab.intersection<ERational>(uShape());
    const auto cutPolygons = piecesOfType<EPolygon>(cut);
    REQUIRE(cut.size() == 2);
    REQUIRE(cutPolygons.size() == 2);
    CHECK(cutPolygons[0] == exactly(PolygonShape(std::vector<Point>{{0, 3}, {2, 3}, {2, 5}, {0, 5}})));
    CHECK(cutPolygons[1] == exactly(PolygonShape(std::vector<Point>{{4, 3}, {6, 3}, {6, 5}, {4, 5}})));
}

TEST_CASE("Region intersection with a polygon: the crossings are rational") {
    // The line through (0,1) and (3,0) crosses the square's edges at thirds.
    const Region tilted(Halfplane(0, 1, 3, 0));
    const auto pieces = tilted.intersection<ERational>(square(0, 1));
    const auto polygons = piecesOfType<EPolygon>(pieces);
    REQUIRE(polygons.size() == 1);
    CHECK(polygons[0] == EPolygon(std::vector<EPoint>{EPoint(0, 1), EPoint(1, ERational(2, 3)),
                                                      EPoint(1, 1)}));
}

TEST_CASE("Region intersection with a polygon: lower-dimensional pieces") {
    // {y >= 6} supports the C's top edge and meets the polygon in that edge
    // alone -- a polyline, not a filled piece.
    const Region above(Halfplane(0, 6, 1, 6));
    const auto edge = above.intersection<ERational>(cShape());
    REQUIRE(edge.size() == 1);
    const auto polylines = piecesOfType<EPolyline>(edge);
    REQUIRE(polylines.size() == 1);
    CHECK(polylines[0] == EPolyline(std::vector<EPoint>{EPoint(0, 6), EPoint(5, 6)}));

    // {x + y >= 10} touches the 5x5 square at its far corner and nowhere else.
    const auto touch = Region(Halfplane(0, 10, 1, 9)).intersection<ERational>(square(0, 5));
    REQUIRE(touch.size() == 1);
    const auto points = piecesOfType<EPoint>(touch);
    REQUIRE(points.size() == 1);
    CHECK(points[0] == EPoint(5, 5));
}

TEST_CASE("Region intersection with a polygon: the empty and degenerate regions") {
    // An empty region meets nothing.
    Region empty(Halfplane(0, 0, 1, 0));
    empty.insert(Halfplane(1, -1, 0, -1));
    REQUIRE(empty.isEmpty());
    CHECK(empty.intersection<ERational>(cShape()).empty());

    // A region that has collapsed onto a line is that line, and the polygon
    // clips it into chords. The C's notch splits the line y = 3 in two.
    Region line(Halfplane(0, 3, 1, 3));
    line.insert(Halfplane(1, 3, 0, 3));
    REQUIRE(line.isDegenerate());
    REQUIRE(!line.isBounded());
    const auto chords = line.intersection<ERational>(cShape());
    const auto polylines = piecesOfType<EPolyline>(chords);
    REQUIRE(chords.size() == 1);
    REQUIRE(polylines.size() == 1);
    CHECK(polylines[0] == EPolyline(std::vector<EPoint>{EPoint(0, 3), EPoint(2, 3)}));

    // A region that has collapsed onto a point is that point.
    Region point(Halfplane(0, 0, 1, 0));
    point.insert(Halfplane(1, 0, 0, 0));
    point.insert(Halfplane(2, 0, 2, 1));
    point.insert(Halfplane(2, 1, 2, 0));
    REQUIRE(point.isPoint());
    const auto hit = point.intersection<ERational>(square(0, 5));
    REQUIRE(hit.size() == 1);
    CHECK(piecesOfType<EPoint>(hit)[0] == EPoint(2, 0));
    CHECK(point.intersection<ERational>(square(3, 5)).empty());
}

TEST_CASE("Region intersection with a polygon: the polygon answers the same") {
    // The polygon outranks nothing here, so its rank forwarder sends the call
    // back to the region and both spellings agree.
    const Region k = box6();
    const PolygonShape overlapping(std::vector<Point>{{3, 3}, {9, 3}, {9, 9}, {3, 9}});
    CHECK(k.intersection<ERational>(overlapping) == overlapping.intersection<ERational>(k));
}
