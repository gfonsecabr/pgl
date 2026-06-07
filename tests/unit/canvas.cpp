#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <fstream>
#include <string>

#include "pgl.hpp"

TEST_CASE("Canvas stores SVG attributes with each inserted shape and writes an SVG file") {
    const std::string path = "build/tests/output/canvas_test.svg";

    pgl::Canvas canvas;
    canvas.borders(true);
    canvas.pointRadius(5.0);
    canvas.strokeWidth(5.0);
    const pgl::Point<int> first_point(10, 20);
    const pgl::Point<int> second_point(30, 70);
    const pgl::Segment<pgl::Point<int>> first_segment(first_point, second_point);
    const pgl::Point<double> translation(35.0, 15.0);
    const pgl::Segment<pgl::Point<double, std::string>> second_segment(
        pgl::Point<double, std::string>(45.0, 35.0, "left"),
        pgl::Point<double, std::string>(65.0, 85.0, "right")
    );
    const pgl::OrientedSegment<pgl::Point<double, std::string>> third_segment(
        second_segment.max() + translation,
        second_segment.min() + translation
    );
    const pgl::Line<pgl::Point<int>> line(
        pgl::Point<int>(0, 100),
        pgl::Point<int>(100, 0)
    );

    canvas << pgl::stroke("blue")
           << pgl::fill("blue")
           << first_segment
           << first_segment.min()
           << first_segment.max()
           << pgl::stroke("black")
           << pgl::fill("black")
           << second_segment
           << pgl::stroke("red")
           << line
           << pgl::stroke("black")
           << third_segment
           << second_segment.min()
           << second_segment.max();

    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());

    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(svg.find("<svg") != std::string::npos);
    CHECK(svg.find("width=\"1000\" height=\"1000\"") != std::string::npos);
    CHECK(svg.find("viewBox=\"0 0 1000 1000\"") != std::string::npos);
    CHECK(svg.find("<circle") != std::string::npos);
    CHECK(svg.find("<line") != std::string::npos);
    CHECK(svg.find("<path") != std::string::npos);
    CHECK(svg.find("<rect x=\"0.5\" y=\"0.5\" width=\"999\" height=\"999\" stroke=\"black\" fill=\"none\" stroke-width=\"1\"/>") != std::string::npos);
    CHECK(svg.find("<marker id=\"pgl-arrowhead\"") != std::string::npos);
    CHECK(svg.find("<title>(10,20)--(30,70)</title>") != std::string::npos);
    CHECK(svg.find("<title>(10,20)</title>") != std::string::npos);
    CHECK(svg.find("<title>left:(45,35)--right:(65,85)</title>") != std::string::npos);
    CHECK(svg.find("<title>-(0,100)--(100,0)-</title>") != std::string::npos);
    CHECK(svg.find("<title>right:(100,100)-&gt;left:(80,50)</title>") != std::string::npos);
    CHECK(svg.find("marker-mid=\"url(#pgl-arrowhead)\"") != std::string::npos);
    CHECK(svg.find("x1=\"132\" y1=\"776\" x2=\"316\" y2=\"316\"") != std::string::npos);
    CHECK(svg.find("cx=\"132\" cy=\"776\" r=\"5\"") != std::string::npos);
    CHECK(svg.find("x1=\"0\" y1=\"6.39488e-14\" x2=\"1000\" y2=\"1000\"") != std::string::npos);
    CHECK(svg.find("d=\"M 960 40 L 868 270 L 776 500\"") != std::string::npos);
    CHECK(svg.find("stroke=\"blue\"") != std::string::npos);
    CHECK(svg.find("fill=\"blue\"") != std::string::npos);
    CHECK(svg.find("stroke=\"black\"") != std::string::npos);
    CHECK(svg.find("fill=\"black\"") != std::string::npos);
    CHECK(svg.find("stroke-width=\"5\"") != std::string::npos);
    CHECK(svg.find("vector-effect=\"non-scaling-stroke\"") != std::string::npos);
}

