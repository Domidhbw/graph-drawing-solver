
#include "BaselineLayouts.hpp"
#include "CrossingStatistics.hpp"
#include "FruchtermanReingoldLayout.hpp"
#include "JsonReader.hpp"
#include "JsonWriter.hpp"
#include "LayoutScore.hpp"
#include "SolutionValidator.hpp"
#include "SimulatedAnnealingOptimizer.hpp"
#include "SolutionRepairer.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct SolverSettings
    {
        double layoutWidth = 1000.0;
        double layoutHeight = 1000.0;
        unsigned int randomSeed = 42;
        int fruchtermanReingoldIterations = 500;
        int simulatedAnnealingIterations = 10000;

        // Optional path to an externally produced layout (e.g. FMMM from OGDF via
        // the ogdf_layout tool). When set, it enters the initialisation portfolio
        // as an additional candidate, behind the same validation gate as the
        // built-in baselines. Empty by default, so the standard pipeline is
        // unchanged.
        std::string initCandidatePath;

        bool layoutWidthOverridden = false;
        bool layoutHeightOverridden = false;
    };

    struct ProgramOptions
    {
        std::string inputPath;
        std::string layoutName;
        std::string outputPath;
        SolverSettings settings;

        bool hasLayout() const
        {
            return !layoutName.empty();
        }

        bool hasOutput() const
        {
            return !outputPath.empty();
        }
    };


struct InitialLayoutSelection
{
    Graph bestOptimizationStartGraph;
    LayoutScore bestOptimizationStartScore;
    bool hasOptimizationStart = false;

    Graph bestValidFallbackGraph;
    LayoutScore bestValidFallbackScore;
    bool hasValidFallback = false;
};


    


