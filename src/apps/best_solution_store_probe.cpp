#include "BestSolutionStore.hpp"
#include "JsonReader.hpp"
#include "LayoutScore.hpp"
#include "SolutionValidator.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void printUsage()
    {
        std::cerr
            << "Usage: best_solution_store_probe <first.json> <second.json> <output.json> "
            << "<expectedFirstStored> <expectedSecondStored> <expectedFinalK> <expectedFinalCrossings>\n";
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

    bool parseBool01(
        const std::string& value,
        const std::string& name
    )
    {
        if (value == "0")
        {
            return false;
        }

        if (value == "1")
        {
            return true;
        }

        throw std::runtime_error(
            "Invalid boolean value for " + name + ": " + value + ". Expected 0 or 1."
        );
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

    void assertEqualBool(
        const bool actual,
        const bool expected,
        const std::string& name
    )
    {
        if (actual != expected)
        {
            throw std::runtime_error(
                name +
                " mismatch. Expected " +
                std::to_string(expected ? 1 : 0) +
                ", got " +
                std::to_string(actual ? 1 : 0)
            );
        }
    }

    void assertCheckpointExists(const std::string& outputPath)
    {
        if (!std::filesystem::exists(outputPath))
        {
            throw std::runtime_error(
                "Expected best-solution checkpoint does not exist: " + outputPath
            );
        }
    }

    void assertNoTemporaryFileLeft(const std::string& outputPath)
    {
        const std::string temporaryPath = outputPath + ".tmp";

        if (std::filesystem::exists(temporaryPath))
        {
            throw std::runtime_error(
                "Temporary checkpoint file was not cleaned up: " + temporaryPath
            );
        }
    }

    void assertOutputIsValid(const Graph& graph)
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

        throw std::runtime_error("Best-solution checkpoint is invalid.");
    }

    void assertFinalScore(
        const std::string& outputPath,
        const std::size_t expectedFinalK,
        const std::size_t expectedFinalCrossings
    )
    {
        const Graph outputGraph = JsonReader::load(outputPath);

        assertOutputIsValid(outputGraph);

        const LayoutScore score = LayoutScoreCalculator::compute(outputGraph);

        assertEqual(
            score.maxCrossingsPerEdge,
            expectedFinalK,
            "final maxCrossingsPerEdge"
        );

        assertEqual(
            score.totalCrossings,
            expectedFinalCrossings,
            "final totalCrossings"
        );
    }
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 8)
        {
            printUsage();
            return 1;
        }

        const std::string firstPath = argv[1];
        const std::string secondPath = argv[2];
        const std::string outputPath = argv[3];

        const bool expectedFirstStored = parseBool01(
            argv[4],
            "expectedFirstStored"
        );

        const bool expectedSecondStored = parseBool01(
            argv[5],
            "expectedSecondStored"
        );

        const std::size_t expectedFinalK = parseSizeT(
            argv[6],
            "expectedFinalK"
        );

        const std::size_t expectedFinalCrossings = parseSizeT(
            argv[7],
            "expectedFinalCrossings"
        );

        std::filesystem::remove(outputPath);
        std::filesystem::remove(outputPath + ".tmp");

        const Graph firstGraph = JsonReader::load(firstPath);
        const Graph secondGraph = JsonReader::load(secondPath);

        BestSolutionStore store(outputPath);

        const bool firstStored = store.consider(firstGraph);
        const bool secondStored = store.consider(secondGraph);

        assertEqualBool(
            firstStored,
            expectedFirstStored,
            "first stored"
        );

        assertEqualBool(
            secondStored,
            expectedSecondStored,
            "second stored"
        );

        assertCheckpointExists(outputPath);
        assertNoTemporaryFileLeft(outputPath);

        assertFinalScore(
            outputPath,
            expectedFinalK,
            expectedFinalCrossings
        );

        std::cout << "Best solution store probe passed.\n";
        std::cout << "First stored: " << (firstStored ? "yes" : "no") << '\n';
        std::cout << "Second stored: " << (secondStored ? "yes" : "no") << '\n';
        std::cout << "Output: " << outputPath << '\n';
        std::cout << "Final k: " << expectedFinalK << '\n';
        std::cout << "Final crossings: " << expectedFinalCrossings << '\n';

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
