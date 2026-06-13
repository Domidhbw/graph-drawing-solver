
#include "JsonReader.hpp"
#include "LeafReducer.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void printUsage()
    {
        std::cerr
            << "Usage: leaf_reducer_probe <graph.json> "
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
        const Graph& reducedGraph
    )
    {
        if (originalGraph.width != reducedGraph.width)
        {
            throw std::runtime_error("Reduced graph width does not match original graph width.");
        }

        if (originalGraph.height != reducedGraph.height)
        {
            throw std::runtime_error("Reduced graph height does not match original graph height.");
        }
    }

    void assertRemovedLeavesHaveParents(
        const LeafReductionResult& result
    )
    {
        for (const RemovedLeaf& removedLeaf : result.removedLeaves)
        {
            if (removedLeaf.node.id == removedLeaf.parentNodeId)
            {
                throw std::runtime_error(
                    "Removed leaf parent must not be the leaf itself."
                );
            }
        }
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
        const LeafReductionResult result = LeafReducer::reduceSinglePass(graph);

        assertEqual(
            result.reducedGraph.nodes.size(),
            expectedReducedNodes,
            "reduced node count"
        );

        assertEqual(
            result.reducedGraph.edges.size(),
            expectedReducedEdges,
            "reduced edge count"
        );

        assertEqual(
            result.removedLeaves.size(),
            expectedRemovedLeaves,
            "removed leaf count"
        );

        assertGraphBoundsPreserved(
            graph,
            result.reducedGraph
        );

        assertRemovedLeavesHaveParents(result);

        std::cout << "Leaf reducer probe passed.\n";
        std::cout << "Original nodes: " << graph.nodes.size() << '\n';
        std::cout << "Original edges: " << graph.edges.size() << '\n';
        std::cout << "Reduced nodes: " << result.reducedGraph.nodes.size() << '\n';
        std::cout << "Reduced edges: " << result.reducedGraph.edges.size() << '\n';
        std::cout << "Removed leaves: " << result.removedLeaves.size() << '\n';

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
