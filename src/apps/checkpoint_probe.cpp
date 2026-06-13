
#include "CheckpointStore.hpp"
#include "JsonReader.hpp"
#include "SolutionValidator.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void printUsage()
    {
        std::cerr << "Usage: checkpoint_probe <input.json> <output.json>\n";
    }

    void assertFileExists(const std::string& path)
    {
        if (!std::filesystem::exists(path))
        {
            throw std::runtime_error(
                "Expected checkpoint file does not exist: " + path
            );
        }
    }

    void assertTemporaryFileDoesNotExist(const std::string& path)
    {
        const std::string temporaryPath = path + ".tmp";

        if (std::filesystem::exists(temporaryPath))
        {
            throw std::runtime_error(
                "Temporary checkpoint file was not cleaned up: " + temporaryPath
            );
        }
    }

    void assertCheckpointIsValid(const std::string& outputPath)
    {
        const Graph graph = JsonReader::load(outputPath);
        const ValidationResult validationResult = SolutionValidator::validate(graph);

        if (validationResult.isValid())
        {
            return;
        }

        for (const std::string& error : validationResult.errors)
        {
            std::cerr << "Validation error: " << error << '\n';
        }

        throw std::runtime_error(
            "Checkpoint output is not valid: " + outputPath
        );
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

        const Graph graph = JsonReader::load(inputPath);

        CheckpointStore::saveValidAtomic(
            graph,
            outputPath
        );

        assertFileExists(outputPath);
        assertTemporaryFileDoesNotExist(outputPath);
        assertCheckpointIsValid(outputPath);

        std::cout << "Checkpoint probe passed.\n";
        std::cout << "Input: " << inputPath << '\n';
        std::cout << "Output: " << outputPath << '\n';

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
