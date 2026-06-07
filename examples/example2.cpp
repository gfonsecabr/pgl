#include <iostream>
#include "pgl.hpp"

int main() {
    pgl::Canvas canvas;
    canvas << pgl::Point(0,0);

    pgl::Triangle tri = {-1, -1, 0, 2, 1, -2};
    canvas << pgl::stroke("green") << tri;
    canvas << pgl::stroke("blue") << 2*tri;
    canvas.writeSVG("example2.svg");

    return 0;
}
