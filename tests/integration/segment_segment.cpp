#include <cstdint>
#include <variant>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"


namespace {

template <typename Segment, typename Point>
void check_center_is_strictly_inside(const Segment& segment, const Point& center) {
    INFO("segment=" << segment << " center=" << center);
    CHECK(segment.contains(center));
    CHECK(segment.interiorContains(center));
    CHECK_FALSE(segment.boundaryContains(center));
}

template <typename Segment, typename Point>
void check_center_is_an_endpoint(const Segment& segment, const Point& center) {
    INFO("segment=" << segment << " center=" << center);
    CHECK(segment.contains(center));
    CHECK_FALSE(segment.interiorContains(center));
    CHECK(segment.boundaryContains(center));
}

} // namespace

TEST_CASE("Segments around a shared midpoint") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Point center(-3, 5);

    SUBCASE("crossing at center") {
        const Segment first_diameter(center - Point{2, 2}, center + Point{2, 2});
        const Segment second_diameter(center - Point{2, -2}, center + Point{2, -2});
        const Segment endpoint_connector(center + Point{2, 2}, center + Point{2, -2});

        check_center_is_strictly_inside(first_diameter, center);
        check_center_is_strictly_inside(second_diameter, center);
        CHECK(first_diameter.midpoint() == center);
        CHECK(second_diameter.midpoint() == center);

        CHECK(first_diameter.intersects(second_diameter));
        CHECK(second_diameter.intersects(first_diameter));
        CHECK(first_diameter.crosses(second_diameter));
        CHECK(first_diameter.separates(second_diameter));
        CHECK(second_diameter.separates(first_diameter));
        CHECK_FALSE(first_diameter.contains(second_diameter));
        CHECK_FALSE(second_diameter.contains(first_diameter));

        CHECK(endpoint_connector.intersects(first_diameter));
        CHECK(endpoint_connector.intersects(second_diameter));
        CHECK_FALSE(endpoint_connector.crosses(first_diameter));
        CHECK_FALSE(endpoint_connector.crosses(second_diameter));
        CHECK_FALSE(endpoint_connector.contains(first_diameter));
        CHECK_FALSE(endpoint_connector.contains(second_diameter));
    }

    SUBCASE("colinear overlap with strict containment") {
        const Segment longer_diameter(center - Point{3, 3}, center + Point{3, 3});
        const Segment shorter_diameter(center - Point{1, 1}, center + Point{1, 1});

        check_center_is_strictly_inside(longer_diameter, center);
        check_center_is_strictly_inside(shorter_diameter, center);
        CHECK(longer_diameter.midpoint() == center);
        CHECK(shorter_diameter.midpoint() == center);

        CHECK(longer_diameter.intersects(shorter_diameter));
        CHECK_FALSE(longer_diameter.crosses(shorter_diameter));
        CHECK(longer_diameter.contains(shorter_diameter));
        CHECK_FALSE(shorter_diameter.contains(longer_diameter));
        CHECK_FALSE(longer_diameter.separates(shorter_diameter));
        CHECK(shorter_diameter.separates(longer_diameter));
    }

    SUBCASE("identical segments overlap completely") {
        const Segment reference_diameter(center - Point{2, 1}, center + Point{2, 1});
        const Segment identical_diameter(center - Point{2, 1}, center + Point{2, 1});

        check_center_is_strictly_inside(reference_diameter, center);
        check_center_is_strictly_inside(identical_diameter, center);
        CHECK(reference_diameter.midpoint() == center);
        CHECK(identical_diameter.midpoint() == center);

        CHECK(reference_diameter.intersects(identical_diameter));
        CHECK_FALSE(reference_diameter.crosses(identical_diameter));
        CHECK(reference_diameter.contains(identical_diameter));
        CHECK(identical_diameter.contains(reference_diameter));
    }
}

TEST_CASE("Segments sharing an endpoint") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    const Point center(-3, 4);

    SUBCASE("shared endpoint without overlap") {
        const Segment host_segment(center - Point{3, 1}, center + Point{3, 1});
        const Segment branch_segment(center, center + Point{1, 3});

        check_center_is_strictly_inside(host_segment, center);
        CHECK(host_segment.midpoint() == center);
        check_center_is_an_endpoint(branch_segment, center);

        CHECK(host_segment.intersects(branch_segment));
        CHECK(branch_segment.intersects(host_segment));
        CHECK_FALSE(host_segment.crosses(branch_segment));
        CHECK_FALSE(host_segment.interiorsIntersect(branch_segment));
        CHECK_FALSE(branch_segment.interiorsIntersect(host_segment));
        CHECK_FALSE(host_segment.contains(branch_segment));
        CHECK_FALSE(host_segment.separates(branch_segment));
        CHECK(branch_segment.separates(host_segment));
    }

    SUBCASE("shared endpoint with strict containment") {
        const Segment host_segment(center - Point{3, 3}, center + Point{3, 3});
        const Segment contained_branch(center, center + Point{1, 1});

        check_center_is_strictly_inside(host_segment, center);
        CHECK(host_segment.midpoint() == center);
        check_center_is_an_endpoint(contained_branch, center);

        CHECK(host_segment.intersects(contained_branch));
        CHECK(contained_branch.intersects(host_segment));
        CHECK_FALSE(host_segment.crosses(contained_branch));
        CHECK(host_segment.contains(contained_branch));
        CHECK_FALSE(contained_branch.contains(host_segment));
        CHECK(host_segment.interiorsIntersect(contained_branch));
        CHECK_FALSE(host_segment.separates(contained_branch));
        CHECK(contained_branch.separates(host_segment));
    }
}

