#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <algorithm>
#include <compare>
#include <cstdlib>
#include <list>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <unordered_set>
#include <type_traits>
#include <utility>
#include <vector>

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Rect = pgl::Rectangle<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using Matrix = pgl::BitMatrix<>;
using pgl::GridAdjacency;

namespace {

struct Rng {
    std::uint64_t state = 88172645463325252ULL;

    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }

    int range(int low, int high) {
        return low + static_cast<int>(next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

using CellSet = std::set<std::pair<int, int>>;

CellSet cellsOf(const Matrix& matrix) {
    CellSet result;
    for (const Point& cell : matrix) {
        result.emplace(cell.x(), cell.y());
    }
    return result;
}

Matrix filledBox(int x, int y, int width, int height) {
    Matrix result(Point(x, y), width, height);
    result.setAll();
    return result;
}

// A random matrix, its window placed anywhere and its density varied so that
// both sparse and dense words occur.
Matrix randomMatrix(Rng& rng, int span = 8) {
    const Point origin(rng.range(-span, span), rng.range(-span, span));
    Matrix result(origin, rng.range(1, span * 2), rng.range(1, span * 2));
    const int threshold = rng.range(1, 9);
    for (int i = 0; i < result.width(); ++i) {
        for (int j = 0; j < result.height(); ++j) {
            if (rng.range(1, 10) <= threshold) {
                result.set(origin.x() + i, origin.y() + j);
            }
        }
    }
    return result;
}

}  // namespace

TEST_CASE("a default matrix has an empty window and no cells") {
    const Matrix matrix;
    CHECK(matrix.emptyWindow());
    CHECK(matrix.empty());
    CHECK(matrix.width() == 0);
    CHECK(matrix.height() == 0);
    CHECK(matrix.count() == 0);
    CHECK(matrix.window().empty());
    CHECK(matrix.bbox().empty());
    CHECK(matrix.begin() == matrix.end());
    CHECK(matrix.lattice().empty());
    CHECK(matrix == Matrix());
}

TEST_CASE("a non-positive extent empties the window") {
    CHECK(Matrix(Point(3, 4), 0, 10).emptyWindow());
    CHECK(Matrix(Point(3, 4), 10, 0).emptyWindow());
    CHECK(Matrix(Point(3, 4), -1, 10).emptyWindow());
    CHECK(!Matrix(Point(3, 4), 1, 1).emptyWindow());
}

TEST_CASE("the window is the rectangle its cells cover") {
    const Rect box(Point(-2, -3), Point(5, 4));
    const Matrix matrix(box);
    CHECK(matrix.origin() == Point(-2, -3));
    CHECK(matrix.width() == 7);
    CHECK(matrix.height() == 7);
    CHECK(matrix.window() == box);
    CHECK(matrix.inWindow(-2, -3));
    CHECK(matrix.inWindow(4, 3));
    CHECK(!matrix.inWindow(5, 3));   // max() is past the last cell
    CHECK(!matrix.inWindow(-3, -3));
    CHECK(Matrix(Rect()).emptyWindow());
}

TEST_CASE("cells outside the window are dropped on write and false on read") {
    Matrix matrix(Point(0, 0), 4, 4);
    matrix.set(2, 2);
    matrix.set(9, 9);
    matrix.set(-1, 0);
    CHECK(matrix.count() == 1);
    CHECK(matrix.get(2, 2));
    CHECK(!matrix.get(9, 9));
    CHECK(!matrix.get(-1, 0));
    CHECK(!matrix.get(100, 100));

    matrix.reset(9, 9);   // also a no-op, and must not corrupt anything
    matrix.flip(-5, -5);
    CHECK(matrix.count() == 1);

    matrix.set(2, 2, false);
    CHECK(matrix.empty());
    matrix.flip(3, 1);
    CHECK(matrix.get(3, 1));
}

TEST_CASE("cells are addressable across word boundaries") {
    Matrix matrix(Point(-100, -100), 200, 3);
    const std::vector<Point> written{Point(-100, -100), Point(-37, -99), Point(0, -100),
                                     Point(63, -98),    Point(64, -98),  Point(99, -98)};
    for (const Point& cell : written) {
        matrix.set(cell);
    }
    CHECK(matrix.count() == written.size());
    for (const Point& cell : written) {
        CHECK(matrix.get(cell));
        CHECK(!matrix.get(cell.x() + 1, cell.y() + 1));
    }
    CHECK(matrix.lattice().size() == written.size());
}

TEST_CASE("setAll fills the window and leaves no bit past the width") {
    for (int width : {1, 63, 64, 65, 127, 128, 129}) {
        Matrix matrix(Point(7, -7), width, 3);
        matrix.setAll();
        CHECK(matrix.count() == static_cast<std::size_t>(width) * 3);
        CHECK(matrix.bbox() == matrix.window());
        CHECK((~matrix).count() == 0);
        matrix.clear();
        CHECK(matrix.empty());
        CHECK(!matrix.emptyWindow());
    }
}

TEST_CASE("iteration visits the set cells in row-major order") {
    Matrix matrix(Point(0, 0), 70, 3);
    matrix.set(65, 2);
    matrix.set(3, 0);
    matrix.set(64, 0);
    matrix.set(1, 1);
    const std::vector<Point> expected{Point(3, 0), Point(64, 0), Point(1, 1), Point(65, 2)};
    CHECK(matrix.lattice() == expected);

    Matrix::Iterator it = matrix.begin();
    CHECK(*it == expected[0]);
    CHECK(it->x() == 3);
    const Matrix::Iterator copy = it++;
    CHECK(*copy == expected[0]);
    CHECK(*it == expected[1]);
    CHECK(copy != it);
}

TEST_CASE("equality compares the window along with the cells") {
    Matrix wide(Point(-10, -10), 40, 40);
    wide.set(0, 0);
    wide.set(5, 3);
    Matrix tight(Point(0, 0), 6, 4);
    tight.set(0, 0);
    tight.set(5, 3);

    // Same region, different value: the window is part of what a matrix is.
    CHECK(wide != tight);
    CHECK(wide.samePointSet(tight));
    CHECK(wide != wide.trimmed());
    CHECK(wide.samePointSet(wide.trimmed()));
    CHECK(wide.trimmed() == tight);
    CHECK(wide.trimmed().window() == wide.bbox());
    CHECK(wide.trimmed().origin() == Point(0, 0));
    CHECK(!wide.sameWindow(tight));
    CHECK(wide.sameWindow(wide));
    CHECK(wide == wide);

    // Trimming is idempotent, so it is the canonical form to compare.
    CHECK(wide.trimmed().trimmed() == wide.trimmed());

    // Every window covering no cell has the one canonical form.
    CHECK(Matrix(Point(7, 9), 0, 5) == Matrix());
    CHECK(Matrix(Point(7, 9), 0, 5).origin() == Point(0, 0));
    CHECK(Matrix(Point(0, 0), 3, 3) != Matrix(Point(1, 0), 3, 3));

    // The complement is relative to the window, so matrices covering the same
    // region over different windows have different complements.
    CHECK(!(~wide).samePointSet(~tight));

    std::unordered_set<Matrix> set;
    set.insert(wide);
    set.insert(tight);
    set.insert(wide.trimmed());
    CHECK(set.size() == 2);     // wide.trimmed() equals tight
    set.insert(~tight);
    CHECK(set.size() == 3);
}

TEST_CASE("matrices are ordered, so they key a set or a map") {
    const Matrix empty;
    Matrix low(Point(0, 0), 4, 4), high(Point(1, 0), 4, 4), wider(Point(0, 0), 5, 4);
    low.set(1, 1);
    high.set(1, 1);
    wider.set(1, 1);
    Matrix denser(Point(0, 0), 4, 4);
    denser.set(1, 1);
    denser.set(2, 2);

    CHECK(empty < low);
    CHECK(low < high);        // origin decides first
    CHECK(low < wider);       // then the width
    CHECK(low < denser);      // then the cells
    CHECK(!(low < low));
    CHECK((low <=> low) == std::strong_ordering::equal);
    CHECK((low <=> denser) == (low.lattice() <=> denser.lattice()));

    // The order is total and consistent with equality.
    std::set<Matrix> ordered{denser, low, high, wider, empty, low, denser};
    CHECK(ordered.size() == 5);
    CHECK(*ordered.begin() == empty);
    CHECK(ordered.count(low) == 1);
    CHECK(ordered.count(low.resized(Rect(Point(-2, -2), Point(9, 9)))) == 0);

    std::map<Matrix, int> counts;
    counts[low] = 1;
    counts[low.trimmed()] += 2;   // a different window, so a different key
    CHECK(counts.size() == 2);
    CHECK(counts[low] == 1);
}

TEST_CASE("resized keeps the cells the new window holds") {
    Matrix matrix(Point(0, 0), 10, 10);
    matrix.set(0, 0);
    matrix.set(9, 9);
    matrix.set(4, 5);

    const Matrix grown = matrix.resized(Rect(Point(-5, -5), Point(20, 20)));
    CHECK(grown.samePointSet(matrix));
    CHECK(grown != matrix);
    CHECK(grown.origin() == Point(-5, -5));
    CHECK(grown.width() == 25);

    const Matrix cropped = matrix.resized(Rect(Point(0, 0), Point(5, 6)));
    CHECK(cropped.count() == 2);
    CHECK(cropped.get(0, 0));
    CHECK(cropped.get(4, 5));
    CHECK(!cropped.get(9, 9));

    CHECK(matrix.resized(Rect()).empty());
    CHECK(Matrix().trimmed().emptyWindow());
}

TEST_CASE("the complement is taken within the window") {
    Matrix matrix(Point(0, 0), 3, 2);
    matrix.set(1, 1);
    const Matrix complement = ~matrix;
    CHECK(complement.count() == 5);
    CHECK(complement.sameWindow(matrix));
    CHECK(!complement.get(1, 1));
    CHECK(complement.get(0, 0));
    CHECK(~complement == matrix);
    CHECK(!matrix.interiorsIntersect(complement));   // no shared cell
    CHECK(matrix.intersects(complement));            // but the regions touch
    CHECK(matrix.orCount(complement) == 6);
}

TEST_CASE("binary operators return the smallest window that loses nothing") {
    Matrix left(Point(0, 0), 10, 10), right(Point(5, 5), 10, 10);
    left.set(1, 1);
    left.set(7, 7);
    right.set(7, 7);
    right.set(12, 12);

    CHECK((left & right).window() == Rect(Point(5, 5), Point(10, 10)));
    CHECK((left | right).window() == Rect(Point(0, 0), Point(15, 15)));
    CHECK((left ^ right).window() == Rect(Point(0, 0), Point(15, 15)));
    CHECK(left.difference(right).window() == left.window());

    CHECK((left & right).lattice() == std::vector<Point>{Point(7, 7)});
    CHECK((left | right).count() == 3);
    CHECK((left ^ right).count() == 2);
    CHECK(left.difference(right).lattice() == std::vector<Point>{Point(1, 1)});
    CHECK(left.symmetricDifference(right) == (left ^ right));

    CHECK(left.andCount(right) == 1);
    CHECK(left.orCount(right) == 3);
    CHECK(left.xorCount(right) == 2);
    CHECK(left.intersects(right));
    CHECK(left.contains(left & right));
    CHECK(!left.contains(right));
    CHECK(left.contains(left));
    CHECK(Matrix().contains(Matrix()));
    CHECK(left.samePointSet(left.trimmed()));
    CHECK(!left.samePointSet(right));
    CHECK(left != left.trimmed());
}

TEST_CASE("compound assignment never moves the window") {
    Matrix left(Point(0, 0), 4, 4);
    left.set(1, 1);
    Matrix right(Point(3, 3), 4, 4);
    right.set(3, 3);
    right.set(6, 6);

    Matrix united = left;
    united |= right;
    CHECK(united.sameWindow(left));
    CHECK(united.count() == 2);     // (6,6) falls outside and is dropped
    CHECK(united.get(3, 3));

    Matrix flipped = left;
    flipped ^= left;
    CHECK(flipped.empty());

    Matrix kept = left;
    kept &= left;
    CHECK(kept == left);
}

TEST_CASE("set algebra agrees with a brute-force reference") {
    Rng rng;
    for (int trial = 0; trial < 300; ++trial) {
        const Matrix left = randomMatrix(rng), right = randomMatrix(rng);
        const CellSet leftCells = cellsOf(left), rightCells = cellsOf(right);

        CellSet expectedAnd, expectedOr, expectedXor, expectedDifference;
        for (const auto& cell : leftCells) {
            if (rightCells.count(cell) != 0) {
                expectedAnd.insert(cell);
            } else {
                expectedDifference.insert(cell);
            }
        }
        expectedOr = leftCells;
        expectedOr.insert(rightCells.begin(), rightCells.end());
        expectedXor = expectedOr;
        for (const auto& cell : expectedAnd) {
            expectedXor.erase(cell);
        }

        CHECK(cellsOf(left & right) == expectedAnd);
        CHECK(cellsOf(left | right) == expectedOr);
        CHECK(cellsOf(left ^ right) == expectedXor);
        CHECK(cellsOf(left.difference(right)) == expectedDifference);
        CHECK(left.andCount(right) == expectedAnd.size());
        CHECK(left.orCount(right) == expectedOr.size());
        CHECK(left.xorCount(right) == expectedXor.size());
        CHECK(left.contains(right) == (expectedAnd.size() == rightCells.size()));
        CHECK(left.interiorsIntersect(right) == !expectedAnd.empty());
        CHECK(left.samePointSet(right) == (leftCells == rightCells));
        CHECK((left == right) == (left.sameWindow(right) && leftCells == rightCells));
        CHECK((left.trimmed() == right.trimmed()) == (leftCells == rightCells));

        // The cells are closed squares, so the regions meet as soon as two of
        // them come within Chebyshev distance one of each other.
        bool touching = false;
        for (const auto& [ax, ay] : leftCells) {
            for (const auto& [bx, by] : rightCells) {
                touching = touching || (std::abs(ax - bx) <= 1 && std::abs(ay - by) <= 1);
            }
        }
        CHECK(left.intersects(right) == touching);
    }
}

TEST_CASE("area, perimeter, bounding box and centroid describe the covered region") {
    const Matrix block = filledBox(2, 3, 5, 4);
    CHECK(block.area() == 20);
    CHECK(block.perimeter() == 18);
    CHECK(block.bbox() == Rect(Point(2, 3), Point(7, 7)));
    CHECK(block.centroid() == pgl::Point<pgl::Rational<int>>(pgl::Rational<int>(9, 2),
                                                             pgl::Rational<int>(5)));

    Matrix single(Point(0, 0), 3, 3);
    single.set(1, 1);
    CHECK(single.perimeter() == 4);
    CHECK(single.centroid() == pgl::Point<pgl::Rational<int>>(pgl::Rational<int>(3, 2),
                                                              pgl::Rational<int>(3, 2)));

    // Two cells touching only at a corner keep all eight of their edges.
    Matrix diagonal(Point(0, 0), 3, 3);
    diagonal.set(0, 0);
    diagonal.set(1, 1);
    CHECK(diagonal.perimeter() == 8);

    // The coordinate mass overflows int long before the exact result does.
    Matrix distant(Point(1000000, 0), 1000, 1000);
    distant.setAll();
    CHECK(distant.centroid() == pgl::Point<pgl::Rational<int>>(pgl::Rational<int>(1000500),
                                                               pgl::Rational<int>(500)));

    CHECK_THROWS_AS(static_cast<void>(Matrix().centroid()), std::logic_error);
}

TEST_CASE("area and perimeter agree with the region the cells cover") {
    Rng rng;
    for (int trial = 0; trial < 60; ++trial) {
        const Matrix matrix = randomMatrix(rng, 5);
        if (matrix.empty()) {
            continue;
        }
        const pgl::PolygonSet<Point> covered = matrix.asPolygonSet();
        CHECK(covered.area<int>() == matrix.area());
        CHECK(covered.bbox() == matrix.bbox());
    }
}

TEST_CASE("asPolygonSet reproduces the covered region") {
    // A comb: several components, one of them holed, so the conversion has to
    // trace an outer ring and a hole ring and keep the components apart.
    Matrix comb = filledBox(0, 0, 4, 4) | filledBox(6, 0, 1, 3) | filledBox(9, 2, 3, 3);
    comb.reset(1, 1);
    comb.reset(2, 1);

    const pgl::PolygonSet<Point> set = comb.asPolygonSet();
    CHECK(set.componentCount() == 3);
    CHECK(set.area<int>() == comb.area());
    CHECK(set.bbox() == comb.bbox());
    CHECK(set.isValid());

    // Every cell center is covered and no other lattice-cell center is.
    for (int x = -2; x < 14; ++x) {
        for (int y = -2; y < 7; ++y) {
            const pgl::Point<pgl::Rational<int>> center(pgl::Rational<int>(2 * x + 1, 2),
                                                        pgl::Rational<int>(2 * y + 1, 2));
            CHECK(set.contains(center) == comb.get(x, y));
        }
    }

    // The rings carry no vertex in the middle of a straight stretch: a filled
    // box is four corners however many cells it holds.
    const pgl::PolygonSet<Point> block = filledBox(0, 0, 10, 7).asPolygonSet();
    CHECK(block.componentCount() == 1);
    CHECK(block.components().front().outer().size() == 4);
    CHECK(block.components().front().holes().empty());

    CHECK(Matrix().asPolygonSet().componentCount() == 0);
    CHECK(Matrix().asPolygonSet().bbox() == Matrix().bbox());
}

TEST_CASE("bbox always agrees with asPolygonSet") {
    Rng rng;
    for (int trial = 0; trial < 150; ++trial) {
        const Matrix matrix = randomMatrix(rng, 5);
        const pgl::PolygonSet<Point> set = matrix.asPolygonSet();
        CHECK(matrix.bbox() == set.bbox());
        CHECK(set.area<int>() == matrix.area());
        CHECK(set.componentCount() == matrix.componentCount());
    }
    for (std::size_t size = 1; size <= 6; ++size) {
        for (const Region& region : pgl::polyominoRegions(size)) {
            const Matrix raster(region);
            CHECK(raster.bbox() == raster.asPolygonSet().bbox());
            CHECK(raster.bbox() == Rect(region.bbox()));
        }
    }
}

TEST_CASE("lattice and cells are the two readings of the same list") {
    Matrix matrix(Point(0, 0), 70, 3);
    matrix.set(3, 0);
    matrix.set(64, 0);
    matrix.set(65, 2);

    const std::vector<Point> points = matrix.lattice();
    const std::vector<Rect> squares = matrix.cells();
    CHECK(points == std::vector<Point>{Point(3, 0), Point(64, 0), Point(65, 2)});
    CHECK(squares.size() == points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        CHECK(squares[i] == Rect(points[i], Point(points[i].x() + 1, points[i].y() + 1)));
        CHECK(squares[i].min() == points[i]);
        CHECK(squares[i].area() == 1);
    }

    // The lattice points are what the iterators yield and what get() addresses.
    CHECK(std::vector<Point>(matrix.begin(), matrix.end()) == points);
    for (const Point& cell : points) {
        CHECK(matrix.get(cell));
    }

    // One square per cell here; rectangles() merges each row's runs instead.
    const Matrix block = filledBox(0, 0, 4, 2);
    CHECK(block.lattice().size() == 8);
    CHECK(block.cells().size() == 8);
    CHECK(block.rectangles().size() == 2);

    CHECK(Matrix().lattice().empty());
    CHECK(Matrix().cells().empty());
}

TEST_CASE("the views compute the same cells without allocating") {
    static_assert(std::forward_iterator<Matrix::Iterator>);
    static_assert(std::ranges::forward_range<decltype(std::declval<const Matrix&>().latticeView())>);
    static_assert(std::ranges::forward_range<decltype(std::declval<const Matrix&>().cellsView())>);
    static_assert(std::is_same_v<std::ranges::range_value_t<
                                     decltype(std::declval<const Matrix&>().cellsView())>,
                                 Rect>);

    Matrix matrix(Point(-3, -3), 80, 4);
    matrix.set(-3, -3);
    matrix.set(64, -1);
    matrix.set(70, 0);

    CHECK(std::ranges::distance(matrix.latticeView()) == 3);
    CHECK(std::vector<Point>(matrix.latticeView().begin(), matrix.latticeView().end()) ==
          matrix.lattice());
    const auto squares = matrix.cellsView();
    CHECK(std::vector<Rect>(squares.begin(), squares.end()) == matrix.cells());

    // The views compose with the standard algorithms and adaptors.
    CHECK(std::ranges::count_if(matrix.latticeView(),
                                [](const Point& cell) { return cell.y() < 0; }) == 2);
    int area = 0;
    for (const Rect& square : matrix.cellsView()) {
        area += square.area();
    }
    CHECK(area == matrix.area());

    // A view of a matrix with no cell is empty, not ill-formed.
    const Matrix nothing;
    CHECK(std::ranges::empty(nothing.latticeView()));
    CHECK(std::ranges::empty(nothing.cellsView()));

    // Re-reading a view after the matrix changes reflects the change: it holds
    // no copy of the cells.
    matrix.set(0, 0);
    CHECK(std::ranges::distance(matrix.latticeView()) == 4);
}

TEST_CASE("rectangles cover the cells as maximal runs") {
    Matrix matrix(Point(0, 0), 8, 2);
    matrix.set(0, 0);
    matrix.set(1, 0);
    matrix.set(2, 0);
    matrix.set(5, 0);
    matrix.set(7, 1);
    const std::vector<Rect> runs = matrix.rectangles();
    CHECK(runs.size() == 3);
    CHECK(runs[0] == Rect(Point(0, 0), Point(3, 1)));
    CHECK(runs[1] == Rect(Point(5, 0), Point(6, 1)));
    CHECK(runs[2] == Rect(Point(7, 1), Point(8, 2)));

    int total = 0;
    for (const Rect& run : runs) {
        total += run.area();
    }
    CHECK(total == static_cast<int>(matrix.count()));
}

TEST_CASE("a rectilinear region rasterizes to the cells it covers") {
    const PolygonShape ell({0, 0, 4, 0, 4, 1, 1, 1, 1, 3, 0, 3});
    const Region region(ell);
    const Matrix raster(region);

    CHECK(raster.window() == Rect(region.bbox()));
    CHECK(raster.area() == region.area<int>());
    CHECK(raster.perimeter() == 14);
    CHECK(raster.asPolygonWithHoles() == region);
    CHECK(raster.isConnected());
    CHECK(raster.holeCount() == 0);
    CHECK(raster.eulerNumber() == 1);
}

TEST_CASE("a region with a hole keeps it through the round trip") {
    const PolygonShape outer({0, 0, 5, 0, 5, 5, 0, 5});
    const PolygonShape hole({2, 2, 2, 3, 3, 3, 3, 2});
    const Region region(outer, std::vector<PolygonShape>{hole});
    const Matrix raster(region);

    CHECK(raster.area() == 24);
    CHECK(raster.componentCount() == 1);
    CHECK(raster.holeCount() == 1);
    CHECK(raster.eulerNumber() == 0);
    CHECK(raster.asPolygonWithHoles() == region);
    CHECK(raster.fillHoles().count() == 25);
    CHECK(raster.fillHoles().sameWindow(raster));
    CHECK(raster.fillHoles().holeCount() == 0);
}

TEST_CASE("a region that is not rectilinear is rejected") {
    const PolygonShape triangle({0, 0, 4, 0, 0, 4});
    CHECK_THROWS_AS(Matrix(Region(triangle)), std::logic_error);
}

TEST_CASE("rasterizing a polygon set inverts asPolygonSet") {
    Rng rng;
    for (int trial = 0; trial < 300; ++trial) {
        const Matrix matrix = randomMatrix(rng, 5);
        if (matrix.empty()) {
            continue;
        }
        // Holes, corner pinches and components nested in holes all occur here.
        const pgl::PolygonSet<Point> set = matrix.asPolygonSet();
        CHECK(set.asBitMatrix() == matrix.trimmed());
    }
}

TEST_CASE("a polygon and a set of regions rasterize like a region") {
    const PolygonShape ell({0, 0, 4, 0, 4, 1, 1, 1, 1, 3, 0, 3});
    CHECK(Matrix(ell) == Matrix(Region(ell)));
    CHECK(Matrix(ell) == ell.asBitMatrix());

    const PolygonShape square({5, 5, 7, 5, 7, 7, 5, 7});
    const pgl::PolygonSet<Point> set(std::vector{Region(ell), Region(square)});
    const Matrix raster(set);

    CHECK(raster.window() == Rect(set.bbox()));
    CHECK(raster == set.asBitMatrix());
    CHECK(raster.count() == 10);
    CHECK(raster.componentCount() == 2);
    CHECK(raster.asPolygonSet() == set);
    // The window spans the whole set, so a component alone covers less of it.
    CHECK(raster.contains(Matrix(ell)));

    CHECK_THROWS_AS(Matrix(PolygonShape({0, 0, 4, 0, 0, 4})), std::logic_error);
    CHECK_THROWS_AS(Matrix(pgl::PolygonSet<Point>(Region(PolygonShape({0, 0, 4, 0, 0, 4})))),
                    std::logic_error);
}

TEST_CASE("a shape over another coordinate type rasterizes when its coordinates are whole") {
    using RationalPoint = pgl::Point<pgl::Rational<int>>;
    using RationalPolygon = pgl::Polygon<RationalPoint>;

    const PolygonShape ell({0, 0, 4, 0, 4, 1, 1, 1, 1, 3, 0, 3});
    const RationalPolygon exact(ell);

    // The constructor is what asBitMatrix spells, over either coordinate type.
    CHECK(Matrix(exact) == Matrix(ell));
    CHECK(Matrix(exact) == exact.asBitMatrix());
    CHECK(Matrix(pgl::PolygonWithHoles<RationalPoint>(exact)) == Matrix(Region(ell)));
    CHECK(Matrix(pgl::PolygonSet<RationalPoint>(pgl::PolygonWithHoles<RationalPoint>(exact)))
          == Matrix(pgl::PolygonSet<Point>(Region(ell))));

    const pgl::Rational<int> half(5, 2);
    const RationalPolygon fractional({RationalPoint(0, 0), RationalPoint(half, 0),
                                      RationalPoint(half, 2), RationalPoint(0, 2)});
    CHECK_THROWS_AS(static_cast<void>(Matrix(fractional)), std::logic_error);
}

TEST_CASE("a range of points sets one cell each, over the smallest window") {
    const std::vector<Point> points{Point(2, 3), Point(5, 3), Point(2, 7), Point(2, 3)};
    const Matrix matrix(points);

    // The window is the smallest one holding the points, so it is the bbox.
    CHECK(matrix.origin() == Point(2, 3));
    CHECK(matrix.width() == 4);
    CHECK(matrix.height() == 5);
    CHECK(matrix.window() == matrix.bbox());
    CHECK(matrix.trimmed() == matrix);

    // The repeated point sets the same cell again, which changes nothing.
    CHECK(matrix.count() == 3);
    CHECK(cellsOf(matrix) == CellSet{{2, 3}, {2, 7}, {5, 3}});

    // A braced list of cells works through the default range type.
    CHECK(Matrix({Point(2, 3), Point(5, 3), Point(2, 7)}) == matrix);
    CHECK(Matrix({{2, 3}, {5, 3}, {2, 7}}) == matrix);

    // Any point range does, single-pass ones included.
    CHECK(Matrix(std::set<Point>(points.begin(), points.end())) == matrix);
    CHECK(Matrix(std::list<Point>(points.begin(), points.end())) == matrix);
    CHECK(Matrix(points | std::views::filter([](const Point& p) { return p.x() == 2; }))
          == Matrix({Point(2, 3), Point(2, 7)}));

    // An empty range gives the one empty window every empty window equals.
    CHECK(Matrix(std::vector<Point>{}) == Matrix());
    CHECK(Matrix(std::vector<Point>{Point(-4, 6)}).window() == Rect(Point(-4, 6), Point(-3, 7)));

    // The lattice points are exactly what the constructor reads back.
    Rng rng;
    for (int trial = 0; trial < 200; ++trial) {
        const Matrix source = randomMatrix(rng);
        CHECK(Matrix(source.lattice()) == source.trimmed());
        CHECK(Matrix(source.latticeView()) == source.trimmed());
        CHECK(Matrix(source.lattice()).samePointSet(source));
    }
}

TEST_CASE("a range of points deduces its point type and converts its coordinates") {
    const std::vector<pgl::Point<long>> wide{{1, 1}, {3, 4}};
    const pgl::BitMatrix deduced(wide);
    static_assert(std::is_same_v<decltype(deduced)::PointType, pgl::Point<long>>);
    CHECK(deduced.count() == 2);
    CHECK(pgl::BitMatrix(std::vector<Point>{Point(0, 0)}).window() == Rect(Point(0, 0), Point(1, 1)));

    // Whole coordinates of any type name a cell; the rest are refused, not rounded.
    using RationalPoint = pgl::Point<pgl::Rational<int>>;
    const Matrix matrix({Point(2, 3), Point(5, 3)});
    CHECK(Matrix(std::vector<RationalPoint>{{pgl::Rational<int>(4, 2), 3}, {5, 3}}) == matrix);
    CHECK(Matrix(std::vector<pgl::Point<double>>{{2.0, 3.0}, {5.0, 3.0}}) == matrix);
    CHECK_THROWS_AS(Matrix(std::vector<pgl::Point<double>>{{0.5, 0.0}}), std::logic_error);
    CHECK_THROWS_AS(
        Matrix(std::vector<RationalPoint>{{pgl::Rational<int>(1, 2), 0}}), std::logic_error);
}

TEST_CASE("a shape and a matrix are not point clouds") {
    // Every shape that iterates over its vertices keeps out of the point-range
    // constructor: those vertices are a boundary, not a set of cells. The ones
    // that rasterize keep their own constructor, whatever the argument's
    // constness, and a matrix keeps copying its window along with its cells.
    static_assert(!std::constructible_from<Matrix, const pgl::Convex<Point>&>);
    static_assert(!std::constructible_from<Matrix, pgl::Convex<Point>&>);
    static_assert(!std::constructible_from<Matrix, pgl::Polyline<Point>&>);
    static_assert(!std::constructible_from<Matrix, pgl::MonotoneChain<Point>&>);
    static_assert(!std::constructible_from<Matrix, pgl::Triangle<Point>&>);
    static_assert(!std::constructible_from<Matrix, const pgl::BitMatrix<pgl::Point<long>>&>);
    static_assert(std::constructible_from<Matrix, const std::vector<Point>&>);

    PolygonShape ell({0, 0, 4, 0, 4, 1, 1, 1, 1, 3, 0, 3});
    CHECK(Matrix(ell).count() == 6);
    CHECK(Matrix(std::as_const(ell)) == Matrix(ell));
    // The vertices alone are six cells, the filled polygon is six other ones.
    CHECK(Matrix(std::vector<Point>(ell.begin(), ell.end())) != Matrix(ell));

    Matrix wide(Point(-5, -5), 20, 20);
    wide.set(0, 0);
    CHECK(Matrix(wide).sameWindow(wide));
    CHECK(Matrix(std::as_const(wide)).sameWindow(wide));
    CHECK(!Matrix(wide.lattice()).sameWindow(wide));
}

TEST_CASE("polyomino regions round trip through the raster") {
    for (std::size_t size = 1; size <= 6; ++size) {
        for (const Region& region : pgl::polyominoRegions(size)) {
            const Matrix raster(region);
            CHECK(raster.count() == size);
            CHECK(raster.area() == region.area<int>());
            CHECK(raster.isConnected());
            CHECK(raster.asPolygonWithHoles() == region);
            CHECK(raster.asPolygonSet() == pgl::PolygonSet<Point>(region));
            CHECK(raster.eulerNumber() == 1);
            CHECK(raster.holeCount() == 0);
            CHECK(raster.trimmed() == raster);
        }
    }
}

TEST_CASE("the smallest holed polyomino keeps its hole") {
    // The seven-cell ring around a single missing cell.
    Matrix ring = filledBox(0, 0, 3, 3);
    ring.reset(1, 1);
    CHECK(ring.count() == 8);
    CHECK(ring.componentCount() == 1);
    CHECK(ring.holeCount() == 1);
    CHECK(ring.eulerNumber() == 0);
    CHECK(ring.fillHoles() == filledBox(0, 0, 3, 3));
    CHECK(ring.asPolygonWithHoles().holes().size() == 1);
}

TEST_CASE("inner and outer rasters bracket a shape") {
    const pgl::Triangle<Point> triangle({0, 0}, {10, 0}, {0, 10});
    const Matrix inner = pgl::innerRaster(triangle);
    const Matrix outer = pgl::outerRaster(triangle);

    CHECK(inner.count() == 45);   // cells with i + j <= 8
    CHECK(outer.count() == 64);   // cells with i + j <= 10
    CHECK(outer.contains(inner));
    CHECK(inner.area() <= triangle.area<int>());
    CHECK(outer.area() >= triangle.area<int>());

    // An axis-parallel rectangle is rasterized exactly either way.
    const Rect box(Point(-3, -2), Point(4, 5));
    CHECK(pgl::innerRaster(box) == filledBox(-3, -2, 7, 7));
    CHECK(pgl::outerRaster(box) == filledBox(-3, -2, 7, 7));
}

TEST_CASE("the inner raster of a rectilinear region matches the scanline") {
    for (std::size_t size = 1; size <= 6; ++size) {
        for (const Region& region : pgl::polyominoRegions(size)) {
            CHECK(pgl::innerRaster(region) == Matrix(region));
        }
    }
}

TEST_CASE("an explicit window clips a raster") {
    const pgl::Disk<Point> disk({0, 0}, 5);
    const Matrix quadrant = pgl::outerRaster(disk, Rect(Point(0, 0), Point(5, 5)));
    CHECK(quadrant.window() == Rect(Point(0, 0), Point(5, 5)));
    CHECK(quadrant.count() == 24);   // every cell but the far corner, at distance sqrt(32)
    CHECK(pgl::outerRaster(disk).contains(quadrant));
}

TEST_CASE("the symmetries move the covered region") {
    Matrix matrix(Point(0, 0), 4, 3);
    matrix.set(0, 0);
    matrix.set(3, 2);
    matrix.set(1, 0);

    const Matrix moved = matrix.translated(Point(5, -7));
    CHECK(moved.count() == matrix.count());
    CHECK(moved.get(5, -7));
    CHECK(moved.origin() == Point(5, -7));
    CHECK(moved.translated(Point(-5, 7)) == matrix);
    CHECK((matrix + Point(5, -7)) == moved);

    // A symmetry acts on the squares, so cell c goes to -c - (1,1), not to -c.
    const Matrix reflected = -matrix;
    CHECK(reflected.get(-1, -1));
    CHECK(reflected.get(-4, -3));
    CHECK(!reflected.get(0, 0));
    CHECK(-reflected == matrix);
    CHECK(reflected == matrix.reflectedX().reflectedY());
    CHECK(matrix.reflectedX().get(3, -3));
    CHECK(matrix.reflectedY().get(-4, 2));
    CHECK(matrix.transposed().get(2, 3));
    CHECK(matrix.transposed().transposed() == matrix);
    CHECK(matrix.latticeTransposed() == matrix.transposed());

    CHECK(matrix.rotated90(0) == matrix);
    CHECK(matrix.rotated90(2) == reflected);
    CHECK(matrix.rotated90(4) == matrix);
    CHECK(matrix.rotated90(-1) == matrix.rotated90(3));
    CHECK(matrix.rotated90(1).count() == matrix.count());
    CHECK(Matrix().rotated90(1).emptyWindow());

    Matrix turned = matrix;
    turned.rotate90();
    CHECK(turned == matrix.rotated90());
}

TEST_CASE("the lattice symmetries move the cells as lattice points") {
    Matrix matrix(Point(0, 0), 4, 3);
    matrix.set(0, 0);
    matrix.set(3, 2);
    matrix.set(1, 0);

    const Matrix reflected = matrix.latticeReflected();
    CHECK(reflected.get(0, 0));
    CHECK(reflected.get(-3, -2));
    CHECK(reflected.latticeReflected() == matrix);
    CHECK(reflected == matrix.latticeReflectedX().latticeReflectedY());
    CHECK(matrix.latticeReflectedX().get(3, -2));
    CHECK(matrix.latticeReflectedY().get(-3, 2));
    CHECK(matrix.latticeRotated90(2) == reflected);
    CHECK(matrix.latticeRotated90(4) == matrix);
    CHECK(matrix.latticeRotated90(-1) == matrix.latticeRotated90(3));

    Matrix turned = matrix;
    turned.latticeRotate90();
    CHECK(turned == matrix.latticeRotated90());

    // Each pair differs by exactly one cell in the directions it flips.
    CHECK(matrix.reflected() == reflected.translated(Point(-1, -1)));
    CHECK(matrix.reflectedX() == matrix.latticeReflectedX().translated(Point(0, -1)));
    CHECK(matrix.reflectedY() == matrix.latticeReflectedY().translated(Point(-1, 0)));
    CHECK(matrix.rotated90(1) == matrix.latticeRotated90(1).translated(Point(-1, 0)));
}

TEST_CASE("the symmetries commute with asPolygonSet") {
    using Transform = pgl::Transformation<int>;
    Rng rng;
    for (int trial = 0; trial < 40; ++trial) {
        const Matrix matrix = randomMatrix(rng, 5);
        if (matrix.empty()) {
            continue;
        }
        const pgl::PolygonSet<Point> set = matrix.asPolygonSet();
        CHECK(matrix.reflectedX().asPolygonSet() == Transform::reflectionX() * set);
        CHECK(matrix.reflectedY().asPolygonSet() == Transform::reflectionY() * set);
        CHECK(matrix.reflected().asPolygonSet() == Transform::scaling(-1) * set);
        for (int k = 0; k < 4; ++k) {
            CHECK(matrix.rotated90(k).asPolygonSet() == Transform::rotation90(k) * set);
        }
    }
}

TEST_CASE("the predicate family reads the cells as closed unit squares") {
    const Matrix block = filledBox(0, 0, 5, 5);
    const Matrix core = filledBox(1, 1, 3, 3);
    const Matrix corner = filledBox(0, 0, 1, 1);
    const Matrix neighbor = filledBox(5, 0, 2, 2);       // shares the edge x = 5
    const Matrix touching = filledBox(5, 5, 2, 2);       // shares only the corner (5,5)
    const Matrix away = filledBox(6, 0, 2, 2);

    CHECK(block.contains(core));
    CHECK(block.contains(corner));
    CHECK(!block.contains(neighbor));
    CHECK(block.interiorContains(core));
    CHECK(!block.interiorContains(corner));   // its closure meets the boundary
    CHECK(!block.interiorContains(block));

    CHECK(block.intersects(neighbor));
    CHECK(block.intersects(touching));        // a single shared corner counts
    CHECK(!block.intersects(away));
    CHECK(!block.interiorsIntersect(neighbor));
    CHECK(!block.interiorsIntersect(touching));
    CHECK(block.interiorsIntersect(core));

    CHECK(!block.boundaryContains(corner));
    CHECK(block.boundaryContains(Matrix()));
    CHECK(block.samePointSet(block.resized(Rect(Point(-9, -9), Point(9, 9)))));

    // The empty region is contained by everything and meets nothing.
    CHECK(block.contains(Matrix()));
    CHECK(block.interiorContains(Matrix()));
    CHECK(!block.intersects(Matrix()));
    CHECK(!Matrix().intersects(block));
}

TEST_CASE("the shape spellings of translation all agree") {
    Matrix matrix(Point(0, 0), 4, 3);
    matrix.set(0, 0);
    matrix.set(3, 2);
    const Point vector(5, -7);

    CHECK((matrix + vector) == matrix.translated(vector));
    CHECK((vector + matrix) == matrix.translated(vector));
    CHECK((matrix - vector) == matrix.translated(Point(-5, 7)));
    CHECK((matrix + vector).origin() == Point(5, -7));

    Matrix moved = matrix;
    moved += vector;
    CHECK(moved == matrix.translated(vector));
    CHECK(moved.origin() == Point(5, -7));
    moved -= vector;
    CHECK(moved == matrix);
    CHECK(moved.sameWindow(matrix));

    Matrix turned = matrix;
    turned.rotate90();
    CHECK(turned == matrix.rotated90());
    turned.rotate90(3);
    CHECK(turned == matrix);
}

TEST_CASE("bbox, fbox and pointInside describe the covered region") {
    const Matrix block = filledBox(2, 3, 5, 4);
    CHECK(block.bbox() == Rect(Point(2, 3), Point(7, 7)));
    CHECK(block.fbox() == pgl::Rectangle<pgl::Point<double>>(pgl::Point<double>(2, 3),
                                                             pgl::Point<double>(7, 7)));
    CHECK(block.pointInside() == pgl::Point<pgl::Rational<int>>(pgl::Rational<int>(5, 2),
                                                                pgl::Rational<int>(7, 2)));
    CHECK(block.asPolygonSet().interiorContains(block.pointInside()));

    Matrix scattered(Point(0, 0), 8, 8);
    scattered.set(6, 5);
    scattered.set(7, 7);
    CHECK(scattered.bbox() == Rect(Point(6, 5), Point(8, 8)));
    CHECK(scattered.pointInside() == pgl::Point<pgl::Rational<int>>(pgl::Rational<int>(13, 2),
                                                                    pgl::Rational<int>(11, 2)));

    CHECK_THROWS_AS(static_cast<void>(Matrix().pointInside()), std::logic_error);
}

TEST_CASE("the lattice Minkowski sum adds the cells as lattice points") {
    const Matrix left = filledBox(0, 0, 3, 2), right = filledBox(0, 0, 2, 3);
    const Matrix sum = left.latticeMinkowskiSum(right);
    CHECK(sum == filledBox(0, 0, 4, 4));
    CHECK(sum.window() == sum.bbox());
    CHECK(right.latticeMinkowskiSum(left) == sum);

    // A single cell is the identity, and translates when it is not at the origin.
    Matrix unit(Point(0, 0), 1, 1);
    unit.set(0, 0);
    CHECK(left.latticeMinkowskiSum(unit) == left);
    CHECK(left.latticeMinkowskiSum(unit.translated(Point(4, 5))) == left.translated(Point(4, 5)));

    CHECK(left.latticeMinkowskiSum(Matrix()).empty());
    CHECK(Matrix().latticeMinkowskiSum(left).empty());
}

TEST_CASE("the region Minkowski sum is the one the shapes compute") {
    const Matrix left = filledBox(0, 0, 3, 2), right = filledBox(0, 0, 2, 3);

    // [0,3]x[0,2] (+) [0,2]x[0,3] = [0,5]x[0,5].
    CHECK(left.minkowskiSum(right) == filledBox(0, 0, 5, 5));
    CHECK((left + right) == left.minkowskiSum(right));
    CHECK(right.minkowskiSum(left) == left.minkowskiSum(right));

    // One cell more in each direction than the lattice sum: the unit square is
    // not the identity, since U (+) U is the two-by-two square.
    CHECK(left.minkowskiSum(right) ==
          left.latticeMinkowskiSum(right).latticeMinkowskiSum(filledBox(0, 0, 2, 2)));

    Matrix unit(Point(0, 0), 1, 1);
    unit.set(0, 0);
    CHECK(unit.minkowskiSum(unit) == filledBox(0, 0, 2, 2));
    CHECK(unit.latticeMinkowskiSum(unit) == unit);

    CHECK(left.minkowskiSum(Matrix()).empty());
    CHECK(Matrix().minkowskiSum(left).empty());
}

TEST_CASE("the region Minkowski sum commutes with asPolygonSet") {
    using ExactSet = pgl::PolygonSet<pgl::EPoint>;
    Rng rng;
    for (int trial = 0; trial < 25; ++trial) {
        const Matrix left = randomMatrix(rng, 4), right = randomMatrix(rng, 3);
        if (left.empty() || right.empty()) {
            continue;
        }
        const ExactSet viaCells(left.minkowskiSum(right).asPolygonSet());
        const ExactSet viaShapes = left.asPolygonSet().minkowskiSum(right.asPolygonSet());
        CHECK(viaCells == viaShapes);

        // The lattice sum is the one that does not commute: it is smaller.
        const ExactSet lattice(left.latticeMinkowskiSum(right).asPolygonSet());
        CHECK(lattice != viaShapes);
        CHECK(left.minkowskiSum(right).contains(left.latticeMinkowskiSum(right)));
    }
}

TEST_CASE("the region Minkowski erosion is the regularized one") {
    Matrix unit(Point(0, 0), 1, 1);
    unit.set(0, 0);

    // [0,3]^2 (-) [0,1]^2 = [0,2]^2, and one cell eroded by one cell is the
    // single point p = 0, which regularizes away.
    CHECK(filledBox(0, 0, 3, 3).minkowskiErosion(unit) == filledBox(0, 0, 2, 2));
    CHECK(unit.minkowskiErosion(unit).empty());
    CHECK(filledBox(0, 0, 2, 1).minkowskiErosion(unit).empty());   // a segment, not area
    CHECK(unit.latticeMinkowskiErosion(unit) == unit);             // the lattice one keeps it

    // One lattice erosion by the operand dilated with the two-by-two block.
    const Matrix block = filledBox(1, 1, 6, 5), stamp = filledBox(0, 0, 2, 3);
    CHECK(block.minkowskiErosion(stamp) ==
          block.latticeMinkowskiErosion(stamp.latticeMinkowskiSum(filledBox(0, 0, 2, 2))));
    CHECK(block.contains(block.minkowskiErosion(stamp)));

    // Eroding by nothing is the whole plane, which the window stands in for.
    CHECK(block.minkowskiErosion(Matrix()).count() == block.count());
}

TEST_CASE("the region Minkowski erosion commutes with asPolygonSet") {
    using ExactSet = pgl::PolygonSet<pgl::EPoint>;
    Rng rng;
    int nonEmpty = 0;
    for (int trial = 0; trial < 25; ++trial) {
        Matrix left = randomMatrix(rng, 6), right = randomMatrix(rng, 3);
        left.makeHvConvex();   // keep enough area that the erosion is often nonempty
        if (left.empty() || right.empty()) {
            continue;
        }
        const ExactSet viaCells(left.minkowskiErosion(right).asPolygonSet());
        const ExactSet viaShapes = left.asPolygonSet().minkowskiErosion(right.asPolygonSet());
        CHECK(viaCells == viaShapes);
        if (!left.minkowskiErosion(right).empty()) {
            ++nonEmpty;
        }
    }
    CHECK(nonEmpty > 0);   // the agreement above is not vacuous
}

TEST_CASE("the lattice Minkowski sum agrees with a brute-force reference") {
    Rng rng;
    for (int trial = 0; trial < 120; ++trial) {
        const Matrix left = randomMatrix(rng, 4), right = randomMatrix(rng, 3);
        CellSet expected;
        for (const auto& [ax, ay] : cellsOf(left)) {
            for (const auto& [bx, by] : cellsOf(right)) {
                expected.emplace(ax + bx, ay + by);
            }
        }
        CHECK(cellsOf(left.latticeMinkowskiSum(right)) == expected);
        CHECK(cellsOf(right.latticeMinkowskiSum(left)) == expected);
    }
}

TEST_CASE("the erosion is the dual of the sum within the window") {
    const Matrix stamp = filledBox(0, 0, 2, 3);
    const Matrix block = filledBox(0, 0, 3, 2);
    const Matrix sum = block.latticeMinkowskiSum(stamp);
    CHECK(sum.latticeMinkowskiErosion(stamp) == block);

    // Eroding by a stamp that cannot fit anywhere leaves nothing.
    CHECK(block.latticeMinkowskiErosion(stamp).empty());

    // Eroding by nothing is vacuously true over the whole window.
    const Matrix all = block.latticeMinkowskiErosion(Matrix());
    CHECK(all.sameWindow(block));
    CHECK(all.count() == 6);

    // The classical identity, with the complement taken over the same window.
    Rng rng;
    for (int trial = 0; trial < 60; ++trial) {
        const Matrix matrix = randomMatrix(rng, 4);
        const Matrix element = randomMatrix(rng, 2);
        if (element.empty()) {
            continue;
        }
        const Matrix eroded = matrix.latticeMinkowskiErosion(element);
        const Matrix dual = ~((~matrix).latticeMinkowskiSum(element.latticeReflected()).resized(eroded.window()));
        CHECK(eroded == dual);
    }
}

TEST_CASE("opening and closing bracket the original") {
    Rng rng;
    for (int trial = 0; trial < 60; ++trial) {
        const Matrix matrix = randomMatrix(rng, 4);
        const Matrix element = randomMatrix(rng, 2);
        if (element.empty() || matrix.empty()) {
            continue;
        }
        CHECK(matrix.contains(matrix.latticeOpening(element)));
        CHECK(matrix.latticeClosing(element).contains(matrix));
        // Both are idempotent.
        CHECK(matrix.latticeOpening(element).latticeOpening(element) == matrix.latticeOpening(element));
    }
}

TEST_CASE("interior and boundary split the cells") {
    const Matrix block = filledBox(0, 0, 5, 5);
    CHECK(block.interior().count() == 9);
    CHECK(block.boundary().count() == 16);
    CHECK((block.interior() | block.boundary()) == block);
    CHECK(!block.interior().interiorsIntersect(block.boundary()));

    // The window edge counts as outside, so a full window is all boundary.
    CHECK(block.interior(GridAdjacency::vertex).count() == 9);
    Matrix cross(Point(0, 0), 3, 3);
    cross.set(1, 1);
    cross.set(0, 1);
    cross.set(2, 1);
    cross.set(1, 0);
    cross.set(1, 2);
    CHECK(cross.interior().count() == 1);
    CHECK(cross.interior(GridAdjacency::vertex).empty());
}

TEST_CASE("components split on the chosen adjacency") {
    Matrix matrix(Point(0, 0), 4, 4);
    matrix.set(0, 0);
    matrix.set(1, 1);   // touches (0,0) only at a corner
    matrix.set(3, 3);

    CHECK(matrix.componentCount() == 3);
    CHECK(matrix.componentCount(GridAdjacency::vertex) == 2);
    CHECK(!matrix.isConnected());
    CHECK(matrix.connectedComponents().size() == 3);
    CHECK(matrix.connectedComponents(GridAdjacency::vertex).size() == 2);

    std::size_t total = 0;
    Matrix rebuilt(matrix.origin(), matrix.width(), matrix.height());
    for (const Matrix& component : matrix.connectedComponents()) {
        CHECK(component.window() == component.bbox());
        CHECK(component.isConnected());
        total += component.count();
        rebuilt |= component;
    }
    CHECK(total == matrix.count());
    CHECK(rebuilt == matrix);

    CHECK(Matrix().componentCount() == 0);
    CHECK(!Matrix().isConnected());
}

TEST_CASE("a set of regions comes back from a disconnected matrix") {
    Matrix matrix(Point(0, 0), 10, 3);
    matrix.set(0, 0);
    matrix.set(1, 0);
    matrix.set(5, 2);

    const pgl::PolygonSet<Point> set = matrix.asPolygonSet();
    CHECK(set.componentCount() == 2);
    CHECK(set.area<int>() == 3);
    CHECK(set.bbox() == matrix.bbox());
    CHECK(Matrix().asPolygonSet().componentCount() == 0);
    CHECK(Matrix().asPolygonWithHoles() == Region());

    // A single region cannot describe two components, so asking for one fails
    // rather than quietly dropping a component.
    CHECK_THROWS_AS(static_cast<void>(matrix.asPolygonWithHoles()), std::logic_error);
}

TEST_CASE("a canvas draws the covered region as one element") {
    Matrix matrix(Point(0, 0), 10, 3);
    matrix.set(0, 0);
    matrix.set(1, 0);
    matrix.set(5, 2);

    pgl::Canvas canvas;
    canvas << matrix;
    const std::string svg = canvas.toSVG();

    const auto count = [](const std::string& text, const std::string& needle) {
        std::size_t total = 0;
        for (std::size_t at = text.find(needle); at != std::string::npos;
             at = text.find(needle, at + 1)) {
            ++total;
        }
        return total;
    };

    // The matrix draws exactly as its polygon set does: one <path> for the
    // whole matrix, one subpath per ring.
    CHECK(count(svg, "<path") == 1);
    CHECK(count(svg, "M ") == 2);

    pgl::Canvas reference;
    reference << matrix.asPolygonSet();
    CHECK(svg == reference.toSVG());

    // A matrix with a hole keeps it: two rings for the one component.
    Matrix ring = filledBox(0, 0, 3, 3);
    ring.reset(1, 1);
    pgl::Canvas holed;
    holed << ring;
    CHECK(count(holed.toSVG(), "M ") == 2);
}

TEST_CASE("the Euler number counts components minus holes") {
    // Two separate rings: two components, two holes.
    Matrix rings = filledBox(0, 0, 3, 3) | filledBox(5, 0, 3, 3);
    rings.reset(1, 1);
    rings.reset(6, 1);
    CHECK(rings.componentCount() == 2);
    CHECK(rings.eulerNumber() == 0);
    CHECK(rings.holeCount() == 2);

    // Under 8-adjacency a diagonal pair is one component, not two.
    Matrix diagonal(Point(0, 0), 2, 2);
    diagonal.set(0, 0);
    diagonal.set(1, 1);
    CHECK(diagonal.eulerNumber(GridAdjacency::edge) == 2);
    CHECK(diagonal.eulerNumber(GridAdjacency::vertex) == 1);
    CHECK(diagonal.componentCount(GridAdjacency::edge) == 2);
    CHECK(diagonal.componentCount(GridAdjacency::vertex) == 1);
    CHECK(diagonal.holeCount(GridAdjacency::edge) == 0);
    CHECK(diagonal.holeCount(GridAdjacency::vertex) == 0);
    CHECK(Matrix().eulerNumber() == 0);
}

TEST_CASE("the Euler number agrees with flood filling") {
    Rng rng;
    for (int trial = 0; trial < 200; ++trial) {
        const Matrix matrix = randomMatrix(rng, 5);
        for (const GridAdjacency adjacency : {GridAdjacency::edge, GridAdjacency::vertex}) {
            // Holes are the background components that cannot reach the outside,
            // which is what filling them and counting the difference finds.
            const Matrix filled = matrix.fillHoles(adjacency);
            const std::size_t holes =
                filled.difference(matrix).componentCount(adjacency == GridAdjacency::edge
                                                             ? GridAdjacency::vertex
                                                             : GridAdjacency::edge);
            CHECK(matrix.eulerNumber(adjacency) ==
                  static_cast<std::int64_t>(matrix.componentCount(adjacency)) -
                      static_cast<std::int64_t>(holes));
            CHECK(matrix.holeCount(adjacency) == holes);
        }
    }
}

TEST_CASE("filling holes adds only enclosed background") {
    Rng rng;
    for (int trial = 0; trial < 100; ++trial) {
        const Matrix matrix = randomMatrix(rng, 5);
        const Matrix filled = matrix.fillHoles();
        CHECK(filled.contains(matrix));
        CHECK(filled.sameWindow(matrix));
        CHECK(filled.holeCount() == 0);
        CHECK(filled.componentCount() == matrix.componentCount());
        CHECK(filled.bbox() == matrix.bbox());
    }
}

TEST_CASE("hv-convexity is the fixed point of the row and column fills") {
    Matrix corners(Point(0, 0), 5, 5);
    corners.set(0, 0);
    corners.set(4, 0);
    corners.set(0, 4);
    corners.set(4, 4);
    CHECK(!corners.isHvConvex());
    CHECK(corners.makeHvConvex() == 21);
    CHECK(corners.isHvConvex());
    CHECK(corners == filledBox(0, 0, 5, 5));
    CHECK(corners.makeHvConvex() == 0);

    Matrix gap(Point(0, 0), 5, 1);
    gap.set(0, 0);
    gap.set(4, 0);
    CHECK(!gap.isRowConvex());
    CHECK(gap.isColumnConvex());
    CHECK(gap.fillRows());
    CHECK(gap.count() == 5);
    CHECK(!gap.fillRows());

    // An L is hv-convex without being convex.
    Matrix ell = filledBox(0, 0, 4, 1) | filledBox(0, 0, 1, 3);
    CHECK(ell.isHvConvex());
    CHECK(filledBox(0, 0, 6, 6).isHvConvex());
    CHECK(Matrix().isHvConvex());
}

TEST_CASE("hv-convexity holds after makeHvConvex on random input") {
    Rng rng;
    for (int trial = 0; trial < 100; ++trial) {
        Matrix matrix = randomMatrix(rng, 5);
        const std::size_t before = matrix.count();
        const std::size_t added = matrix.makeHvConvex();
        CHECK(matrix.count() == before + added);
        CHECK(matrix.isHvConvex());
        CHECK(matrix.isRowConvex());
        CHECK(matrix.isColumnConvex());
        CHECK(matrix.bbox() == matrix.bbox());
    }
}

TEST_CASE("the convex hull encloses the covered cells") {
    const PolygonShape ell({0, 0, 4, 0, 4, 1, 1, 1, 1, 3, 0, 3});
    const Matrix raster{Region(ell)};
    const pgl::Convex<Point> hull = raster.convexHull();
    CHECK(hull.area<int>() > raster.area());
    for (const Rect& run : raster.rectangles()) {
        CHECK(hull.contains(run));
    }

    const Matrix block = filledBox(1, 2, 3, 4);
    CHECK(block.convexHull().area<int>() == 12);
    CHECK(block.convexHull().bbox() == block.bbox());
}

TEST_CASE("set algebra agrees with the regularized boolean operations") {
    using ExactSet = pgl::PolygonSet<pgl::EPoint>;
    const Region first(PolygonShape({0, 0, 6, 0, 6, 4, 0, 4}));
    const Region second(PolygonShape({3, 2, 9, 2, 9, 7, 3, 7}));
    const Matrix left{first}, right{second};

    CHECK(ExactSet((left | right).asPolygonSet()) == first.regularizedUnion(second));
    CHECK(ExactSet((left & right).asPolygonSet()) == first.regularizedIntersection(second));
    CHECK(ExactSet(left.difference(right).asPolygonSet()) == first.difference(second));
    CHECK(ExactSet((left ^ right).asPolygonSet()) == first.symmetricDifference(second));
}

TEST_CASE("a wide matrix keeps its cells across many words") {
    Matrix matrix(Point(-1000, -5), 2000, 11);
    for (int x = -1000; x < 1000; x += 7) {
        matrix.set(x, x % 11 < 0 ? x % 11 + 11 - 5 : x % 11 - 5);
    }
    const std::size_t written = matrix.count();
    CHECK(written == 286);
    CHECK(matrix.trimmed().samePointSet(matrix));
    CHECK(matrix.trimmed().window() == matrix.bbox());
    CHECK((~matrix).count() == 2000 * 11 - written);
    CHECK(matrix.translated(Point(3, 0)).count() == written);
    CHECK(matrix.rotated90(1).count() == written);
    CHECK(matrix.rotated90(1).rotated90(3) == matrix);
    CHECK(matrix.andCount(matrix) == written);
    CHECK(matrix.andCount(matrix.translated(Point(1, 0))) == 0);
}

TEST_CASE("a matrix over wider coordinates behaves the same") {
    using WidePoint = pgl::Point<long long>;
    using WideMatrix = pgl::BitMatrix<WidePoint>;

    const long long far_away = 4000000000LL;
    WideMatrix matrix(WidePoint(far_away, -far_away), 5, 5);
    matrix.set(far_away, -far_away);
    matrix.set(far_away + 4, -far_away + 4);
    CHECK(matrix.count() == 2);
    CHECK(matrix.get(far_away, -far_away));
    CHECK(matrix.bbox() ==
          pgl::Rectangle<WidePoint>(WidePoint(far_away, -far_away),
                                    WidePoint(far_away + 5, -far_away + 5)));
    CHECK(matrix.componentCount() == 2);
    CHECK(matrix.componentCount(GridAdjacency::vertex) == 2);
    CHECK(matrix.asPolygonSet().componentCount() == 2);
    CHECK(matrix.asPolygonSet().area() == 2);
    CHECK(matrix.trimmed().origin() == WidePoint(far_away, -far_away));
}

TEST_CASE("set over a range of points sets one cell each") {
    Matrix m(Rect(Point(0, 0), Point(4, 4)));

    SUBCASE("a braced list of cells") {
        m.set({Point(0, 0), Point(2, 3), Point(2, 3)});
        CHECK(m.count() == 2);
        CHECK(m.get(Point(0, 0)));
        CHECK(m.get(Point(2, 3)));
    }

    SUBCASE("a container, with cells outside the window dropped") {
        const std::vector<Point> cells = {Point(1, 1), Point(9, 9), Point(-1, 2)};
        m.set(cells);
        CHECK(m.count() == 1);
        CHECK(m.get(Point(1, 1)));
    }

    SUBCASE("a single-pass view") {
        const std::vector<Point> cells = {Point(0, 1), Point(3, 3)};
        m.set(cells | std::views::filter([](const Point& p) { return p.x() > 0; }));
        CHECK(m.count() == 1);
        CHECK(m.get(Point(3, 3)));
    }

    SUBCASE("points over another coordinate type") {
        const std::vector<pgl::Point<double>> cells = {{1.0, 2.0}, {3.0, 0.0}};
        m.set(cells);
        CHECK(m.count() == 2);
        CHECK(m.get(Point(1, 2)));
        CHECK(m.get(Point(3, 0)));
        const std::vector<pgl::Point<double>> fractional = {{1.5, 2.0}};
        CHECK_THROWS_AS(m.set(fractional), std::logic_error);
    }

    SUBCASE("the single-cell overloads still resolve") {
        m.set(Point(2, 2));
        m.set(1, 1);
        m.set(0, 0, false);
        CHECK(m.count() == 2);
    }

    SUBCASE("an empty range changes nothing") {
        m.set(std::vector<Point>{});
        CHECK(m.empty());
    }
}

TEST_CASE("reset over a range of points clears one cell each") {
    Matrix m(Rect(Point(0, 0), Point(4, 4)));
    m.setAll();

    SUBCASE("a braced list of cells, repetition included") {
        m.reset({Point(0, 0), Point(2, 3), Point(2, 3)});
        CHECK(m.count() == 14);
        CHECK_FALSE(m.get(Point(0, 0)));
        CHECK_FALSE(m.get(Point(2, 3)));
    }

    SUBCASE("a container, with cells outside the window dropped") {
        const std::vector<Point> cells = {Point(1, 1), Point(9, 9), Point(-1, 2)};
        m.reset(cells);
        CHECK(m.count() == 15);
        CHECK_FALSE(m.get(Point(1, 1)));
    }

    SUBCASE("a single-pass view") {
        const std::vector<Point> cells = {Point(0, 1), Point(3, 3)};
        m.reset(cells | std::views::filter([](const Point& p) { return p.x() > 0; }));
        CHECK(m.count() == 15);
        CHECK_FALSE(m.get(Point(3, 3)));
    }

    SUBCASE("points over another coordinate type") {
        const std::vector<pgl::Point<double>> cells = {{1.0, 2.0}, {3.0, 0.0}};
        m.reset(cells);
        CHECK(m.count() == 14);
        CHECK_FALSE(m.get(Point(1, 2)));
        const std::vector<pgl::Point<double>> fractional = {{1.5, 2.0}};
        CHECK_THROWS_AS(m.reset(fractional), std::logic_error);
    }

    SUBCASE("the single-cell overloads still resolve") {
        m.reset(Point(2, 2));
        m.reset(1, 1);
        CHECK(m.count() == 14);
    }

    SUBCASE("an empty range changes nothing") {
        m.reset(std::vector<Point>{});
        CHECK(m.count() == 16);
    }
}

TEST_CASE("flip over a range of points flips one cell each") {
    Matrix m(Rect(Point(0, 0), Point(4, 4)));

    SUBCASE("a braced list of cells") {
        m.set(Point(0, 0));
        m.flip({Point(0, 0), Point(2, 3)});
        CHECK(m.count() == 1);
        CHECK_FALSE(m.get(Point(0, 0)));
        CHECK(m.get(Point(2, 3)));
    }

    SUBCASE("a repeated point cancels, unlike set and reset") {
        m.flip({Point(2, 3), Point(2, 3)});
        CHECK(m.empty());
        m.flip({Point(2, 3), Point(2, 3), Point(2, 3)});
        CHECK(m.count() == 1);
        CHECK(m.get(Point(2, 3)));
    }

    SUBCASE("a container, with cells outside the window dropped") {
        const std::vector<Point> cells = {Point(1, 1), Point(9, 9), Point(-1, 2)};
        m.flip(cells);
        CHECK(m.count() == 1);
        CHECK(m.get(Point(1, 1)));
    }

    SUBCASE("a single-pass view") {
        const std::vector<Point> cells = {Point(0, 1), Point(3, 3)};
        m.flip(cells | std::views::filter([](const Point& p) { return p.x() > 0; }));
        CHECK(m.count() == 1);
        CHECK(m.get(Point(3, 3)));
    }

    SUBCASE("points over another coordinate type") {
        const std::vector<pgl::Point<double>> cells = {{1.0, 2.0}, {3.0, 0.0}};
        m.flip(cells);
        CHECK(m.count() == 2);
        CHECK(m.get(Point(1, 2)));
        CHECK(m.get(Point(3, 0)));
        const std::vector<pgl::Point<double>> fractional = {{1.5, 2.0}};
        CHECK_THROWS_AS(m.flip(fractional), std::logic_error);
    }

    SUBCASE("the single-cell overloads still resolve") {
        m.flip(Point(2, 2));
        m.flip(1, 1);
        CHECK(m.count() == 2);
    }

    SUBCASE("an empty range changes nothing") {
        m.flip(std::vector<Point>{});
        CHECK(m.empty());
    }
}

TEST_CASE("BitMatrix hashes agree with equality and the centroid sums whole words") {
    using Point = pgl::Point<int>;
    using Matrix = pgl::BitMatrix<Point>;
    Matrix a(Point(-3, 2), 80, 5), b(Point(-3, 2), 80, 5);   // wider than the cells, so trimming moves the window
    a.set(Point(-3, 2));
    a.set(Point(60, 4));
    a.set(Point(66, 3));
    b.set(Point(66, 3));
    b.set(Point(60, 4));
    b.set(Point(-3, 2));
    CHECK(a == b);
    CHECK(std::hash<Matrix>{}(a) == std::hash<Matrix>{}(b));
    const std::unordered_set<Matrix> values{a, b, a.trimmed()};
    CHECK(values.size() == 2);   // a and b are one value; the trimmed copy has another window
    // Three cells across two words of a row and three rows: the mean of the
    // cell centres.
    CHECK(a.centroid() == pgl::Point<pgl::ERational>(pgl::ERational(83, 2), pgl::ERational(7, 2)));
    CHECK(a.centroid() == a.trimmed().centroid());
}
