#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <functional>
#include <sstream>
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
        CHECK(set.isEmpty());
        CHECK(set.componentCount() == 0);
        CHECK(set.vertexCount() == 0);
        CHECK(set.holeCount() == 0);
        CHECK(!set.hasHoles());
        CHECK(set.begin() == set.end());
    }

    SUBCASE("single component") {
        const RegionSet set{Region(squareA())};
        CHECK(!set.isEmpty());
        CHECK(set.componentCount() == 1);
        CHECK(set.component(0) == Region(squareA()));
        CHECK(set.vertexCount() == 4);
    }

    SUBCASE("a zero-area component covers nothing and is dropped") {
        const PolygonShape collapsed({3, 3, 5, 3, 7, 3});
        CHECK(RegionSet{Region(collapsed)}.isEmpty());
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
            CHECK(!component.isEmpty());
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
        CHECK((set * 0).isEmpty());
        CHECK(set.scaledUpX(0).isEmpty());
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
        CHECK(pgl::Triangle<Point>(Point(0, 0), Point(2, 0), Point(4, 0)).asPolygonSet().isEmpty());
        CHECK(pgl::Rectangle<Point>(Point(0, 0), Point(0, 2)).asPolygonSet().isEmpty());
    }
}

TEST_CASE("PolygonSet drawing") {
    SUBCASE("a set draws one region per component") {
        pgl::Canvas canvas;
        canvas << RegionSet(std::vector{holed(), Region(squareB())});
        const std::string svg = canvas.toSVG();
        // One <path> per component: the holed one and the plain square.
        CHECK(svg.find("fill-rule=\"evenodd\"") != std::string::npos);
        std::size_t paths = 0;
        for (std::size_t at = svg.find("<path"); at != std::string::npos;
             at = svg.find("<path", at + 1)) {
            ++paths;
        }
        CHECK(paths == 2);
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