TEST_CASE("Rational segment intersections return the expected shape") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using Segment = pgl::Segment<Point>;

    const auto rational = [](int64_t numerator, int64_t denominator = 1) {
        return Rational(numerator, denominator);
    };

    SUBCASE("crossing segments intersect at a rational interior point") {
        const Segment rising_diagonal(Point{rational(0), rational(0)}, Point{rational(1), rational(1)});
        const Segment vertical_slice(Point{rational(1, 2), rational(0)}, Point{rational(1, 2), rational(2)});
        const auto intersection = rising_diagonal.intersection(vertical_slice);

        INFO("first=" << rising_diagonal << " second=" << vertical_slice);
        REQUIRE(intersection);
        REQUIRE(std::holds_alternative<Point>(*intersection));

        const Point expected_point(rational(1, 2), rational(1, 2));
        CHECK(rising_diagonal.crosses(vertical_slice));
        CHECK(std::get<Point>(*intersection) == expected_point);
        CHECK(rising_diagonal.interiorContains(expected_point));
        CHECK(vertical_slice.interiorContains(expected_point));
    }

    SUBCASE("touching segments intersect at a shared endpoint") {
        const Segment first_segment(Point{rational(0), rational(0)}, Point{rational(1), rational(1)});
        const Segment second_segment(Point{rational(1), rational(1)}, Point{rational(3, 2), rational(0)});
        const auto intersection = first_segment.intersection(second_segment);

        INFO("first=" << first_segment << " second=" << second_segment);
        REQUIRE(intersection);
        REQUIRE(std::holds_alternative<Point>(*intersection));

        const Point shared_endpoint(rational(1), rational(1));
        CHECK(first_segment.intersects(second_segment));
        CHECK_FALSE(first_segment.crosses(second_segment));
        CHECK(std::get<Point>(*intersection) == shared_endpoint);
        CHECK(first_segment.contains(shared_endpoint));
        CHECK(second_segment.contains(shared_endpoint));
    }

    SUBCASE("colinear overlap returns the shared subsegment") {
        const Segment outer_segment(Point{rational(0), rational(0)}, Point{rational(2), rational(0)});
        const Segment inner_segment(Point{rational(1, 2), rational(0)}, Point{rational(3, 2), rational(0)});
        const auto intersection = outer_segment.intersection(inner_segment);

        INFO("first=" << outer_segment << " second=" << inner_segment);
        REQUIRE(intersection);
        REQUIRE(std::holds_alternative<Segment>(*intersection));

        CHECK(outer_segment.intersects(inner_segment));
        CHECK_FALSE(outer_segment.crosses(inner_segment));
        CHECK(std::get<Segment>(*intersection) == inner_segment);
        CHECK(outer_segment.contains(std::get<Segment>(*intersection)));
        CHECK(inner_segment.contains(std::get<Segment>(*intersection)));
    }

    SUBCASE("identical segments return the full segment") {
        const Segment reference_segment(Point{rational(1, 3), rational(2, 3)}, Point{rational(5, 3), rational(8, 3)});
        const Segment identical_segment(Point{rational(1, 3), rational(2, 3)}, Point{rational(5, 3), rational(8, 3)});
        const auto intersection = reference_segment.intersection(identical_segment);

        INFO("first=" << reference_segment << " second=" << identical_segment);
        REQUIRE(intersection);
        REQUIRE(std::holds_alternative<Segment>(*intersection));

        CHECK(reference_segment.intersects(identical_segment));
        CHECK(std::get<Segment>(*intersection) == reference_segment);
    }

    SUBCASE("disjoint segments return no intersection") {
        const Segment first_segment(Point{rational(0), rational(0)}, Point{rational(1), rational(0)});
        const Segment second_segment(Point{rational(2), rational(1)}, Point{rational(3), rational(1)});
        const auto intersection = first_segment.intersection(second_segment);

        INFO("first=" << first_segment << " second=" << second_segment);
        CHECK_FALSE(first_segment.intersects(second_segment));
        CHECK_FALSE(intersection);
    }
}
