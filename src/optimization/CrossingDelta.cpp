
#include "CrossingDelta.hpp"

#include "Geometry.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    long long signedDifference(
        const std::size_t first,
        const std::size_t second
    )
    {
        return static_cast<long long>(first) - static_cast<long long>(second);
    }

    void validateMove(
        const Graph& graph,
        const NodeMove& move
    )
    {
        if (move.nodeIndex >= graph.nodes.size())
        {
            throw std::out_of_range(
                "Node move index out of range: " +
                std::to_string(move.nodeIndex)
            );
        }
    }

    bool shareEndpoint(const Edge& first, const Edge& second)
    {
        return first.source == second.source
            || first.source == second.target
            || first.target == second.source
            || first.target == second.target;
    }

    bool isIncidentToNode(const Edge& edge, const int nodeId)
    {
        return edge.source == nodeId || edge.target == nodeId;
    }

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
                    "Duplicate node id while evaluating crossing delta: " +
                    std::to_string(node.id)
                );
            }
        }

        return nodesById;
    }

    const Node& findNode(
        const std::unordered_map<int, const Node*>& nodesById,
        const int nodeId
    )
    {
        const auto iterator = nodesById.find(nodeId);

        if (iterator == nodesById.end())
        {
            throw std::runtime_error(
                "Edge references unknown node id while evaluating crossing delta: " +
                std::to_string(nodeId)
            );
        }

        return *iterator->second;
    }

    const Node& endpointForEdge(
        const std::unordered_map<int, const Node*>& nodesById,
        const Node& movedNode,
        const int endpointNodeId
    )
    {
        if (endpointNodeId == movedNode.id)
        {
            return movedNode;
        }

        return findNode(nodesById, endpointNodeId);
    }

    bool edgesIntersect(
        const Edge& firstEdge,
        const Edge& secondEdge,
        const std::unordered_map<int, const Node*>& nodesById
    )
    {
        const Node& a = findNode(nodesById, firstEdge.source);
        const Node& b = findNode(nodesById, firstEdge.target);
        const Node& c = findNode(nodesById, secondEdge.source);
        const Node& d = findNode(nodesById, secondEdge.target);

        return Geometry::segmentsIntersect(a, b, c, d);
    }

    bool edgesIntersectAfterMove(
        const Edge& firstEdge,
        const Edge& secondEdge,
        const std::unordered_map<int, const Node*>& nodesById,
        const Node& movedNode
    )
    {
        const Node& a = endpointForEdge(nodesById, movedNode, firstEdge.source);
        const Node& b = endpointForEdge(nodesById, movedNode, firstEdge.target);
        const Node& c = endpointForEdge(nodesById, movedNode, secondEdge.source);
        const Node& d = endpointForEdge(nodesById, movedNode, secondEdge.target);

        return Geometry::segmentsIntersect(a, b, c, d);
    }

    std::vector<std::size_t> collectIncidentEdgeIndices(
        const Graph& graph,
        const int nodeId
    )
    {
        std::vector<std::size_t> incidentEdgeIndices;

        for (std::size_t edgeIndex = 0; edgeIndex < graph.edges.size(); ++edgeIndex)
        {
            if (isIncidentToNode(graph.edges[edgeIndex], nodeId))
            {
                incidentEdgeIndices.push_back(edgeIndex);
            }
        }

        return incidentEdgeIndices;
    }

    std::size_t computeMaxCrossingsPerEdge(
        const std::vector<std::size_t>& crossingsPerEdge
    )
    {
        if (crossingsPerEdge.empty())
        {
            return 0;
        }

        return *std::max_element(
            crossingsPerEdge.begin(),
            crossingsPerEdge.end()
        );
    }

    CrossingStats buildStats(
        const std::size_t totalCrossings,
        const std::vector<std::size_t>& crossingsPerEdge
    )
    {
        CrossingStats stats;
        stats.totalCrossings = totalCrossings;
        stats.crossingsPerEdge = crossingsPerEdge;
        stats.maxCrossingsPerEdge = computeMaxCrossingsPerEdge(crossingsPerEdge);

        return stats;
    }

    CrossingDeltaResult buildResult(
        const LayoutScore& beforeScore,
        const LayoutScore& afterScore
    )
    {
        return CrossingDeltaResult{
            beforeScore,
            afterScore,
            signedDifference(
                afterScore.maxCrossingsPerEdge,
                beforeScore.maxCrossingsPerEdge
            ),
            signedDifference(
                afterScore.totalCrossings,
                beforeScore.totalCrossings
            )
        };
    }
}

