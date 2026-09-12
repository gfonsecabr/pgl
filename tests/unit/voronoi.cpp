#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "pgl.hpp"

namespace {

using Site = pgl::Point<int>;
using Diagram = pgl::Arrangement<pgl::EPoint, Site>;

Site P(int x, int y) {
    return Site(x, y);
}

pgl::EPoint exact(const Site& point) {
    return pgl::EPoint(point);
}

std::set<Site> sitesAround(const Diagram& diagram, Diagram::VertexId vertex) {
    std::set<Site> result;
    for (const Diagram::HalfedgeId h : diagram.outgoingHalfedges(vertex)) {
        result.insert(diagram.label(diagram.face(h)));
    }
    return result;
}

// The free voronoiDiagram labels a face with every site that owns it, so its
// label type is a vector however small the order is.
using WeightedSite = pgl::Disk<Site>;
using OrderDiagram = pgl::Arrangement<pgl::EPoint, std::vector<Site>>;
using PowerDiagram = pgl::Arrangement<pgl::EPoint, std::vector<WeightedSite>>;

// The ordinary entry points label a face with the one site that owns it, so
// their diagrams carry the element itself: `Diagram` above for points, this for
// disks.
using PowerCells = pgl::Arrangement<pgl::EPoint, WeightedSite>;

WeightedSite D(int x, int y, int radius) {
    return WeightedSite(P(x, y), radius);
}

pgl::ERational powerDistance(const Site& site, const pgl::EPoint& query) {
    return query.squaredDistance<pgl::ERational>(exact(site));
}

pgl::ERational powerDistance(const WeightedSite& site, const pgl::EPoint& query) {
    return query.squaredDistance<pgl::ERational>(pgl::EPoint(site.center<pgl::ERational>())) -
           site.squaredRadius<pgl::ERational>();
}

// The k sites of smallest power distance, or nothing when the k-th and the
// (k+1)-th tie and the answer is therefore not the label of any one face.
template <class Element>
std::optional<std::vector<Element>> nearestSites(const std::vector<Element>& elements,
                                                 const pgl::EPoint& query, int k) {
    std::vector<std::pair<pgl::ERational, std::size_t>> ranked;
    for (std::size_t i = 0; i < elements.size(); ++i) {
        ranked.emplace_back(powerDistance(elements[i], query), i);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        return left.first != right.first ? left.first < right.first : left.second < right.second;
    });
    const std::size_t order = static_cast<std::size_t>(k);
    if (ranked.size() > order && ranked[order - 1].first == ranked[order].first) {
        return std::nullopt;
    }
    std::vector<std::size_t> chosen;
    for (std::size_t i = 0; i < order; ++i) {
        chosen.push_back(ranked[i].second);
    }
    std::sort(chosen.begin(), chosen.end());
    std::vector<Element> result;
    for (const std::size_t i : chosen) {
        result.push_back(elements[i]);
    }
    return result;
}

// Whichever of the two entry points takes this kind of site.
template <class Element>
auto diagramOf(const std::vector<Element>& elements, int k) {
    if constexpr (pgl::PointConcept<Element>) {
        return pgl::voronoiDiagram(elements, k);
    } else {
        return pgl::powerDiagram(elements, k);
    }
}

// Checks the diagram against the definition at half-integer queries, which the
// integer sites never tie at by more than the diagram itself records.
template <class Element>
void checkAgainstDefinition(const std::vector<Element>& elements, int k, int extent) {
    auto diagram = diagramOf(elements, k);
    diagram.buildPointLocation();
    const pgl::ERational half(1, 2);
    for (int x = -extent; x <= extent; ++x) {
        for (int y = -extent; y <= extent; ++y) {
            const pgl::EPoint query(pgl::ERational(x) + half, pgl::ERational(y) + half);
            const auto expected = nearestSites(elements, query, k);
            if (!expected) {
                continue;
            }
            REQUIRE(diagram.label(diagram.locateFace(query)) == *expected);
        }
    }
}

// The site farthest from the query, or nothing when the two farthest tie and
// the answer is therefore not the label of any one face.
std::optional<Site> farthestSite(const std::vector<Site>& sites, const pgl::EPoint& query) {
    std::vector<std::pair<pgl::ERational, std::size_t>> ranked;
    for (std::size_t i = 0; i < sites.size(); ++i) {
        ranked.emplace_back(powerDistance(sites[i], query), i);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        return left.first != right.first ? right.first < left.first : left.second < right.second;
    });
    if (ranked.size() > 1 && ranked[0].first == ranked[1].first) {
        return std::nullopt;
    }
    return sites[ranked[0].second];
}

