#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

// Two 2x2 squares far apart, and a 10x10 square with a 2x2 hole.
static PolygonShape squareA() { return PolygonShape({0, 0, 2, 0, 2, 2, 0, 2}); }
static PolygonShape squareB() { return PolygonShape({5, 5, 7, 5, 7, 7, 5, 7}); }
static PolygonShape bigSquare() { return PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}); }
static PolygonShape smallHole() { return PolygonShape({2, 2, 4, 2, 4, 4, 2, 4}); }

static Region holed() { return Region(bigSquare(), std::vector{smallHole()}); }

TEST_CASE("PolygonSet construction and component access") {
    SUBCASE("default construction is the empty set") {
        const RegionSet set;
        CHECK(set.empty());
        CHECK(set.componentCount() == 0);
        CHECK(set.vertexCount() == 0);
        CHECK(set.holeCount() == 0);
        CHECK(!set.hasHoles());
        CHECK(set.begin() == set.end());
    }

    SUBCASE("single component") {
        const RegionSet set{Region(squareA())};
        CHECK(!set.empty());
        CHECK(set.componentCount() == 1);
        CHECK(set.component(0) == Region(squareA()));
        CHECK(set.vertexCount() == 4);
    }

    SUBCASE("a zero-area component covers nothing and is dropped") {
        const PolygonShape collapsed({3, 3, 5, 3, 7, 3});
        CHECK(RegionSet{Region(collapsed)}.empty());
        const RegionSet set(std::vector{Region(collapsed), Region(squareA())});
        CHECK(set.componentCount() == 1);
        CHECK(set.component(0) == Region(squareA()));
    }

    SUBCASE("components are sorted and deduplicated") {
        const RegionSet set(
            std::vector{Region(squareB()), Region(squareA()), Region(squareB())});
        REQUIRE(set.componentCount() == 2);
        CHECK(set.component(0) == Region(squareA()));
        CHECK(set.component(1) == Region(squareB()));
        CHECK(set.component(0) < set.component(1));
    }

    SUBCASE("the order components are supplied in does not matter") {
        const RegionSet forward(std::vector{Region(squareA()), Region(squareB())});
        const RegionSet backward(std::vector{Region(squareB()), Region(squareA())});
        CHECK(forward == backward);
    }

    SUBCASE("a trusted range is adopted verbatim") {
        // Deliberately out of canonical order: `trusted` promises it is not.
        const RegionSet set(std::vector{Region(squareB()), Region(squareA())}, true);
        REQUIRE(set.componentCount() == 2);
        CHECK(set.component(0) == Region(squareB()));
    }

    SUBCASE("components carry their own holes") {
        const RegionSet set(std::vector{holed(), Region(squareB())});
        CHECK(set.componentCount() == 2);
        CHECK(set.hasHoles());
        CHECK(set.holeCount() == 1);
        CHECK(set.vertexCount() == 4 + 4 + 4);
    }

    SUBCASE("range-for and std::ranges see the components") {
        const RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        std::size_t seen = 0;
        for (const auto& component : set) {
            CHECK(!component.empty());
            ++seen;
        }
        CHECK(seen == 2);
        CHECK(std::distance(set.cbegin(), set.cend()) == 2);
    }

    SUBCASE("class template argument deduction") {
        const pgl::PolygonSet deduced(std::vector{Region(squareA())});
        CHECK(std::is_same_v<decltype(deduced), const RegionSet>);
        const Region component = Region(squareA());
        const pgl::PolygonSet single(component);
        CHECK(std::is_same_v<decltype(single), const RegionSet>);
    }

    SUBCASE("conversion to another coordinate type") {
        const RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        const pgl::PolygonSet<pgl::Point<double>> converted(set);
        REQUIRE(converted.componentCount() == 2);
        CHECK(converted.component(0).outer()[0] == pgl::Point<double>(0.0, 0.0));
    }
}

