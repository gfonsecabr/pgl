#pragma once
//
// Dataset generators for the asymptotic benchmarks.
//
// Point, segment, rectangle and triangle datasets come straight from the
// shape-pair benchmark's generators (../randomshapes.hpp), so "small" and
// "large" mean exactly what they mean on the pairs page: a small shape spans
// smallRange in a disk of diameter largeRange and a random pair usually misses;
// a large one spans mediumRange in a disk of diameter mediumRange and a random
// pair usually meets.
//
// Polygons do not, and that is deliberate. randomshapes.hpp's polygon
// generators call legacyUntangledPolygon() to pin the shapes the pairs page's
// recorded history was measured on (see ../legacy_untangle.hpp); the asymptotic
// benchmarks have no such history to protect and would only be paying the old
// implementation's cubic cost for nothing. randomPolygon() below is
// randomLargePolygons()'s construction with the library's current untangle().
//
// Everything is generated with `int` coordinates. Callers convert (see
// bench::convert) rather than re-generating per number type: the generators
// draw integer coordinates whatever the target type is, so generating in
// ERational would buy nothing but slower arithmetic on the same numbers, and
// converting is what guarantees the int and ERational runs measure the
// identical input — without which comparing their result signatures would prove
// nothing.
//
// One point dataset is not generated at all: euro-night is read from
// data/euro-night-0100000.instance, a CG:SHOP 2019 instance of 100,000 points
// sampled from a night-time image of Europe, whose point lines were shuffled
// once before being checked in. Its n-point sample is simply the file's first n
// points, so every size of the sweep is a uniform sample of the same picture and
// each is a prefix of the next.
//
// Nor are three polygon datasets taken from the Salzburg Database of Polygonal
// Data (SBPD, release sbgdb-20200507): fpg and spg, simple polygons, and
// fpg-holes, polygons with holes, read from data/sbpd-<name>.polygons as
// written by data/sbpd.py. The database has one polygon per size, at sizes of
// its own choosing, so a sweep over any of them snaps each size of its list to
// the nearest size the file holds (sbpdSizes), and the file keeps only the
// sizes the checked-in lists snap to.
//
#include "../randomshapes.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace bench {

using IntPoint     = pgl::Point<int>;
using IntSegment   = pgl::Segment<IntPoint>;
using IntTriangle  = pgl::Triangle<IntPoint>;
using IntRectangle = pgl::Rectangle<IntPoint>;
using IntPolygon   = pgl::Polygon<IntPoint>;
using IntRegion    = pgl::PolygonWithHoles<IntPoint>;
using IntShape     = pgl::Shape<IntPoint>;

// An `int` dataset shape over another number type: what a driver converts the
// generated shape into.
template <class Shape, class Number> struct Retyped;
template <class Number> struct Retyped<IntPolygon, Number> {
    using type = pgl::Polygon<pgl::Point<Number>>;
};
template <class Number> struct Retyped<IntRegion, Number> {
    using type = pgl::PolygonWithHoles<pgl::Point<Number>>;
};
template <class Shape, class Number>
using retyped_t = typename Retyped<Shape, Number>::type;

// n distinct random points in the large disk.
inline std::vector<IntPoint> randomPoints(int n) {
    return ::randomPoints<int>(n);
}

// n query points in the same disk, drawn from a different seed than
// randomPoints(). Sharing the seed would make the queries a prefix of the
// dataset, so every one of them would land exactly on a vertex — the one case a
// point location is least representative of.
inline std::vector<IntPoint> queryPoints(int n) {
    Rng rng{0x9E3779B97F4A7C15ULL};
    std::vector<IntPoint> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v.push_back(randomPoint<int>(rng, largeRange));
    }
    return v;
}

// n small / large random segments, as on the pairs page.
inline std::vector<IntSegment> smallSegments(int n) {
    return randomSmallBishape<IntSegment>(n);
}
inline std::vector<IntSegment> largeSegments(int n) {
    return randomLargeBishape<IntSegment>(n);
}