// Checks the farthest-point diagram against the definition at half-integer
// queries, which the integer sites never tie at by more than the diagram itself
// records.
void checkFarthestAgainstDefinition(const std::vector<Site>& sites, int extent) {
    Diagram diagram = pgl::farthestVoronoiDiagram(sites);
    diagram.buildPointLocation();
    const pgl::ERational half(1, 2);
    for (int x = -extent; x <= extent; ++x) {
        for (int y = -extent; y <= extent; ++y) {
            const pgl::EPoint query(pgl::ERational(x) + half, pgl::ERational(y) + half);
            const auto expected = farthestSite(sites, query);
            if (!expected) {
                continue;
            }
            REQUIRE(diagram.label(diagram.locateFace(query)) == *expected);
        }
    }
}

}  // namespace

TEST_CASE("Triangulation Voronoi diagram labels every face with its site") {
    const std::vector<Site> sites{
        P(0, 0), P(12, 0), P(12, 12), P(0, 12), P(6, 6),
    };
    const pgl::Triangulation triangulation(sites);
    const Diagram diagram = triangulation.voronoiDiagram();

    static_assert(std::same_as<typename Diagram::PointType, pgl::EPoint>);
    static_assert(std::same_as<typename Diagram::LabelType, Site>);
    CHECK(diagram.faceCount() == sites.size());
    CHECK_FALSE(diagram.hasPointLocation());

    for (const Site& site : sites) {
        const Diagram::FaceId face = diagram.locateFace(exact(site));
        CHECK(diagram.label(face) == site);
    }

    const Diagram::FaceId centerFace = diagram.locateFace(exact(P(6, 6)));
    CHECK_FALSE(diagram.isUnbounded(centerFace));
    CHECK(diagram.label(centerFace) == P(6, 6));
}

TEST_CASE("A triangular Delaunay triangulation dualizes to three rays") {
    const std::vector<Site> sites{P(0, 0), P(6, 0), P(0, 6)};
    const pgl::Triangulation triangulation(sites);
    Diagram diagram = triangulation.voronoiDiagram();

    CHECK(diagram.vertexCount() == 1);
    CHECK(diagram.edgeCount() == 3);
    CHECK(diagram.faceCount() == 3);
    CHECK(diagram.isUnbounded());
    CHECK(diagram.vertices().front() == pgl::EPoint(3, 3));

    for (const Site& site : sites) {
        CHECK(diagram.label(diagram.locateFace(exact(site))) == site);
    }

    const Diagram::CellId center = diagram.locateCell(pgl::EPoint(3, 3));
    REQUIRE(std::holds_alternative<Diagram::VertexId>(center));
    CHECK(sitesAround(diagram, std::get<Diagram::VertexId>(center)) ==
          std::set<Site>(sites.begin(), sites.end()));

    diagram.buildPointLocation();
    CHECK(diagram.hasPointLocation());
    for (const Site& site : sites) {
        CHECK(diagram.label(diagram.locateFace(exact(site))) == site);
    }
}

TEST_CASE("Cocircular Delaunay triangles share one Voronoi vertex") {
    const std::vector<Site> sites{P(0, 0), P(8, 0), P(8, 8), P(0, 8)};
    const pgl::Triangulation triangulation(sites);
    const Diagram diagram = triangulation.voronoiDiagram();

    // The chosen Delaunay diagonal has coincident circumcenters and therefore
    // contributes no spurious zero-length Arrangement edge.
    CHECK(diagram.vertexCount() == 1);
    CHECK(diagram.edgeCount() == 4);
    CHECK(diagram.faceCount() == 4);
    REQUIRE(diagram.vertices().front() == pgl::EPoint(4, 4));

    const Diagram::CellId center = diagram.locateCell(pgl::EPoint(4, 4));
    REQUIRE(std::holds_alternative<Diagram::VertexId>(center));
    CHECK(sitesAround(diagram, std::get<Diagram::VertexId>(center)) ==
          std::set<Site>(sites.begin(), sites.end()));
    for (const Site& site : sites) {
        CHECK(diagram.label(diagram.locateFace(exact(site))) == site);
    }
}

TEST_CASE("Every Voronoi edge is equidistant from its adjacent face sites") {
    const std::vector<Site> sites{
        P(0, 0), P(11, 1), P(13, 10), P(7, 15), P(-2, 9), P(4, 6),
    };
    const pgl::Triangulation triangulation(sites);
    const Diagram diagram = triangulation.voronoiDiagram();

    for (std::uint32_t i = 0; i < diagram.halfedgeCount(); i += 2) {
        const Diagram::HalfedgeId h(i);
        const Site& left = diagram.label(diagram.face(h));
        const Site& right = diagram.label(diagram.face(diagram.twin(h)));
        const pgl::EPoint witness = diagram.witness(h);
        CHECK(left != right);
        CHECK(witness.squaredDistance<pgl::ERational>(exact(left)) ==
              witness.squaredDistance<pgl::ERational>(exact(right)));
    }
}

