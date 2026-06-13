
#include "BaselineLayouts.hpp"
#include "BestSolutionStore.hpp"
#include "FruchtermanReingoldLayout.hpp"
#include "Graph.hpp"
#include "JsonReader.hpp"
#include "LayoutScore.hpp"
#include "SimulatedAnnealingOptimizer.hpp"
#include "SolutionRepairer.hpp"
#include "SolutionValidator.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct ProgramOptions
    {
        std::string inputPath;
        std::string outputPath;
        unsigned int seed = 0;
        int iterations = 0;
        bool resume = false;
        double timeBudgetSeconds = -1.0;
        int restarts = 1;
    };

    struct InitialLayoutCandidate
    {
        std::string name;
        Graph graph;
    };

    struct InitialLayoutSelection
    {
        std::string name;
        Graph graph;
        LayoutScore score;
        bool hasSelection = false;
    };

    void printUsage()
    {
        std::cerr
            << "Usage: checkpointed_sa_probe <input.json> <output.json> <seed> <iterations>"
            << " [--resume] [--seconds <wall_clock_budget>] [--restarts <count>]\n"
            << "  With --seconds the SA cooling schedule follows wall-clock time and stops"
            << " at the budget; <iterations> then acts only as an upper bound.\n"
            << "  With --restarts <count> the total budget is split across that many"
            << " independent SA passes (multi-start); the global best is kept.\n";
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

    int parseIterations(const std::string& value)
    {
        std::size_t parsedCharacters = 0;
        const int parsed = std::stoi(value, &parsedCharacters);

        if (parsedCharacters != value.size() || parsed < 0)
        {
            throw std::runtime_error("Iterations must be a non-negative integer.");
        }

        return parsed;
    }

    ProgramOptions parseProgramOptions(int argc, char* argv[])
    {
        if (argc < 5)
        {
            throw std::runtime_error("Invalid argument count.");
        }

        ProgramOptions options;
        options.inputPath = argv[1];
        options.outputPath = argv[2];
        options.seed = parseSeed(argv[3]);
        options.iterations = parseIterations(argv[4]);

        for (int argIndex = 5; argIndex < argc; ++argIndex)
        {
            const std::string flag = argv[argIndex];

            if (flag == "--resume")
            {
                options.resume = true;
            }
            else if (flag == "--seconds")
            {
                if (argIndex + 1 >= argc)
                {
                    throw std::runtime_error("--seconds requires a value.");
                }

                std::size_t parsedCharacters = 0;
                const double parsed = std::stod(argv[++argIndex], &parsedCharacters);

                if (parsedCharacters == 0 || parsed <= 0.0)
                {
                    throw std::runtime_error("--seconds must be a positive number.");
                }

                options.timeBudgetSeconds = parsed;
            }
            else if (flag == "--restarts")
            {
                if (argIndex + 1 >= argc)
                {
                    throw std::runtime_error("--restarts requires a value.");
                }

                options.restarts = parseIterations(argv[++argIndex]);

                if (options.restarts < 1)
                {
                    throw std::runtime_error("--restarts must be at least 1.");
                }
            }
            else
            {
                throw std::runtime_error("Unknown option: " + flag);
            }
        }

        return options;
    }

    bool isValid(const Graph& graph)
    {
        return SolutionValidator::validate(graph).isValid();
    }

    void printValidationErrors(const Graph& graph)
    {
        const ValidationResult validationResult = SolutionValidator::validate(graph);

        for (const std::string& error : validationResult.errors)
        {
            std::cerr << "Validation error: " << error << '\n';
        }
    }

    bool hasSameGraphStructure(const Graph& referenceGraph, const Graph& candidateGraph)
    {
        if (referenceGraph.nodes.size() != candidateGraph.nodes.size())
        {
            return false;
        }

        if (referenceGraph.edges.size() != candidateGraph.edges.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < referenceGraph.nodes.size(); ++index)
        {
            if (referenceGraph.nodes[index].id != candidateGraph.nodes[index].id)
            {
                return false;
            }
        }

        for (std::size_t index = 0; index < referenceGraph.edges.size(); ++index)
        {
            if (referenceGraph.edges[index].source != candidateGraph.edges[index].source)
            {
                return false;
            }

            if (referenceGraph.edges[index].target != candidateGraph.edges[index].target)
            {
                return false;
            }
        }

        return true;
    }

    Graph createOriginalCandidate(const Graph& inputGraph)
    {
        return inputGraph;
    }

    Graph createGridCandidate(const Graph& inputGraph)
    {
        Graph graph = inputGraph;

        BaselineLayouts::applyGrid(
            graph,
            static_cast<double>(graph.width),
            static_cast<double>(graph.height)
        );

        return graph;
    }

    Graph createCircularCandidate(const Graph& inputGraph)
    {
        Graph graph = inputGraph;

        BaselineLayouts::applyCircular(
            graph,
            static_cast<double>(graph.width),
            static_cast<double>(graph.height)
        );

        return graph;
    }

    Graph createRandomCandidate(
        const Graph& inputGraph,
        const unsigned int seed
    )
    {
        Graph graph = inputGraph;

        BaselineLayouts::applyRandom(
            graph,
            static_cast<double>(graph.width),
            static_cast<double>(graph.height),
            seed
        );

        return graph;
    }

    Graph createFruchtermanReingoldCandidate(const Graph& inputGraph)
    {
        Graph graph = inputGraph;

        FruchtermanReingoldLayout::apply(
            graph,
            static_cast<double>(graph.width),
            static_cast<double>(graph.height),
            500
        );

        return graph;
    }

    std::vector<InitialLayoutCandidate> createInitialLayoutCandidates(
        const Graph& inputGraph,
        const unsigned int seed
    )
    {
        std::vector<InitialLayoutCandidate> candidates;
        candidates.reserve(5);

        candidates.push_back(InitialLayoutCandidate{
            "original",
            createOriginalCandidate(inputGraph)
        });

        candidates.push_back(InitialLayoutCandidate{
            "grid",
            createGridCandidate(inputGraph)
        });

        candidates.push_back(InitialLayoutCandidate{
            "circular",
            createCircularCandidate(inputGraph)
        });

        candidates.push_back(InitialLayoutCandidate{
            "random",
            createRandomCandidate(inputGraph, seed)
        });

        candidates.push_back(InitialLayoutCandidate{
            "fr",
            createFruchtermanReingoldCandidate(inputGraph)
        });

        return candidates;
    }

    bool shouldReplaceSelection(
        const InitialLayoutSelection& selection,
        const LayoutScore& candidateScore
    )
    {
        if (!selection.hasSelection)
        {
            return true;
        }

        return LayoutScoreCalculator::isBetter(
            candidateScore,
            selection.score
        );
    }

    void printCandidateStatus(
        const std::string& name,
        const bool valid,
        const LayoutScore& score
    )
    {
        std::cout
            << "Initial candidate: " << name
            << " valid=" << (valid ? "yes" : "no")
            << " k=" << score.maxCrossingsPerEdge
            << " crossings=" << score.totalCrossings
            << '\n';
    }

    Graph createValidStartGraph(
        const Graph& inputGraph,
        const unsigned int seed
    )
    {
        InitialLayoutSelection selection;

        std::vector<InitialLayoutCandidate> candidates = createInitialLayoutCandidates(
            inputGraph,
            seed
        );

        for (InitialLayoutCandidate& candidate : candidates)
        {
            SolutionRepairer::repairDuplicateRoundedCoordinates(candidate.graph);

            const LayoutScore candidateScore = LayoutScoreCalculator::compute(candidate.graph);
            const bool candidateIsValid = isValid(candidate.graph);

            printCandidateStatus(
                candidate.name,
                candidateIsValid,
                candidateScore
            );

            if (!candidateIsValid)
            {
                continue;
            }

            if (shouldReplaceSelection(selection, candidateScore))
            {
                selection.name = candidate.name;
                selection.graph = std::move(candidate.graph);
                selection.score = candidateScore;
                selection.hasSelection = true;
            }
        }

        if (!selection.hasSelection)
        {
            printValidationErrors(inputGraph);
            throw std::runtime_error("Could not create a valid checkpoint start graph.");
        }

        std::cout
            << "Selected initial layout: " << selection.name
            << " k=" << selection.score.maxCrossingsPerEdge
            << " crossings=" << selection.score.totalCrossings
            << '\n';

        return selection.graph;
    }

    // Selects the candidate with the best score (lowest k), regardless of
    // validity. This deliberately allows a marginally invalid but geometrically
    // strong start (e.g. Fruchterman-Reingold): SA plus repair recover validity,
    // and the validity gate of BestSolutionStore guarantees a valid final result.
    // Starting SA only from a valid-but-poor layout (e.g. circular) leaves the
    // optimizer stuck near the baseline.
    Graph createBestStartGraph(
        const Graph& inputGraph,
        const unsigned int seed
    )
    {
        InitialLayoutSelection selection;

        std::vector<InitialLayoutCandidate> candidates = createInitialLayoutCandidates(
            inputGraph,
            seed
        );

        for (InitialLayoutCandidate& candidate : candidates)
        {
            SolutionRepairer::repairDuplicateRoundedCoordinates(candidate.graph);

            const LayoutScore candidateScore = LayoutScoreCalculator::compute(candidate.graph);

            if (shouldReplaceSelection(selection, candidateScore))
            {
                selection.name = candidate.name;
                selection.graph = std::move(candidate.graph);
                selection.score = candidateScore;
                selection.hasSelection = true;
            }
        }

        if (!selection.hasSelection)
        {
            throw std::runtime_error("Could not create an SA start graph.");
        }

        std::cout
            << "SA start layout: " << selection.name
            << " k=" << selection.score.maxCrossingsPerEdge
            << " crossings=" << selection.score.totalCrossings
            << '\n';

        return selection.graph;
    }

    bool canResumeFromCheckpoint(
        const Graph& inputGraph,
        const std::string& outputPath,
        Graph& checkpointGraph
    )
    {
        if (!std::filesystem::exists(outputPath))
        {
            return false;
        }

        if (std::filesystem::exists(outputPath + ".tmp"))
        {
            throw std::runtime_error("Cannot resume while temporary checkpoint file exists.");
        }

        checkpointGraph = JsonReader::load(outputPath);

        if (!hasSameGraphStructure(inputGraph, checkpointGraph))
        {
            throw std::runtime_error("Existing checkpoint does not match input graph structure.");
        }

        if (!isValid(checkpointGraph))
        {
            printValidationErrors(checkpointGraph);
            throw std::runtime_error("Existing checkpoint is invalid.");
        }

        return true;
    }

    Graph createStartGraph(
        const Graph& inputGraph,
        const ProgramOptions& options,
        bool& resumedFromCheckpoint
    )
    {
        resumedFromCheckpoint = false;

        if (options.resume)
        {
            Graph checkpointGraph;

            if (canResumeFromCheckpoint(
                    inputGraph,
                    options.outputPath,
                    checkpointGraph
                ))
            {
                resumedFromCheckpoint = true;

                const LayoutScore checkpointScore = LayoutScoreCalculator::compute(checkpointGraph);

                std::cout
                    << "Resumed from checkpoint: yes"
                    << " k=" << checkpointScore.maxCrossingsPerEdge
                    << " crossings=" << checkpointScore.totalCrossings
                    << '\n';

                return checkpointGraph;
            }

            std::cout << "Resumed from checkpoint: no\n";
        }

        return createValidStartGraph(
            inputGraph,
            options.seed
        );
    }

    SimulatedAnnealingOptions createOptions(
        const Graph& graph,
        const unsigned int seed,
        const int iterations,
        const double timeBudgetSeconds
    )
    {
        return SimulatedAnnealingOptions{
            static_cast<double>(graph.width),
            static_cast<double>(graph.height),
            iterations,
            seed,
            10.0,
            0.01,
            80.0,
            2.0,
            true,
            true,
            timeBudgetSeconds
        };
    }

    void prepareOutputFiles(const ProgramOptions& options)
    {
        if (options.resume)
        {
            return;
        }

        std::filesystem::remove(options.outputPath);
        std::filesystem::remove(options.outputPath + ".tmp");
    }

    void ensureValidCheckpoint(const std::string& outputPath)
    {
        if (!std::filesystem::exists(outputPath))
        {
            throw std::runtime_error("Checkpoint output was not written.");
        }

        if (std::filesystem::exists(outputPath + ".tmp"))
        {
            throw std::runtime_error("Temporary checkpoint file was not cleaned up.");
        }

        const Graph checkpointGraph = JsonReader::load(outputPath);

        if (!isValid(checkpointGraph))
        {
            printValidationErrors(checkpointGraph);
            throw std::runtime_error("Checkpoint output is invalid.");
        }
    }
}