// smallSegments(n) with every twentieth shape a ray and every twentieth a line.
// Each is drawn as a small segment is -- a point in the large disk and a small
// offset from it -- but the draw order is kept, since a Segment would sort its
// endpoints: the ray starts at the first point and passes through the second.
inline std::vector<IntShape> mixedShapes(int n) {
    std::vector<IntShape> w;
    w.reserve(static_cast<std::size_t>(n));
    std::set<IntShape> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<IntSegment>)};
    while (static_cast<int>(w.size()) < n) {
        const auto p1 = randomPoint<int>(rng, largeRange);
        const auto p2 = p1 + randomPoint<int>(rng, smallRange);
        if (p1 == p2) continue;
        IntShape s;
        switch (w.size() % 20) {
            case 0:  s = IntShape(pgl::Ray<IntPoint>(p1, p2)); break;
            case 1:  s = IntShape(pgl::Line<IntPoint>(p1, p2)); break;
            default: s = IntShape(IntSegment(p1, p2)); break;
        }
        if (seen.insert(s).second) {
            w.push_back(s);
        }
    }
    return w;
}

// The edges of the Voronoi diagram of randomPoints(n): a segment per Delaunay
// edge whose two circumcentres differ, and a ray per convex-hull edge. The one
// dataset not in `int` coordinates, since a Voronoi vertex is a circumcentre:
// the edges come exact, and every consumer takes them as they are.
inline std::vector<pgl::EShape> voronoiEdges(int n) {
    const pgl::Triangulation<IntTriangle> delaunay(randomPoints(n));
    return delaunay.voronoiEdges<pgl::ERational>();
}

// smallSegments(n) under the shear (x, y) -> (x, 10x + y). The map is affine,
// so exactly the same pairs meet, in the same way, but the segments turn nearly
// vertical and their y-extents stretch tenfold: pairs whose bounding boxes
// overlap outnumber the crossings many times over, where the unsheared small
// segments have about as many of one as of the other.
inline std::vector<IntSegment> shearedSegments(int n) {
    std::vector<IntSegment> segments = smallSegments(n);
    for (auto& s : segments) {
        s = IntSegment(IntPoint(s[0].x(), 10 * s[0].x() + s[0].y()),
                       IntPoint(s[1].x(), 10 * s[1].x() + s[1].y()));
    }
    return segments;
}

// n large random triangles, as on the pairs page: each spans the disk it is
// scattered over, so a set of them overlaps heavily.
inline std::vector<IntTriangle> largeTriangles(int n) {
    return randomLargeTrishape<IntTriangle>(n);
}

// Query shapes. Small, so a query selects a modest neighbourhood rather than
// most of the tree.
inline std::vector<IntRectangle> queryRectangles(int n) {
    return randomSmallBishape<IntRectangle>(n);
}
inline std::vector<IntTriangle> queryTriangles(int n) {
    return randomSmallTrishape<IntTriangle>(n);
}

namespace detail {

// One simple polygon of at most m vertices: m points drawn around a common
// anchor and untangled, which is randomLargePolygons' construction with the
// library's current untangle(). Retries on the anchor's own stream, so a draw
// that collapses to a degenerate ring simply advances it.
inline IntPolygon polygonOfSpan(int m, int disk, int span, std::uint64_t seed) {
    Rng rng{seed};
    while (true) {
        const auto base = randomPoint<int>(rng, disk);
        std::vector<IntPoint> vertices;
        vertices.reserve(static_cast<std::size_t>(m));
        for (int i = 0; i < m; ++i) {
            vertices.push_back(base + randomPoint<int>(rng, span));
        }
        IntPolygon polygon(vertices);
        polygon.untangle();
        if (!polygon.isDegenerate()) {
            return polygon;
        }
    }
}

}  // namespace detail

// A polygon at the pairs page's "large" scale. `seed` shifts the draw, so a
// caller needing two independent polygons of the same size gets two different
// ones.
inline IntPolygon randomPolygon(int m, std::uint64_t seed = 0) {
    return detail::polygonOfSpan(m, mediumRange, mediumRange, seed);
}

// A polygon at the "small" scale, for the categories that pair a polygon of the
// swept size against a fixed small operand.
inline IntPolygon randomSmallPolygon(int m, std::uint64_t seed = 0) {
    return detail::polygonOfSpan(m, largeRange, smallRange, seed);
}