TEST_CASE("voronoiDiagram of points reproduces the Delaunay dual") {
    const std::vector<Site> sites{
        P(0, 0), P(11, 1), P(13, 10), P(7, 15), P(-2, 9), P(4, 6),
    };
    const pgl::Triangulation triangulation(sites);
    const Diagram dual = triangulation.voronoiDiagram();
    const Diagram ordinary = pgl::voronoiDiagram(sites);
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 1);

    // The no-order call labels a face with the site itself and is the member's
    // own dual; the order-k call at k = 1 reaches the same diagram by the
    // refinement route, and labels by the vector every order shares.
    static_assert(std::same_as<typename Diagram::LabelType, Site>);
    static_assert(std::same_as<typename OrderDiagram::PointType, pgl::EPoint>);
    static_assert(std::same_as<typename OrderDiagram::LabelType, std::vector<Site>>);
    CHECK(ordinary.vertices() == dual.vertices());
    CHECK(ordinary.edgeCount() == dual.edgeCount());
    CHECK(ordinary.faceCount() == dual.faceCount());
    CHECK(diagram.vertexCount() == dual.vertexCount());
    CHECK(diagram.edgeCount() == dual.edgeCount());
    CHECK(diagram.faceCount() == dual.faceCount());
    CHECK(diagram.vertices() == dual.vertices());
    CHECK_FALSE(diagram.hasPointLocation());
    CHECK_FALSE(ordinary.hasPointLocation());

    for (const Site& site : sites) {
        CHECK(ordinary.label(ordinary.locateFace(exact(site))) == site);
        CHECK(diagram.label(diagram.locateFace(exact(site))) == std::vector<Site>{site});
        CHECK(diagram.label(diagram.locateFace(exact(site))).front() ==
              dual.label(dual.locateFace(exact(site))));
    }
}

TEST_CASE("The Delaunay dual is already an arrangement") {
    // Both Voronoi entry points overlay the dual edges under the promise that
    // they meet only at shared endpoints, which is what lets the construction
    // skip its splitting step. Two dual edges have different nearest pairs
    // throughout their relative interiors, so the promise holds; check it
    // against the general overlay on the inputs most likely to break it, a
    // lattice whose cocircular sites collapse dual edges to nothing and whose
    // cells share long collinear boundaries.
    std::vector<std::vector<Site>> inputs{
        {P(-2, -1), P(2, -1), P(2, 1), P(-2, 1), P(-1, -2), P(1, -2), P(1, 2), P(-1, 2)},
        {P(0, 0), P(11, 1), P(13, 10), P(7, 15), P(-2, 9), P(4, 6)},
    };
    for (int width = 2; width <= 5; ++width) {
        std::vector<Site> grid;
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < width + 1; ++y) {
                grid.push_back(P(3 * x, 3 * y));
            }
        }
        inputs.push_back(std::move(grid));
    }

    for (const std::vector<Site>& sites : inputs) {
        const pgl::Triangulation triangulation(sites);
        const auto edges = triangulation.voronoiEdges();
        const pgl::Arrangement<pgl::EPoint> split(edges);
        const Diagram dual = triangulation.voronoiDiagram();

        CHECK(dual.vertexCount() == split.vertexCount());
        CHECK(dual.edgeCount() == split.edgeCount());
        CHECK(dual.faceCount() == split.faceCount());
        CHECK(dual.halfedgeCount() == split.halfedgeCount());
        CHECK(dual.vertices() == split.vertices());
        CHECK(dual.faceCount() == sites.size());
    }
}

TEST_CASE("The ordinary diagram labels faces with the caller's own elements") {
    // The no-order call dualizes a triangulation built from the caller's points
    // rather than from copies of their coordinates, which is what lets a face
    // carry the element as handed over — a point's label included. A label is
    // metadata that equality, ordering and hashing all ignore, so it reaches no
    // predicate the triangulation runs and simply rides along.
    using Tagged = pgl::Point<int, std::string>;
    const std::vector<Tagged> sites{
        Tagged(0, 0, "sw"), Tagged(12, 0, "se"), Tagged(12, 12, "ne"),
        Tagged(0, 12, "nw"), Tagged(6, 6, "middle"),
    };
    const auto diagram = pgl::voronoiDiagram(sites);
    static_assert(std::same_as<typename decltype(diagram)::LabelType, Tagged>);

    REQUIRE(diagram.faceCount() == sites.size());
    for (const Tagged& site : sites) {
        const auto face = diagram.locateFace(pgl::EPoint(site.x(), site.y()));
        CHECK(diagram.label(face) == site);
        CHECK(diagram.label(face).label() == site.label());
    }
}

