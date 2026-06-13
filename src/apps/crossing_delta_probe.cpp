
#include "CrossingDelta.hpp"
#include "JsonReader.hpp"
#include "LayoutScore.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void printUsage()
    {
        std::cerr
            << "Usage: crossing_delta_probe <graph.json> <nodeIndex> <newX> <newY> "
            << "<expectedBeforeK> <expectedBeforeCrossings> <expectedAfterK> <expectedAfterCrossings>\n";
    }

    std::size_t parseSizeT(const std::string& value, const std::string& name)
    {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed = std::stoull(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error("Invalid unsigned integer for " + name + ": " + value);
        }

        return static_cast<std::size_t>(parsed);
    }

    double parseDouble(const std::string& value, const std::string& name)
    {
        std::size_t parsedCharacters = 0;
        const double parsed = std::stod(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error("Invalid double for " + name + ": " + value);
        }

        return parsed;
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

    void assertSignedEqual(
        const long long actual,
        const long long expected,
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

    void assertSameDeltaResult(
        const CrossingDeltaResult& actual,
        const CrossingDeltaResult& expected,
        const std::string& name
    )
    {
        assertEqual(
            actual.beforeScore.maxCrossingsPerEdge,
            expected.beforeScore.maxCrossingsPerEdge,
            name + " before maxCrossingsPerEdge"
        );

        assertEqual(
            actual.beforeScore.totalCrossings,
            expected.beforeScore.totalCrossings,
            name + " before totalCrossings"
        );

        assertEqual(
            actual.afterScore.maxCrossingsPerEdge,
            expected.afterScore.maxCrossingsPerEdge,
            name + " after maxCrossingsPerEdge"
        );

        assertEqual(
            actual.afterScore.totalCrossings,
            expected.afterScore.totalCrossings,
            name + " after totalCrossings"
        );

        assertSignedEqual(
            actual.deltaMaxCrossingsPerEdge,
            expected.deltaMaxCrossingsPerEdge,
            name + " delta maxCrossingsPerEdge"
        );

        assertSignedEqual(
            actual.deltaTotalCrossings,
            expected.deltaTotalCrossings,
            name + " delta totalCrossings"
        );
    }

    void assertExpectedResult(
        const CrossingDeltaResult& result,
        const std::size_t expectedBeforeK,
        const std::size_t expectedBeforeCrossings,
        const std::size_t expectedAfterK,
        const std::size_t expectedAfterCrossings
    )
    {
        assertEqual(
            result.beforeScore.maxCrossingsPerEdge,
            expectedBeforeK,
            "before maxCrossingsPerEdge"
        );

        assertEqual(
            result.beforeScore.totalCrossings,
            expectedBeforeCrossings,
            "before totalCrossings"
        );

        assertEqual(
            result.afterScore.maxCrossingsPerEdge,
            expectedAfterK,
            "after maxCrossingsPerEdge"
        );

        assertEqual(
            result.afterScore.totalCrossings,
            expectedAfterCrossings,
            "after totalCrossings"
        );
    }

    void assertGraphUnchanged(
        const Graph& graph,
        const LayoutScore& originalScore
    )
    {
        const LayoutScore scoreAfterDeltaEvaluation = LayoutScoreCalculator::compute(graph);

        assertEqual(
            scoreAfterDeltaEvaluation.maxCrossingsPerEdge,
            originalScore.maxCrossingsPerEdge,
            "Graph unchanged maxCrossingsPerEdge"
        );

        assertEqual(
            scoreAfterDeltaEvaluation.totalCrossings,
            originalScore.totalCrossings,
            "Graph unchanged totalCrossings"
        );
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 9)
        {
            printUsage();
            return 1;
        }

        const std::string graphPath = argv[1];

        const NodeMove move{
            parseSizeT(argv[2], "nodeIndex"),
            parseDouble(argv[3], "newX"),
            parseDouble(argv[4], "newY")
        };

        const std::size_t expectedBeforeK = parseSizeT(argv[5], "expectedBeforeK");
        const std::size_t expectedBeforeCrossings = parseSizeT(argv[6], "expectedBeforeCrossings");
        const std::size_t expectedAfterK = parseSizeT(argv[7], "expectedAfterK");
        const std::size_t expectedAfterCrossings = parseSizeT(argv[8], "expectedAfterCrossings");

        const Graph graph = JsonReader::load(graphPath);
        const LayoutScore originalScore = LayoutScoreCalculator::compute(graph);

        const CrossingDeltaResult fullResult = CrossingDelta::evaluateNodeMoveByFullRecompute(
            graph,
            move
        );

        const CrossingDeltaResult localResult = CrossingDelta::evaluateNodeMoveLocally(
            graph,
            move
        );

        assertExpectedResult(
            fullResult,
            expectedBeforeK,
            expectedBeforeCrossings,
            expectedAfterK,
            expectedAfterCrossings
        );

        assertSameDeltaResult(
            localResult,
            fullResult,
            "local-vs-full"
        );

        assertGraphUnchanged(
            graph,
            originalScore
        );

        std::cout << "Crossing delta probe passed.\n";
        std::cout << "Before k: " << fullResult.beforeScore.maxCrossingsPerEdge << '\n';
        std::cout << "Before crossings: " << fullResult.beforeScore.totalCrossings << '\n';
        std::cout << "After k: " << fullResult.afterScore.maxCrossingsPerEdge << '\n';
        std::cout << "After crossings: " << fullResult.afterScore.totalCrossings << '\n';
        std::cout << "Delta k: " << fullResult.deltaMaxCrossingsPerEdge << '\n';
        std::cout << "Delta crossings: " << fullResult.deltaTotalCrossings << '\n';
        std::cout << "Local result matches full recompute.\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
