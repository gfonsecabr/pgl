// Draws a small "HELLO WORLD" poster with geometry rather than text.
//
// Output: example_helloworld.svg

#include <iostream>

#include "pgl.hpp"

using IntPoint = pgl::Point<int>;
using RationalPoint = pgl::Point<pgl::ERational>;

int main() {
    pgl::Canvas canvas;

    // HELLO -----------------------------------------------------------------
    // H is made from three bold integer-coordinate segments.
    canvas << pgl::stroke("midnightblue") << pgl::strokeWidth("4px")
           << pgl::Segment<IntPoint>(0, 28, 0, 44)
           << pgl::Segment<IntPoint>(10, 28, 10, 44)
           << pgl::Segment<IntPoint>(0, 36, 10, 36);

    // E is a stack of rectangular brush strokes.
    canvas << pgl::stroke("none") << pgl::fill("tomato")
           << pgl::Rectangle<IntPoint>(14, 28, 17, 44)
           << pgl::Rectangle<IntPoint>(16, 41, 25, 44)
           << pgl::Rectangle<IntPoint>(16, 34, 23, 37)
           << pgl::Rectangle<IntPoint>(16, 28, 25, 31);

    // This L is a template.  Its two occurrences are translated copies rather
    // than two separately authored letters.
    const pgl::Polyline<IntPoint> letterL({IntPoint(0, 16), IntPoint(0, 0), IntPoint(10, 0)});
    canvas << pgl::stroke("seagreen") << pgl::strokeWidth("4px")
           << letterL + IntPoint(29, 28)
           << letterL + IntPoint(42, 28);

    // O is an exact-rational polygon with a hexagonal hole.  The half-unit
    // corners give the letter a deliberately off-grid, hand-cut feel.
    const pgl::PolygonWithHoles<RationalPoint> letterO(
        pgl::Polygon<RationalPoint>({
            {pgl::ERational(2), pgl::ERational(0)}, {pgl::ERational(9), pgl::ERational(0)},
            {pgl::ERational(11), pgl::ERational(3)}, {pgl::ERational(11), pgl::ERational(13)},
            {pgl::ERational(9), pgl::ERational(16)}, {pgl::ERational(2), pgl::ERational(16)},
            {pgl::ERational(0), pgl::ERational(13)}, {pgl::ERational(0), pgl::ERational(3)},
        }),
        std::vector{pgl::Polygon<RationalPoint>({
            {pgl::ERational(3), pgl::ERational(4)}, {pgl::ERational(8), pgl::ERational(3)},
            {pgl::ERational(9), pgl::ERational(6)}, {pgl::ERational(9), pgl::ERational(10)},
            {pgl::ERational(8), pgl::ERational(13)}, {pgl::ERational(3), pgl::ERational(12)},
            {pgl::ERational(2), pgl::ERational(9)}, {pgl::ERational(2), pgl::ERational(6)},
        })});
    const RationalPoint helloOOffset(pgl::ERational(56), pgl::ERational(28));
    canvas << pgl::stroke("darkorange") << pgl::strokeWidth("2px") << pgl::fill("gold")
           << letterO + helloOOffset;

    // WORLD -----------------------------------------------------------------
    // W is a single zig-zag polyline; two points make the corners sparkle.
    const pgl::Polyline<IntPoint> letterW(
        {IntPoint(0, 16), IntPoint(2, 0), IntPoint(6, 9), IntPoint(10, 0), IntPoint(12, 16)});
    canvas << pgl::stroke("rebeccapurple") << pgl::strokeWidth("4px")
           << letterW;
    canvas << pgl::stroke("plum") << pgl::fill("plum")
           << IntPoint(2, 0)
           << IntPoint(10, 0);

    // Reuse the rational O, translated into WORLD's second slot.
    const RationalPoint worldOOffset(pgl::ERational(17), pgl::ERational(0));
    canvas << pgl::stroke("darkorange") << pgl::strokeWidth("2px") << pgl::fill("gold")
           << letterO + worldOOffset;

    // R combines an integer rectangle, disks, and a triangular kickstand.
    canvas << pgl::stroke("none") << pgl::fill("steelblue")
           << pgl::Rectangle<IntPoint>(34, 0, 37, 16)
           << pgl::Disk<IntPoint>(IntPoint(38, 12), 6)
           << pgl::Triangle<IntPoint>(IntPoint(37, 8), IntPoint(46, 0), IntPoint(41, 0));
    canvas << pgl::fill("white") << pgl::Disk<IntPoint>(IntPoint(38, 12), 3);

    // The L comes from the same translated template used above.
    canvas << pgl::stroke("seagreen") << pgl::strokeWidth("4px")
           << letterL + IntPoint(50, 0);

    // D is a faceted, integer-coordinate polygon set against a solid spine.
    canvas << pgl::stroke("sienna") << pgl::strokeWidth("2px") << pgl::fill("peachpuff")
           << pgl::Rectangle<IntPoint>(63, 0, 66, 16)
           << pgl::Polygon<IntPoint>({IntPoint(65, 0), IntPoint(72, 0), IntPoint(77, 4),
                                      IntPoint(77, 12), IntPoint(72, 16), IntPoint(65, 16)});
    canvas << pgl::stroke("none") << pgl::fill("white")
           << pgl::Convex<IntPoint>({IntPoint(67, 3), IntPoint(71, 3), IntPoint(74, 6),
                                     IntPoint(74, 10), IntPoint(71, 13), IntPoint(67, 13)});

    // ! is a triangle dot and a halfspace intersection
    canvas << pgl::stroke("grey") << pgl::strokeWidth("2px") << pgl::fill("lightyellow")
           << pgl::Triangle<IntPoint>(79, 0, 82, 7, 85,0);
    pgl::Halfplane<IntPoint> h1(80,15,81,10);
    pgl::Halfplane<IntPoint> h2(1,10,10,10);
    pgl::Halfplane<IntPoint> h3(83,10,84,25);
    canvas << pgl::HalfplaneIntersection<IntPoint>({h1,h2,h3});

    canvas.writeSVG("example_helloworld.svg");
    std::cout << "wrote example_helloworld.svg\n";
    return 0;
}
