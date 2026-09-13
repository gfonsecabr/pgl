#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using Point = pgl::Point<int>;
using LabeledPoint = pgl::Point<int, std::string>;

template <class Number>
auto sampleShapes() {
    using SamplePoint = pgl::Point<Number>;
    using SamplePolygon = pgl::Polygon<SamplePoint>;
    using SampleRegion = pgl::PolygonWithHoles<SamplePoint>;
    const SamplePolygon square({Number(0), Number(0), Number(2), Number(0),
                                Number(2), Number(2), Number(0), Number(2)});
    const SampleRegion region(square);
    return std::tuple{
        pgl::EmptyShape<SamplePoint>{},
        SamplePoint(0, 0),
        pgl::Segment<SamplePoint>({0, 0}, {1, 0}),
        pgl::OrientedSegment<SamplePoint>({1, 0}, {0, 0}),
        pgl::Line<SamplePoint>({0, 0}, {1, 0}),
        pgl::OrientedLine<SamplePoint>({1, 0}, {0, 0}),
        pgl::Ray<SamplePoint>({0, 0}, {1, 0}),
        pgl::Halfplane<SamplePoint>({0, 0}, {1, 0}),
        pgl::Rectangle<SamplePoint>({0, 0}, {2, 2}),
        pgl::Triangle<SamplePoint>({0, 0}, {2, 0}, {0, 2}),
        pgl::Disk<SamplePoint>(Number(0), Number(0), Number(1)),
        pgl::Convex<SamplePoint>(
            {SamplePoint(0, 0), SamplePoint(2, 0), SamplePoint(0, 2)}),
        pgl::MonotoneChain<SamplePoint>({SamplePoint(0, 0), SamplePoint(1, 0)}),
        pgl::Polyline<SamplePoint>({SamplePoint(0, 0), SamplePoint(1, 0)}),
        square,
        pgl::HalfplaneIntersection<SamplePoint>(
            pgl::Rectangle<SamplePoint>({0, 0}, {2, 2})),
        region,
        pgl::PolygonSet<SamplePoint>(region),
        pgl::Shape<SamplePoint>(square)};
}

template <class S, class Label>
concept HasWithLabel = requires(const S& s) { s.template withLabel<Label>(); };

template <class S, class Label>
concept HasWithPointLabel = requires(const S& s) { s.template withPointLabel<Label>(); };

template <class S>
struct PointLabelOf {
    using type = typename S::PointType::LabelType;
};

template <class Number, class Label>
struct PointLabelOf<pgl::Point<Number, Label>> {
    using type = Label;
};

template <class S>
void checkFamily(const S& s) {
    const auto wide = s.template with<long long>();
    CHECK(std::same_as<typename std::remove_cvref_t<decltype(wide)>::NumberType, long long>);
    CHECK(std::same_as<typename std::remove_cvref_t<decltype(wide)>::LabelType, typename S::LabelType>);
    CHECK(wide.samePointSet(s));
    CHECK(wide.template with<int>().samePointSet(s));

    const auto labeled = s.template withLabel<int>();
    CHECK(std::same_as<typename std::remove_cvref_t<decltype(labeled)>::LabelType, int>);
    CHECK(labeled.samePointSet(s));

    const auto pointLabeled = s.template withPointLabel<int>();
    CHECK(std::same_as<typename PointLabelOf<std::remove_cvref_t<decltype(pointLabeled)>>::type, int>);
    CHECK(std::same_as<typename std::remove_cvref_t<decltype(pointLabeled)>::NumberType, int>);
    CHECK(pointLabeled.samePointSet(s));
}

}  // namespace

TEST_CASE("with, withLabel and withPointLabel are defined for every shape kind") {
    std::apply([](const auto&... shapes) { (checkFamily(shapes), ...); }, sampleShapes<int>());
}

TEST_CASE("with changes only the number type and keeps both labels") {
    using Source = pgl::Segment<LabeledPoint, std::string>;
    const Source s(LabeledPoint(3, 4, "b"), LabeledPoint(0, 0, "a"), std::string("s"));

    const auto t = s.with<long>();
    CHECK(std::same_as<decltype(t), const pgl::Segment<pgl::Point<long, std::string>, std::string>>);
    CHECK(t.samePointSet(pgl::Segment<pgl::Point<long>>({0, 0}, {3, 4})));
    CHECK(t.min().label() == "a");
    CHECK(t.max().label() == "b");
    CHECK(t.label() == "s");

    const auto r = s.with<pgl::ERational>();
    CHECK(r.max() == pgl::Point<pgl::ERational>(3, 4));
    CHECK(r.label() == "s");
}

TEST_CASE("withLabel and withPointLabel convert, create or drop labels") {
    using Source = pgl::Segment<LabeledPoint, std::string>;
    const Source s(LabeledPoint(0, 0, "a"), LabeledPoint(3, 4, "b"), std::string("s"));

    const auto view = s.withLabel<std::string_view>();
    CHECK(std::same_as<decltype(view), const pgl::Segment<LabeledPoint, std::string_view>>);
    CHECK(view.label() == "s");
    CHECK(view.min().label() == "a");

    const auto dropped = s.withLabel<pgl::NoLabel>().withPointLabel<pgl::NoLabel>();
    CHECK(std::same_as<decltype(dropped), const pgl::Segment<Point>>);
    CHECK(dropped == pgl::Segment<Point>({0, 0}, {3, 4}));

    const auto created = dropped.withLabel<std::string>().withPointLabel<int>();
    CHECK(std::same_as<decltype(created), const pgl::Segment<pgl::Point<int, int>, std::string>>);
    CHECK(created.label().empty());
    CHECK(created.max().label() == 0);

    const auto points = s.withPointLabel<std::string_view>();
    CHECK(std::same_as<decltype(points), const pgl::Segment<pgl::Point<int, std::string_view>, std::string>>);
    CHECK(points.max().label() == "b");
    CHECK(points.label() == "s");
}

