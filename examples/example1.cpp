#include <iostream>
#include "pgl.hpp"

int main() {
    pgl::Point p = {1, 0}, q = {4, 7};
    pgl::Segment s = {p, q}, t = {0, 8, 2, 1};
    if (s.intersects(t))
        std::cout << s << " intersects " << t << std::endl;

    pgl::Triangle tri(p, q, t.min());
    if (tri.intersects(s))
        std::cout << tri << " intersects " << s << std::endl;
    if (!tri.interiorsIntersect(s))
        std::cout << "The interiors of " << tri << " and " << s << " do not intersect\n";

    return 0;
}
// Output:
// (1,0)--(4,7) intersects (0,8)--(2,1)
// <(0,8)(1,0)(4,7)> intersects (1,0)--(4,7)
// The interiors of <(0,8)(1,0)(4,7)> and (1,0)--(4,7) do not intersect