TEST_CASE("Canvas draws all implemented primitive shapes into SVG") {
    const std::string path = "build/tests/output/primitives_canvas.svg";

    pgl::Canvas canvas;
    canvas.size(900.0, 700.0).margin(30.0).pointRadius(4.0).strokeWidth(4.0).borders(true);

    const pgl::Point<int> point(10, 20);
    const pgl::Segment<pgl::Point<int>> segment({0, 0}, {40, 30});
    const pgl::OrientedSegment<pgl::Point<int>> oriented_segment({60, 10}, {110, 40});
    const pgl::Line<pgl::Point<int>> line({-20, 80}, {80, -20});
    const pgl::OrientedLine<pgl::Point<int>> oriented_line({120, 0}, {180, 60});
    const pgl::Ray<pgl::Point<int>> ray({20, 100}, {70, 130});
    const pgl::Halfplane<pgl::Point<int>> halfplane({140, 110}, {200, 110});
    const pgl::Rectangle<pgl::Point<int>> rectangle({150, 10}, {220, 70});
    const pgl::Triangle<pgl::Point<int>> triangle({20, 140}, {90, 150}, {50, 210});
    const pgl::Convex<pgl::Point<int>> convex(std::vector<pgl::Point<int>>{{230, 130}, {290, 130}, {290, 190}, {230, 190}});

    canvas << pgl::stroke("black")
           << pgl::fill("black")
           << point
           << pgl::stroke("royalblue")
           << pgl::fill("none")
           << segment
           << pgl::stroke("crimson")
           << oriented_segment
           << pgl::stroke("darkgreen")
           << line
           << pgl::stroke("orange")
           << oriented_line
           << pgl::stroke("purple")
           << ray
           << pgl::stroke("teal")
           << pgl::fill("teal")
           << pgl::fillOpacity("0.18")
           << halfplane
           << pgl::stroke("sienna")
           << pgl::fill("gold")
           << pgl::fillOpacity("0.22")
           << rectangle
           << pgl::stroke("navy")
           << pgl::fill("skyblue")
           << pgl::fillOpacity("0.28")
           << triangle
           << pgl::stroke("darkmagenta")
           << pgl::fill("plum")
           << pgl::fillOpacity("0.30")
           << convex;

    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());

    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(svg.find("<svg") != std::string::npos);
    CHECK(svg.find("<circle") != std::string::npos);
    CHECK(svg.find("<line") != std::string::npos);
    CHECK(svg.find("<path") != std::string::npos);
    CHECK(svg.find("<polygon") != std::string::npos);
    CHECK(svg.find("<rect") != std::string::npos);
    CHECK(svg.find("<marker id=\"pgl-arrowhead\"") != std::string::npos);

    CHECK(svg.find("<title>(10,20)</title>") != std::string::npos);
    CHECK(svg.find("<title>(0,0)--(40,30)</title>") != std::string::npos);
    CHECK(svg.find("<title>(60,10)-&gt;(110,40)</title>") != std::string::npos);
    CHECK(svg.find("<title>-(-20,80)--(80,-20)-</title>") != std::string::npos);
    CHECK(svg.find("<title>-(120,0)--(180,60)-&gt;</title>") != std::string::npos);
    CHECK(svg.find("<title>(20,100)--(70,130)-&gt;</title>") != std::string::npos);
    CHECK(svg.find("<title>^-(140,110)--(200,110)-^</title>") != std::string::npos);
    CHECK(svg.find("<title>[(150,10),(220,70)]</title>") != std::string::npos);
    CHECK(svg.find("<title>&lt;(20,140)(90,150)(50,210)&gt;</title>") != std::string::npos);
    CHECK(svg.find("<title>Convex[(230,130),(290,130),(290,190),(230,190)]</title>") != std::string::npos);

    CHECK(svg.find("marker-mid=\"url(#pgl-arrowhead)\"") != std::string::npos);
    CHECK(svg.find("stroke=\"teal\"") != std::string::npos);
    CHECK(svg.find("fill=\"teal\"") != std::string::npos);
    CHECK(svg.find("fill-opacity=\"0.18\"") != std::string::npos);
    CHECK(svg.find("stroke=\"sienna\"") != std::string::npos);
    CHECK(svg.find("fill=\"gold\"") != std::string::npos);
    CHECK(svg.find("fill-opacity=\"0.22\"") != std::string::npos);
    CHECK(svg.find("stroke=\"navy\"") != std::string::npos);
    CHECK(svg.find("fill=\"skyblue\"") != std::string::npos);
    CHECK(svg.find("fill-opacity=\"0.28\"") != std::string::npos);
    CHECK(svg.find("stroke=\"darkmagenta\"") != std::string::npos);
    CHECK(svg.find("fill=\"plum\"") != std::string::npos);
    CHECK(svg.find("fill-opacity=\"0.30\"") != std::string::npos);
}

TEST_CASE("Canvas draws runtime Shape alternatives") {
    const std::string path = "build/tests/output/shape_canvas.svg";

    using Point = pgl::Point<int>;
    using Shape = pgl::Shape<Point>;

    pgl::Canvas canvas;
    canvas << Shape(Point(1, 2))
           << Shape(pgl::Segment<Point>({0, 0}, {4, 0}))
           << Shape(pgl::Rectangle<Point>({0, 0}, {3, 2}))
           << Shape(pgl::Triangle<Point>({0, 0}, {4, 0}, {0, 3}))
           << Shape(pgl::Convex<Point>(std::vector<Point>{{0, 0}, {5, 0}, {5, 4}, {0, 4}}));

    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());

    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(svg.find("<title>(1,2)</title>") != std::string::npos);
    CHECK(svg.find("<title>(0,0)--(4,0)</title>") != std::string::npos);
    CHECK(svg.find("<title>[(0,0),(3,2)]</title>") != std::string::npos);
    CHECK(svg.find("<title>&lt;(0,0)(4,0)(0,3)&gt;</title>") != std::string::npos);
    CHECK(svg.find("<title>Convex[(0,0),(5,0),(5,4),(0,4)]</title>") != std::string::npos);
}