TEST_CASE("withLabel and withPointLabel reject labels that cannot be converted") {
    CHECK_FALSE(HasWithLabel<pgl::Segment<Point, std::string>, std::vector<int>>);
    CHECK_FALSE(HasWithPointLabel<pgl::Segment<LabeledPoint>, std::vector<int>>);
    CHECK_FALSE(HasWithLabel<LabeledPoint, std::vector<int>>);
    CHECK_FALSE(HasWithLabel<pgl::Shape<LabeledPoint>, std::vector<int>>);
    CHECK(HasWithLabel<pgl::Segment<Point, std::string>, std::string_view>);
    CHECK(HasWithPointLabel<pgl::Segment<Point, std::string>, std::vector<int>>);
}

TEST_CASE("Point: withLabel and withPointLabel are the same") {
    constexpr pgl::Point<int> p(1, 2);
    static_assert(p.with<long>() == pgl::Point<long>(1, 2));

    const LabeledPoint q(1, 2, "q");
    CHECK(q.with<double>().label() == "q");
    CHECK(std::same_as<decltype(q.withLabel<std::string_view>()), pgl::Point<int, std::string_view>>);
    CHECK(q.withPointLabel<std::string_view>().label() == "q");
}

TEST_CASE("Regions keep their own label and their vertex labels") {
    using LabeledPolygon = pgl::Polygon<LabeledPoint>;
    const LabeledPolygon square({LabeledPoint(0, 0, "a"), LabeledPoint(2, 0, "b"),
                                 LabeledPoint(2, 2, "c"), LabeledPoint(0, 2, "d")});

    pgl::PolygonWithHoles<LabeledPoint, std::string> region(square);
    region.label() = "region";
    const auto wideRegion = region.with<double>();
    CHECK(wideRegion.label() == "region");
    CHECK(wideRegion.outer().vertices()[0].label() == "a");

    pgl::PolygonSet<LabeledPoint, std::string> set{pgl::PolygonWithHoles<LabeledPoint>(square)};
    set.label() = "set";
    const auto wideSet = set.with<double>();
    CHECK(wideSet.label() == "set");
    CHECK(wideSet.components()[0].outer().vertices()[0].label() == "a");
}

TEST_CASE("Vertex sequences keep their vertex labels") {
    const std::vector<LabeledPoint> points{LabeledPoint(0, 0, "a"), LabeledPoint(2, 0, "b"),
                                           LabeledPoint(0, 2, "c")};
    const auto firstLabel = [](const auto& shape) { return shape.vertices()[0].label(); };

    CHECK(firstLabel(pgl::Polygon<LabeledPoint>(points).with<double>()) == "a");
    CHECK(firstLabel(pgl::Convex<LabeledPoint>(points).with<double>()) == "a");
    CHECK(firstLabel(pgl::Polyline<LabeledPoint>(points).with<double>()) == "a");
    CHECK(firstLabel(pgl::MonotoneChain<LabeledPoint>(points).with<double>()) == "a");

    pgl::Polygon<LabeledPoint> moved(points);
    moved += LabeledPoint(1, 1);
    const auto wideMoved = moved.with<long>();
    CHECK(wideMoved.vertices()[0] == pgl::Point<long, std::string>(1, 1));
    CHECK(firstLabel(wideMoved) == "a");
}

TEST_CASE("MonotoneChain: a view converts into an owning chain") {
    const std::vector<Point> points{Point(0, 0), Point(1, 2), Point(2, 1)};
    const pgl::MonotoneChainView<Point> view(points);
    const auto chain = view.with<long>();
    CHECK(std::same_as<decltype(chain), const pgl::MonotoneChain<pgl::Point<long>>>);
    CHECK(chain.size() == 3);
    CHECK(chain[1] == pgl::Point<long>(1, 2));
}

TEST_CASE("Shape: with converts the held alternative") {
    const pgl::Shape<LabeledPoint> shape(
        pgl::Segment<LabeledPoint>(LabeledPoint(0, 0, "a"), LabeledPoint(3, 4, "b")));

    const auto wide = shape.with<double>();
    CHECK(std::same_as<decltype(wide), const pgl::Shape<pgl::Point<double, std::string>>>);
    REQUIRE(wide.holdsSegment());
    CHECK(wide.asHeldSegment().max().label() == "b");

    const auto relabeled = shape.withLabel<std::string_view>();
    CHECK(std::same_as<decltype(relabeled), const pgl::Shape<pgl::Point<int, std::string_view>>>);
    CHECK(relabeled.asHeldSegment().min().label() == "a");
}

TEST_CASE("with works on a dependent receiver") {
    const auto widen = [](const auto& shape) { return shape.template with<long long>(); };
    CHECK(widen(pgl::Triangle<Point>({0, 0}, {2, 0}, {0, 2})) ==
          pgl::Triangle<pgl::Point<long long>>({0, 0}, {2, 0}, {0, 2}));
}