TEST_CASE("The dual of a non-Delaunay triangulation is a circumcentric dual") {
    // A constraint edge is how a triangulation stops being Delaunay, and the
    // dual then has edges that genuinely cross — so the promise the Delaunay
    // dual is overlaid under does not hold, and the diagram has to be cut
    // against itself like any other set of curves. What comes back is a real
    // subdivision of the plane: the same one the general overlay of the same
    // edges produces, and one the point-location index can be built over.
    const std::vector<Site> quad{P(10, 10), P(50, 0), P(90, 10), P(50, 20)};
    const std::vector<pgl::Segment<Site>> longDiagonal{
        pgl::Segment<Site>(quad[0], quad[2])};
    const pgl::Triangulation constrained(quad, longDiagonal);
    const pgl::Triangulation plain(quad);

    // The long diagonal is the one Delaunay rejects, so forcing it is what makes
    // the difference: the same four sites, two triangulations, two duals.
    const Diagram dual = constrained.voronoiDiagram();
    const pgl::Arrangement<pgl::EPoint> split(constrained.voronoiEdges());
    CHECK(dual.vertexCount() == split.vertexCount());
    CHECK(dual.edgeCount() == split.edgeCount());
    CHECK(dual.faceCount() == split.faceCount());
    CHECK(dual.vertices() == split.vertices());

    // The Delaunay dual of the same sites is the Voronoi diagram, one face per
    // site; the circumcentric dual is a different subdivision with more.
    CHECK(plain.voronoiDiagram().faceCount() == quad.size());
    CHECK(dual.faceCount() > quad.size());

    // A structure this consistent can be indexed, which is the part that fails
    // outright when crossing edges are overlaid as though they were disjoint.
    Diagram indexed = dual;
    indexed.buildPointLocation();
    CHECK(indexed.hasPointLocation());

    // What is left of the face labels outside the Delaunay precondition: a face
    // carries a site that falls inside it. Not necessarily the one asked after,
    // because the circumcentric dual no longer keeps the sites in faces of their
    // own — here two of the four share one — and faces no site falls in keep the
    // default.
    for (const Site& site : quad) {
        const Diagram::FaceId face = indexed.locateFace(exact(site));
        CHECK(indexed.locateFace(exact(indexed.label(face))) == face);
    }
}

TEST_CASE("voronoiDiagram of collinear points has no Delaunay dual to borrow") {
    // No triangle to dualize, so both overloads take the bisector route instead.
    const std::vector<Site> sites{P(0, 0), P(4, 0), P(10, 0)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 1);
    const Diagram ordinary = pgl::voronoiDiagram(sites);

    CHECK(diagram.vertexCount() == 0);
    CHECK(diagram.edgeCount() == 2);
    CHECK(diagram.faceCount() == 3);
    CHECK(ordinary.vertexCount() == 0);
    CHECK(ordinary.edgeCount() == 2);
    CHECK(ordinary.faceCount() == 3);
    for (const Site& site : sites) {
        CHECK(ordinary.label(ordinary.locateFace(exact(site))) == site);
    }
    checkAgainstDefinition(sites, 1, 8);
}

TEST_CASE("voronoiDiagram of one site is the whole plane") {
    const std::vector<Site> sites{P(3, -2)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 1);
    const Diagram ordinary = pgl::voronoiDiagram(sites);

    CHECK(diagram.edgeCount() == 0);
    REQUIRE(diagram.faceCount() == 1);
    CHECK(diagram.label(OrderDiagram::FaceId(0)) == sites);
    REQUIRE(ordinary.faceCount() == 1);
    CHECK(ordinary.label(Diagram::FaceId(0)) == sites.front());

    const std::vector<WeightedSite> disk{D(3, -2, 5)};
    const PowerDiagram powerCell = pgl::powerDiagram(disk, 1);
    REQUIRE(powerCell.faceCount() == 1);
    CHECK(powerCell.label(PowerDiagram::FaceId(0)) == disk);
    const PowerCells ordinaryCell = pgl::powerDiagram(disk);
    static_assert(std::same_as<typename PowerCells::LabelType, WeightedSite>);
    REQUIRE(ordinaryCell.faceCount() == 1);
    CHECK(ordinaryCell.label(PowerCells::FaceId(0)) == disk.front());
}

