#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <concepts>
#include <cstdint>
#include <set>
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