TEST_CASE("PolygonSet component editing") {
    SUBCASE("addComponent keeps canonical order") {
        RegionSet set;
        set.addComponent(Region(squareB()));
        set.addComponent(Region(squareA()));
        REQUIRE(set.componentCount() == 2);
        CHECK(set.component(0) == Region(squareA()));
        CHECK(set.component(1) == Region(squareB()));
    }

    SUBCASE("addComponent ignores zero-area and duplicate components") {
        RegionSet set{Region(squareA())};
        set.addComponent(Region(PolygonShape({3, 3, 5, 3})));
        set.addComponent(Region(squareA()));
        CHECK(set.componentCount() == 1);
    }

    SUBCASE("eraseComponent by index") {
        RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        set.eraseComponent(0);
        REQUIRE(set.componentCount() == 1);
        CHECK(set.component(0) == Region(squareB()));
    }

    SUBCASE("eraseComponent by value") {
        RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        CHECK(set.eraseComponent(Region(squareA())));
        CHECK(!set.eraseComponent(Region(squareA())));
        CHECK(set.componentCount() == 1);
    }
}

TEST_CASE("PolygonSet vertices, edges and oriented edges") {
    const RegionSet set(std::vector{holed(), Region(squareB())});

    SUBCASE("vertices span every ring of every component") {
        const auto vertices = set.vertices();
        CHECK(vertices.size() == set.vertexCount());
        CHECK(vertices.front() == Point(0, 0));
    }

    SUBCASE("verticesView is the lazy counterpart of vertices") {
        const auto materialized = set.vertices();
        const auto lazy = set.verticesView();
        CHECK(std::equal(lazy.begin(), lazy.end(), materialized.begin(), materialized.end()));
        // The view walks points, unlike begin(), which walks components.
        CHECK(std::distance(set.verticesBegin(), set.verticesEnd())
              == static_cast<std::ptrdiff_t>(set.vertexCount()));
    }

    SUBCASE("verticesView of an empty set is empty") {
        const RegionSet empty;
        CHECK(empty.verticesBegin() == empty.verticesEnd());
        CHECK(empty.verticesView().empty());
    }

    SUBCASE("edges span every ring of every component") {
        CHECK(set.edges().size() == set.vertexCount());
    }

    SUBCASE("oriented edges keep the set on the left") {
        const auto oriented = set.orientedEdges();
        CHECK(oriented.size() == set.vertexCount());
        // The hole ring is emitted reversed, so the hole's stored first edge
        // appears the other way round.
        const pgl::OrientedSegment<Point> reversedHoleEdge(Point(4, 2), Point(2, 2));
        CHECK(std::find(oriented.begin(), oriented.end(), reversedHoleEdge) != oriented.end());
    }
}

TEST_CASE("PolygonSet value semantics") {
    const RegionSet set(std::vector{Region(squareA()), Region(squareB())});

    SUBCASE("equality and ordering") {
        CHECK(set == RegionSet(std::vector{Region(squareB()), Region(squareA())}));
        CHECK(set != RegionSet{Region(squareA())});
        CHECK(RegionSet{Region(squareA())} < set);  // fewer components first
        CHECK((set <=> set) == std::strong_ordering::equal);
    }

    SUBCASE("hashing agrees with equality and is order-independent") {
        const std::hash<RegionSet> hasher;
        CHECK(hasher(set) == hasher(RegionSet(std::vector{Region(squareB()), Region(squareA())})));
        std::unordered_set<RegionSet> seen;
        seen.insert(set);
        seen.insert(RegionSet(std::vector{Region(squareB()), Region(squareA())}));
        CHECK(seen.size() == 1);
        seen.insert(RegionSet{Region(squareA())});
        CHECK(seen.size() == 2);
    }

    SUBCASE("the memoized hash survives a mutation") {
        RegionSet mutated = set;
        const std::hash<RegionSet> hasher;
        const std::size_t before = hasher(mutated);
        mutated.eraseComponent(1);
        CHECK(hasher(mutated) != before);
        CHECK(hasher(mutated) == hasher(RegionSet{Region(squareA())}));
    }
}

