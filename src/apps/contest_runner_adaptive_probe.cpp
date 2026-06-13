
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
    struct Score
    {
        std::size_t k = 0;
        std::size_t crossings = 0;
    };

    struct GraphState
    {
        fs::path graphPath;
        fs::path outputPath;
        std::optional<Score> score;
        bool valid = false;
    };

    struct RunRecord
    {
        int round = 0;
        std::size_t graphIndex = 0;
        fs::path graphPath;
        fs::path outputPath;
        bool resume = false;
        int iterations = 0;
        long long durationMs = 0;
        bool checkpointExists = false;
        bool temporaryFileExists = false;
        bool checkpointValid = false;
        std::optional<Score> score;
        std::string reason;
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
        std::cout << "[contest_runner_adaptive_probe] $ " << command << '\n';
        return std::system(command.c_str());
    }

    std::string readCommandOutput(const std::string& command)
    {
        std::cout << "[contest_runner_adaptive_probe] $ " << command << '\n';

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

    fs::path temporaryPathFor(const fs::path& outputPath)
    {
        return fs::path(outputPath.string() + ".tmp");
    }

    fs::path telemetryCsvPathFor(const fs::path& outputDirectory)
    {
        return outputDirectory / "adaptive_summary.csv";
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
                << "[contest_runner_adaptive_probe] Could not parse score for "
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

    bool isWorseScore(const Score& candidate, const Score& currentWorst)
    {
        if (candidate.k != currentWorst.k)
        {
            return candidate.k > currentWorst.k;
        }

        return candidate.crossings > currentWorst.crossings;
    }

    std::size_t selectWorstGraphIndex(const std::vector<GraphState>& states)
    {
        std::optional<std::size_t> selectedIndex;
        std::optional<Score> selectedScore;

        for (std::size_t index = 0; index < states.size(); ++index)
        {
            const GraphState& state = states[index];

            if (!state.valid || !state.score.has_value())
            {
                continue;
            }

            if (!selectedIndex.has_value())
            {
                selectedIndex = index;
                selectedScore = state.score;
                continue;
            }

            if (isWorseScore(*state.score, *selectedScore))
            {
                selectedIndex = index;
                selectedScore = state.score;
            }
        }

        if (!selectedIndex.has_value())
        {
            throw std::runtime_error("No valid graph state available for adaptive scheduling.");
        }

        return *selectedIndex;
    }

    RunRecord runGraph(
        GraphState& state,
        const std::size_t graphIndex,
        const int round,
        const std::string& reason,
        const fs::path& checkpointedSaExecutable,
        const fs::path& solverExecutable,
        const unsigned int seed,
        const int iterations,
        const bool resume
    )
    {
        RunRecord record;
        record.round = round;
        record.graphIndex = graphIndex;
        record.graphPath = state.graphPath;
        record.outputPath = state.outputPath;
        record.resume = resume;
        record.iterations = iterations;
        record.reason = reason;

        const fs::path temporaryPath = temporaryPathFor(state.outputPath);

        fs::remove(temporaryPath);

        const auto start = std::chrono::steady_clock::now();

        const bool runnerOk = runCheckpointedSa(
            checkpointedSaExecutable,
            state.graphPath,
            state.outputPath,
            seed,
            iterations,
            resume
        );

        const auto end = std::chrono::steady_clock::now();

        record.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start
        ).count();

        record.checkpointExists = fs::exists(state.outputPath);
        record.temporaryFileExists = fs::exists(temporaryPath);

        if (runnerOk && record.checkpointExists && !record.temporaryFileExists)
        {
            record.checkpointValid = validateCheckpoint(
                solverExecutable,
                state.outputPath
            );

            if (record.checkpointValid)
            {
                record.score = readScore(
                    solverExecutable,
                    state.outputPath
                );
            }
        }

        state.valid = record.checkpointValid;
        state.score = record.score;

        if (!runnerOk)
        {
            std::cerr
                << "[contest_runner_adaptive_probe] checkpointed_sa_probe failed for graph: "
                << state.graphPath << '\n';
        }

        if (!record.checkpointExists)
        {
            std::cerr
                << "[contest_runner_adaptive_probe] Missing checkpoint: "
                << state.outputPath << '\n';
        }

        if (record.temporaryFileExists)
        {
            std::cerr
                << "[contest_runner_adaptive_probe] Temporary file was not cleaned up: "
                << temporaryPath << '\n';
        }

        if (!record.checkpointValid)
        {
            std::cerr
                << "[contest_runner_adaptive_probe] Checkpoint is not valid: "
                << state.outputPath << '\n';
        }

        return record;
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
        const std::vector<RunRecord>& records
    )
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open telemetry CSV: " + path.string());
        }

        file
            << "round,graph_index,graph,output,resume,iterations,duration_ms,k,crossings,exists,tmp,valid,reason\n";

        for (const RunRecord& record : records)
        {
            file
                << record.round << ','
                << record.graphIndex << ','
                << csvEscape(record.graphPath.string()) << ','
                << csvEscape(record.outputPath.string()) << ','
                << (record.resume ? "yes" : "no") << ','
                << record.iterations << ','
                << record.durationMs << ',';

            if (record.score.has_value())
            {
                file
                    << record.score->k << ','
                    << record.score->crossings << ',';
            }
            else
            {
                file << ",,";
            }

            file
                << (record.checkpointExists ? "yes" : "no") << ','
                << (record.temporaryFileExists ? "yes" : "no") << ','
                << (record.checkpointValid ? "yes" : "no") << ','
                << csvEscape(record.reason)
                << '\n';
        }
    }

    bool isSuccessfulRecord(const RunRecord& record)
    {
        return record.checkpointExists
            && !record.temporaryFileExists
            && record.checkpointValid
            && record.score.has_value();
    }

    void printUsage(const char* programName)
    {
        std::cerr
            << "Usage:\n"
            << "  " << programName
            << " <output_dir> <seed> <adaptive_rounds> <iterations_per_round> <graph1.json> [graph2.json ...]\n";
    }

    void printSummary(
        const std::vector<GraphState>& states,
        const std::vector<RunRecord>& records,
        const fs::path& telemetryCsvPath
    )
    {
        std::cout << "\n[contest_runner_adaptive_probe] Summary\n";
        std::cout << "------------------------------------------------------------\n";

        std::size_t validCount = 0;

        for (std::size_t index = 0; index < states.size(); ++index)
        {
            const GraphState& state = states[index];

            if (state.valid)
            {
                ++validCount;
            }

            std::cout
                << "graph_index=" << index
                << " graph=" << state.graphPath
                << " output=" << state.outputPath
                << " k=" << (state.score.has_value() ? std::to_string(state.score->k) : "n/a")
                << " crossings=" << (state.score.has_value() ? std::to_string(state.score->crossings) : "n/a")
                << " valid=" << (state.valid ? "yes" : "no")
                << '\n';
        }

        std::cout << "------------------------------------------------------------\n";
        std::cout
            << "valid_checkpoints=" << validCount
            << "/" << states.size()
            << '\n';
        std::cout << "run_records=" << records.size() << '\n';
        std::cout << "telemetry_csv=" << telemetryCsvPath << '\n';
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 6)
        {
            printUsage(argv[0]);
            return 2;
        }

        const fs::path outputDirectory = argv[1];
        const unsigned int seed = parseSeed(argv[2]);
        const int adaptiveRounds = parseNonNegativeInt(argv[3], "Adaptive rounds");
        const int iterationsPerRound = parseNonNegativeInt(argv[4], "Iterations per round");

        if (iterationsPerRound <= 0)
        {
            throw std::runtime_error("Iterations per round must be positive.");
        }

        fs::create_directories(outputDirectory);

        const fs::path executableDir = executableDirectory(argv[0]);
        const fs::path checkpointedSaExecutable = executableDir / "checkpointed_sa_probe";
        const fs::path solverExecutable = executableDir / "solver";
        const fs::path telemetryCsvPath = telemetryCsvPathFor(outputDirectory);

        if (!fs::exists(checkpointedSaExecutable))
        {
            std::cerr
                << "[contest_runner_adaptive_probe] Missing executable: "
                << checkpointedSaExecutable << '\n';
            return 1;
        }

        if (!fs::exists(solverExecutable))
        {
            std::cerr
                << "[contest_runner_adaptive_probe] Missing executable: "
                << solverExecutable << '\n';
            return 1;
        }

        std::vector<GraphState> states;
        states.reserve(static_cast<std::size_t>(argc - 5));

        for (int argIndex = 5; argIndex < argc; ++argIndex)
        {
            const std::size_t graphIndex = static_cast<std::size_t>(argIndex - 5);
            const fs::path graphPath = argv[argIndex];

            if (!fs::exists(graphPath))
            {
                throw std::runtime_error("Graph does not exist: " + graphPath.string());
            }

            states.push_back(GraphState{
                graphPath,
                makeOutputPath(
                    outputDirectory,
                    graphPath,
                    graphIndex
                ),
                std::nullopt,
                false
            });
        }

        std::vector<RunRecord> records;

        bool allOk = true;

        std::cout
            << "[contest_runner_adaptive_probe] graph_count=" << states.size()
            << " adaptive_rounds=" << adaptiveRounds
            << " iterations_per_round=" << iterationsPerRound
            << '\n';

        for (std::size_t index = 0; index < states.size(); ++index)
        {
            std::cout
                << "\n[contest_runner_adaptive_probe] Bootstrap graph "
                << index
                << ": " << states[index].graphPath
                << '\n';

            RunRecord record = runGraph(
                states[index],
                index,
                0,
                "bootstrap",
                checkpointedSaExecutable,
                solverExecutable,
                seed,
                iterationsPerRound,
                false
            );

            allOk = allOk && isSuccessfulRecord(record);
            records.push_back(record);
        }

        for (int round = 1; round <= adaptiveRounds; ++round)
        {
            const std::size_t selectedIndex = selectWorstGraphIndex(states);

            std::cout
                << "\n[contest_runner_adaptive_probe] Adaptive round "
                << round
                << " selected graph "
                << selectedIndex
                << ": " << states[selectedIndex].graphPath
                << '\n';

            RunRecord record = runGraph(
                states[selectedIndex],
                selectedIndex,
                round,
                "worst-score",
                checkpointedSaExecutable,
                solverExecutable,
                seed + static_cast<unsigned int>(round),
                iterationsPerRound,
                true
            );

            allOk = allOk && isSuccessfulRecord(record);
            records.push_back(record);
        }

        writeTelemetryCsv(
            telemetryCsvPath,
            records
        );

        printSummary(
            states,
            records,
            telemetryCsvPath
        );

        return allOk ? 0 : 1;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "[contest_runner_adaptive_probe] Fatal error: "
            << exception.what()
            << '\n';

        printUsage(argv[0]);
        return 1;
    }
}
