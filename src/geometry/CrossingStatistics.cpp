
#include "CrossingStatistics.hpp"

#include "Geometry.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace
{
    bool shareEndpoint(const Edge& first, const Edge& second)
    {
        return first.source == second.source
            || first.source == second.target
            || first.target == second.source
            || first.target == second.target;
    }

    std::unordered_map<int, const Node*> buildNodeLookup(const Graph& graph)
    {
        std::unordered_map<int, const Node*> nodesById;
        nodesById.reserve(graph.nodes.size());

        for (const Node& node : graph.nodes)
        {
            nodesById.emplace(node.id, &node);
        }

        return nodesById;
    }

    const Node& findNode(
        const std::unordered_map<int, const Node*>& nodesById,
        const int id
    )
    {
        const auto iterator = nodesById.find(id);

        if (iterator == nodesById.end())
        {
            throw std::runtime_error("Edge references unknown node id: " + std::to_string(id));
        }

        return *iterator->second;
    }
}

CrossingStats CrossingStatistics::compute(const Graph& graph)
{
    CrossingStats stats;
    stats.crossingsPerEdge.assign(graph.edges.size(), 0);

    const auto nodesById = buildNodeLookup(graph);

    for (std::size_t i = 0; i < graph.edges.size(); ++i)
    {
        const Edge& firstEdge = graph.edges[i];

        const Node& a = findNode(nodesById, firstEdge.source);
        const Node& b = findNode(nodesById, firstEdge.target);

        for (std::size_t j = i + 1; j < graph.edges.size(); ++j)
        {
            const Edge& secondEdge = graph.edges[j];

            if (shareEndpoint(firstEdge, secondEdge))
            {
                continue;
            }

            const Node& c = findNode(nodesById, secondEdge.source);
            const Node& d = findNode(nodesById, secondEdge.target);

            if (Geometry::segmentsIntersect(a, b, c, d))
            {
                ++stats.totalCrossings;
                ++stats.crossingsPerEdge[i];
                ++stats.crossingsPerEdge[j];
            }
        }
    }

    for (const std::size_t edgeCrossings : stats.crossingsPerEdge)
    {
        if (edgeCrossings > stats.maxCrossingsPerEdge)
        {
            stats.maxCrossingsPerEdge = edgeCrossings;
        }
    }

    return stats;
}
