
#include "SimulatedAnnealingOptimizer.hpp"

#include "CrossingCounter.hpp"
#include "CrossingStatistics.hpp"
#include "IncrementalCrossingIndex.hpp"
#include "LayoutScore.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr double Epsilon = 1e-9;
    constexpr int HotspotRefreshInterval = 100;
    constexpr int BestScoreRefreshInterval = 100;

    double clamp(const double value, const double minValue, const double maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    std::unordered_map<int, std::size_t> buildNodeIndexLookup(const Graph& graph)
    {
        std::unordered_map<int, std::size_t> indexByNodeId;
        indexByNodeId.reserve(graph.nodes.size());

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            const auto [_, inserted] = indexByNodeId.emplace(
                graph.nodes[index].id,
                index
            );

            if (!inserted)
            {
                throw std::runtime_error(
                    "Duplicate node id while building SA node index lookup: " +
                    std::to_string(graph.nodes[index].id)
                );
            }
        }

        return indexByNodeId;
    }

    std::size_t findNodeIndex(
        const std::unordered_map<int, std::size_t>& indexByNodeId,
        const int nodeId
    )
    {
        const auto iterator = indexByNodeId.find(nodeId);
        if (iterator == indexByNodeId.end())
        {
            throw std::runtime_error(
                "Edge references unknown node id: " +
                std::to_string(nodeId)
            );
        }

        return iterator->second;
    }

    std::vector<std::size_t> buildMovableNodeIndices(const Graph& graph)
    {
        std::vector<std::size_t> movableNodeIndices;
        movableNodeIndices.reserve(graph.nodes.size());

        for (std::size_t index = 0; index < graph.nodes.size(); ++index)
        {
            if (!graph.nodes[index].fixed)
            {
                movableNodeIndices.push_back(index);
            }
        }

        return movableNodeIndices;
    }

    double interpolate(
        const double start,
        const double end,
        const double progress
    )
    {
        return start + (end - start) * progress;
    }

    double geometricTemperature(
        const double initialTemperature,
        const double finalTemperature,
        const double progress
    )
    {
        return initialTemperature * std::pow(
            finalTemperature / initialTemperature,
            progress
        );
    }

    bool hasSameScore(
        const LayoutScore& first,
        const LayoutScore& second
    )
    {
        return first.maxCrossingsPerEdge == second.maxCrossingsPerEdge
            && first.totalCrossings == second.totalCrossings;
    }

    long long signedDifference(
        const std::size_t first,
        const std::size_t second
    )
    {
        return static_cast<long long>(first) - static_cast<long long>(second);
    }

    long long computeScorePenaltyWeight(const Graph& graph)
    {
        return std::max(100LL, static_cast<long long>(graph.edges.size()));
    }

    long long computeWeightedScoreDelta(
        const LayoutScore& candidateScore,
        const LayoutScore& currentScore,
        const long long kPenaltyWeight
    )
    {
        const long long kDelta = signedDifference(
            candidateScore.maxCrossingsPerEdge,
            currentScore.maxCrossingsPerEdge
        );

        const long long crossingDelta = signedDifference(
            candidateScore.totalCrossings,
            currentScore.totalCrossings
        );

        return kDelta * kPenaltyWeight + crossingDelta;
    }

    bool shouldAcceptWorseScore(
        const long long weightedScoreDelta,
        const double temperature,
        std::mt19937& generator
    )
    {
        if (weightedScoreDelta <= 0)
        {
            return true;
        }

        if (temperature <= Epsilon)
        {
            return false;
        }

        const double probability = std::exp(
            -static_cast<double>(weightedScoreDelta) / temperature
        );

        std::uniform_real_distribution<double> distribution(0.0, 1.0);
        return distribution(generator) < probability;
    }

    bool shouldAcceptMove(
        const LayoutScore& candidateScore,
        const LayoutScore& currentScore,
        const long long kPenaltyWeight,
        const double temperature,
        std::mt19937& generator
    )
    {
        if (LayoutScoreCalculator::isBetter(candidateScore, currentScore))
        {
            return true;
        }

        if (hasSameScore(candidateScore, currentScore))
        {
            return true;
        }

        const long long weightedScoreDelta = computeWeightedScoreDelta(
            candidateScore,
            currentScore,
            kPenaltyWeight
        );

        return shouldAcceptWorseScore(
            weightedScoreDelta,
            temperature,
            generator
        );
    }

    void validateOptions(const SimulatedAnnealingOptions& options)
    {
        if (options.width <= 0.0)
        {
            throw std::invalid_argument("Simulated annealing width must be positive.");
        }

        if (options.height <= 0.0)
        {
            throw std::invalid_argument("Simulated annealing height must be positive.");
        }

        if (options.initialTemperature <= 0.0)
        {
            throw std::invalid_argument("Initial temperature must be positive.");
        }

        if (options.finalTemperature <= 0.0)
        {
            throw std::invalid_argument("Final temperature must be positive.");
        }

        if (options.initialStepSize < 0.0 || options.finalStepSize < 0.0)
        {
            throw std::invalid_argument("Step sizes must not be negative.");
        }
    }

    std::vector<double> buildHotspotWeights(
        const Graph& graph,
        const CrossingStats& crossingStats,
        const std::unordered_map<int, std::size_t>& indexByNodeId
    )
    {
        std::vector<double> weights(graph.nodes.size(), 0.0);

        for (std::size_t nodeIndex = 0; nodeIndex < graph.nodes.size(); ++nodeIndex)
        {
            if (!graph.nodes[nodeIndex].fixed)
            {
                weights[nodeIndex] = 1.0;
            }
        }

        const std::size_t maxCrossingsPerEdge = crossingStats.maxCrossingsPerEdge;

        for (std::size_t edgeIndex = 0; edgeIndex < graph.edges.size(); ++edgeIndex)
        {
            const std::size_t edgeCrossings = crossingStats.crossingsPerEdge[edgeIndex];
            if (edgeCrossings == 0)
            {
                continue;
            }

            const Edge& edge = graph.edges[edgeIndex];
            const std::size_t sourceIndex = findNodeIndex(indexByNodeId, edge.source);
            const std::size_t targetIndex = findNodeIndex(indexByNodeId, edge.target);

            const double crossingValue = static_cast<double>(edgeCrossings);
            double weightIncrease = crossingValue * crossingValue;

            if (edgeCrossings == maxCrossingsPerEdge)
            {
                weightIncrease += crossingValue * crossingValue;
            }

            if (!graph.nodes[sourceIndex].fixed)
            {
                weights[sourceIndex] += weightIncrease;
            }

            if (!graph.nodes[targetIndex].fixed)
            {
                weights[targetIndex] += weightIncrease;
            }
        }

        return weights;
    }

    std::size_t sampleHotspotNodeIndex(
        const std::vector<double>& weights,
        std::mt19937& generator
    )
    {
        std::discrete_distribution<std::size_t> distribution(
            weights.begin(),
            weights.end()
        );

        return distribution(generator);
    }

    std::size_t sampleUniformNodeIndex(
        const std::vector<std::size_t>& movableNodeIndices,
        std::mt19937& generator
    )
    {
        std::uniform_int_distribution<std::size_t> distribution(
            0,
            movableNodeIndices.size() - 1
        );

        return movableNodeIndices[distribution(generator)];
    }

    bool shouldRefreshHotspotWeights(const int iteration)
    {
        return iteration % HotspotRefreshInterval == 0;
    }

    bool shouldRefreshBestScore(const int iteration)
    {
        return iteration % BestScoreRefreshInterval == 0;
    }

    void updateBestGraphIfImproved(
        const Graph& candidateGraph,
        Graph& bestGraph,
        LayoutScore& bestScore
    )
    {
        const LayoutScore candidateScore = LayoutScoreCalculator::compute(candidateGraph);

        if (LayoutScoreCalculator::isBetter(candidateScore, bestScore))
        {
            bestScore = candidateScore;
            bestGraph = candidateGraph;
        }
    }

    void updateBestGraphIfScoreImproved(
        const Graph& candidateGraph,
        const LayoutScore& candidateScore,
        Graph& bestGraph,
        LayoutScore& bestScore
    )
    {
        if (LayoutScoreCalculator::isBetter(candidateScore, bestScore))
        {
            bestScore = candidateScore;
            bestGraph = candidateGraph;
        }
    }

    std::size_t sampleNodeIndex(
        const std::vector<double>& hotspotWeights,
        const std::vector<std::size_t>& movableNodeIndices,
        const bool useHotspotSampling,
        std::mt19937& generator
    )
    {
        if (useHotspotSampling)
        {
            return sampleHotspotNodeIndex(hotspotWeights, generator);
        }

        return sampleUniformNodeIndex(movableNodeIndices, generator);
    }

    NodeMove createRandomNodeMove(
        const Graph& graph,
        const std::size_t nodeIndex,
        const double stepSize,
        const double width,
        const double height,
        std::mt19937& generator
    )
    {
        std::uniform_real_distribution<double> movementDistribution(
            -stepSize,
            stepSize
        );

        const Node& node = graph.nodes[nodeIndex];

        return NodeMove{
            nodeIndex,
            clamp(
                node.x + movementDistribution(generator),
                0.0,
                width
            ),
            clamp(
                node.y + movementDistribution(generator),
                0.0,
                height
            )
        };
    }

    std::size_t optimizeWithFullRecompute(
        Graph& graph,
        const SimulatedAnnealingOptions& options
    )
    {
        const std::vector<std::size_t> movableNodeIndices = buildMovableNodeIndices(graph);

        if (movableNodeIndices.empty())
        {
            return CrossingCounter::countCrossings(graph);
        }

        Graph bestGraph = graph;

        LayoutScore currentScore = LayoutScoreCalculator::compute(graph);
        LayoutScore bestScore = currentScore;

        const auto indexByNodeId = buildNodeIndexLookup(graph);
        const long long kPenaltyWeight = computeScorePenaltyWeight(graph);

        std::mt19937 generator(options.seed);

        std::vector<double> hotspotWeights;
        if (options.useHotspotSampling)
        {
            hotspotWeights = buildHotspotWeights(
                graph,
                CrossingStatistics::compute(graph),
                indexByNodeId
            );
        }

        const bool useTimeBudget = options.timeBudgetSeconds > 0.0;
        const auto startTime = std::chrono::steady_clock::now();

        for (int iteration = 0; iteration < options.iterations; ++iteration)
        {
            double progress;
            if (useTimeBudget)
            {
                const double elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - startTime
                ).count();

                if (elapsed >= options.timeBudgetSeconds)
                {
                    break;
                }

                progress = elapsed / options.timeBudgetSeconds;
            }
            else
            {
                progress = options.iterations > 1
                    ? static_cast<double>(iteration) / static_cast<double>(options.iterations - 1)
                    : 1.0;
            }

            if (options.useHotspotSampling && shouldRefreshHotspotWeights(iteration))
            {
                hotspotWeights = buildHotspotWeights(
                    graph,
                    CrossingStatistics::compute(graph),
                    indexByNodeId
                );
            }

            if (shouldRefreshBestScore(iteration))
            {
                updateBestGraphIfImproved(
                    graph,
                    bestGraph,
                    bestScore
                );
            }

            const double temperature = geometricTemperature(
                options.initialTemperature,
                options.finalTemperature,
                progress
            );

            const double stepSize = interpolate(
                options.initialStepSize,
                options.finalStepSize,
                progress
            );

            const std::size_t nodeIndex = sampleNodeIndex(
                hotspotWeights,
                movableNodeIndices,
                options.useHotspotSampling,
                generator
            );

            Node& node = graph.nodes[nodeIndex];

            const double oldX = node.x;
            const double oldY = node.y;

            const NodeMove move = createRandomNodeMove(
                graph,
                nodeIndex,
                stepSize,
                options.width,
                options.height,
                generator
            );

            node.x = move.newX;
            node.y = move.newY;

            const LayoutScore candidateScore = LayoutScoreCalculator::compute(graph);

            if (shouldAcceptMove(
                    candidateScore,
                    currentScore,
                    kPenaltyWeight,
                    temperature,
                    generator
                ))
            {
                currentScore = candidateScore;

                updateBestGraphIfScoreImproved(
                    graph,
                    currentScore,
                    bestGraph,
                    bestScore
                );
            }
            else
            {
                node.x = oldX;
                node.y = oldY;
            }
        }

        updateBestGraphIfImproved(
            graph,
            bestGraph,
            bestScore
        );

        graph = bestGraph;

        return CrossingCounter::countCrossings(graph);
    }

    std::size_t optimizeWithIncrementalIndex(
        Graph& graph,
        const SimulatedAnnealingOptions& options
    )
    {
        IncrementalCrossingIndex index(graph);

        const std::vector<std::size_t> movableNodeIndices = buildMovableNodeIndices(index.graph());

        if (movableNodeIndices.empty())
        {
            return CrossingCounter::countCrossings(index.graph());
        }

        Graph bestGraph = index.graph();

        LayoutScore currentScore = index.score();
        LayoutScore bestScore = currentScore;

        const auto indexByNodeId = buildNodeIndexLookup(index.graph());
        const long long kPenaltyWeight = computeScorePenaltyWeight(index.graph());

        std::mt19937 generator(options.seed);

        std::vector<double> hotspotWeights;
        if (options.useHotspotSampling)
        {
            hotspotWeights = buildHotspotWeights(
                index.graph(),
                index.stats(),
                indexByNodeId
            );
        }

        const bool useTimeBudget = options.timeBudgetSeconds > 0.0;
        const auto startTime = std::chrono::steady_clock::now();

        for (int iteration = 0; iteration < options.iterations; ++iteration)
        {
            double progress;
            if (useTimeBudget)
            {
                const double elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - startTime
                ).count();

                if (elapsed >= options.timeBudgetSeconds)
                {
                    break;
                }

                progress = elapsed / options.timeBudgetSeconds;
            }
            else
            {
                progress = options.iterations > 1
                    ? static_cast<double>(iteration) / static_cast<double>(options.iterations - 1)
                    : 1.0;
            }

            if (options.useHotspotSampling && shouldRefreshHotspotWeights(iteration))
            {
                hotspotWeights = buildHotspotWeights(
                    index.graph(),
                    index.stats(),
                    indexByNodeId
                );
            }

            if (shouldRefreshBestScore(iteration))
            {
                updateBestGraphIfScoreImproved(
                    index.graph(),
                    currentScore,
                    bestGraph,
                    bestScore
                );
            }

            const double temperature = geometricTemperature(
                options.initialTemperature,
                options.finalTemperature,
                progress
            );

            const double stepSize = interpolate(
                options.initialStepSize,
                options.finalStepSize,
                progress
            );

            const std::size_t nodeIndex = sampleNodeIndex(
                hotspotWeights,
                movableNodeIndices,
                options.useHotspotSampling,
                generator
            );

            const NodeMove move = createRandomNodeMove(
                index.graph(),
                nodeIndex,
                stepSize,
                options.width,
                options.height,
                generator
            );

            const CrossingDeltaResult candidateDelta = index.evaluateNodeMove(move);
            const LayoutScore candidateScore = candidateDelta.afterScore;

            if (shouldAcceptMove(
                    candidateScore,
                    currentScore,
                    kPenaltyWeight,
                    temperature,
                    generator
                ))
            {
                const CrossingDeltaResult appliedDelta = index.applyNodeMove(move);
                currentScore = appliedDelta.afterScore;

                updateBestGraphIfScoreImproved(
                    index.graph(),
                    currentScore,
                    bestGraph,
                    bestScore
                );
            }
        }

        updateBestGraphIfScoreImproved(
            index.graph(),
            currentScore,
            bestGraph,
            bestScore
        );

        graph = bestGraph;

        return CrossingCounter::countCrossings(graph);
    }
}

std::size_t SimulatedAnnealingOptimizer::optimize(
    Graph& graph,
    const SimulatedAnnealingOptions& options
)
{
    validateOptions(options);

    if (graph.nodes.empty())
    {
        return 0;
    }

    if (options.iterations <= 0)
    {
        return CrossingCounter::countCrossings(graph);
    }

    if (options.useIncrementalCrossingIndex)
    {
        return optimizeWithIncrementalIndex(
            graph,
            options
        );
    }

    return optimizeWithFullRecompute(
        graph,
        options
    );
}