TEST_CASE("PolygonSet streaming") {
    SUBCASE("the empty set") {
        std::ostringstream out;
        out << RegionSet();
        CHECK(out.str() == "PolygonSet[]");
    }

    SUBCASE("components print as regions, in canonical order") {
        std::ostringstream out;
        out << RegionSet(std::vector{Region(squareB()), Region(squareA())});
        CHECK(out.str() ==
              "PolygonSet[PolygonWithHoles[Polygon[(0,0),(2,0),(2,2),(0,2)]],"
              "PolygonWithHoles[Polygon[(5,5),(7,5),(7,7),(5,7)]]]");
    }
}

TEST_CASE("PolygonSet transformations") {
    const RegionSet set(std::vector{Region(squareA()), Region(squareB())});

    SUBCASE("translation") {
        RegionSet moved = set;
        moved += Point(1, 1);
        CHECK(moved.component(0) == Region(PolygonShape({1, 1, 3, 1, 3, 3, 1, 3})));
        moved -= Point(1, 1);
        CHECK(moved == set);
    }

    SUBCASE("set + point is the translating Minkowski sum") {
        const auto moved = set + Point(1, 1);
        CHECK(moved.componentCount() == 2);
        CHECK(moved.component(0) == Region(PolygonShape({1, 1, 3, 1, 3, 3, 1, 3})));
        CHECK((moved - Point(1, 1)) == set);
    }

    SUBCASE("scaling by a scalar") {
        const auto doubled = set * 2;
        CHECK(doubled.component(0) == Region(PolygonShape({0, 0, 4, 0, 4, 4, 0, 4})));
        CHECK((2 * set) == doubled);
        CHECK((doubled / 2) == set);
    }

    SUBCASE("a zero scale collapses every component away") {
        CHECK((set * 0).empty());
        CHECK(set.scaledUpX(0).empty());
    }

    SUBCASE("a negative scale re-sorts the components") {
        const auto reflected = set * -1;
        REQUIRE(reflected.componentCount() == 2);
        // (-7,-7)..(-5,-5) is lexicographically smaller than (-2,-2)..(0,0).
        CHECK(reflected.component(0).outer()[0] == Point(-7, -7));
    }

    SUBCASE("rotation by 90 degrees") {
        const auto turned = set.rotated90(1);
        CHECK(turned.componentCount() == 2);
        CHECK(turned.rotated90(-1) == set);
        RegionSet inPlace = set;
        inPlace.rotate90(4);
        CHECK(inPlace == set);
    }

    SUBCASE("axis scaling") {
        CHECK(set.scaledUpX(2).scaledDownX(2) == set);
        CHECK(set.scaledUpY(3).component(0) == Region(PolygonShape({0, 0, 2, 0, 2, 6, 0, 6})));
        RegionSet inPlace = set;
        inPlace.scaleUpY(3);
        CHECK(inPlace == set.scaledUpY(3));
    }

    SUBCASE("affine transformation") {
        const auto shifted = pgl::Transformation<int>::translation(1, 1) * set;
        CHECK(shifted.componentCount() == 2);
        CHECK(shifted.component(0) == Region(PolygonShape({1, 1, 3, 1, 3, 3, 1, 3})));
    }

    SUBCASE("a transformation preserves holes") {
        const RegionSet withHole{holed()};
        const auto shifted = pgl::Transformation<int>::translation(1, 0) * withHole;
        REQUIRE(shifted.componentCount() == 1);
        CHECK(shifted.component(0).holeCount() == 1);
    }
}

TEST_CASE("PolygonSet conversions from the area shapes") {
    SUBCASE("every area shape converts to a one-component set") {
        CHECK(squareA().asPolygonSet() == RegionSet{Region(squareA())});
        CHECK(holed().asPolygonSet() == RegionSet{holed()});
        CHECK(pgl::Rectangle<Point>(Point(0, 0), Point(2, 2)).asPolygonSet().componentCount() == 1);
        CHECK(pgl::Triangle<Point>(Point(0, 0), Point(2, 0), Point(0, 2))
                  .asPolygonSet()
                  .componentCount() == 1);
        CHECK(pgl::Convex<Point>({0, 0, 2, 0, 2, 2, 0, 2}).asPolygonSet().componentCount() == 1);
    }

    SUBCASE("a shape without area converts to the empty set") {
        CHECK(pgl::Triangle<Point>(Point(0, 0), Point(2, 0), Point(4, 0)).asPolygonSet().empty());
        CHECK(pgl::Rectangle<Point>(Point(0, 0), Point(0, 2)).asPolygonSet().empty());
    }
}