void printUsage()
{
    std::cerr << "Usage: solver <graph.json> [layout] [output.json] [options]\n";
    std::cerr << "Layouts: random, circular, grid, fr, sa, sa-uniform, sa-index, compare, validate, repair, submission\n";
    std::cerr << '\n';
    std::cerr << "Options:\n";
    std::cerr << "  --seed <uint>\n";
    std::cerr << "  --width <double>\n";
    std::cerr << "  --height <double>\n";
    std::cerr << "  --fr-iterations <int>\n";
    std::cerr << "  --sa-iterations <int>\n";
    std::cerr << "  --init-candidate <layout.json>  (extra portfolio candidate, e.g. FMMM/OGDF)\n";
    std::cerr << '\n';
    std::cerr << "Examples:\n";
    std::cerr << "  solver graph.json\n";
    std::cerr << "  solver graph.json circular\n";
    std::cerr << "  solver graph.json sa result.json\n";
    std::cerr << "  solver graph.json sa-uniform result.json\n";
    std::cerr << "  solver graph.json sa-index result.json\n";
    std::cerr << "  solver graph.json compare\n";
    std::cerr << "  solver graph.json validate\n";
    std::cerr << "  solver graph.json repair repaired.json\n";
    std::cerr << "  solver graph.json submission submission.json\n";
    std::cerr << "  solver graph.json sa result.json --seed 123 --sa-iterations 50000\n";
    std::cerr << "  solver graph.json compare --seed 123 --fr-iterations 1000\n";
    std::cerr << "  solver graph.json sa result.json --width 1000 --height 1000\n";
}



    bool isOption(const std::string& value)
    {
        return value.rfind("--", 0) == 0;
    }

    int parseInt(const std::string& value, const std::string& optionName)
    {
        std::size_t parsedCharacters = 0;
        const int result = std::stoi(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error("Invalid integer value for " + optionName + ": " + value);
        }

        return result;
    }

    unsigned int parseUnsignedInt(const std::string& value, const std::string& optionName)
    {
        if (value.empty() || value.front() == '-')
        {
            throw std::runtime_error("Invalid unsigned integer value for " + optionName + ": " + value);
        }

        std::size_t parsedCharacters = 0;
        const unsigned long result = std::stoul(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error("Invalid unsigned integer value for " + optionName + ": " + value);
        }

        if (result > static_cast<unsigned long>(std::numeric_limits<unsigned int>::max()))
        {
            throw std::runtime_error("Unsigned integer value out of range for " + optionName + ": " + value);
        }

        return static_cast<unsigned int>(result);
    }

    double parseDouble(const std::string& value, const std::string& optionName)
    {
        std::size_t parsedCharacters = 0;
        const double result = std::stod(value, &parsedCharacters);

        if (parsedCharacters != value.size())
        {
            throw std::runtime_error("Invalid double value for " + optionName + ": " + value);
        }

        return result;
    }

    std::string requireOptionValue(
        const std::vector<std::string>& arguments,
        std::size_t& index,
        const std::string& optionName
    )
    {
        if (index + 1 >= arguments.size())
        {
            throw std::runtime_error("Missing value for option: " + optionName);
        }

        const std::string value = arguments[index + 1];

        if (isOption(value))
        {
            throw std::runtime_error("Missing value for option: " + optionName);
        }

        ++index;
        return value;
    }

    void applyCommandLineOption(
        SolverSettings& settings,
        const std::vector<std::string>& arguments,
        std::size_t& index
    )
    {
        const std::string& optionName = arguments[index];

        if (optionName == "--seed")
        {
            settings.randomSeed = parseUnsignedInt(
                requireOptionValue(arguments, index, optionName),
                optionName
            );
            return;
        }

        if (optionName == "--width")
        {
            settings.layoutWidth = parseDouble(
                requireOptionValue(arguments, index, optionName),
                optionName
            );
            settings.layoutWidthOverridden = true;
            return;
        }

        if (optionName == "--height")
        {
            settings.layoutHeight = parseDouble(
                requireOptionValue(arguments, index, optionName),
                optionName
            );
            settings.layoutHeightOverridden = true;
            return;
        }

        if (optionName == "--fr-iterations")
        {
            settings.fruchtermanReingoldIterations = parseInt(
                requireOptionValue(arguments, index, optionName),
                optionName
            );
            return;
        }

        if (optionName == "--sa-iterations")
        {
            settings.simulatedAnnealingIterations = parseInt(
                requireOptionValue(arguments, index, optionName),
                optionName
            );
            return;
        }

        if (optionName == "--init-candidate")
        {
            settings.initCandidatePath =
                requireOptionValue(arguments, index, optionName);
            return;
        }

        throw std::runtime_error("Unknown option: " + optionName);
    }

    void validateSettings(const SolverSettings& settings)
    {
        if (settings.layoutWidth <= 0.0)
        {
            throw std::runtime_error("Width must be positive.");
        }

        if (settings.layoutHeight <= 0.0)
        {
            throw std::runtime_error("Height must be positive.");
        }

        if (settings.fruchtermanReingoldIterations < 0)
        {
            throw std::runtime_error("FR iterations must not be negative.");
        }

        if (settings.simulatedAnnealingIterations < 0)
        {
            throw std::runtime_error("SA iterations must not be negative.");
        }
    }

    ProgramOptions parseProgramOptions(int argc, char* argv[])
    {
        if (argc < 2)
        {
            throw std::runtime_error("Missing input graph path.");
        }

        ProgramOptions options;
        options.inputPath = argv[1];

        std::vector<std::string> arguments;

        for (int index = 2; index < argc; ++index)
        {
            arguments.emplace_back(argv[index]);
        }

        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string& argument = arguments[index];

            if (isOption(argument))
            {
                applyCommandLineOption(options.settings, arguments, index);
                continue;
            }

            if (!options.hasLayout())
            {
                options.layoutName = argument;
                continue;
            }

            if (!options.hasOutput())
            {
                options.outputPath = argument;
                continue;
            }

            throw std::runtime_error("Unexpected positional argument: " + argument);
        }

        validateSettings(options.settings);

        if (options.layoutName == "compare" && options.hasOutput())
        {
            throw std::runtime_error("Compare mode does not support an output JSON path.");
        }

        if (options.layoutName == "validate" && options.hasOutput())
        {
            throw std::runtime_error("Validate mode does not support an output JSON path.");
        }
        
        if (options.layoutName == "repair" && !options.hasOutput())
        {
            throw std::runtime_error("Repair mode requires an output JSON path.");
        }
                
        if (options.layoutName == "submission" && !options.hasOutput())
        {
            throw std::runtime_error("Submission mode requires an output JSON path.");
        }


        return options;
    }

    SolverSettings resolveSettingsForGraph(
        const SolverSettings& parsedSettings,
        const Graph& graph
    )
    {
        SolverSettings resolvedSettings = parsedSettings;

        if (!resolvedSettings.layoutWidthOverridden)
        {
            resolvedSettings.layoutWidth = static_cast<double>(graph.width);
        }

        if (!resolvedSettings.layoutHeightOverridden)
        {
            resolvedSettings.layoutHeight = static_cast<double>(graph.height);
        }

        validateSettings(resolvedSettings);

        return resolvedSettings;
    }

    void applyResolvedBoundsToGraph(Graph& graph, const SolverSettings& settings)
    {
        graph.width = static_cast<int>(settings.layoutWidth);
        graph.height = static_cast<int>(settings.layoutHeight);
    }


