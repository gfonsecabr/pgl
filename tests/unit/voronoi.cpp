#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
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
    const OrderDiagram diagram = pgl::voronoiDiagram(sites);

    static_assert(std::same_as<typename OrderDiagram::PointType, pgl::EPoint>);
    static_assert(std::same_as<typename OrderDiagram::LabelType, std::vector<Site>>);
    CHECK(diagram.vertexCount() == dual.vertexCount());
    CHECK(diagram.edgeCount() == dual.edgeCount());
    CHECK(diagram.faceCount() == dual.faceCount());
    CHECK(diagram.vertices() == dual.vertices());
    CHECK_FALSE(diagram.hasPointLocation());

    for (const Site& site : sites) {
        CHECK(diagram.label(diagram.locateFace(exact(site))) == std::vector<Site>{site});
        CHECK(diagram.label(diagram.locateFace(exact(site))).front() ==
              dual.label(dual.locateFace(exact(site))));
    }
}

TEST_CASE("voronoiDiagram of collinear points has no Delaunay dual to borrow") {
    // No triangle to dualize, so this takes the bisector route instead.
    const std::vector<Site> sites{P(0, 0), P(4, 0), P(10, 0)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites);

    CHECK(diagram.vertexCount() == 0);
    CHECK(diagram.edgeCount() == 2);
    CHECK(diagram.faceCount() == 3);
    checkAgainstDefinition(sites, 1, 8);
}

TEST_CASE("voronoiDiagram of one site is the whole plane") {
    const std::vector<Site> sites{P(3, -2)};
    const OrderDiagram diagram = pgl::voronoiDiagram(sites);

    CHECK(diagram.edgeCount() == 0);
    REQUIRE(diagram.faceCount() == 1);
    CHECK(diagram.label(OrderDiagram::FaceId(0)) == sites);

    const std::vector<WeightedSite> disk{D(3, -2, 5)};
    const PowerDiagram powerCell = pgl::powerDiagram(disk);
    REQUIRE(powerCell.faceCount() == 1);
    CHECK(powerCell.label(PowerDiagram::FaceId(0)) == disk);
}

TEST_CASE("powerDiagram weighs each site by its squared radius") {
    // The power bisector of two disks is their radical axis, which the heavier
    // disk pushes past the midpoint towards the lighter one: 16 of squared
    // radius moves it from x = 5 to x = 29/5, so the heavier disk owns more.
    const std::vector<WeightedSite> disks{D(0, 0, 4), D(10, 0, 0)};
    const PowerDiagram diagram = pgl::powerDiagram(disks);

    REQUIRE(diagram.edgeCount() == 1);
    CHECK(diagram.faceCount() == 2);
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(5, 0))) ==
          std::vector<WeightedSite>{disks[0]});
    CHECK(diagram.label(diagram.locateFace(pgl::EPoint(6, 0))) ==
          std::vector<WeightedSite>{disks[1]});

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
    const PowerDiagram diagram = pgl::powerDiagram(disks);

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
    const OrderDiagram fromPoints = pgl::voronoiDiagram(sites);
    const PowerDiagram fromDisks = pgl::powerDiagram(disks);

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