TEST_CASE("PolygonSet drawing") {
    const auto count = [](const std::string& text, const std::string& needle) {
        std::size_t total = 0;
        for (std::size_t at = text.find(needle); at != std::string::npos;
             at = text.find(needle, at + 1)) {
            ++total;
        }
        return total;
    };

    SUBCASE("a set draws as one even-odd path, one subpath per ring") {
        pgl::Canvas canvas;
        canvas << RegionSet(std::vector{holed(), Region(squareB())});
        const std::string svg = canvas.toSVG();
        // The whole set is one element, so one <path> carrying every ring of
        // every component: the holed one's outer ring and hole, and the plain
        // square. The even-odd rule is what punches the hole out.
        CHECK(svg.find("fill-rule=\"evenodd\"") != std::string::npos);
        CHECK(count(svg, "<path") == 1);
        CHECK(count(svg, "M ") == 3);
        CHECK(count(svg, " Z") == 3);
        CHECK(svg.find("<title>PolygonSet[") != std::string::npos);
    }

    SUBCASE("a set drawn through the runtime Shape wrapper draws the same") {
        const RegionSet set(std::vector{holed(), Region(squareB())});
        pgl::Canvas direct;
        direct << set;
        pgl::Canvas wrapped;
        wrapped << pgl::Shape<Point>(set);
        CHECK(direct.toSVG() == wrapped.toSVG());
    }

    SUBCASE("the empty set draws nothing") {
        pgl::Canvas canvas;
        canvas << RegionSet();
        CHECK(canvas.toSVG().find("<path") == std::string::npos);
    }
}

TEST_CASE("PolygonSet over exact rational coordinates") {
    const pgl::EPolygonSet set(
        std::vector{pgl::EPolygonWithHoles(pgl::EPolygon({0, 0, 1, 0, 1, 1, 0, 1}))});
    CHECK(set.componentCount() == 1);
    CHECK((set * pgl::ERational(1, 2)).component(0).outer()[2] ==
          pgl::EPoint(pgl::ERational(1, 2), pgl::ERational(1, 2)));
}

TEST_CASE("PolygonSet measures") {
    const RegionSet set(std::vector{Region(squareA()), Region(squareB())});

    SUBCASE("area is the sum over components, the interiors being disjoint") {
        CHECK(set.twiceArea() == 16);
        CHECK(set.area<double>() == doctest::Approx(8.0));
        CHECK(RegionSet().twiceArea() == 0);
    }

    SUBCASE("a component's holes come out of its own area") {
        const RegionSet withHole{holed()};
        CHECK(withHole.twiceArea() == 200 - 8);
    }

    SUBCASE("centroid is area-weighted over the components") {
        CHECK(set.centroid<double>() == pgl::Point<double>(3.5, 3.5));
        CHECK(set.verticesCentroid<double>() == pgl::Point<double>(3.5, 3.5));
    }

    SUBCASE("an unequal pair pulls the centroid toward the larger component") {
        const RegionSet lopsided(std::vector{
            Region(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1})),
            Region(PolygonShape({10, 0, 14, 0, 14, 4, 10, 4}))});
        // Areas 1 and 16 at x = 0.5 and x = 12.
        CHECK(lopsided.centroid<double>().x() ==
              doctest::Approx((1.0 * 0.5 + 16.0 * 12.0) / 17.0));
    }

    SUBCASE("the empty set has no centroid to weight") {
        CHECK(RegionSet().centroid<double>() == pgl::Point<double>(0.0, 0.0));
    }

    SUBCASE("bounding box is the union of the components' boxes") {
        CHECK(set.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(7, 7)));
        CHECK(set.fbox<double>() ==
              pgl::Rectangle<pgl::Point<double>>(pgl::Point<double>(0, 0), pgl::Point<double>(7, 7)));
        CHECK(RegionSet().bbox() == pgl::Rectangle<Point>());
    }

    SUBCASE("the cached box follows a mutation") {
        RegionSet mutated = set;
        CHECK(mutated.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(7, 7)));
        mutated.eraseComponent(1);
        CHECK(mutated.bbox() == pgl::Rectangle<Point>(Point(0, 0), Point(2, 2)));
        mutated += Point(1, 1);
        CHECK(mutated.bbox() == pgl::Rectangle<Point>(Point(1, 1), Point(3, 3)));
    }

    SUBCASE("the diameter spans two components") {
        // Neither component's own diameter reaches from one to the other.
        CHECK(set.diameter() == pgl::Segment<Point>(Point(0, 0), Point(7, 7)));
    }

    SUBCASE("convexHull spans both components, dropping their inner-facing corners") {
        // (2,2) and (5,5) each face the other square and sit strictly inside
        // the hull of the two squares together.
        const auto hull = set.convexHull();
        CHECK(hull == pgl::Convex<Point>(std::vector<Point>{
            Point(0, 0), Point(2, 0), Point(7, 5), Point(7, 7), Point(5, 7), Point(0, 2)}));
        CHECK(RegionSet().convexHull().empty());
    }

    SUBCASE("pointInside lands in a component's interior") {
        const auto witness = set.pointInside<double>();
        CHECK(set.component(0).interiorContains(witness));
    }
}

