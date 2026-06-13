
#include "BaselineLayouts.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>

namespace
{
    constexpr double Pi = 3.14159265358979323846;

    double marginFor(double size)
    {
        return size * 0.1;
    }
}

void BaselineLayouts::applyRandom(
    Graph& graph,
    const double width,
    const double height,
    const unsigned int seed
)
{
    const double marginX = marginFor(width);
    const double marginY = marginFor(height);

    std::mt19937 generator(seed);
    std::uniform_real_distribution<double> xDistribution(marginX, width - marginX);
    std::uniform_real_distribution<double> yDistribution(marginY, height - marginY);

    for (Node& node : graph.nodes)
    {
        if (node.fixed)
        {
            continue;
        }

        node.x = xDistribution(generator);
        node.y = yDistribution(generator);
    }
}

void BaselineLayouts::applyCircular(
    Graph& graph,
    const double width,
    const double height
)
{
    if (graph.nodes.empty())
    {
        return;
    }

    const double centerX = width / 2.0;
    const double centerY = height / 2.0;
    const double radius = std::min(width, height) * 0.4;

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

void BaselineLayouts::applyGrid(
    Graph& graph,
    const double width,
    const double height
)
{
    if (graph.nodes.empty())
    {
        return;
    }

    const std::size_t nodeCount = graph.nodes.size();
    const std::size_t columns = static_cast<std::size_t>(std::ceil(std::sqrt(nodeCount)));
    const std::size_t rows = static_cast<std::size_t>(std::ceil(
        static_cast<double>(nodeCount) / static_cast<double>(columns)
    ));

    const double marginX = marginFor(width);
    const double marginY = marginFor(height);

    const double usableWidth = width - 2.0 * marginX;
    const double usableHeight = height - 2.0 * marginY;

    const double stepX = columns > 1
        ? usableWidth / static_cast<double>(columns - 1)
        : 0.0;

    const double stepY = rows > 1
        ? usableHeight / static_cast<double>(rows - 1)
        : 0.0;

    for (std::size_t index = 0; index < nodeCount; ++index)
    {
        Node& node = graph.nodes[index];

        if (node.fixed)
        {
            continue;
        }

        const std::size_t row = index / columns;
        const std::size_t column = index % columns;

        node.x = marginX + static_cast<double>(column) * stepX;
        node.y = marginY + static_cast<double>(row) * stepY;
    }
}
