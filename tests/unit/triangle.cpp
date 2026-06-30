#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "pgl.hpp"

static_assert(std::is_same_v<pgl::Triangle<>, pgl::Triangle<pgl::Point<int>>>);

TEST_CASE_TEMPLATE("Triangle canonicalizes vertex order and iterates over its three canonical vertices", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Triangle = pgl::Triangle<Point>;
    using Number = std::remove_cvref_t<decltype(std::declval<Point>().x())>;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    const Triangle degenerate;
    if constexpr (requires { Point(Number{}, Number{}, "tag"); }) {
        CHECK(degenerate.a() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.b() == Point(Number{}, Number{}, ""));
        CHECK(degenerate.c() == Point(Number{}, Number{}, ""));
    } else {
        CHECK(degenerate.a() == Point(Number{}, Number{}));
        CHECK(degenerate.b() == Point(Number{}, Number{}));
        CHECK(degenerate.c() == Point(Number{}, Number{}));
    }


    {
        const Triangle triangle(
            make_point(static_cast<Number>(1), static_cast<Number>(2), "a"),
            make_point(static_cast<Number>(4), static_cast<Number>(2), "b"),
            make_point(static_cast<Number>(1), static_cast<Number>(5), "c"));

        CHECK(triangle[0] == make_point(static_cast<Number>(1), static_cast<Number>(2), "a"));
        CHECK(triangle[1] == make_point(static_cast<Number>(4), static_cast<Number>(2), "b"));
        CHECK(triangle[2] == make_point(static_cast<Number>(1), static_cast<Number>(5), "c"));

        Number coordinate_sum{};
        for (const auto& vertex : triangle) {
            coordinate_sum += vertex.x() + vertex.y();
        }
        CHECK(coordinate_sum == Number(15));

        if constexpr (requires { triangle.a().label(); }) {
            CHECK(triangle.a().label() == "a");
            CHECK(triangle.b().label() == "b");
            CHECK(triangle.c().label() == "c");
        }
    }

    {
        const Triangle triangle(
            make_point(static_cast<Number>(1), static_cast<Number>(5), "c"),
            make_point(static_cast<Number>(1), static_cast<Number>(2), "a"),
            make_point(static_cast<Number>(4), static_cast<Number>(2), "b"));

        CHECK(triangle[0] == make_point(static_cast<Number>(1), static_cast<Number>(2), "a"));
        CHECK(triangle[1] == make_point(static_cast<Number>(4), static_cast<Number>(2), "b"));
        CHECK(triangle[2] == make_point(static_cast<Number>(1), static_cast<Number>(5), "c"));

        Number coordinate_sum{};
        for (const auto& vertex : triangle) {
            coordinate_sum += vertex.x() + vertex.y();
        }
        CHECK(coordinate_sum == Number(15));

        if constexpr (requires { triangle.a().label(); }) {
            CHECK(triangle.a().label() == "a");
            CHECK(triangle.b().label() == "b");
            CHECK(triangle.c().label() == "c");
        }
    }
}

TEST_CASE("Triangle converts between labeled and unlabeled vertices") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainTriangle = pgl::Triangle<PlainPoint>;
    using LabelTriangle = pgl::Triangle<LabelPoint>;

    const LabelTriangle labeled(LabelPoint(0, 0, "a"), LabelPoint(4, 0, "b"), LabelPoint(0, 3, "c"));
    const PlainTriangle plain_source(0, 0, 4, 0, 0, 3);

    const PlainTriangle plain_from_labeled = labeled;
    const LabelTriangle labeled_from_plain = plain_source;

    CHECK(plain_from_labeled.a() == PlainPoint(0, 0));
    CHECK(plain_from_labeled.b() == PlainPoint(4, 0));
    CHECK(plain_from_labeled.c() == PlainPoint(0, 3));
    CHECK(labeled_from_plain.a() == LabelPoint(0, 0, ""));
    CHECK(labeled_from_plain.b() == LabelPoint(4, 0, ""));
    CHECK(labeled_from_plain.c() == LabelPoint(0, 3, ""));
}