// One convex polygon at the pairs page's "small" scale: its first small Convex,
// the hull of 1000 points drawn in a disk of diameter 1,000 and placed in the
// disk of diameter 10,000. The same shape at every size, for the categories
// that pair a polygon of the swept size against a fixed small convex operand.
inline pgl::Convex<IntPoint> smallConvex() {
    return randomSmallConvexes<int>(1, 1000).front();
}

// `count` points strictly inside `polygon` (a Polygon or a PolygonWithHoles),
// rejection-sampled from its bounding box. Drawn per polygon, since a point inside one means nothing for another,
// and drawn on the integer polygon so that converting gives the ERational run —
// and the CGAL baseline — the identical queries.
template <class Shape>
std::vector<IntPoint> interiorPoints(const Shape& polygon, int count) {
    const auto box = polygon.bbox();
    Rng rng{12345};
    const int width  = box.max().x() - box.min().x();
    const int height = box.max().y() - box.min().y();
    std::vector<IntPoint> queries;
    queries.reserve(static_cast<std::size_t>(count));
    while (static_cast<int>(queries.size()) < count) {
        const IntPoint candidate(box.min().x() + rng.range(width),
                                 box.min().y() + rng.range(height));
        if (polygon.interiorContains(candidate)) {
            queries.push_back(candidate);
        }
    }
    return queries;
}

// All 100,000 points of the euro-night instance, in the file's (shuffled) order.
// Read once per process, and found next to this header rather than relative to
// the working directory, so a driver runs from anywhere.
inline const std::vector<IntPoint>& euroNightAll() {
    static const std::vector<IntPoint> all = [] {
        const auto path = std::filesystem::path(__FILE__).parent_path() / "data" /
                          "euro-night-0100000.instance";
        std::ifstream in(path);
        if (!in) {
            std::cerr << "cannot read the euro-night dataset at " << path << "\n";
            std::exit(3);
        }
        std::vector<IntPoint> v;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream fields(line);
            long long index = 0;
            int x = 0, y = 0;
            if (!(fields >> index >> x >> y)) {
                std::cerr << "malformed line in " << path << ": " << line << "\n";
                std::exit(3);
            }
            v.emplace_back(x, y);
        }
        return v;
    }();
    return all;
}

// The first n points of euro-night.
inline std::vector<IntPoint> euroNightPoints(int n) {
    const auto& all = euroNightAll();
    if (n < 0 || static_cast<std::size_t>(n) > all.size()) {
        std::cerr << "euro-night has " << all.size() << " points; asked for " << n << "\n";
        std::exit(3);
    }
    return std::vector<IntPoint>(all.begin(), all.begin() + n);
}

namespace detail {

// The map that carries the random datasets' disk onto euro-night's bounding box:
// centre onto centre, each axis scaled by the whole number of times the disk's
// diameter fits into the box's extent. Whole numbers, so the image of an integer
// shape is an integer shape, distinct shapes stay distinct and a non-degenerate
// one stays non-degenerate.
struct EuroNightFrame {
    IntPoint centre;
    int sx, sy;

    IntPoint operator()(const IntPoint& p) const {
        return IntPoint(centre.x() + sx * p.x(), centre.y() + sy * p.y());
    }
};

inline EuroNightFrame euroNightFrame() {
    const auto& all = euroNightAll();
    int minX = all.front().x(), maxX = minX, minY = all.front().y(), maxY = minY;
    for (const auto& p : all) {
        minX = std::min(minX, p.x());
        maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y());
        maxY = std::max(maxY, p.y());
    }
    return {IntPoint((minX + maxX) / 2, (minY + maxY) / 2),
            std::max(1, (maxX - minX) / largeRange), std::max(1, (maxY - minY) / largeRange)};
}

}  // namespace detail

// The random datasets' query batches carried onto euro-night's bounding box, so
// the queries sit where its points are and the two datasets are asked the same
// queries, up to that map.
inline std::vector<IntPoint> euroNightQueryPoints(int n) {
    const auto frame = detail::euroNightFrame();
    auto v = queryPoints(n);
    for (auto& p : v) p = frame(p);
    return v;
}
inline std::vector<IntRectangle> euroNightQueryRectangles(int n) {
    const auto frame = detail::euroNightFrame();
    auto v = queryRectangles(n);
    for (auto& r : v) r = IntRectangle(frame(r.min()), frame(r.max()));
    return v;
}
inline std::vector<IntTriangle> euroNightQueryTriangles(int n) {
    const auto frame = detail::euroNightFrame();
    auto v = queryTriangles(n);
    for (auto& t : v) t = IntTriangle(frame(t[0]), frame(t[1]), frame(t[2]));
    return v;
}