TEST_CASE("PolygonSet structural queries") {
    SUBCASE("a set of separated components is valid and regular") {
        const RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        CHECK(set.isValid());
        CHECK(set.isSimple());
        CHECK(set.isRegular());
    }

    SUBCASE("the empty set is valid and regular") {
        CHECK(RegionSet().isValid());
        CHECK(RegionSet().isRegular());
        CHECK(RegionSet().isUndefined());
        CHECK(RegionSet().isDegenerate());
    }

    SUBCASE("components touching at a single vertex are valid") {
        const RegionSet corner(std::vector{
            Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
            Region(PolygonShape({2, 2, 4, 2, 4, 4, 2, 4}))});
        CHECK(corner.isValid());
    }

    SUBCASE("components sharing a stretch of edge are not valid") {
        // Glued along x = 2: their union has interior points that lie in neither
        // component's interior, which is what the invariant rules out.
        const RegionSet glued(std::vector{
            Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
            Region(PolygonShape({2, 0, 4, 0, 4, 2, 2, 2}))});
        CHECK(!glued.isValid());
    }

    SUBCASE("overlapping components are not valid") {
        const RegionSet overlapping(std::vector{
            Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
            Region(PolygonShape({1, 1, 3, 1, 3, 3, 1, 3}))});
        CHECK(!overlapping.isValid());
    }

    SUBCASE("an island inside another component's hole is valid") {
        const RegionSet nested(std::vector{
            Region(bigSquare(), std::vector{PolygonShape({2, 2, 8, 2, 8, 8, 2, 8})}),
            Region(PolygonShape({4, 4, 6, 4, 6, 6, 4, 6}))});
        CHECK(nested.isValid());
        CHECK(nested.twiceArea() == 200 - 72 + 8);
    }

    SUBCASE("a set is regular exactly when every component is") {
        // A square whose hole shares an edge with the outer ring pinches shut
        // along that edge: a slit, so the component is not regular.
        const Region slit(bigSquare(), std::vector{PolygonShape({0, 2, 4, 2, 4, 4, 0, 4})});
        REQUIRE(!slit.isRegular());
        CHECK(!RegionSet{slit}.isRegular());
        CHECK(!RegionSet(std::vector{slit, Region(squareB())}).isRegular());
    }

    SUBCASE("regularized returns a set, so it is idempotent in the type system") {
        const Region slit(bigSquare(), std::vector{PolygonShape({0, 2, 4, 2, 4, 4, 0, 4})});
        const RegionSet cleaned = RegionSet{slit}.regularized<int>();
        CHECK(std::is_same_v<decltype(cleaned), const RegionSet>);
        CHECK(cleaned.isRegular());
        CHECK(cleaned.twiceArea() == slit.twiceArea());
        CHECK(cleaned.regularized<int>() == cleaned);
    }

    SUBCASE("regularized drops a component without area") {
        const RegionSet degenerate(std::vector{Region(PolygonShape({0, 0, 4, 0, 8, 0}))}, true);
        REQUIRE(degenerate.componentCount() == 1);
        CHECK(degenerate.regularized<int>().empty());
    }
}