TEST_CASE("Triangle supports default template parameters and CTAD") {
    const pgl::Triangle<> triangle(0, 0, 4, 0, 0, 3);
    pgl::Triangle deduced(0, 0, 4, 0, 0, 3);

    static_assert(std::is_same_v<decltype(deduced), pgl::Triangle<pgl::Point<int>>>);

    CHECK(triangle.a() == pgl::Point<int>(0, 0));
    CHECK(triangle.b() == pgl::Point<int>(4, 0));
    CHECK(triangle.c() == pgl::Point<int>(0, 3));
}

TEST_CASE("Triangle streams, transforms, and exposes vertices and edges") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Triangle triangle(1, 1, 4, 1, 1, 5);

    std::ostringstream stream;
    stream << triangle;
    CHECK(stream.str() == "<(1,1)(4,1)(1,5)>");

    const auto translated = triangle + Point(2, 3);
    const auto scaled = 2 * triangle;
    const auto shifted = translated - Point(1, 1);

    CHECK(translated.a() == Point(3, 4));
    CHECK(translated.b() == Point(6, 4));
    CHECK(translated.c() == Point(3, 8));
    CHECK(scaled.b() == Point(8, 2));
    CHECK(shifted.c() == Point(2, 7));

    const auto vertices = triangle.vertices();
    CHECK(vertices[0] == Point(1, 1));
    CHECK(vertices[1] == Point(4, 1));
    CHECK(vertices[2] == Point(1, 5));

    const auto edges = triangle.edges();
    CHECK(edges[0] == Segment(Point(1, 1), Point(4, 1)));
    CHECK(edges[1] == Segment(Point(1, 5), Point(4, 1)));
    CHECK(edges[2] == Segment(Point(1, 1), Point(1, 5)));

    const auto oriented_edges = triangle.orientedEdges();
    CHECK(oriented_edges[0] == OrientedSegment(Point(1, 1), Point(4, 1)));
    CHECK(oriented_edges[1] == OrientedSegment(Point(4, 1), Point(1, 5)));
    CHECK(oriented_edges[2] == OrientedSegment(Point(1, 5), Point(1, 1)));

    std::vector<Segment> iterated_edges;
    for (auto it = triangle.edgesBegin(); it != triangle.edgesEnd(); ++it) {
        iterated_edges.push_back(*it);
    }
    CHECK(iterated_edges == std::vector<Segment>(edges.begin(), edges.end()));

    std::vector<OrientedSegment> iterated_oriented_edges;
    for (auto it = triangle.orientedEdgesBegin(); it != triangle.orientedEdgesEnd(); ++it) {
        iterated_oriented_edges.push_back(*it);
    }
    CHECK(iterated_oriented_edges == std::vector<OrientedSegment>(oriented_edges.begin(), oriented_edges.end()));
}

TEST_CASE("Triangle reports exact area, centroid, interior point, and bounding boxes") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;

    const Triangle triangle(0, 0, 4, 0, 0, 2);

    CHECK(triangle.twiceArea() == int64_t(8));
    const auto area = triangle.area<Rational>();
    CHECK(area.numerator() == 4);
    CHECK(area.denominator() == 1);

    const auto centroid = triangle.centroid<Rational>();
    CHECK(centroid == RationalPoint(Rational(4, 3), Rational(2, 3)));
    CHECK(triangle.interiorContains(triangle.pointInside()));

    const auto circumcircle = triangle.circumcircle();
    CHECK(circumcircle.center() == RationalPoint(Rational(2), Rational(1)));
    CHECK(circumcircle.squaredRadius<Rational>() == Rational(5));

    const auto box = triangle.bbox();
    CHECK(box.min() == Point(0, 0));
    CHECK(box.max() == Point(4, 2));

    const auto fbox = triangle.fbox<float>();
    CHECK(fbox.min() == pgl::Point<float>(0.0f, 0.0f));
    CHECK(fbox.max() == pgl::Point<float>(4.0f, 2.0f));
}