SimulatedAnnealingOptions createSimulatedAnnealingOptions(
    const SolverSettings& settings,
    const bool useHotspotSampling,
    const bool useIncrementalCrossingIndex
)
{
    return SimulatedAnnealingOptions{
        settings.layoutWidth,
        settings.layoutHeight,
        settings.simulatedAnnealingIterations,
        settings.randomSeed,
        10.0,
        0.01,
        80.0,
        2.0,
        useHotspotSampling,
        useIncrementalCrossingIndex
    };
}



    bool isGraphValid(const Graph& graph)
    {
        return SolutionValidator::validate(graph).isValid();
    }

    Graph createRandomLayoutGraph(
        const Graph& originalGraph,
        const SolverSettings& settings
    )
    {
        Graph graph = originalGraph;

        BaselineLayouts::applyRandom(
            graph,
            settings.layoutWidth,
            settings.layoutHeight,
            settings.randomSeed
        );

        return graph;
    }

    Graph createCircularLayoutGraph(
        const Graph& originalGraph,
        const SolverSettings& settings
    )
    {
        Graph graph = originalGraph;

        BaselineLayouts::applyCircular(
            graph,
            settings.layoutWidth,
            settings.layoutHeight
        );

        return graph;
    }

    Graph createGridLayoutGraph(
        const Graph& originalGraph,
        const SolverSettings& settings
    )
    {
        Graph graph = originalGraph;

        BaselineLayouts::applyGrid(
            graph,
            settings.layoutWidth,
            settings.layoutHeight
        );

        return graph;
    }

    Graph createFruchtermanReingoldLayoutGraph(
        const Graph& originalGraph,
        const SolverSettings& settings
    )
    {
        Graph graph = originalGraph;

        FruchtermanReingoldLayout::apply(
            graph,
            settings.layoutWidth,
            settings.layoutHeight,
            settings.fruchtermanReingoldIterations
        );

        return graph;
    }


