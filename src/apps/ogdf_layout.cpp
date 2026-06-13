
#include "Graph.hpp"
#include "JsonReader.hpp"
#include "JsonWriter.hpp"
#include "SolutionRepairer.hpp"
#include "SolutionValidator.hpp"

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/energybased/FMMMLayout.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct OgdfLayoutContext
    {
        ogdf::Graph graph;
        std::vector<ogdf::node> nodesByInputIndex;
        std::unordered_map<int, ogdf::node> nodeById;
    };

    void printUsage()
    {
        std::cerr << "Usage: ogdf_layout <input.json> <output.json>\n";
        std::cerr << '\n';
        std::cerr << "Example:\n";
        std::cerr << "  ogdf_layout graph.json graph_ogdf.json\n";
    }

    OgdfLayoutContext buildOgdfGraph(const Graph& inputGraph)
    {
        OgdfLayoutContext context;
        context.nodesByInputIndex.reserve(inputGraph.nodes.size());
        context.nodeById.reserve(inputGraph.nodes.size());

        for (const Node& inputNode : inputGraph.nodes)
        {
            const ogdf::node ogdfNode = context.graph.newNode();

            const auto [_, inserted] = context.nodeById.emplace(
                inputNode.id,
                ogdfNode
            );

            if (!inserted)
            {
                throw std::runtime_error("Duplicate node id: " + std::to_string(inputNode.id));
            }

            context.nodesByInputIndex.push_back(ogdfNode);
        }

        for (const Edge& inputEdge : inputGraph.edges)
        {
            const auto sourceIterator = context.nodeById.find(inputEdge.source);
            const auto targetIterator = context.nodeById.find(inputEdge.target);

            if (sourceIterator == context.nodeById.end())
            {
                throw std::runtime_error(
                    "Edge references unknown source node id: " +
                    std::to_string(inputEdge.source)
                );
            }

            if (targetIterator == context.nodeById.end())
            {
                throw std::runtime_error(
                    "Edge references unknown target node id: " +
                    std::to_string(inputEdge.target)
                );
            }

            context.graph.newEdge(
                sourceIterator->second,
                targetIterator->second
            );
        }

        return context;
    }

    void runOgdfLayout(
        const ogdf::Graph& graph,
        ogdf::GraphAttributes& attributes
    )
    {
        for (const ogdf::node node : graph.nodes)
        {
            attributes.width(node) = 10.0;
            attributes.height(node) = 10.0;
        }

        ogdf::FMMMLayout layout;
        layout.call(attributes);
    }

    bool isFiniteCoordinate(const double value)
    {
        return std::isfinite(value);
    }

    void computeLayoutBounds(
        const std::vector<ogdf::node>& ogdfNodes,
        const ogdf::GraphAttributes& attributes,
        double& minX,
        double& maxX,
        double& minY,
        double& maxY
    )
    {
        minX = std::numeric_limits<double>::infinity();
        maxX = -std::numeric_limits<double>::infinity();
        minY = std::numeric_limits<double>::infinity();
        maxY = -std::numeric_limits<double>::infinity();

        for (const ogdf::node node : ogdfNodes)
        {
            const double x = attributes.x(node);
            const double y = attributes.y(node);

            if (!isFiniteCoordinate(x) || !isFiniteCoordinate(y))
            {
                throw std::runtime_error("OGDF produced non-finite coordinates.");
            }

            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }

        if (ogdfNodes.empty())
        {
            minX = 0.0;
            maxX = 0.0;
            minY = 0.0;
            maxY = 0.0;
        }
    }

    double computeScale(
        const double sourceWidth,
        const double sourceHeight,
        const double targetWidth,
        const double targetHeight
    )
    {
        const bool hasSourceWidth = sourceWidth > 0.0;
        const bool hasSourceHeight = sourceHeight > 0.0;

        if (!hasSourceWidth && !hasSourceHeight)
        {
            return 1.0;
        }

        if (!hasSourceWidth)
        {
            return targetHeight / sourceHeight;
        }

        if (!hasSourceHeight)
        {
            return targetWidth / sourceWidth;
        }

        return std::min(
            targetWidth / sourceWidth,
            targetHeight / sourceHeight
        );
    }

    void copyScaledCoordinatesToGraph(
        Graph& graph,
        const std::vector<ogdf::node>& ogdfNodes,
        const ogdf::GraphAttributes& attributes
    )
    {
        if (graph.nodes.size() != ogdfNodes.size())
        {
            throw std::runtime_error("Internal error: OGDF node count does not match input node count.");
        }

        if (graph.width <= 0 || graph.height <= 0)
        {
            throw std::runtime_error("Graph bounds must be positive for OGDF layout scaling.");
        }

        double minX = 0.0;
        double maxX = 0.0;
        double minY = 0.0;
        double maxY = 0.0;

        computeLayoutBounds(
            ogdfNodes,
            attributes,
            minX,
            maxX,
            minY,
            maxY
        );

        const double sourceWidth = maxX - minX;
        const double sourceHeight = maxY - minY;

        const double targetWidth = static_cast<double>(graph.width);
        const double targetHeight = static_cast<double>(graph.height);

        const double scale = computeScale(
            sourceWidth,
            sourceHeight,
            targetWidth,
            targetHeight
        );

        const double scaledWidth = sourceWidth * scale;
        const double scaledHeight = sourceHeight * scale;

        const double offsetX = (targetWidth - scaledWidth) / 2.0;
        const double offsetY = (targetHeight - scaledHeight) / 2.0;

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            const ogdf::node ogdfNode = ogdfNodes[index];

            if (sourceWidth <= 0.0 && sourceHeight <= 0.0)
            {
                graph.nodes[index].x = targetWidth / 2.0;
                graph.nodes[index].y = targetHeight / 2.0;
                continue;
            }

            graph.nodes[index].x = offsetX + (attributes.x(ogdfNode) - minX) * scale;
            graph.nodes[index].y = offsetY + (attributes.y(ogdfNode) - minY) * scale;
        }
    }

    void printValidationErrors(const ValidationResult& validationResult)
    {
        for (const std::string& error : validationResult.errors)
        {
            std::cerr << "Validation error: " << error << '\n';
        }
    }

    void validateAndWriteGraph(
        Graph& graph,
        const std::string& outputPath
    )
    {
        SolutionRepairer::repairDuplicateRoundedCoordinates(graph);

        const ValidationResult validationResult = SolutionValidator::validate(graph);

        if (!validationResult.isValid())
        {
            printValidationErrors(validationResult);
            throw std::runtime_error("OGDF layout did not produce a valid solution.");
        }

        JsonWriter::save(graph, outputPath);
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            printUsage();
            return 1;
        }

        const std::string inputPath = argv[1];
        const std::string outputPath = argv[2];

        Graph graph = JsonReader::load(inputPath);

        OgdfLayoutContext context = buildOgdfGraph(graph);

        ogdf::GraphAttributes attributes(
            context.graph,
            ogdf::GraphAttributes::nodeGraphics |
            ogdf::GraphAttributes::edgeGraphics
        );

        runOgdfLayout(
            context.graph,
            attributes
        );

        copyScaledCoordinatesToGraph(
            graph,
            context.nodesByInputIndex,
            attributes
        );

        validateAndWriteGraph(
            graph,
            outputPath
        );

        std::cout << "OGDF layout written: " << outputPath << '\n';

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