TEST_CASE("Triangle exposes its longest side and basic angle classification helpers") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;

    const Triangle right_triangle(0, 0, 4, 0, 0, 3);
    CHECK(right_triangle.isRectangle());
    CHECK_FALSE(right_triangle.isObtuse());
    CHECK_FALSE(right_triangle.isIsosceles());
    CHECK(right_triangle.diameter() == Segment(Point(0, 3), Point(4, 0)));

    const Triangle obtuse_triangle(0, 0, 4, 0, 1, 1);
    CHECK_FALSE(obtuse_triangle.isRectangle());
    CHECK(obtuse_triangle.isObtuse());
    CHECK_FALSE(obtuse_triangle.isIsosceles());

    const Triangle isosceles_triangle(0, 0, 4, 0, 2, 3);
    CHECK_FALSE(isosceles_triangle.isRectangle());
    CHECK_FALSE(isosceles_triangle.isObtuse());
    CHECK(isosceles_triangle.isIsosceles());
}

TEST_CASE("Triangle point predicates distinguish vertices, boundary, interior, exterior, and degenerate cases") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    CHECK(triangle.verticesContain(Point(0, 0)));

    const Triangle degenerate(0, 0, 2, 0, 4, 0);
    CHECK(degenerate.isDegenerate());
    CHECK(degenerate.contains(Point(1, 0)));
    CHECK(degenerate.boundaryContains(Point(1, 0)));
    CHECK_FALSE(degenerate.interiorContains(Point(1, 0)));
    CHECK_FALSE(degenerate.contains(Point(1, 1)));
}

TEST_CASE("Triangle boundary containment accepts edge segments and rejects interior chords") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);
    const Segment edge_segment({1, 0}, {4, 0});
    const Segment interior_chord({0, 3}, {3, 0});

    CHECK(triangle.boundaryContains(edge_segment));
    CHECK_FALSE(triangle.boundaryContains(interior_chord));
}

TEST_CASE("Triangle intersections with lines and segments return points or clipped segments") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using Triangle = pgl::Triangle<Point>;
    using Line = pgl::Line<Point>;
    using Segment = pgl::Segment<Point>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    const Triangle triangle(0, 0, 4, 0, 0, 4);

    const auto line_crossing = triangle.intersection<Rational>(Line({-1, 1}, {5, 1}));
    REQUIRE(line_crossing);
    REQUIRE(std::holds_alternative<RationalSegment>(*line_crossing));
    CHECK(std::get<RationalSegment>(*line_crossing) == RationalSegment(RationalPoint(0, 1), RationalPoint(3, 1)));

    const auto line_vertex = triangle.intersection(Line({-1, 1}, {1, -1}));
    REQUIRE(line_vertex);
    REQUIRE(std::holds_alternative<Point>(*line_vertex));
    CHECK(std::get<Point>(*line_vertex) == Point(0, 0));

    CHECK_FALSE(triangle.intersection(Line({0, 5}, {4, 5})));
    CHECK(triangle.intersects(Line({-1, 1}, {5, 1})));
    CHECK_FALSE(triangle.intersects(Line({0, 5}, {4, 5})));

    const auto segment_crossing = triangle.intersection<Rational>(Segment({-1, 1}, {5, 1}));
    REQUIRE(segment_crossing);
    REQUIRE(std::holds_alternative<RationalSegment>(*segment_crossing));
    CHECK(std::get<RationalSegment>(*segment_crossing) == RationalSegment(RationalPoint(0, 1), RationalPoint(3, 1)));

    const auto segment_touching = triangle.intersection(Segment({-1, -1}, {0, 0}));
    REQUIRE(segment_touching);
    REQUIRE(std::holds_alternative<Point>(*segment_touching));
    CHECK(std::get<Point>(*segment_touching) == Point(0, 0));

    CHECK_FALSE(triangle.intersection(Segment({5, 5}, {6, 6})));
    CHECK(triangle.intersects(Segment({-1, 1}, {5, 1})));
    CHECK_FALSE(triangle.intersects(Segment({5, 5}, {6, 6})));
}


TEST_CASE("Triangle interiorsIntersect distinguishes strict interior hits from boundary-only contact") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    CHECK(triangle.interiorsIntersect(Line({-1, 2}, {7, 2})));
    CHECK_FALSE(triangle.interiorsIntersect(Line({0, 0}, {6, 0})));
    CHECK_FALSE(triangle.interiorsIntersect(Line({-1, 1}, {1, -1})));

    CHECK(triangle.interiorsIntersect(Segment({-1, 2}, {5, 2})));
    CHECK(triangle.interiorsIntersect(Segment({1, 1}, {2, 2})));
    CHECK_FALSE(triangle.interiorsIntersect(Segment({0, 0}, {6, 0})));
    CHECK_FALSE(triangle.interiorsIntersect(Segment({-1, -1}, {0, 0})));
}

