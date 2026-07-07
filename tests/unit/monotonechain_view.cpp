#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <compare>
#include <functional>
#include <type_traits>
#include <vector>

// MonotoneChainView is MonotoneChain instantiated over a std::span: it borrows
// an external, already-canonical vertex buffer instead of owning one. These
// tests pin down the borrowing semantics and confirm the view answers the
// read-only contract identically to an owning chain over the same points.

using Point = pgl::Point<int>;
using Chain = pgl::MonotoneChain<Point>;
using View = pgl::MonotoneChainView<Point>;

// True when the chain type exposes the owning-only in-place insert; used to
// assert that view instantiations drop mutating members from their interface.
template <class T>
constexpr bool hasInsert() {
    return requires(T t, Point p) { t.insert(p); };
}

TEST_CASE("MonotoneChainView borrows an external vertex buffer") {
    // Canonical (sorted, duplicate-free) storage owned by the caller.
    const std::vector<Point> pts{Point(0, 0), Point(1, 2), Point(2, 1), Point(4, 3)};

    SUBCASE("view reflects the external vertices without copying") {
        const View view(pts);
        REQUIRE(view.size() == 4);
        CHECK(view[0] == Point(0, 0));
        CHECK(view[3] == Point(4, 3));
        CHECK_FALSE(view.empty());
        // The span points at the caller's buffer, so &view[0] must be &pts[0]
        // (operator[] returns by value, so compare vertices, not addresses).
        CHECK(view.vertices().front() == pts.front());
    }

    SUBCASE("a view compares equal to the owning chain over the same points") {
        const Chain owning(pts, /*trusted=*/true);
        const View view(pts);
        CHECK(view == owning);
        CHECK(owning == view);
        CHECK((view <=> owning) == std::strong_ordering::equal);
    }

    SUBCASE("mutating the external buffer is visible through the view") {
        std::vector<Point> buffer{Point(0, 0), Point(1, 1), Point(2, 2)};
        const View view(buffer);
        REQUIRE(view[1] == Point(1, 1));
        buffer[1] = Point(1, 5);  // still canonical
        CHECK(view[1] == Point(1, 5));
    }
}

TEST_CASE("MonotoneChainView answers read-only predicates like an owning chain") {
    const std::vector<Point> pts{Point(0, 0), Point(2, 2), Point(4, 0)};
    const Chain owning(pts, /*trusted=*/true);
    const View view(pts);

    SUBCASE("contains / isBelow / yAtX agree") {
        CHECK(view.contains(Point(2, 2)) == owning.contains(Point(2, 2)));
        CHECK(view.contains(Point(3, 3)) == owning.contains(Point(3, 3)));
        CHECK(view.isBelow(Point(1, 5)).has_value() == owning.isBelow(Point(1, 5)).has_value());
        CHECK(view.yAtX(1) == owning.yAtX(1));
    }

    SUBCASE("bounding box and length agree") {
        CHECK(view.bbox() == owning.bbox());
        CHECK(view.template length<double>() == doctest::Approx(owning.template length<double>()));
    }

    SUBCASE("intersection with a segment agrees") {
        const pgl::Segment<Point> seg(Point(0, 1), Point(4, 1));
        CHECK(view.intersects(seg) == owning.intersects(seg));
    }

    SUBCASE("hashing matches the owning chain") {
        CHECK(std::hash<View>{}(view) == std::hash<Chain>{}(owning));
    }
}

TEST_CASE("MonotoneChainView supports O(1) translation without touching the buffer") {
    const std::vector<Point> pts{Point(0, 0), Point(1, 1), Point(2, 0)};
    View view(pts);

    view += Point(10, 5);
    CHECK(view[0] == Point(10, 5));
    CHECK(view[2] == Point(12, 5));
    // The underlying buffer is untouched; the shift lives in the view's lazy
    // translation only.
    CHECK(pts[0] == Point(0, 0));

    // A translated view still equals the correspondingly translated owning chain.
    const Chain owning(pts, /*trusted=*/true);
    CHECK(view == (owning + Point(10, 5)));
}

TEST_CASE("MonotoneChainView value-returning transforms yield owning chains") {
    const std::vector<Point> pts{Point(0, 0), Point(1, 2), Point(2, 1)};
    const View view(pts);

    const auto rotated = view.rotated90();
    // The result owns its vertices (a vector-backed chain), not a span.
    static_assert(std::is_same_v<decltype(rotated), const Chain>);

    const Chain owning(pts, /*trusted=*/true);
    CHECK(rotated == owning.rotated90());

    const auto scaled = view.scaledUpX(3);
    static_assert(std::is_same_v<std::remove_const_t<decltype(scaled)>, Chain>);
    CHECK(scaled == owning.scaledUpX(3));
}

TEST_CASE("MonotoneChainView satisfies MonotoneChainConcept and drops mutators") {
    static_assert(pgl::MonotoneChainConcept<View>);
    static_assert(hasInsert<Chain>(), "owning chain has insert");
    static_assert(!hasInsert<View>(), "view must not expose insert");
}
