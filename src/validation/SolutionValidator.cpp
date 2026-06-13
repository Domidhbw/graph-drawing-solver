
#include "SolutionValidator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
    struct IntPoint
    {
        int x = 0;
        int y = 0;
    };

    struct CoordinateKey
    {
        int x = 0;
        int y = 0;

        bool operator==(const CoordinateKey& other) const
        {
            return x == other.x && y == other.y;
        }
    };

    struct CoordinateKeyHash
    {
        std::size_t operator()(const CoordinateKey& key) const
        {
            const std::size_t xHash = std::hash<int>{}(key.x);
            const std::size_t yHash = std::hash<int>{}(key.y);

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

    std::unordered_map<int, std::size_t> buildNodeIndexById(
        const Graph& graph,
        ValidationResult& result
    )
    {
        std::unordered_map<int, std::size_t> nodeIndexById;
        nodeIndexById.reserve(graph.nodes.size());

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            const Node& node = graph.nodes[index];

            const auto [_, inserted] = nodeIndexById.emplace(node.id, index);

            if (!inserted)
            {
                result.errors.push_back("Duplicate node id: " + std::to_string(node.id));
            }
        }

        return nodeIndexById;
    }

    std::vector<IntPoint> buildRoundedCoordinates(
        const Graph& graph,
        ValidationResult& result
    )
    {
        std::vector<IntPoint> coordinates;
        coordinates.reserve(graph.nodes.size());

        for (const Node& node : graph.nodes)
        {
            if (!std::isfinite(node.x) || !std::isfinite(node.y))
            {
                result.errors.push_back("Node has non-finite coordinates: " + std::to_string(node.id));
            }

            const int x = roundAndClampCoordinate(node.x, graph.width);
            const int y = roundAndClampCoordinate(node.y, graph.height);

            coordinates.push_back(IntPoint{ x, y });
        }

        return coordinates;
    }

    void validateGraphBounds(const Graph& graph, ValidationResult& result)
    {
        if (graph.width < 0)
        {
            result.errors.push_back("Graph width must not be negative.");
        }

        if (graph.height < 0)
        {
            result.errors.push_back("Graph height must not be negative.");
        }
    }

    void validateUniqueCoordinates(
        const Graph& graph,
        const std::vector<IntPoint>& coordinates,
        ValidationResult& result
    )
    {
        std::unordered_map<CoordinateKey, int, CoordinateKeyHash> occupiedCoordinates;
        occupiedCoordinates.reserve(coordinates.size());

        for (std::size_t index = 0; index < coordinates.size(); ++index)
        {
            const Node& node = graph.nodes[index];
            const IntPoint& point = coordinates[index];

            const CoordinateKey key{ point.x, point.y };

            const auto [iterator, inserted] = occupiedCoordinates.emplace(key, node.id);

            if (!inserted)
            {
                result.errors.push_back(
                    "Duplicate rounded coordinate (" +
                    std::to_string(point.x) +
                    ", " +
                    std::to_string(point.y) +
                    ") for nodes " +
                    std::to_string(iterator->second) +
                    " and " +
                    std::to_string(node.id)
                );
            }
        }
    }

    void validateEdgeReferences(
        const Graph& graph,
        const std::unordered_map<int, std::size_t>& nodeIndexById,
        ValidationResult& result
    )
    {
        for (const Edge& edge : graph.edges)
        {
            if (!nodeIndexById.contains(edge.source))
            {
                result.errors.push_back("Edge references unknown source node id: " + std::to_string(edge.source));
            }

            if (!nodeIndexById.contains(edge.target))
            {
                result.errors.push_back("Edge references unknown target node id: " + std::to_string(edge.target));
            }
        }
    }

    std::int64_t orientation(
        const IntPoint& a,
        const IntPoint& b,
        const IntPoint& c
    )
    {
        const std::int64_t abX = static_cast<std::int64_t>(b.x) - static_cast<std::int64_t>(a.x);
        const std::int64_t abY = static_cast<std::int64_t>(b.y) - static_cast<std::int64_t>(a.y);
        const std::int64_t acX = static_cast<std::int64_t>(c.x) - static_cast<std::int64_t>(a.x);
        const std::int64_t acY = static_cast<std::int64_t>(c.y) - static_cast<std::int64_t>(a.y);

        return abX * acY - abY * acX;
    }

    bool isBetweenInclusive(
        const int value,
        const int first,
        const int second
    )
    {
        return value >= std::min(first, second)
            && value <= std::max(first, second);
    }

    bool isPointOnSegment(
        const IntPoint& segmentStart,
        const IntPoint& segmentEnd,
        const IntPoint& point
    )
    {
        return orientation(segmentStart, segmentEnd, point) == 0
            && isBetweenInclusive(point.x, segmentStart.x, segmentEnd.x)
            && isBetweenInclusive(point.y, segmentStart.y, segmentEnd.y);
    }

    bool isPointStrictlyInsideSegment(
        const IntPoint& segmentStart,
        const IntPoint& segmentEnd,
        const IntPoint& point
    )
    {
        if (!isPointOnSegment(segmentStart, segmentEnd, point))
        {
            return false;
        }

        const bool isStart = point.x == segmentStart.x && point.y == segmentStart.y;
        const bool isEnd = point.x == segmentEnd.x && point.y == segmentEnd.y;

        return !isStart && !isEnd;
    }

    void validateNoVertexInsideNonIncidentEdge(
        const Graph& graph,
        const std::vector<IntPoint>& coordinates,
        const std::unordered_map<int, std::size_t>& nodeIndexById,
        ValidationResult& result
    )
    {
        for (const Edge& edge : graph.edges)
        {
            const auto sourceIterator = nodeIndexById.find(edge.source);
            const auto targetIterator = nodeIndexById.find(edge.target);

            if (sourceIterator == nodeIndexById.end() || targetIterator == nodeIndexById.end())
            {
                continue;
            }

            const std::size_t sourceIndex = sourceIterator->second;
            const std::size_t targetIndex = targetIterator->second;

            const IntPoint& sourcePoint = coordinates[sourceIndex];
            const IntPoint& targetPoint = coordinates[targetIndex];

            for (std::size_t nodeIndex = 0; nodeIndex < graph.nodes.size(); ++nodeIndex)
            {
                const Node& node = graph.nodes[nodeIndex];

                if (node.id == edge.source || node.id == edge.target)
                {
                    continue;
                }

                const IntPoint& point = coordinates[nodeIndex];

                if (isPointStrictlyInsideSegment(sourcePoint, targetPoint, point))
                {
                    result.errors.push_back(
                        "Node " +
                        std::to_string(node.id) +
                        " lies inside edge (" +
                        std::to_string(edge.source) +
                        ", " +
                        std::to_string(edge.target) +
                        ") after rounding."
                    );
                }
            }
        }
    }
}

ValidationResult SolutionValidator::validate(const Graph& graph)
{
    ValidationResult result;

    validateGraphBounds(graph, result);

    if (graph.width < 0 || graph.height < 0)
    {
        return result;
    }

    const std::unordered_map<int, std::size_t> nodeIndexById = buildNodeIndexById(graph, result);
    const std::vector<IntPoint> coordinates = buildRoundedCoordinates(graph, result);

    validateUniqueCoordinates(graph, coordinates, result);
    validateEdgeReferences(graph, nodeIndexById, result);
    validateNoVertexInsideNonIncidentEdge(graph, coordinates, nodeIndexById, result);

    return result;
}
