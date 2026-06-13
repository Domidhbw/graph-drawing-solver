
#include "LeafReducer.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::unordered_map<int, const Node*> buildNodeLookup(const Graph& graph)
    {
        std::unordered_map<int, const Node*> nodesById;
        nodesById.reserve(graph.nodes.size());

        for (const Node& node : graph.nodes)
        {
            const auto [_, inserted] = nodesById.emplace(node.id, &node);

            if (!inserted)
            {
                throw std::runtime_error(
                    "Duplicate node id while reducing leaves: " +
                    std::to_string(node.id)
                );
            }
        }

        return nodesById;
    }

    void validateEdgeReferences(
        const Graph& graph,
        const std::unordered_map<int, const Node*>& nodesById
    )
    {
        for (const Edge& edge : graph.edges)
        {
            if (!nodesById.contains(edge.source))
            {
                throw std::runtime_error(
                    "Edge references unknown source node id while reducing leaves: " +
                    std::to_string(edge.source)
                );
            }

            if (!nodesById.contains(edge.target))
            {
                throw std::runtime_error(
                    "Edge references unknown target node id while reducing leaves: " +
                    std::to_string(edge.target)
                );
            }
        }
    }

    std::unordered_map<int, int> computeDegrees(const Graph& graph)
    {
        std::unordered_map<int, int> degreeByNodeId;
        degreeByNodeId.reserve(graph.nodes.size());

        for (const Node& node : graph.nodes)
        {
            degreeByNodeId.emplace(node.id, 0);
        }

        for (const Edge& edge : graph.edges)
        {
            ++degreeByNodeId.at(edge.source);
            ++degreeByNodeId.at(edge.target);
        }

        return degreeByNodeId;
    }

    bool isLeaf(
        const std::unordered_map<int, int>& degreeByNodeId,
        const int nodeId
    )
    {
        const auto iterator = degreeByNodeId.find(nodeId);

        if (iterator == degreeByNodeId.end())
        {
            return false;
        }

        return iterator->second == 1;
    }

    int otherEndpoint(const Edge& edge, const int nodeId)
    {
        if (edge.source == nodeId)
        {
            return edge.target;
        }

        if (edge.target == nodeId)
        {
            return edge.source;
        }

        throw std::runtime_error(
            "Cannot find other endpoint for non-incident edge."
        );
    }

    Edge findLeafEdge(const Graph& graph, const int leafNodeId)
    {
        for (const Edge& edge : graph.edges)
        {
            if (edge.source == leafNodeId || edge.target == leafNodeId)
            {
                return edge;
            }
        }

        throw std::runtime_error(
            "Leaf node has no incident edge: " +
            std::to_string(leafNodeId)
        );
    }

    std::unordered_set<int> collectLeafNodeIds(
        const Graph& graph,
        const std::unordered_map<int, int>& degreeByNodeId
    )
    {
        std::unordered_set<int> leafNodeIds;
        leafNodeIds.reserve(graph.nodes.size());

        for (const Node& node : graph.nodes)
        {
            if (isLeaf(degreeByNodeId, node.id))
            {
                leafNodeIds.insert(node.id);
            }
        }

        return leafNodeIds;
    }

    bool isKeptEdge(
        const Edge& edge,
        const std::unordered_set<int>& removedNodeIds
    )
    {
        return !removedNodeIds.contains(edge.source)
            && !removedNodeIds.contains(edge.target);
    }
}

LeafReductionResult LeafReducer::reduceSinglePass(const Graph& graph)
{
    const std::unordered_map<int, const Node*> nodesById = buildNodeLookup(graph);
    validateEdgeReferences(graph, nodesById);

    const std::unordered_map<int, int> degreeByNodeId = computeDegrees(graph);
    const std::unordered_set<int> leafNodeIds = collectLeafNodeIds(
        graph,
        degreeByNodeId
    );

    LeafReductionResult result;
    result.reducedGraph.width = graph.width;
    result.reducedGraph.height = graph.height;

    result.removedLeaves.reserve(leafNodeIds.size());

    for (const Node& node : graph.nodes)
    {
        if (!leafNodeIds.contains(node.id))
        {
            result.reducedGraph.nodes.push_back(node);
            continue;
        }

        const Edge leafEdge = findLeafEdge(graph, node.id);

        result.removedLeaves.push_back(RemovedLeaf{
            node,
            leafEdge,
            otherEndpoint(leafEdge, node.id)
        });
    }

    result.reducedGraph.edges.reserve(graph.edges.size());

    for (const Edge& edge : graph.edges)
    {
        if (isKeptEdge(edge, leafNodeIds))
        {
            result.reducedGraph.edges.push_back(edge);
        }
    }

    return result;
}
