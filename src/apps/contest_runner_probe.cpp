
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct GraphRunResult {
    fs::path graphPath;
    fs::path outputPath;
    bool checkpointExists = false;
    bool temporaryFileExists = false;
    bool checkpointValid = false;
};

std::string shellQuote(const fs::path& path) {
    const std::string value = path.string();

    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('\'');

    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }

    quoted.push_back('\'');
    return quoted;
}

std::string shellQuote(const std::string& value) {
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('\'');

    for (const char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }

    quoted.push_back('\'');
    return quoted;
}

int runCommand(const std::string& command) {
    std::cout << "[contest_runner_probe] $ " << command << '\n';
    return std::system(command.c_str());
}

fs::path executableDirectory(const char* argv0) {
    fs::path executablePath(argv0);

    if (executablePath.has_parent_path()) {
        return fs::absolute(executablePath).parent_path();
    }

    return fs::current_path();
}

fs::path makeOutputPath(const fs::path& outputDirectory, const fs::path& graphPath, const std::size_t index) {
    const std::string stem = graphPath.stem().string().empty()
        ? "graph"
        : graphPath.stem().string();

    std::ostringstream fileName;
    fileName << index << "_" << stem << ".json";

    return outputDirectory / fileName.str();
}

fs::path temporaryPathFor(const fs::path& outputPath) {
    return fs::path(outputPath.string() + ".tmp");
}


bool validateCheckpoint(const fs::path& solverExecutable, const fs::path& checkpointPath) {
    std::ostringstream command;
    command
        << shellQuote(solverExecutable)
        << ' '
        << shellQuote(checkpointPath)
        << " validate";

    return runCommand(command.str()) == 0;
}

bool runCheckpointedSa(
    const fs::path& checkpointedSaExecutable,
    const fs::path& graphPath,
    const fs::path& outputPath,
    const std::string& seed,
    const std::string& iterations
) {
    std::ostringstream command;
    command
        << shellQuote(checkpointedSaExecutable)
        << ' '
        << shellQuote(graphPath)
        << ' '
        << shellQuote(outputPath)
        << ' '
        << shellQuote(seed)
        << ' '
        << shellQuote(iterations);

    return runCommand(command.str()) == 0;
}

void printUsage(const char* programName) {
    std::cerr
        << "Usage:\n"
        << "  " << programName << " <output_dir> <seed> <iterations> <graph1.json> [graph2.json ...]\n";
}

void printSummary(const std::vector<GraphRunResult>& results) {
    std::cout << "\n[contest_runner_probe] Summary\n";
    std::cout << "------------------------------------------------------------\n";

    std::size_t validCount = 0;

    for (const GraphRunResult& result : results) {
        if (result.checkpointValid) {
            ++validCount;
        }

        std::cout
            << "graph=" << result.graphPath
            << " output=" << result.outputPath
            << " exists=" << (result.checkpointExists ? "yes" : "no")
            << " tmp=" << (result.temporaryFileExists ? "yes" : "no")
            << " valid=" << (result.checkpointValid ? "yes" : "no")
            << '\n';
    }

    std::cout << "------------------------------------------------------------\n";
    std::cout
        << "valid_checkpoints=" << validCount
        << "/" << results.size()
        << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 5) {
            printUsage(argv[0]);
            return 2;
        }

        const fs::path outputDirectory = argv[1];
        const std::string seed = argv[2];
        const std::string iterations = argv[3];

        fs::create_directories(outputDirectory);

        const fs::path executableDir = executableDirectory(argv[0]);
        const fs::path checkpointedSaExecutable = executableDir / "checkpointed_sa_probe";
        const fs::path solverExecutable = executableDir / "solver";

        if (!fs::exists(checkpointedSaExecutable)) {
            std::cerr
                << "[contest_runner_probe] Missing executable: "
                << checkpointedSaExecutable << '\n';
            return 1;
        }

        if (!fs::exists(solverExecutable)) {
            std::cerr
                << "[contest_runner_probe] Missing executable: "
                << solverExecutable << '\n';
            return 1;
        }

        std::vector<GraphRunResult> results;
        results.reserve(static_cast<std::size_t>(argc - 4));

        bool allOk = true;

        for (int argIndex = 4; argIndex < argc; ++argIndex) {
            const std::size_t graphIndex = static_cast<std::size_t>(argIndex - 4);
            const fs::path graphPath = argv[argIndex];
            const fs::path outputPath = makeOutputPath(outputDirectory, graphPath, graphIndex);
            const fs::path temporaryPath = temporaryPathFor(outputPath);

            GraphRunResult result;
            result.graphPath = graphPath;
            result.outputPath = outputPath;

            std::cout
                << "\n[contest_runner_probe] Processing graph "
                << graphIndex
                << ": " << graphPath
                << '\n';

            if (!fs::exists(graphPath)) {
                std::cerr
                    << "[contest_runner_probe] Graph does not exist: "
                    << graphPath << '\n';

                allOk = false;
                results.push_back(result);
                continue;
            }

            fs::remove(temporaryPath);

            const bool runnerOk = runCheckpointedSa(
                checkpointedSaExecutable,
                graphPath,
                outputPath,
                seed,
                iterations
            );

            result.checkpointExists = fs::exists(outputPath);
            result.temporaryFileExists = fs::exists(temporaryPath);

            if (runnerOk && result.checkpointExists && !result.temporaryFileExists) {
                result.checkpointValid = validateCheckpoint(solverExecutable, outputPath);
            }

            if (!runnerOk) {
                std::cerr
                    << "[contest_runner_probe] checkpointed_sa_probe failed for graph: "
                    << graphPath << '\n';
            }

            if (!result.checkpointExists) {
                std::cerr
                    << "[contest_runner_probe] Missing checkpoint: "
                    << outputPath << '\n';
            }

            if (result.temporaryFileExists) {
                std::cerr
                    << "[contest_runner_probe] Temporary file was not cleaned up: "
                    << temporaryPath << '\n';
            }

            if (!result.checkpointValid) {
                std::cerr
                    << "[contest_runner_probe] Checkpoint is not valid: "
                    << outputPath << '\n';
            }

            allOk = allOk
                && runnerOk
                && result.checkpointExists
                && !result.temporaryFileExists
                && result.checkpointValid;

            results.push_back(result);
        }

        printSummary(results);

        return allOk ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "[contest_runner_probe] Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
