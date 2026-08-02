// A gallery of every bounded shape, laid out on a grid.
//
// Each shape is built inside the box [0,8]x[0,8] and then translated into its
// own 10x10 cell of a 4x3 grid, so every cell keeps a one-unit gutter around
// its shape. Translation is just "shape += point", which every shape supports,
// including HalfplaneIntersection (its half-planes move with the region).
//
// The unbounded shapes (Line, OrientedLine, Ray, Halfplane) are left out, and
// so is EmptyShape, which draws nothing. Point is bounded but has no extent, so
// its cell holds a single dot at the center rather than an 8x8 figure.
//
// Hover over a shape in a browser to see its textual form: the canvas stores it
// as the SVG <title> of the element.
//
// Output: canvas_gallery.svg, .pdf, and .ipe

#include <vector>
#include "pgl.hpp"

using Point = pgl::Point<int>;

constexpr int cellSize = 10;   // side of a grid cell
constexpr int shapeSize = 8;   // side of the box every shape is built in
constexpr int gutter = (cellSize - shapeSize) / 2;
constexpr int columns = 4;
constexpr int rows = 3;

// Returns the shape translated from the box [0,8]x[0,8] into the cell at the
// given column and row. Row 0 is the top row: the canvas y-axis points up, so
// rows are laid out downwards to match reading order.
template <class Shape>
Shape atCell(Shape shape, int column, int row) {
    return shape + Point(cellSize * column + gutter, cellSize * (rows - 1 - row) + gutter);
}

int main() {
    // The five half-planes of a unit square with its top-right corner cut off,
    // each one traversed counterclockwise so that the region is on its left.
    const std::vector<pgl::Halfplane<Point>> halfplanes = {
        {0, 0, 8, 0},   // y >= 0
        {8, 0, 8, 8},   // x <= 8
        {8, 8, 0, 8},   // y <= 8
        {0, 8, 0, 0},   // x >= 0
        {7, 3, 5, 7},   // a corner cut
    };

    const pgl::Point<int> point(4, 4);
    const pgl::Segment<Point> segment(0, 0, 8, 8);
    const pgl::OrientedSegment<Point> orientedSegment(0, 8, 8, 0);
    const pgl::MonotoneChain<Point> chain({0, 0, 2, 6, 4, 1, 6, 8, 8, 3});
    const pgl::Polyline<Point> polyline({0, 0, 8, 8, 0, 8, 8, 0});   // self-crossing
    const pgl::Triangle<Point> triangle(Point(0, 0), Point(8, 0), Point(4, 8));
    const pgl::Rectangle<Point> rectangle(Point(0, 0), Point(8, 8));
    const pgl::Disk<Point> disk(Point(4, 4), 4);
    const pgl::Convex<Point> convex({0, 3, 0, 5, 3, 8, 5, 8, 8, 5, 8, 3, 5, 0, 3, 0});
    const pgl::Polygon<Point> polygon({0, 0, 8, 0, 8, 8, 4, 4, 0, 8});   // non-convex
    const pgl::PolygonWithHoles<Point> region(pgl::Polygon<Point>({0, 0, 8, 0, 7,5, 8, 8, 0, 8}),
                                              std::vector{pgl::Polygon<Point>({2, 2, 4, 2, 6, 6})});
    const pgl::HalfplaneIntersection<Point> halfplaneIntersection(halfplanes);

    const std::vector<std::string> colors = {
        "crimson", "darkorange", "goldenrod", "seagreen",
        "teal", "steelblue", "royalblue", "slateblue",
        "purple", "magenta", "sienna", "olivedrab",
    };

    pgl::Canvas canvas;
    canvas.size(800, 600);
    canvas << pgl::fillOpacity("25%") << pgl::strokeWidth("2px");

    // Draws one cell of the gallery, in its own color, at the next free slot.
    int cell = 0;
    auto draw = [&](const auto& shape) {
        const std::string& color = colors[cell % colors.size()];
        const std::string& fillColor = colors[(cell + 5) % colors.size()];
        canvas << pgl::stroke(color)
               << pgl::fill(fillColor)
               << atCell(shape, cell % columns, cell / columns);
        ++cell;
    };

    draw(point);
    draw(segment);
    draw(orientedSegment);
    draw(chain);
    draw(polyline);
    draw(triangle);
    draw(rectangle);
    draw(disk);
    draw(convex);
    draw(polygon);
    draw(region);
    draw(halfplaneIntersection);

    canvas.writeSVG("canvas_gallery.svg");
    canvas.writePDF("canvas_gallery.pdf");
    canvas.writeIPE("canvas_gallery.ipe");

    return 0;
}
