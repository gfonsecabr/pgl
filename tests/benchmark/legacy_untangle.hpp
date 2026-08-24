#pragma once
//
// The pre-batching Polygon::untangle(), preserved verbatim for the shape-pair
// benchmark's dataset generators.
//
// untangle() was rewritten to select edge-disjoint batches of flips through an
// interval tree, which is orders of magnitude faster but does not pick the same
// flips as the old sequential scan — so it untangles a given ring into a
// *different* simple polygon. Every recorded run in tests/benchmark/history/ was
// measured on the polygons the old scan produced, and the dashboard plots those
// runs against each other over time, so the generators must keep producing the
// same shapes or the history stops being a comparison. The generators call
// legacyUntangledPolygon() instead of Polygon::untangle() for that reason alone;
// nothing here reflects a preference about the algorithm itself.
//
// The body below is the old Polygon::untangle() with its private-member access
// rewritten against a local vector: the generators all build their ring, hand it
// to the trusted Polygon constructor (which stores the points as given, without
// normalizing) and then untangle, so running the same loop on the raw vector and
// normalizing at the end -- which the untrusted constructor does -- reproduces
// the old result exactly.
//
#include "pgl.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

template <class PointType>
pgl::Polygon<PointType> legacyUntangledPolygon(std::vector<PointType> points) {
    const auto edge = [&points](std::ptrdiff_t a) {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points.size());
        return pgl::Segment<PointType>(points[static_cast<std::size_t>(a)],
                                       points[static_cast<std::size_t>((a + 1) % n)]);
    };

    while (points.size() >= 3) {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points.size());

        // 1) Uncross a transversally crossing pair of non-adjacent edges. The
        //    reversal of the sub-path v[i+1..j] strictly shortens the perimeter,
        //    so this can happen only finitely often for a fixed vertex count.
        bool flipped = false;
        for (std::ptrdiff_t i = 0; i < n && !flipped; ++i) {
            for (std::ptrdiff_t j = i + 1; j < n; ++j) {
                const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
                if (adjacent) {
                    continue;
                }
                if (edge(i).crosses(edge(j))) {
                    std::reverse(points.begin() + (i + 1), points.begin() + (j + 1));
                    flipped = true;
                    break;
                }
            }
        }
        if (flipped) {
            continue;
        }

        // 2) No transversal crossing remains, so any residual self-contact is a
        //    collinear touch/overlap, a coincident vertex, or a zero-length edge.
        //    In each case a vertex lies on a non-incident edge; deleting it clears
        //    the contact and strictly decreases the vertex count.
        bool removed = false;
        for (std::ptrdiff_t k = 0; k < n && !removed; ++k) {
            if (points[static_cast<std::size_t>(k)] ==
                points[static_cast<std::size_t>((k + 1) % n)]) {
                points.erase(points.begin() + k);  // zero-length edge
                removed = true;
                break;
            }
            for (std::ptrdiff_t e = 0; e < n; ++e) {
                if (e == k || e == (k - 1 + n) % n) {
                    continue;  // edges incident to vertex k always contain it
                }
                if (edge(e).contains(points[static_cast<std::size_t>(k)])) {
                    points.erase(points.begin() + k);
                    removed = true;
                    break;
                }
            }
        }
        if (!removed) {
            break;  // no crossing and no self-contact: the polygon is simple
        }
    }

    // Untrusted: the constructor normalizes, as the old untangle() did itself.
    return pgl::Polygon<PointType>(points);
}