CrossingDeltaResult CrossingDelta::evaluateNodeMoveByFullRecompute(
    const Graph& graph,
    const NodeMove& move
)
{
    validateMove(graph, move);

    const LayoutScore beforeScore = LayoutScoreCalculator::compute(graph);

    Graph movedGraph = graph;
    movedGraph.nodes[move.nodeIndex].x = move.newX;
    movedGraph.nodes[move.nodeIndex].y = move.newY;

    const LayoutScore afterScore = LayoutScoreCalculator::compute(movedGraph);

    return buildResult(beforeScore, afterScore);
}

CrossingDeltaResult CrossingDelta::evaluateNodeMoveLocally(
    const Graph& graph,
    const NodeMove& move
)
{
    return evaluateNodeMoveStatsLocally(
        graph,
        move
    ).scoreDelta;
}

CrossingDeltaStatsResult CrossingDelta::evaluateNodeMoveStatsLocally(
    const Graph& graph,
    const NodeMove& move
)
{
    validateMove(graph, move);

    return evaluateNodeMoveStatsLocally(
        graph,
        move,
        CrossingStatistics::compute(graph)
    );
}

CrossingDeltaStatsResult CrossingDelta::evaluateNodeMoveStatsLocally(
    const Graph& graph,
    const NodeMove& move,
    const CrossingStats& beforeStats
)
{
    validateMove(graph, move);

    std::vector<std::size_t> updatedCrossingsPerEdge = beforeStats.crossingsPerEdge;
    std::size_t updatedTotalCrossings = beforeStats.totalCrossings;

    const Node& originalNode = graph.nodes[move.nodeIndex];

    Node movedNode = originalNode;
    movedNode.x = move.newX;
    movedNode.y = move.newY;

    const auto nodesById = buildNodeLookup(graph);

    const std::vector<std::size_t> incidentEdgeIndices = collectIncidentEdgeIndices(
        graph,
        originalNode.id
    );

    for (const std::size_t incidentEdgeIndex : incidentEdgeIndices)
    {
        const Edge& incidentEdge = graph.edges[incidentEdgeIndex];

        for (std::size_t otherEdgeIndex = 0; otherEdgeIndex < graph.edges.size(); ++otherEdgeIndex)
        {
            if (incidentEdgeIndex == otherEdgeIndex)
            {
                continue;
            }

            const Edge& otherEdge = graph.edges[otherEdgeIndex];

            if (shareEndpoint(incidentEdge, otherEdge))
            {
                continue;
            }

            const bool crossedBefore = edgesIntersect(
                incidentEdge,
                otherEdge,
                nodesById
            );

            const bool crossedAfter = edgesIntersectAfterMove(
                incidentEdge,
                otherEdge,
                nodesById,
                movedNode
            );

            if (crossedBefore == crossedAfter)
            {
                continue;
            }

            if (crossedBefore)
            {
                --updatedTotalCrossings;
                --updatedCrossingsPerEdge[incidentEdgeIndex];
                --updatedCrossingsPerEdge[otherEdgeIndex];
            }
            else
            {
                ++updatedTotalCrossings;
                ++updatedCrossingsPerEdge[incidentEdgeIndex];
                ++updatedCrossingsPerEdge[otherEdgeIndex];
            }
        }
    }

    const CrossingStats afterStats = buildStats(
        updatedTotalCrossings,
        updatedCrossingsPerEdge
    );

    const LayoutScore beforeScore = LayoutScoreCalculator::fromCrossingStats(beforeStats);
    const LayoutScore afterScore = LayoutScoreCalculator::fromCrossingStats(afterStats);

    return CrossingDeltaStatsResult{
        beforeStats,
        afterStats,
        buildResult(
            beforeScore,
            afterScore
        )
    };
}
