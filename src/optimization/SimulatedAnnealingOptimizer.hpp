
#pragma once

#include "Graph.hpp"

#include <cstddef>

struct SimulatedAnnealingOptions
{
    double width;
    double height;
    int iterations;
    unsigned int seed;
    double initialTemperature;
    double finalTemperature;
    double initialStepSize;
    double finalStepSize;
    bool useHotspotSampling = true;
    bool useIncrementalCrossingIndex = false;
    // Real wall-clock budget in seconds. When > 0, the cooling schedule is
    // driven by elapsed time instead of the iteration index, and the loop stops
    // once the budget is exhausted (iterations then acts only as an upper bound).
    double timeBudgetSeconds = -1.0;
};

class SimulatedAnnealingOptimizer
{
public:
    static std::size_t optimize(
        Graph& graph,
        const SimulatedAnnealingOptions& options
    );
};