TEST_CASE("Triangle interiorsIntersect handles triangle-triangle overlap without counting shared boundaries") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle base(0, 0, 6, 0, 0, 6);
    const Triangle overlapping(2, -1, 5, 2, 2, 5);
    const Triangle contained(1, 1, 2, 1, 1, 2);
    const Triangle same(0, 0, 6, 0, 0, 6);
    const Triangle shared_edge(0, 0, 6, 0, 3, -3);
    const Triangle touching_vertex(6, 0, 8, 0, 6, 2);
    const Triangle disjoint(7, 0, 9, 0, 7, 2);

    CHECK(base.interiorsIntersect(overlapping));
    CHECK(base.interiorsIntersect(contained));
    CHECK(base.interiorsIntersect(same));
    CHECK_FALSE(base.interiorsIntersect(shared_edge));
    CHECK_FALSE(base.interiorsIntersect(touching_vertex));
    CHECK_FALSE(base.interiorsIntersect(disjoint));
}

TEST_CASE("Linear primitives separate triangles only when their trace cuts the interior from boundary to boundary") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Ray = pgl::Ray<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    const Segment edge_to_edge({-1, 2}, {5, 2});
    const Segment vertex_to_opposite_edge({0, 0}, {3, 3});
    const Segment touching_vertex({-1, -1}, {0, 0});
    const Segment starts_inside({1, 1}, {5, 1});
    const Segment along_edge({0, 0}, {6, 0});

    CHECK(edge_to_edge.separates(triangle));
    CHECK(edge_to_edge.crosses(triangle));
    CHECK(vertex_to_opposite_edge.separates(triangle));

    CHECK_FALSE(touching_vertex.separates(triangle));
    CHECK_FALSE(starts_inside.separates(triangle));
    CHECK_FALSE(along_edge.separates(triangle));

    CHECK(Line({-1, 2}, {5, 2}).separates(triangle));
    CHECK(Ray({-1, 2}, {5, 2}).separates(triangle));
    CHECK(Ray({0, 0}, {3, 3}).separates(triangle));
    CHECK_FALSE(Ray({1, 1}, {5, 1}).separates(triangle));
    CHECK_FALSE(Line({0, 0}, {6, 0}).separates(triangle));
}

TEST_CASE("Triangles can separate other triangles when they carry a true cut") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle big_triangle({0, 0}, {6, 0}, {0, 6});
    const Triangle cutting_triangle({2, -1}, {4, -1}, {3, 5});
    CHECK(cutting_triangle.separates(big_triangle));
}

TEST_CASE("Triangle covers the non-Convex contract for interiorsIntersect") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Shape = pgl::Shape<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    CHECK(triangle.interiorsIntersect(Line({-1, 2}, {7, 2})));
    CHECK(triangle.interiorsIntersect(Segment({-1, 2}, {5, 2})));
    CHECK(triangle.interiorsIntersect(Triangle({2, -1}, {5, 2}, {2, 5})));
    CHECK(triangle.interiorsIntersect(Shape(Segment({-1, 2}, {5, 2}))));
}

TEST_CASE("Triangle covers the non-Convex contract for separates") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Shape = pgl::Shape<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    CHECK(triangle.separates(Line({-1, 2}, {5, 2})));
    CHECK(triangle.separates(Segment({-1, 2}, {5, 2})));
    CHECK(triangle.separates(Shape(Segment({-1, 2}, {5, 2}))));
}

TEST_CASE("Triangle covers the non-Convex contract for crosses") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Shape = pgl::Shape<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    CHECK(triangle.crosses(Line({-1, 2}, {5, 2})));
    CHECK(triangle.crosses(Segment({-1, 2}, {5, 2})));
    CHECK(triangle.crosses(Shape(Segment({-1, 2}, {5, 2}))));
}

