
#include "CheckpointStore.hpp"

#include "JsonWriter.hpp"
#include "SolutionValidator.hpp"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
    std::string temporaryPathFor(const std::string& outputPath)
    {
        return outputPath + ".tmp";
    }

    void removeIfExists(const std::string& path)
    {
        std::error_code errorCode;
        std::filesystem::remove(path, errorCode);
    }

    void validateGraphBeforeCheckpoint(const Graph& graph)
    {
        const ValidationResult validationResult = SolutionValidator::validate(graph);

        if (validationResult.isValid())
        {
            return;
        }

        std::string message = "Refusing to checkpoint invalid graph.";

        for (const std::string& error : validationResult.errors)
        {
            message += "\nValidation error: ";
            message += error;
        }

        throw std::runtime_error(message);
    }

    void renameAtomically(
        const std::string& temporaryPath,
        const std::string& outputPath
    )
    {
        std::error_code errorCode;

        std::filesystem::rename(
            temporaryPath,
            outputPath,
            errorCode
        );

        if (errorCode)
        {
            removeIfExists(temporaryPath);

            throw std::runtime_error(
                "Could not atomically rename checkpoint from " +
                temporaryPath +
                " to " +
                outputPath +
                ": " +
                errorCode.message()
            );
        }
    }
}

void CheckpointStore::saveValidAtomic(
    const Graph& graph,
    const std::string& outputPath
)
{
    validateGraphBeforeCheckpoint(graph);

    const std::string temporaryPath = temporaryPathFor(outputPath);

    removeIfExists(temporaryPath);

    try
    {
        JsonWriter::save(
            graph,
            temporaryPath
        );

        renameAtomically(
            temporaryPath,
            outputPath
        );
    }
    catch (...)
    {
        removeIfExists(temporaryPath);
        throw;
    }
}
