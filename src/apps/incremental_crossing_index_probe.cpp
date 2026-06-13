#include "IncrementalCrossingIndex.hpp"
#include "JsonReader.hpp"
#include "LayoutScore.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr double CoordinateTolerance = 1e-9;

    void printUsage()
    {
        std::cerr
            << "Usage: incremental_crossing_index_probe <graph.json> <nodeIndex> <newX> <newY> "
            << "<expectedBeforeK> <expectedBeforeCrossings> <expectedAfterK> <expectedAfterCrossings>\n";
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

    double parseDouble(
        const std::string& value,
        const std::string& name
    )
    {
        std::size_t parsedCharacters = 0;

        const double parsed = std::stod(
            value,
            &parsedCharacters
        );

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error(
                "Invalid double for " + name + ": " + value
            );
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

    void assertDoubleEqual(
        const double actual,
        const double expected,
        const std::string& name
    )
    {
        if (std::abs(actual - expected) > CoordinateTolerance)
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

    void assertIndexScoreMatches(
        const IncrementalCrossingIndex& index,
        const std::size_t expectedK,
        const std::size_t expectedCrossings
    )
    {
        const LayoutScore score = index.score();

        assertEqual(
            score.maxCrossingsPerEdge,
            expectedK,
            "index score maxCrossingsPerEdge"
        );

        assertEqual(
            score.totalCrossings,
            expectedCrossings,
            "index score totalCrossings"
        );
    }

    void assertAppliedCoordinates(
        const IncrementalCrossingIndex& index,
        const NodeMove& move
    )
    {
        const Graph& graph = index.graph();

        if (move.nodeIndex >= graph.nodes.size())
        {
            throw std::runtime_error(
                "Cannot assert applied coordinates: node index out of range."
            );
        }

        assertDoubleEqual(
            graph.nodes[move.nodeIndex].x,
            move.newX,
            "applied x coordinate"
        );

        assertDoubleEqual(
            graph.nodes[move.nodeIndex].y,
            move.newY,
            "applied y coordinate"
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

        const std::size_t expectedBeforeK = parseSizeT(
            argv[5],
            "expectedBeforeK"
        );

        const std::size_t expectedBeforeCrossings = parseSizeT(
            argv[6],
            "expectedBeforeCrossings"
        );

        const std::size_t expectedAfterK = parseSizeT(
            argv[7],
            "expectedAfterK"
        );

        const std::size_t expectedAfterCrossings = parseSizeT(
            argv[8],
            "expectedAfterCrossings"
        );

        const Graph graph = JsonReader::load(graphPath);

        IncrementalCrossingIndex index(graph);

        if (!index.isConsistentWithFullRecompute())
        {
            throw std::runtime_error(
                "IncrementalCrossingIndex initial stats are inconsistent with full recompute."
            );
        }

        assertIndexScoreMatches(
            index,
            expectedBeforeK,
            expectedBeforeCrossings
        );

        const CrossingDeltaResult indexResult = index.evaluateNodeMove(move);

        const CrossingDeltaResult fullResult = index.evaluateNodeMoveByFullRecompute(
            move
        );

        assertExpectedResult(
            indexResult,
            expectedBeforeK,
            expectedBeforeCrossings,
            expectedAfterK,
            expectedAfterCrossings
        );

        assertSameDeltaResult(
            indexResult,
            fullResult,
            "index-vs-full"
        );

        if (!index.isConsistentWithFullRecompute())
        {
            throw std::runtime_error(
                "IncrementalCrossingIndex changed during move evaluation."
            );
        }

        const CrossingDeltaResult appliedResult = index.applyNodeMove(move);

        assertSameDeltaResult(
            appliedResult,
            fullResult,
            "apply-vs-full"
        );

        assertIndexScoreMatches(
            index,
            expectedAfterK,
            expectedAfterCrossings
        );

        assertAppliedCoordinates(
            index,
            move
        );

        if (!index.isConsistentWithFullRecompute())
        {
            throw std::runtime_error(
                "IncrementalCrossingIndex is inconsistent after applyNodeMove."
            );
        }

        std::cout << "Incremental crossing index probe passed.\n";
        std::cout << "Before k: " << indexResult.beforeScore.maxCrossingsPerEdge << '\n';
        std::cout << "Before crossings: " << indexResult.beforeScore.totalCrossings << '\n';
        std::cout << "After k: " << indexResult.afterScore.maxCrossingsPerEdge << '\n';
        std::cout << "After crossings: " << indexResult.afterScore.totalCrossings << '\n';
        std::cout << "Delta k: " << indexResult.deltaMaxCrossingsPerEdge << '\n';
        std::cout << "Delta crossings: " << indexResult.deltaTotalCrossings << '\n';
        std::cout << "Index local evaluation matches full recompute.\n";
        std::cout << "applyNodeMove updated index consistently.\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