TEST_CASE("powerDiagram weighs each site by its squared radius") {
    // The power bisector of two disks is their radical axis, which the heavier
    // disk pushes past the midpoint towards the lighter one: 16 of squared
    // radius moves it from x = 5 to x = 29/5, so the heavier disk owns more.
    const std::vector<WeightedSite> disks{D(0, 0, 4), D(10, 0, 0)};
    const PowerDiagram diagram = pgl::powerDiagram(disks, 1);

    REQUIRE(diagram.edgeCount() == 1);
    CHECK(diagram.faceCount() == 2);
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(5, 0))) ==
          std::vector<WeightedSite>{disks[0]});
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(6, 0))) ==
          std::vector<WeightedSite>{disks[1]});

    const PowerCells cells = pgl::powerDiagram(disks);
    CHECK(cells.edgeCount() == diagram.edgeCount());
    CHECK(cells.faceCount() == diagram.faceCount());
    CHECK(cells.label(cells.locateFace(pgl::EPoint(5, 0))) == disks[0]);
    CHECK(cells.label(cells.locateFace(pgl::EPoint(6, 0))) == disks[1]);

    const auto edge = diagram.edges().front();
    REQUIRE(std::holds_alternative<PowerDiagram::LineType>(edge));
    const PowerDiagram::LineType& axis = std::get<PowerDiagram::LineType>(edge);
    const pgl::ERational abscissa(29, 5);
    CHECK(axis.intersects(pgl::EPoint(abscissa, pgl::ERational(0))));
    CHECK(axis.intersects(pgl::EPoint(abscissa, pgl::ERational(7))));
    checkAgainstDefinition(disks, 1, 12);
}

TEST_CASE("A disk its neighbours swallow owns no power cell") {
    // The two heavy disks meet on x = 10, right over the light one's center, so
    // the light one is nearest nowhere and the two cells cover the plane.
    const std::vector<WeightedSite> disks{D(0, 0, 12), D(10, 0, 0), D(20, 0, 12)};
    const PowerDiagram diagram = pgl::powerDiagram(disks, 1);

    const PowerCells cells = pgl::powerDiagram(disks);
    REQUIRE(cells.faceCount() == 2);
    CHECK(cells.label(cells.locateFace(pgl::EPoint(9, 0))) == disks[0]);
    CHECK(cells.label(cells.locateFace(pgl::EPoint(11, 0))) == disks[2]);

    REQUIRE(diagram.edgeCount() == 1);
    REQUIRE(diagram.faceCount() == 2);
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(9, 0))) ==
          std::vector<WeightedSite>{disks[0]});
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(11, 0))) ==
          std::vector<WeightedSite>{disks[2]});
    checkAgainstDefinition(disks, 1, 14);
}

TEST_CASE("powerDiagram of radius-zero disks is the Voronoi diagram of their centers") {
    const std::vector<Site> sites{P(0, 0), P(9, 2), P(3, 11), P(-5, 6)};
    std::vector<WeightedSite> disks;
    for (const Site& site : sites) {
        disks.emplace_back(site, 0);
    }
    const OrderDiagram fromPoints = pgl::voronoiDiagram(sites, 1);
    const PowerDiagram fromDisks = pgl::powerDiagram(disks, 1);

    CHECK(fromDisks.vertexCount() == fromPoints.vertexCount());
    CHECK(fromDisks.edgeCount() == fromPoints.edgeCount());
    CHECK(fromDisks.faceCount() == fromPoints.faceCount());
    // The two routes number the vertices differently, so compare the sets.
    CHECK(std::set<pgl::EPoint>(fromDisks.vertices().begin(), fromDisks.vertices().end()) ==
          std::set<pgl::EPoint>(fromPoints.vertices().begin(), fromPoints.vertices().end()));
}

TEST_CASE("The order-2 diagram of a triangle labels each face with a pair") {
    const std::vector<Site> sites{P(0, 0), P(12, 0), P(0, 12)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 2);

    CHECK(diagram.vertexCount() == 1);
    CHECK(diagram.vertices().front() == pgl::EPoint(6, 6));
    CHECK(diagram.faceCount() == 3);

    // Each order-2 cell is the one across the circumcenter from the site it
    // leaves out, and every cell of this diagram is unbounded.
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(20, 20))) ==
          std::vector<Site>{sites[1], sites[2]});
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(-20, 6))) ==
          std::vector<Site>{sites[0], sites[2]});
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(6, -20))) ==
          std::vector<Site>{sites[0], sites[1]});
    for (std::size_t i = 0; i < diagram.faceCount(); ++i) {
        CHECK(diagram.isUnbounded(OrderDiagram::FaceId(static_cast<std::uint32_t>(i))));
    }
}