bool shouldReplaceScoreCandidate(
    const bool hasCurrentCandidate,
    const LayoutScore& currentScore,
    const LayoutScore& candidateScore
)
{
    if (!hasCurrentCandidate)
    {
        return true;
    }

    return LayoutScoreCalculator::isBetter(candidateScore, currentScore);
}

    
void considerInitialLayoutCandidate(
    InitialLayoutSelection& selection,
    Graph candidateGraph
)
{
    const LayoutScore candidateScore = LayoutScoreCalculator::compute(candidateGraph);

    if (shouldReplaceScoreCandidate(
            selection.hasOptimizationStart,
            selection.bestOptimizationStartScore,
            candidateScore
        ))
    {
        selection.bestOptimizationStartGraph = candidateGraph;
        selection.bestOptimizationStartScore = candidateScore;
        selection.hasOptimizationStart = true;
    }

    if (!isGraphValid(candidateGraph))
    {
        return;
    }

    if (shouldReplaceScoreCandidate(
            selection.hasValidFallback,
            selection.bestValidFallbackScore,
            candidateScore
        ))
    {
        selection.bestValidFallbackGraph = std::move(candidateGraph);
        selection.bestValidFallbackScore = candidateScore;
        selection.hasValidFallback = true;
    }
}


    
InitialLayoutSelection selectInitialLayouts(
    const Graph& originalGraph,
    const SolverSettings& settings
)
{
    InitialLayoutSelection selection;

    considerInitialLayoutCandidate(selection, originalGraph);
    considerInitialLayoutCandidate(selection, createRandomLayoutGraph(originalGraph, settings));
    considerInitialLayoutCandidate(selection, createCircularLayoutGraph(originalGraph, settings));
    considerInitialLayoutCandidate(selection, createGridLayoutGraph(originalGraph, settings));
    considerInitialLayoutCandidate(selection, createFruchtermanReingoldLayoutGraph(originalGraph, settings));

    // Optional external candidate (e.g. FMMM/OGDF). Loaded behind the same gate:
    // it can win the SA start slot on quality, but only counts as a valid
    // fallback if it passes validation. This lets FMMM's low-k layout be used
    // where it is valid, while the robust SA path covers instances where FMMM
    // produces an unusable (vertex-on-edge) layout.
    if (!settings.initCandidatePath.empty())
    {
        considerInitialLayoutCandidate(
            selection, JsonReader::load(settings.initCandidatePath));
    }

    if (!selection.hasOptimizationStart)
    {
        throw std::runtime_error("No initial layout candidate could be created.");
    }

    return selection;
}




void applySimulatedAnnealingPipeline(
    Graph& graph,
    const SolverSettings& settings,
    const bool useHotspotSampling,
    const bool useIncrementalCrossingIndex
)
{
    const InitialLayoutSelection initialLayouts = selectInitialLayouts(graph, settings);

    graph = initialLayouts.bestOptimizationStartGraph;

    const SimulatedAnnealingOptions options = createSimulatedAnnealingOptions(
        settings,
        useHotspotSampling,
        useIncrementalCrossingIndex
    );

    SimulatedAnnealingOptimizer::optimize(graph, options);

    const LayoutScore optimizedScore = LayoutScoreCalculator::compute(graph);
    const bool optimizedIsValid = isGraphValid(graph);

    if (optimizedIsValid)
    {
        if (!initialLayouts.hasValidFallback)
        {
            return;
        }

        if (LayoutScoreCalculator::isBetter(
                initialLayouts.bestValidFallbackScore,
                optimizedScore
            ))
        {
            graph = initialLayouts.bestValidFallbackGraph;
        }

        return;
    }

    if (initialLayouts.hasValidFallback)
    {
        graph = initialLayouts.bestValidFallbackGraph;
        return;
    }

    throw std::runtime_error(
        "Simulated annealing did not produce a valid solution and no valid fallback layout exists."
    );
}



    void applyLayout(
        Graph& graph,
        const std::string& layoutName,
        const SolverSettings& settings
    )
    {
        if (layoutName == "random")
        {
            BaselineLayouts::applyRandom(
                graph,
                settings.layoutWidth,
                settings.layoutHeight,
                settings.randomSeed
            );
            return;
        }

        if (layoutName == "circular")
        {
            BaselineLayouts::applyCircular(
                graph,
                settings.layoutWidth,
                settings.layoutHeight
            );
            return;
        }

        if (layoutName == "grid")
        {
            BaselineLayouts::applyGrid(
                graph,
                settings.layoutWidth,
                settings.layoutHeight
            );
            return;
        }

        if (layoutName == "fr")
        {
            FruchtermanReingoldLayout::apply(
                graph,
                settings.layoutWidth,
                settings.layoutHeight,
                settings.fruchtermanReingoldIterations
            );
            return;
        }


    if (layoutName == "sa")
    {
        applySimulatedAnnealingPipeline(
            graph,
            settings,
            true,
            false
        );
        return;
    }

    if (layoutName == "sa-uniform")
    {
        applySimulatedAnnealingPipeline(
            graph,
            settings,
            false,
            false
        );
        return;
    }
    
    if (layoutName == "sa-index")
    {
        applySimulatedAnnealingPipeline(
            graph,
            settings,
            true,
            true
        );
        return;
    }


        throw std::runtime_error("Unknown layout: " + layoutName);
    }

    CrossingStats computeStatsAfterLayout(
        const Graph& originalGraph,
        const std::string& layoutName,
        const SolverSettings& settings
    )
    {
        Graph graph = originalGraph;
        applyLayout(graph, layoutName, settings);
        return CrossingStatistics::compute(graph);
    }