TEST_CASE("PolygonSet decompositions") {
    SUBCASE("the triangulated domain covers exactly the area of the set") {
        const RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        const auto mesh = set.triangulation();
        int twiceArea = 0;
        for (const auto& triangle : mesh.triangles()) {
            twiceArea += triangle.twiceArea();
        }
        CHECK(twiceArea == set.twiceArea());
    }

    SUBCASE("a hole and an island inside it are handled together") {
        const RegionSet nested(std::vector{
            Region(bigSquare(), std::vector{PolygonShape({2, 2, 8, 2, 8, 8, 2, 8})}),
            Region(PolygonShape({4, 4, 6, 4, 6, 6, 4, 6}))});
        const auto mesh = nested.triangulation();
        int twiceArea = 0;
        for (const auto& triangle : mesh.triangles()) {
            twiceArea += triangle.twiceArea();
        }
        CHECK(twiceArea == nested.twiceArea());
    }

    SUBCASE("convexPartition and convexCovering cover the set") {
        const RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        const auto pieces = set.convexPartition();
        CHECK(pieces.size() == 2);
        int twiceArea = 0;
        for (const auto& piece : pieces) {
            twiceArea += piece.twiceArea();
        }
        CHECK(twiceArea == set.twiceArea());
        CHECK(set.convexCovering().size() == 2);
    }

    SUBCASE("triangulation with interior constraint segments") {
        const RegionSet set{Region(squareA())};
        const std::vector<pgl::Segment<Point>> constraints{
            pgl::Segment<Point>(Point(0, 0), Point(2, 2))};
        const auto mesh = set.triangulation(constraints);
        CHECK(mesh.numTriangles() == 2);
    }
}

TEST_CASE("PolygonSet asBitMatrix rasterizes a rectilinear set") {
    SUBCASE("the window spans the whole set and every component is filled") {
        const RegionSet set(std::vector{Region(squareA()), Region(squareB())});
        const auto raster = set.asBitMatrix();
        CHECK(raster.window() == set.bbox());
        CHECK(raster.area() == set.area<int>());
        CHECK(raster.componentCount() == 2);
        CHECK(raster.asPolygonSet() == set);
    }

    SUBCASE("a component inside a hole of another one keeps its gap") {
        const PolygonShape frame({0, 0, 10, 0, 10, 10, 0, 10});
        const PolygonShape bigHole({2, 2, 8, 2, 8, 8, 2, 8});
        const PolygonShape island({4, 4, 6, 4, 6, 6, 4, 6});
        const RegionSet set(std::vector{Region(frame, std::vector{bigHole}), Region(island)});
        const auto raster = set.asBitMatrix();
        CHECK(raster.area() == set.area<int>());
        CHECK(raster.asPolygonSet() == set);
    }

    SUBCASE("components touching at a corner stay apart") {
        const RegionSet set(std::vector{Region(squareA()),
                                        Region(PolygonShape({2, 2, 4, 2, 4, 4, 2, 4}))});
        const auto raster = set.asBitMatrix();
        CHECK(raster.area() == set.area<int>());
        CHECK(raster.asPolygonSet() == set);
    }

    SUBCASE("a slanted edge cannot be represented") {
        const RegionSet set{Region(PolygonShape({0, 0, 4, 0, 0, 4}))};
        CHECK_THROWS_AS(static_cast<void>(set.asBitMatrix()), std::logic_error);
    }

    SUBCASE("the empty set rasterizes to no cell") {
        CHECK(RegionSet().asBitMatrix().empty());
    }
}