TEST_CASE("The order-n diagram is the whole plane, owned by every site") {
    const std::vector<Site> sites{P(0, 0), P(7, 1), P(2, 9)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 3);

    CHECK(diagram.edgeCount() == 0);
    REQUIRE(diagram.faceCount() == 1);
    CHECK(diagram.label(OrderDiagram::FaceId(0)) == sites);
}

TEST_CASE("Order-k faces agree with the k nearest sites everywhere") {
    const std::vector<Site> sites{
        P(-6, -4), P(5, -7), P(9, 3), P(1, 8), P(-8, 5), P(0, 0),
    };
    for (int k = 1; k <= 4; ++k) {
        CAPTURE(k);
        checkAgainstDefinition(sites, k, 6);
    }

    const std::vector<WeightedSite> disks{
        D(-6, -4, 3), D(5, -7, 0), D(9, 3, 6), D(1, 8, 2), D(-8, 5, 5), D(0, 0, 4),
    };
    for (int k = 1; k <= 4; ++k) {
        CAPTURE(k);
        checkAgainstDefinition(disks, k, 6);
    }
}

TEST_CASE("Order-k edges cross where four sites share a circle") {
    // The first four sites are on the circle of squared radius 5 around
    // (2, -3), and the fifth is nearer to that point than any of them. At order
    // 3 the bisectors of two of the cocircular pairs are both edges of the
    // diagram and they cross there rather than ending: walking along either
    // one, a site of the other pair takes over from its partner at exactly that
    // point and the number of sites nearer never changes. The crossing is a
    // vertex of the diagram all the same, and one of degree four.
    const std::vector<Site> sites{P(0, -4), P(3, -1), P(4, -4), P(1, -1), P(3, -4)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 3);

    const auto crossing =
        std::find(diagram.vertices().begin(), diagram.vertices().end(), pgl::EPoint(2, -3));
    REQUIRE(crossing != diagram.vertices().end());
    const auto index = static_cast<std::uint32_t>(crossing - diagram.vertices().begin());
    CHECK(diagram.degree(OrderDiagram::VertexId(index)) == 4);
    checkAgainstDefinition(sites, 3, 8);
}

TEST_CASE("The refinement and the bisector construction build the same diagram") {
    // A grid is as degenerate as a point set gets -- the corners of every
    // square in it share a circle -- and the two routes to the order-k diagram
    // have nothing in common but the answer. Points refine one order into the
    // next from the Delaunay dual down; the same points as radius-zero disks
    // have no refinement to take and cut every bisector against every site.
    std::vector<Site> sites;
    std::vector<WeightedSite> disks;
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            sites.push_back(P(x, y));
            disks.push_back(D(x, y, 0));
        }
    }
    for (int k = 1; k <= 4; ++k) {
        CAPTURE(k);
        const OrderDiagram refined = pgl::voronoiDiagram(sites, k);
        const PowerDiagram cut = pgl::powerDiagram(disks, k);
        CHECK(refined.vertexCount() == cut.vertexCount());
        CHECK(refined.edgeCount() == cut.edgeCount());
        CHECK(refined.faceCount() == cut.faceCount());
        CHECK(std::set<pgl::EPoint>(refined.vertices().begin(), refined.vertices().end()) ==
              std::set<pgl::EPoint>(cut.vertices().begin(), cut.vertices().end()));
    }
}

TEST_CASE("Collinear sites have no refinement to take at any order") {
    const std::vector<Site> sites{P(0, 0), P(3, 0), P(7, 0), P(12, 0)};
    for (int k = 1; k <= 3; ++k) {
        CAPTURE(k);
        const OrderDiagram diagram = pgl::voronoiDiagram(sites, k);
        // The cells are the slabs between consecutive bisectors, one fewer at
        // each order.
        CHECK(diagram.vertexCount() == 0);
        CHECK(diagram.faceCount() == sites.size() - static_cast<std::size_t>(k) + 1);
        checkAgainstDefinition(sites, k, 12);
    }
}