// A point dataset and the queries asked of it, for the categories that run over
// a set of points: each such driver sweeps every entry of pointDatasets() over
// the same size lists. `centre` is what sort by angle sorts around.
struct PointDataset {
    const char* name;
    std::vector<IntPoint> (*points)(int n);
    std::vector<IntPoint> (*queryPoints)(int n);
    std::vector<IntRectangle> (*queryRectangles)(int n);
    std::vector<IntTriangle> (*queryTriangles)(int n);
    IntPoint (*centre)();
};

inline const std::vector<PointDataset>& pointDatasets() {
    static const std::vector<PointDataset> datasets = {
        {"random", randomPoints, queryPoints, queryRectangles, queryTriangles,
         [] { return IntPoint(0, 0); }},
        {"euro-night", euroNightPoints, euroNightQueryPoints, euroNightQueryRectangles,
         euroNightQueryTriangles, [] { return detail::euroNightFrame().centre; }},
    };
    return datasets;
}

// A polygon's or a region's boundary as independent segments.
template <class Shape>
std::vector<IntSegment> edgesOf(const Shape& polygon) {
    std::vector<IntSegment> edges;
    for (const auto& e : polygon.edges()) {
        edges.emplace_back(e[0], e[1]);
    }
    return edges;
}

// The polygon's boundary as independent segments — a dataset of n segments that,
// unlike smallSegments/largeSegments, has no crossings at all.
inline std::vector<IntSegment> polygonEdges(int m) {
    return edgesOf(randomPolygon(m));
}

// ---------------------------------------------------------------------------
// The SBPD polygons
// ---------------------------------------------------------------------------

// The SBPD datasets of simple polygons, and of polygons with holes. Their names
// are also their files' suffixes.
inline constexpr const char* kSbpdDatasets[] = {"fpg", "spg"};
inline constexpr const char* kSbpdRegionDatasets[] = {"fpg-holes"};

namespace detail {

// One file's polygons, keyed by total vertex count, each a list of rings with
// the outer ring first. A file of simple polygons writes each as `polygon <n>`
// and its vertices; a file of polygons with holes writes `region <n>`, then
// `ring <m>` and its vertices for every ring. Read once per process, and found
// next to this header, as euro-night is.
using SbpdRings = std::vector<std::vector<IntPoint>>;

inline const std::map<int, SbpdRings>& sbpdFile(std::string_view name) {
    static std::map<std::string, std::map<int, SbpdRings>, std::less<>> files;
    if (const auto it = files.find(name); it != files.end()) {
        return it->second;
    }
    const auto path = std::filesystem::path(__FILE__).parent_path() / "data" /
                      ("sbpd-" + std::string(name) + ".polygons");
    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot read the SBPD dataset at " << path << "\n";
        std::exit(3);
    }
    std::map<int, SbpdRings> polygons;
    SbpdRings* current = nullptr;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string keyword;
        int n = 0, x = 0, y = 0;
        if (line.starts_with("polygon") && fields >> keyword >> n) {
            current = &polygons[n];
            current->emplace_back();
        } else if (line.starts_with("region") && fields >> keyword >> n) {
            current = &polygons[n];
        } else if (line.starts_with("ring") && current != nullptr && fields >> keyword >> n) {
            current->emplace_back();
        } else if (current != nullptr && !current->empty() && fields >> x >> y) {
            current->back().emplace_back(x, y);
        } else {
            std::cerr << "malformed line in " << path << ": " << line << "\n";
            std::exit(3);
        }
    }
    for (const auto& [n, rings] : polygons) {
        std::size_t total = 0;
        for (const auto& ring : rings) total += ring.size();
        if (static_cast<int>(total) != n) {
            std::cerr << path << ": polygon " << n << " has " << total << " vertices\n";
            std::exit(3);
        }
    }
    return files.emplace(std::string(name), std::move(polygons)).first->second;
}