TEST_CASE("Triangle ordering and hashing follow the stored vertices") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle first(0, 0, 4, 0, 0, 3);
    const Triangle second(0, 0, 4, 0, 0, 3);
    const Triangle permuted(4, 0, 0, 3, 0, 0);

    CHECK(first == second);
    CHECK(first == permuted);

    std::set<Triangle> ordered_set;
    ordered_set.insert(first);
    ordered_set.insert(second);
    ordered_set.insert(permuted);
    CHECK(ordered_set.size() == 1);

    std::unordered_set<Triangle> unordered_set;
    unordered_set.insert(first);
    unordered_set.insert(second);
    unordered_set.insert(permuted);
    CHECK(unordered_set.size() == 1);
}

TEST_CASE("Triangle mixing number types") {
    pgl::Triangle<pgl::Point<int>> t(0,0,1,2,0,-2);
    CHECK(t.contains(pgl::Point<double>(0.5,0.5)));
    CHECK_FALSE(t.contains(pgl::Point<double>(1,2.1)));
    CHECK_FALSE(t.contains(pgl::Point<pgl::Rational<int>>({1,8},{1,3})));
    CHECK(t.interiorContains(pgl::Point<pgl::Rational<int>>({1,5},-1)));
}

TEST_CASE("Triangle predicates against another triangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle base({0, 0}, {6, 0}, {0, 6});

    SUBCASE("contains (closed)") {
        CHECK(base.contains(Triangle({1, 1}, {2, 1}, {1, 2})));   // strictly inside
        CHECK(base.contains(base));                               // closed: contains itself
        CHECK(base.contains(Triangle({0, 0}, {6, 0}, {0, 3})));   // shares the bottom edge
        CHECK_FALSE(base.contains(Triangle({1, 1}, {8, 1}, {1, 2})));        // one vertex outside
        CHECK_FALSE(base.contains(Triangle({10, 10}, {11, 10}, {10, 11})));  // disjoint
    }

    SUBCASE("boundaryContains") {
        // A degenerate (collinear) triangle lying along the bottom edge.
        CHECK(base.boundaryContains(Triangle({1, 0}, {2, 0}, {3, 0})));
        // A non-degenerate rectangle cannot lie on a 1D boundary.
        CHECK_FALSE(base.boundaryContains(Rectangle({1, 0}, {2, 2})));
        // Leaving the boundary into the interior is not boundary containment.
        CHECK_FALSE(base.boundaryContains(Triangle({1, 0}, {2, 0}, {2, 2})));
        // An interior triangle is not on the boundary at all.
        CHECK_FALSE(base.boundaryContains(Triangle({1, 1}, {2, 1}, {1, 2})));
    }

    SUBCASE("interiorContains (strict)") {
        CHECK(base.interiorContains(Triangle({1, 1}, {2, 1}, {1, 2})));          // strictly inside
        CHECK_FALSE(base.interiorContains(Triangle({0, 1}, {2, 1}, {1, 2})));    // vertex on edge x=0
        CHECK_FALSE(base.interiorContains(base));                               // own vertices on boundary
    }

    SUBCASE("intersects") {
        CHECK(base.intersects(Triangle({2, 2}, {8, 2}, {2, 8})));   // overlapping area
        CHECK(base.intersects(Triangle({6, 0}, {8, 0}, {8, 2})));   // touch only at the vertex (6,0)
        CHECK(base.intersects(Triangle({2, -1}, {4, -1}, {3, 5}))); // edges cross, no vertex inside
        CHECK_FALSE(base.intersects(Triangle({10, 10}, {11, 10}, {10, 11})));  // disjoint
    }

    SUBCASE("crosses") {
        // A thin triangle stabbing through base: each separates the other.
        CHECK(base.crosses(Triangle({2, -1}, {4, -1}, {3, 5})));
        // Containment is not crossing.
        CHECK_FALSE(base.crosses(Triangle({1, 1}, {2, 1}, {1, 2})));
        // Disjoint is not crossing.
        CHECK_FALSE(base.crosses(Triangle({10, 10}, {11, 10}, {10, 11})));
    }

    SUBCASE("intersection") {
        using Convex = pgl::Convex<Point>;
        using Segment = pgl::Segment<Point>;

        // Area overlap is a convex polygon: base ∩ (2,2)(8,2)(2,8) is the triangle
        // (2,2)(4,2)(2,4), twiceArea 4.
        const auto overlap = base.intersection(Triangle({2, 2}, {8, 2}, {2, 8}));
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Convex>(*overlap));
        CHECK(std::get<Convex>(*overlap).twiceArea() == 4);

        // A triangle sharing the whole bottom edge from below meets along it.
        const auto edge = base.intersection(Triangle({0, 0}, {6, 0}, {0, -6}));
        REQUIRE(edge);
        REQUIRE(std::holds_alternative<Segment>(*edge));
        CHECK(std::get<Segment>(*edge) == Segment(Point(0, 0), Point(6, 0)));

        // A strictly interior triangle is returned as the (convex) overlap.
        const auto inner = base.intersection(Triangle({1, 1}, {2, 1}, {1, 2}));
        REQUIRE(inner);
        REQUIRE(std::holds_alternative<Convex>(*inner));
        CHECK(std::get<Convex>(*inner).twiceArea() == 1);

        // Touching at a single vertex yields a Point, never a degenerate segment.
        const auto tip = base.intersection(Triangle({6, 0}, {8, 0}, {7, 2}));
        REQUIRE(tip);
        REQUIRE(std::holds_alternative<Point>(*tip));
        CHECK(std::get<Point>(*tip) == Point(6, 0));

        // Disjoint triangles do not intersect.
        CHECK_FALSE(base.intersection(Triangle({10, 10}, {11, 10}, {10, 11})));
    }
}