TEST_CASE("Neighbouring order-k cells differ by a single site") {
    const std::vector<Site> sites{
        P(0, 0), P(10, 0), P(10, 10), P(0, 10), P(5, 4), P(-7, 3),
    };
    const OrderDiagram diagram = pgl::voronoiDiagram(sites, 3);

    for (std::uint32_t i = 0; i < diagram.halfedgeCount(); i += 2) {
        const OrderDiagram::HalfedgeId h(i);
        const std::vector<Site>& left = diagram.label(diagram.face(h));
        const std::vector<Site>& right = diagram.label(diagram.face(diagram.twin(h)));
        REQUIRE(left.size() == 3);
        REQUIRE(right.size() == 3);
        // A label is ordered by the site's position in the input, so reorder
        // before taking differences.
        const std::set<Site> leftSet(left.begin(), left.end());
        const std::set<Site> rightSet(right.begin(), right.end());
        std::vector<Site> swapped;
        std::set_symmetric_difference(leftSet.begin(), leftSet.end(), rightSet.begin(),
                                      rightSet.end(), std::back_inserter(swapped));
        REQUIRE(swapped.size() == 2);

        // The two sites that are swapped are the ones the edge bisects.
        const pgl::EPoint witness = diagram.witness(h);
        CHECK(witness.squaredDistance<pgl::ERational>(exact(swapped[0])) ==
              witness.squaredDistance<pgl::ERational>(exact(swapped[1])));
    }
}

TEST_CASE("Both entry points reject an order no set of sites can have") {
    const std::vector<Site> sites{P(0, 0), P(4, 4)};
    CHECK_THROWS_AS((void)pgl::voronoiDiagram(sites, 0), std::invalid_argument);
    CHECK_THROWS_AS((void)pgl::voronoiDiagram(sites, 3), std::invalid_argument);
    CHECK_THROWS_AS((void)pgl::voronoiDiagram(std::vector<Site>{}), std::invalid_argument);

    const std::vector<WeightedSite> disks{D(0, 0, 1), D(4, 4, 2)};
    CHECK_THROWS_AS((void)pgl::powerDiagram(disks, 0), std::invalid_argument);
    CHECK_THROWS_AS((void)pgl::powerDiagram(disks, 3), std::invalid_argument);
    CHECK_THROWS_AS((void)pgl::powerDiagram(std::vector<WeightedSite>{}), std::invalid_argument);
}

TEST_CASE("farthestVoronoiDiagram labels every face with the site farthest from it") {
    const std::vector<Site> sites{
        P(0, 0), P(11, 1), P(13, 10), P(7, 15), P(-2, 9), P(4, 6),
    };
    const Diagram diagram = pgl::farthestVoronoiDiagram(sites);

    // The conventions are the ordinary diagram's: the caller's own element on
    // every face, exact vertices, and no point-location index until asked for.
    static_assert(std::same_as<typename Diagram::PointType, pgl::EPoint>);
    static_assert(std::same_as<typename Diagram::LabelType, Site>);
    CHECK_FALSE(diagram.hasPointLocation());

    // P(4, 6) is inside the hull of the others, so it is farthest from nowhere
    // and labels no face.
    CHECK(diagram.faceCount() == sites.size() - 1);
    checkFarthestAgainstDefinition(sites, 16);
}

TEST_CASE("Every farthest-point cell is unbounded and owned by a hull vertex") {
    // The diagram is a tree of segments and rays: one cell per hull vertex, no
    // bounded face anywhere, and an edge of it equidistant from the two sites
    // whose cells it separates.
    const std::vector<Site> sites{
        P(0, 0), P(10, 0), P(10, 10), P(0, 10), P(5, 4), P(3, 7),
    };
    const Diagram diagram = pgl::farthestVoronoiDiagram(sites);
    const std::vector<Site> hull = pgl::convexHull(sites);

    REQUIRE(diagram.faceCount() == hull.size());
    std::set<Site> owners;
    for (std::size_t f = 0; f < diagram.faceCount(); ++f) {
        const Diagram::FaceId face(static_cast<std::uint32_t>(f));
        CHECK(diagram.isUnbounded(face));
        owners.insert(diagram.label(face));
    }
    CHECK(owners == std::set<Site>(hull.begin(), hull.end()));

    for (std::uint32_t i = 0; i < diagram.halfedgeCount(); i += 2) {
        const Diagram::HalfedgeId h(i);
        const Site& left = diagram.label(diagram.face(h));
        const Site& right = diagram.label(diagram.face(diagram.twin(h)));
        const pgl::EPoint witness = diagram.witness(h);
        CHECK(left != right);
        CHECK(witness.squaredDistance<pgl::ERational>(exact(left)) ==
              witness.squaredDistance<pgl::ERational>(exact(right)));
    }
    checkFarthestAgainstDefinition(sites, 12);
}