inline const SbpdRings& sbpdRings(std::string_view name, int n) {
    const auto& polygons = sbpdFile(name);
    const auto it = polygons.find(n);
    if (it == polygons.end()) {
        std::cerr << "the SBPD dataset " << name << " has no polygon of " << n
                  << " vertices\n";
        std::exit(3);
    }
    return it->second;
}

// The map (x, y) -> (cx - (y - cy), cy + (x - cx)), a quarter turn
// counterclockwise about the lattice point nearest the box's centre.
inline auto quarterTurnAbout(const IntRectangle& box) {
    const int cx = (box.min().x() + box.max().x()) / 2;
    const int cy = (box.min().y() + box.max().y()) / 2;
    return [cx, cy](const IntPolygon& polygon) {
        std::vector<IntPoint> turned;
        turned.reserve(polygon.vertices().size());
        for (const auto& p : polygon.vertices()) {
            turned.emplace_back(cx - (p.y() - cy), cy + (p.x() - cx));
        }
        return IntPolygon(turned);
    };
}

}  // namespace detail

// `sizes` snapped to the sizes the dataset's file holds: each to the nearest,
// the smaller on a tie, with repeats dropped. data/sbpd.py snaps the same way
// when it chooses which polygons to keep.
inline std::vector<int> sbpdSizes(std::string_view name, const std::vector<int>& sizes) {
    const auto& polygons = detail::sbpdFile(name);
    std::vector<int> snapped;
    for (const int n : sizes) {
        auto above = polygons.lower_bound(n);
        int best = above == polygons.end() ? std::prev(above)->first : above->first;
        if (above != polygons.begin() && n - std::prev(above)->first <= best - n) {
            best = std::prev(above)->first;
        }
        if (snapped.empty() || snapped.back() != best) {
            snapped.push_back(best);
        }
    }
    return snapped;
}

// The dataset's polygon of exactly n vertices, which must be one of its sizes.
// Rounding the database's coordinates to integers could in principle make a
// ring touch itself, so each is checked simple here, outside any timed region.
inline IntPolygon sbpdPolygon(std::string_view name, int n) {
    const auto& rings = detail::sbpdRings(name, n);
    if (rings.size() != 1) {
        std::cerr << "the SBPD polygon " << name << " " << n << " has holes\n";
        std::exit(3);
    }
    IntPolygon polygon(rings.front());
    if (static_cast<int>(polygon.vertices().size()) != n || !polygon.isSimple()) {
        std::cerr << "the SBPD polygon " << name << " " << n << " is not simple\n";
        std::exit(3);
    }
    return polygon;
}

// The dataset's polygon with holes of exactly n vertices in all. Checked the
// same way: no two of its edges meet except consecutive ones of a ring at their
// shared vertex, which is exactly n meeting pairs, so every ring is simple and
// the rings are pairwise disjoint; a hole cannot have left the outer ring
// without crossing it.
inline IntRegion sbpdRegion(std::string_view name, int n) {
    const auto& rings = detail::sbpdRings(name, n);
    std::vector<IntPolygon> holes(rings.begin() + 1, rings.end());
    IntRegion region(IntPolygon(rings.front()), holes);
    if (static_cast<int>(region.vertexCount()) != n ||
        region.holes().size() != holes.size() ||
        static_cast<int>(pgl::findIntersections(edgesOf(region)).size()) != n) {
        std::cerr << "the SBPD region " << name << " " << n << " is not a valid region\n";
        std::exit(3);
    }
    return region;
}

// The polygon turned a quarter turn counterclockwise about the lattice point
// nearest its bounding box's centre: the second operand of the categories that
// take two polygons, since the database has only one polygon of each size. It
// covers the same box, so the two overlap as heavily as two independent draws
// would, and the turn is exact on integers.
inline IntPolygon quarterTurn(const IntPolygon& polygon) {
    return detail::quarterTurnAbout(polygon.bbox())(polygon);
}

// The region turned the same way, every ring about the outer ring's centre.
inline IntRegion quarterTurn(const IntRegion& region) {
    const auto turn = detail::quarterTurnAbout(region.bbox());
    std::vector<IntPolygon> holes;
    for (const auto& hole : region.holes()) holes.push_back(turn(hole));
    return IntRegion(turn(region.outer()), holes);
}

}  // namespace bench
