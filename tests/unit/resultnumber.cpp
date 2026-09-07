#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

#include "pgl.hpp"

namespace {

using Point = pgl::Point<int>;
using EPoint = pgl::Point<pgl::ERational>;
using FloatPoint = pgl::Point<float>;
using Rational = pgl::Rational<long long>;
using RationalPoint = pgl::Point<Rational>;

template <class Shape>
constexpr bool hasNativeAreaResultPolicy =
    std::is_same_v<decltype(std::declval<const Shape&>().area()), typename Shape::NumberType> &&
    std::is_same_v<decltype(std::declval<const Shape&>().template area<long double>()), long double>;

static_assert(hasNativeAreaResultPolicy<pgl::Segment<Point>>);
static_assert(hasNativeAreaResultPolicy<pgl::OrientedSegment<Point>>);
static_assert(hasNativeAreaResultPolicy<pgl::Line<Point>>);
static_assert(hasNativeAreaResultPolicy<pgl::OrientedLine<Point>>);
static_assert(hasNativeAreaResultPolicy<pgl::Ray<Point>>);
static_assert(hasNativeAreaResultPolicy<pgl::Rectangle<Point>>);

static_assert(std::is_same_v<pgl::division_result_t<int>, pgl::ERational>);
static_assert(std::is_same_v<pgl::division_result_t<pgl::BigInt>, pgl::ERational>);
static_assert(std::is_same_v<pgl::division_result_t<float>, float>);
static_assert(std::is_same_v<pgl::division_result_t<Rational>, Rational>);

static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<Point>&>().midpoint()),
                             EPoint>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<FloatPoint>&>().midpoint()),
                             FloatPoint>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<RationalPoint>&>().midpoint()),
                             RationalPoint>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<FloatPoint>&>().intersection(
                                 std::declval<const pgl::Segment<FloatPoint>&>())),
                             std::optional<std::variant<FloatPoint, pgl::Segment<FloatPoint>>>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<FloatPoint>&>().squaredDistance(
                                 std::declval<const FloatPoint&>())),
                             float>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().squaredDistance(
                                 std::declval<const Point&>())),
                             int>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().distanceL1(
                                 std::declval<const Point&>())),
                             int>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().distanceL1<pgl::ERational>(
                                 std::declval<const Point&>())),
                             pgl::ERational>);
static_assert(std::is_same_v<decltype(std::declval<const FloatPoint&>().distanceLInf(
                                 std::declval<const FloatPoint&>())),
                             float>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Rectangle<Point>&>().squaredDistance(
                                 std::declval<const Point&>())),
                             int>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().squaredDistance(
                                 std::declval<const pgl::Rectangle<Point>&>())),
                             int>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().intersection(
                                 std::declval<const Point&>())),
                             std::optional<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Rectangle<Point>&>().intersection(
                                 std::declval<const pgl::Rectangle<Point>&>())),
                             std::optional<pgl::Rectangle<Point>>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Halfplane<Point>&>().intersection(
                                 std::declval<const pgl::Halfplane<Point>&>())),
                             pgl::HalfplaneIntersection<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Halfplane<Point>&>().intersection(
                                 std::declval<const pgl::HalfplaneIntersection<Point>&>())),
                             pgl::HalfplaneIntersection<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Rectangle<Point>&>().intersection(
                                 std::declval<const pgl::HalfplaneIntersection<Point>&>())),
                             pgl::HalfplaneIntersection<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Triangle<Point>&>().intersection(
                                 std::declval<const pgl::HalfplaneIntersection<Point>&>())),
                             pgl::HalfplaneIntersection<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Convex<Point>&>().intersection(
                                 std::declval<const pgl::HalfplaneIntersection<Point>&>())),
                             pgl::HalfplaneIntersection<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().dual()),
                             pgl::Line<Point>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Triangle<Point>&>().area()),
                             pgl::ERational>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Triangle<FloatPoint>&>().area()),
                             float>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Triangle<RationalPoint>&>().area()),
                             Rational>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().center()),
                             EPoint>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<FloatPoint>&>().center()),
                             FloatPoint>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().squaredRadius()),
                             pgl::ERational>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().radius()),
                             double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().squaredDistance(
                                 std::declval<const Point&>())),
                             double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().distanceL1(
                                 std::declval<const Point&>())),
                             double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().distanceLInf<long double>(
                                 std::declval<const Point&>())),
                             long double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().distanceL1(
                                 std::declval<const pgl::Shape<Point>&>())),
                             double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Disk<Point>&>().distanceLInf<long double>(
                                 std::declval<const pgl::Shape<Point>&>())),
                             long double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::HalfplaneIntersection<Point>&>().bbox()),
                             pgl::Rectangle<EPoint>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Transformation<int>&>().inverse()),
                             pgl::Transformation<pgl::ERational>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Transformation<float>&>().inverse()),
                             pgl::Transformation<float>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Shape<Point>&>().squaredDistance(
                                 std::declval<const Point&>())),
                             double>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Shape<Point>&>().intersection(
                                 std::declval<const Point&>())),
                             pgl::Shape<EPoint>>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<Point>&>().squaredDistance(
                                 std::declval<const Point&>())),
                             pgl::ERational>);
