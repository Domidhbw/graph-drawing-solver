
#include "FruchtermanReingoldLayout.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr double Epsilon = 1e-9;
    constexpr double Pi = 3.14159265358979323846;

    struct Vector2
    {
        double x = 0.0;
        double y = 0.0;
    };

    double length(const Vector2& vector)
    {
        return std::sqrt(vector.x * vector.x + vector.y * vector.y);
    }

    double clamp(const double value, const double minValue, const double maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    std::unordered_map<int, std::size_t> buildNodeIndexLookup(const Graph& graph)
    {
        std::unordered_map<int, std::size_t> indexByNodeId;
        indexByNodeId.reserve(graph.nodes.size());

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            indexByNodeId.emplace(graph.nodes[index].id, index);
        }

        return indexByNodeId;
    }

    std::size_t findNodeIndex(
        const std::unordered_map<int, std::size_t>& indexByNodeId,
        const int nodeId
    )
    {
        const auto iterator = indexByNodeId.find(nodeId);
        if (iterator == indexByNodeId.end())
        {
            throw std::runtime_error("Edge references unknown node id: " + std::to_string(nodeId));
        }

        return iterator->second;
    }

    bool hasDegenerateCoordinates(const Graph& graph)
    {
        if (graph.nodes.size() < 2)
        {
            return false;
        }

        const double firstX = graph.nodes.front().x;
        const double firstY = graph.nodes.front().y;

        for (const Node& node : graph.nodes)
        {
            if (std::abs(node.x - firstX) > Epsilon || std::abs(node.y - firstY) > Epsilon)
            {
                return false;
            }
        }

        return true;
    }

    void initializeCircular(Graph& graph, const double width, const double height)
    {
        if (graph.nodes.empty())
        {
            return;
        }

        const double centerX = width / 2.0;
        const double centerY = height / 2.0;
        const double radius = std::min(width, height) * 0.35;

        const std::size_t nodeCount = graph.nodes.size();

        for (std::size_t index = 0; index < nodeCount; ++index)
        {
            Node& node = graph.nodes[index];

            if (node.fixed)
            {
                continue;
            }

            const double angle = 2.0 * Pi * static_cast<double>(index) / static_cast<double>(nodeCount);
            node.x = centerX + radius * std::cos(angle);
            node.y = centerY + radius * std::sin(angle);
        }
    }

    void keepNodesInsideBounds(Graph& graph, const double width, const double height)
    {
        for (Node& node : graph.nodes)
        {
            if (node.fixed)
            {
                continue;
            }

            node.x = clamp(node.x, 0.0, width);
            node.y = clamp(node.y, 0.0, height);
        }
    }
}

void FruchtermanReingoldLayout::apply(
    Graph& graph,
    const double width,
    const double height,
    const int iterations
)
{
    if (graph.nodes.empty() || iterations <= 0)
    {
        return;
    }

    if (hasDegenerateCoordinates(graph))
    {
        initializeCircular(graph, width, height);
    }

    const auto indexByNodeId = buildNodeIndexLookup(graph);

    const double area = width * height;
    const double idealDistance = std::sqrt(area / static_cast<double>(graph.nodes.size()));

    double temperature = std::min(width, height) * 0.1;
    const double coolingFactor = temperature / static_cast<double>(iterations);

    std::vector<Vector2> displacement(graph.nodes.size());

    for (int iteration = 0; iteration < iterations; ++iteration)
    {
        std::fill(displacement.begin(), displacement.end(), Vector2{});

        for (std::size_t i = 0; i < graph.nodes.size(); ++i)
        {
            for (std::size_t j = i + 1; j < graph.nodes.size(); ++j)
            {
                const double deltaX = graph.nodes[i].x - graph.nodes[j].x;
                const double deltaY = graph.nodes[i].y - graph.nodes[j].y;

                const double distance = std::max(
                    Epsilon,
                    std::sqrt(deltaX * deltaX + deltaY * deltaY)
                );

                const double force = idealDistance * idealDistance / distance;
                const double forceX = deltaX / distance * force;
                const double forceY = deltaY / distance * force;

                if (!graph.nodes[i].fixed)
                {
                    displacement[i].x += forceX;
                    displacement[i].y += forceY;
                }

                if (!graph.nodes[j].fixed)
                {
                    displacement[j].x -= forceX;
                    displacement[j].y -= forceY;
                }
            }
        }

        for (const Edge& edge : graph.edges)
        {
            const std::size_t sourceIndex = findNodeIndex(indexByNodeId, edge.source);
            const std::size_t targetIndex = findNodeIndex(indexByNodeId, edge.target);

            const double deltaX = graph.nodes[sourceIndex].x - graph.nodes[targetIndex].x;
            const double deltaY = graph.nodes[sourceIndex].y - graph.nodes[targetIndex].y;

            const double distance = std::max(
                Epsilon,
                std::sqrt(deltaX * deltaX + deltaY * deltaY)
            );

            const double force = distance * distance / idealDistance;
            const double forceX = deltaX / distance * force;
            const double forceY = deltaY / distance * force;

            if (!graph.nodes[sourceIndex].fixed)
            {
                displacement[sourceIndex].x -= forceX;
                displacement[sourceIndex].y -= forceY;
            }

            if (!graph.nodes[targetIndex].fixed)
            {
                displacement[targetIndex].x += forceX;
                displacement[targetIndex].y += forceY;
            }
        }

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            if (graph.nodes[index].fixed)
            {
                continue;
            }

            const double displacementLength = length(displacement[index]);
            if (displacementLength < Epsilon)
            {
                continue;
            }

            const double limitedMovement = std::min(displacementLength, temperature);

            graph.nodes[index].x += displacement[index].x / displacementLength * limitedMovement;
            graph.nodes[index].y += displacement[index].y / displacementLength * limitedMovement;
        }

        keepNodesInsideBounds(graph, width, height);

        temperature = std::max(0.0, temperature - coolingFactor);
    }
}