TEST_CASE("The farthest-point diagram of cocircular sites meets at their center") {
    const std::vector<Site> sites{P(0, 0), P(8, 0), P(8, 8), P(0, 8)};
    const Diagram diagram = pgl::farthestVoronoiDiagram(sites);

    REQUIRE(diagram.vertexCount() == 1);
    CHECK(diagram.vertices().front() == pgl::EPoint(4, 4));
    CHECK(diagram.edgeCount() == 4);
    REQUIRE(diagram.faceCount() == 4);

    // Each cell is the one across the center from the site that owns it.
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(-4, -4))) == P(8, 8));
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(12, 12))) == P(0, 0));
    checkFarthestAgainstDefinition(sites, 12);
}

TEST_CASE("Only the extremes of collinear sites are ever farthest") {
    const std::vector<Site> sites{P(0, 0), P(4, 0), P(10, 0)};
    const Diagram diagram = pgl::farthestVoronoiDiagram(sites);

    CHECK(diagram.vertexCount() == 0);
    CHECK(diagram.edgeCount() == 1);
    REQUIRE(diagram.faceCount() == 2);
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(-1, 0))) == P(10, 0));
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(11, 0))) == P(0, 0));
    checkFarthestAgainstDefinition(sites, 12);
}

TEST_CASE("The farthest-point diagram of one site is the whole plane") {
    const std::vector<Site> sites{P(3, -2)};
    const Diagram diagram = pgl::farthestVoronoiDiagram(sites);

    CHECK(diagram.edgeCount() == 0);
    REQUIRE(diagram.faceCount() == 1);
    CHECK(diagram.label(Diagram::FaceId(0)) == sites.front());

    // Repeated sites share a cell, which carries the first of them.
    const std::vector<Site> repeated{P(0, 0), P(0, 0), P(4, 0)};
    const Diagram shared = pgl::farthestVoronoiDiagram(repeated);
    REQUIRE(shared.faceCount() == 2);
    CHECK(shared.label(shared.locateFace(pgl::EPoint(-1, 0))) == P(4, 0));
    CHECK(shared.label(shared.locateFace(pgl::EPoint(5, 0))) == P(0, 0));

    CHECK_THROWS_AS((void)pgl::farthestVoronoiDiagram(std::vector<Site>{}),
                    std::invalid_argument);
}

TEST_CASE("The farthest-point diagram labels faces with the caller's own elements") {
    // The hull the sites are pruned to is built from copies the scan is free to
    // reorder and thin out, so what survives the pruning has to be the caller's
    // own element rather than whichever copy the hull kept.
    using Tagged = pgl::Point<int, std::string>;
    const std::vector<Tagged> sites{
        Tagged(0, 0, "sw"), Tagged(12, 0, "se"), Tagged(12, 12, "ne"),
        Tagged(0, 12, "nw"), Tagged(6, 6, "middle"),
    };
    const auto diagram = pgl::farthestVoronoiDiagram(sites);
    static_assert(std::same_as<typename decltype(diagram)::LabelType, Tagged>);

    REQUIRE(diagram.faceCount() == 4);
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(-1, -1))).label() == "ne");
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(13, 13))).label() == "sw");
}

TEST_CASE("The farthest-point diagram is the order-(n-1) diagram, complemented") {
    // One site is strictly farthest exactly where the other n - 1 are strictly
    // nearest, so the two are one subdivision and each label is the complement
    // of the other -- by two constructions with nothing in common but that.
    const std::vector<Site> sites{P(-6, -4), P(5, -7), P(9, 3), P(1, 8), P(-8, 5)};
    Diagram farthest = pgl::farthestVoronoiDiagram(sites);
    OrderDiagram rest = pgl::voronoiDiagram(sites, static_cast<int>(sites.size()) - 1);

    CHECK(farthest.vertexCount() == rest.vertexCount());
    CHECK(farthest.edgeCount() == rest.edgeCount());
    CHECK(farthest.faceCount() == rest.faceCount());
    CHECK(std::set<pgl::EPoint>(farthest.vertices().begin(), farthest.vertices().end()) ==
          std::set<pgl::EPoint>(rest.vertices().begin(), rest.vertices().end()));

    farthest.buildPointLocation();
    rest.buildPointLocation();
    const pgl::ERational half(1, 2);
    for (int x = -12; x <= 12; ++x) {
        for (int y = -12; y <= 12; ++y) {
            const pgl::EPoint query(pgl::ERational(x) + half, pgl::ERational(y) + half);
            const auto expected = farthestSite(sites, query);
            if (!expected) {
                continue;
            }
            REQUIRE(farthest.label(farthest.locateFace(query)) == *expected);
            std::vector<Site> others;
            for (const Site& site : sites) {
                if (site != *expected) {
                    others.push_back(site);
                }
            }
            REQUIRE(rest.label(rest.locateFace(query)) == others);
        }
    }
}