static_assert(std::is_same_v<decltype(std::declval<const pgl::Segment<Point>&>().squaredHausdorffDistance(
                                 std::declval<const Point&>())),
                             int>);
static_assert(std::is_same_v<decltype(std::declval<const Point&>().squaredHausdorffDistance(
                                 std::declval<const pgl::Segment<Point>&>())),
                             int>);

} // namespace

TEST_CASE("Rational constructions are exact by default") {
    const pgl::Segment<Point> diagonal(Point(0, 0), Point(2, 1));
    CHECK(diagonal.midpoint() == EPoint(pgl::ERational(1), pgl::ERational(1, 2)));

    const pgl::Segment<Point> vertical(Point(1, -1), Point(1, 2));
    const auto crossing = diagonal.intersection(vertical);
    REQUIRE(crossing);
    REQUIRE(std::holds_alternative<EPoint>(*crossing));
    CHECK(std::get<EPoint>(*crossing) == EPoint(pgl::ERational(1), pgl::ERational(1, 2)));
}

TEST_CASE("Disk keeps rational invariants exact and irrational values floating") {
    const pgl::Disk<Point> disk(Point(0, 0), Point(2, 0), Point(0, 2));
    CHECK(disk.center() == EPoint(1, 1));
    CHECK(disk.squaredRadius() == pgl::ERational(2));
    CHECK(disk.radius() == doctest::Approx(std::sqrt(2.0)));
}

// Every signed built-in integer must widen, whatever the data model calls it.
//
// Regression: the promotion trait was specialized on the fixed-width aliases
// alone. `int`, `long` and `long long` are three distinct types and the aliases
// name only two of them -- `int64_t` is `long` under LP64 and `long long` under
// LLP64 -- so the third matched nothing but the identity primary template and
// promoted to itself. That is invisible at the use site: a `long long`
// coordinate still compiles everywhere, and every overflow guard built on the
// trait is simply absent, so an intermediate meant to be evaluated 128 bits
// wide wraps in 64 instead.
namespace promotion {

template <class T>
using Promoted = pgl::detail::promoted_number_t<T>;

// Doubling the width is what the guard is for: the products the predicates form
// are degree two and three in the coordinates.
template <class T>
constexpr bool widens = sizeof(Promoted<T>) >= 2 * sizeof(T);

static_assert(widens<signed char>);
static_assert(widens<short>);
static_assert(widens<int>);
static_assert(widens<long>);
static_assert(widens<long long>);
static_assert(widens<std::int8_t>);
static_assert(widens<std::int16_t>);
static_assert(widens<std::int32_t>);
static_assert(widens<std::int64_t>);
static_assert(widens<std::ptrdiff_t>);

// The top of the chain leaves fixed width behind rather than saturating, so
// sizeof stops being the measure.
static_assert(std::is_same_v<Promoted<pgl::int128>, pgl::BigInt>);
static_assert(std::is_same_v<Promoted<long long>, pgl::int128>);
static_assert(std::is_same_v<Promoted<Promoted<long long>>, pgl::BigInt>);

// Rational manages its own overflow by reducing, so it is deliberately fixed.
static_assert(std::is_same_v<Promoted<Rational>, Rational>);
static_assert(std::is_same_v<Promoted<pgl::BigInt>, pgl::BigInt>);

}  // namespace promotion

// BigInt has to be convertible back to each of them for the same reason: the
// conversion operators were named after the same two aliases.
TEST_CASE("BigInt converts to every signed built-in integer width") {
    const pgl::BigInt value(-1234567890123LL);

    CHECK(static_cast<int>(pgl::BigInt(-42)) == -42);
    CHECK(static_cast<long>(value) == -1234567890123L);
    CHECK(static_cast<long long>(value) == -1234567890123LL);
    CHECK(static_cast<std::int64_t>(value) == -1234567890123LL);
    CHECK(static_cast<short>(pgl::BigInt(-3)) == -3);
    CHECK(static_cast<pgl::int128>(value) == pgl::int128(-1234567890123LL));
}