int main(int argc, char* argv[])
{
    try
    {
        const ProgramOptions options = parseProgramOptions(argc, argv);

        prepareOutputFiles(options);

        const Graph inputGraph = JsonReader::load(options.inputPath);

        bool resumedFromCheckpoint = false;

        // Guaranteed-valid fallback (resume-aware). Seeds the store so a valid
        // result always exists even if every SA pass should fail to stay valid.
        Graph fallbackGraph = createStartGraph(
            inputGraph,
            options,
            resumedFromCheckpoint
        );

        BestSolutionStore bestSolutionStore(options.outputPath);

        const bool initialStored = bestSolutionStore.consider(fallbackGraph);

        // Multi-start: split the total budget across independent SA passes and
        // keep the global best. restarts == 1 reproduces the single-pass behaviour.
        const int perRestartIterations = std::max(
            1,
            options.iterations / options.restarts
        );
        const double perRestartTimeBudget = options.timeBudgetSeconds > 0.0
            ? options.timeBudgetSeconds / static_cast<double>(options.restarts)
            : options.timeBudgetSeconds;

        int storedCount = 0;

        for (int restart = 0; restart < options.restarts; ++restart)
        {
            const unsigned int restartSeed = options.seed + static_cast<unsigned int>(restart);

            // A resumed checkpoint is continued as-is; otherwise SA starts from
            // the best-scoring candidate (incl. FR), which is essential for the
            // optimizer to escape the baseline.
            Graph restartGraph = (resumedFromCheckpoint && restart == 0)
                ? fallbackGraph
                : createBestStartGraph(inputGraph, restartSeed);

            SimulatedAnnealingOptimizer::optimize(
                restartGraph,
                createOptions(
                    restartGraph,
                    restartSeed,
                    perRestartIterations,
                    perRestartTimeBudget
                )
            );

            SolutionRepairer::repairDuplicateRoundedCoordinates(restartGraph);

            if (bestSolutionStore.consider(restartGraph))
            {
                ++storedCount;
            }
        }

        const bool optimizedStored = storedCount > 0;

        ensureValidCheckpoint(options.outputPath);

        std::cout << "Checkpointed SA probe passed.\n";
        std::cout << "Input: " << options.inputPath << '\n';
        std::cout << "Output: " << options.outputPath << '\n';
        std::cout << "Seed: " << options.seed << '\n';
        std::cout << "Iterations: " << options.iterations << '\n';
        std::cout << "Restarts: " << options.restarts << '\n';
        if (options.timeBudgetSeconds > 0.0)
        {
            std::cout << "Time budget (s): " << options.timeBudgetSeconds << '\n';
        }
        std::cout << "Resume: " << (options.resume ? "yes" : "no") << '\n';
        std::cout << "Resumed from checkpoint: " << (resumedFromCheckpoint ? "yes" : "no") << '\n';
        std::cout << "Initial stored: " << (initialStored ? "yes" : "no") << '\n';
        std::cout << "Optimized stored: " << (optimizedStored ? "yes" : "no") << '\n';

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
