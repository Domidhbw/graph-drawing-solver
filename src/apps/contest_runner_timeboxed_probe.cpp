#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr int MinimumIterationsPerGraph = 1;
    constexpr int IterationsPerSecond = 100;

    struct Score
    {
        std::size_t k = 0;
        std::size_t crossings = 0;
    };


    struct GraphRunResult
    {
        fs::path graphPath;
        fs::path outputPath;
        int allocatedSeconds = 0;
        int iterations = 0;
        long long durationMs = 0;
        bool resume = false;
        bool checkpointExists = false;
        bool temporaryFileExists = false;
        bool checkpointValid = false;
        std::optional<Score> score;
    };


    std::string shellQuote(const fs::path& path)
    {
        const std::string value = path.string();

        std::string quoted;
        quoted.reserve(value.size() + 2);
        quoted.push_back('\'');

        for (const char ch : value)
        {
            if (ch == '\'')
            {
                quoted += "'\\''";
            }
            else
            {
                quoted.push_back(ch);
            }
        }

        quoted.push_back('\'');
        return quoted;
    }

    std::string shellQuote(const std::string& value)
    {
        std::string quoted;
        quoted.reserve(value.size() + 2);
        quoted.push_back('\'');

        for (const char ch : value)
        {
            if (ch == '\'')
            {
                quoted += "'\\''";
            }
            else
            {
                quoted.push_back(ch);
            }
        }

        quoted.push_back('\'');
        return quoted;
    }

    int runCommand(const std::string& command)
    {
        std::cout << "[contest_runner_timeboxed_probe] $ " << command << '\n';
        return std::system(command.c_str());
    }

    std::string readCommandOutput(const std::string& command)
    {
        std::cout << "[contest_runner_timeboxed_probe] $ " << command << '\n';

        std::array<char, 256> buffer{};
        std::string output;

        FILE* pipe = popen(command.c_str(), "r");
        if (pipe == nullptr)
        {
            throw std::runtime_error("Could not execute command: " + command);
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        {
            output += buffer.data();
        }

        const int status = pclose(pipe);
        if (status != 0)
        {
            throw std::runtime_error("Command failed: " + command + "\nOutput:\n" + output);
        }

        return output;
    }

    fs::path executableDirectory(const char* argv0)
    {
        fs::path executablePath(argv0);

        if (executablePath.has_parent_path())
        {
            return fs::absolute(executablePath).parent_path();
        }

        return fs::current_path();
    }

    int parseNonNegativeInt(const std::string& value, const std::string& name)
    {
        if (value.empty() || value.front() == '-')
        {
            throw std::runtime_error(name + " must be a non-negative integer.");
        }

        std::size_t parsedCharacters = 0;
        const int parsed = std::stoi(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error(name + " must be a non-negative integer.");
        }

        return parsed;
    }

    unsigned int parseSeed(const std::string& value)
    {
        if (value.empty() || value.front() == '-')
        {
            throw std::runtime_error("Seed must be a non-negative unsigned integer.");
        }

        std::size_t parsedCharacters = 0;
        const unsigned long parsed = std::stoul(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error("Seed must be a non-negative unsigned integer.");
        }

        return static_cast<unsigned int>(parsed);
    }

    fs::path makeOutputPath(
        const fs::path& outputDirectory,
        const fs::path& graphPath,
        const std::size_t index
    )
    {
        const std::string stem = graphPath.stem().string().empty()
            ? "graph"
            : graphPath.stem().string();

        std::ostringstream fileName;
        fileName << index << "_" << stem << ".json";

        return outputDirectory / fileName.str();
    }

    fs::path telemetryCsvPathFor(const fs::path& outputDirectory)
    {
        return outputDirectory / "timeboxed_summary.csv";
    }

    fs::path temporaryPathFor(const fs::path& outputPath)
    {
        return fs::path(outputPath.string() + ".tmp");
    }

    int allocateSecondsForGraph(
        const int totalSeconds,
        const std::size_t graphCount
    )
    {
        if (graphCount == 0)
        {
            return 0;
        }

        return std::max(0, totalSeconds / static_cast<int>(graphCount));
    }

    int iterationsForAllocatedSeconds(const int allocatedSeconds)
    {
        return std::max(
            MinimumIterationsPerGraph,
            allocatedSeconds * IterationsPerSecond
        );
    }

    bool validateCheckpoint(
        const fs::path& solverExecutable,
        const fs::path& checkpointPath
    )
    {
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
        const unsigned int seed,
        const int iterations,
        const bool resume
    )
    {
        std::ostringstream command;
        command
            << shellQuote(checkpointedSaExecutable)
            << ' '
            << shellQuote(graphPath)
            << ' '
            << shellQuote(outputPath)
            << ' '
            << seed
            << ' '
            << iterations;

        if (resume)
        {
            command << " --resume";
        }

        return runCommand(command.str()) == 0;
    }


    std::optional<std::size_t> extractSizeValue(
        const std::string& output,
        const std::string& label
    )
    {
        std::istringstream stream(output);
        std::string line;

        const std::string prefix = label + ": ";

        while (std::getline(stream, line))
        {
            if (line.rfind(prefix, 0) != 0)
            {
                continue;
            }

            const std::string value = line.substr(prefix.size());
            std::size_t parsedCharacters = 0;
            const unsigned long long parsed = std::stoull(value, &parsedCharacters);

            if (parsedCharacters != value.size())
            {
                return std::nullopt;
            }

            return static_cast<std::size_t>(parsed);
        }

        return std::nullopt;
    }

    std::optional<Score> readScore(
        const fs::path& solverExecutable,
        const fs::path& checkpointPath
    )
    {
        std::ostringstream command;
        command
            << shellQuote(solverExecutable)
            << ' '
            << shellQuote(checkpointPath);

        const std::string output = readCommandOutput(command.str());

        const std::optional<std::size_t> k = extractSizeValue(output, "K");
        const std::optional<std::size_t> crossings = extractSizeValue(output, "Crossings");

        if (!k.has_value() || !crossings.has_value())
        {
            std::cerr
                << "[contest_runner_timeboxed_probe] Could not parse score for "
                << checkpointPath
                << "\nOutput:\n"
                << output
                << '\n';

            return std::nullopt;
        }

        return Score{
            *k,
            *crossings
        };
    }

    std::string csvEscape(const std::string& value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 2);
        escaped.push_back('"');

        for (const char ch : value)
        {
            if (ch == '"')
            {
                escaped += "\"\"";
            }
            else
            {
                escaped.push_back(ch);
            }
        }

        escaped.push_back('"');
        return escaped;
    }


    void writeTelemetryCsv(
        const fs::path& path,
        const std::vector<GraphRunResult>& results
    )
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open telemetry CSV: " + path.string());
        }

        file
            << "graph,output,allocated_seconds,iterations,duration_ms,resume,k,crossings,exists,tmp,valid\n";

        for (const GraphRunResult& result : results)
        {
            file
                << csvEscape(result.graphPath.string()) << ','
                << csvEscape(result.outputPath.string()) << ','
                << result.allocatedSeconds << ','
                << result.iterations << ','
                << result.durationMs << ','
                << (result.resume ? "yes" : "no")
                << ',';

            if (result.score.has_value())
            {
                file
                    << result.score->k << ','
                    << result.score->crossings << ',';
            }
            else
            {
                file << ",,";
            }

            file
                << (result.checkpointExists ? "yes" : "no") << ','
                << (result.temporaryFileExists ? "yes" : "no") << ','
                << (result.checkpointValid ? "yes" : "no")
                << '\n';
        }
    }



    void printUsage(const char* programName)
    {
        std::cerr
            << "Usage:\n"
            << "  " << programName
            << " <output_dir> <seed> <total_seconds> <graph1.json> [graph2.json ...] [--resume]\n";
    }


    void printSummary(
        const std::vector<GraphRunResult>& results,
        const fs::path& telemetryCsvPath
    )
    {
        std::cout << "\n[contest_runner_timeboxed_probe] Summary\n";
        std::cout << "------------------------------------------------------------\n";

        std::size_t validCount = 0;

        for (const GraphRunResult& result : results)
        {
            if (result.checkpointValid)
            {
                ++validCount;
            }

            std::cout
                << "graph=" << result.graphPath
                << " output=" << result.outputPath
                << " allocated_seconds=" << result.allocatedSeconds
                << " iterations=" << result.iterations
                << " duration_ms=" << result.durationMs
                << " resume=" << (result.resume ? "yes" : "no")
                << " k=" << (result.score.has_value() ? std::to_string(result.score->k) : "n/a")
                << " crossings=" << (result.score.has_value() ? std::to_string(result.score->crossings) : "n/a")
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
        std::cout << "telemetry_csv=" << telemetryCsvPath << '\n';
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 5)
        {
            printUsage(argv[0]);
            return 2;
        }

        bool resume = false;
        int graphArgumentEnd = argc;

        if (std::string(argv[argc - 1]) == "--resume")
        {
            resume = true;
            graphArgumentEnd = argc - 1;
        }

        if (graphArgumentEnd <= 4)
        {
            printUsage(argv[0]);
            return 2;
        }

        const fs::path outputDirectory = argv[1];
        const unsigned int seed = parseSeed(argv[2]);
        const int totalSeconds = parseNonNegativeInt(argv[3], "Total seconds");

        fs::create_directories(outputDirectory);

        const fs::path executableDir = executableDirectory(argv[0]);
        const fs::path checkpointedSaExecutable = executableDir / "checkpointed_sa_probe";
        const fs::path solverExecutable = executableDir / "solver";
        const fs::path telemetryCsvPath = telemetryCsvPathFor(outputDirectory);

        if (!fs::exists(checkpointedSaExecutable))
        {
            std::cerr
                << "[contest_runner_timeboxed_probe] Missing executable: "
                << checkpointedSaExecutable << '\n';
            return 1;
        }

        if (!fs::exists(solverExecutable))
        {
            std::cerr
                << "[contest_runner_timeboxed_probe] Missing executable: "
                << solverExecutable << '\n';
            return 1;
        }

        const std::size_t graphCount = static_cast<std::size_t>(graphArgumentEnd - 4);
        const int allocatedSeconds = allocateSecondsForGraph(
            totalSeconds,
            graphCount
        );

        std::vector<GraphRunResult> results;
        results.reserve(graphCount);

        bool allOk = true;

        std::cout
            << "[contest_runner_timeboxed_probe] total_seconds=" << totalSeconds
            << " graph_count=" << graphCount
            << " allocated_seconds_per_graph=" << allocatedSeconds
            << " iterations_per_second=" << IterationsPerSecond
            << " resume=" << (resume ? "yes" : "no")
            << '\n';

        for (int argIndex = 4; argIndex < graphArgumentEnd; ++argIndex)
        {
            const std::size_t graphIndex = static_cast<std::size_t>(argIndex - 4);
            const fs::path graphPath = argv[argIndex];
            const fs::path outputPath = makeOutputPath(
                outputDirectory,
                graphPath,
                graphIndex
            );
            const fs::path temporaryPath = temporaryPathFor(outputPath);

            const int iterations = iterationsForAllocatedSeconds(allocatedSeconds);

            GraphRunResult result;
            result.graphPath = graphPath;
            result.outputPath = outputPath;
            result.allocatedSeconds = allocatedSeconds;
            result.iterations = iterations;
            result.resume = resume;

            std::cout
                << "\n[contest_runner_timeboxed_probe] Processing graph "
                << graphIndex
                << ": " << graphPath
                << '\n';

            if (!fs::exists(graphPath))
            {
                std::cerr
                    << "[contest_runner_timeboxed_probe] Graph does not exist: "
                    << graphPath << '\n';

                allOk = false;
                results.push_back(result);
                continue;
            }

            fs::remove(temporaryPath);

            const auto start = std::chrono::steady_clock::now();

            const bool runnerOk = runCheckpointedSa(
                checkpointedSaExecutable,
                graphPath,
                outputPath,
                seed,
                iterations,
                resume
            );

            const auto end = std::chrono::steady_clock::now();

            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start
            ).count();

            result.checkpointExists = fs::exists(outputPath);
            result.temporaryFileExists = fs::exists(temporaryPath);

            if (runnerOk && result.checkpointExists && !result.temporaryFileExists)
            {
                result.checkpointValid = validateCheckpoint(
                    solverExecutable,
                    outputPath
                );

                if (result.checkpointValid)
                {
                    result.score = readScore(
                        solverExecutable,
                        outputPath
                    );
                }
            }

            if (!runnerOk)
            {
                std::cerr
                    << "[contest_runner_timeboxed_probe] checkpointed_sa_probe failed for graph: "
                    << graphPath << '\n';
            }

            if (!result.checkpointExists)
            {
                std::cerr
                    << "[contest_runner_timeboxed_probe] Missing checkpoint: "
                    << outputPath << '\n';
            }

            if (result.temporaryFileExists)
            {
                std::cerr
                    << "[contest_runner_timeboxed_probe] Temporary file was not cleaned up: "
                    << temporaryPath << '\n';
            }

            if (!result.checkpointValid)
            {
                std::cerr
                    << "[contest_runner_timeboxed_probe] Checkpoint is not valid: "
                    << outputPath << '\n';
            }

            allOk = allOk
                && runnerOk
                && result.checkpointExists
                && !result.temporaryFileExists
                && result.checkpointValid
                && result.score.has_value();

            results.push_back(result);
        }

        writeTelemetryCsv(
            telemetryCsvPath,
            results
        );

        printSummary(
            results,
            telemetryCsvPath
        );

        return allOk ? 0 : 1;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "[contest_runner_timeboxed_probe] Fatal error: "
            << exception.what()
            << '\n';

        printUsage(argv[0]);
        return 1;
    }
}
