#pragma once

#include "core/hash.hpp"
#include "core/graph.hpp"

/**
 * @file visibilitygraph.hpp
 * @brief Visibility graphs of simple polygons.
 */

namespace pgl {

template <class PointType, class LabelType>
Graph<PointType> Polygon<PointType, LabelType>::visibilityGraph() const {
    Graph<PointType> result;
    const auto translatedVertices = vertices();

    // Add vertices explicitly: Graph::addEdge deliberately ignores self-loops,
    // so an isolated vertex (notably a one-point polygon) would otherwise be
    // absent from the result.
    for (const auto& vertex : translatedVertices) {
        result.addVertex(vertex);
    }

    for (std::size_t i = 0; i < translatedVertices.size(); ++i) {
        for (std::size_t j = i + 1; j < translatedVertices.size(); ++j) {
            const Segment<PointType> candidate(translatedVertices[i], translatedVertices[j]);
            if (contains(candidate)) {
                result.addEdge(translatedVertices[i], translatedVertices[j]);
            }
        }
    }

    return result;
}

}  // namespace pgl
