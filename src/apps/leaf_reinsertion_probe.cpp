#include "JsonReader.hpp"
#include "LeafReducer.hpp"
#include "LeafReinserter.hpp"
#include "SolutionValidator.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void printUsage()
    {
        std::cerr
            << "Usage: leaf_reinsertion_probe <graph.json> "
            << "<expectedReducedNodes> <expectedReducedEdges> <expectedRemovedLeaves>\n";
    }

    std::size_t parseSizeT(
        const std::string& value,
        const std::string& name
    )
    {
        std::size_t parsedCharacters = 0;

        const unsigned long long parsed = std::stoull(
            value,
            &parsedCharacters
        );

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error(
                "Invalid unsigned integer for " + name + ": " + value
            );
        }

        return static_cast<std::size_t>(parsed);
    }

    void assertEqual(
        const std::size_t actual,
        const std::size_t expected,
        const std::string& name
    )
    {
        if (actual != expected)
        {
            throw std::runtime_error(
                name +
                " mismatch. Expected " +
                std::to_string(expected) +
                ", got " +
                std::to_string(actual)
            );
        }
    }

    void assertGraphBoundsPreserved(
        const Graph& originalGraph,
        const Graph& reinsertedGraph
    )
    {
        if (originalGraph.width != reinsertedGraph.width)
        {
            throw std::runtime_error(
                "Reinserted graph width does not match original graph width."
            );
        }

        if (originalGraph.height != reinsertedGraph.height)
        {
            throw std::runtime_error(
                "Reinserted graph height does not match original graph height."
            );
        }
    }

    void assertValidGraph(const Graph& graph)
    {
        const ValidationResult validationResult = SolutionValidator::validate(graph);

        if (validationResult.isValid())
        {
            return;
        }

        for (const std::string& error : validationResult.errors)
        {
            std::cerr << "Validation error: " << error << '\n';
        }

        throw std::runtime_error("Reinserted graph is not valid.");
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 5)
        {
            printUsage();
            return 1;
        }

        const std::string graphPath = argv[1];

        const std::size_t expectedReducedNodes = parseSizeT(
            argv[2],
            "expectedReducedNodes"
        );

        const std::size_t expectedReducedEdges = parseSizeT(
            argv[3],
            "expectedReducedEdges"
        );

        const std::size_t expectedRemovedLeaves = parseSizeT(
            argv[4],
            "expectedRemovedLeaves"
        );

        const Graph graph = JsonReader::load(graphPath);
        const LeafReductionResult reductionResult = LeafReducer::reduceSinglePass(graph);
        const Graph reinsertedGraph = LeafReinserter::reinsertLeaves(reductionResult);

        assertEqual(
            reductionResult.reducedGraph.nodes.size(),
            expectedReducedNodes,
            "reduced node count"
        );

        assertEqual(
            reductionResult.reducedGraph.edges.size(),
            expectedReducedEdges,
            "reduced edge count"
        );

        assertEqual(
            reductionResult.removedLeaves.size(),
            expectedRemovedLeaves,
            "removed leaf count"
        );

        assertEqual(
            reinsertedGraph.nodes.size(),
            graph.nodes.size(),
            "reinserted node count"
        );

        assertEqual(
            reinsertedGraph.edges.size(),
            graph.edges.size(),
            "reinserted edge count"
        );

        assertGraphBoundsPreserved(
            graph,
            reinsertedGraph
        );

        assertValidGraph(reinsertedGraph);

        std::cout << "Leaf reinsertion probe passed.\n";
        std::cout << "Original nodes: " << graph.nodes.size() << '\n';
        std::cout << "Original edges: " << graph.edges.size() << '\n';
        std::cout << "Reduced nodes: " << reductionResult.reducedGraph.nodes.size() << '\n';
        std::cout << "Reduced edges: " << reductionResult.reducedGraph.edges.size() << '\n';
        std::cout << "Removed leaves: " << reductionResult.removedLeaves.size() << '\n';
        std::cout << "Reinserted nodes: " << reinsertedGraph.nodes.size() << '\n';
        std::cout << "Reinserted edges: " << reinsertedGraph.edges.size() << '\n';

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