void runCompare(
    const Graph& originalGraph,
    const SolverSettings& settings
)
{
    const CrossingStats originalStats = CrossingStatistics::compute(originalGraph);
    const CrossingStats randomStats = computeStatsAfterLayout(originalGraph, "random", settings);
    const CrossingStats circularStats = computeStatsAfterLayout(originalGraph, "circular", settings);
    const CrossingStats gridStats = computeStatsAfterLayout(originalGraph, "grid", settings);
    const CrossingStats frStats = computeStatsAfterLayout(originalGraph, "fr", settings);
    const CrossingStats saStats = computeStatsAfterLayout(originalGraph, "sa", settings);
    const CrossingStats saUniformStats = computeStatsAfterLayout(originalGraph, "sa-uniform", settings);

    std::cout << "Nodes: " << originalGraph.nodes.size() << '\n';
    std::cout << "Edges: " << originalGraph.edges.size() << '\n';
    std::cout << "Width: " << settings.layoutWidth << '\n';
    std::cout << "Height: " << settings.layoutHeight << '\n';
    std::cout << "Seed: " << settings.randomSeed << '\n';
    std::cout << "FR iterations: " << settings.fruchtermanReingoldIterations << '\n';
    std::cout << "SA iterations: " << settings.simulatedAnnealingIterations << '\n';

    std::cout << "Original k: " << originalStats.maxCrossingsPerEdge << '\n';
    std::cout << "Original crossings: " << originalStats.totalCrossings << '\n';

    std::cout << "Random k: " << randomStats.maxCrossingsPerEdge << '\n';
    std::cout << "Random crossings: " << randomStats.totalCrossings << '\n';

    std::cout << "Circular k: " << circularStats.maxCrossingsPerEdge << '\n';
    std::cout << "Circular crossings: " << circularStats.totalCrossings << '\n';

    std::cout << "Grid k: " << gridStats.maxCrossingsPerEdge << '\n';
    std::cout << "Grid crossings: " << gridStats.totalCrossings << '\n';

    std::cout << "FR k: " << frStats.maxCrossingsPerEdge << '\n';
    std::cout << "FR crossings: " << frStats.totalCrossings << '\n';

    std::cout << "SA k: " << saStats.maxCrossingsPerEdge << '\n';
    std::cout << "SA crossings: " << saStats.totalCrossings << '\n';

    std::cout << "SA uniform k: " << saUniformStats.maxCrossingsPerEdge << '\n';
    std::cout << "SA uniform crossings: " << saUniformStats.totalCrossings << '\n';
}


    void printValidationErrors(const ValidationResult& validationResult)
    {
        for (const std::string& error : validationResult.errors)
        {
            std::cerr << "Validation error: " << error << '\n';
        }
    }

    int runValidationMode(const Graph& graph)
    {
        const ValidationResult validationResult = SolutionValidator::validate(graph);

        if (validationResult.isValid())
        {
            std::cout << "Validation: valid\n";
            return 0;
        }

        printValidationErrors(validationResult);
        std::cout << "Validation: invalid\n";

        return 1;
    }

