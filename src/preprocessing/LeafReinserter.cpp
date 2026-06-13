
#include "LeafReinserter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    struct IntPoint
    {
        int x = 0;
        int y = 0;

        bool operator==(const IntPoint& other) const
        {
            return x == other.x && y == other.y;
        }
    };

    struct IntPointHash
    {
        std::size_t operator()(const IntPoint& point) const
        {
            const std::size_t xHash = std::hash<int>{}(point.x);
            const std::size_t yHash = std::hash<int>{}(point.y);

            return xHash ^ (yHash + 0x9e3779b97f4a7c15ULL + (xHash << 6U) + (xHash >> 2U));
        }
    };

    int roundAndClampCoordinate(const double value, const int maxValue)
    {
        const long long rounded = std::llround(value);

        const long long clamped = std::max(
            0LL,
            std::min(static_cast<long long>(maxValue), rounded)
        );

        return static_cast<int>(clamped);
    }

    IntPoint roundedPointForNode(const Node& node, const Graph& graph)
    {
        return IntPoint{
            roundAndClampCoordinate(node.x, graph.width),
            roundAndClampCoordinate(node.y, graph.height)
        };
    }

    bool isInsideBounds(const IntPoint& point, const Graph& graph)
    {
        return point.x >= 0
            && point.y >= 0
            && point.x <= graph.width
            && point.y <= graph.height;
    }

    long long gridCapacity(const Graph& graph)
    {
        return (static_cast<long long>(graph.width) + 1LL)
            * (static_cast<long long>(graph.height) + 1LL);
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
                    "Duplicate node id while reinserting leaves: " +
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
                "Cannot reinsert leaf because parent node is missing: " +
                std::to_string(nodeId)
            );
        }

        return *iterator->second;
    }

    std::unordered_set<IntPoint, IntPointHash> collectOccupiedCoordinates(
        const Graph& graph
    )
    {
        std::unordered_set<IntPoint, IntPointHash> occupied;
        occupied.reserve(graph.nodes.size());

        for (const Node& node : graph.nodes)
        {
            occupied.insert(roundedPointForNode(node, graph));
        }

        return occupied;
    }

    std::vector<IntPoint> generateManhattanShellCandidates(
        const IntPoint& center,
        const int radius
    )
    {
        std::vector<IntPoint> candidates;

        if (radius == 0)
        {
            candidates.push_back(center);
            return candidates;
        }

        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int remaining = radius - std::abs(dx);

            if (remaining == 0)
            {
                candidates.push_back(IntPoint{ center.x + dx, center.y });
                continue;
            }

            candidates.push_back(IntPoint{ center.x + dx, center.y - remaining });
            candidates.push_back(IntPoint{ center.x + dx, center.y + remaining });
        }

        return candidates;
    }

    IntPoint findNearestFreePointAroundParent(
        const Node& parentNode,
        const Graph& graph,
        const std::unordered_set<IntPoint, IntPointHash>& occupiedCoordinates
    )
    {
        const IntPoint parentPoint = roundedPointForNode(parentNode, graph);
        const int maxSearchRadius = graph.width + graph.height + 2;

        for (int radius = 1; radius <= maxSearchRadius; ++radius)
        {
            const std::vector<IntPoint> candidates = generateManhattanShellCandidates(
                parentPoint,
                radius
            );

            for (const IntPoint& candidate : candidates)
            {
                if (!isInsideBounds(candidate, graph))
                {
                    continue;
                }

                if (!occupiedCoordinates.contains(candidate))
                {
                    return candidate;
                }
            }
        }

        throw std::runtime_error(
            "Could not find free coordinate while reinserting leaf."
        );
    }

    void validateCapacityForReinsertion(
        const Graph& graph,
        const std::size_t additionalNodeCount
    )
    {
        if (graph.width < 0 || graph.height < 0)
        {
            throw std::runtime_error(
                "Cannot reinsert leaves into graph with negative bounds."
            );
        }

        const long long requiredNodes = static_cast<long long>(graph.nodes.size())
            + static_cast<long long>(additionalNodeCount);

        if (gridCapacity(graph) < requiredNodes)
        {
            throw std::runtime_error(
                "Cannot reinsert leaves: grid has fewer positions than required nodes."
            );
        }
    }
}

Graph LeafReinserter::reinsertLeaves(const LeafReductionResult& reductionResult)
{
    Graph graph = reductionResult.reducedGraph;

    validateCapacityForReinsertion(
        graph,
        reductionResult.removedLeaves.size()
    );

    std::unordered_set<IntPoint, IntPointHash> occupiedCoordinates = collectOccupiedCoordinates(graph);

    for (const RemovedLeaf& removedLeaf : reductionResult.removedLeaves)
    {
        const std::unordered_map<int, const Node*> nodesById = buildNodeLookup(graph);
        const Node& parentNode = findNode(nodesById, removedLeaf.parentNodeId);

        const IntPoint leafPoint = findNearestFreePointAroundParent(
            parentNode,
            graph,
            occupiedCoordinates
        );

        Node reinsertedNode = removedLeaf.node;
        reinsertedNode.x = static_cast<double>(leafPoint.x);
        reinsertedNode.y = static_cast<double>(leafPoint.y);

        graph.nodes.push_back(reinsertedNode);
        graph.edges.push_back(removedLeaf.edge);

        occupiedCoordinates.insert(leafPoint);
    }

    return graph;
}
