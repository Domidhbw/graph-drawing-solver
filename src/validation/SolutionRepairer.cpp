
#include "SolutionRepairer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
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

    IntPoint findNearestFreePoint(
        const IntPoint& preferredPoint,
        const Graph& graph,
        const std::unordered_set<IntPoint, IntPointHash>& occupiedCoordinates
    )
    {
        const int maxSearchRadius = graph.width + graph.height + 2;

        for (int radius = 0; radius <= maxSearchRadius; ++radius)
        {
            const std::vector<IntPoint> candidates = generateManhattanShellCandidates(
                preferredPoint,
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

        throw std::runtime_error("Could not find a free grid coordinate for duplicate-coordinate repair.");
    }

    void validateRepairPreconditions(const Graph& graph)
    {
        if (graph.width < 0)
        {
            throw std::runtime_error("Cannot repair graph with negative width.");
        }

        if (graph.height < 0)
        {
            throw std::runtime_error("Cannot repair graph with negative height.");
        }

        if (gridCapacity(graph) < static_cast<long long>(graph.nodes.size()))
        {
            throw std::runtime_error("Cannot repair duplicate coordinates: grid has fewer positions than nodes.");
        }
    }
}

bool SolutionRepairer::repairDuplicateRoundedCoordinates(Graph& graph)
{
    validateRepairPreconditions(graph);

    bool changed = false;

    std::unordered_set<IntPoint, IntPointHash> occupiedCoordinates;
    occupiedCoordinates.reserve(graph.nodes.size());

    for (Node& node : graph.nodes)
    {
        const IntPoint preferredPoint = roundedPointForNode(node, graph);

        if (!occupiedCoordinates.contains(preferredPoint))
        {
            occupiedCoordinates.insert(preferredPoint);
            node.x = static_cast<double>(preferredPoint.x);
            node.y = static_cast<double>(preferredPoint.y);
            continue;
        }

        const IntPoint repairedPoint = findNearestFreePoint(
            preferredPoint,
            graph,
            occupiedCoordinates
        );

        node.x = static_cast<double>(repairedPoint.x);
        node.y = static_cast<double>(repairedPoint.y);

        occupiedCoordinates.insert(repairedPoint);
        changed = true;
    }

    return changed;
}