int runRepairMode(Graph& graph, const std::string& outputPath)
{
    const bool changed = SolutionRepairer::repairDuplicateRoundedCoordinates(graph);

    const ValidationResult validationResult = SolutionValidator::validate(graph);

    if (!validationResult.isValid())
    {
        printValidationErrors(validationResult);
        throw std::runtime_error("Repair did not produce a valid solution.");
    }

    JsonWriter::save(graph, outputPath);

    std::cout << "Repair changed graph: " << (changed ? "yes" : "no") << '\n';
    std::cout << "Output: " << outputPath << '\n';

    return 0;
}

    void printSettingsIfLayoutWasApplied(const SolverSettings& settings)
    {
        std::cout << "Width: " << settings.layoutWidth << '\n';
        std::cout << "Height: " << settings.layoutHeight << '\n';
        std::cout << "Seed: " << settings.randomSeed << '\n';
        std::cout << "FR iterations: " << settings.fruchtermanReingoldIterations << '\n';
        std::cout << "SA iterations: " << settings.simulatedAnnealingIterations << '\n';
    }
}

int runSubmissionMode(const Graph& graph, const std::string& outputPath)
{
    const ValidationResult validationResult = SolutionValidator::validate(graph);
    if (!validationResult.isValid())
    {
        printValidationErrors(validationResult);
        throw std::runtime_error("Refusing to write invalid submission.");
    }

    JsonWriter::saveSubmission(graph, outputPath);

    std::cout << "Submission output: " << outputPath << '\n';

    return 0;
}

int main(int argc, char* argv[])
{
    try
    {
        const ProgramOptions options = parseProgramOptions(argc, argv);

        Graph graph = JsonReader::load(options.inputPath);

        const SolverSettings settings = resolveSettingsForGraph(
            options.settings,
            graph
        );

        applyResolvedBoundsToGraph(graph, settings);

        if (options.layoutName == "validate")
        {
            return runValidationMode(graph);
        }
        if (options.layoutName == "repair")
        {
            return runRepairMode(graph, options.outputPath);
        }
        
        if (options.layoutName == "submission")
        {
            return runSubmissionMode(graph, options.outputPath);
        }


        if (options.layoutName == "compare")
        {
            runCompare(graph, settings);
            return 0;
        }

        if (options.hasLayout())
        {
            applyLayout(graph, options.layoutName, settings);
        }

        const CrossingStats stats = CrossingStatistics::compute(graph);

        std::cout << "Nodes: " << graph.nodes.size() << '\n';
        std::cout << "Edges: " << graph.edges.size() << '\n';

        if (options.hasLayout())
        {
            std::cout << "Layout: " << options.layoutName << '\n';
            printSettingsIfLayoutWasApplied(settings);
        }

        std::cout << "K: " << stats.maxCrossingsPerEdge << '\n';
        std::cout << "Crossings: " << stats.totalCrossings << '\n';


        if (options.hasOutput())
        {
            SolutionRepairer::repairDuplicateRoundedCoordinates(graph);

            const ValidationResult validationResult = SolutionValidator::validate(graph);

            if (!validationResult.isValid())
            {
                printValidationErrors(validationResult);
                throw std::runtime_error("Refusing to write invalid solution.");
            }

            JsonWriter::save(graph, options.outputPath);
            std::cout << "Output: " << options.outputPath << '\n';
        }


        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Error: " << exception.what() << '\n';
        printUsage();
        return 1;
    }
}