TEST_CASE("Triangle measures squared distance to every lower-ranked shape") {
    using P = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    const pgl::Triangle<P> tri({0, 0}, {4, 0}, {0, 4});

    SUBCASE("intersecting shapes are at distance zero") {
        CHECK(tri.squaredDistance(P(1, 1)) == 0);                              // interior point
        CHECK(tri.squaredDistance(pgl::Line<P>({0, 1}, {1, 1})) == 0);         // y=1 cuts the triangle
        CHECK(tri.squaredDistance(pgl::Segment<P>({-1, 1}, {5, 1})) == 0);     // crossing segment
        CHECK(tri.squaredDistance(pgl::Rectangle<P>({1, 1}, {2, 2})) == 0);    // rectangle inside
    }

    SUBCASE("disjoint shapes whose nearest feature is a vertex are integer-exact") {
        CHECK(tri.squaredDistance(P(10, 0)) == 36);                            // -> vertex (4,0)
        CHECK(tri.squaredDistance(pgl::Segment<P>({10, 0}, {10, 4})) == 36);
        CHECK(tri.squaredDistance(pgl::OrientedSegment<P>({10, 0}, {10, 4})) == 36);
        CHECK(tri.squaredDistance(pgl::Ray<P>({10, 0}, {11, 0})) == 36);
        CHECK(tri.squaredDistance(pgl::Line<P>({0, -1}, {1, -1})) == 1);       // bottom edge to y=-1
        CHECK(tri.squaredDistance(pgl::OrientedLine<P>({0, -1}, {1, -1})) == 1);
    }

    SUBCASE("fractional distances need a floating-point or rational ResultNumber") {
        const P near_hypotenuse(2, 3);                                        // exact squared distance 1/2
        CHECK(tri.squaredDistance(near_hypotenuse) == 0);                     // integer default truncates
        CHECK(tri.squaredDistance<double>(near_hypotenuse) == doctest::Approx(0.5));
        CHECK(tri.squaredDistance<Rational>(near_hypotenuse) == Rational(1, 2));

        // A line parallel to the hypotenuse, just outside the triangle.
        CHECK(tri.squaredDistance<double>(pgl::Line<P>({5, 0}, {0, 5})) == doctest::Approx(0.5));
    }

    SUBCASE("the relation is symmetric: lower-ranked shapes forward to the triangle") {
        CHECK(P(10, 0).squaredDistance(tri) == tri.squaredDistance(P(10, 0)));
        CHECK(pgl::Segment<P>({10, 0}, {10, 4}).squaredDistance(tri)
              == tri.squaredDistance(pgl::Segment<P>({10, 0}, {10, 4})));
        CHECK(pgl::Line<P>({0, -1}, {1, -1}).squaredDistance(tri)
              == tri.squaredDistance(pgl::Line<P>({0, -1}, {1, -1})));
        CHECK(pgl::Rectangle<P>({10, 10}, {12, 12}).squaredDistance(tri)
              == tri.squaredDistance(pgl::Rectangle<P>({10, 10}, {12, 12})));
    }
}
