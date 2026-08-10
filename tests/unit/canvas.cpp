#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "pgl.hpp"

static_assert(std::is_same_v<
              decltype(std::declval<const pgl::Canvas&>().writeSVG(std::declval<const std::string&>())),
              void>);
static_assert(std::is_same_v<
              decltype(std::declval<const pgl::Canvas&>().writePDF(std::declval<const std::string&>())),
              void>);
static_assert(std::is_same_v<
              decltype(std::declval<const pgl::Canvas&>().writeIPE(std::declval<const std::string&>())),
              void>);

TEST_CASE("Canvas stores SVG attributes with each inserted shape and writes an SVG file") {
    const std::string path = "build/tests/output/canvas_test.svg";

    pgl::Canvas canvas;
    canvas.borders(true);
    canvas << pgl::pointRadius("5") << pgl::strokeWidth("5");
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
    // third_segment is built from value-returning Point::operator+ (max()+translation),
    // so per the label rules the endpoint labels are default-constructed: the label
    // type (std::string) is kept but the value is empty, which IO prints as a bare ":".
    CHECK(svg.find("<title>:(100,100)-&gt;:(80,50)</title>") != std::string::npos);
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

TEST_CASE("Canvas writes a PDF file") {
    const std::string path = "build/tests/output/canvas_test.pdf";

    pgl::Canvas canvas;
    canvas.size(640.0, 480.0).margin(24.0).borders(true);

    canvas << pgl::pointRadius("4.0")
           << pgl::strokeWidth("3.0")
           << pgl::stroke("royalblue")
           << pgl::fill("none")
           << pgl::Segment<pgl::Point<int>>({0, 0}, {40, 30})
           << pgl::stroke("black")
           << pgl::fill("gold")
           << pgl::Triangle<pgl::Point<int>>({10, 10}, {30, 15}, {20, 35})
           << pgl::stroke("darkmagenta")
           << pgl::fill("plum")
           << pgl::Disk<pgl::Point<int>>({55, 20}, 8);

    canvas.writePDF(path);

    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());

    const std::string pdf((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    REQUIRE(pdf.size() > 32);

    CHECK(pdf.rfind("%PDF-1.3", 0) == 0);
    CHECK(pdf.find("xref") != std::string::npos);
    CHECK(pdf.find("trailer") != std::string::npos);
    CHECK(pdf.find("/Type /Page") != std::string::npos);
    CHECK(pdf.find("/ExtGState") == std::string::npos);
    CHECK(pdf.find("%%EOF") != std::string::npos);
    CHECK(pdf.find("%%EOF") + 5 >= pdf.size() - 2);
}

TEST_CASE("Canvas writes PDF opacity through ExtGState resources") {
    const std::string path = "build/tests/output/canvas_opacity_test.pdf";

    pgl::Canvas canvas;
    canvas.size(320.0, 240.0).margin(16.0);

    canvas << pgl::strokeWidth("4.0")
           << pgl::stroke("darkmagenta")
           << pgl::fill("plum")
           << pgl::fillOpacity("0.5")
           << pgl::strokeOpacity("0.25")
           << pgl::Triangle<pgl::Point<int>>({10, 10}, {70, 20}, {35, 80})
           << pgl::Rectangle<pgl::Point<int>>({90, 15}, {150, 75});

    canvas.writePDF(path);

    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());

    const std::string pdf((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    REQUIRE(pdf.size() > 32);

    const auto countSubstring = [](const std::string& haystack, const std::string& needle) {
        std::size_t count = 0;
        std::size_t offset = haystack.find(needle);
        while (offset != std::string::npos) {
            ++count;
            offset = haystack.find(needle, offset + needle.size());
        }
        return count;
    };

    CHECK(pdf.rfind("%PDF-1.4", 0) == 0);
    CHECK(pdf.find("/ExtGState <<") != std::string::npos);
    CHECK(pdf.find("/ca 0.500000") != std::string::npos);
    CHECK(pdf.find("/CA 0.250000") != std::string::npos);
    CHECK(pdf.find("/GS0 gs") != std::string::npos);
    CHECK(countSubstring(pdf, "/Type /ExtGState") == 1);
}

TEST_CASE("Canvas resolves CSS colors and percentage opacities in every backend") {
    const std::string pdfPath = "build/tests/output/canvas_css_style.pdf";
    const std::string ipePath = "build/tests/output/canvas_css_style.ipe";

    pgl::Canvas canvas;
    canvas.size(320.0, 240.0).margin(16.0);

    // seagreen and olivedrab are CSS color names; a backend that does not know
    // them leaves the shape unpainted. The opacity is a percentage and the
    // stroke width carries the "px" unit, as SVG allows for both.
    canvas << pgl::stroke("olivedrab")
           << pgl::fill("seagreen")
           << pgl::fillOpacity("25%")
           << pgl::strokeWidth("2px")
           << pgl::Rectangle<pgl::Point<int>>({0, 0}, {40, 30});

    canvas.writePDF(pdfPath);
    canvas.writeIPE(ipePath);

    std::ifstream pdfInput(pdfPath, std::ios::binary);
    REQUIRE(pdfInput.good());
    const std::string pdf((std::istreambuf_iterator<char>(pdfInput)), std::istreambuf_iterator<char>());

    // seagreen is 46,139,87 and olivedrab is 107,142,35, both scaled to [0,1];
    // "25%" is the fill alpha and "2px" the stroke width.
    CHECK(pdf.find("0.180392 0.545098 0.341176 rg") != std::string::npos);
    CHECK(pdf.find("0.419608 0.556863 0.137255 RG") != std::string::npos);
    CHECK(pdf.find("/ca 0.250000") != std::string::npos);
    CHECK(pdf.find("2.000000 w") != std::string::npos);

    std::ifstream ipeInput(ipePath);
    REQUIRE(ipeInput.good());
    const std::string ipe((std::istreambuf_iterator<char>(ipeInput)), std::istreambuf_iterator<char>());

    CHECK(ipe.find("<opacity name=\"op250\" value=\"0.25\"/>") != std::string::npos);
    CHECK(ipe.find("fill=\"0.180392 0.545098 0.341176\" opacity=\"op250\"") != std::string::npos);
    CHECK(ipe.find("stroke=\"0.419608 0.556863 0.137255\"") != std::string::npos);
    CHECK(ipe.find("pen=\"2\"") != std::string::npos);

    // Ipe's `opacity` dims the whole object, so an opaque stroke around a
    // translucent fill has to name its own opacity, declared like any other.
    CHECK(ipe.find("<opacity name=\"op1000\" value=\"1\"/>") != std::string::npos);
    CHECK(ipe.find("stroke-opacity=\"op1000\"") != std::string::npos);
}

TEST_CASE("Canvas writes an Ipe (.ipe) XML file") {
    const std::string path = "build/tests/output/canvas_test.ipe";

    pgl::Canvas canvas;
    canvas.size(320.0, 240.0).margin(16.0).borders(true);

    canvas << pgl::pointRadius("4.0")
           << pgl::strokeWidth("3.0")
           << pgl::stroke("royalblue")
           << pgl::fill("none")
           << pgl::Segment<pgl::Point<int>>({0, 0}, {40, 30})
           << pgl::stroke("black")
           << pgl::fill("gold")
           << pgl::fillOpacity("0.5")
           << pgl::Triangle<pgl::Point<int>>({10, 10}, {30, 15}, {20, 35})
           << pgl::stroke("darkmagenta")
           << pgl::fill("plum")
           << pgl::fillOpacity("1")
           << pgl::Disk<pgl::Point<int>>({55, 20}, 8)
           << pgl::OrientedSegment<pgl::Point<int>>({0, 30}, {40, 0});

    canvas.writeIPE(path);

    std::ifstream input(path);
    REQUIRE(input.good());

    const std::string ipe((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(ipe.rfind("<?xml version=\"1.0\"?>", 0) == 0);
    CHECK(ipe.find("<!DOCTYPE ipe SYSTEM \"ipe.dtd\">") != std::string::npos);
    CHECK(ipe.find("<ipe version=\"70218\"") != std::string::npos);
    CHECK(ipe.find("<ipestyle name=\"pgl\">") != std::string::npos);
    CHECK(ipe.find("<layout paper=\"320 240\" origin=\"0 0\" frame=\"320 240\"/>") != std::string::npos);
    // fillOpacity("0.5") must be declared as a named <opacity> entry, since
    // Ipe requires opacity attributes to reference a symbolic name.
    CHECK(ipe.find("<opacity name=\"op500\" value=\"0.5\"/>") != std::string::npos);
    CHECK(ipe.find("<layer name=\"alpha\"/>") != std::string::npos);
    CHECK(ipe.find("<view layers=\"alpha\" active=\"alpha\"/>") != std::string::npos);
    CHECK(ipe.find("</page>") != std::string::npos);
    CHECK(ipe.find("</ipe>") != std::string::npos);

    // The Segment is stroked only, with no fill.
    CHECK(ipe.find("stroke=\"0.254902 0.411765 0.882353\"") != std::string::npos);
    CHECK(ipe.find(" m\n") != std::string::npos);
    CHECK(ipe.find(" l\n") != std::string::npos);

    // The Triangle is stroked and filled, with the fill opacity applied.
    CHECK(ipe.find("fill=\"1 0.843137 0\" opacity=\"op500\"") != std::string::npos);
    CHECK(ipe.find("h\n") != std::string::npos);

    // The Disk is exported as an ellipse using Ipe's affine-matrix syntax
    // (a circle of radius r centered at (cx, cy) is "r 0 0 r cx cy e").
    CHECK(ipe.find(" e\n") != std::string::npos);

    // The OrientedSegment gets an extra filled triangle path for its arrowhead.
    const std::size_t pathCount = [&ipe]() {
        std::size_t count = 0;
        std::size_t offset = ipe.find("<path");
        while (offset != std::string::npos) {
            ++count;
            offset = ipe.find("<path", offset + 5);
        }
        return count;
    }();
    // border + segment + triangle + disk + oriented segment + its arrowhead.
    CHECK(pathCount == 6);
}

TEST_CASE("Canvas draws all implemented primitive shapes into SVG") {
    const std::string path = "build/tests/output/primitives_canvas.svg";

    pgl::Canvas canvas;
    canvas.size(900.0, 700.0).margin(30.0).borders(true);

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

    canvas << pgl::pointRadius("4.0")
           << pgl::strokeWidth("4.0")
           << pgl::stroke("black")
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

TEST_CASE("Canvas appends variants, optionals, and containers of drawable objects") {
    const std::string path = "build/tests/output/wrapped_objects_canvas.svg";

    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Drawable = std::variant<Point, Segment>;

    const Drawable variant = Point(1, 2);
    const std::optional<Drawable> optional = Segment({3, 4}, {5, 6});
    const std::optional<Drawable> empty;
    const std::vector<std::optional<Drawable>> objects = {
        Drawable(Point(7, 8)),
        std::nullopt,
        Drawable(Segment({9, 10}, {11, 12})),
    };

    pgl::Canvas canvas;
    canvas << variant << optional << empty << objects;
    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());
    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(svg.find("<title>(1,2)</title>") != std::string::npos);
    CHECK(svg.find("<title>(3,4)--(5,6)</title>") != std::string::npos);
    CHECK(svg.find("<title>(7,8)</title>") != std::string::npos);
    CHECK(svg.find("<title>(9,10)--(11,12)</title>") != std::string::npos);
}

TEST_CASE("Canvas renders a MonotoneChain as an open polyline") {
    const std::string path = "build/tests/output/monotonechain_canvas.svg";

    pgl::Canvas canvas;
    canvas << pgl::fill("gold")
           << pgl::MonotoneChain<pgl::Point<int>>({0, 0, 2, 4, 4, 0})
           << pgl::Shape<pgl::Point<int>>(pgl::MonotoneChain<pgl::Point<int>>({0, 2, 4, 2}));
    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());
    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(svg.find("<polyline") != std::string::npos);
    CHECK(svg.find("<polygon") == std::string::npos);   // never closed or filled
    // A curve is never filled, whatever the current fill color is: SVG would
    // otherwise paint the area enclosed by the implicit closing edge.
    CHECK(svg.find("fill=\"gold\"") == std::string::npos);
    CHECK(svg.find("fill=\"none\"") != std::string::npos);
    CHECK(svg.find("<title>MonotoneChain[(0,0),(2,4),(4,0)]</title>") != std::string::npos);
}

TEST_CASE("Canvas renders a HalfplaneIntersection clipped to the viewport") {
    const std::string path = "build/tests/output/halfplaneintersection_canvas.svg";

    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Region = pgl::HalfplaneIntersection<Point>;

    // A bounded cell, an unbounded wedge, and a Shape-wrapped strip.
    const Region cell{pgl::Rectangle<Point>({0, 0}, {6, 6})};
    const Region wedge({Halfplane({10, 0}, {14, 0}), Halfplane({10, 4}, {10, 0})});
    const pgl::Shape<Point> strip = Region({Halfplane({0, -8}, {1, -8}), Halfplane({1, -6}, {0, -6})});

    pgl::Canvas canvas;
    canvas << pgl::stroke("teal")
           << pgl::fill("teal")
           << pgl::fillOpacity("0.2")
           << cell
           << wedge
           << strip;
    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());
    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    // The region fills a clipped polygon and strokes only its real boundary
    // edges as separate lines.
    CHECK(svg.find("<polygon") != std::string::npos);
    CHECK(svg.find("stroke=\"none\"") != std::string::npos);
    CHECK(svg.find("<line") != std::string::npos);
    CHECK(svg.find("<title>HalfplaneIntersection[^-(0,0)--(6,0)-^") != std::string::npos);
    CHECK(svg.find("<title>HalfplaneIntersection[^-(10,0)--(14,0)-^") != std::string::npos);
    CHECK(svg.find("^-(0,-8)--(1,-8)-^") != std::string::npos);
    CHECK(svg.find("fill=\"teal\"") != std::string::npos);
    CHECK(svg.find("fill-opacity=\"0.2\"") != std::string::npos);

    // The empty region draws nothing.
    pgl::Canvas emptyCanvas;
    emptyCanvas << Region({Halfplane({0, 0}, {1, 0}), Halfplane({1, -1}, {0, -1})});
    const std::string emptyPath = "build/tests/output/halfplaneintersection_empty_canvas.svg";
    emptyCanvas.writeSVG(emptyPath);
    std::ifstream emptyInput(emptyPath);
    REQUIRE(emptyInput.good());
    const std::string emptySVG((std::istreambuf_iterator<char>(emptyInput)), std::istreambuf_iterator<char>());
    CHECK(emptySVG.find("<polygon") == std::string::npos);
    CHECK(emptySVG.find("<line") == std::string::npos);

    // PDF and Ipe exports accept the region as well.
    canvas.writePDF("build/tests/output/halfplaneintersection_canvas.pdf");
    canvas.writeIPE("build/tests/output/halfplaneintersection_canvas.ipe");
    std::ifstream pdfInput("build/tests/output/halfplaneintersection_canvas.pdf", std::ios::binary);
    REQUIRE(pdfInput.good());
    const std::string pdf((std::istreambuf_iterator<char>(pdfInput)), std::istreambuf_iterator<char>());
    CHECK(pdf.rfind("%PDF-", 0) == 0);
    std::ifstream ipeInput("build/tests/output/halfplaneintersection_canvas.ipe");
    REQUIRE(ipeInput.good());
    const std::string ipe((std::istreambuf_iterator<char>(ipeInput)), std::istreambuf_iterator<char>());
    CHECK(ipe.find("</ipe>") != std::string::npos);
}

TEST_CASE("Canvas view fits an explicit window instead of the inserted geometry") {
    using Point = pgl::Point<int>;

    pgl::Canvas canvas;
    canvas.size(200.0, 200.0).margin(0.0).view(pgl::Rectangle<Point>({0, 0}, {10, 10}));
    canvas << pgl::pointRadius("0") << Point(5, 5) << Point(1000, 1000);
    const std::string svg = canvas.toSVG();

    // The window, not the geometry, decides the mapping: the middle of the
    // window is the middle of the image, and the far point falls outside it.
    CHECK(svg.find("cx=\"100\" cy=\"100\"") != std::string::npos);
    CHECK(svg.find("<title>(5,5)</title>") != std::string::npos);
    CHECK(svg.find("cx=\"19801\"") != std::string::npos);

    // Without the window, the same two points are what gets fitted.
    pgl::Canvas fitted;
    fitted.size(200.0, 200.0).margin(0.0);
    fitted << pgl::pointRadius("0") << Point(5, 5) << Point(1000, 1000);
    CHECK(fitted.toSVG().find("cx=\"100\" cy=\"100\"") == std::string::npos);
}

TEST_CASE("Canvas renders a Polyline as an open polyline") {
    const std::string path = "build/tests/output/polyline_canvas.svg";

    pgl::Canvas canvas;
    canvas << pgl::fill("gold")
           << pgl::Polyline<pgl::Point<int>>({0, 0, 4, 4, 4, 0, 0, 4})
           << pgl::Shape<pgl::Point<int>>(pgl::Polyline<pgl::Point<int>>({0, 2, 4, 2}));
    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());
    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    CHECK(svg.find("<polyline") != std::string::npos);
    CHECK(svg.find("<polygon") == std::string::npos);   // never closed or filled
    CHECK(svg.find("fill=\"gold\"") == std::string::npos);
    CHECK(svg.find("fill=\"none\"") != std::string::npos);
    CHECK(svg.find("<title>Polyline[(0,0),(4,4),(4,0),(0,4)]</title>") != std::string::npos);
}

TEST_CASE("Canvas renders a PolygonWithHoles as one even-odd path per ring") {
    const std::string path = "build/tests/output/polygonwithholes_canvas.svg";

    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;

    const Region annulus(Polygon({0, 0, 8, 0, 8, 8, 0, 8}),
                         std::vector<Polygon>{Polygon({2, 2, 4, 2, 4, 4, 2, 4}),
                                              Polygon({5, 5, 7, 5, 7, 7, 5, 7})});
    const Region plain(Polygon({12, 0, 16, 0, 16, 4, 12, 4}));

    pgl::Canvas canvas;
    canvas << pgl::stroke("navy")
           << pgl::fill("skyblue")
           << pgl::fillOpacity("0.4")
           << annulus
           << pgl::Shape<Point>(plain);
    canvas.writeSVG(path);

    std::ifstream input(path);
    REQUIRE(input.good());
    const std::string svg((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    // One <path> per region, with the holes as extra closed subpaths and the
    // even-odd rule that punches them out.
    CHECK(svg.find("fill-rule=\"evenodd\"") != std::string::npos);
    CHECK(svg.find("<polygon") == std::string::npos);
    CHECK(svg.find("fill=\"skyblue\"") != std::string::npos);
    CHECK(svg.find("<title>PolygonWithHoles[Polygon[(0,0),(8,0),(8,8),(0,8)],"
                   "Polygon[(2,2),(4,2),(4,4),(2,4)],Polygon[(5,5),(7,5),(7,7),(5,7)]]</title>") !=
          std::string::npos);
    CHECK(svg.find("<title>PolygonWithHoles[Polygon[(12,0),(16,0),(16,4),(12,4)]]</title>") !=
          std::string::npos);

    // Three rings on the annulus and one on the hole-free region: four
    // subpaths, each opened by an M and closed by a Z.
    const auto count = [&svg](const std::string& needle) {
        std::size_t total = 0;
        for (std::size_t at = svg.find(needle); at != std::string::npos; at = svg.find(needle, at + 1)) {
            ++total;
        }
        return total;
    };
    CHECK(count("M ") == 4);
    CHECK(count(" Z") == 4);

    // Every hole must wind *against* the outer ring. That is what makes the
    // fill come out as the region under the nonzero-winding rule as well as
    // under even-odd, which is the only reason the PDF and Ipe backends — which
    // cannot ask for even-odd — get it right. Recovering the signed area of each
    // subpath pins it: the outer ring's sign is one, every hole's the other.
    // Split on the subpath *closer* rather than its opener: SVG writes the
    // opener before its coordinates (`M x,y … Z`) while PDF and Ipe write it
    // after (`x y m … h`), but all three end a subpath after its last point.
    const auto subpathSigns = [](const std::string& body, char closer) {
        std::vector<int> signs;
        std::size_t from = 0;
        while (true) {
            const std::size_t stop = body.find(closer, from);
            if (stop == std::string::npos) break;
            const std::string run = body.substr(from, stop - from);
            from = stop + 1;
            std::vector<double> values;
            std::size_t pos = 0;
            while (pos < run.size()) {
                const std::size_t start = run.find_first_of("-0123456789.", pos);
                if (start == std::string::npos) break;
                std::size_t end = start;
                while (end < run.size() && (std::isdigit(static_cast<unsigned char>(run[end])) != 0 ||
                                            run[end] == '-' || run[end] == '.')) {
                    ++end;
                }
                values.push_back(std::stod(run.substr(start, end - start)));
                pos = end;
            }
            REQUIRE(values.size() >= 6);
            REQUIRE(values.size() % 2 == 0);
            double twiceArea = 0.0;
            for (std::size_t i = 0; i + 1 < values.size(); i += 2) {
                const std::size_t j = (i + 2) % values.size();
                twiceArea += values[i] * values[j + 1] - values[j] * values[i + 1];
            }
            signs.push_back(twiceArea > 0.0 ? 1 : -1);
        }
        return signs;
    };

    const std::size_t firstPath = svg.find("<path d=\"");
    REQUIRE(firstPath != std::string::npos);
    const std::string first = svg.substr(firstPath, svg.find("</path>", firstPath) - firstPath);
    CHECK(first.find("fill-rule") != std::string::npos);
    // Only the d attribute: the <title> that follows it spells out the region's
    // own coordinates, which would parse as path numbers.
    const std::size_t dStart = firstPath + std::string("<path d=\"").size();
    const std::string svgPathData = svg.substr(dStart, svg.find('"', dStart) - dStart);
    const auto svgSigns = subpathSigns(svgPathData, 'Z');
    REQUIRE(svgSigns.size() == 3);
    CHECK(svgSigns[1] == -svgSigns[0]);
    CHECK(svgSigns[2] == -svgSigns[0]);

    // The empty region draws nothing at all.
    pgl::Canvas emptyCanvas;
    emptyCanvas << pgl::Point<int>(0, 0) << Region();
    const std::string emptyPath = "build/tests/output/polygonwithholes_empty_canvas.svg";
    emptyCanvas.writeSVG(emptyPath);
    std::ifstream emptyInput(emptyPath);
    REQUIRE(emptyInput.good());
    const std::string emptySVG((std::istreambuf_iterator<char>(emptyInput)), std::istreambuf_iterator<char>());
    CHECK(emptySVG.find("<path") == std::string::npos);

    // PDF and Ipe take the region too, one subpath per ring in both.
    canvas.writePDF("build/tests/output/polygonwithholes_canvas.pdf");
    canvas.writeIPE("build/tests/output/polygonwithholes_canvas.ipe");
    std::ifstream pdfInput("build/tests/output/polygonwithholes_canvas.pdf", std::ios::binary);
    REQUIRE(pdfInput.good());
    const std::string pdf((std::istreambuf_iterator<char>(pdfInput)), std::istreambuf_iterator<char>());
    CHECK(pdf.rfind("%PDF-", 0) == 0);
    std::ifstream ipeInput("build/tests/output/polygonwithholes_canvas.ipe");
    REQUIRE(ipeInput.good());
    const std::string ipe((std::istreambuf_iterator<char>(ipeInput)), std::istreambuf_iterator<char>());
    CHECK(ipe.find("</ipe>") != std::string::npos);

    // The annulus is one Ipe path holding three closed subpaths, with the same
    // opposite windings the SVG carries — Ipe takes its fill rule from the style
    // sheet, so the winding is the whole of the correctness here.
    const std::size_t ipePath = ipe.find("<path");
    REQUIRE(ipePath != std::string::npos);
    const std::string ipeFirst = ipe.substr(ipePath, ipe.find("</path>", ipePath) - ipePath);
    std::size_t closers = 0;
    for (std::size_t at = ipeFirst.find("\nh\n"); at != std::string::npos; at = ipeFirst.find("\nh\n", at + 1)) {
        ++closers;
    }
    CHECK(closers == 3);
    CHECK(ipeFirst.find("fill=") != std::string::npos);
    // Drop the attribute line, whose "0.529412 0.807843 0.921569" would parse
    // as coordinates.
    const std::string ipeBody = ipeFirst.substr(ipeFirst.find(">\n") + 1);
    const auto ipeSigns = subpathSigns(ipeBody, 'h');
    REQUIRE(ipeSigns.size() == 3);
    CHECK(ipeSigns[1] == -ipeSigns[0]);
    CHECK(ipeSigns[2] == -ipeSigns[0]);

    // The PDF carries the same three subpaths in one path object: three `h`
    // closers, then a single `B` (fill and stroke). pdfgen emits its content
    // streams uncompressed, so they are readable as text.
    const std::size_t pdfRing = pdf.find("23.000000 261.500000 m");
    REQUIRE(pdfRing != std::string::npos);
    const std::string pdfPath = pdf.substr(pdfRing, pdf.find("\r\nB\r\n", pdfRing) - pdfRing);
    std::size_t pdfClosers = 0;
    for (std::size_t at = pdfPath.find("h\r\n"); at != std::string::npos; at = pdfPath.find("h\r\n", at + 1)) {
        ++pdfClosers;
    }
    CHECK(pdfClosers == 3);
    const auto pdfSigns = subpathSigns(pdfPath, 'h');
    REQUIRE(pdfSigns.size() == 3);
    CHECK(pdfSigns[1] == -pdfSigns[0]);
    CHECK(pdfSigns[2] == -pdfSigns[0]);
}

TEST_CASE("Canvas renders a PolygonSet as one path over every ring of every component") {
    using Point = pgl::Point<int>;
    using PolygonShape = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;
    using RegionSet = pgl::PolygonSet<Point>;

    // A holed component and a plain one: three rings in all, and the hole
    // belongs to the first component only.
    const RegionSet set(std::vector<Region>{
        Region(PolygonShape({0, 0, 8, 0, 8, 8, 0, 8}),
               std::vector<PolygonShape>{PolygonShape({2, 2, 4, 2, 4, 4, 2, 4})}),
        Region(PolygonShape({12, 0, 16, 0, 16, 4, 12, 4}))});

    pgl::Canvas canvas;
    canvas << pgl::stroke("navy") << pgl::fill("skyblue") << set;
    const std::string svg = canvas.toSVG();

    const auto count = [](const std::string& text, const std::string& needle) {
        std::size_t total = 0;
        for (std::size_t at = text.find(needle); at != std::string::npos;
             at = text.find(needle, at + 1)) {
            ++total;
        }
        return total;
    };

    // The whole set is a single element: one <path>, one title, three subpaths.
    CHECK(count(svg, "<path") == 1);
    CHECK(count(svg, "M ") == 3);
    CHECK(count(svg, " Z") == 3);
    CHECK(svg.find("fill-rule=\"evenodd\"") != std::string::npos);
    CHECK(svg.find("<polygon") == std::string::npos);
    CHECK(svg.find("<title>PolygonSet[PolygonWithHoles[Polygon[(0,0),(8,0),(8,8),(0,8)],"
                   "Polygon[(2,2),(4,2),(4,4),(2,4)]],"
                   "PolygonWithHoles[Polygon[(12,0),(16,0),(16,4),(12,4)]]]</title>") !=
          std::string::npos);

    // The subpath windings, recovered from each run's signed area. Only the hole
    // winds against the rest: the second component's outer ring must wind *with*
    // the first's, or the nonzero rule the PDF and Ipe backends use would punch
    // it out as if it were a hole of the other component.
    const std::size_t dStart = svg.find("<path d=\"") + std::string("<path d=\"").size();
    const std::string data = svg.substr(dStart, svg.find('"', dStart) - dStart);
    std::vector<int> signs;
    for (std::size_t from = 0; from < data.size();) {
        const std::size_t stop = data.find('Z', from);
        if (stop == std::string::npos) break;
        std::vector<double> values;
        const std::string run = data.substr(from, stop - from);
        std::size_t pos = 0;
        while (pos < run.size()) {
            const std::size_t start = run.find_first_of("-0123456789.", pos);
            if (start == std::string::npos) break;
            std::size_t end = start;
            while (end < run.size() && (std::isdigit(static_cast<unsigned char>(run[end])) != 0 ||
                                        run[end] == '-' || run[end] == '.')) {
                ++end;
            }
            values.push_back(std::stod(run.substr(start, end - start)));
            pos = end;
        }
        REQUIRE(values.size() >= 6);
        REQUIRE(values.size() % 2 == 0);
        double twiceArea = 0.0;
        for (std::size_t i = 0; i + 1 < values.size(); i += 2) {
            const std::size_t j = (i + 2) % values.size();
            twiceArea += values[i] * values[j + 1] - values[j] * values[i + 1];
        }
        signs.push_back(twiceArea > 0.0 ? 1 : -1);
        from = stop + 1;
    }
    REQUIRE(signs.size() == 3);
    CHECK(signs[1] == -signs[0]);
    CHECK(signs[2] == signs[0]);

    // The wrapper draws the set exactly as the concrete shape does.
    pgl::Canvas wrapped;
    wrapped << pgl::stroke("navy") << pgl::fill("skyblue") << pgl::Shape<Point>(set);
    CHECK(wrapped.toSVG() == svg);

    // Ipe and PDF take the set too, as one path with three closed subpaths.
    const std::string ipe = canvas.toIPE();
    CHECK(count(ipe, "<path") == 1);
    CHECK(count(ipe, "\nh\n") == 3);
    const std::string pdf = canvas.toPDF();
    CHECK(pdf.rfind("%PDF-", 0) == 0);
    CHECK(count(pdf, "h\r\n") == 3);

    // The empty set draws nothing at all.
    pgl::Canvas emptyCanvas;
    emptyCanvas << Point(0, 0) << RegionSet();
    CHECK(emptyCanvas.toSVG().find("<path") == std::string::npos);
}
